#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CkDebugOverlay_SourceAdmission.generated.h"

class UWorld;

/** World-local visibility gate for one externally proven source bank. */
UCLASS()
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_SourceAdmission_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Caller creates only bare direct roots and calls this in the same non-yielding action. */
    UFUNCTION(BlueprintCallable, Category="CK|DebugOverlay|SourceAdmission")
    static bool
    TryBegin(UWorld* World, const FCk_Handle& WorldRoot, int64 Generation,
        const TArray<FCk_Handle>& DirectRoots, FString& OutReason);

    /** Atomically replaces admitted membership at this generation; caller proves completeness. */
    UFUNCTION(BlueprintCallable, Category="CK|DebugOverlay|SourceAdmission")
    static bool
    TryPublish(UWorld* World, const FCk_Handle& WorldRoot, int64 Generation,
        const TArray<FCk_Handle>& Members, FString& OutReason);

    /** Closes exposure, retaining every previously known identity until physical retirement. */
    UFUNCTION(BlueprintCallable, Category="CK|DebugOverlay|SourceAdmission")
    static bool
    TryClose(UWorld* World, const FCk_Handle& WorldRoot, int64 Generation,
        FString& OutReason);

    /** Generic discovery admits unrelated live entities; scoped entities need exact published membership. */
    UFUNCTION(BlueprintPure, Category="CK|DebugOverlay|SourceAdmission")
    static bool
    IsAdmitted(const FCk_Handle& Entity);
};
