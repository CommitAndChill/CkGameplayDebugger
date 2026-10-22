#include "CkDebugger_ProfileTestTypes.h"

#include "CkGameplayDebugger/Bridge/CkDebugger_Bridge.h"

auto
    UCk_DebuggerProfile_TestAction::
    DoActivateAction_Implementation()
    -> void
{
    ++_ActivationCount;
}

auto
    UCk_DebuggerProfile_TestAction::
    DoDeactivateAction_Implementation()
    -> void
{
    ++_DeactivationCount;
}

auto
    UCk_DebuggerProfile_TestSubmenu::
    DoActivateSubmenu_Implementation()
    -> void
{
    ++_ActivationCount;
    if (_BridgeForReentry.IsValid())
    { _BridgeForReentry->LoadNewDebugProfile(_ProfileForReentry); }
}

auto
    UCk_DebuggerProfile_TestSubmenu::
    DoDeactivateSubmenu_Implementation()
    -> void
{
    ++_DeactivationCount;
    if (_BridgeForReentry.IsValid())
    { _BridgeForReentry->UnloadCurrentDebugProfile(); }
}

auto
    UCk_DebuggerProfile_TestSubmenu::
    ConfigureReentry(
        ACk_GameplayDebugger_DebugBridge_UE* InBridge,
        UCk_GameplayDebugger_DebugProfile_PDA* InProfile)
    -> void
{
    _BridgeForReentry = InBridge;
    _ProfileForReentry = InProfile;
}
