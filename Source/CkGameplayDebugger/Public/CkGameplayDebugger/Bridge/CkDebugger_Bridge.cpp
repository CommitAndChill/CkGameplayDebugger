#include "CkDebugger_Bridge.h"

#include "Engine/GameViewportClient.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GlobalRenderResources.h"
#include "GameplayDebuggerConfig.h"
#endif

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Math/Arithmetic/CkArithmetic_Utils.h"
#include "CkCore/Object/CkObject_Utils.h"

#include "CkGameplayDebugger/Action/CkDebugger_Action.h"
#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerCategoryReplicator.h"
#include "CkGameplayDebugger/Filter/CkDebugger_Filter.h"
#include "CkGameplayDebugger/Profile/CkDebugger_Profile.h"
#include "CkGameplayDebugger/Submenu/CkDebugger_Submenu.h"
#include "CkGameplayDebugger/Widget/CkDebugger_Widget.h"

#include "CkInput/CkInput_Utils.h"

#include <GameFramework/PlayerController.h>
#include <Templates/UnrealTemplate.h>

// --------------------------------------------------------------------------------------------------------------------

ACk_GameplayDebugger_DebugBridge_UE::
    ACk_GameplayDebugger_DebugBridge_UE()
{
    PrimaryActorTick.bCanEverTick = true;

#if WITH_GAMEPLAY_DEBUGGER
    const auto& GameplayDebugger = FCk_GameplayDebugger_Category::Get_Instance();

    if (ck::Is_NOT_Valid(GameplayDebugger))
    { return; }

    GameplayDebugger->_OnActivatedDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_Activated);
    GameplayDebugger->_OnDeactivatedDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_Deactivated);
    GameplayDebugger->_OnCollectDataDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_CollectData);
    GameplayDebugger->_OnDrawDataDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_DrawData);
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    EndPlay(
        const EEndPlayReason::Type InEndPlayReason)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    CK_ENSURE_IF_NOT(NOT _IsProfileTransitionActive,
        TEXT("GameplayDebugger profile lifecycle callbacks must not destroy their owning bridge."))
    {
        Super::EndPlay(InEndPlayReason);
        return;
    }
#if !CK_DISABLE_ENSURE_CHECKS
    const auto TransitionScope = TGuardValue<bool>{_IsProfileTransitionActive, true};
#endif
    _IsCategoryActive = false;
    DoUnloadLoadedProfile();
