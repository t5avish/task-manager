# task-manager

A lightweight Windows task manager built with Win32 API that lets you view running processes and terminate them.

The application fetches live process data using the Windows Toolhelp32 snapshot API, displays them in a sortable list view, and supports both graceful (WM_CLOSE) and forced (TerminateProcess) termination.

## How it works

Process enumeration runs on a dedicated background worker thread to keep the UI responsive. The worker waits on a Windows Event object - either waking up on a timer (auto-refresh) or immediately when signaled (manual refresh or after ending a task). Once it fetches a fresh snapshot, it stores it in a shared buffer and posts a message to the main window, which then updates the list view on the UI thread. This means the UI never blocks on process enumeration regardless of system load.