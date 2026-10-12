#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"
#include "CkEntityDebugOverlay/Subsystem/CkDebugOverlay_Subsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && WITH_CK_DEBUG_OVERLAY

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealEdGlobals.h"

namespace ck_debug_overlay_subsystem_lifecycle_spec
{
    constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    constexpr auto ReadyTimeoutSeconds = 30.0;

    struct FPlaySettingsSnapshot
    {
        int32 NumClients = 1;
        EPlayNetMode NetMode = EPlayNetMode::PIE_Standalone;
        bool RunUnderOneProcess = true;
        bool LaunchSeparateServer = false;
    };

    struct FCVarSnapshot
    {
        FString Value;
        EConsoleVariableFlags Flags = ECVF_Default;
    };

    struct FState
    {
        FPlaySettingsSnapshot PlaySettings;
        FCVarSnapshot MasterCVar;
        TStrongObjectPtr<UCk_DebugOverlay_Subsystem> Subsystem;
        TSharedPtr<SCkDebugOverlay_Root> HeldRoot;
        FCk_DebugOverlay_Subsystem_TestState Activated;
        FCk_DebugOverlay_Subsystem_TestState Deactivated;
        FCk_DebugOverlay_Subsystem_TestState Deinitialized;
        bool HasSnapshot = false;
        bool ActivatedThroughViewport = false;
        bool SelectionPanelOpened = false;
        bool HeldRootReleased = false;
        bool DeactivatedCleanly = false;
        bool DeinitializedCleanly = false;
        bool PieEnded = false;
    };

