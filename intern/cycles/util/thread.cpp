/*
 * Copyright 2011-2016 Blender Foundation
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

#include "util/thread.h"

#include "util/system.h"
#include "util/windows.h"

CCL_NAMESPACE_BEGIN

thread::thread(function<void()> run_cb, int node) : run_cb_(run_cb), joined_(false), node_(node)
{
#ifdef __APPLE__
  /* Set the stack size to 2MB to match Linux. The default 512KB on macOS is
   * too small for Embree, and consistent stack size also makes things more
   * predictable in general. */
  pthread_attr_t attribute;
  pthread_attr_init(&attribute);
  pthread_attr_setstacksize(&attribute, 1024 * 1024 * 2);
  pthread_create(&pthread_id, &attribute, run, (void *)this);
#else
  std_thread = std::thread(&thread::run, this);
#endif
}

thread::~thread()
{
  if (!joined_) {
    join();
  }
}

void *thread::run(void *arg)
{
  thread *self = (thread *)(arg);
  if (self->node_ != -1) {
    system_cpu_run_thread_on_node(self->node_);
  }
  self->run_cb_();
  return NULL;
}

bool thread::join()
{
  joined_ = true;
#ifdef __APPLE__
  return pthread_join(pthread_id, NULL) == 0;
#else
  try {
    std_thread.join();
    return true;
  }
  catch (const std::system_error &) {
    return false;
  }
#endif
}

CCL_NAMESPACE_END
