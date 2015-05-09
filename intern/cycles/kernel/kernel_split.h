/*
 * Copyright 2011-2015 Blender Foundation
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

#ifndef  _KERNEL_SPLIT_H_
#define  _KERNEL_SPLIT_H_

#include "kernel_compat_opencl.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"

/* atomic_add_float function should be defined prior to its usage in kernel_passes.h */
#if defined(__SPLIT_KERNEL__) && defined(__WORK_STEALING__)
/* Utility functions for float atomics */
/* float atomics impl credits : http://suhorukov.blogspot.in/2011/12/opencl-11-atomic-operations-on-floating.html */
ccl_device_inline void atomic_add_float(volatile ccl_global float *source, const float operand) {
	union {
		unsigned int intVal;
		float floatVal;

	} newVal;
	union {
		unsigned int intVal;
		float floatVal;

	} prevVal;
	do {
		prevVal.floatVal = *source;
		newVal.floatVal = prevVal.floatVal + operand;

	} while (atomic_cmpxchg((volatile ccl_global unsigned int *)source, prevVal.intVal, newVal.intVal) != prevVal.intVal);
}
#endif // __SPLIT_KERNEL__ && __WORK_STEALING__

#ifdef __OSL__
#include "osl_shader.h"
#endif

#include "kernel_random.h"
#include "kernel_projection.h"
#include "kernel_montecarlo.h"
#include "kernel_differential.h"
#include "kernel_camera.h"

#include "geom/geom.h"

#include "kernel_accumulate.h"
#include "kernel_shader.h"
#include "kernel_light.h"
#include "kernel_passes.h"

#ifdef __SUBSURFACE__
#include "kernel_subsurface.h"
#endif

#ifdef __VOLUME__
#include "kernel_volume.h"
#endif

#include "kernel_path_state.h"
#include "kernel_shadow.h"
#include "kernel_emission.h"
#include "kernel_path_common.h"
#include "kernel_path_surface.h"
#include "kernel_path_volume.h"

#ifdef __KERNEL_DEBUG__
#include "kernel_debug.h"
#endif

#include "kernel_queues.h"
#include "kernel_work_stealing.h"

#endif
