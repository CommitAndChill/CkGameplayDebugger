# CkHangMonitor

CkHangMonitor is a standalone DeveloperTool tab for one current-game external
ProcDump worker. It detects a window that remains unresponsive for more than
five seconds and captures at most the configured 1–3 dumps. Its controller
survives tab closure so an armed capture remains active; module shutdown and
engine pre-exit service graceful cancellation for up to two seconds. Both the
worker and cancellation helper must be observed exited before cleanup is complete.
An incomplete explicit shutdown retains observations and rejects rearm; final
destruction records incomplete cleanup before releasing handles without killing
either child. Active runtime unload is unsupported: stop successfully before
unload. Module-manager unload can destroy the owner despite unfinished cleanup.

ProcDump is a local prerequisite selected by the user. Do not stage, download,
or add it as a runtime dependency. The UI uses shared debugger chrome, section
headers, status pill, numeric editor and selectable labels; all paths and PIDs
remain copyable.

The controller owns process lifetime and must reject any target other than the
current game process. UI actions only call the controller's start and stop
methods; never launch a second worker from Slate state.
