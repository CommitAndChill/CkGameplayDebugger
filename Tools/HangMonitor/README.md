# Hang Monitor

Hang Monitor is a Windows development debugger that arms Microsoft's external ProcDump utility against the current game or editor process. Its capture worker continues while the game thread and debugger UI are stalled. Closing the tab leaves monitoring active; Stop cancels monitoring without terminating the game.

## Setup

Run once on the machine used for testing:

```powershell
& 'D:\Repos\BusterBlock\Plugins\CkGameplayDebugger\Tools\HangMonitor\Install-ProcDump.ps1' -AcceptEula
```

The script downloads the official Microsoft distribution, verifies its Microsoft signature, and installs `procdump64.exe` in `%LOCALAPPDATA%\CkDiagnostics\ProcDump`. `-AcceptEula` accepts Microsoft's ProcDump license. Omit it to install and review the included `Eula.txt` first. Another machine needs its own installation.

## Capture a stall

1. Package **Development Win64** and retain the exact executable and matching PDB symbols. The tool is excluded from Shipping and Test configurations, and ProcDump is not bundled or staged.
2. Launch the packaged game normally. Open **Hang Monitor** from the debugger suite or enter `ck.HangMonitor` in the console.
3. Leave the dump budget at **1** initially. Choose `procdump64.exe` with Browse if it is installed elsewhere.
4. Click **Arm** and wait for the monitoring status. Close the debugger UI if desired, then play the heavy level normally.
5. When a game window stops responding to Windows messages for at least five seconds, ProcDump writes a mini dump. Let the game recover if it can. Reopen Hang Monitor and use **Open output folder**.
6. Keep the entire session folder under the game's `Saved\Diagnostics\HangCaptures`, its game log, and the matching executable/PDBs. The dump contains thread stacks for investigating the blocking operation. Nothing is uploaded automatically.

The trigger measures an unresponsive window, not a frame-time threshold. Loading screens or a debugger pause can also trigger it. Capture itself briefly pauses the target. If the game remains responsive to window messages during a stall, this trigger may not fire. Each arming is bounded to 1–3 mini dumps; rearm for another session. Stop uses ProcDump's PID cancellation command, which cancels any other ProcDump monitor attached to that same PID too.

This tool records evidence; it does not generate stress or establish the cause of the reported Shipping stalls. A Development reproduction is still required. Preserve symbols from that exact build for useful stack analysis.

Engine exit and module shutdown make one best-effort graceful cancellation attempt with a two-second service budget. An early Stop waits for ProcDump's attachment banner before issuing cancellation. Completion requires observing both the worker and the cancellation helper exit. No child is forcibly terminated. If either exit remains unconfirmed at the deadline, the manifest records `state=cleanup-incomplete` with the child PIDs and unconfirmed-exit fields; engine exit may continue while a child remains alive. Repeated shutdown calls do not start another wait budget or retry a failed cancellation command. While the controller remains alive it retains unresolved handles and rejects rearm; final destruction records the unresolved state and releases observations.

Active runtime module unload is unsupported. Successfully Stop the monitor and wait for both child processes to exit before requesting unload. The engine's direct unload and abandonment paths can destroy the module regardless of pending cleanup; shutdown cannot veto those paths or guarantee child cleanup within its budget.

The `Ck.TenetIntegrity.HangMonitor` tests include deterministic deadline and cancellation-launch-failure coverage. Real ProcDump shutdown coverage additionally requires `CK_HANG_MONITOR_PROCDUMP` to name an existing local `procdump64.exe`; an unset variable is reported explicitly as **NOT EXERCISED**. Those tests independently observe worker and cancellation-helper process handles, including immediate shutdown before any controller tick and shutdown after attachment.

See [Microsoft's ProcDump documentation](https://learn.microsoft.com/en-us/sysinternals/downloads/procdump) for the `-h`, `-mm`, `-n`, and `-cancel` options.
