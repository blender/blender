/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

#if defined(__APPLE__) || defined(__linux__) && !defined(__GLIBC__)
#  include <pthread.h>
#else
#  include <thread>
#endif

#ifdef _WIN32
#  include "util/windows.h"
#endif

/* NOTE: Use tbb/spin_mutex.h instead of util_tbb.h because some of the TBB
 * functionality requires RTTI, which is disabled for OSL kernel. */
#include <tbb/spin_mutex.h>

CCL_NAMESPACE_BEGIN

using thread_mutex = std::mutex;
using thread_scoped_lock = std::unique_lock<std::mutex>;
using thread_condition_variable = std::condition_variable;

/**
 * Own thread implementation similar to std::thread, so we can set a
 * custom stack size on macOS.
 */
class thread {
 public:
  thread(std::function<void()> run_cb);
  ~thread();

  static void *run(void *arg);
  bool join();

 protected:
  std::function<void()> run_cb_;
#if defined(__APPLE__) || defined(__linux__) && !defined(__GLIBC__)
  pthread_t pthread_id;
#else
  std::thread std_thread;
#endif
  bool joined_;
};

using thread_spin_lock = tbb::spin_mutex;

class thread_scoped_spin_lock {
 public:
  explicit thread_scoped_spin_lock(thread_spin_lock &lock) : lock_(lock)
  {
    lock_.lock();
  }

  ~thread_scoped_spin_lock()
  {
    lock_.unlock();
  }

  /* TODO(sergey): Implement manual control over lock/unlock. */

 protected:
  thread_spin_lock &lock_;
};

CCL_NAMESPACE_END
