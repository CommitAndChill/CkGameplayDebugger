#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "EngineGlobals.h"
#include "HAL/IConsoleManager.h"
#include "CkCrowd/Debug/CkCrowd_RemoteDebug_Utils.h"
#include <Engine/World.h>
#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <GameFramework/GameStateBase.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/Pawn.h>
#include <UObject/UnrealType.h>

// --------------------------------------------------------------------------------------------------------------------

auto FCkCrowdDebugger_ViewModel::Reset_ForWorldChange() -> void
{
	const bool WasRemote = _IsRemote;
	_DataCollector.Reset_ForWorldChange();
	_IsRemote = false; _HasPendingReceipt = false; _RequiresFreshPawnSelection = false; _PreviousPawnSelectionHigh = 0; _RemoteEnvelope = {};
	_RemotePC.Reset(); _RemoteLP.Reset(); _RemotePawn.Reset();
	_SeenWorldGeneration = 0; _SeenSessionGeneration = 0;
	_SeenSelectionRequest = 0; _SeenSample = 0; _ConsumedDelivery = 0;
	_SelectedHandle = FCk_Handle{};
	_LastWorld.Reset();
	_LastAgentCount = -1;

	if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.Crowd.SelectedEntityId")))
	{ if (!WasRemote) { CVar->Set(-1, ECVF_SetByConsole); } }

	OnAgentListChanged.Broadcast(_DataCollector.Get_AllAgents());
	OnSelectedAgentChanged.Broadcast(_SelectedHandle);
	OnAgentDataRefreshed.Broadcast(nullptr);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Tick(UWorld* InWorld, float InDeltaTime, APlayerController* InOwningPC)
	-> void
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_ViewModelTick);
	if (IsValid(InWorld) && InWorld->GetNetMode() == NM_Client)
	{ TickRemote(InWorld, InOwningPC); return; }
	if (_IsRemote || _LastWorld.Get() != InWorld) { Reset_ForWorldChange(); }
	_LastWorld = InWorld;

	if (_IsPaused)
	{ return; }

	_DataCollector.Collect(InWorld, _SelectedHandle);

	static const auto* PathNetworkTraceCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.CrowdDebugger.PathNetworkTrace"));
	if (PathNetworkTraceCVar != nullptr && PathNetworkTraceCVar->GetInt() != 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("CkCrowdDebugger.PathNetworkTrace stage=view_model frame=%llu world=%s world_ptr=%p view_model_ribbons=%d"),
			static_cast<unsigned long long>(GFrameCounter),
			IsValid(InWorld) ? *InWorld->GetName() : TEXT("Invalid"),
			InWorld,
			Get_PathNetworkRibbons().Num());
	}

	// Gate 3 — broadcast every frame so the agent-list rows reflect live neighbor counts,
	// status, etc. The Gate 0 count-change-only gate left per-agent fields stale (n= column
	// stuck at 0 even after NeighborSync populated the cache). The AgentListPanel already
	// restores selection by handle after rebuild (see OnAgentListChanged), so per-frame
	// broadcast is safe; cost is rebuilding ~N TSharedPtr items per tick at typical N≤150.
	{
		// Rebuilds/refreshes the agent list rows every frame.
		TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_BroadcastAgentList);
		OnAgentListChanged.Broadcast(_DataCollector.Get_AllAgents());
	}
	_LastAgentCount = _DataCollector.Get_AgentCount();

	if (ck::IsValid(_SelectedHandle))
	{
		const auto* Selected = Get_SelectedSnapshot();
		OnAgentDataRefreshed.Broadcast(Selected);
	}
	else
	{
		OnAgentDataRefreshed.Broadcast(nullptr);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Run_HealthCheckProbe()
	-> void
{
	if (_IsRemote) { return; }
	_DataCollector.Run_HealthCheckProbe(_LastWorld.Get());
	// No specific delegate for health-check yet; the panel reads the status via
	// TAttribute bindings on every paint, so the next frame picks up the result.
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Set_SelectedHandle(FCk_Handle InHandle)
	-> void
{
	if (_IsRemote) { return; }
	if (_SelectedHandle == InHandle)
	{ return; }

	_SelectedHandle = InHandle;

	// Mirror the selection to the CkCrowd-side `ck.Crowd.SelectedEntityId` CVar so the
	// in-world breadcrumb / planned-path draw processors render the selected agent
	// regardless of their global toggles. -1 means "no selection".
	if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.Crowd.SelectedEntityId")))
	{
		const auto NewId = ck::IsValid(_SelectedHandle)
			? static_cast<int32>(GetTypeHash(_SelectedHandle))
			: -1;
		CVar->Set(NewId, ECVF_SetByConsole);
	}

	OnSelectedAgentChanged.Broadcast(_SelectedHandle);
	OnAgentDataRefreshed.Broadcast(Get_SelectedSnapshot());
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Get_SelectedSnapshot() const
	-> const FCkCrowdDebugger_AgentSnapshot*
{
	if (_IsRemote)
	{
		if (_RequiresFreshPawnSelection || !_RemoteEnvelope.bHasDetail || !_RemoteEnvelope.SelectedId.IsValid()) { return nullptr; }
		for (const auto& Agent : _DataCollector.Get_AllAgents())
		{ if (Agent.IsRemote && Agent.RemoteId == _RemoteEnvelope.SelectedId) { return &Agent; } }
		return nullptr;
	}
	if (NOT ck::IsValid(_SelectedHandle))
	{ return nullptr; }

	const auto& Agents = _DataCollector.Get_AllAgents();
	for (const auto& Agent : Agents)
	{
		if (Agent.Handle == _SelectedHandle)
		{ return &Agent; }
	}

	return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Set_Paused(bool InPaused)
	-> void
{
	if (_IsPaused == InPaused)
	{ return; }

	_IsPaused = InPaused;
	OnPausedChanged.Broadcast(_IsPaused);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Set_ViewMode(ECk_CrowdDebugger_ViewMode InMode)
	-> void
{
	if (_ViewMode == InMode)
	{ return; }

	_ViewMode = InMode;
	OnViewModeChanged.Broadcast(_ViewMode);
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowd_debugger_owner
{
    auto IsExact(UWorld* World, APlayerController* PC, ULocalPlayer* LP) -> bool
    {
        if (!IsInGameThread() || !IsValid(World) || !IsValid(PC) || !IsValid(LP) ||
            World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || !World->IsGameWorld() ||
            World->GetNetMode() != NM_Client || PC->GetWorld() != World || !PC->IsLocalController() ||
            PC->GetLocalPlayer() != LP || LP->GetWorld() != World || LP->GetPlayerController(World) != PC)
        { return false; }
        const auto* GI = World->GetGameInstance();
        if (!IsValid(GI) || GI->GetWorld() != World || GI->GetLocalPlayers().Num() != 1 ||
            GI->GetLocalPlayers()[0] != LP) { return false; }
        const auto* State = World->GetGameState();
        const auto* Provider = IsValid(State) ? Cast<ICkCrowdRemoteDebugProvider>(State) : nullptr;
        return Provider != nullptr && Provider->IsForWorld(*World);
    }
}

auto FCkCrowdDebugger_ViewModel::HasExactRemoteOwner() const -> bool
{ return _IsRemote && ck_crowd_debugger_owner::IsExact(_LastWorld.Get(), _RemotePC.Get(), _RemoteLP.Get()); }

auto FCkCrowdDebugger_ViewModel::ClearRemotePresentation() -> void
{
    _RemoteEnvelope = {}; _HasPendingReceipt = false;
    _DataCollector.Reset_ForWorldChange(); _SelectedHandle = {};
    PublishRemoteCache();
}

auto FCkCrowdDebugger_ViewModel::PublishRemoteCache() -> void
{
    OnAgentListChanged.Broadcast(_DataCollector.Get_AllAgents());
    OnSelectedAgentChanged.Broadcast(FCk_Handle{}); // no foreign ECS identity is published
    OnAgentDataRefreshed.Broadcast(Get_SelectedSnapshot());
}

auto FCkCrowdDebugger_ViewModel::TickRemote(UWorld* World, APlayerController* PC) -> void
{
    auto* LP = IsValid(PC) ? PC->GetLocalPlayer() : nullptr;
    if (!ck_crowd_debugger_owner::IsExact(World, PC, LP))
    { Reset_ForWorldChange(); _IsRemote = true; _LastWorld = World; return; }
    if (!_IsRemote || _LastWorld.Get() != World || _RemotePC.Get() != PC || _RemoteLP.Get() != LP)
    {
        Reset_ForWorldChange(); _IsRemote = true; _LastWorld = World; _RemotePC = PC; _RemoteLP = LP;
        _RemotePawn = PC->GetPawn();
    }
    // Context retirement is processed before pause. Never receipt an old pawn's Accepted state.
    if (_RemotePawn.Get() != PC->GetPawn() ||
        (_RemoteEnvelope.Outcome == ECkCrowdRemoteSelectionOutcome::Accepted &&
         _RemoteEnvelope.WorldGeneration > 0 && !IsValid(PC->GetPawn())))
    {
        ClearRemotePresentation(); _RemotePawn = PC->GetPawn();
        _RequiresFreshPawnSelection = true; _PreviousPawnSelectionHigh = _SeenSelectionRequest;
    }
    if (_IsPaused) { return; }
    FCkCrowdRemoteSelectionEnvelope Candidate;
    if (!UCk_Utils_CrowdRemoteDebug_UE::TryCopySelectionEnvelope(PC, Candidate)) { return; }
    if (!FCkCrowdRemoteDebugValidation::IsValid(Candidate) || !HasExactRemoteOwner()) { return; }
    if (_HasPendingReceipt)
    {
        // Retries only the installed immutable delivery; do not poll/install a newer epoch.
        if (!Candidate.bRetainedDelivery || !FCkCrowdRemoteSelectionEnvelope::StaticStruct()->CompareScriptStruct(
            &Candidate, &_RemoteEnvelope, PPF_None)) { return; }
        if (UCk_Utils_CrowdRemoteDebug_UE::TryAcknowledgeSelection(PC, _RemoteEnvelope.Receipt))
        { _ConsumedDelivery = _RemoteEnvelope.Receipt.DeliverySerial; _HasPendingReceipt = false; }
        return;
    }
    if (_SeenWorldGeneration != 0 && Candidate.WorldGeneration != _SeenWorldGeneration) { return; }
    if (Candidate.SessionGeneration < _SeenSessionGeneration) { return; }
    const bool NewSession = Candidate.SessionGeneration > _SeenSessionGeneration;
    if (_RequiresFreshPawnSelection && !NewSession && Candidate.bRetainedDelivery &&
        Candidate.Outcome == ECkCrowdRemoteSelectionOutcome::Accepted &&
        Candidate.SelectionRequestSerial <= _PreviousPawnSelectionHigh) { return; }
    if (!NewSession)
    {
        if (Candidate.Outcome == ECkCrowdRemoteSelectionOutcome::Accepted &&
            Candidate.SelectionRequestSerial < _SeenSelectionRequest) { return; }
        if (!Candidate.bRetainedDelivery && Candidate.SampleSequence < _SeenSample) { return; }
        if (Candidate.bRetainedDelivery && Candidate.Receipt.DeliverySerial <= _ConsumedDelivery) { return; }
    }
    TArray<FCkCrowdDebugger_AgentSnapshot> Agents;
    if (!FCkCrowdDebugger_DataCollector::TryBuildRemoteAgents(Candidate, Agents) || !HasExactRemoteOwner()) { return; }
    // Entire validated rows/detail/feedback cache installs in one synchronous non-fallible step.
    _DataCollector.InstallRemoteAgents(MoveTemp(Agents));
    _RemoteEnvelope = MoveTemp(Candidate);
    // A complete continuous copy can retain the host's previous per-PC selection. Keep its
    // wire values intact, but do not expose that selected presentation under a replacement pawn.
    if (NewSession || (_RemoteEnvelope.Outcome == ECkCrowdRemoteSelectionOutcome::Accepted &&
        _RemoteEnvelope.SelectionRequestSerial > _PreviousPawnSelectionHigh)) { _RequiresFreshPawnSelection = false; }
    if (NewSession) { _SeenSelectionRequest = 0; _SeenSample = 0; _ConsumedDelivery = 0; }
    _SeenWorldGeneration = _RemoteEnvelope.WorldGeneration;
    _SeenSessionGeneration = _RemoteEnvelope.SessionGeneration;
    if (_RemoteEnvelope.Outcome == ECkCrowdRemoteSelectionOutcome::Accepted)
    { _SeenSelectionRequest = FMath::Max(_SeenSelectionRequest, _RemoteEnvelope.SelectionRequestSerial); }
    _SeenSample = FMath::Max(_SeenSample, _RemoteEnvelope.SampleSequence);
    _HasPendingReceipt = _RemoteEnvelope.bRetainedDelivery;
    PublishRemoteCache();
    if (_HasPendingReceipt && HasExactRemoteOwner() &&
        UCk_Utils_CrowdRemoteDebug_UE::TryAcknowledgeSelection(PC, _RemoteEnvelope.Receipt))
    { _ConsumedDelivery = _RemoteEnvelope.Receipt.DeliverySerial; _HasPendingReceipt = false; }
}

auto FCkCrowdDebugger_ViewModel::TrySelectRemoteAgent(const FCkCrowdDebugger_AgentSnapshot& Agent) -> bool
{
    if (!HasExactRemoteOwner() || _IsPaused || _HasPendingReceipt || !Agent.IsRemote ||
        !Agent.RemoteId.IsValid() || Agent.RemoteId.WorldGeneration != _RemoteEnvelope.WorldGeneration ||
        Agent.RemoteSession != _RemoteEnvelope.SessionGeneration || Agent.RemoteSample != _RemoteEnvelope.SampleSequence)
    { return false; }
    bool Found = false;
    for (const auto& Row : _DataCollector.Get_AllAgents())
    { if (Row.IsRemote && Row.RemoteId == Agent.RemoteId) { Found = true; break; } }
    if (!Found) { return false; }
    int64 Issued = 0;
    return UCk_Utils_CrowdRemoteDebug_UE::TryRequestNextSelection(_RemotePC.Get(), Agent.RemoteId, false, Issued);
}

auto FCkCrowdDebugger_ViewModel::TryClearRemoteSelection() -> bool
{
    if (!HasExactRemoteOwner() || _IsPaused || _HasPendingReceipt) { return false; }
    int64 Issued = 0;
    return UCk_Utils_CrowdRemoteDebug_UE::TryRequestNextSelection(_RemotePC.Get(), {}, true, Issued);
}

auto FCkCrowdDebugger_ViewModel::Get_RemoteSelectedId() const -> FCkCrowdRemoteAgentId
{ const auto* Selected = Get_SelectedSnapshot(); return Selected && Selected->IsRemote ? Selected->RemoteId : FCkCrowdRemoteAgentId{}; }