#endif
    Super::EndPlay(InEndPlayReason);
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    Tick(float InDeltaSeconds)
    -> void
{
    Super::Tick(InDeltaSeconds);
    DoUpdateDebugWidgetDiagnosticVisibility();
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoUpdateDebugWidgetDiagnosticVisibility()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(_DebugWidget))
    {
        _DebugWidgetVisibilityBeforeDiagnosticSuppression.Reset();
        return;
    }

    if (ck::diagnostic_visibility::Is_HiddenForStreamerMode())
    {
        if (NOT _DebugWidgetVisibilityBeforeDiagnosticSuppression.IsSet())
        {
            _DebugWidgetVisibilityBeforeDiagnosticSuppression = _DebugWidget->GetVisibility();
        }

        _DebugWidget->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (_DebugWidgetVisibilityBeforeDiagnosticSuppression.IsSet())
    {
        _DebugWidget->SetVisibility(_DebugWidgetVisibilityBeforeDiagnosticSuppression.GetValue());
        _DebugWidgetVisibilityBeforeDiagnosticSuppression.Reset();
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    LoadNewDebugProfile(
        UCk_GameplayDebugger_DebugProfile_PDA* InDebugProfile)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    CK_ENSURE_IF_NOT(NOT _IsProfileTransitionActive,
        TEXT("GameplayDebugger profile lifecycle callbacks must not request nested profile transitions."))
    { return; }
#if !CK_DISABLE_ENSURE_CHECKS
    const auto TransitionScope = TGuardValue<bool>{_IsProfileTransitionActive, true};
#endif

    CK_ENSURE_IF_NOT(ck::IsValid(InDebugProfile), TEXT("Invalid GameplayDebugger Debug Profile to load!"))
    { return; }

    CK_ENSURE_IF_NOT(InDebugProfile->Get_HasValidRequiredEntries(),
        TEXT("GameplayDebugger Debug Profile contains an invalid required entry."))
    { return; }

    auto ReplacementProfile = UCk_Utils_Object_UE::Request_CloneObject(this, InDebugProfile);
    CK_ENSURE_IF_NOT(ck::IsValid(ReplacementProfile), TEXT("Unable to clone GameplayDebugger Debug Profile."))
    { return; }

    DoUnloadLoadedProfile();
    _CurrentlyLoadedDebugProfile = ReplacementProfile;
    DoActivateLoadedProfile();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    UnloadCurrentDebugProfile()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    CK_ENSURE_IF_NOT(NOT _IsProfileTransitionActive,
        TEXT("GameplayDebugger profile lifecycle callbacks must not request nested profile transitions."))
    { return; }
#if !CK_DISABLE_ENSURE_CHECKS
    const auto TransitionScope = TGuardValue<bool>{_IsProfileTransitionActive, true};
#endif

    DoUnloadLoadedProfile();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoUnloadLoadedProfile()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    DoDeactivateLoadedProfile();
    DoResetProfileSelection();
    _CurrentlyLoadedDebugProfile = nullptr;
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_Activated()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    CK_ENSURE_IF_NOT(NOT _IsProfileTransitionActive,
        TEXT("GameplayDebugger profile lifecycle callbacks must not request nested profile transitions."))
    { return; }
#if !CK_DISABLE_ENSURE_CHECKS
    const auto TransitionScope = TGuardValue<bool>{_IsProfileTransitionActive, true};
#endif

    _IsCategoryActive = true;
    DoActivateLoadedProfile();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoActivateLoadedProfile()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (NOT _IsCategoryActive || _IsProfileActive)
    { return; }

    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    _IsProfileActive = true;

    if (const auto& DebugWidgetToCreate = _CurrentlyLoadedDebugProfile->Get_HUD_DebugWidget();
        ck::IsValid(DebugWidgetToCreate))
    {
        _DebugWidget = UCk_GameplayDebugger_DebugWidget_UE::Create
        (
            GetWorld(),
            DebugWidgetToCreate,
            FCk_GameplayDebugger_DebugWidget_Params{_CurrentlyLoadedDebugProfile->Get_DebugNavControls()}
        );

        if (ck::IsValid(_DebugWidget))
        {
            static constexpr auto ZOrder = 9999;
            _DebugWidget->AddToViewport(ZOrder);
            DoUpdateDebugWidgetDiagnosticVisibility();
        }
    }

    // Activate all debug submenus
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [](const TObjectPtr<UCk_GameplayDebugger_DebugSubmenu_UE>& InSubmenu) -> void
    {
        InSubmenu->ActivateSubmenu();
    });

    // Activate all global debug actions
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_GlobalActions(),
    [](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InAction) -> void
    {
        InAction->ActivateAction();
    });

    // Activate the currently selected debug filter
    DoActivateCurrentlySelectedFilter();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_Deactivated()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    CK_ENSURE_IF_NOT(NOT _IsProfileTransitionActive,
        TEXT("GameplayDebugger profile lifecycle callbacks must not request nested profile transitions."))
    { return; }
#if !CK_DISABLE_ENSURE_CHECKS
    const auto TransitionScope = TGuardValue<bool>{_IsProfileTransitionActive, true};
