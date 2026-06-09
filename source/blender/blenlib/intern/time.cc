/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_time.h"

#include <chrono>

#ifdef WIN32

#  include <cmath>
#  include <cstdio>
#  include <windows.h>
/* timeapi.h needs to be included after windows.h. */
#  include <timeapi.h>

#else

#  include <thread>
#  include <unistd.h>

#endif

namespace blender {

double BLI_time_now_seconds()
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

int64_t BLI_time_now_seconds_i()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

#ifdef WIN32

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
              "BLI_time_sleep_precise_us: CreateWaitableTimerExW failed: %lx\n",
              GetLastError());
    }
    return;
  }

  /* Wait time is specified in 100 nanosecond intervals. */
  LARGE_INTEGER wait_time;
  wait_time.QuadPart = -us * 10;
  if (!SetWaitableTimer(timerHandle, &wait_time, 0, nullptr, nullptr, 0)) {
    fprintf(stderr, "BLI_time_sleep_precise_us: SetWaitableTimer failed: %lx\n", GetLastError());
    CloseHandle(timerHandle);
    return;
  }

  if (WaitForSingleObject(timerHandle, INFINITE) != WAIT_OBJECT_0) {
    fprintf(
        stderr, "BLI_time_sleep_precise_us: WaitForSingleObject failed: %lx\n", GetLastError());
    CloseHandle(timerHandle);
    return;
  }

  CloseHandle(timerHandle);
}

#else

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

}  // namespace blender
