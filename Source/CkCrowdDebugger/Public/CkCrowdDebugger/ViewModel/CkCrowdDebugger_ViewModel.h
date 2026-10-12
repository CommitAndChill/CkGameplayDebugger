#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.h"
#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "CkEcs/Handle/CkHandle.h"

#include "Delegates/Delegate.h"

class UWorld;
class APlayerController;
class ULocalPlayer;
class APawn;

// --------------------------------------------------------------------------------------------------------------------
// MVVM-style view model. Plain C++ (not UObject). Owned by the window. Owns
// the DataCollector and exposes 5 multicast delegates that panels bind to.
// Mirrors the CkGoapDebugger ViewModel pattern.
// --------------------------------------------------------------------------------------------------------------------

enum class ECk_CrowdDebugger_ViewMode : uint8
{
	Live,
	Scrub,    // reserved; only Live is implemented in v1
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_AgentListChanged,    const TArray<FCkCrowdDebugger_AgentSnapshot>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_AgentDataRefreshed,  const FCkCrowdDebugger_AgentSnapshot*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_SelectedAgentChanged,FCk_Handle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_PausedChanged,       bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_ViewModeChanged,     ECk_CrowdDebugger_ViewMode);

// --------------------------------------------------------------------------------------------------------------------

class CKCROWDDEBUGGER_API FCkCrowdDebugger_ViewModel
{
public:
	auto Tick(UWorld* InWorld, float InDeltaTime, APlayerController* InOwningPC = nullptr) -> void;
	auto Get_IsRemote() const -> bool { return _IsRemote; }
	// Exact copied protocol value; presentation selection separately respects pawn admission.
	auto Get_RemoteEnvelope() const -> const FCkCrowdRemoteSelectionEnvelope& { return _RemoteEnvelope; }
	auto Get_RemoteSelectedId() const -> FCkCrowdRemoteAgentId;
	auto TrySelectRemoteAgent(const FCkCrowdDebugger_AgentSnapshot& InAgent) -> bool;
	auto TryClearRemoteSelection() -> bool;
	auto Reset_ForWorldChange() -> void;

	// Run a synthetic FindPathSync probe via the DataCollector. The world is the
	// last one passed to Tick; if there isn't one (no PIE / world torn down), the
	// probe records a NoWorld failure so the panel still updates.
	auto Run_HealthCheckProbe() -> void;

	// Selection
	auto Set_SelectedHandle(FCk_Handle InHandle) -> void;
	auto Get_SelectedHandle() const -> FCk_Handle { return _SelectedHandle; }
	auto Get_SelectedSnapshot() const -> const FCkCrowdDebugger_AgentSnapshot*;

	// One-shot "frame the selected agent" request (e.g. a cross-debugger sync resolved to an
	// agent). The window listens and applies the viewport's FrameSelection camera preset.
	auto Request_FrameSelectedAgent() -> void { OnFrameSelectedAgentRequested.Broadcast(); }

	// All agents
	auto Get_AllAgents() const -> const TArray<FCkCrowdDebugger_AgentSnapshot>&
	{ return _DataCollector.Get_AllAgents(); }

	auto Get_AgentCount() const -> int32 { return _DataCollector.Get_AgentCount(); }

	// Navmesh
	auto Get_NavmeshStatus() const -> const FCkCrowdDebugger_NavmeshStatus&
	{ return _DataCollector.Get_NavmeshStatus(); }

	auto Get_NavTriVerts() const -> const TArray<FVector>&
	{ return _DataCollector.Get_NavTriVerts(); }
	auto Get_NavGeometryRevision() const -> uint64
	{ return _DataCollector.Get_NavGeometryRevision(); }
	auto Get_PathNetworkRibbons() const -> const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>&
	{ return _DataCollector.Get_PathNetworkRibbons(); }
	auto Get_Queues() const -> const TArray<FCkCrowdDebugger_QueueSnapshot>&
	{ return _DataCollector.Get_Queues(); }
	auto Get_AvoidanceVolumes() const -> const TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>&
	{ return _DataCollector.Get_AvoidanceVolumes(); }

	// Pause
	auto Set_Paused(bool InPaused) -> void;
	auto Get_Paused() const -> bool { return _IsPaused; }

	// View Mode
	auto Set_ViewMode(ECk_CrowdDebugger_ViewMode InMode) -> void;
	auto Get_ViewMode() const -> ECk_CrowdDebugger_ViewMode { return _ViewMode; }

public:
	FOnCrowd_AgentListChanged     OnAgentListChanged;
	FOnCrowd_AgentDataRefreshed   OnAgentDataRefreshed;
	FOnCrowd_SelectedAgentChanged OnSelectedAgentChanged;
	FOnCrowd_PausedChanged        OnPausedChanged;
	FOnCrowd_ViewModeChanged      OnViewModeChanged;
	FSimpleMulticastDelegate      OnFrameSelectedAgentRequested;

private:
	auto TickRemote(UWorld* InWorld, APlayerController* InOwningPC) -> void;
	auto ClearRemotePresentation() -> void;
	auto PublishRemoteCache() -> void;
	auto HasExactRemoteOwner() const -> bool;
	bool _IsRemote = false, _HasPendingReceipt = false, _RequiresFreshPawnSelection = false;
	int64 _PreviousPawnSelectionHigh = 0;
	FCkCrowdRemoteSelectionEnvelope _RemoteEnvelope;
	TWeakObjectPtr<APlayerController> _RemotePC;
	TWeakObjectPtr<ULocalPlayer> _RemoteLP;
	TWeakObjectPtr<APawn> _RemotePawn;
	int64 _SeenWorldGeneration = 0, _SeenSessionGeneration = 0;
	int64 _SeenSelectionRequest = 0, _SeenSample = 0, _ConsumedDelivery = 0;
	FCkCrowdDebugger_DataCollector _DataCollector;
	FCk_Handle _SelectedHandle;
	bool _IsPaused = false;
	int32 _LastAgentCount = -1;
	ECk_CrowdDebugger_ViewMode _ViewMode = ECk_CrowdDebugger_ViewMode::Live;

	// Cached on each Tick so the Run_HealthCheckProbe can grab the current world
	// without the button click handler having to re-find one.
	TWeakObjectPtr<UWorld> _LastWorld;
};

// --------------------------------------------------------------------------------------------------------------------