#endif

    _IsCategoryActive = false;
    DoDeactivateLoadedProfile();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoDeactivateLoadedProfile()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (NOT _IsProfileActive)
    { return; }

    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    _IsProfileActive = false;

    if (ck::IsValid(_DebugWidget))
    {
        _DebugWidget->RemoveFromParent();
        _DebugWidget = nullptr;
    }
    _DebugWidgetVisibilityBeforeDiagnosticSuppression.Reset();

    // Deactivate all debug submenus
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [](const TObjectPtr<UCk_GameplayDebugger_DebugSubmenu_UE>& InSubmenu) -> void
    {
        InSubmenu->DeactivateSubmenu();
    });

    // Deactivate the currently selected debug filter
    DoDeactivateCurrentlySelectedFilter();

    // Deactivate all global debug actions
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_GlobalActions(),
    [](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InGlobalAction) -> void
    {
        InGlobalAction->DeactivateAction();
    });
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoResetProfileSelection()
    -> void
{
    _CurrentlySelectedActorIndex = 0;
    _CurrentlySelectedFilterIndex = 0;
    _CurrentWorldToUseIndex = 0;
    _PreviouslySelectedActor.Reset();
    _SelectedDebugActor.Reset();
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_CollectData(
        const FCk_Payload_GameplayDebugger_OnCollectData& InPayload)
    -> void
{
    // Empty on purpose.
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_DrawData(
        const FCk_Payload_GameplayDebugger_OnDrawData& InPayload)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::diagnostic_visibility::Is_HiddenForStreamerMode())
    { return; }

    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    const auto& CanvasContext = InPayload.Get_CanvasContext();
    if (ck::Is_NOT_Valid(CanvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    const auto& OwnerPC = InPayload.Get_OwnerPC();
    if (ck::Is_NOT_Valid(OwnerPC))
    { return; }

    const auto& DisplaySettings = _CurrentlyLoadedDebugProfile->Get_DisplaySettings();

    DoApplyDisplaySettings(CanvasContext, DisplaySettings);

    FString MainMenu;

    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [&](const UCk_GameplayDebugger_DebugSubmenu_UE* InSubmenu) -> void
    {
        if (const auto& KeyToShowMenu = InSubmenu->Get_KeyToShowMenu(); ck::Is_NOT_Valid(KeyToShowMenu))
        { return; }

        const auto& IsSubmenuVisible = InSubmenu->Get_ShowState() == ECk_GameplayDebugger_DebugSubmenu_ShowState::Visible;

        MainMenu += ck::Format_UE
        (
            TEXT("{{white}}{} {}{}] "),
            InSubmenu->Get_MenuName(),
            IsSubmenuVisible ? TEXT("{{green}}[") : TEXT("{{red}}["),
            InSubmenu->Get_KeyToShowMenu()
        );
    });

    CanvasContext->Print(MainMenu);

    DoHandleDebugActorSelectionCycling(InPayload);

    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [&](UCk_GameplayDebugger_DebugSubmenu_UE* InSubmenu) -> void
    {
        // NOTE: We handle input in "DrawData" since "CollectData" does not run while the game is paused
        const auto& KeyToShowMenu = InSubmenu->Get_KeyToShowMenu();
        if (ck::Is_NOT_Valid(KeyToShowMenu))
        { return; }

        const auto& StickyModifierKey = _CurrentlyLoadedDebugProfile->Get_DebugNavControls().Get_StickyModiferKey();
        if (UCk_Utils_Input_UE::WasInputKeyJustPressed_WithCustomModifier(OwnerPC.Get(), KeyToShowMenu, StickyModifierKey))
        {
            InSubmenu->ToggleShowState();
        }

        if (InSubmenu->Get_ShowState() == ECk_GameplayDebugger_DebugSubmenu_ShowState::Hidden)
        { return; }

        if (ck::IsValid(_PreviouslySelectedActor))
        {
            InSubmenu->DrawData
            (
                FCk_GameplayDebugger_DrawSubmenuData_Params
                {
                    _PreviouslySelectedActor.Get(),
                    InPayload
                }
            );
        }
    });
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoChangeFilter(
        int32 InPreviousFilterIndex,
        int32 InNewFilterIndex)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    CK_ENSURE_IF_NOT(NOT _IsProfileTransitionActive,
        TEXT("GameplayDebugger profile lifecycle callbacks must not request nested profile transitions."))
    { return; }
#if !CK_DISABLE_ENSURE_CHECKS
    const auto TransitionScope = TGuardValue<bool>{_IsProfileTransitionActive, true};
#endif

    CK_ENSURE_IF_NOT(ck::IsValid(_CurrentlyLoadedDebugProfile), TEXT("Cannot change filter without a debug profile."))
    { return; }

    const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();
    if (DebugFilters.IsEmpty())
    { return; }

    CK_ENSURE_IF_NOT(DebugFilters.IsValidIndex(InPreviousFilterIndex), TEXT("Invalid previous debug filter index."))
    { return; }

    const auto NewFilterIndex = ((InNewFilterIndex % DebugFilters.Num()) + DebugFilters.Num()) % DebugFilters.Num();
    const auto& PreviousFilter = DebugFilters[InPreviousFilterIndex];
    const auto& NewFilter = DebugFilters[NewFilterIndex];
    CK_ENSURE_IF_NOT(ck::IsValid(PreviousFilter), TEXT("Invalid previous debug filter."))
    { return; }
    CK_ENSURE_IF_NOT(ck::IsValid(NewFilter), TEXT("Invalid replacement debug filter."))
    { return; }

    if (InPreviousFilterIndex == NewFilterIndex)
    { return; }

    if (_IsProfileActive)
    { PreviousFilter->DeactivateFilter(); }

    _CurrentlySelectedFilterIndex = NewFilterIndex;
    _CurrentlySelectedActorIndex = 0;
    _PreviouslySelectedActor.Reset();
    _SelectedDebugActor.Reset();

    if (_IsProfileActive)
    { NewFilter->ActivateFilter(); }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleDebugActorSelectionCycling(
        const FCk_Payload_GameplayDebugger_OnDrawData& InDrawData)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& OwnerPC = InDrawData.Get_OwnerPC().Get();
    if (ck::Is_NOT_Valid(OwnerPC))
    { return; }

    const auto& CanvasContext = InDrawData.Get_CanvasContext();
    if (ck::Is_NOT_Valid(CanvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    const auto& DoPrintMessageToCanvas = [&](const FString& InMessage)
    {
        CanvasContext->Print(ck::Format_UE(TEXT("{}"), InMessage));
    };

    const auto& UpdateSelectedDebugActor = [&](AActor* InSelectedDebugActor)
    {
        const auto& Replicator = InDrawData.Get_Replicator().Get();
        if (ck::Is_NOT_Valid(Replicator))
        { return; }

        _PreviouslySelectedActor = InSelectedDebugActor;
    };

    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    if (DebugFilters.IsEmpty())
    {
        _CurrentlySelectedFilterIndex = 0;
        return;
    }

    const auto& DebugNavControls = _CurrentlyLoadedDebugProfile->Get_DebugNavControls();

    DoHandleFilterChanges(OwnerPC, DebugNavControls);
    DoHandleWorldChange(OwnerPC, DebugNavControls, InDrawData.Get_AvailableWorlds());

    CK_ENSURE_IF_NOT(DebugFilters.IsValidIndex(_CurrentlySelectedFilterIndex), TEXT("Invalid selected debug filter index."))
    { return; }

    const auto& CurrentlySelectedFilter = DebugFilters[_CurrentlySelectedFilterIndex];

    DoHandleFilterActionActivationToggling(OwnerPC, DebugNavControls, CurrentlySelectedFilter);

    if (ck::Is_NOT_Valid(CurrentlySelectedFilter))
    { return; }

    const auto& AvailableWorlds = InDrawData.Get_AvailableWorlds();
    if (AvailableWorlds.IsEmpty())
    { return; }

    const auto CurrentWorldToUse = AvailableWorlds[_CurrentWorldToUseIndex];
    if (ck::Is_NOT_Valid(CurrentWorldToUse))
    { return; }
    {
        // followed GameplayDebuggerLocalController:307
        const auto NetModeText = [&]() -> FString
        {
            switch (CurrentWorldToUse->GetNetMode())
            {
                case NM_Client:
                {
                    return FString{TEXT("== CLIENT == ")};
                }
                case NM_DedicatedServer:
                {
                    return FString{TEXT("== DEDICATED SERVER == ")};
                }
                case NM_ListenServer:
                {
                    return FString{TEXT("== LISTEN SERVER == ")};
                }
                case NM_Standalone:
                {
                    return FString{TEXT("== STANDALONE == ")};
                }
                default:
                {
                    return FString{TEXT("== NO NETMODE ==")};
                }
            }
        }();

        const auto SimulateMode = FGameplayDebuggerAddonBase::IsSimulateInEditor() || InDrawData.Get_Replicator()->IsEditorWorldReplicator();
        DoDrawTextOnCanvas(InDrawData.Get_CanvasContext(), NetModeText, (SimulateMode ? 30.0f : 0) + 30.0f, FColor::Cyan);
    }

    const auto& SortedFilteredActorList = CurrentlySelectedFilter->Get_SortedFilteredActors
    (
        FCk_GameplayDebugger_GetSortedFilteredActors_Params
        {
            FCk_Payload_GameplayDebugger_OnDrawData
            {
                InDrawData.Get_OwnerPC(),
                InDrawData.Get_CanvasContext(),
#if WITH_GAMEPLAY_DEBUGGER
                InDrawData.Get_Replicator(),
#endif
                InDrawData.Get_AvailableWorlds(),
                InDrawData.Get_AvailableWorlds()[_CurrentWorldToUseIndex]
            }
        }
        .Set_PreviouslySelectedActor(_PreviouslySelectedActor)
    );

    const auto& SortedFilteredActors = SortedFilteredActorList.Get_DebugActors();

    const auto& CanvasMessage = DoGet_FormattedCanvasMessage(CurrentlySelectedFilter, SortedFilteredActorList);

    if (SortedFilteredActors.IsEmpty())
    {
        UpdateSelectedDebugActor(nullptr);
        DoPrintMessageToCanvas(CanvasMessage);

        _CurrentlySelectedActorIndex = 0;

        return;
    }

    DoHandleSelectedActorChange(OwnerPC, DebugNavControls, SortedFilteredActors);

    const auto CurrentlySelectedActor = SortedFilteredActors[_CurrentlySelectedActorIndex];
    UpdateSelectedDebugActor(CurrentlySelectedActor);
    DoDrawTextOnCanvas(InDrawData.Get_CanvasContext(),
        UCk_Utils_Debug_UE::Get_DebugName_AsString(CurrentlySelectedActor, ECk_DebugNameVerbosity_Policy::Compact), 15.0f, FColor::Green);

    DoPerformActionsOnFilteredActors(CurrentlySelectedFilter, SortedFilteredActorList, CanvasContext);

    DoPrintMessageToCanvas(CanvasMessage);
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoActivateCurrentlySelectedFilter() const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER

    if (const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();
        DebugFilters.IsValidIndex(_CurrentlySelectedFilterIndex))
    {
        const auto& CurrentlySelectedFilter = DebugFilters[_CurrentlySelectedFilterIndex];

        if (ck::IsValid(CurrentlySelectedFilter))
        {
            CurrentlySelectedFilter->ActivateFilter();
        }
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoDeactivateCurrentlySelectedFilter() const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER

    if (const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();
        DebugFilters.IsValidIndex(_CurrentlySelectedFilterIndex))
    {
        const auto& CurrentlySelectedFilter = DebugFilters[_CurrentlySelectedFilterIndex];

        if (ck::IsValid(CurrentlySelectedFilter))
        {
            CurrentlySelectedFilter->DeactivateFilter();
        }
    }
#endif
}

#if WITH_GAMEPLAY_DEBUGGER
auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoApplyDisplaySettings(
        FGameplayDebuggerCanvasContext* InCanvasContext,
        const FCk_GameplayDebugger_DisplaySettings& InDisplaySettings) const
    -> void
{
    if (ck::Is_NOT_Valid(InCanvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    InCanvasContext->FontRenderInfo.bEnableShadow = InDisplaySettings.Get_EnableTextDropShadow();
    InCanvasContext->Font = UCk_Utils_IO_UE::Get_Engine_DefaultTextFont(InDisplaySettings.Get_FontSize());

    if (InDisplaySettings.Get_DisplayTranslucentBackground())
    {
        auto ViewportSize = FVector2D{};
        GEngine->GameViewport->GetViewportSize(ViewportSize);

        constexpr auto BackgroundPadding = 5.0f;

        const auto& BackgroundWidth = [&]()
        {
            switch (const auto& SelectedWidth = InDisplaySettings.Get_BackgroundWidth())
            {
                case ECk_GameplayDebugger_BackgroundWidth::OneThird:
                {
                    return ViewportSize.X / 3.0f;
                }
                case ECk_GameplayDebugger_BackgroundWidth::Half:
                {
                    return ViewportSize.X / 2.0f;
                }
                case ECk_GameplayDebugger_BackgroundWidth::ThreeFourth:
                {
                    return ViewportSize.X * 0.75f;
                }
                case ECk_GameplayDebugger_BackgroundWidth::Full:
                {
                    return ViewportSize.X;
                }
                default:
                {
                    CK_INVALID_ENUM(SelectedWidth);
                    return ViewportSize.X;
                }
            }
        }();

        const auto BackgroundSize = FVector2D(BackgroundWidth - (2.0f * BackgroundPadding), ViewportSize.Y);
        const auto BackgroundColor = InDisplaySettings.Get_BackgroundColor();

        FCanvasTileItem Background(FVector2D::ZeroVector, GWhiteTexture, BackgroundSize, BackgroundColor);
        Background.BlendMode = SE_BLEND_Translucent;

        InCanvasContext->DrawItem(Background, InCanvasContext->DefaultX - BackgroundPadding, InCanvasContext->DefaultY - BackgroundPadding);
    }
}
#endif

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoGet_FormattedCanvasMessage(
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter,
        const FCk_GameplayDebugger_DebugActorList& InFilteredActors) const
    -> FString
{
    if (ck::Is_NOT_Valid(InSelectedFilter))
    { return {}; }

    FString Message;

#if WITH_GAMEPLAY_DEBUGGER
    // Construct Filter message
    {
        Message += ck::Format_UE(TEXT("{{white}}CURRENT FILTER: {{cyan}}{}"), InSelectedFilter->Get_FilterName());

        const auto& FilteredDebugActors = InFilteredActors.Get_DebugActors();

        if (FilteredDebugActors.IsEmpty())
        {
            Message += ck::Format_UE(TEXT(" {{red}}[NO ACTORS RETURNED]"));
        }
        else
        {
            Message += ck::Format_UE
            (
                TEXT(" {{white}}[{{yellow}}{}{{white}}/{}]"),
                _CurrentlySelectedActorIndex + 1,
                FilteredDebugActors.Num()
            );
        }
    }

    const auto& DoGet_ActionMessage = [](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InAction) -> FString
    {
        const auto& IsActionActive = InAction->Get_IsActive();

        const auto IsActionActiveStr = ck::Format_UE
        (
            TEXT("{}{}]"),
            (IsActionActive ? TEXT(" {{green}}[") : TEXT(" {{red}}[")),
            InAction->Get_ToggleActivateKey()
        );

        return ck::Format_UE
        (
            TEXT("\n\t{{white}}{} {}"),
            InAction->Get_ActionName(),
            IsActionActiveStr
        );
    };

    // Construct Filter Actions message
    {
        Message += ck::Format_UE(TEXT("\n{{white}}FILTER ACTIONS:"));
        const auto& FilterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        if (FilterDebugActions.IsEmpty())
        {
            Message += ck::Format_UE(TEXT(" {{red}}[NO ACTIONS]"));
        }
        else
        {
            ck::algo::ForEachIsValid(FilterDebugActions,
            [&](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InAction) -> void
            {
                Message += DoGet_ActionMessage(InAction);
            });
        }
    }

    // Construct Global Actions Message
    {
        Message += ck::Format_UE(TEXT("\n{{white}}GLOBAL ACTIONS:"));

        if (ck::IsValid(_CurrentlyLoadedDebugProfile))
        {
            const auto& GlobalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

            if (GlobalDebugActions.IsEmpty())
            {
                Message += ck::Format_UE(TEXT(" {{red}}[NO ACTIONS]"));
            }
            else
            {
                ck::algo::ForEachIsValid(GlobalDebugActions,
                [&](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InAction) -> void
            {
                Message += DoGet_ActionMessage(InAction);
            });
            }
        }
    }
#endif

    return Message;
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoPerformActionsOnFilteredActors(
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter,
        const FCk_GameplayDebugger_DebugActorList& InFilteredActors,
        FGameplayDebuggerCanvasContext* InCanvasContext) const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& PerformDebugActionParams = FCk_GameplayDebugger_PerformDebugAction_Params
    {
        InFilteredActors,
        _CurrentlySelectedActorIndex,
        InCanvasContext
    };

    if (ck::IsValid(InSelectedFilter))
    {
        const auto& FilterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        ck::algo::ForEachIsValid(FilterDebugActions,
        [&](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InAction) -> void
        {
            InAction->PerformDebugAction(PerformDebugActionParams);
        });
    }

    if (ck::IsValid(_CurrentlyLoadedDebugProfile))
    {
        const auto& GlobalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

        ck::algo::ForEachIsValid(GlobalDebugActions,
        [&](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InAction) -> void
        {
            InAction->PerformDebugAction(PerformDebugActionParams);
        });
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleFilterChanges(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_NextFilterKey()))
    {
        DoChangeFilter(_CurrentlySelectedFilterIndex, _CurrentlySelectedFilterIndex + 1);
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_PreviousFilterKey()))
    {
        DoChangeFilter(_CurrentlySelectedFilterIndex, _CurrentlySelectedFilterIndex - 1);
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleWorldChange(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls,
        const TArray<TWeakObjectPtr<UWorld>>& InAvailableWorlds)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    if (InAvailableWorlds.IsEmpty())
    { return; }

    const auto PreviousWorldIndex = _CurrentWorldToUseIndex;
    if (NOT InAvailableWorlds.IsValidIndex(_CurrentWorldToUseIndex))
    { _CurrentWorldToUseIndex = 0; }

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_NextWorldKey()))
    {
        _CurrentWorldToUseIndex = UCk_Utils_Arithmetic_UE::Get_Increment_WithWrap(_CurrentWorldToUseIndex,
            FCk_IntRange{0, InAvailableWorlds.Num()}, ECk_Inclusiveness::Exclusive);
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_PrevWorldKey()))
    {
        _CurrentWorldToUseIndex = UCk_Utils_Arithmetic_UE::Get_Decrement_WithWrap(_CurrentWorldToUseIndex,
            FCk_IntRange{0, InAvailableWorlds.Num()}, ECk_Inclusiveness::Exclusive);
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_ServerClientWorldToggleKey()))
    {
        const auto SelectedWorldIndex = InAvailableWorlds.IndexOfByPredicate([&](const TWeakObjectPtr<UWorld>& InWorld)
        {
            if (ck::Is_NOT_Valid(InWorld) || ck::Is_NOT_Valid(InAvailableWorlds[_CurrentWorldToUseIndex]))
            { return false; }

            const auto CurrentWorldNetMode = InAvailableWorlds[_CurrentWorldToUseIndex]->GetNetMode();

            if (CurrentWorldNetMode == NM_Client)
            {
                if (InWorld->IsNetMode(NM_DedicatedServer) || InWorld->IsNetMode(NM_ListenServer) ||
                    InWorld->IsNetMode(NM_Standalone))
                { return true; }
            }
            else
            {
                if (InWorld->IsNetMode(NM_Client))
                { return true; }
            }

            return false;
        });

        if (SelectedWorldIndex != INDEX_NONE)
        { _CurrentWorldToUseIndex = SelectedWorldIndex; }
    }
    if (PreviousWorldIndex != _CurrentWorldToUseIndex)
    {
        _CurrentlySelectedActorIndex = 0;
        _PreviouslySelectedActor.Reset();
        _SelectedDebugActor.Reset();
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleFilterActionActivationToggling(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls,
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter) const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    const auto& DoTryToggleAllValidActions = [&](TArray<TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>> InActions) -> void
    {
        ck::algo::ForEachIsValid(InActions,
        [&](const TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>& InValidAction) -> void
        {
            if (NOT UCk_Utils_Input_UE::WasInputKeyJustPressed_WithCustomModifier(InOwnerPC,
                InValidAction->Get_ToggleActivateKey(), InDebugNavControls.Get_StickyModiferKey()))
            { return; }

            InValidAction->ToggleAction();
        });
    };

    if (ck::IsValid(InSelectedFilter))
    {
        const auto& FilterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        DoTryToggleAllValidActions(FilterDebugActions);
    }

    if (ck::IsValid(_CurrentlyLoadedDebugProfile))
    {
        const auto& GlobalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

        DoTryToggleAllValidActions(GlobalDebugActions);
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleSelectedActorChange(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls,
        const TArray<TObjectPtr<AActor>>& InSortedFilteredActors)
    -> void
{
    const auto PreviouslySelectedActorIndex = _CurrentlySelectedActorIndex;

#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_NextActorKey()))
    {
        ++_CurrentlySelectedActorIndex;
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_PreviousActorKey()))
    {
        --_CurrentlySelectedActorIndex;
    }

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_FirstActorKey()))
    {
        _CurrentlySelectedActorIndex = 0;
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_LastActorKey()))
    {
        _CurrentlySelectedActorIndex = InSortedFilteredActors.Num() - 1;
    }

    // Enforce Selected Actor stability if user did not request any actor selection change
    if (PreviouslySelectedActorIndex == _CurrentlySelectedActorIndex && ck::IsValid(_PreviouslySelectedActor))
    {
        const auto& IndexOfPreviouslySelectedActor = InSortedFilteredActors.IndexOfByPredicate(
        [&](const TObjectPtr<AActor>& InSortedFilteredActor)
        {
            return InSortedFilteredActor == _PreviouslySelectedActor.Get();
        });

        if (IndexOfPreviouslySelectedActor != INDEX_NONE)
        {
            _CurrentlySelectedActorIndex = IndexOfPreviouslySelectedActor;
        }
    }

    _CurrentlySelectedActorIndex = _CurrentlySelectedActorIndex % InSortedFilteredActors.Num();

    if (_CurrentlySelectedActorIndex < 0)
    {
        _CurrentlySelectedActorIndex += InSortedFilteredActors.Num();
    }
#endif
}

