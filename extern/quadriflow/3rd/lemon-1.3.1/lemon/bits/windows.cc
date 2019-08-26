/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

///\file
///\brief Some basic non-inline functions and static global data.

#include<lemon/bits/windows.h>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef UNICODE
#undef UNICODE
#endif
#include <windows.h>
#ifdef LOCALE_INVARIANT
#define MY_LOCALE LOCALE_INVARIANT
#else
#define MY_LOCALE LOCALE_NEUTRAL
#endif
#else
#include <unistd.h>
#include <ctime>
#ifndef WIN32
#include <sys/times.h>
#endif
#include <sys/time.h>
#endif

#include <cmath>
#include <sstream>

namespace lemon {
  namespace bits {
    void getWinProcTimes(double &rtime,
                         double &utime, double &stime,
                         double &cutime, double &cstime)
    {
#ifdef WIN32
      static const double ch = 4294967296.0e-7;
      static const double cl = 1.0e-7;

      FILETIME system;
      GetSystemTimeAsFileTime(&system);
      rtime = ch * system.dwHighDateTime + cl * system.dwLowDateTime;

      FILETIME create, exit, kernel, user;
      if (GetProcessTimes(GetCurrentProcess(),&create, &exit, &kernel, &user)) {
        utime = ch * user.dwHighDateTime + cl * user.dwLowDateTime;
        stime = ch * kernel.dwHighDateTime + cl * kernel.dwLowDateTime;
        cutime = 0;
        cstime = 0;
      } else {
        rtime = 0;
        utime = 0;
        stime = 0;
        cutime = 0;
        cstime = 0;
      }
#else
      timeval tv;
      gettimeofday(&tv, 0);
      rtime=tv.tv_sec+double(tv.tv_usec)/1e6;

      tms ts;
      double tck=sysconf(_SC_CLK_TCK);
      times(&ts);
      utime=ts.tms_utime/tck;
      stime=ts.tms_stime/tck;
      cutime=ts.tms_cutime/tck;
      cstime=ts.tms_cstime/tck;
#endif
    }

    std::string getWinFormattedDate()
    {
      std::ostringstream os;
#ifdef WIN32
      SYSTEMTIME time;
      GetSystemTime(&time);
      char buf1[11], buf2[9], buf3[5];
          if (GetDateFormat(MY_LOCALE, 0, &time,
                        ("ddd MMM dd"), buf1, 11) &&
          GetTimeFormat(MY_LOCALE, 0, &time,
                        ("HH':'mm':'ss"), buf2, 9) &&
          GetDateFormat(MY_LOCALE, 0, &time,
                        ("yyyy"), buf3, 5)) {
        os << buf1 << ' ' << buf2 << ' ' << buf3;
      }
      else os << "unknown";
#else
      timeval tv;
      gettimeofday(&tv, 0);

      char cbuf[26];
      ctime_r(&tv.tv_sec,cbuf);
      os << cbuf;
#endif
      return os.str();
    }

    int getWinRndSeed()
    {
#ifdef WIN32
      FILETIME time;
      GetSystemTimeAsFileTime(&time);
      return GetCurrentProcessId() + time.dwHighDateTime + time.dwLowDateTime;
#else
      timeval tv;
      gettimeofday(&tv, 0);
      return getpid() + tv.tv_sec + tv.tv_usec;
#endif
    }

    WinLock::WinLock() {
#ifdef WIN32
      CRITICAL_SECTION *lock = new CRITICAL_SECTION;
      InitializeCriticalSection(lock);
      _repr = lock;
#else
      _repr = 0; //Just to avoid 'unused variable' warning with clang
#endif
    }

    WinLock::~WinLock() {
#ifdef WIN32
      CRITICAL_SECTION *lock = static_cast<CRITICAL_SECTION*>(_repr);
      DeleteCriticalSection(lock);
      delete lock;
#endif
    }

    void WinLock::lock() {
#ifdef WIN32
      CRITICAL_SECTION *lock = static_cast<CRITICAL_SECTION*>(_repr);
      EnterCriticalSection(lock);
#endif
    }

    void WinLock::unlock() {
#ifdef WIN32
      CRITICAL_SECTION *lock = static_cast<CRITICAL_SECTION*>(_repr);
      LeaveCriticalSection(lock);
#endif
    }
  }
}
