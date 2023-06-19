/* SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef _WIN32
#  include <windows.h>
#endif

#include "util/windows.h"

CCL_NAMESPACE_BEGIN

bool system_windows_version_at_least(int major, int build)
{
#ifdef _WIN32
  HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
  if (hMod == 0) {
    return false;
  }

  typedef NTSTATUS(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
  RtlGetVersionPtr rtl_get_version = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
  if (rtl_get_version == NULL) {
    return false;
  }

  RTL_OSVERSIONINFOW rovi = {0};
  rovi.dwOSVersionInfoSize = sizeof(rovi);
  if (rtl_get_version(&rovi) != 0) {
    return false;
  }

  return (rovi.dwMajorVersion > major ||
          (rovi.dwMajorVersion == major && rovi.dwBuildNumber >= build));
#else
  (void)major;
  (void)build;
  return false;
#endif
}

CCL_NAMESPACE_END
