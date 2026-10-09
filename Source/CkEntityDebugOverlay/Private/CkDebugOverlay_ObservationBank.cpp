#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_ObservationBank.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_SourceAdmission.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Fragment.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkTimer/CkTimer_Fragment.h"
#include "CkAttribute/IntegerAttribute/CkIntegerAttribute_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "Engine/World.h"

namespace
{
    auto IsLive(const FCk_Handle& Handle) -> bool
    {
        return ck::IsValid(Handle) &&
            NOT Handle.Has<ck::FTag_DestroyEntity_Initiate>() &&
            NOT Handle.Has<ck::FTag_DestroyEntity_EndPlay>() &&
            NOT Handle.Has<ck::FTag_DestroyEntity_Teardown>() &&
            NOT Handle.Has<ck::FTag_DestroyEntity_Await>() &&
            NOT Handle.Has<ck::FTag_DestroyEntity_Finalize>();
    }

    auto IsLiveClientWorld(const UWorld* World) -> bool
    {
        return IsValid(World) &&
            NOT World->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject |
                RF_BeginDestroyed | RF_FinishDestroyed) &&
            World->IsGameWorld() && World->GetNetMode() == NM_Client &&
            NOT World->bIsTearingDown && NOT World->IsBeingCleanedUp();
    }

    auto HasExactWorld(const FCk_Handle& WorldRoot, UWorld* World) -> bool
    {
        if (NOT IsLiveClientWorld(World) || NOT IsLive(WorldRoot) ||
            NOT WorldRoot.Has<TWeakObjectPtr<UWorld>>())
        { return false; }
        return WorldRoot.Get<TWeakObjectPtr<UWorld>>().Get() == World &&
            UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World) == WorldRoot;
    }

    auto IsSameRegistry(const FCk_Handle& A, const FCk_Handle& B) -> bool
    {
        return A.Get_RegistryView().Get_RegistryHandle() ==
            B.Get_RegistryView().Get_RegistryHandle();
    }

    auto IsSupportedState(ECk_Timer_State State) -> bool
    {
        return State == ECk_Timer_State::Paused || State == ECk_Timer_State::Running;
    }

    auto IsSupportedStatus(ECk_SmRunStatus Status) -> bool
    {
        return Status == ECk_SmRunStatus::Stopped ||
            Status == ECk_SmRunStatus::Running ||
            Status == ECk_SmRunStatus::Paused;
    }

    auto IsPassiveLocalSource(const FCk_Handle& Source) -> bool
    {
        if (NOT IsLive(Source))
        { return false; }
        // A direct WorldTransient child intentionally receives no copied NetParams.
        // Absent params cannot replicate; present params must explicitly be DNR.
        if (Source.Has<ck::FFragment_Net_Params>() &&
            Source.Get<ck::FFragment_Net_Params>()
                .Get_ConnectionSettings().Get_Replication() !=
                    ECk_Replication::DoesNotReplicate)
        { return false; }
        return NOT Source.Has_Any<
            ck::FFragment_RecordOfTimers, ck::FFragment_Timer,
            ck::FFragment_Timer_Params, ck::FFragment_Timer_Requests,
            ck::FTag_Timer_NeedsSetup, ck::FTag_Timer_NeedsUpdate,
            ck::FTag_Timer_Countdown,
            ck::FFragment_RecordOfIntegerAttributes,
            ck::FFragment_IntegerAttribute, ck::FFragment_IntegerAttribute_Min,
            ck::FFragment_IntegerAttribute_Max,
            ck::FFragment_IntegerAttributeModifier,
            ck::FFragment_IntegerAttributeModifier_Min,
            ck::FFragment_IntegerAttributeModifier_Max,
            ck::FFragment_Sm, ck::FFragment_Sm_Params,
            ck::FFragment_Sm_Requests, ck::FFragment_Sm_PendingTransition,
            ck::FTag_Sm_RequiresSetup, ck::FTag_Sm_Running,
            ck::FTag_Sm_Paused>();
    }

    auto SameFacts(const FCk_DebugOverlay_SourceObservation& A,
                   const FCk_DebugOverlay_SourceObservation& B) -> bool
    {
        if (A.Source != B.Source || A.Timers.Num() != B.Timers.Num() ||
            A.Integers.Num() != B.Integers.Num() ||
            A.HasSm != B.HasSm ||
            A.SmStates.Num() != B.SmStates.Num() ||
            A.HasDebugHistory != B.HasDebugHistory ||
            A.SmHistory.Num() != B.SmHistory.Num())
        { return false; }
        for (int32 I = 0; I < A.Timers.Num(); ++I)
        {
            if (A.Timers[I].Name != B.Timers[I].Name ||
                A.Timers[I].State != B.Timers[I].State)
            { return false; }
        }
        for (int32 I = 0; I < A.Integers.Num(); ++I)
        {
            if (A.Integers[I].Label != B.Integers[I].Label ||
                A.Integers[I].Base != B.Integers[I].Base ||
                A.Integers[I].Final != B.Integers[I].Final)
            { return false; }
        }
        for (int32 I = 0; I < A.SmStates.Num(); ++I)
        {
            if (A.SmStates[I].Depth != B.SmStates[I].Depth ||
                A.SmStates[I].StateName != B.SmStates[I].StateName ||
                A.SmStates[I].Status != B.SmStates[I].Status)
            { return false; }
        }
        for (int32 I = 0; I < A.SmHistory.Num(); ++I)
        {
            if (A.SmHistory[I].FromStateName != B.SmHistory[I].FromStateName ||
                A.SmHistory[I].ToStateName != B.SmHistory[I].ToStateName)
            { return false; }
        }
        return true;
    }
}

