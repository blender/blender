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

#ifndef __KERNEL_H__
#define __KERNEL_H__

/* CPU Kernel Interface */

#include "util_types.h"

CCL_NAMESPACE_BEGIN

struct KernelGlobals;

KernelGlobals *kernel_globals_create();
void kernel_globals_free(KernelGlobals *kg);

void *kernel_osl_memory(KernelGlobals *kg);
bool kernel_osl_use(KernelGlobals *kg);

void kernel_const_copy(KernelGlobals *kg, const char *name, void *host, size_t size);
void kernel_tex_copy(KernelGlobals *kg, const char *name, device_ptr mem, size_t width, size_t height, size_t depth, InterpolationType interpolation=INTERPOLATION_LINEAR);

void kernel_cpu_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state,
	int sample, int x, int y, int offset, int stride);
void kernel_cpu_convert_to_byte(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_convert_to_half_float(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_shader(KernelGlobals *kg, uint4 *input, float4 *output,
	int type, int i);

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
void kernel_cpu_sse2_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state,
	int sample, int x, int y, int offset, int stride);
void kernel_cpu_sse2_convert_to_byte(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_sse2_convert_to_half_float(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_sse2_shader(KernelGlobals *kg, uint4 *input, float4 *output,
	int type, int i);
#endif

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
void kernel_cpu_sse3_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state,
	int sample, int x, int y, int offset, int stride);
void kernel_cpu_sse3_convert_to_byte(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_sse3_convert_to_half_float(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_sse3_shader(KernelGlobals *kg, uint4 *input, float4 *output,
	int type, int i);
#endif

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
void kernel_cpu_sse41_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state,
	int sample, int x, int y, int offset, int stride);
void kernel_cpu_sse41_convert_to_byte(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_sse41_convert_to_half_float(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_sse41_shader(KernelGlobals *kg, uint4 *input, float4 *output,
	int type, int i);
#endif

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
void kernel_cpu_avx_path_trace(KernelGlobals *kg, float *buffer, unsigned int *rng_state,
	int sample, int x, int y, int offset, int stride);
void kernel_cpu_avx_convert_to_byte(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_avx_convert_to_half_float(KernelGlobals *kg, uchar4 *rgba, float *buffer,
	float sample_scale, int x, int y, int offset, int stride);
void kernel_cpu_avx_shader(KernelGlobals *kg, uint4 *input, float4 *output,
	int type, int i);
#endif

CCL_NAMESPACE_END

#endif /* __KERNEL_H__ */

