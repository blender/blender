/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __KERNEL_H__
#define __KERNEL_H__

/* CPU Kernel Interfae */

#include "util_types.h"

CCL_NAMESPACE_BEGIN

struct KernelGlobals;

KernelGlobals *kernel_globals_create();
void kernel_globals_free(KernelGlobals *kg);

void *kernel_osl_memory(KernelGlobals *kg);
bool kernel_osl_use(KernelGlobals *kg);

void kernel_const_copy(KernelGlobals *kg, const char *name, void *host, size_t size);
void kernel_tex_copy(KernelGlobals *kg, const char *name, device_ptr mem, size_t width, size_t height);

void kernel_cpu_path_trace(KernelGlobals *kg, float4 *buffer, unsigned int *rng_state,
	int sample, int x, int y, int offset, int stride);
void kernel_cpu_tonemap(KernelGlobals *kg, uchar4 *rgba, float4 *buffer,
	int sample, int resolution, int x, int y, int offset, int stride);
void kernel_cpu_shader(KernelGlobals *kg, uint4 *input, float3 *output,
	int type, int i);

#ifdef WITH_OPTIMIZED_KERNEL
void kernel_cpu_optimized_path_trace(KernelGlobals *kg, float4 *buffer, unsigned int *rng_state,
	int sample, int x, int y, int offset, int stride);
void kernel_cpu_optimized_tonemap(KernelGlobals *kg, uchar4 *rgba, float4 *buffer,
	int sample, int resolution, int x, int y, int offset, int stride);
void kernel_cpu_optimized_shader(KernelGlobals *kg, uint4 *input, float3 *output,
	int type, int i);
#endif

CCL_NAMESPACE_END

#endif /* __KERNEL_H__ */