    auto GetPieWorld() -> UWorld*
    {
        if (GEngine == nullptr) { return nullptr; }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
            { return Context.World(); }
        }
        return nullptr;
    }

    auto GetPlayer(UWorld* InWorld) -> ULocalPlayer*
    {
        UGameInstance* Instance = IsValid(InWorld) ? InWorld->GetGameInstance() : nullptr;
        return IsValid(Instance) ? Instance->GetLocalPlayerByIndex(0) : nullptr;
    }

    auto IsEmpty(const FCk_DebugOverlay_Subsystem_TestState& InState) -> bool
    {
        return !InState.HasRootWidget && !InState.HasAttachedViewport && !InState.HasTicker && !InState.HasHistory &&
            !InState.HasInputProcessor && !InState.HasSelectionUi && !InState.HasSelectionPanel && !InState.HasSessionInvalidationCallback &&
            !InState.HasWorldInvalidationCallback && !InState.HasOwnedConsoleObjects;
    }

    class FStartPie final : public IAutomationLatentCommand
    {
    public:
        explicit FStartPie(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            if (GUnrealEd == nullptr) { return true; }
            ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
            if (Settings == nullptr) { return true; }

            Settings->GetPlayNumberOfClients(State->PlaySettings.NumClients);
            Settings->GetPlayNetMode(State->PlaySettings.NetMode);
            Settings->GetRunUnderOneProcess(State->PlaySettings.RunUnderOneProcess);
            State->PlaySettings.LaunchSeparateServer = Settings->bLaunchSeparateServer;
            Settings->SetPlayNumberOfClients(1);
            Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;

            FRequestPlaySessionParams Params;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            GUnrealEd->RequestPlaySession(Params);
            return true;
        }

    private:
        TSharedRef<FState> State;
    };

    class FWaitForPie final : public IAutomationLatentCommand
    {
    public:
        explicit FWaitForPie(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            if (StartTime < 0.0) { StartTime = FPlatformTime::Seconds(); }
            UWorld* World = GetPieWorld();
            ULocalPlayer* Player = GetPlayer(World);
            if (IsValid(World) && World->HasBegunPlay() && IsValid(Player) &&
                IsValid(Player->GetSubsystem<UCk_DebugOverlay_Subsystem>()))
            { return true; }
            return FPlatformTime::Seconds() - StartTime >= ReadyTimeoutSeconds;
        }

    private:
        TSharedRef<FState> State;
        double StartTime = -1.0;
    };

    class FExerciseLifecycle final : public IAutomationLatentCommand
    {
    public:
        explicit FExerciseLifecycle(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            ULocalPlayer* Player = GetPlayer(GetPieWorld());
            UCk_DebugOverlay_Subsystem* Subsystem = IsValid(Player)
                ? Player->GetSubsystem<UCk_DebugOverlay_Subsystem>() : nullptr;
            IConsoleVariable* Master = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay"));
            if (!IsValid(Subsystem) || Master == nullptr || !FSlateApplication::IsInitialized()) { return true; }

            State->Subsystem.Reset(Subsystem);
            State->MasterCVar = {Master->GetString(), static_cast<EConsoleVariableFlags>(Master->GetFlags())};
            State->HasSnapshot = true;
            Master->Set(1, ECVF_SetByConsole);
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();

            State->HeldRoot = Subsystem->Get_TestRootWidget();
            if (GEngine != nullptr) { GEngine->Exec(GetPieWorld(), TEXT("ck.DebugOverlay.Settings")); }
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();
            State->Activated = Subsystem->Get_TestState();
            State->ActivatedThroughViewport = State->HeldRoot.IsValid() && State->Activated.HasRootWidget &&
                State->Activated.HasAttachedViewport && State->Activated.HasTicker && State->Activated.HasHistory &&
                State->Activated.HasInputProcessor && State->Activated.HasSelectionUi &&
                State->Activated.HasSessionInvalidationCallback && State->Activated.HasWorldInvalidationCallback;
            State->SelectionPanelOpened = State->Activated.HasSelectionPanel;

            Master->Set(0, ECVF_SetByConsole);
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();
            State->Deactivated = Subsystem->Get_TestState();
            State->HeldRootReleased = State->HeldRoot.IsValid() && !State->HeldRoot->Get_AuthoredView().IsValid() &&
                !State->HeldRoot->Get_WorldTagPort().IsValid() && !State->HeldRoot->Get_CardPort().IsValid() &&
                State->HeldRoot->Get_AdmittedWorldTagKeys().IsEmpty();
            State->DeactivatedCleanly = !State->Deactivated.HasRootWidget && !State->Deactivated.HasAttachedViewport &&
                !State->Deactivated.HasTicker && !State->Deactivated.HasHistory && !State->Deactivated.HasSelectionUi &&
                !State->Deactivated.HasSelectionPanel &&
                // The global preprocessor and lifecycle delegates deliberately survive cvar-off so input can reactivate.
                State->Deactivated.HasInputProcessor && State->Deactivated.HasSessionInvalidationCallback &&
                State->Deactivated.HasWorldInvalidationCallback;

            return true;
        }

    private:
        TSharedRef<FState> State;
    };

    class FAssertLifecycle final : public IAutomationLatentCommand
    {
    public:
        FAssertLifecycle(FAutomationTestBase* InTest, const TSharedRef<FState>& InState) : Test(InTest), State(InState) {}

        virtual bool Update() override
        {
            if (Test == nullptr) { return true; }
            Test->TestTrue(TEXT("real LocalPlayer subsystem activated and attached both overlay surfaces to its viewport"),
                State->ActivatedThroughViewport);
            Test->TestTrue(TEXT("production settings command attaches a selection panel that cvar-off later owns and removes"),
                State->SelectionPanelOpened);
            Test->TestTrue(TEXT("held root is made inert before the production viewport detaches it"), State->HeldRootReleased);
            Test->TestTrue(TEXT("cvar deactivation clears active ticker, viewport, history, and selection UI while retaining reactivation routing"),
                State->DeactivatedCleanly);
            Test->TestTrue(TEXT("ending PIE drives the engine-owned LocalPlayer subsystem through deinitialization"), State->PieEnded);
            Test->TestTrue(TEXT("natural deinitialization clears input preprocessing, lifecycle callbacks, and owned console objects"),
                State->DeinitializedCleanly);
            return true;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        TSharedRef<FState> State;
    };

    class FEndPie final : public IAutomationLatentCommand
    {
    public:
        explicit FEndPie(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            if (StartTime < 0.0) { StartTime = FPlatformTime::Seconds(); }
            if (!Requested)
            {
                if (GUnrealEd != nullptr) { GUnrealEd->RequestEndPlayMap(); }
                Requested = true;
                return false;
            }

            if (UCk_DebugOverlay_Subsystem* Subsystem = State->Subsystem.Get(); IsValid(Subsystem))
            {
                const auto Current = Subsystem->Get_TestState();
                if (IsEmpty(Current))
                {
                    State->Deinitialized = Current;
                    State->DeinitializedCleanly = true;
                }
            }

            if (GetPieWorld() != nullptr && FPlatformTime::Seconds() - StartTime < ReadyTimeoutSeconds)
            { return false; }

            State->PieEnded = GetPieWorld() == nullptr;

            if (ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>())
            {
                Settings->SetPlayNumberOfClients(State->PlaySettings.NumClients);
                Settings->SetPlayNetMode(State->PlaySettings.NetMode);
                Settings->SetRunUnderOneProcess(State->PlaySettings.RunUnderOneProcess);
                Settings->bLaunchSeparateServer = State->PlaySettings.LaunchSeparateServer;
            }
            if (State->HasSnapshot)
            {
                if (IConsoleVariable* Master = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay")))
                {
                    Master->Set(*State->MasterCVar.Value, ECVF_SetByConsole);
                    Master->SetFlags(State->MasterCVar.Flags);
                }
            }
            return true;
        }

    private:
        TSharedRef<FState> State;
        bool Requested = false;
        double StartTime = -1.0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_SubsystemLifecycle_Test,
    "Ck.DebugOverlay.Subsystem.Lifecycle.Core",
    ck_debug_overlay_subsystem_lifecycle_spec::TestFlags)

bool FCkDebugOverlay_SubsystemLifecycle_Test::RunTest(const FString&)
{
    using namespace ck_debug_overlay_subsystem_lifecycle_spec;

    const TSharedRef<FState> State = MakeShared<FState>();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPie(State));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPie(State));
    ADD_LATENT_AUTOMATION_COMMAND(FExerciseLifecycle(State));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPie(State));
    ADD_LATENT_AUTOMATION_COMMAND(FAssertLifecycle(this, State));
    return true;
}