auto FCk_DebugOverlay_ObservationBankUtils::TryPublish(
    UWorld* World,
    const FCk_Handle& WorldRoot,
    const FCk_Handle& ScenarioRoot,
    uint64 Generation,
    uint64 Sequence,
    const TArray<FCk_DebugOverlay_SourceObservation>& Sources) -> bool
{
    if (NOT IsInGameThread() || NOT IsLiveClientWorld(World) ||
        Generation == 0 || Sequence == 0 || NOT HasExactWorld(WorldRoot, World) ||
        NOT IsLive(ScenarioRoot) || NOT IsSameRegistry(ScenarioRoot, WorldRoot) ||
        ScenarioRoot == WorldRoot)
    { return false; }

    if (ScenarioRoot.Has<TWeakObjectPtr<UWorld>>() &&
        ScenarioRoot.Get<TWeakObjectPtr<UWorld>>().Get() != World)
    { return false; }

    auto Candidate = FCk_DebugOverlay_ObservationBank{};
    Candidate.ScenarioRoot = ScenarioRoot;
    Candidate.Generation = Generation;
    Candidate.Sequence = Sequence;

    for (const auto& Row : Sources)
    {
        if (NOT IsPassiveLocalSource(Row.Source) ||
            NOT IsSameRegistry(Row.Source, WorldRoot) ||
            Row.Source == WorldRoot || Candidate.Sources.Contains(Row.Source))
        { return false; }
        if (Row.Source.Has<TWeakObjectPtr<UWorld>>() &&
            Row.Source.Get<TWeakObjectPtr<UWorld>>().Get() != World)
        { return false; }
        for (const auto& Fact : Row.Timers)
        {
            if (NOT IsSupportedState(Fact.State))
            { return false; }
        }
        for (const auto& Fact : Row.SmStates)
        {
            if (Fact.Depth < 0 || NOT IsSupportedStatus(Fact.Status))
            { return false; }
        }
        if ((NOT Row.HasSm &&
                (NOT Row.SmStates.IsEmpty() || Row.HasDebugHistory ||
                    NOT Row.SmHistory.IsEmpty())) ||
            (Row.HasSm && (Row.SmStates.IsEmpty() ||
                Row.SmStates[0].Depth != 0)) ||
            (NOT Row.HasDebugHistory && NOT Row.SmHistory.IsEmpty()))
        { return false; }
        for (int32 Index = 1; Index < Row.SmStates.Num(); ++Index)
        {
            const int32 Depth = Row.SmStates[Index].Depth;
            if (Depth < 1 || Depth > Index ||
                Depth > Row.SmStates[Index - 1].Depth + 1)
            { return false; }
        }
        Candidate.Sources.Add(Row.Source, Row);
    }

    // Same-epoch mutation is rejected. Exact duplicate publication is idempotent.
    if (WorldRoot.Has<FCk_DebugOverlay_ObservationBank>())
    {
        const auto& Current = WorldRoot.Get<FCk_DebugOverlay_ObservationBank>();
        if (Generation < Current.Generation ||
            (Generation == Current.Generation && Sequence < Current.Sequence))
        { return false; }
        if (Generation == Current.Generation && Sequence == Current.Sequence)
        {
            if (Current.ScenarioRoot != ScenarioRoot)
            { return false; }
            if (Current.Sources.Num() != Candidate.Sources.Num())
            { return false; }
            for (const auto& Pair : Candidate.Sources)
            {
                const auto* Old = Current.Sources.Find(Pair.Key);
                if (Old == nullptr || NOT SameFacts(*Old, Pair.Value))
                { return false; }
            }
            return true;
        }
    }

    // All input checks precede the sole publication mutation.
    if (NOT HasExactWorld(WorldRoot, World) || NOT IsLive(ScenarioRoot))
    { return false; }
    auto MutableWorldRoot = WorldRoot;
    MutableWorldRoot.AddOrReplace<FCk_DebugOverlay_ObservationBank>(MoveTemp(Candidate));
    return true;
}

auto FCk_DebugOverlay_ObservationBankUtils::TryGetPublishedSource(
    const FCk_Handle& LocalSource,
    FCk_DebugOverlay_SourceObservation& OutObservation) -> bool
{
    // OutObservation.Source may alias LocalSource. Keep the input before clearing output.
    const auto Source = LocalSource;
    OutObservation = {};
    if (NOT IsInGameThread() || NOT IsPassiveLocalSource(Source) ||
        NOT UCk_DebugOverlay_SourceAdmission_UE::IsAdmitted(Source))
    { return false; }

    const auto WorldRoot = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(Source);
    if (NOT IsLive(WorldRoot) || NOT WorldRoot.Has<TWeakObjectPtr<UWorld>>() ||
        NOT WorldRoot.Has<FCk_DebugOverlay_ObservationBank>())
    { return false; }
    auto* World = WorldRoot.Get<TWeakObjectPtr<UWorld>>().Get();
    if (NOT HasExactWorld(WorldRoot, World))
    { return false; }

    const auto& Bank = WorldRoot.Get<FCk_DebugOverlay_ObservationBank>();
    if (Bank.Generation == 0 || Bank.Sequence == 0 || NOT IsLive(Bank.ScenarioRoot) ||
        NOT IsSameRegistry(Bank.ScenarioRoot, WorldRoot))
    { return false; }
    if (Bank.ScenarioRoot.Has<TWeakObjectPtr<UWorld>>() &&
        Bank.ScenarioRoot.Get<TWeakObjectPtr<UWorld>>().Get() != World)
    { return false; }
    if (Source.Has<TWeakObjectPtr<UWorld>>() &&
        Source.Get<TWeakObjectPtr<UWorld>>().Get() != World)
    { return false; }
    const auto* Found = Bank.Sources.Find(Source);
    if (Found == nullptr)
    { return false; }
    OutObservation = *Found;
    return true;
}
