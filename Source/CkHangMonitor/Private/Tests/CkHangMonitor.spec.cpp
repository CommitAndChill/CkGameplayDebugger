#include "CkHangMonitor/CkHangMonitorController.h"

#include "CkHangMonitor_Module.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

class FCkHangMonitorTestAccess
{
public:
    static auto
    Service(
        FCkHangMonitorController& InController)
    -> void
    { InController.DoService_Processes(); }

    static auto
    ObserveCancellation(
        FCkHangMonitorController& InController,
        TFunction<void(uint32)> InObserver)
    -> void
    { InController._OnCancelStarted = MoveTemp(InObserver); }

    static auto
    AdoptRunningHost(
        FCkHangMonitorController& InController,
        const bool InReady)
    -> void
    {
        InController._MonitorProcess = FPlatformProcess::OpenProcess(FPlatformProcess::GetCurrentProcessId());
        InController._Snapshot.IsMonitoring = true;
        InController._Snapshot.IsReady = InReady;
        InController._RejectCancelLaunchForTest = true;
        InController._ShutdownBudget = FCk_Time{};
    }

    static auto
    HasPendingProcesses(
        const FCkHangMonitorController& InController)
    -> bool
    { return InController.Get_HasPendingProcesses(); }

    static auto
    AdoptRunningHelper(
        FCkHangMonitorController& InController)
    -> void
    {
        InController._CancelProcess = FPlatformProcess::OpenProcess(FPlatformProcess::GetCurrentProcessId());
        InController._Snapshot.IsStopping = true;
        InController._CancelIssued = true;
        InController._ShutdownBudget = FCk_Time{};
    }
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_hang_monitor_tests
{
    constexpr auto kTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    constexpr auto kControllerTimeoutSeconds = 20.0;

    static auto Test_IsStoppedWithoutPartialState(
        FAutomationTestBase& InTest,
        const FCkHangMonitorSnapshot& InSnapshot,
        const TCHAR* InContext) -> void
    {
        InTest.TestFalse(*FString::Printf(TEXT("%s does not report monitoring"), InContext), InSnapshot.IsMonitoring);
        InTest.TestFalse(*FString::Printf(TEXT("%s does not report stopping"), InContext), InSnapshot.IsStopping);
        InTest.TestFalse(*FString::Printf(TEXT("%s does not report ready"), InContext), InSnapshot.IsReady);
        InTest.TestEqual(*FString::Printf(TEXT("%s retains no target PID"), InContext), InSnapshot.TargetPid, uint32{0});
        InTest.TestTrue(*FString::Printf(TEXT("%s retains no output directory"), InContext), InSnapshot.OutputDirectory.IsEmpty());
        InTest.TestTrue(*FString::Printf(TEXT("%s retains no ProcDump path"), InContext), InSnapshot.ProcDumpPath.IsEmpty());
        InTest.TestEqual(*FString::Printf(TEXT("%s retains no dump count"), InContext), InSnapshot.NumDumps, 0);
        InTest.TestEqual(*FString::Printf(TEXT("%s retains the default dump bound"), InContext), InSnapshot.MaxDumps, 1);
    }

    static auto Get_MissingProcDumpPath() -> FString
    {
        return FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("Automation"),
            TEXT("CkHangMonitor"),
            TEXT("does-not-exist"),
            TEXT("procdump64.exe"));
    }

    class FCk_Latent_HangMonitorArmStop final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_HangMonitorArmStop(FAutomationTestBase* InTest, FString InProcDumpPath)
            : _Test(InTest)
            , _ProcDumpPath(MoveTemp(InProcDumpPath))
        {
        }