namespace ck_debug_overlay_net_lifecycle_spec
{
    enum class ETarget : uint8 { ListenHost, ListenClient, DedicatedClient };

    struct FState
    {
        FAutomationTestBase* Test = nullptr;
        ETarget Target = ETarget::ListenHost;
        int32 SavedNumClients = 1;
        EPlayNetMode SavedNetMode = EPlayNetMode::PIE_Standalone;
        bool SavedOneProcess = true;
        bool SavedSeparateServer = false;
        bool HasPlaySettings = false;
        FDelegateHandle RestoreHandle;
        TWeakObjectPtr<UWorld> OwnedWorld;
        TStrongObjectPtr<UCk_DebugOverlay_Subsystem> Subsystem;
        FString SavedCVar;
        EConsoleVariableFlags SavedCVarPriority = ECVF_Default;
        bool HasCVar = false;
        bool OwnsRequest = false;

        void RestoreSettings()
        {
            if (RestoreHandle.IsValid())
            {
                FEditorDelegates::EndPIE.Remove(RestoreHandle);
                RestoreHandle.Reset();
            }
            if (NOT HasPlaySettings) { return; }
            HasPlaySettings = false;
            if (auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>())
            {
                Settings->SetPlayNumberOfClients(SavedNumClients);
                Settings->SetPlayNetMode(SavedNetMode);
                Settings->SetRunUnderOneProcess(SavedOneProcess);
                Settings->bLaunchSeparateServer = SavedSeparateServer;
            }
        }

        void RestoreCVar()
        {
            if (NOT HasCVar) { return; }
            HasCVar = false;
            if (auto* Master = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay")))
            {
                const auto CurrentPriority =
                    static_cast<EConsoleVariableFlags>(Master->GetFlags() & ECVF_SetByMask);
                Master->Set(*SavedCVar, CurrentPriority);
                Master->SetFlags(static_cast<EConsoleVariableFlags>(
                    (Master->GetFlags() & ~ECVF_SetByMask) | SavedCVarPriority));
            }
        }
    };

    auto Worlds() -> TArray<UWorld*>
    {
        auto Result = TArray<UWorld*>{};
        if (GEngine == nullptr) { return Result; }
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
            { Result.Add(Context.World()); }
        }
        return Result;
    }

