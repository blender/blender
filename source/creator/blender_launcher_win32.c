/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <Windows.h>
#include <strsafe.h>

#include <PathCch.h>
#include <tlhelp32.h>

BOOL LaunchedFromSteam()
{
  HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  BOOL isSteam = FALSE;
  if (!hSnapShot)
    return (FALSE);

  PROCESSENTRY32 process_entry;
  process_entry.dwSize = sizeof(PROCESSENTRY32);

  if (!Process32First(hSnapShot, &process_entry)) {
    CloseHandle(hSnapShot);
    return (FALSE);
  }

  /* First find our parent process ID. */
  DWORD our_pid = GetCurrentProcessId();
  DWORD parent_pid = -1;

  do {
    if (process_entry.th32ProcessID == our_pid) {
      parent_pid = process_entry.th32ParentProcessID;
      break;
    }
  } while (Process32Next(hSnapShot, &process_entry));

  if (parent_pid == -1 || !Process32First(hSnapShot, &process_entry)) {
    CloseHandle(hSnapShot);
    return (FALSE);
  }
  /* Then do another loop to find the process name of the parent.
   * this is done in 2 loops, since the order of the processes is
   * unknown and we may already have passed the parent process by
   * the time we figure out its pid in the first loop. */
  do {
    if (process_entry.th32ProcessID == parent_pid) {
      if (_wcsicmp(process_entry.szExeFile, L"steam.exe") == 0) {
        isSteam = TRUE;
      }
      break;
    }
  } while (Process32Next(hSnapShot, &process_entry));

  CloseHandle(hSnapShot);
  return isSteam;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  /* Silence unreferenced formal parameter warning. */
  (void)hInstance;
  (void)hPrevInstance;
  (void)nCmdShow;

  STARTUPINFO siStartInfo = {0};
  PROCESS_INFORMATION procInfo;
  wchar_t path[MAX_PATH];

  siStartInfo.wShowWindow = SW_HIDE;
  siStartInfo.dwFlags = STARTF_USESHOWWINDOW;

  /* Get the path to the currently running executable (blender-launcher.exe) */

  DWORD nSize = GetModuleFileName(NULL, path, MAX_PATH);
  if (!nSize) {
    return -1;
  }

  /* GetModuleFileName returns the number of characters written, but GetLastError needs to be
   * called to see if it ran out of space or not. However where would we be without exceptions
   * to the rule: "If the buffer is too small to hold the module name, the function returns nSize.
   * The last error code remains ERROR_SUCCESS." - source: MSDN. */

  if (GetLastError() == ERROR_SUCCESS && nSize == MAX_PATH) {
    return -1;
  }

  /* Remove the filename (blender-launcher.exe) from path. */
  if (PathCchRemoveFileSpec(path, MAX_PATH) != S_OK) {
    return -1;
  }

  /* Add blender.exe to path, resulting in the full path to the blender executable. */
  if (PathCchCombine(path, MAX_PATH, path, L"blender.exe") != S_OK) {
    return -1;
  }

  int required_size_chars = lstrlenW(path) +     /* Module name */
                            3 +                  /* 2 quotes + Space */
                            lstrlenW(pCmdLine) + /* Original command line */
                            1;                   /* Zero terminator */
  size_t required_size_bytes = required_size_chars * sizeof(wchar_t);
  wchar_t *buffer = (wchar_t *)malloc(required_size_bytes);
  if (!buffer) {
    return -1;
  }

  if (StringCbPrintfEx(buffer,
                       required_size_bytes,
                       NULL,
                       NULL,
                       STRSAFE_NULL_ON_FAILURE,
                       L"\"%s\" %s",
                       path,
                       pCmdLine) != S_OK)
  {
    free(buffer);
    return -1;
  }

  BOOL success = CreateProcess(
      path, buffer, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &siStartInfo, &procInfo);

  DWORD returnValue = success ? 0 : -1;

  if (success) {
    /* If blender-launcher is called with background command line flag or launched from steam,
     * wait for the blender process to exit and return its return value. */
    BOOL background = LaunchedFromSteam();
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(pCmdLine, &argc);
    for (int i = 0; i < argc; i++) {
      if ((wcscmp(argv[i], L"-b") == 0) || (wcscmp(argv[i], L"--background") == 0)) {
        background = TRUE;
        break;
      }
    }

    if (background) {
      WaitForSingleObject(procInfo.hProcess, INFINITE);
      GetExitCodeProcess(procInfo.hProcess, &returnValue);
    }

    /* Handles in PROCESS_INFORMATION must be closed with CloseHandle when they are no longer
     * needed - MSDN. Closing the handles will NOT terminate the thread/process that we just
     * started. */
    CloseHandle(procInfo.hThread);
    CloseHandle(procInfo.hProcess);
  }

  free(buffer);
  return returnValue;
}
