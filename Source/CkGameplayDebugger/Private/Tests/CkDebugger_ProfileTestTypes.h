#pragma once

#include "CkGameplayDebugger/Action/CkDebugger_Action.h"
#include "CkGameplayDebugger/Filter/CkDebugger_Filter.h"
#include "CkGameplayDebugger/Submenu/CkDebugger_Submenu.h"

#include "CkDebugger_ProfileTestTypes.generated.h"

UCLASS()
class UCk_DebuggerProfile_TestAction : public UCk_GameplayDebugger_DebugAction_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_DebuggerProfile_TestAction);

protected:
    auto DoActivateAction_Implementation() -> void override;
    auto DoDeactivateAction_Implementation() -> void override;

private:
    int32 _ActivationCount = 0;
    int32 _DeactivationCount = 0;

public:
    CK_PROPERTY_GET(_ActivationCount);
    CK_PROPERTY_GET(_DeactivationCount);
};

UCLASS()
class UCk_DebuggerProfile_TestSubmenu : public UCk_GameplayDebugger_DebugSubmenu_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_DebuggerProfile_TestSubmenu);

    auto ConfigureReentry(class ACk_GameplayDebugger_DebugBridge_UE* InBridge,
        class UCk_GameplayDebugger_DebugProfile_PDA* InProfile) -> void;

protected:
    auto DoActivateSubmenu_Implementation() -> void override;
    auto DoDeactivateSubmenu_Implementation() -> void override;

private:
    UPROPERTY()
    TWeakObjectPtr<class ACk_GameplayDebugger_DebugBridge_UE> _BridgeForReentry;

    UPROPERTY()
    TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _ProfileForReentry;

    int32 _ActivationCount = 0;
    int32 _DeactivationCount = 0;

public:
    CK_PROPERTY_GET(_ActivationCount);
    CK_PROPERTY_GET(_DeactivationCount);
};

UCLASS()
class UCk_DebuggerProfile_TestFilter : public UCk_GameplayDebugger_DebugFilter_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_DebuggerProfile_TestFilter);
};