    auto HasPieContext() -> bool
    {
        if (GEngine == nullptr) { return false; }
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE) { return true; }
        }
        return false;
    }

    auto LocalPlayer(UWorld* InWorld) -> ULocalPlayer*
    {
        auto* Instance = IsValid(InWorld) ? InWorld->GetGameInstance() : nullptr;
        return IsValid(Instance) ? Instance->GetLocalPlayerByIndex(0) : nullptr;
    }

    auto IsPlayerReady(UWorld* InWorld) -> bool
    {
        auto* Player = LocalPlayer(InWorld);
        if (NOT IsValid(Player) || NOT IsValid(Player->GetSubsystem<UCk_DebugOverlay_Subsystem>()) ||
            NOT IsValid(Player->ViewportClient.Get()) ||
            Player->ViewportClient.Get() != InWorld->GetGameViewport() ||
            NOT IsValid(Player->GetPlayerController(InWorld)))
        { return false; }
        auto Projection = ck::DebugViewportView::FProjection{};
        return ck::DebugViewportView::TryGet_Projection(InWorld, Player, Projection);
    }

    class FRun final : public IAutomationLatentCommand
    {
    public:
        FRun(FAutomationTestBase* InTest, ETarget InTarget)
            : State(MakeShared<FState>())
        {
            State->Test = InTest;
            State->Target = InTarget;
        }

        virtual ~FRun() override
        {
            if (State->OwnsRequest && GUnrealEd != nullptr)
            {
                GUnrealEd->CancelRequestPlaySession();
                State->OwnsRequest = false;
                if (State->OwnedWorld.IsValid() && Worlds().Contains(State->OwnedWorld.Get()))
                { GUnrealEd->RequestEndPlayMap(); }
            }
            State->RestoreCVar();
            // If an owned PIE is still tearing down, EndPIE must restore before UE saves config.
            if (NOT (State->OwnedWorld.IsValid() && HasPieContext()))
            { State->RestoreSettings(); }
        }

        virtual bool Update() override
        {
            switch (Phase)
            {
                case EPhase::Start: return Start();
                case EPhase::WaitReady: return WaitReady();
                case EPhase::Stop: return Stop();
            }
            return true;
        }

    private:
        enum class EPhase : uint8 { Start, WaitReady, Stop };
        EPhase Phase = EPhase::Start;
        TSharedRef<FState> State;
        double PhaseStart = -1.0;
        bool StopRequested = false;
        bool RequestedEnd = false;

        void Error(const TCHAR* InMessage)
        {
            if (State->Test != nullptr) { State->Test->AddError(InMessage); }
        }

        bool Start()
        {
            if (GUnrealEd == nullptr)
            { Error(TEXT("Overlay net lifecycle requires GUnrealEd")); return true; }
            if (GUnrealEd->IsPlaySessionInProgress() || HasPieContext())
            { Error(TEXT("Overlay net lifecycle refuses an existing PIE session")); return true; }
            auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
            if (Settings == nullptr)
            { Error(TEXT("Overlay net lifecycle cannot read Play settings")); return true; }

            Settings->GetPlayNumberOfClients(State->SavedNumClients);
            Settings->GetPlayNetMode(State->SavedNetMode);
            Settings->GetRunUnderOneProcess(State->SavedOneProcess);
            State->SavedSeparateServer = Settings->bLaunchSeparateServer;
            State->HasPlaySettings = true;
            State->RestoreHandle = FEditorDelegates::EndPIE.AddLambda(
                [Saved = State](bool) { Saved->RestoreSettings(); });

            // PIE_Client adds one in-process dedicated server to the requested client count.
            Settings->SetPlayNumberOfClients(State->Target == ETarget::DedicatedClient ? 1 : 2);
            Settings->SetPlayNetMode(State->Target == ETarget::DedicatedClient
                ? EPlayNetMode::PIE_Client : EPlayNetMode::PIE_ListenServer);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;

            FRequestPlaySessionParams Params;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            State->OwnsRequest = true;
            GUnrealEd->RequestPlaySession(Params);
            Phase = EPhase::WaitReady;
            PhaseStart = FPlatformTime::Seconds();
            return false;
        }

        bool WaitReady()
        {
            const auto Current = Worlds();
            if (NOT Current.IsEmpty() && NOT State->OwnedWorld.IsValid())
            { State->OwnedWorld = Current[0]; }

            auto* Server = static_cast<UWorld*>(nullptr);
            auto* Client = static_cast<UWorld*>(nullptr);
            for (auto* World : Current)
            {
                if (World->GetNetMode() == NM_Client) { Client = World; }
                else if (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer)
                { Server = World; }
            }
            const auto ExpectedServerMode = State->Target == ETarget::DedicatedClient
                ? NM_DedicatedServer : NM_ListenServer;
            auto* TargetWorld = State->Target == ETarget::ListenHost ? Server : Client;
            auto* Player = LocalPlayer(TargetWorld);
            if (Current.Num() == 2 && IsValid(Server) && IsValid(Client) &&
                Server->GetNetMode() == ExpectedServerMode &&
                Server->HasBegunPlay() && Client->HasBegunPlay() &&
                IsPlayerReady(TargetWorld) &&
                (State->Target == ETarget::DedicatedClient
                    ? LocalPlayer(Server) == nullptr
                    : IsPlayerReady(Server) && IsPlayerReady(Client)))
            {
                Exercise(TargetWorld, Player);
                Phase = EPhase::Stop;
                PhaseStart = FPlatformTime::Seconds();
                return false;
            }
            if (FPlatformTime::Seconds() - PhaseStart >= 30.0)
            {
                Error(TEXT("Overlay net lifecycle timed out waiting for exactly one server, one client, and target LocalPlayer"));
                Phase = EPhase::Stop;
                PhaseStart = FPlatformTime::Seconds();
            }
            return false;
        }

        void Exercise(UWorld* InWorld, ULocalPlayer* InPlayer)
        {
            auto* Subsystem = InPlayer->GetSubsystem<UCk_DebugOverlay_Subsystem>();
            auto* Master = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay"));
            if (NOT IsValid(Subsystem) || Master == nullptr || NOT FSlateApplication::IsInitialized())
            { Error(TEXT("Overlay net lifecycle lost subsystem, cvar, or Slate")); return; }
            State->Subsystem.Reset(Subsystem);
            State->SavedCVar = Master->GetString();
            State->SavedCVarPriority =
                static_cast<EConsoleVariableFlags>(Master->GetFlags() & ECVF_SetByMask);
            State->HasCVar = true;
            auto Projection = ck::DebugViewportView::FProjection{};
            State->Test->TestTrue(TEXT("target player supplies its own projection"),
                ck::DebugViewportView::TryGet_Projection(InWorld, InPlayer, Projection));
            State->Test->TestFalse(TEXT("missing local player cannot supply a projection"),
                ck::DebugViewportView::TryGet_Projection(InWorld, nullptr, Projection));

            auto* OtherSubsystem = static_cast<UCk_DebugOverlay_Subsystem*>(nullptr);
            auto* OtherPlayer = static_cast<ULocalPlayer*>(nullptr);
            for (auto* World : Worlds())
            {
                if (World == InWorld || World->GetNetMode() == NM_DedicatedServer)
                { continue; }
                OtherPlayer = LocalPlayer(World);
                OtherSubsystem = IsValid(OtherPlayer)
                    ? OtherPlayer->GetSubsystem<UCk_DebugOverlay_Subsystem>() : nullptr;
            }

            Master->Set(0, ECVF_SetByConsole);
            Subsystem->Toggle_LocalVisibilityForTest();
            State->Test->TestTrue(TEXT("local shortcut activates only its owning player's overlay"),
                Subsystem->Get_TestState().HasRootWidget &&
                Subsystem->Get_TestAttachedPlayer() == InPlayer &&
                (OtherSubsystem == nullptr || NOT OtherSubsystem->Get_TestState().HasRootWidget));
            if (IsValid(OtherSubsystem))
            {
                auto ForeignProjection = ck::DebugViewportView::FProjection{};
                State->Test->TestFalse(TEXT("foreign local player cannot supply target-world projection"),
                    ck::DebugViewportView::TryGet_Projection(InWorld, OtherPlayer, ForeignProjection));
                State->Test->TestTrue(TEXT("foreign local player cannot borrow target editor camera"),
                    ck::DebugViewportView::TryGet_LevelEditorViewportForPlayer(InWorld, OtherPlayer) == nullptr);
                OtherSubsystem->Toggle_LocalVisibilityForTest();
                State->Test->TestTrue(TEXT("two local players own independent root widgets"),
                    OtherSubsystem->Get_TestState().HasRootWidget &&
                    OtherSubsystem->Get_TestState().HasInputProcessor &&
                    OtherSubsystem->Get_TestAttachedPlayer() == OtherPlayer &&
                    OtherSubsystem->Get_TestRootWidget() != Subsystem->Get_TestRootWidget());
                Subsystem->Toggle_LocalVisibilityForTest();
                State->Test->TestTrue(TEXT("hiding one local overlay preserves the other"),
                    NOT Subsystem->Get_TestState().HasRootWidget &&
                    OtherSubsystem->Get_TestState().HasRootWidget);
            }

            Master->Set(1, ECVF_SetByConsole);
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();
            const auto Active = Subsystem->Get_TestState();
            State->Test->TestTrue(TEXT("target LocalPlayer overlay activates in requested topology"),
                Active.HasRootWidget && Active.HasAttachedViewport && Active.HasTicker &&
                Active.HasInputProcessor && Active.HasSessionInvalidationCallback &&
                Active.HasWorldInvalidationCallback);
            if (IsValid(OtherSubsystem))
            {
                State->Test->TestTrue(TEXT("master cvar activates both local players"),
                    OtherSubsystem->Get_TestState().HasRootWidget &&
                    OtherSubsystem->Get_TestAttachedPlayer() == OtherPlayer);
            }
            if (GEngine != nullptr) { GEngine->Exec(InWorld, TEXT("ck.DebugOverlay.Settings")); }
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();
            State->Test->TestTrue(TEXT("target LocalPlayer owns settings panel"),
                Subsystem->Get_TestState().HasSelectionPanel);

            Master->Set(0, ECVF_SetByConsole);
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();
            const auto Inactive = Subsystem->Get_TestState();
            State->Test->TestTrue(TEXT("target LocalPlayer overlay deactivates without losing routing"),
                NOT Inactive.HasRootWidget && NOT Inactive.HasAttachedViewport &&
                NOT Inactive.HasTicker && NOT Inactive.HasSelectionPanel &&
                Inactive.HasInputProcessor && Inactive.HasSessionInvalidationCallback &&
                Inactive.HasWorldInvalidationCallback);
            if (IsValid(OtherSubsystem))
            { State->Test->TestFalse(TEXT("master cvar deactivates peer overlay"), OtherSubsystem->Get_TestState().HasRootWidget); }
        }

        bool Stop()
        {
            const auto Current = Worlds();
            const auto OwnsVisibleWorld = State->OwnedWorld.IsValid() &&
                Current.Contains(State->OwnedWorld.Get());
            if (NOT StopRequested)
            {
                StopRequested = true;
                if (State->OwnsRequest && GUnrealEd != nullptr)
                {
                    GUnrealEd->CancelRequestPlaySession();
                    State->OwnsRequest = false;
                }
                if (OwnsVisibleWorld)
                {
                    if (GUnrealEd == nullptr)
                    { Error(TEXT("Cannot stop owned overlay PIE without GUnrealEd")); }
                    else { GUnrealEd->RequestEndPlayMap(); RequestedEnd = true; }
                    return false;
                }
                if (NOT Current.IsEmpty())
                { Error(TEXT("Overlay net lifecycle refuses to stop an unobserved PIE session")); }
            }
            if (RequestedEnd && NOT Current.IsEmpty() &&
                FPlatformTime::Seconds() - PhaseStart < 30.0)
            { return false; }
            if (RequestedEnd && NOT Current.IsEmpty())
            { Error(TEXT("Overlay PIE worlds did not end within 30 seconds")); }

            if (auto* Subsystem = State->Subsystem.Get(); IsValid(Subsystem))
            {
                const auto Final = Subsystem->Get_TestState();
                State->Test->TestTrue(TEXT("target LocalPlayer overlay deinitializes on PIE end"),
                    NOT Final.HasRootWidget && NOT Final.HasAttachedViewport &&
                    NOT Final.HasInputProcessor && NOT Final.HasSessionInvalidationCallback &&
                    NOT Final.HasWorldInvalidationCallback);
            }
            State->RestoreCVar();
            if (NOT (State->OwnedWorld.IsValid() && HasPieContext()))
            { State->RestoreSettings(); }
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SubsystemLifecycle_ListenHost,
    "Ck.DebugOverlay.Subsystem.Lifecycle.ListenServer.Host",
    ck_debug_overlay_subsystem_lifecycle_spec::TestFlags)
bool FCkDebugOverlay_SubsystemLifecycle_ListenHost::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(ck_debug_overlay_net_lifecycle_spec::FRun(
        this, ck_debug_overlay_net_lifecycle_spec::ETarget::ListenHost));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SubsystemLifecycle_ListenClient,
    "Ck.DebugOverlay.Subsystem.Lifecycle.ListenServer.Client",
    ck_debug_overlay_subsystem_lifecycle_spec::TestFlags)
bool FCkDebugOverlay_SubsystemLifecycle_ListenClient::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(ck_debug_overlay_net_lifecycle_spec::FRun(
        this, ck_debug_overlay_net_lifecycle_spec::ETarget::ListenClient));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SubsystemLifecycle_DedicatedClient,
    "Ck.DebugOverlay.Subsystem.Lifecycle.ClientServer.Client",
    ck_debug_overlay_subsystem_lifecycle_spec::TestFlags)
bool FCkDebugOverlay_SubsystemLifecycle_DedicatedClient::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(ck_debug_overlay_net_lifecycle_spec::FRun(
        this, ck_debug_overlay_net_lifecycle_spec::ETarget::DedicatedClient));
    return true;
}


#endif
