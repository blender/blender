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

#ifndef __UTIL_PROFILING_H__
#define __UTIL_PROFILING_H__

#include <atomic>

#include "util/util_foreach.h"
#include "util/util_map.h"
#include "util/util_thread.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

enum ProfilingEvent : uint32_t {
	PROFILING_UNKNOWN,
	PROFILING_RAY_SETUP,
	PROFILING_PATH_INTEGRATE,
	PROFILING_SCENE_INTERSECT,
	PROFILING_INDIRECT_EMISSION,
	PROFILING_VOLUME,
	PROFILING_SHADER_SETUP,
	PROFILING_SHADER_EVAL,
	PROFILING_SHADER_APPLY,
	PROFILING_AO,
	PROFILING_SUBSURFACE,
	PROFILING_CONNECT_LIGHT,
	PROFILING_SURFACE_BOUNCE,
	PROFILING_WRITE_RESULT,

	PROFILING_INTERSECT,
	PROFILING_INTERSECT_LOCAL,
	PROFILING_INTERSECT_SHADOW_ALL,
	PROFILING_INTERSECT_VOLUME,
	PROFILING_INTERSECT_VOLUME_ALL,

	PROFILING_CLOSURE_EVAL,
	PROFILING_CLOSURE_SAMPLE,
	PROFILING_CLOSURE_VOLUME_EVAL,
	PROFILING_CLOSURE_VOLUME_SAMPLE,

	PROFILING_DENOISING,
	PROFILING_DENOISING_CONSTRUCT_TRANSFORM,
	PROFILING_DENOISING_RECONSTRUCT,
	PROFILING_DENOISING_DIVIDE_SHADOW,
	PROFILING_DENOISING_NON_LOCAL_MEANS,
	PROFILING_DENOISING_COMBINE_HALVES,
	PROFILING_DENOISING_GET_FEATURE,
	PROFILING_DENOISING_DETECT_OUTLIERS,

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
	vector<ProfilingState*> states;
};

class ProfilingHelper {
public:
	ProfilingHelper(ProfilingState *state, ProfilingEvent event)
	 : state(state)
	{
		previous_event = state->event;
		state->event = event;
	}

	inline void set_event(ProfilingEvent event)
	{
		state->event = event;
	}

	inline void set_shader(int shader)
	{
		state->shader = shader;
		if(state->active) {
			assert(shader < state->shader_hits.size());
			state->shader_hits[shader]++;
		}
	}

	inline void set_object(int object)
	{
		state->object = object;
		if(state->active) {
			assert(object < state->object_hits.size());
			state->object_hits[object]++;
		}
	}

	~ProfilingHelper()
	{
		state->event = previous_event;
	}
private:
	ProfilingState *state;
	uint32_t previous_event;
};

CCL_NAMESPACE_END

#endif  /* __UTIL_PROFILING_H__ */
