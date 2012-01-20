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

/* Optimized CPU kernel entry points. This file is compiled with SSE3
   optimization flags and nearly all functions inlined, while kernel.cpp
   is compiled without for other CPU's. */

#ifdef WITH_OPTIMIZED_KERNEL

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

void kernel_cpu_optimized_path_trace(KernelGlobals *kg, float4 *buffer, unsigned int *rng_state, int sample, int x, int y, int offset, int stride)
{
	kernel_path_trace(kg, buffer, rng_state, sample, x, y, offset, stride);
}

/* Tonemapping */

void kernel_cpu_optimized_tonemap(KernelGlobals *kg, uchar4 *rgba, float4 *buffer, int sample, int resolution, int x, int y, int offset, int stride)
{
	kernel_film_tonemap(kg, rgba, buffer, sample, resolution, x, y, offset, stride);
}

/* Shader Evaluate */

void kernel_cpu_optimized_shader(KernelGlobals *kg, uint4 *input, float4 *output, int type, int i)
{
	kernel_shader_evaluate(kg, input, output, (ShaderEvalType)type, i);
}

CCL_NAMESPACE_END

#endif

