#include "CkProceduralAnimationDebugger/Model/CkProceduralAnimationDebugger_Model.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Utils.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_debug_model
{
    auto
        Get_HasRosterChanged(
            const TArray<FCkProceduralAnimationDebugger_Row>& InBefore,
            const TArray<FCkProceduralAnimationDebugger_Row>& InAfter)
        -> bool
    {
        if (InBefore.Num() != InAfter.Num())
        { return true; }

        for (auto Index = 0; Index < InBefore.Num(); ++Index)
        {
            const auto& Before = InBefore[Index];
            const auto& After = InAfter[Index];
            if (NOT (Before.Entity == After.Entity) || Before.Label != After.Label || NOT (Before.Summary == After.Summary))
            { return true; }
        }
        return false;
    }
}

// --------------------------------------------------------------------------------------------------------------------

FCkProceduralAnimationDebugger_Model::
    FCkProceduralAnimationDebugger_Model()
{
    _SessionHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddRaw(this, &FCkProceduralAnimationDebugger_Model::Reset);
    _SelectionHandle = ck::DebugSelectionSync::Get_OnSelection().AddRaw(this, &FCkProceduralAnimationDebugger_Model::HandleSelection);
    _WorldInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnWorldInvalidated().AddRaw(this, &FCkProceduralAnimationDebugger_Model::HandleWorldInvalidated);
}

// --------------------------------------------------------------------------------------------------------------------

