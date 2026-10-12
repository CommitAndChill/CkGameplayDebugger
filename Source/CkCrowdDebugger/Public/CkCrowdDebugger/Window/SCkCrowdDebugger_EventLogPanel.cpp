#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"
#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkCrowdDebugger/Window/CkCrowdDebugger_PanelAxes.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowd_debugger_event_log
{
	// Rolling window. Path trouble is bursty — a blocked doorway can emit one line per agent per
	// replan — so the cap is what keeps the panel readable rather than a scroll-forever transcript.
	constexpr auto MaxEntries = 300;

	// A path-trouble record whose navigation status actually failed is an error; a partial or
	// pending one is a warning. Same split the agent list uses for its status colour.
	auto Get_Tone(const FCkCrowdDebugger_AgentSnapshot& InAgent) -> ECk_Tone
	{
		return InAgent.TroubleNavigationStatus == ECk_Nav_PathStatus::Failed
			? ECk_Tone::Err
			: ECk_Tone::Warn;
	}

	// Short bucket for the leading chip: which layer reported the problem.
	auto Get_Category(const FCkCrowdDebugger_AgentSnapshot& InAgent) -> FString
	{
		return InAgent.HadPathNetworkFailure ? FString{TEXT("SIDEWALK")} : FString{TEXT("NAV")};
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	if (_ViewModel.IsValid())
	{
		_OnAgentListChangedHandle = _ViewModel->OnAgentListChanged.AddSP(
			SharedThis(this), &SCkCrowdDebugger_EventLogPanel::Handle_AgentListChanged);
	}

	_OnSessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
		SharedThis(this), &SCkCrowdDebugger_EventLogPanel::Handle_SessionInvalidated);

	const TWeakPtr<SCkCrowdDebugger_EventLogPanel> WeakPanel = SharedThis(this);

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Padding_Lambda([]()
		{
			return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS});
		})
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[
				ck::crowd_debugger_axes::Make_PaneHeading(TEXT("Event Log"))
			]

			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(_EventLog, SCkDebug_EventLog)
					.MaxEntries(ck_crowd_debugger_event_log::MaxEntries)
					.ShowTimestamps(true)
					.AutoScroll(true)
					// The panel is live but the feed is narrow: path trouble is the only event stream
					// the crowd collector actually records today. Say so rather than render an
					// unexplained blank list.
					.EmptyText(FText::FromString(
						FString{TEXT("No path-trouble events yet.\n")}
						+ TEXT("Lines land here when an agent's route fails or falls back. Sleep / pierce / ")
						+ TEXT("proxy categories follow once those systems record events of their own.")))
					.SelectedId_Lambda([WeakPanel]() -> int32
					{
						const auto Panel = WeakPanel.Pin();
						if (!Panel.IsValid() || !Panel->_ViewModel.IsValid())
						{ return INDEX_NONE; }

						if (Panel->_ViewModel->Get_IsRemote())
						{ return Panel->Get_RemoteSelectedToken(); }

						const auto Selected = Panel->_ViewModel->Get_SelectedHandle();
						const auto* Token = ck::IsValid(Selected)
							? Panel->_NativeTokensById.Find(static_cast<int32>(GetTypeHash(Selected))) : nullptr;
						return Token ? *Token : INDEX_NONE;
					})
					.OnEntrySelected(FOnCkDebug_EventLogEntrySelected::CreateSP(
						SharedThis(this), &SCkCrowdDebugger_EventLogPanel::Handle_EntrySelected))
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebugger_EventLogPanel::~SCkCrowdDebugger_EventLogPanel()
{
	if (_ViewModel.IsValid() && _OnAgentListChangedHandle.IsValid())
	{ _ViewModel->OnAgentListChanged.Remove(_OnAgentListChangedHandle); }

	if (_OnSessionInvalidatedHandle.IsValid())
	{ ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_OnSessionInvalidatedHandle); }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Handle_AgentListChanged(
	const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents)
	-> void
{
	if (NOT _EventLog.IsValid())
	{ return; }

	if (_ViewModel.IsValid() && _ViewModel->Get_IsRemote())
	{ Handle_RemoteAgentListChanged(InAgents); return; }
	if (_RemoteDomainActive)
	{ Reset_RemoteDomain(); }
	_NativeDomainActive = true;

	// The broadcast fires every refresh with the SAME retained trouble record until the agent hits a
	// new problem, so the event time is the dedup key: one line per (agent, event), not per tick.
	auto NewEntries = TArray<FCkDebug_EventLogEntry>{};
	TSet<int32> CurrentNativeIds;
	for (const auto& Agent : InAgents)
	{
		if (!Agent.IsRemote)
		{ CurrentNativeIds.Add(Compute_SelectionId(Agent)); }
	}

	for (const auto& Agent : InAgents)
	{
		if (Agent.IsRemote || NOT Agent.HasPathTroubleEvent || Agent.PathTroubleEventTimeSeconds < 0.0)
		{ continue; }

		const auto SelectionId = Compute_SelectionId(Agent);

		if (const auto* LastLogged = _LastLoggedEventTimeById.Find(SelectionId);
			LastLogged != nullptr && *LastLogged >= Agent.PathTroubleEventTimeSeconds)
		{ continue; }

		_LastLoggedEventTimeById.Add(SelectionId, Agent.PathTroubleEventTimeSeconds);

		auto Entry = FCkDebug_EventLogEntry{};
		Entry.Message = Agent.OwnerName.IsEmpty()
			? ck::Format_UE(TEXT("{} — {}"), Agent.Handle.Get_Entity(), Agent.PathTroubleSummary)
			: ck::Format_UE(TEXT("{} ({}) — {}"),
				Agent.Handle.Get_Entity(), Agent.OwnerName, Agent.PathTroubleSummary);
		Entry.Category = ck_crowd_debugger_event_log::Get_Category(Agent);
		Entry.Tone = ck_crowd_debugger_event_log::Get_Tone(Agent);
		Entry.TimeSeconds = Agent.PathTroubleEventTimeSeconds;
		const auto Token = TryGet_NativeToken(SelectionId);
		Entry.SelectionId = Token;

		NewEntries.Add(MoveTemp(Entry)); _NativeHistoryTokens.Add(Token);
	}

	const auto Excess = _NativeHistoryTokens.Num() - ck_crowd_debugger_event_log::MaxEntries;
	if (Excess > 0)
	{ _NativeHistoryTokens.RemoveAt(0, Excess, EAllowShrinking::No); }
	Prune_NativeMappings(CurrentNativeIds);
	if (NewEntries.IsEmpty())
	{ return; }

	// Append (never Set_Entries) so surviving rows keep their TSharedPtr and the user's selection
	// outlives the refresh — see the list-row contract in CkDebuggerCommon/CLAUDE.md.
	_EventLog->Add_Entries(MoveTemp(NewEntries));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Handle_SessionInvalidated() -> void
{
	Reset_RemoteDomain();
	_LastLoggedEventTimeById.Reset();

	if (_EventLog.IsValid())
	{ _EventLog->Clear_Entries(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Handle_EntrySelected(int32 InSelectionId) -> void
{
	if (NOT _ViewModel.IsValid())
	{ return; }

	if (_ViewModel->Get_IsRemote())
	{
		const auto* Subject = _RemoteSubjectsByToken.Find(InSelectionId);
		const auto& E = _ViewModel->Get_RemoteEnvelope();
		if (!Subject || !_RemoteDomainActive || !FCkCrowdRemoteDebugValidation::IsValid(E) ||
			E.Outcome != ECkCrowdRemoteSelectionOutcome::Accepted || E.WorldGeneration != _RemoteWorldGeneration ||
			E.SessionGeneration != _RemoteSessionGeneration || Subject->SessionGeneration != E.SessionGeneration ||
			Subject->Id.WorldGeneration != E.WorldGeneration)
		{ return; }
		for (const auto& Agent : _ViewModel->Get_AllAgents())
		{
			if (Agent.IsRemote && Agent.RemoteId == Subject->Id && Agent.RemoteSession == E.SessionGeneration &&
				Agent.RemoteSample == E.SampleSequence)
			{ _ViewModel->TrySelectRemoteAgent(Agent); return; }
		}
		return; // no foreign handle, focus, selection synchronization, or receipt
	}

	const auto* NativeId = _NativeIdsByToken.Find(InSelectionId);
	if (!NativeId)
	{ return; }
	// Resolve the id against the LIVE agent list rather than a retained handle — the row may name an
	// agent that has since been destroyed, and this panel must never hold a PIE handle.
	for (const auto& Agent : _ViewModel->Get_AllAgents())
	{
		if (Agent.IsRemote || Compute_SelectionId(Agent) != *NativeId)
		{ continue; }

		if (!ck::IsValid(Agent.Handle))
		{ return; }
		_ViewModel->Set_SelectedHandle(Agent.Handle);
		ck::DebugSelectionSync::Broadcast(Agent.Handle, TEXT("CrowdDebugger"));
		return;
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Compute_SelectionId(
	const FCkCrowdDebugger_AgentSnapshot& InAgent)
	-> int32
{
	return static_cast<int32>(GetTypeHash(InAgent.Handle));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Reset_RemoteDomain() -> void
{
	_RemoteDomainActive = false; _NativeDomainActive = false;
	_RemoteWorldGeneration = 0; _RemoteSessionGeneration = 0;
	_RemoteTokensById.Reset(); _RemoteSubjectsByToken.Reset();
	_RemoteLastLoggedTimeById.Reset(); _RemoteHistoryTokens.Reset(); _RemoteHistoryIds.Reset();
	_NativeTokensById.Reset(); _NativeIdsByToken.Reset(); _NativeHistoryTokens.Reset();
	_LastLoggedEventTimeById.Reset();
	// The issuer/exhaustion latch deliberately outlive every domain reset.
	if (_EventLog.IsValid())
	{ _EventLog->Clear_Entries(); }
}

auto SCkCrowdDebugger_EventLogPanel::Get_RemoteSelectedToken() const -> int32
{
	if (!_ViewModel.IsValid() || !_RemoteDomainActive)
	{ return INDEX_NONE; }
	const auto& E = _ViewModel->Get_RemoteEnvelope();
	if (E.WorldGeneration != _RemoteWorldGeneration || E.SessionGeneration != _RemoteSessionGeneration ||
		E.Outcome != ECkCrowdRemoteSelectionOutcome::Accepted)
	{ return INDEX_NONE; }
	const auto* Token = _RemoteTokensById.Find(_ViewModel->Get_RemoteSelectedId());
	return Token ? *Token : INDEX_NONE;
}

auto SCkCrowdDebugger_EventLogPanel::TryGet_RemoteToken(const FCkCrowdRemoteAgentId& InId) -> int32
{
	if (const auto* Existing = _RemoteTokensById.Find(InId))
	{ return *Existing; }
	const auto Token = TryAllocate_SelectionToken();
	if (Token == INDEX_NONE)
	{ return INDEX_NONE; }
	_RemoteTokensById.Add(InId, Token);
	_RemoteSubjectsByToken.Add(Token, FRemoteSubject{InId, _RemoteSessionGeneration});
	return Token;
}

auto SCkCrowdDebugger_EventLogPanel::Prune_RemoteMappings(const TSet<FCkCrowdRemoteAgentId>& InCurrentIds) -> void
{
	for (auto It = _RemoteSubjectsByToken.CreateIterator(); It; ++It)
	{
		if (!InCurrentIds.Contains(It.Value().Id) && !_RemoteHistoryTokens.Contains(It.Key()))
		{ _RemoteTokensById.Remove(It.Value().Id); It.RemoveCurrent(); }
	}
	for (auto It = _RemoteLastLoggedTimeById.CreateIterator(); It; ++It)
	{
		if (!InCurrentIds.Contains(It.Key()) && !_RemoteHistoryIds.Contains(It.Key()))
		{ It.RemoveCurrent(); }
	}
}

auto SCkCrowdDebugger_EventLogPanel::Handle_RemoteAgentListChanged(
	const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents) -> void
{
	if (!_ViewModel.IsValid() || !_EventLog.IsValid())
	{ return; }
	if (_NativeDomainActive)
	{ Reset_RemoteDomain(); }
	const auto& E = _ViewModel->Get_RemoteEnvelope();
	if (!FCkCrowdRemoteDebugValidation::IsValid(E) || E.Outcome == ECkCrowdRemoteSelectionOutcome::Gone)
	{ Reset_RemoteDomain(); return; }
	if (E.Outcome != ECkCrowdRemoteSelectionOutcome::Accepted)
	{ return; } // copied rejection feedback lives in Window; no Accepted event subjects
	if (InAgents.Num() != E.Rows.Num())
	{ return; }
	TSet<FCkCrowdRemoteAgentId> CurrentIds;
	// Validate the entire actual copied publication before changing history/mappings or publishing a subset.
	for (int32 I = 0; I < InAgents.Num(); ++I)
	{
		const auto& Agent = InAgents[I];
		if (!Agent.IsRemote || !Agent.RemoteId.IsValid() || Agent.RemoteId != E.Rows[I].Id ||
			Agent.RemoteSession != E.SessionGeneration || Agent.RemoteSample != E.SampleSequence ||
			(Agent.HasPathTroubleEvent && (!FMath::IsFinite(Agent.PathTroubleEventTimeSeconds) || Agent.PathTroubleEventTimeSeconds < 0.0)))
		{ return; }
		CurrentIds.Add(Agent.RemoteId);
	}
	if (!_RemoteDomainActive || E.WorldGeneration != _RemoteWorldGeneration || E.SessionGeneration != _RemoteSessionGeneration)
	{
		Reset_RemoteDomain(); _RemoteDomainActive = true;
		_RemoteWorldGeneration = E.WorldGeneration; _RemoteSessionGeneration = E.SessionGeneration;
	}
	auto Entries = TArray<FCkDebug_EventLogEntry>{};
	for (const auto& Agent : InAgents)
	{
		if (!Agent.HasPathTroubleEvent)
		{ continue; }
		if (const auto* Time = _RemoteLastLoggedTimeById.Find(Agent.RemoteId);
			Time && *Time >= Agent.PathTroubleEventTimeSeconds)
		{ continue; }
		_RemoteLastLoggedTimeById.Add(Agent.RemoteId, Agent.PathTroubleEventTimeSeconds);
		const auto Token = TryGet_RemoteToken(Agent.RemoteId);
		auto Entry = FCkDebug_EventLogEntry{};
		const auto Identity = Agent.RemoteDisplayId.IsEmpty()
			? FString::Printf(TEXT("remote %lld:%lld"), static_cast<long long>(Agent.RemoteId.WorldGeneration),
				static_cast<long long>(Agent.RemoteId.AgentSerial)) : Agent.RemoteDisplayId;
		Entry.Message = Agent.OwnerName.IsEmpty()
			? ck::Format_UE(TEXT("{} — {}"), Identity, Agent.PathTroubleSummary)
			: ck::Format_UE(TEXT("{} ({}) — {}"), Identity, Agent.OwnerName, Agent.PathTroubleSummary);
		Entry.Category = ck_crowd_debugger_event_log::Get_Category(Agent);
		Entry.Tone = ck_crowd_debugger_event_log::Get_Tone(Agent);
		Entry.TimeSeconds = Agent.PathTroubleEventTimeSeconds; Entry.SelectionId = Token;
		Entries.Add(MoveTemp(Entry)); _RemoteHistoryTokens.Add(Token); _RemoteHistoryIds.Add(Agent.RemoteId);
	}
	const auto Excess = _RemoteHistoryTokens.Num() - ck_crowd_debugger_event_log::MaxEntries;
	if (Excess > 0)
	{
		_RemoteHistoryTokens.RemoveAt(0, Excess, EAllowShrinking::No);
		_RemoteHistoryIds.RemoveAt(0, Excess, EAllowShrinking::No);
	}
	Prune_RemoteMappings(CurrentIds);
	if (!Entries.IsEmpty())
	{ _EventLog->Add_Entries(MoveTemp(Entries)); }
}

auto SCkCrowdDebugger_EventLogPanel::TryAllocate_SelectionToken() -> int32
{
	if (_SelectionTokenExhausted || _NextSelectionToken > TNumericLimits<int32>::Max())
	{ _SelectionTokenExhausted = true; return INDEX_NONE; }
	return static_cast<int32>(_NextSelectionToken++);
}

auto SCkCrowdDebugger_EventLogPanel::TryGet_NativeToken(int32 InSubjectHash) -> int32
{
	if (const auto* Existing = _NativeTokensById.Find(InSubjectHash))
	{ return *Existing; }
	const auto Token = TryAllocate_SelectionToken();
	if (Token == INDEX_NONE)
	{ return INDEX_NONE; }
	_NativeTokensById.Add(InSubjectHash, Token); _NativeIdsByToken.Add(Token, InSubjectHash);
	return Token;
}

auto SCkCrowdDebugger_EventLogPanel::Prune_NativeMappings(const TSet<int32>& InCurrentIds) -> void
{
	for (auto It = _NativeIdsByToken.CreateIterator(); It; ++It)
	{
		if (!InCurrentIds.Contains(It.Value()) && !_NativeHistoryTokens.Contains(It.Key()))
		{ _NativeTokensById.Remove(It.Value()); It.RemoveCurrent(); }
	}
}
