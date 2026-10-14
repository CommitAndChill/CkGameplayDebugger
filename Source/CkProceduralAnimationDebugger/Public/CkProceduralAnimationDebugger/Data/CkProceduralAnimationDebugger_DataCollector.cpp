#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Utils.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Utils.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_LimbControl::
    operator==(
        const FCkProceduralAnimationDebugger_LimbControl& InOther) const
    -> bool
{
    return Get_LimbId() == InOther.Get_LimbId() && Get_OwnerId() == InOther.Get_OwnerId()
        && Get_State() == InOther.Get_State() && Get_Generation() == InOther.Get_Generation()
        && Get_Locomotor() == InOther.Get_Locomotor() && Get_Participating() == InOther.Get_Participating()
        && Get_TrustedSupport() == InOther.Get_TrustedSupport() && Get_Reason() == InOther.Get_Reason();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_DataCollector::
    Collect_LimbControls(
        const FCk_Handle& InSubject)
    -> TArray<FCkProceduralAnimationDebugger_LimbControl>
{
    auto Result = TArray<FCkProceduralAnimationDebugger_LimbControl>{};
    if (NOT Is_Supported(InSubject) || NOT UCk_Utils_ProceduralArticulation_UE::Has(InSubject))
    { return Result; }

    const auto Articulation = UCk_Utils_ProceduralArticulation_UE::CastChecked(InSubject);
    for (const auto& Limb : UCk_Utils_ProceduralArticulation_UE::Get_Limbs(Articulation))
    {
        const auto Id = UCk_Utils_ProceduralLimb_UE::Get_Id(Limb);
        const auto Observation = UCk_Utils_ProceduralArticulation_UE::Get_LimbControl(Articulation, Id);
        auto Copy = FCkProceduralAnimationDebugger_LimbControl{};
        Copy.Set_LimbId(Id)
            .Set_OwnerId(ck::IsValid(Observation.Get_Owner()) ? Observation.Get_Owner().Get_Entity().ToString() : FString{})
            .Set_State(Observation.Get_State()).Set_Generation(Observation.Get_Generation())
            .Set_Locomotor(Observation.Get_Locomotor()).Set_Participating(Observation.Get_Participating())
            .Set_TrustedSupport(Observation.Get_TrustedSupport()).Set_Reason(Observation.Get_Reason());
        Result.Add(MoveTemp(Copy));
    }
    return Result;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_debug_collector
{
    auto
        DoAggregate_Status(
            ECk_ProceduralAnimation_Status InAggregate,
            ECk_ProceduralAnimation_Status InRig)
        -> ECk_ProceduralAnimation_Status
    {
        if (InAggregate == ECk_ProceduralAnimation_Status::Failed || InRig == ECk_ProceduralAnimation_Status::Failed)
        { return ECk_ProceduralAnimation_Status::Failed; }

        if (InAggregate == ECk_ProceduralAnimation_Status::PendingSetup || InRig == ECk_ProceduralAnimation_Status::PendingSetup)
        { return ECk_ProceduralAnimation_Status::PendingSetup; }

        return ECk_ProceduralAnimation_Status::Ready;
    }

    auto
        Make_Summary(
            const FCk_Handle& InBody)
        -> FCkProceduralAnimationDebugger_Summary
    {
        auto Summary = FCkProceduralAnimationDebugger_Summary{};
        Summary.Set_EntityName(InBody.Get_DebugName())
            .Set_EntityId(InBody.Get_Entity().ToString())
            .Set_HasGait(UCk_Utils_ProceduralGait_UE::Has(InBody))
            .Set_HasArticulation(UCk_Utils_ProceduralArticulation_UE::Has(InBody))
            .Set_HasPlayer(UCk_Utils_ProceduralActionPlayer_UE::Has(InBody));
        if (Summary.Get_HasArticulation())
        {
            const auto Articulation = UCk_Utils_ProceduralArticulation_UE::CastChecked(InBody);
            Summary.Set_LimbCount(UCk_Utils_ProceduralArticulation_UE::Get_Limbs(Articulation).Num())
                .Set_ArticulationStatus(UCk_Utils_ProceduralArticulation_UE::Get_Status(Articulation));
        }
        if (NOT Summary.Get_HasGait())
        { return Summary; }

        const auto Gait = UCk_Utils_ProceduralGait_UE::CastChecked(InBody);
        const auto Legs = UCk_Utils_ProceduralGait_UE::Get_Legs(Gait, ECk_ProceduralLeg_Filter::NoFilter);

        auto HasRig = false;
        auto RigStatus = ECk_ProceduralAnimation_Status::Ready;
        for (const auto& Leg : Legs)
        {
            const auto Rig = UCk_Utils_ProceduralRig_UE::Cast(Leg);
            if (ck::Is_NOT_Valid(Rig))
            { continue; }

            HasRig = true;
            RigStatus = DoAggregate_Status(RigStatus, UCk_Utils_ProceduralRig_UE::Get_Status(Rig));
        }

        Summary.Set_EntityName(InBody.Get_DebugName())
            .Set_EntityId(InBody.Get_Entity().ToString())
            .Set_GaitStatus(UCk_Utils_ProceduralGait_UE::Get_Status(Gait))
            .Set_HasRig(HasRig)
            .Set_RigStatus(HasRig ? RigStatus : ECk_ProceduralAnimation_Status::PendingSetup)
            .Set_LegCount(Legs.Num())
            .Set_AttachedLegCount(UCk_Utils_ProceduralGait_UE::Get_Legs(Gait, ECk_ProceduralLeg_Filter::OnlyAttached).Num())
            .Set_EnabledLegCount(UCk_Utils_ProceduralGait_UE::Get_EnabledLegCount(Gait))
            .Set_PlantedCount(UCk_Utils_ProceduralGait_UE::Get_PlantedCount(Gait));
        return Summary;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Summary::
    operator==(
        const FCkProceduralAnimationDebugger_Summary& InOther) const
    -> bool
{
    return _EntityName == InOther._EntityName
        && _EntityId == InOther._EntityId
        && _HasGait == InOther._HasGait
        && _HasArticulation == InOther._HasArticulation
        && _HasPlayer == InOther._HasPlayer
        && _LimbCount == InOther._LimbCount
        && _ArticulationStatus == InOther._ArticulationStatus
        && _GaitStatus == InOther._GaitStatus
        && _HasRig == InOther._HasRig
        && _RigStatus == InOther._RigStatus
        && _LegCount == InOther._LegCount
        && _AttachedLegCount == InOther._AttachedLegCount
        && _EnabledLegCount == InOther._EnabledLegCount
        && _PlantedCount == InOther._PlantedCount;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> TArray<FCkProceduralAnimationDebugger_Row>
{
    auto Rows = TArray<FCkProceduralAnimationDebugger_Row>{};
    if (ck::Is_NOT_Valid(InWorld) || NOT InWorld->HasBegunPlay())
    { return Rows; }

    auto Entities = UCk_Utils_ProceduralAnimation_Debug_UE::Get_Articulations(InWorld);
    for (const auto& Gait : UCk_Utils_ProceduralAnimation_Debug_UE::Get_Entities(InWorld))
    {
        if (NOT UCk_Utils_ProceduralArticulation_UE::Has(Gait))
        { Entities.Add(Gait); }
    }

    for (const auto& Entity : Entities)
    {
        auto Row = FCkProceduralAnimationDebugger_Row{};
        Row.Entity = Entity;
        Row.Summary = ck_procedural_debug_collector::Make_Summary(Entity);
        Row.Label = Row.Summary.Get_EntityName().IsNone() ? Row.Summary.Get_EntityId() : Row.Summary.Get_EntityName().ToString();
        Rows.Add(MoveTemp(Row));
    }

    Rows.Sort([](const FCkProceduralAnimationDebugger_Row& InA, const FCkProceduralAnimationDebugger_Row& InB)
    {
        const auto NameOrder = InA.Label.Compare(InB.Label);
        return NameOrder == 0 ? InA.Summary.Get_EntityId() < InB.Summary.Get_EntityId() : NameOrder < 0;
    });
    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_DataCollector::
    Is_Supported(
        const FCk_Handle& InEntity)
    -> bool
{
    return ck::IsValid(InEntity)
        && NOT InEntity.Has<ck::FTag_DestroyEntity_Initiate>()
        && (UCk_Utils_ProceduralGait_UE::Has(InEntity) || UCk_Utils_ProceduralArticulation_UE::Has(InEntity));
}

// --------------------------------------------------------------------------------------------------------------------
