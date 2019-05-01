/*
 * Copyright 2011-2018 Blender Foundation
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

#include "util/util_algorithm.h"
#include "util/util_profiling.h"
#include "util/util_set.h"

CCL_NAMESPACE_BEGIN

Profiler::Profiler() : do_stop_worker(true), worker(NULL)
{
}

Profiler::~Profiler()
{
  assert(worker == NULL);
}

void Profiler::run()
{
  uint64_t updates = 0;
  auto start_time = std::chrono::system_clock::now();
  while (!do_stop_worker) {
    thread_scoped_lock lock(mutex);
    foreach (ProfilingState *state, states) {
      uint32_t cur_event = state->event;
      int32_t cur_shader = state->shader;
      int32_t cur_object = state->object;

      /* The state reads/writes should be atomic, but just to be sure
       * check the values for validity anyways. */
      if (cur_event < PROFILING_NUM_EVENTS) {
        event_samples[cur_event]++;
      }

      if (cur_shader >= 0 && cur_shader < shader_samples.size()) {
        /* Only consider the active shader during events whose runtime significantly depends on it.
         */
        if (((cur_event >= PROFILING_SHADER_EVAL) && (cur_event <= PROFILING_SUBSURFACE)) ||
            ((cur_event >= PROFILING_CLOSURE_EVAL) &&
             (cur_event <= PROFILING_CLOSURE_VOLUME_SAMPLE))) {
          shader_samples[cur_shader]++;
        }
      }

      if (cur_object >= 0 && cur_object < object_samples.size()) {
        object_samples[cur_object]++;
      }
    }
    lock.unlock();

    /* Relative waits always overshoot a bit, so just waiting 1ms every
     * time would cause the sampling to drift over time.
     * By keeping track of the absolute time, the wait times correct themselves -
     * if one wait overshoots a lot, the next one will be shorter to compensate. */
    updates++;
    std::this_thread::sleep_until(start_time + updates * std::chrono::milliseconds(1));
  }
}

void Profiler::reset(int num_shaders, int num_objects)
{
  bool running = (worker != NULL);
  if (running) {
    stop();
  }

  /* Resize and clear the accumulation vectors. */
  shader_hits.assign(num_shaders, 0);
  object_hits.assign(num_objects, 0);

  event_samples.assign(PROFILING_NUM_EVENTS, 0);
  shader_samples.assign(num_shaders, 0);
  object_samples.assign(num_objects, 0);

  if (running) {
    start();
  }
}

void Profiler::start()
{
  assert(worker == NULL);
  do_stop_worker = false;
  worker = new thread(function_bind(&Profiler::run, this));
}

void Profiler::stop()
{
  if (worker != NULL) {
    do_stop_worker = true;

    worker->join();
    delete worker;
    worker = NULL;
  }
}

void Profiler::add_state(ProfilingState *state)
{
  thread_scoped_lock lock(mutex);

  /* Add the ProfilingState from the list of sampled states. */
  assert(std::find(states.begin(), states.end(), state) == states.end());
  states.push_back(state);

  /* Resize thread-local hit counters. */
  state->shader_hits.assign(shader_hits.size(), 0);
  state->object_hits.assign(object_hits.size(), 0);

  /* Initialize the state. */
  state->event = PROFILING_UNKNOWN;
  state->shader = -1;
  state->object = -1;
  state->active = true;
}

void Profiler::remove_state(ProfilingState *state)
{
  thread_scoped_lock lock(mutex);

  /* Remove the ProfilingState from the list of sampled states. */
  states.erase(std::remove(states.begin(), states.end(), state), states.end());
  state->active = false;

  /* Merge thread-local hit counters. */
  assert(shader_hits.size() == state->shader_hits.size());
  for (int i = 0; i < shader_hits.size(); i++) {
    shader_hits[i] += state->shader_hits[i];
  }

  assert(object_hits.size() == state->object_hits.size());
  for (int i = 0; i < object_hits.size(); i++) {
    object_hits[i] += state->object_hits[i];
  }
}

uint64_t Profiler::get_event(ProfilingEvent event)
{
  assert(worker == NULL);
  return event_samples[event];
}

bool Profiler::get_shader(int shader, uint64_t &samples, uint64_t &hits)
{
  assert(worker == NULL);
  if (shader_samples[shader] == 0) {
    return false;
  }
  samples = shader_samples[shader];
  hits = shader_hits[shader];
  return true;
}

bool Profiler::get_object(int object, uint64_t &samples, uint64_t &hits)
{
  assert(worker == NULL);
  if (object_samples[object] == 0) {
    return false;
  }
  samples = object_samples[object];
  hits = object_hits[object];
  return true;
}

CCL_NAMESPACE_END
