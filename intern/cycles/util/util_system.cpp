/*
 * Copyright 2011-2013 Blender Foundation
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

#include "util/util_system.h"

#include "util/util_logging.h"
#include "util/util_string.h"
#include "util/util_types.h"

#include <numaapi.h>

#include <OpenImageIO/sysutil.h>
OIIO_NAMESPACE_USING

#ifdef _WIN32
#  if (!defined(FREE_WINDOWS))
#    include <intrin.h>
#  endif
#  include "util_windows.h"
#elif defined(__APPLE__)
#  include <sys/ioctl.h>
#  include <sys/sysctl.h>
#  include <sys/types.h>
#else
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

CCL_NAMESPACE_BEGIN

bool system_cpu_ensure_initialized()
{
  static bool is_initialized = false;
  static bool result = false;
  if (is_initialized) {
    return result;
  }
  is_initialized = true;
  const NUMAAPI_Result numa_result = numaAPI_Initialize();
  result = (numa_result == NUMAAPI_SUCCESS);
  return result;
}

/* Fallback solution, which doesn't use NUMA/CPU groups. */
static int system_cpu_thread_count_fallback()
{
#ifdef _WIN32
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
#elif defined(__APPLE__)
  int count;
  size_t len = sizeof(count);
  int mib[2] = {CTL_HW, HW_NCPU};
  sysctl(mib, 2, &count, &len, NULL, 0);
  return count;
#else
  return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

int system_cpu_thread_count()
{
  const int num_nodes = system_cpu_num_numa_nodes();
  int num_threads = 0;
  for (int node = 0; node < num_nodes; ++node) {
    if (!system_cpu_is_numa_node_available(node)) {
      continue;
    }
    num_threads += system_cpu_num_numa_node_processors(node);
  }
  return num_threads;
}

int system_cpu_num_numa_nodes()
{
  if (!system_cpu_ensure_initialized()) {
    /* Fallback to a single node with all the threads. */
    return 1;
  }
  return numaAPI_GetNumNodes();
}

bool system_cpu_is_numa_node_available(int node)
{
  if (!system_cpu_ensure_initialized()) {
    return true;
  }
  return numaAPI_IsNodeAvailable(node);
}

int system_cpu_num_numa_node_processors(int node)
{
  if (!system_cpu_ensure_initialized()) {
    return system_cpu_thread_count_fallback();
  }
  return numaAPI_GetNumNodeProcessors(node);
}

bool system_cpu_run_thread_on_node(int node)
{
  if (!system_cpu_ensure_initialized()) {
    return true;
  }
  return numaAPI_RunThreadOnNode(node);
}

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

int system_cpu_num_active_group_processors()
{
  if (!system_cpu_ensure_initialized()) {
    return system_cpu_thread_count_fallback();
  }
  return numaAPI_GetNumCurrentNodesProcessors();
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
  bool x64;
  bool mmx;
  bool sse;
  bool sse2;
  bool sse3;
  bool ssse3;
  bool sse41;
  bool sse42;
  bool sse4a;
  bool avx;
  bool f16c;
  bool avx2;
  bool xop;
  bool fma3;
  bool fma4;
  bool bmi1;
  bool bmi2;
};

static CPUCapabilities &system_cpu_capabilities()
{
  static CPUCapabilities caps;
  static bool caps_init = false;

  if (!caps_init) {
    int result[4], num;

    memset(&caps, 0, sizeof(caps));

    __cpuid(result, 0);
    num = result[0];

    if (num >= 1) {
      __cpuid(result, 0x00000001);
      caps.mmx = (result[3] & ((int)1 << 23)) != 0;
      caps.sse = (result[3] & ((int)1 << 25)) != 0;
      caps.sse2 = (result[3] & ((int)1 << 26)) != 0;
      caps.sse3 = (result[2] & ((int)1 << 0)) != 0;

      caps.ssse3 = (result[2] & ((int)1 << 9)) != 0;
      caps.sse41 = (result[2] & ((int)1 << 19)) != 0;
      caps.sse42 = (result[2] & ((int)1 << 20)) != 0;

      caps.fma3 = (result[2] & ((int)1 << 12)) != 0;
      caps.avx = false;
      bool os_uses_xsave_xrestore = (result[2] & ((int)1 << 27)) != 0;
      bool cpu_avx_support = (result[2] & ((int)1 << 28)) != 0;

      if (os_uses_xsave_xrestore && cpu_avx_support) {
        // Check if the OS will save the YMM registers
        uint32_t xcr_feature_mask;
#  if defined(__GNUC__)
        int edx; /* not used */
        /* actual opcode for xgetbv */
        __asm__(".byte 0x0f, 0x01, 0xd0" : "=a"(xcr_feature_mask), "=d"(edx) : "c"(0));
#  elif defined(_MSC_VER) && defined(_XCR_XFEATURE_ENABLED_MASK)
        xcr_feature_mask = (uint32_t)_xgetbv(
            _XCR_XFEATURE_ENABLED_MASK); /* min VS2010 SP1 compiler is required */
#  else
        xcr_feature_mask = 0;
#  endif
        caps.avx = (xcr_feature_mask & 0x6) == 0x6;
      }

      caps.f16c = (result[2] & ((int)1 << 29)) != 0;

      __cpuid(result, 0x00000007);
      caps.bmi1 = (result[1] & ((int)1 << 3)) != 0;
      caps.bmi2 = (result[1] & ((int)1 << 8)) != 0;
      caps.avx2 = (result[1] & ((int)1 << 5)) != 0;
    }

    caps_init = true;
  }

  return caps;
}

bool system_cpu_support_sse2()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.sse && caps.sse2;
}

bool system_cpu_support_sse3()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.sse && caps.sse2 && caps.sse3 && caps.ssse3;
}

bool system_cpu_support_sse41()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41;
}

bool system_cpu_support_avx()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41 && caps.avx;
}

bool system_cpu_support_avx2()
{
  CPUCapabilities &caps = system_cpu_capabilities();
  return caps.sse && caps.sse2 && caps.sse3 && caps.ssse3 && caps.sse41 && caps.avx && caps.f16c &&
         caps.avx2 && caps.fma3 && caps.bmi1 && caps.bmi2;
}
#else

bool system_cpu_support_sse2()
{
  return false;
}

bool system_cpu_support_sse3()
{
  return false;
}

bool system_cpu_support_sse41()
{
  return false;
}

bool system_cpu_support_avx()
{
  return false;
}
bool system_cpu_support_avx2()
{
  return false;
}

#endif

bool system_call_self(const vector<string> &args)
{
  /* Escape program and arguments in case they contain spaces. */
  string cmd = "\"" + Sysutil::this_program_path() + "\"";

  for (int i = 0; i < args.size(); i++) {
    cmd += " \"" + args[i] + "\"";
  }

#ifdef _WIN32
  /* Use cmd /S to avoid issues with spaces in arguments. */
  cmd = "cmd /S /C \"" + cmd + " > nul \"";
#else
  /* Quiet output. */
  cmd += " > /dev/null";
#endif

  return (system(cmd.c_str()) == 0);
}

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

CCL_NAMESPACE_END
