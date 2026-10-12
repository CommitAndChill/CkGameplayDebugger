#pragma once

#include "Widgets/SCompoundWidget.h"
#include "CkCrowd/Debug/CkCrowd_RemoteDebugContract.h"

class FCkCrowdDebugger_ViewModel;
class SCkDebug_EventLog;

struct FCkCrowdDebugger_AgentSnapshot;

// --------------------------------------------------------------------------------------------------------------------
// Crowd event log.
//
// The panel is a thin feed on top of the shared SCkDebug_EventLog: it watches the collector's
// per-agent path-trouble records (the only event stream CkCrowdDebugger actually collects today)
// and appends one line per NEW event. Nothing here reaches into CkFoundation — every field read is
// already in FCkCrowdDebugger_AgentSnapshot.
//
// No FCk_Handle is retained. Remote rows use private, never-reused UI tokens for typed wire identities.
// Native tokens resolve to the entity's original type hash and the click
// handler resolves it against the view model's live agent list, so a dead PIE registry can't be
// dereferenced through this panel.
// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_EventLogPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_EventLogPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	~SCkCrowdDebugger_EventLogPanel();

private:
	auto Handle_AgentListChanged(const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents) -> void;
	auto Handle_SessionInvalidated() -> void;
	auto Handle_EntrySelected(int32 InSelectionId) -> void;
	auto Handle_RemoteAgentListChanged(const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents) -> void;
	auto Reset_RemoteDomain() -> void;
	auto Get_RemoteSelectedToken() const -> int32;
	auto TryGet_RemoteToken(const FCkCrowdRemoteAgentId& InId) -> int32;
	auto TryGet_NativeToken(int32 InSubjectHash) -> int32;
	auto TryAllocate_SelectionToken() -> int32;
	auto Prune_NativeMappings(const TSet<int32>& InCurrentIds) -> void;
	auto Prune_RemoteMappings(const TSet<FCkCrowdRemoteAgentId>& InCurrentIds) -> void;

	/** Native subject key. Widget selection IDs are private tokens; the ECS target remains this hash. */
	static auto Compute_SelectionId(const FCkCrowdDebugger_AgentSnapshot& InAgent) -> int32;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	TSharedPtr<SCkDebug_EventLog> _EventLog;

	FDelegateHandle _OnAgentListChangedHandle;
	FDelegateHandle _OnSessionInvalidatedHandle;

	// Last path-trouble timestamp already turned into a line, per agent selection id. Plain data —
	// no handles — so it survives a registry teardown without holding anything alive.
	TMap<int32, double> _LastLoggedEventTimeById;

	struct FRemoteSubject
	{
		FCkCrowdRemoteAgentId Id;
		int64 SessionGeneration = 0;
	};
	bool _RemoteDomainActive = false, _NativeDomainActive = false, _SelectionTokenExhausted = false;
	int64 _RemoteWorldGeneration = 0, _RemoteSessionGeneration = 0;
	// Never reset or reuse while this panel can receive a delayed widget callback.
	int64 _NextSelectionToken = 0;
	TMap<FCkCrowdRemoteAgentId, int32> _RemoteTokensById;
	TMap<int32, FRemoteSubject> _RemoteSubjectsByToken;
	TMap<FCkCrowdRemoteAgentId, double> _RemoteLastLoggedTimeById;
	TArray<int32> _RemoteHistoryTokens;
	TArray<FCkCrowdRemoteAgentId> _RemoteHistoryIds;
	TMap<int32, int32> _NativeTokensById, _NativeIdsByToken;
	TArray<int32> _NativeHistoryTokens;
};

// --------------------------------------------------------------------------------------------------------------------