#if WITH_GAMEPLAY_DEBUGGER
auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoDrawTextOnCanvas(
        FGameplayDebuggerCanvasContext* InCanvasContext,
        const FString& InText,
        float InYOffset,
        const FColor& InColor)
    -> void
{
    auto StringSizeX = 0.0f;
    auto StringSizeY = 0.0f;
    InCanvasContext->MeasureString(InText, StringSizeX, StringSizeY);

    const auto SettingsCDO = UGameplayDebuggerConfig::StaticClass()->GetDefaultObject<UGameplayDebuggerConfig>();
    const auto PaddingLeft = SettingsCDO->DebugCanvasPaddingLeft;
    const auto PaddingRight = SettingsCDO->DebugCanvasPaddingRight;
    const auto PaddingTop = SettingsCDO->DebugCanvasPaddingTop;

    const auto DPIScale = InCanvasContext->Canvas->GetDPIScale();
    const auto CanvasSizeX = (InCanvasContext->Canvas->SizeX / DPIScale) - PaddingLeft - PaddingRight;
    const auto UsePaddingTop = PaddingTop + InYOffset;

    InCanvasContext->PrintAt((CanvasSizeX - StringSizeX) * 0.5f, UsePaddingTop, InColor, InText);
}
#endif

// --------------------------------------------------------------------------------------------------------------------
