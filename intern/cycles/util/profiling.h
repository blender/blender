/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_PROFILING_H__
#define __UTIL_PROFILING_H__

#include <atomic>

#include "util/map.h"
#include "util/thread.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

enum ProfilingEvent : uint32_t {
  PROFILING_UNKNOWN,
  PROFILING_RAY_SETUP,

  PROFILING_INTERSECT_CLOSEST,
  PROFILING_INTERSECT_SUBSURFACE,
  PROFILING_INTERSECT_SHADOW,
  PROFILING_INTERSECT_VOLUME_STACK,
  PROFILING_INTERSECT_DEDICATED_LIGHT,

  PROFILING_SHADE_SURFACE_SETUP,
  PROFILING_SHADE_SURFACE_EVAL,
  PROFILING_SHADE_SURFACE_DIRECT_LIGHT,
  PROFILING_SHADE_SURFACE_INDIRECT_LIGHT,
  PROFILING_SHADE_SURFACE_AO,
  PROFILING_SHADE_SURFACE_PASSES,
  PROFILING_SHADE_DEDICATED_LIGHT,

  PROFILING_SHADE_VOLUME_SETUP,
  PROFILING_SHADE_VOLUME_INTEGRATE,
  PROFILING_SHADE_VOLUME_DIRECT_LIGHT,
  PROFILING_SHADE_VOLUME_INDIRECT_LIGHT,

  PROFILING_SHADE_SHADOW_SETUP,
  PROFILING_SHADE_SHADOW_SURFACE,
  PROFILING_SHADE_SHADOW_VOLUME,

  PROFILING_SHADE_LIGHT_SETUP,
  PROFILING_SHADE_LIGHT_EVAL,

  PROFILING_NUM_EVENTS,
};

/* Contains the current execution state of a worker thread.
 * These values are constantly updated by the worker.
 * Periodically the profiler thread will wake up, read them
 * and update its internal counters based on it.
 *
 * Atomics aren't needed here since we're only doing direct
 * writes and reads to (4-byte-aligned) uint32_t, which is
 * guaranteed to be atomic on x86 since the 486.
 * Memory ordering is not guaranteed but does not matter.
 *
 * And even on other architectures, the extremely rare corner
 * case of reading an intermediate state could at worst result
 * in a single incorrect sample. */
struct ProfilingState {
  volatile uint32_t event = PROFILING_UNKNOWN;
  volatile int32_t shader = -1;
  volatile int32_t object = -1;
  volatile bool active = false;

  vector<uint64_t> shader_hits;
  vector<uint64_t> object_hits;
};

class Profiler {
 public:
  Profiler();
  ~Profiler();

  void reset(int num_shaders, int num_objects);

  void start();
  void stop();

  void add_state(ProfilingState *state);
  void remove_state(ProfilingState *state);

  uint64_t get_event(ProfilingEvent event);
  bool get_shader(int shader, uint64_t &samples, uint64_t &hits);
  bool get_object(int object, uint64_t &samples, uint64_t &hits);

  bool active() const;

 protected:
  void run();

  /* Tracks how often the worker was in each ProfilingEvent while sampling,
   * so multiplying the values by the sample frequency (currently 1ms)
   * gives the approximate time spent in each state. */
  vector<uint64_t> event_samples;
  vector<uint64_t> shader_samples;
  vector<uint64_t> object_samples;

  /* Tracks the total amounts every object/shader was hit.
   * Used to evaluate relative cost, written by the render thread.
   * Indexed by the shader and object IDs that the kernel also uses
   * to index __object_flag and __shaders. */
  vector<uint64_t> shader_hits;
  vector<uint64_t> object_hits;

  volatile bool do_stop_worker;
  thread *worker;

  thread_mutex mutex;
  vector<ProfilingState *> states;
};

class ProfilingHelper {
 public:
  ProfilingHelper(ProfilingState *state, ProfilingEvent event) : state(state)
  {
    previous_event = state->event;
    state->event = event;
  }

  ~ProfilingHelper()
  {
    state->event = previous_event;
  }

  inline void set_event(ProfilingEvent event)
  {
    state->event = event;
  }

 protected:
  ProfilingState *state;
  uint32_t previous_event;
};

class ProfilingWithShaderHelper : public ProfilingHelper {
 public:
  ProfilingWithShaderHelper(ProfilingState *state, ProfilingEvent event)
      : ProfilingHelper(state, event)
  {
  }

  ~ProfilingWithShaderHelper()
  {
    state->object = -1;
    state->shader = -1;
  }

  inline void set_shader(int object, int shader)
  {
    if (state->active) {
      state->shader = shader;
      state->object = object;

      if (shader >= 0) {
        assert(shader < state->shader_hits.size());
        state->shader_hits[shader]++;
      }

      if (object >= 0) {
        assert(object < state->object_hits.size());
        state->object_hits[object]++;
      }
    }
  }
};

CCL_NAMESPACE_END

#endif /* __UTIL_PROFILING_H__ */
