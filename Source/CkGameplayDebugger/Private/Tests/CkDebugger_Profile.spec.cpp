#include "CkDebugger_ProfileTestTypes.h"

#include "CkGameplayDebugger/Bridge/CkDebugger_Bridge.h"
#include "CkGameplayDebugger/Profile/CkDebugger_Profile.h"

#include <Engine/Engine.h>
#include <Engine/EngineBaseTypes.h>
#include <Engine/World.h>
#include <Misc/AutomationTest.h>
#include <UObject/StrongObjectPtr.h>

#if WITH_DEV_AUTOMATION_TESTS && WITH_GAMEPLAY_DEBUGGER

struct FCk_DebuggerProfile_TestAccess
{
    static auto
        Request_Activate(
            ACk_GameplayDebugger_DebugBridge_UE& InBridge)
        -> void
    { InBridge.OnGameplayDebugger_Activated(); }

    static auto
        Request_Deactivate(
            ACk_GameplayDebugger_DebugBridge_UE& InBridge)
        -> void
    { InBridge.OnGameplayDebugger_Deactivated(); }

    static auto
        Get_LoadedProfile(
            ACk_GameplayDebugger_DebugBridge_UE& InBridge)
        -> UCk_GameplayDebugger_DebugProfile_PDA*
    { return InBridge._CurrentlyLoadedDebugProfile; }

    static auto
        Request_ChangeFilter(
            ACk_GameplayDebugger_DebugBridge_UE& InBridge,
            int32 InPrevious,
            int32 InNext)
        -> void
    { InBridge.DoChangeFilter(InPrevious, InNext); }

    static auto
        Request_SetSelection(
            ACk_GameplayDebugger_DebugBridge_UE& InBridge,
            AActor* InActor)
        -> void
    {
        InBridge._CurrentlySelectedActorIndex = 9;
        InBridge._CurrentWorldToUseIndex = 2;
        InBridge._PreviouslySelectedActor = InActor;
        InBridge._SelectedDebugActor = InActor;
    }

    static auto
        Get_IsSelectionReset(
            const ACk_GameplayDebugger_DebugBridge_UE& InBridge)
        -> bool
    {
        return InBridge._CurrentlySelectedActorIndex == 0 && InBridge._CurrentlySelectedFilterIndex == 0 &&
            InBridge._CurrentWorldToUseIndex == 0 && NOT InBridge._PreviouslySelectedActor.IsValid() &&
            NOT InBridge._SelectedDebugActor.IsValid();
    }

    static auto
        Get_FilterIndex(
            const ACk_GameplayDebugger_DebugBridge_UE& InBridge)
        -> int32
    { return InBridge._CurrentlySelectedFilterIndex; }

    static auto
        Request_Configure(
            UCk_GameplayDebugger_DebugProfile_PDA& InProfile,
            int32 InFilterCount)
        -> void
    {
        InProfile._Submenus.Add(NewObject<UCk_DebuggerProfile_TestSubmenu>(&InProfile));
        InProfile._GlobalActions.Add(NewObject<UCk_DebuggerProfile_TestAction>(&InProfile));
        for (auto Index = 0; Index < InFilterCount; ++Index)
        {
            auto Filter = NewObject<UCk_DebuggerProfile_TestFilter>(&InProfile);
            Filter->_FilterDebugActions.Add(NewObject<UCk_DebuggerProfile_TestAction>(Filter));
            InProfile._Filters.Add(Filter);
        }
    }

    static auto
        Request_AddInvalidEntry(
            UCk_GameplayDebugger_DebugProfile_PDA& InProfile,
            int32 InKind)
        -> void
    {
        switch (InKind)
        {
            case 0:
            {
                InProfile._Submenus.Add(nullptr);
                break;
            }
            case 1:
            {
                InProfile._Filters.Add(nullptr);
                break;
            }
            case 2:
            {
                InProfile._GlobalActions.Add(nullptr);
                break;
            }
            case 3:
            {
                auto Filter = NewObject<UCk_DebuggerProfile_TestFilter>(&InProfile);
                Filter->_FilterDebugActions.Add(nullptr);
                InProfile._Filters.Add(Filter);
                break;
            }
        }
    }

    static auto
        Request_InvalidateFilter(
            UCk_GameplayDebugger_DebugProfile_PDA& InProfile,
            int32 InIndex)
        -> void
    { InProfile._Filters[InIndex] = nullptr; }
};

namespace ck_debugger_profile_tests
{
    struct FWorldScope
    {
    public:
        CK_GENERATED_BODY(FWorldScope);

    public:
        FWorldScope()
            : _World{UWorld::CreateWorld(EWorldType::Game, false)}
        {}

