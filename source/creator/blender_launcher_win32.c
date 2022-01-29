/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <Windows.h>
#include <strsafe.h>

#include <PathCch.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
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
                       pCmdLine) != S_OK) {
    free(buffer);
    return -1;
  }

  BOOL success = CreateProcess(
      path, buffer, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &siStartInfo, &procInfo);

  DWORD returnValue = success ? 0 : -1;

  if (success) {
    /* If blender-launcher is called with background command line flag,
     * wait for the blender process to exit and return its return value. */
    BOOL background = FALSE;
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
