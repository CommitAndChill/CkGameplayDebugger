#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkProceduralAnimation/CkProceduralAnimation_Fragment_Data.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// A copied control observation. Owner identity is text so the view never prolongs an ECS registry's lifetime.
struct CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_LimbControl
{
    CK_GENERATED_BODY(FCkProceduralAnimationDebugger_LimbControl);

private:
    FName _LimbId;
    FString _OwnerId;
    ECk_ProceduralLimbControl_State _State = ECk_ProceduralLimbControl_State::Unavailable;
    int64 _Generation = 0;
    bool _Locomotor = false;
    bool _Participating = false;
    bool _TrustedSupport = false;
    FName _Reason;

public:
    CK_PROPERTY(_LimbId);
    CK_PROPERTY(_OwnerId);
    CK_PROPERTY(_State);
    CK_PROPERTY(_Generation);
    CK_PROPERTY(_Locomotor);
    CK_PROPERTY(_Participating);
    CK_PROPERTY(_TrustedSupport);
    CK_PROPERTY(_Reason);

    auto operator==(const FCkProceduralAnimationDebugger_LimbControl& InOther) const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

// Roster values read through the public feature utils; only the selected entity pays for a full snapshot.
struct CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Summary
{
    CK_GENERATED_BODY(FCkProceduralAnimationDebugger_Summary);

private:
    FName _EntityName;
    FString _EntityId;
    bool _HasGait = false;
    bool _HasArticulation = false;
    bool _HasPlayer = false;
    int32 _LimbCount = 0;
    ECk_ProceduralAnimation_Status _ArticulationStatus = ECk_ProceduralAnimation_Status::PendingSetup;
    ECk_ProceduralAnimation_Status _GaitStatus = ECk_ProceduralAnimation_Status::PendingSetup;
    bool _HasRig = false;
    ECk_ProceduralAnimation_Status _RigStatus = ECk_ProceduralAnimation_Status::PendingSetup;
    int32 _LegCount = 0;
    int32 _AttachedLegCount = 0;
    int32 _EnabledLegCount = 0;
    int32 _PlantedCount = 0;

public:
    CK_PROPERTY(_EntityName);
    CK_PROPERTY(_EntityId);
    CK_PROPERTY(_HasGait);
    CK_PROPERTY(_HasArticulation);
    CK_PROPERTY(_HasPlayer);
    CK_PROPERTY(_LimbCount);
    CK_PROPERTY(_ArticulationStatus);
    CK_PROPERTY(_GaitStatus);
    CK_PROPERTY(_HasRig);
    CK_PROPERTY(_RigStatus);
    CK_PROPERTY(_LegCount);
    CK_PROPERTY(_AttachedLegCount);
    CK_PROPERTY(_EnabledLegCount);
    CK_PROPERTY(_PlantedCount);

public:
    auto operator==(const FCkProceduralAnimationDebugger_Summary& InOther) const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

struct CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Row
{
    FCk_Handle Entity;
    FString Label;
    FCkProceduralAnimationDebugger_Summary Summary;
};

// --------------------------------------------------------------------------------------------------------------------

class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_DataCollector
{
public:
    static auto Collect_LimbControls(const FCk_Handle& InSubject)
        -> TArray<FCkProceduralAnimationDebugger_LimbControl>;

    static auto
        Collect(
            UWorld* InWorld)
        -> TArray<FCkProceduralAnimationDebugger_Row>;

    static auto
        Is_Supported(
            const FCk_Handle& InEntity)
        -> bool;
};

// --------------------------------------------------------------------------------------------------------------------