        ~FWorldScope()
        {
            if (_World.IsValid())
            {
                _World->DestroyWorld(false);
                if (_HasWorldContext && GEngine != nullptr)
                { GEngine->DestroyWorldContext(_World.Get()); }
            }
        }

        auto
            Request_InitializeActorsForPlay()
            -> bool
        {
            if (NOT _World.IsValid() || GEngine == nullptr)
            { return false; }

            auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            Context.SetCurrentWorld(_World.Get());
            _HasWorldContext = true;
            _World->InitializeActorsForPlay(FURL{});
            return _World->AreActorsInitialized();
        }

    private:
        TStrongObjectPtr<UWorld> _World;

        bool _HasWorldContext = false;

    public:
        CK_PROPERTY_GET(_World);
    };

    auto
        Get_HasFixtureEntries(
            const UCk_GameplayDebugger_DebugProfile_PDA& InProfile,
            int32 InFilterCount)
        -> bool
    {
        if (InProfile.Get_Submenus().Num() != 1 || InProfile.Get_GlobalActions().Num() != 1 ||
            InProfile.Get_Filters().Num() != InFilterCount || NOT InProfile.Get_HasValidRequiredEntries())
        { return false; }

        for (const auto& Filter : InProfile.Get_Filters())
        {
            if (Filter->Get_FilterDebugActions().Num() != 1)
            { return false; }
        }
        return true;
    }

