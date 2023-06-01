/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "PIL_time.h"

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

double PIL_check_seconds_timer(void)
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

long int PIL_check_seconds_timer_i(void)
{
  return (long int)PIL_check_seconds_timer();
}

void PIL_sleep_ms(int ms)
{
  Sleep(ms);
}

#else

#  include <sys/time.h>
#  include <unistd.h>

double PIL_check_seconds_timer(void)
{
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  return ((double)tv.tv_sec + tv.tv_usec / 1000000.0);
}

long int PIL_check_seconds_timer_i(void)
{
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  return tv.tv_sec;
}

void PIL_sleep_ms(int ms)
{
  if (ms >= 1000) {
    sleep(ms / 1000);
    ms = (ms % 1000);
  }

  usleep(ms * 1000);
}

#endif