FCkProceduralAnimationDebugger_Model::
    ~FCkProceduralAnimationDebugger_Model()
{
    ReleaseSession();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Get_World() const
    -> UWorld*
{
    return _World.Get();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Set_World(
        UWorld* InWorld)
    -> void
{
    if (_Released || _World.Get() == InWorld)
    { return; }

    Reset();
    _World = InWorld;
    Refresh();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Refresh()
    -> void
{
    if (_Released)
    { return; }

    auto Rows = FCkProceduralAnimationDebugger_DataCollector::Collect(_World.Get());
    const auto RosterChanged = ck_procedural_debug_model::Get_HasRosterChanged(_Rows, Rows);
    _Rows = MoveTemp(Rows);

    const auto RevisionBefore = _HistoryRevision;
    const auto GoneBefore = _SelectedGone;
    if (NOT _SelectedId.IsEmpty())
    { DoCapture_Selected(); }

    if (RosterChanged || _HistoryRevision != RevisionBefore || _SelectedGone != GoneBefore)
    { _OnChanged.Broadcast(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Select(
        const FCk_Handle& InEntity,
        ECkProceduralAnimationDebugger_SelectionSync InSync)
    -> bool
{
    if (_Released)
    { return false; }

    const auto Target = ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InEntity,
        [](const FCk_Handle& InCandidate) -> bool
        {
            return FCkProceduralAnimationDebugger_DataCollector::Is_Supported(InCandidate);
        });

    if (ck::Is_NOT_Valid(Target))
    { return false; }

    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Target);
    if (_World.Get() != World)
    {
        DoClear();
        _World = World;
    }

    _Rows = FCkProceduralAnimationDebugger_DataCollector::Collect(World);
    const auto* Row = _Rows.FindByPredicate([&](const FCkProceduralAnimationDebugger_Row& InRow)
    {
        return InRow.Entity == Target;
    });

    if (Row == nullptr)
    {
        _OnChanged.Broadcast();
        return false;
    }

    if (_SelectedId != Row->Summary.Get_EntityId())
    {
        _History.Reset();
        _LiveSnapshot.Reset();
        _ActionStreams.Reset();
        _LimbControls.Reset();
        _ActionRecording.Reset({}, {});
        _ActionRecordingStopped = false;
        ++_HistoryRevision;
        _SelectedId = Row->Summary.Get_EntityId();
        _SelectedLegId.Reset();
    }

    if (UCk_Utils_ProceduralLeg_UE::Has(InEntity))
    { _SelectedLegId = InEntity.Get_Entity().ToString(); }

    _SelectedGone = false;
    DoCapture_Selected();
    _OnChanged.Broadcast();

    if (InSync == ECkProceduralAnimationDebugger_SelectionSync::Broadcast)
    { ck::DebugSelectionSync::Broadcast(Target, Get_SelectionSource()); }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_SelectLeg(
        const FString& InLegEntityId)
    -> void
{
    if (_Released || _SelectedLegId == InLegEntityId)
    { return; }

    _SelectedLegId = InLegEntityId;
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Get_SelectedHandle() const
    -> FCk_Handle
{
    const auto* Row = _Rows.FindByPredicate([&](const FCkProceduralAnimationDebugger_Row& InRow)
    {
        return InRow.Summary.Get_EntityId() == _SelectedId;
    });

    return Row != nullptr ? Row->Entity : FCk_Handle{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Get_SelectedLeg() const
    -> FCk_Handle_ProceduralLeg
{
    if (_Released || _SelectedLegId.IsEmpty())
    { return {}; }

    const auto Body = Get_SelectedHandle();
    if (ck::Is_NOT_Valid(Body))
    { return {}; }

    for (const auto& Leg : UCk_Utils_ProceduralLeg_UE::Get_Legs(Body))
    {
        if (Leg.Get_Entity().ToString() == _SelectedLegId)
        { return Leg; }
    }

    return {};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_EnableDisableSelectedLeg(
        ECk_EnableDisable InEnableDisable)
    -> bool
{
    auto Leg = Get_SelectedLeg();
    if (NOT UCk_Utils_ProceduralLeg_UE::Get_IsAttached(Leg))
    { return false; }

    UCk_Utils_ProceduralLeg_UE::Request_EnableDisable(Leg, FCk_Request_ProceduralLeg_EnableDisable{InEnableDisable}, {});
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_DetachSelectedLeg(
        ECk_ProceduralLeg_ReleasedPartsOwnership InPartsOwnership)
    -> bool
{
    auto Leg = Get_SelectedLeg();
    if (NOT UCk_Utils_ProceduralLeg_UE::Get_IsAttached(Leg))
    { return false; }

    UCk_Utils_ProceduralLeg_UE::Request_Detach(Leg, FCk_Request_ProceduralLeg_Detach{InPartsOwnership}, {});

    _SelectedLegId.Reset();
    _OnChanged.Broadcast();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Get_LiveStatus() const
    -> const FCk_ProceduralAnimation_DebugSnapshot*
{
    return _LiveSnapshot.IsSet() ? &_LiveSnapshot.GetValue() : nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Reset()
    -> void
{
    DoClear();
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    ReleaseSession()
    -> void
{
    if (_Released)
    { return; }

    _Released = true;
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionHandle);
    ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionHandle);
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Remove(_WorldInvalidatedHandle);
    Reset();
    _OnChanged.Clear();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_Hold()
    -> void
{
    if (_Released)
    { return; }

    _History.Hold();
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_GoLive()
    -> void
{
    if (_Released)
    { return; }

    _History.GoLive();
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_Scrub(
        int32 InIndex)
    -> bool
{
    if (_Released || NOT _History.Scrub(InIndex))
    { return false; }

    _OnChanged.Broadcast();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_ScrubBySequence(
        uint64 InSequence)
    -> bool
{
    if (_Released || NOT _History.Scrub_BySequence(InSequence))
    { return false; }

    _OnChanged.Broadcast();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    DoClear()
    -> void
{
    _Rows.Reset();
    _LiveSnapshot.Reset();
    _History.Reset();
    _ActionStreams.Reset();
    _LimbControls.Reset();
    _ActionRecording.Reset({}, {});
    _ActionRecordingStopped = false;
    ++_HistoryRevision;
    _SelectedId.Reset();
    _SelectedLegId.Reset();
    _SelectedGone = false;
    _World.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    DoCapture_Selected()
    -> void
{
    const auto* Row = _Rows.FindByPredicate([&](const FCkProceduralAnimationDebugger_Row& InRow)
    {
        return InRow.Summary.Get_EntityId() == _SelectedId;
    });

    _SelectedGone = Row == nullptr;
    if (_SelectedGone)
    {
        _LiveSnapshot.Reset();
        return;
    }

    auto Controls = FCkProceduralAnimationDebugger_DataCollector::Collect_LimbControls(Row->Entity);
    if (Controls != _LimbControls)
    {
        _LimbControls = MoveTemp(Controls);
        ++_HistoryRevision;
    }
    DoCapture_Actions(Row->Entity);
    if (NOT Row->Summary.Get_HasGait())
    {
        _LiveSnapshot.Reset();
        return;
    }

    auto Snapshot = UCk_Utils_ProceduralAnimation_Debug_UE::Get_Snapshot(Row->Entity);
    if (_History.Push(Snapshot))
    { ++_HistoryRevision; }

    _LiveSnapshot = MoveTemp(Snapshot);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_SelectAction(
        FCk_ProceduralAction_Identity InIdentity)
    -> bool
{
    if (_Released || NOT _ActionStreams.ContainsByPredicate([InIdentity](const auto& InCandidate)
        { return InCandidate.Get_Value() == InIdentity.Get_Value(); }))
    { return false; }
    if (_ActionRecording.Get_Identity().Get_Value() == InIdentity.Get_Value())
    { return true; }

    DoSelect_Action(InIdentity);
    _OnChanged.Broadcast();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    DoSelect_Action(
        FCk_ProceduralAction_Identity InIdentity)
    -> void
{
    auto Provenance = FCk_ProceduralActionRecordedStore::FProvenance{};
    Provenance.Set_Fixture(_SelectedId);
    Provenance.Set_InputDescription(TEXT("Live public receipt stream; gameplay may consume records before capture."));
    _ActionRecording.Reset(InIdentity, MoveTemp(Provenance));
    _ActionRecordingStopped = false;
    ++_HistoryRevision;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    DoCapture_Actions(
        const FCk_Handle& InSubject)
    -> void
{
    if (NOT UCk_Utils_ProceduralActionPlayer_UE::Has(InSubject))
    { return; }

    const auto Player = UCk_Utils_ProceduralActionPlayer_UE::CastChecked(InSubject);
    auto Streams = UCk_Utils_ProceduralActionPlayer_UE::Get_RetainedReceiptIdentities(Player);
    auto Changed = Streams.Num() != _ActionStreams.Num();
    if (NOT Changed)
    {
        for (auto Index = 0; Index < Streams.Num(); ++Index)
        { Changed |= Streams[Index].Get_Value() != _ActionStreams[Index].Get_Value(); }
    }
    _ActionStreams = MoveTemp(Streams);
    if (Changed)
    { ++_HistoryRevision; }

    if (_ActionRecording.Get_Identity().Get_Value() <= 0 && NOT _ActionStreams.IsEmpty())
    { DoSelect_Action(_ActionStreams.Last()); }
    if (_ActionRecording.Get_Identity().Get_Value() <= 0 || _ActionRecordingStopped)
    { return; }

    // One bounded page for the selected stream per refresh. Reading never consumes gameplay receipts.
    const auto Page = UCk_Utils_ProceduralActionPlayer_UE::Read_Receipts(Player,
        _ActionRecording.Get_Identity(), _ActionRecording.Get_LastCopiedSequence(), 64);
    const auto BeforeSequence = _ActionRecording.Get_LastCopiedSequence();
    const auto BeforeResult = _ActionRecording.Get_LastReadResult();
    const auto BeforeTerminal = _ActionRecording.Get_Terminal();
    _ActionRecordingStopped = NOT _ActionRecording.AppendPage(Page);
    if (_ActionRecordingStopped || BeforeSequence != _ActionRecording.Get_LastCopiedSequence()
        || BeforeResult != _ActionRecording.Get_LastReadResult() || BeforeTerminal != _ActionRecording.Get_Terminal())
    { ++_HistoryRevision; }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    HandleSelection(
        const FCk_Handle& InEntity,
        FName InSource)
    -> void
{
    if (_Released || InSource == Get_SelectionSource())
    { return; }

    const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
    Select(InEntity, ECkProceduralAnimationDebugger_SelectionSync::Silent);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    HandleWorldInvalidated(
        UWorld* InWorld)
    -> void
{
    if (_World.Get() == InWorld)
    { Reset(); }
}

// --------------------------------------------------------------------------------------------------------------------
