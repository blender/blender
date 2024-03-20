/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup creator
 */
#include <string>

#if defined(WIN32)
#  include <Windows.h>
#  include <intrin.h>
#endif

/* The code below is duplicated from system.c from bf_blenlib. This is on purpose, since bf_blenlib
 * may be build with CPU flags that are not available on the current cpu so we can't link it. */

#if !defined(_WIN32)
static void __cpuid(
    /* Cannot be const, because it is modified below.
     * NOLINTNEXTLINE: readability-non-const-parameter. */
    int data[4],
    int selector)
{
#  if defined(__x86_64__)
  asm("cpuid" : "=a"(data[0]), "=b"(data[1]), "=c"(data[2]), "=d"(data[3]) : "a"(selector));
#  else
  (void)selector;
  data[0] = data[1] = data[2] = data[3] = 0;
#  endif
}
#endif

static int cpu_supports_sse42()
{
  int result[4], num;
  __cpuid(result, 0);
  num = result[0];

  if (num >= 1) {
    __cpuid(result, 0x00000001);
    return (result[2] & (int(1) << 20)) != 0;
  }
  return 0;
}

static const char *cpu_brand_string()
{
  static char buf[49] = {0};
  int result[4] = {0};
  __cpuid(result, 0x80000000);
  if (result[0] >= int(0x80000004)) {
    __cpuid((int *)(buf + 0), 0x80000002);
    __cpuid((int *)(buf + 16), 0x80000003);
    __cpuid((int *)(buf + 32), 0x80000004);
    const char *buf_ptr = buf;
    // Trim any leading spaces.
    while (*buf_ptr == ' ') {
      buf_ptr++;
    }
    return buf_ptr;
  }
  return nullptr;
}

#ifdef _MSC_VER
extern "C" __declspec(dllexport) void cpu_check_win32()
{
#  ifdef _M_X64
  if (!cpu_supports_sse42()) {
    std::string error_title = "Unsupported CPU - " + std::string(cpu_brand_string());
    MessageBoxA(NULL,
                "Blender requires a CPU with SSE42 support.",
                error_title.c_str(),
                MB_OK | MB_ICONERROR);
    exit(-1);
  }
#  endif
}

BOOL WINAPI DllMain(HINSTANCE /*hinstDLL*/, DWORD fdwReason, LPVOID /*lpvReserved*/)
{
  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
      cpu_check_win32();
      break;
  }
  return TRUE;
}
#else
#  include <cstdio>
#  include <cstdlib>

static __attribute__((constructor)) void cpu_check()
{
#  ifdef __x86_64
  if (!cpu_supports_sse42()) {
    std::string error = "Unsupported CPU - " + std::string(cpu_brand_string()) +
                        "\nBlender requires a CPU with SSE42 support.";
    printf("%s\n", error.c_str());
    exit(-1);
  }
  return;
#  endif
}
#endif