    auto
        Request_CreateProfile(
            UObject* InOuter,
            int32 InFilterCount)
        -> UCk_GameplayDebugger_DebugProfile_PDA*
    {
        auto Profile = NewObject<UCk_GameplayDebugger_DebugProfile_PDA>(InOuter);
        FCk_DebuggerProfile_TestAccess::Request_Configure(*Profile, InFilterCount);
        return Profile;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerProfile_Lifecycle,
    "Ck.GameplayDebugger.TenetIntegrity.ProfileLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebuggerProfile_Lifecycle::
    RunTest(const FString&)
    -> bool
{
    using FAccess = FCk_DebuggerProfile_TestAccess;
    const auto WorldScope = ck_debugger_profile_tests::FWorldScope{};
    auto World = WorldScope.Get_World().Get();
    if (NOT TestNotNull(TEXT("transient world exists"), World))
    { return false; }

    auto Bridge = World->SpawnActor<ACk_GameplayDebugger_DebugBridge_UE>();
    if (NOT TestNotNull(TEXT("bridge spawns through production actor path"), Bridge))
    { return false; }

    auto First = ck_debugger_profile_tests::Request_CreateProfile(World, 3);
    Bridge->LoadNewDebugProfile(First);
    auto FirstLoaded = FAccess::Get_LoadedProfile(*Bridge);
    if (NOT TestNotNull(TEXT("profile is cloned"), FirstLoaded))
    { return false; }

    if (NOT TestTrue(TEXT("cloned profile retains configured fixture entries"),
        ck_debugger_profile_tests::Get_HasFixtureEntries(*FirstLoaded, 3)))
    { return false; }

    auto Submenu = Cast<UCk_DebuggerProfile_TestSubmenu>(FirstLoaded->Get_Submenus()[0]);
    auto GlobalAction = Cast<UCk_DebuggerProfile_TestAction>(FirstLoaded->Get_GlobalActions()[0]);
    auto FilterAction = Cast<UCk_DebuggerProfile_TestAction>(FirstLoaded->Get_Filters()[2]->Get_FilterDebugActions()[0]);
    if (NOT TestNotNull(TEXT("concrete submenu clone exists"), Submenu) ||
        NOT TestNotNull(TEXT("concrete global action clone exists"), GlobalAction) ||
        NOT TestNotNull(TEXT("concrete filter action clone exists"), FilterAction))
    { return false; }

    FAccess::Request_Deactivate(*Bridge);
    TestEqual(TEXT("never activated submenu receives no deactivation"), Submenu->Get_DeactivationCount(), 0);
    FAccess::Request_Activate(*Bridge);
    FAccess::Request_Activate(*Bridge);
    TestEqual(TEXT("repeated category activation invokes submenu once"), Submenu->Get_ActivationCount(), 1);
    TestEqual(TEXT("repeated activation invokes global action once"), GlobalAction->Get_ActivationCount(), 1);
    FAccess::Request_ChangeFilter(*Bridge, 0, 2);
    TestEqual(TEXT("third filter action activates"), FilterAction->Get_ActivationCount(), 1);
    FAccess::Request_SetSelection(*Bridge, Bridge);

    auto Replacement = ck_debugger_profile_tests::Request_CreateProfile(World, 1);
    Bridge->LoadNewDebugProfile(Replacement);
    TestEqual(TEXT("active replacement deactivates old submenu once"), Submenu->Get_DeactivationCount(), 1);
    TestEqual(TEXT("active replacement deactivates global action once"), GlobalAction->Get_DeactivationCount(), 1);
    TestEqual(TEXT("active replacement deactivates selected filter action once"), FilterAction->Get_DeactivationCount(), 1);
    TestTrue(TEXT("three-filter to one-filter replacement resets all selection"), FAccess::Get_IsSelectionReset(*Bridge));

    auto NewLoaded = FAccess::Get_LoadedProfile(*Bridge);
    if (NOT TestNotNull(TEXT("replacement is loaded"), NewLoaded))
    { return false; }
    if (NOT TestTrue(TEXT("cloned profile retains configured fixture entries"),
        ck_debugger_profile_tests::Get_HasFixtureEntries(*NewLoaded, 1)))
    { return false; }

    auto NewSubmenu = Cast<UCk_DebuggerProfile_TestSubmenu>(NewLoaded->Get_Submenus()[0]);
    if (NOT TestNotNull(TEXT("replacement submenu is concrete"), NewSubmenu))
    { return false; }
    TestEqual(TEXT("replacement activates while category stays active"), NewSubmenu->Get_ActivationCount(), 1);
    Bridge->UnloadCurrentDebugProfile();
    Bridge->UnloadCurrentDebugProfile();
    TestEqual(TEXT("repeated unload deactivates replacement once"), NewSubmenu->Get_DeactivationCount(), 1);
    TestTrue(TEXT("unload resets all selection"), FAccess::Get_IsSelectionReset(*Bridge));

    Bridge->LoadNewDebugProfile(Replacement);
    auto Reloaded = FAccess::Get_LoadedProfile(*Bridge);
    if (NOT TestNotNull(TEXT("load after unload succeeds"), Reloaded))
    { return false; }
    if (NOT TestTrue(TEXT("cloned profile retains configured fixture entries"),
        ck_debugger_profile_tests::Get_HasFixtureEntries(*Reloaded, 1)))
    { return false; }

    auto ReloadedSubmenu = Cast<UCk_DebuggerProfile_TestSubmenu>(Reloaded->Get_Submenus()[0]);
    if (NOT TestNotNull(TEXT("reloaded submenu exists"), ReloadedSubmenu))
    { return false; }
    TestEqual(TEXT("unload preserves category activity for next load"), ReloadedSubmenu->Get_ActivationCount(), 1);
    FAccess::Request_Deactivate(*Bridge);
    FAccess::Request_Deactivate(*Bridge);
    Bridge->UnloadCurrentDebugProfile();
    TestEqual(TEXT("deactivate followed by unload invokes hook once"), ReloadedSubmenu->Get_DeactivationCount(), 1);

    auto NeverActivated = NewObject<UCk_DebuggerProfile_TestSubmenu>(World);
    NeverActivated->DeactivateSubmenu();
    TestEqual(TEXT("submenu itself rejects never activated deactivation"), NeverActivated->Get_DeactivationCount(), 0);
    NeverActivated->ActivateSubmenu();
    NeverActivated->ActivateSubmenu();
    NeverActivated->DeactivateSubmenu();
    NeverActivated->DeactivateSubmenu();
    TestEqual(TEXT("submenu activation is idempotent"), NeverActivated->Get_ActivationCount(), 1);
    TestEqual(TEXT("submenu deactivation is idempotent"), NeverActivated->Get_DeactivationCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerProfile_Admission,
    "Ck.GameplayDebugger.TenetIntegrity.AtomicProfileAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebuggerProfile_Admission::
    RunTest(const FString&)
    -> bool
{
    using FAccess = FCk_DebuggerProfile_TestAccess;
    const auto WorldScope = ck_debugger_profile_tests::FWorldScope{};
    auto World = WorldScope.Get_World().Get();
    if (NOT TestNotNull(TEXT("transient world exists"), World))
    { return false; }
    auto Bridge = World->SpawnActor<ACk_GameplayDebugger_DebugBridge_UE>();
    if (NOT TestNotNull(TEXT("production bridge spawns"), Bridge))
    { return false; }
    Bridge->LoadNewDebugProfile(ck_debugger_profile_tests::Request_CreateProfile(World, 2));
    FAccess::Request_Activate(*Bridge);
    auto Loaded = FAccess::Get_LoadedProfile(*Bridge);
    if (NOT TestNotNull(TEXT("initial profile loaded"), Loaded))
    { return false; }
    if (NOT TestTrue(TEXT("profile retains complete fixture entries"), ck_debugger_profile_tests::Get_HasFixtureEntries(*Loaded, 2)))
    { return false; }

    auto Submenu = Cast<UCk_DebuggerProfile_TestSubmenu>(Loaded->Get_Submenus()[0]);
    auto Action = Cast<UCk_DebuggerProfile_TestAction>(Loaded->Get_Filters()[0]->Get_FilterDebugActions()[0]);
    if (NOT TestNotNull(TEXT("submenu fixture exists"), Submenu) ||
        NOT TestNotNull(TEXT("filter action fixture exists"), Action))
    { return false; }

    AddExpectedError(TEXT("GameplayDebugger Debug Profile contains an invalid required entry"), EAutomationExpectedErrorFlags::Contains, 0);
    for (auto Kind = 0; Kind < 4; ++Kind)
    {
        auto Invalid = ck_debugger_profile_tests::Request_CreateProfile(World, 1);
        FAccess::Request_AddInvalidEntry(*Invalid, Kind);
        Bridge->LoadNewDebugProfile(Invalid);
        TestTrue(TEXT("invalid required entry preserves old profile identity"), FAccess::Get_LoadedProfile(*Bridge) == Loaded);
        TestEqual(TEXT("rejected replacement never tears down old submenu"), Submenu->Get_DeactivationCount(), 0);
        auto RejectedSubmenu = Cast<UCk_DebuggerProfile_TestSubmenu>(Invalid->Get_Submenus()[0]);
        if (TestNotNull(TEXT("rejected submenu fixture exists"), RejectedSubmenu))
        { TestEqual(TEXT("rejected profile executes no activation callbacks"), RejectedSubmenu->Get_ActivationCount(), 0); }
    }

    AddExpectedError(TEXT("Invalid previous debug filter index"), EAutomationExpectedErrorFlags::Contains, 0);
    FAccess::Request_ChangeFilter(*Bridge, 8, 1);
    TestEqual(TEXT("invalid old index does not mutate selected filter"), FAccess::Get_FilterIndex(*Bridge), 0);
    TestEqual(TEXT("invalid old index leaves current action active"), Action->Get_DeactivationCount(), 0);
    FAccess::Request_InvalidateFilter(*Loaded, 1);
    AddExpectedError(TEXT("Invalid replacement debug filter"), EAutomationExpectedErrorFlags::Contains, 0);
    FAccess::Request_ChangeFilter(*Bridge, 0, 1);
    TestEqual(TEXT("invalid replacement leaves old selection"), FAccess::Get_FilterIndex(*Bridge), 0);
    TestEqual(TEXT("invalid replacement leaves current action active"), Action->Get_DeactivationCount(), 0);
    Bridge->UnloadCurrentDebugProfile();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerProfile_EndPlay,
    "Ck.GameplayDebugger.TenetIntegrity.BridgeEndPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebuggerProfile_EndPlay::
    RunTest(const FString&)
    -> bool
{
    using FAccess = FCk_DebuggerProfile_TestAccess;
    auto WorldScope = ck_debugger_profile_tests::FWorldScope{};
    auto World = WorldScope.Get_World().Get();
    if (NOT TestNotNull(TEXT("transient world exists"), World))
    { return false; }
    if (NOT TestTrue(TEXT("game world initializes actors through the production play path"), WorldScope.Request_InitializeActorsForPlay()))
    { return false; }

    auto Bridge = World->SpawnActor<ACk_GameplayDebugger_DebugBridge_UE>();
    if (NOT TestNotNull(TEXT("production bridge spawns"), Bridge))
    { return false; }

    if (NOT TestTrue(TEXT("spawned bridge completed actor initialization"), Bridge->IsActorInitialized()))
    { return false; }

    Bridge->DispatchBeginPlay();
    if (NOT TestTrue(TEXT("bridge enters real actor play lifecycle"), Bridge->HasActorBegunPlay()))
    { return false; }
    Bridge->LoadNewDebugProfile(ck_debugger_profile_tests::Request_CreateProfile(World, 1));
    FAccess::Request_Activate(*Bridge);
    auto Loaded = FAccess::Get_LoadedProfile(*Bridge);
    if (NOT TestNotNull(TEXT("active profile loaded"), Loaded))
    { return false; }
    if (NOT TestTrue(TEXT("profile retains complete fixture entries"), ck_debugger_profile_tests::Get_HasFixtureEntries(*Loaded, 1)))
    { return false; }

    auto Submenu = Cast<UCk_DebuggerProfile_TestSubmenu>(Loaded->Get_Submenus()[0]);
    auto Global = Cast<UCk_DebuggerProfile_TestAction>(Loaded->Get_GlobalActions()[0]);
    auto FilterAction = Cast<UCk_DebuggerProfile_TestAction>(Loaded->Get_Filters()[0]->Get_FilterDebugActions()[0]);
    if (NOT TestNotNull(TEXT("submenu fixture exists"), Submenu) ||
        NOT TestNotNull(TEXT("global action fixture exists"), Global) ||
        NOT TestNotNull(TEXT("filter action fixture exists"), FilterAction))
    { return false; }

    TestTrue(TEXT("world destroys active bridge"), World->DestroyActor(Bridge));
    TestEqual(TEXT("EndPlay deactivates submenu once"), Submenu->Get_DeactivationCount(), 1);
    TestEqual(TEXT("EndPlay deactivates global action once"), Global->Get_DeactivationCount(), 1);
    TestEqual(TEXT("EndPlay deactivates selected filter action once"), FilterAction->Get_DeactivationCount(), 1);
    TestNull(TEXT("EndPlay unloads profile"), FAccess::Get_LoadedProfile(*Bridge));
    TestTrue(TEXT("EndPlay resets all selection"), FAccess::Get_IsSelectionReset(*Bridge));
    Bridge->UnloadCurrentDebugProfile();
    TestEqual(TEXT("unload after EndPlay repeats no callback"), Submenu->Get_DeactivationCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerProfile_Reentry,
    "Ck.GameplayDebugger.TenetIntegrity.RejectCallbackReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebuggerProfile_Reentry::
    RunTest(const FString&)
    -> bool
{
    using FAccess = FCk_DebuggerProfile_TestAccess;
    const auto WorldScope = ck_debugger_profile_tests::FWorldScope{};
    auto World = WorldScope.Get_World().Get();
    if (NOT TestNotNull(TEXT("transient world exists"), World))
    { return false; }
    auto Bridge = World->SpawnActor<ACk_GameplayDebugger_DebugBridge_UE>();
    if (NOT TestNotNull(TEXT("production bridge spawns"), Bridge))
    { return false; }
    Bridge->LoadNewDebugProfile(ck_debugger_profile_tests::Request_CreateProfile(World, 1));
    auto Loaded = FAccess::Get_LoadedProfile(*Bridge);
    if (NOT TestNotNull(TEXT("profile loaded"), Loaded))
    { return false; }
    if (NOT TestTrue(TEXT("fixture entries survived cloning"), ck_debugger_profile_tests::Get_HasFixtureEntries(*Loaded, 1)))
    { return false; }
    auto Submenu = Cast<UCk_DebuggerProfile_TestSubmenu>(Loaded->Get_Submenus()[0]);
    auto Action = Cast<UCk_DebuggerProfile_TestAction>(Loaded->Get_GlobalActions()[0]);
    if (NOT TestNotNull(TEXT("submenu fixture exists"), Submenu) ||
        NOT TestNotNull(TEXT("action fixture exists"), Action))
    { return false; }

    auto Replacement = ck_debugger_profile_tests::Request_CreateProfile(World, 1);
    Submenu->ConfigureReentry(Bridge, Replacement);
    AddExpectedError(TEXT("must not request nested profile transitions"), EAutomationExpectedErrorFlags::Contains, 0);
    FAccess::Request_Activate(*Bridge);
    TestTrue(TEXT("nested load preserves profile identity"), FAccess::Get_LoadedProfile(*Bridge) == Loaded);
    TestEqual(TEXT("outer activation completes once"), Submenu->Get_ActivationCount(), 1);
    TestEqual(TEXT("outer activation reaches global action once"), Action->Get_ActivationCount(), 1);
    auto RejectedSubmenu = Cast<UCk_DebuggerProfile_TestSubmenu>(Replacement->Get_Submenus()[0]);
    if (NOT TestNotNull(TEXT("rejected replacement fixture exists"), RejectedSubmenu))
    { return false; }
    TestEqual(TEXT("nested load triggers no replacement callbacks"), RejectedSubmenu->Get_ActivationCount(), 0);
    FAccess::Request_Deactivate(*Bridge);
    TestTrue(TEXT("nested unload preserves inactive loaded profile"), FAccess::Get_LoadedProfile(*Bridge) == Loaded);
    TestEqual(TEXT("outer deactivation completes once"), Submenu->Get_DeactivationCount(), 1);
    TestEqual(TEXT("outer deactivation reaches global action once"), Action->Get_DeactivationCount(), 1);
    Submenu->ConfigureReentry(nullptr, nullptr);
    Bridge->UnloadCurrentDebugProfile();
    return true;
}

#endif
