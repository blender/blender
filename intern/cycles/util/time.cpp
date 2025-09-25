/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/time.h"

#include <chrono>
#include <cstdlib>

#if !defined(_WIN32)
#  include <sys/time.h>
#  include <unistd.h>
#endif

#include "util/string.h"

#ifdef _WIN32
#  include "util/windows.h"
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#  ifdef _MSC_VER
#    include <intrin.h>
#  else
#    include <x86intrin.h>
#  endif
#endif

CCL_NAMESPACE_BEGIN

#ifdef _WIN32
double time_dt()
{
  __int64 frequency, counter;

  QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);
  QueryPerformanceCounter((LARGE_INTEGER *)&counter);

  return (double)counter / (double)frequency;
}

void time_sleep(const double t)
{
  Sleep((int)(t * 1000));
}
#else
double time_dt()
{
  struct timeval now;
  gettimeofday(&now, nullptr);

  return now.tv_sec + now.tv_usec * 1e-6;
}

/* sleep t seconds */
void time_sleep(double t)
{
  /* get whole seconds */
  const int s = (int)t;

  if (s >= 1) {
    sleep(s);

    /* adjust parameter to remove whole seconds */
    t -= s;
  }

  /* get microseconds */
  const int us = (int)(t * 1e6);
  if (us > 0) {
    usleep(us);
  }
}
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
/* Use cntvct_el0/cntfrq_el0 registers on ARM64. */

uint64_t time_fast_tick(uint32_t * /*last_cpu*/)
{
#  if defined(ARCH_COMPILER_MSVC)
  return _ReadStatusReg(ARM64_CNTVCT_EL0);
#  else
  uint64_t counter;
  asm("mrs %x0, cntvct_el0" : "=r"(counter));
  return counter;
#  endif
}
uint64_t time_fast_frequency()
{
#  if defined(ARCH_COMPILER_MSVC)
  return _ReadStatusReg(ARM64_CNTFRQ_EL0);
#  else
  uint64_t freq;
  asm("mrs %x0, cntfrq_el0" : "=r"(freq));
  return freq;
#  endif
}
#elif defined(__x86_64__) || defined(_M_X64)
/* Use RDTSCP on x86-64. */

uint64_t time_fast_tick(uint32_t *last_cpu)
{
  return __rdtscp(last_cpu);
}
uint64_t time_fast_frequency()
{
  static bool initialized = false;
  static uint64_t frequency;

  /* Unfortunately TSC does not provide a easily accessible frequency value, so roughly calibrate
   * by sleeping a millisecond. Not ideal, but good enough for our purposes. */
  if (!initialized) {
    uint32_t cpu;
    uint64_t start_tick = time_fast_tick(&cpu);
    double start_precise = time_dt();
    time_sleep(0.001);
    uint64_t end_tick = time_fast_tick(&cpu);
    double end_precise = time_dt();
    frequency = uint64_t(double(end_tick - start_tick) / (end_precise - start_precise));
    initialized = true;
  }

  return frequency;
}
#else
/* Fall back to std::chrono::steady_clock. */

uint64_t time_fast_tick(uint32_t * /*last_cpu*/)
{
  auto now = std::chrono::steady_clock::now();
  auto nanoseconds = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
  return nanoseconds.time_since_epoch().count();
}
uint64_t time_fast_frequency()
{
  return 1000000000;
}
#endif

/* Time in format "hours:minutes:seconds.hundreds" */

string time_human_readable_from_seconds(const double seconds)
{
  const int h = (((int)seconds) / (60 * 60));
  const int m = (((int)seconds) / 60) % 60;
  const int s = (((int)seconds) % 60);
  const int r = (((int)(seconds * 100)) % 100);

  if (h > 0) {
    return string_printf("%.2d:%.2d:%.2d.%.2d", h, m, s, r);
  }
  return string_printf("%.2d:%.2d.%.2d", m, s, r);
}

double time_human_readable_to_seconds(const string &time_string)
{
  /* Those are multiplies of a corresponding token surrounded by : in the
   * time string, which denotes how to convert value to seconds.
   * Effectively: seconds, minutes, hours, days in seconds. */
  const int multipliers[] = {1, 60, 60 * 60, 24 * 60 * 60};
  const int num_multiplies = sizeof(multipliers) / sizeof(*multipliers);
  if (time_string.empty()) {
    return 0.0;
  }
  double result = 0.0;
  /* Split fractions of a second from the encoded time. */
  vector<string> fraction_tokens;
  string_split(fraction_tokens, time_string, ".", false);
  const int num_fraction_tokens = fraction_tokens.size();
  if (num_fraction_tokens == 0) {
    /* Time string is malformed. */
    return 0.0;
  }
  if (fraction_tokens.size() == 1) {
    /* There is no fraction of a second specified, the rest of the code
     * handles this normally. */
  }
  else if (fraction_tokens.size() == 2) {
    result = atof(fraction_tokens[1].c_str());
    result *= ::pow(0.1, fraction_tokens[1].length());
  }
  else {
    /* This is not a valid string, the result can not be reliable. */
    return 0.0;
  }
  /* Split hours, minutes and seconds.
   * Hours part is optional. */
  vector<string> tokens;
  string_split(tokens, fraction_tokens[0], ":", false);
  const int num_tokens = tokens.size();
  if (num_tokens > num_multiplies) {
    /* Can not reliably represent the value. */
    return 0.0;
  }
  for (int i = 0; i < num_tokens; ++i) {
    result += atoi(tokens[num_tokens - i - 1].c_str()) * multipliers[i];
  }
  return result;
}

CCL_NAMESPACE_END
