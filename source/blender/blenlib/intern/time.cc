/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_time.h"

#ifdef WIN32

#  include <cmath>
#  include <cstdio>

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

/* timeapi.h needs to be included after windows.h. */
#  include <timeapi.h>

double BLI_time_now_seconds()
{
  static int hasperfcounter = -1; /* (-1 == unknown) */
  static double perffreq;

  if (hasperfcounter == -1) {
    __int64 ifreq;
    hasperfcounter = QueryPerformanceFrequency((LARGE_INTEGER *)&ifreq);
    perffreq = (double)ifreq;
  }

  if (hasperfcounter) {
    __int64 count;

    QueryPerformanceCounter((LARGE_INTEGER *)&count);

    return count / perffreq;
  }
  else {
    static double accum = 0.0;
    static int ltick = 0;
    int ntick = GetTickCount();

    if (ntick < ltick) {
      accum += (0xFFFFFFFF - ltick + ntick) / 1000.0;
    }
    else {
      accum += (ntick - ltick) / 1000.0;
    }

    ltick = ntick;
    return accum;
  }
}

long int BLI_time_now_seconds_i()
{
  return (long int)BLI_time_now_seconds();
}

void BLI_time_sleep_ms(int ms)
{
  Sleep(ms);
}

void BLI_time_sleep_precise_us(int us)
{
  /* Prefer thread-safety over caching the timer with a static variable. According to
   * https://github.com/rust-lang/rust/pull/116461/files, this costs only approximately 2000ns. */
  HANDLE timerHandle = CreateWaitableTimerExW(
      nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  if (!timerHandle) {
    if (GetLastError() == ERROR_INVALID_PARAMETER) {
      /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is only supported since Windows 10, version 1803. */
      DWORD duration_ms = DWORD(std::ceil(double(us) / 1000.0));
      Sleep(duration_ms);
    }
    else {
      fprintf(stderr,
              "BLI_time_sleep_precise_us: CreateWaitableTimerExW failed: %d\n",
              GetLastError());
    }
    return;
  }

  /* Wait time is specified in 100 nanosecond intervals. */
  LARGE_INTEGER wait_time;
  wait_time.QuadPart = -us * 10;
  if (!SetWaitableTimer(timerHandle, &wait_time, 0, nullptr, nullptr, 0)) {
    fprintf(stderr, "BLI_time_sleep_precise_us: SetWaitableTimer failed: %d\n", GetLastError());
    CloseHandle(timerHandle);
    return;
  }

  if (WaitForSingleObject(timerHandle, INFINITE) != WAIT_OBJECT_0) {
    fprintf(stderr, "BLI_time_sleep_precise_us: WaitForSingleObject failed: %d\n", GetLastError());
    CloseHandle(timerHandle);
    return;
  }

  CloseHandle(timerHandle);
}

#else

#  include <chrono>
#  include <thread>

#  include <sys/time.h>
#  include <unistd.h>

double BLI_time_now_seconds()
{
  timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  return (double(tv.tv_sec) + tv.tv_usec / 1000000.0);
}

long int BLI_time_now_seconds_i()
{
  timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  return tv.tv_sec;
}

void BLI_time_sleep_ms(int ms)
{
  if (ms >= 1000) {
    sleep(ms / 1000);
    ms = (ms % 1000);
  }

  usleep(ms * 1000);
}

void BLI_time_sleep_precise_us(int us)
{
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}

#endif
