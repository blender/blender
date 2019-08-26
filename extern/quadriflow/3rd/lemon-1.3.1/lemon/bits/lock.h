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

#ifndef LEMON_BITS_LOCK_H
#define LEMON_BITS_LOCK_H

#include <lemon/config.h>
#if defined(LEMON_USE_PTHREAD)
#include <pthread.h>
#elif defined(LEMON_USE_WIN32_THREADS)
#include <lemon/bits/windows.h>
#endif

namespace lemon {
  namespace bits {

#if defined(LEMON_USE_PTHREAD)
    class Lock {
    public:
      Lock() {
        pthread_mutex_init(&_lock, 0);
      }
      ~Lock() {
        pthread_mutex_destroy(&_lock);
      }
      void lock() {
        pthread_mutex_lock(&_lock);
      }
      void unlock() {
        pthread_mutex_unlock(&_lock);
      }

    private:
      pthread_mutex_t _lock;
    };
#elif defined(LEMON_USE_WIN32_THREADS)
    class Lock : public WinLock {};
#else
    class Lock {
    public:
      Lock() {}
      ~Lock() {}
      void lock() {}
      void unlock() {}
    };
#endif
  }
}

#endif
