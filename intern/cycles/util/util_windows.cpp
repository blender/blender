/*
 * Copyright 2019-2019 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef _WIN32
#include <windows.h>
#endif

#include "util_windows.h"

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
