/*
 * Copyright 2011-2013 Blender Foundation
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
 * limitations under the License
 */

/* Optimized CPU kernel entry points. This file is compiled with SSE2
 * optimization flags and nearly all functions inlined, while kernel.cpp
 * is compiled without for other CPU's. */

/* SSE optimization disabled for now on 32 bit, see bug #36316 */
#if !(defined(__GNUC__) && (defined(i386) || defined(_M_IX86)))
#define __KERNEL_SSE2__
#endif

#include "util_optimization.h"

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2

#include "kernel.h"
#include "kernel_compat_cpu.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"
#include "kernel_film.h"
#include "kernel_path.h"
#include "kernel_displace.h"

CCL_NAMESPACE_BEGIN

/* Path Tracing */

void kernel_cpu_sse2_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state, int sample, int x, int y, int offset, int stride)
{
#ifdef __BRANCHED_PATH__
	if(kernel_data.integrator.branched)
		kernel_branched_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
	else
#endif
		kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
}

/* Film */

void kernel_cpu_sse2_convert_to_byte(KernelGlobals *kg, uchar4 *rgba, float *buffer, float sample_scale, int x, int y, int offset, int stride)
{
	kernel_film_convert_to_byte(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

void kernel_cpu_sse2_convert_to_half_float(KernelGlobals *kg, uchar4 *rgba, float *buffer, float sample_scale, int x, int y, int offset, int stride)
{
	kernel_film_convert_to_half_float(kg, rgba, buffer, sample_scale, x, y, offset, stride);
}

/* Shader Evaluate */

void kernel_cpu_sse2_shader(KernelGlobals *kg, uint4 *input, float4 *output, int type, int i)
{
	kernel_shader_evaluate(kg, input, output, (ShaderEvalType)type, i);
}

CCL_NAMESPACE_END

#else

/* needed for some linkers in combination with scons making empty compilation unit in a library */
void __dummy_function_cycles_sse2(void);
void __dummy_function_cycles_sse2(void) {}

#endif