        auto
        Update()
        -> bool override
        {
            auto& Module = FCkHangMonitorModule::Get();
            auto& Controller = Module.Get_Controller();
            const auto Now = FPlatformTime::Seconds();

            switch (_Phase)
            {
                case EPhase::Start:
                {
                    if (Controller.Get_Snapshot().IsMonitoring)
                    {
                        _Test->AddError(TEXT("Opt-in test requires an inactive Hang Monitor controller."));
                        return true;
                    }

                    auto Error = FString{};
                    if (!Controller.TryStart(_ProcDumpPath, 1, Error))
                    {
                        _Test->AddError(FString::Printf(TEXT("Current-editor ProcDump monitor did not arm: %s"), *Error));
                        return true;
                    }

                    _Deadline = Now + kControllerTimeoutSeconds;
                    _Phase = EPhase::AwaitReady;
                    return false;
                }

                case EPhase::AwaitReady:
                {
                    const auto Armed = Controller.Get_Snapshot();
                    if (Armed.IsMonitoring && Armed.IsReady)
                    {
                        _Test->TestFalse(TEXT("ready monitor is not stopping"), Armed.IsStopping);
                        _Test->TestEqual(TEXT("ready monitor targets the current editor process"),
                            Armed.TargetPid, FPlatformProcess::GetCurrentProcessId());
                        _Test->TestEqual(TEXT("ready monitor retains the requested one-dump bound"), Armed.MaxDumps, 1);
                        _Test->TestEqual(TEXT("ready monitor retains the explicit ProcDump path"), Armed.ProcDumpPath, _ProcDumpPath);
                        _Test->TestFalse(TEXT("ready monitor publishes an output directory"), Armed.OutputDirectory.IsEmpty());

                        auto DuplicateError = FString{};
                        _Test->TestFalse(TEXT("duplicate arm is rejected while monitoring"),
                            Controller.TryStart(_ProcDumpPath, 1, DuplicateError));
                        _Test->TestFalse(TEXT("duplicate arm rejection explains why"), DuplicateError.IsEmpty());
                        const auto AfterDuplicate = Controller.Get_Snapshot();
                        _Test->TestTrue(TEXT("duplicate arm retains the original monitor"), AfterDuplicate.IsMonitoring);
                        _Test->TestEqual(TEXT("duplicate arm retains the original target"), AfterDuplicate.TargetPid, Armed.TargetPid);
                        _Test->TestEqual(TEXT("duplicate arm retains the original output directory"),
                            AfterDuplicate.OutputDirectory, Armed.OutputDirectory);

                        Module.OpenDebugger();
                        _Test->TestTrue(TEXT("the Hang Monitor tab opens while its controller is armed"), Module.IsDebuggerOpen());
                        Module.CloseDebugger();
                        _Test->TestFalse(TEXT("the Hang Monitor tab closes"), Module.IsDebuggerOpen());
                        _Test->TestTrue(TEXT("tab closure preserves the armed controller"),
                            Controller.Get_Snapshot().IsMonitoring);

                        auto StopError = FString{};
                        if (!Controller.TryStop(StopError))
                        {
                            _Test->AddError(FString::Printf(TEXT("ProcDump graceful cancellation was rejected: %s"), *StopError));
                            return true;
                        }

                        _Deadline = Now + kControllerTimeoutSeconds;
                        _Phase = EPhase::AwaitStopped;
                        return false;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(FString::Printf(
                        TEXT("ProcDump did not reach its real attachment banner before timeout. Status=[%s] Error=[%s]"),
                        *Armed.Status,
                        *Armed.LastError));
                    RequestStop(Controller);
                    _Deadline = Now + kControllerTimeoutSeconds;
                    _Phase = EPhase::AwaitStopped;
                    return false;
                }

                case EPhase::AwaitStopped:
                {
                    const auto Stopped = Controller.Get_Snapshot();
                    if (!Stopped.IsMonitoring && !Stopped.IsStopping)
                    {
                        _Test->TestFalse(TEXT("stopped monitor leaves no cancellation-in-progress state"), Stopped.IsStopping);
                        _Test->TestFalse(TEXT("stopped monitor clears its readiness state"), Stopped.IsReady);
                        _Test->TestTrue(TEXT("gracefully stopped monitor reports no ProcDump error"), Stopped.LastError.IsEmpty());

                        auto RestartError = FString{};
                        if (!Controller.TryStart(_ProcDumpPath, 1, RestartError))
                        {
                            _Test->AddError(FString::Printf(TEXT("Second monitor arm for early-stop coverage failed: %s"), *RestartError));
                            return true;
                        }

                        auto EarlyStopError = FString{};
                        if (!Controller.TryStop(EarlyStopError))
                        {
                            _Test->AddError(FString::Printf(TEXT("Early ProcDump cancellation was rejected: %s"), *EarlyStopError));
                            return true;
                        }

                        const auto EarlyStopping = Controller.Get_Snapshot();
                        _Test->TestTrue(TEXT("early cancellation retains its child until attachment/cancellation completes"),
                            EarlyStopping.IsMonitoring);
                        _Test->TestTrue(TEXT("early cancellation is queued as stopping"), EarlyStopping.IsStopping);
                        _Deadline = Now + kControllerTimeoutSeconds;
                        _Phase = EPhase::AwaitEarlyStopped;
                        return false;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(FString::Printf(
                        TEXT("ProcDump did not exit after bounded graceful cancellation. Status=[%s] Error=[%s]"),
                        *Stopped.Status,
                        *Stopped.LastError));
                    return true;
                }

                case EPhase::AwaitEarlyStopped:
                {
                    const auto Stopped = Controller.Get_Snapshot();
                    if (!Stopped.IsMonitoring && !Stopped.IsStopping)
                    {
                        _Test->TestFalse(TEXT("early stopped monitor clears cancellation-in-progress state"), Stopped.IsStopping);
                        _Test->TestFalse(TEXT("early stopped monitor clears readiness state"), Stopped.IsReady);
                        _Test->TestTrue(TEXT("early graceful cancellation reports no ProcDump error"), Stopped.LastError.IsEmpty());
                        return true;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(FString::Printf(
                        TEXT("Early ProcDump cancellation did not finish. Status=[%s] Error=[%s]"),
                        *Stopped.Status,
                        *Stopped.LastError));
                    return true;
                }
            }

            _Test->AddError(TEXT("Hang Monitor test reached an unknown latent phase."));
            return true;
        }

    private:
        enum class EPhase : uint8
        {
            Start,
            AwaitReady,
            AwaitStopped,
            AwaitEarlyStopped
        };

        auto RequestStop(FCkHangMonitorController& InController) -> void
        {
            auto IgnoredError = FString{};
            InController.TryStop(IgnoredError);
        }

        FAutomationTestBase* _Test = nullptr;
        FString _ProcDumpPath;
        double _Deadline = 0.0;
        EPhase _Phase = EPhase::Start;
    };
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_RejectsInvalidStartWithoutPartialState,
    "Ck.DebuggerLauncher.HangMonitor.RejectsInvalidStartWithoutPartialState",
    ck_hang_monitor_tests::kTestFlags)

auto
    FCkHangMonitor_RejectsInvalidStartWithoutPartialState::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_hang_monitor_tests;

    auto Controller = FCkHangMonitorController{};
    Controller.Initialize();
    ON_SCOPE_EXIT
    {
        Controller.Shutdown();
    };

    Test_IsStoppedWithoutPartialState(*this, Controller.Get_Snapshot(), TEXT("fresh controller"));

    for (const auto InvalidCount : {0, 4})
    {
        auto Error = FString{};
        TestFalse(
            *FString::Printf(TEXT("invalid dump bound [%d] is rejected"), InvalidCount),
            Controller.TryStart(Get_MissingProcDumpPath(), InvalidCount, Error));
        TestFalse(
            *FString::Printf(TEXT("invalid dump bound [%d] reports its error"), InvalidCount),
            Error.IsEmpty());
        Test_IsStoppedWithoutPartialState(
            *this,
            Controller.Get_Snapshot(),
            *FString::Printf(TEXT("invalid dump bound [%d]"), InvalidCount));
    }

    auto MissingToolError = FString{};
    TestFalse(
        TEXT("missing ProcDump tool is rejected"),
        Controller.TryStart(Get_MissingProcDumpPath(), 1, MissingToolError));
    TestFalse(TEXT("missing ProcDump tool reports its error"), MissingToolError.IsEmpty());
    Test_IsStoppedWithoutPartialState(*this, Controller.Get_Snapshot(), TEXT("missing ProcDump tool"));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_RejectsStaleStopWithoutPartialState,
    "Ck.DebuggerLauncher.HangMonitor.RejectsStaleStopWithoutPartialState",
    ck_hang_monitor_tests::kTestFlags)

auto
    FCkHangMonitor_RejectsStaleStopWithoutPartialState::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_hang_monitor_tests;

