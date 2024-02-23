/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/system.h"

#include "util/log.h"
#include "util/string.h"
#include "util/types.h"

#include <OpenImageIO/sysutil.h>

OIIO_NAMESPACE_USING

#ifdef _WIN32
#  if (!defined(FREE_WINDOWS))
#    include <intrin.h>
#  endif
#  include "util/windows.h"
#elif defined(__APPLE__)
#  include <sys/ioctl.h>
#  include <sys/sysctl.h>
#  include <sys/types.h>
#  include <unistd.h>
#else
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

CCL_NAMESPACE_BEGIN

int system_console_width()
{
  int columns = 0;

#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    columns = csbi.dwSize.X;
  }
#else
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    columns = w.ws_col;
  }
#endif

  return (columns > 0) ? columns : 80;
}

/* Equivalent of Windows __cpuid for x86 processors on other platforms. */
#if (!defined(_WIN32) || defined(FREE_WINDOWS)) && (defined(__x86_64__) || defined(__i386__))
static void __cpuid(int data[4], int selector)
{
#  if defined(__x86_64__)
  asm("cpuid" : "=a"(data[0]), "=b"(data[1]), "=c"(data[2]), "=d"(data[3]) : "a"(selector));
#  elif defined(__i386__)
  asm("pushl %%ebx    \n\t"
      "cpuid          \n\t"
      "movl %%ebx, %1 \n\t"
      "popl %%ebx     \n\t"
      : "=a"(data[0]), "=r"(data[1]), "=c"(data[2]), "=d"(data[3])
      : "a"(selector)
      : "ebx");
#  else
  data[0] = data[1] = data[2] = data[3] = 0;
#  endif
}
#endif

string system_cpu_brand_string()
{
#if defined(__APPLE__)
  /* Get from system on macOS. */
  char modelname[512] = "";
  size_t bufferlen = 512;
  if (sysctlbyname("machdep.cpu.brand_string", &modelname, &bufferlen, NULL, 0) == 0) {
    return modelname;
  }
#elif defined(WIN32) || defined(__x86_64__) || defined(__i386__)
  /* Get from intrinsics on Windows and x86. */
  char buf[49] = {0};
  int result[4] = {0};

  __cpuid(result, 0x80000000);

  if (result[0] != 0 && result[0] >= (int)0x80000004) {
    __cpuid((int *)(buf + 0), 0x80000002);
    __cpuid((int *)(buf + 16), 0x80000003);
    __cpuid((int *)(buf + 32), 0x80000004);

    string brand = buf;

    /* Make it a bit more presentable. */
    brand = string_remove_trademark(brand);

    return brand;
  }
#else
  /* Get from /proc/cpuinfo on Unix systems. */
  FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
  if (cpuinfo != nullptr) {
    char cpuinfo_buf[513] = "";
    fread(cpuinfo_buf, sizeof(cpuinfo_buf) - 1, 1, cpuinfo);
    fclose(cpuinfo);

    char *modelname = strstr(cpuinfo_buf, "model name");
    if (modelname != nullptr) {
      modelname = strchr(modelname, ':');
      if (modelname != nullptr) {
        modelname += 2;
        char *modelname_end = strchr(modelname, '\n');
        if (modelname_end != nullptr) {
          *modelname_end = '\0';
          return modelname;
        }
      }
    }
  }
#endif
  return "Unknown CPU";
}

int system_cpu_bits()
{
  return (sizeof(void *) * 8);
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

struct CPUCapabilities {
  bool sse2;
  bool sse42;
  bool avx2;
};

static CPUCapabilities &system_cpu_capabilities()
{
  static CPUCapabilities caps = {};
  static bool caps_init = false;

  if (!caps_init) {
    int result[4], num;

    __cpuid(result, 0);
    num = result[0];

    if (num >= 1) {
      __cpuid(result, 0x00000001);
      const bool sse = (result[3] & ((int)1 << 25)) != 0;
      const bool sse2 = (result[3] & ((int)1 << 26)) != 0;
      const bool sse3 = (result[2] & ((int)1 << 0)) != 0;

      const bool ssse3 = (result[2] & ((int)1 << 9)) != 0;
      const bool sse41 = (result[2] & ((int)1 << 19)) != 0;
      const bool sse42 = (result[2] & ((int)1 << 20)) != 0;

      const bool fma3 = (result[2] & ((int)1 << 12)) != 0;
      const bool os_uses_xsave_xrestore = (result[2] & ((int)1 << 27)) != 0;
      const bool cpu_avx_support = (result[2] & ((int)1 << 28)) != 0;

      /* Simplify to combined capabilities for which we specialize kernels. */
      caps.sse2 = sse && sse2;
      caps.sse42 = sse && sse2 && sse3 && ssse3 && sse41 && sse42;

      if (os_uses_xsave_xrestore && cpu_avx_support) {
        // Check if the OS will save the YMM registers
        uint32_t xcr_feature_mask;
#  if defined(__GNUC__)
        int edx; /* not used */
        /* actual opcode for xgetbv */
        __asm__(".byte 0x0f, 0x01, 0xd0" : "=a"(xcr_feature_mask), "=d"(edx) : "c"(0));
#  elif defined(_MSC_VER) && defined(_XCR_XFEATURE_ENABLED_MASK)
        /* Minimum VS2010 SP1 compiler is required. */
        xcr_feature_mask = (uint32_t)_xgetbv(_XCR_XFEATURE_ENABLED_MASK);
#  else
        xcr_feature_mask = 0;
#  endif
        const bool avx = (xcr_feature_mask & 0x6) == 0x6;
        const bool f16c = (result[2] & ((int)1 << 29)) != 0;

        __cpuid(result, 0x00000007);
        bool bmi1 = (result[1] & ((int)1 << 3)) != 0;
        bool bmi2 = (result[1] & ((int)1 << 8)) != 0;
        bool avx2 = (result[1] & ((int)1 << 5)) != 0;

        caps.avx2 = sse && sse2 && sse3 && ssse3 && sse41 && sse42 && avx && f16c && avx2 &&
                    fma3 && bmi1 && bmi2;
      }
    }

    caps_init = true;
  }

  return caps;
}

bool system_cpu_support_sse2()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.sse2;
}

bool system_cpu_support_sse42()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.sse42;
}

bool system_cpu_support_avx2()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.avx2;
}
#else

bool system_cpu_support_sse2()
{
  return false;
}

bool system_cpu_support_sse42()
{
  return false;
}

bool system_cpu_support_avx2()
{
  return false;
}

#endif

size_t system_physical_ram()
{
#ifdef _WIN32
  MEMORYSTATUSEX ram;
  ram.dwLength = sizeof(ram);
  GlobalMemoryStatusEx(&ram);
  return ram.ullTotalPhys;
#elif defined(__APPLE__)
  uint64_t ram = 0;
  size_t len = sizeof(ram);
  if (sysctlbyname("hw.memsize", &ram, &len, NULL, 0) == 0) {
    return ram;
  }
  return 0;
#else
  size_t ps = sysconf(_SC_PAGESIZE);
  size_t pn = sysconf(_SC_PHYS_PAGES);
  return ps * pn;
#endif
}

uint64_t system_self_process_id()
{
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

CCL_NAMESPACE_END
