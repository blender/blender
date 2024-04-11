/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_system.h"

/* for backtrace and gethostname/GetComputerName */
#if defined(WIN32)
#  include <intrin.h>

#  include "BLI_winstuff.h"
#else
#  if defined(HAVE_EXECINFO_H)
#    include <execinfo.h>
#  endif
#  include <unistd.h>
#endif

int BLI_cpu_support_sse2(void)
{
#if defined(__x86_64__) || defined(_M_X64)
  /* x86_64 always has SSE2 instructions */
  return 1;
#elif defined(__GNUC__) && defined(i386)
  /* for GCC x86 we check cpuid */
  uint d;
  __asm__(
      "pushl %%ebx\n\t"
      "cpuid\n\t"
      "popl %%ebx\n\t"
      : "=d"(d)
      : "a"(1));
  return (d & 0x04000000) != 0;
#elif (defined(_MSC_VER) && defined(_M_IX86))
  /* also check cpuid for MSVC x86 */
  uint d;
  __asm {
    xor     eax, eax
    inc eax
    push ebx
    cpuid
    pop ebx
    mov d, edx
  }
  return (d & 0x04000000) != 0;
#else
  return 0;
#endif
}

/* Windows stack-walk lives in system_win32.c */
#if !defined(_MSC_VER)
void BLI_system_backtrace(FILE *fp)
{
  /* ----------------------- */
  /* If system as execinfo.h */
#  if defined(HAVE_EXECINFO_H)

#    define SIZE 100
  void *buffer[SIZE];
  int nptrs;
  char **strings;
  int i;

  /* Include a back-trace for good measure.
   *
   * NOTE: often values printed are addresses (no line numbers of function names),
   * this information can be expanded using `addr2line`, a utility is included to
   * conveniently run addr2line on the output generated here:
   *
   *   `./tools/utils/addr2line_backtrace.py --exe=/path/to/blender trace.txt`
   */
  nptrs = backtrace(buffer, SIZE);
  strings = backtrace_symbols(buffer, nptrs);
  for (i = 0; i < nptrs; i++) {
    fputs(strings[i], fp);
    fputc('\n', fp);
  }

  free(strings);
#    undef SIZE

#  else
  /* --------------------- */
  /* Non MSVC/Apple/Linux. */
  (void)fp;
#  endif
}
#endif
/* end BLI_system_backtrace */

/* NOTE: The code for CPU brand string is adopted from Cycles. */

#if !defined(_WIN32) || defined(FREE_WINDOWS)
static void __cpuid(
    /* Cannot be const, because it is modified below.
     * NOLINTNEXTLINE: readability-non-const-parameter. */
    int data[4],
    int selector)
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
  (void)selector;
  data[0] = data[1] = data[2] = data[3] = 0;
#  endif
}
#endif

char *BLI_cpu_brand_string(void)
{
#if !defined(_M_ARM64)
  char buf[49] = {0};
  int result[4] = {0};
  __cpuid(result, 0x80000000);
  if (result[0] >= (int)0x80000004) {
    __cpuid((int *)(buf + 0), 0x80000002);
    __cpuid((int *)(buf + 16), 0x80000003);
    __cpuid((int *)(buf + 32), 0x80000004);
    char *brand = BLI_strdup(buf);
    /* TODO(sergey): Make it a bit more presentable by removing trademark. */
    return brand;
  }
#else
  /* No CPUID on ARM64, so we pull from the registry (on Windows) instead. */
  DWORD vendorIdentifierLength = 255;
  char vendorIdentifier[255];
  if (RegGetValueA(HKEY_LOCAL_MACHINE,
                   "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                   "VendorIdentifier",
                   RRF_RT_REG_SZ,
                   NULL,
                   &vendorIdentifier,
                   &vendorIdentifierLength) == ERROR_SUCCESS)
  {
    return BLI_strdup(vendorIdentifier);
  }
#endif
  return NULL;
}

int BLI_cpu_support_sse42(void)
{
#if !defined(_M_ARM64)
  int result[4], num;
  __cpuid(result, 0);
  num = result[0];

  if (num >= 1) {
    __cpuid(result, 0x00000001);
    return (result[2] & ((int)1 << 20)) != 0;
  }
#endif
  return 0;
}

void BLI_hostname_get(char *buffer, size_t bufsize)
{
#ifndef WIN32
  if (gethostname(buffer, bufsize - 1) < 0) {
    BLI_strncpy(buffer, "-unknown-", bufsize);
  }
  /* When `gethostname()` truncates, it doesn't guarantee the trailing `\0`. */
  buffer[bufsize - 1] = '\0';
#else
  DWORD bufsize_inout = bufsize;
  if (!GetComputerName(buffer, &bufsize_inout)) {
    BLI_strncpy(buffer, "-unknown-", bufsize);
  }
#endif
}

size_t BLI_system_memory_max_in_megabytes(void)
{
  /* Maximum addressable bytes on this platform.
   *
   * NOTE: Due to the shift arithmetic this is a half of the memory. */
  const size_t limit_bytes_half = (((size_t)1) << (sizeof(size_t[8]) - 1));
  /* Convert it to megabytes and return. */
  return (limit_bytes_half >> 20) * 2;
}

int BLI_system_memory_max_in_megabytes_int(void)
{
  const size_t limit_megabytes = BLI_system_memory_max_in_megabytes();
  /* NOTE: The result will fit into integer. */
  return (int)min_zz(limit_megabytes, (size_t)INT_MAX);
}