    auto Controller = FCkHangMonitorController{};
    Controller.Initialize();
    ON_SCOPE_EXIT
    {
        Controller.Shutdown();
    };

    auto Error = FString{};
    TestFalse(TEXT("stopping an inactive monitor is rejected"), Controller.TryStop(Error));
    TestFalse(TEXT("stale-stop rejection explains why"), Error.IsEmpty());
    Test_IsStoppedWithoutPartialState(*this, Controller.Get_Snapshot(), TEXT("stale stop"));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_OptInCurrentProcessArmAndStop,
    "Ck.DebuggerLauncher.HangMonitor.OptInCurrentProcessArmAndStop",
    ck_hang_monitor_tests::kTestFlags | EAutomationTestFlags::NonNullRHI)

auto
    FCkHangMonitor_OptInCurrentProcessArmAndStop::
    RunTest(
        const FString&)
    -> bool
{
    const auto ProcDumpPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CK_HANG_MONITOR_PROCDUMP"));
    if (ProcDumpPath.IsEmpty())
    {
        AddWarning(TEXT("NOT EXERCISED: CK_HANG_MONITOR_PROCDUMP is unavailable; real ProcDump arm/stop verification was not run."));
        return true;
    }

    if (!IFileManager::Get().FileExists(*ProcDumpPath))
    {
        AddError(FString::Printf(
            TEXT("CK_HANG_MONITOR_PROCDUMP was set but is not an existing file: %s"),
            *ProcDumpPath));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(
        ck_hang_monitor_tests::FCk_Latent_HangMonitorArmStop(this, ProcDumpPath));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_BoundedIncompleteShutdown,
    "Ck.TenetIntegrity.HangMonitor.BoundedIncompleteShutdown",
    ck_hang_monitor_tests::kTestFlags)

auto
    FCkHangMonitor_BoundedIncompleteShutdown::
    RunTest(
        const FString&)
    -> bool
{
    for (const auto Ready : {false, true})
    {
        auto Controller = FCkHangMonitorController{};
        Controller.Initialize();
        FCkHangMonitorTestAccess::AdoptRunningHost(Controller, Ready);
        TestTrue(TEXT("deadline seam owns a real handle to a running process"),
            FCkHangMonitorTestAccess::HasPendingProcesses(Controller));
        const auto Started = FPlatformTime::Seconds();
        Controller.Shutdown();
        TestTrue(TEXT("zero-budget seam returns promptly"), FPlatformTime::Seconds() - Started < 0.5);
        const auto Incomplete = Controller.Get_Snapshot();
        TestTrue(TEXT("live child produces explicit incomplete cleanup"), Incomplete.Get_CleanupIncomplete());
        TestTrue(TEXT("incomplete shutdown retains process observation"),
            FCkHangMonitorTestAccess::HasPendingProcesses(Controller));
        TestTrue(TEXT("incomplete shutdown retains active state"), Incomplete.IsMonitoring);
        TestTrue(TEXT("incomplete shutdown retains stop intent"), Incomplete.IsStopping);
        if (Ready)
        {
            TestFalse(TEXT("cancel launch failure is explicit"), Incomplete.LastError.IsEmpty());
            auto Error = FString{};
            TestFalse(TEXT("failed cancellation cannot silently retry"), Controller.TryStop(Error));
            TestEqual(TEXT("failed cancellation preserves error"), Error, Incomplete.LastError);
        }
        Controller.Initialize();
        auto Error = FString{};
        TestFalse(TEXT("reinitialize cannot permit overlapping rearm"),
            Controller.TryStart(FCkHangMonitorController::Get_DefaultProcDumpPath(), 1, Error));
        Controller.Shutdown();
        TestEqual(TEXT("repeated shutdown preserves failure"), Controller.Get_Snapshot().LastError, Incomplete.LastError);
        TestTrue(TEXT("repeated shutdown preserves unconfirmed exit"), Controller.Get_Snapshot().Get_CleanupIncomplete());
        TestTrue(TEXT("test host remains alive"), FPlatformProcess::IsApplicationRunning(FPlatformProcess::GetCurrentProcessId()));
    }
    auto Controller = FCkHangMonitorController{};
    Controller.Initialize();
    FCkHangMonitorTestAccess::AdoptRunningHelper(Controller);
    Controller.Shutdown();
    TestFalse(TEXT("helper-only pending state does not invent a live worker"), Controller.Get_Snapshot().IsMonitoring);
    TestTrue(TEXT("helper remains observed after worker exit"), FCkHangMonitorTestAccess::HasPendingProcesses(Controller));
    TestTrue(TEXT("helper-only timeout reports incomplete cleanup"), Controller.Get_Snapshot().Get_CleanupIncomplete());
    TestTrue(TEXT("helper-only timeout preserves stopping state"), Controller.Get_Snapshot().IsStopping);
    Controller.Initialize();
    auto Error = FString{};
    TestFalse(TEXT("helper-only pending state prohibits rearm"),
        Controller.TryStart(FCkHangMonitorController::Get_DefaultProcDumpPath(), 1, Error));
    return true;
}

namespace ck_hang_monitor_tests
{
    class FCk_Latent_HangMonitorShutdown final : public IAutomationLatentCommand
    {
    public:
        FCk_Latent_HangMonitorShutdown(
            FAutomationTestBase* InTest,
            FString InPath)
            : _Test(InTest), _Path(MoveTemp(InPath))
        {}

        ~FCk_Latent_HangMonitorShutdown() override
        {
            _Controller.Shutdown();
            FCkHangMonitorTestAccess::ObserveCancellation(_Controller, {});
            FPlatformProcess::CloseProc(_WorkerObservation);
            FPlatformProcess::CloseProc(_HelperObservation);
        }

        auto
        Update()
        -> bool override
        {
            if (!_Started)
            {
                _Controller.Initialize();
                auto Error = FString{};
                if (!_Controller.TryStart(_Path, 1, Error))
                {
                    _Test->AddError(FString::Printf(TEXT("ProcDump shutdown test could not start: %s"), *Error));
                    return true;
                }
                _WorkerObservation = FPlatformProcess::OpenProcess(_Controller.Get_Snapshot().Get_MonitorPid());
                _Test->TestTrue(TEXT("independent worker observation opened"), _WorkerObservation.IsValid());
                FCkHangMonitorTestAccess::ObserveCancellation(_Controller, [this](const uint32 InPid)
                {
                    ++_CancelLaunches;
                    _HelperObservation = FPlatformProcess::OpenProcess(InPid);
                    _Test->TestTrue(TEXT("independent helper observation opened"), _HelperObservation.IsValid());
                });
                _Started = true;
                _Deadline = FPlatformTime::Seconds() + FCk_Time{20.0}.Get_Seconds();
                if (_WaitForReady)
                { return false; }
            }

            if (_WaitForReady)
            {
                FCkHangMonitorTestAccess::Service(_Controller);
                if (!_Controller.Get_Snapshot().IsReady)
                {
                    if (FPlatformTime::Seconds() < _Deadline)
                    { return false; }
                    _Test->AddError(TEXT("ProcDump did not attach before the ready-shutdown test deadline."));
                    return true;
                }
            }

            _Controller.Shutdown();
            const auto Stopped = _Controller.Get_Snapshot();
            _Test->TestFalse(TEXT("real shutdown completes within approved budget"), Stopped.Get_CleanupIncomplete());
            _Test->TestFalse(TEXT("worker no longer reported monitoring"), Stopped.IsMonitoring);
            _Test->TestFalse(TEXT("helper no longer reported pending"), Stopped.IsStopping);
            _Test->TestTrue(TEXT("real shutdown reports no operational error"), Stopped.LastError.IsEmpty());
            _Test->TestEqual(TEXT("cancellation launches exactly once"), _CancelLaunches, 1);
            _Test->TestTrue(TEXT("worker exit independently observed"),
                _WorkerObservation.IsValid() && !FPlatformProcess::IsProcRunning(_WorkerObservation));
            _Test->TestTrue(TEXT("helper exit independently observed"),
                _HelperObservation.IsValid() && !FPlatformProcess::IsProcRunning(_HelperObservation));
            const auto RepeatedStart = FPlatformTime::Seconds();
            _Controller.Shutdown();
            _Test->TestTrue(TEXT("repeated shutdown does not wait another budget"), FPlatformTime::Seconds() - RepeatedStart < 0.5);
            _Test->TestEqual(TEXT("repeated shutdown never relaunches cancellation"), _CancelLaunches, 1);
            if (!_WaitForReady && !Stopped.Get_CleanupIncomplete())
            {
                FPlatformProcess::CloseProc(_WorkerObservation);
                FPlatformProcess::CloseProc(_HelperObservation);
                _WaitForReady = true;
                _Started = false;
                _CancelLaunches = 0;
                return false;
            }
            return true;
        }

    private:
        FAutomationTestBase* _Test = nullptr;
        FString _Path;
        bool _WaitForReady = false;
        bool _Started = false;
        double _Deadline = 0.0;
        int32 _CancelLaunches = 0;
        FCkHangMonitorController _Controller;
        FProcHandle _WorkerObservation;
        FProcHandle _HelperObservation;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_OptInShutdown,
    "Ck.TenetIntegrity.HangMonitor.OptInImmediateAndReadyShutdown",
    ck_hang_monitor_tests::kTestFlags | EAutomationTestFlags::NonNullRHI)

auto
    FCkHangMonitor_OptInShutdown::
    RunTest(
        const FString&)
    -> bool
{
    const auto Path = FPlatformMisc::GetEnvironmentVariable(TEXT("CK_HANG_MONITOR_PROCDUMP"));
    if (Path.IsEmpty())
    {
        AddWarning(TEXT("NOT EXERCISED: real ProcDump immediate/ready shutdown requires CK_HANG_MONITOR_PROCDUMP."));
        return true;
    }
    if (!IFileManager::Get().FileExists(*Path))
    {
        AddError(TEXT("CK_HANG_MONITOR_PROCDUMP does not name an existing executable."));
        return false;
    }
    const auto ModuleSnapshot = FCkHangMonitorModule::Get().Get_Controller().Get_Snapshot();
    if (ModuleSnapshot.IsMonitoring || ModuleSnapshot.IsStopping)
    {
        AddError(TEXT("Real ProcDump shutdown tests require an inactive module controller."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(ck_hang_monitor_tests::FCk_Latent_HangMonitorShutdown(this, Path));
    return true;
}

#endif
