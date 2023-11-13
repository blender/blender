/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_THREAD_H__
#define __UTIL_THREAD_H__

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#ifdef _WIN32
#  include "util/windows.h"
#else
#  include <pthread.h>
#endif

/* NOTE: Use tbb/spin_mutex.h instead of util_tbb.h because some of the TBB
 * functionality requires RTTI, which is disabled for OSL kernel. */
#include <tbb/spin_mutex.h>

#include "util/function.h"

CCL_NAMESPACE_BEGIN

typedef std::mutex thread_mutex;
typedef std::unique_lock<std::mutex> thread_scoped_lock;
typedef std::condition_variable thread_condition_variable;

/**
 * Own thread implementation similar to std::thread, so we can set a
 * custom stack size on macOS.
 */
class thread {
 public:
  thread(function<void()> run_cb);
  ~thread();

  static void *run(void *arg);
  bool join();

 protected:
  function<void()> run_cb_;
#ifdef __APPLE__
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

#endif /* __UTIL_THREAD_H__ */
