/*
 * Copyright 2011-2020 Blender Foundation
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

#ifndef __UTIL_SEMAPHORE_H__
#define __UTIL_SEMAPHORE_H__

#include "util/util_thread.h"

CCL_NAMESPACE_BEGIN

/* Counting Semaphore
 *
 * To restrict concurrent access to a resource to a specified number
 * of threads. Similar to std::counting_semaphore from C++20. */

class thread_counting_semaphore {
 public:
  explicit thread_counting_semaphore(const int count) : count(count)
  {
  }

  thread_counting_semaphore(const thread_counting_semaphore &) = delete;

  void acquire()
  {
    thread_scoped_lock lock(mutex);
    while (count == 0) {
      condition.wait(lock);
    }
    count--;
  }

  void release()
  {
    thread_scoped_lock lock(mutex);
    count++;
    condition.notify_one();
  }

 protected:
  thread_mutex mutex;
  thread_condition_variable condition;
  int count;
};

CCL_NAMESPACE_END

#endif /* __UTIL_SEMAPHORE_H__ */
