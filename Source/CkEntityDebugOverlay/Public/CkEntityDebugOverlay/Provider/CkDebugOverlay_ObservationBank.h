#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkTimer/CkTimer_Fragment_Data.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"
#include "GameplayTagContainer.h"

class UWorld;

/** One value-only Timer fact in the order used by the canonical Timer provider. */
struct FCk_DebugOverlay_TimerFact
{
    FGameplayTag Name;
    ECk_Timer_State State = ECk_Timer_State::Paused;
};

struct FCk_DebugOverlay_IntegerFact
{
    FGameplayTag Label;
    int32 Base = 0;
    int32 Final = 0;
};

struct FCk_DebugOverlay_SmStateFact
{
    int32 Depth = 0;
    FString StateName;
    ECk_SmRunStatus Status = ECk_SmRunStatus::Stopped;
};

struct FCk_DebugOverlay_SmHistoryFact
{
    FString FromStateName;
    FString ToStateName;
};

struct FCk_DebugOverlay_SourceObservation
{
    FCk_Handle Source;
    TArray<FCk_DebugOverlay_TimerFact> Timers;
    TArray<FCk_DebugOverlay_IntegerFact> Integers;
    bool HasSm = false;
    TArray<FCk_DebugOverlay_SmStateFact> SmStates;
    bool HasDebugHistory = false;
    TArray<FCk_DebugOverlay_SmHistoryFact> SmHistory;
};

/**
 * A complete, immutable provider observation bank for one captured local scenario in one world.
 * The exact WorldTransient root owns the sole published fragment. The caller, rather than this
 * provider seam, proves that Sources is the complete mirror source census and that all other
 * provider banks and physical presentation are ready before publishing.
 */
struct CKENTITYDEBUGOVERLAY_API FCk_DebugOverlay_ObservationBank
{
    CK_GENERATED_BODY(FCk_DebugOverlay_ObservationBank);

    FCk_Handle ScenarioRoot;
    uint64 Generation = 0;
    uint64 Sequence = 0;
    TMap<FCk_Handle, FCk_DebugOverlay_SourceObservation> Sources;
};

class CKENTITYDEBUGOVERLAY_API FCk_DebugOverlay_ObservationBankUtils
{
public:
    /** Validates the entire proposed bank before one replacement on the exact WorldTransient root. */
    static auto TryPublish(
        UWorld* World,
        const FCk_Handle& WorldRoot,
        const FCk_Handle& ScenarioRoot,
        uint64 Generation,
        uint64 Sequence,
        const TArray<FCk_DebugOverlay_SourceObservation>& Sources) -> bool;

    /** Failure-representable local lookup; never walks source lifetime ancestry. */
    static auto TryGetPublishedSource(
        const FCk_Handle& LocalSource,
        FCk_DebugOverlay_SourceObservation& OutObservation) -> bool;
};
