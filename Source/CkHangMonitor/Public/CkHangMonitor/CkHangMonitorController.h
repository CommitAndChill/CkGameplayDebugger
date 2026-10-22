#pragma once

#include "CoreMinimal.h"
#include "CkCore/Time/CkTime.h"

#include <Containers/Ticker.h>
#include <HAL/PlatformProcess.h>

// --------------------------------------------------------------------------------------------------------------------

struct CKHANGMONITOR_API FCkHangMonitorSnapshot
{
    CK_GENERATED_BODY(FCkHangMonitorSnapshot);
    friend class FCkHangMonitorController;

    bool    IsMonitoring = false;
    bool    IsReady = false;
    bool    IsStopping = false;
    FString Status;
    FString LastError;
    FString OutputDirectory;
    FString ProcDumpPath;
    uint32  TargetPid = 0;
    int32   NumDumps = 0;
    int32   MaxDumps = 1;

private:
    bool _CleanupIncomplete = false;
    uint32 _MonitorPid = 0;
    uint32 _CancelPid = 0;

public:
    CK_PROPERTY_GET(_CleanupIncomplete);
    CK_PROPERTY_GET(_MonitorPid);
    CK_PROPERTY_GET(_CancelPid);
};

// --------------------------------------------------------------------------------------------------------------------

class CKHANGMONITOR_API FCkHangMonitorController
{
public:
    ~FCkHangMonitorController();
    FCkHangMonitorController() = default;
    FCkHangMonitorController(const FCkHangMonitorController&) = delete;
    FCkHangMonitorController& operator=(const FCkHangMonitorController&) = delete;
    auto
    Initialize()
    -> void;
    auto
    Shutdown()
    -> void;

    auto
    Get_Snapshot() const
    -> FCkHangMonitorSnapshot;

    auto
    TryStart(
        const FString& InProcDumpPath,
        int32 InMaxDumps,
        FString& OutError)
    -> bool;

    auto
    TryStop(
        FString& OutError)
    -> bool;

    static auto
    Get_DefaultProcDumpPath()
    -> FString;

private:
    friend class FCkHangMonitorTestAccess;
    auto
    DoTick(
        float InDeltaSeconds)
    -> bool;
    auto
    DoService_Processes()
    -> void;
    auto
    Get_HasPendingProcesses() const
    -> bool;
    auto
    DoTryObserve_ProcessExit(
        FProcHandle& InOutProcess,
        int32& OutReturnCode)
    -> bool;
    auto
    DoClose_Process(
        FProcHandle& InOutProcess)
    -> void;
    auto
    DoClose_OutputPipes()
    -> void;
    auto
    DoRefresh_DumpCount()
    -> void;
    auto
    DoWrite_Manifest() const
    -> void;
    auto
    DoWrite_OutputTail() const
    -> void;

    FCkHangMonitorSnapshot _Snapshot;
    FProcHandle             _MonitorProcess;
    FProcHandle             _CancelProcess;
    void*                   _StdOutPipeRead = nullptr;
    void*                   _StdErrPipeRead = nullptr;
    TArray<uint8>           _StdOutTail;
    FString                 _StdErrTail;
    FString                 _StartedUtc;
    bool                    _CancelIssued = false;
    bool                    _ShutdownRequested = false;
    FCk_Time                _ShutdownBudget{2.0};
#if WITH_DEV_AUTOMATION_TESTS
    TFunction<void(uint32)> _OnCancelStarted;
    bool _RejectCancelLaunchForTest = false;
#endif
    FTSTicker::FDelegateHandle _TickerHandle;
};

// --------------------------------------------------------------------------------------------------------------------
