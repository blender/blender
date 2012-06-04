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

#ifndef __OSL_SHADER_H__
#define __OSL_SHADER_H__

#ifdef WITH_OSL

/* OSL Shader Engine
 *
 * Holds all variables to execute and use OSL shaders from the kernel. These
 * are initialized externally by OSLShaderManager before rendering starts.
 *
 * Before/after a thread starts rendering, thread_init/thread_free must be
 * called, which will store any per thread OSL state in thread local storage.
 * This means no thread state must be passed along in the kernel itself.
 */

#include <OSL/oslexec.h>
#include <OSL/oslclosure.h>

#include "kernel_types.h"

#include "util_map.h"
#include "util_param.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

namespace OSL = ::OSL;

class OSLRenderServices;
class Scene;
class ShaderClosure;
class ShaderData;
class differential3;
class KernelGlobals;

class OSLShader {
public:
	/* init */
	static void register_closures(OSL::ShadingSystem *ss);

	/* per thread data */
	static void thread_init(KernelGlobals *kg);
	static void thread_free(KernelGlobals *kg);

	/* eval */
	static void eval_surface(KernelGlobals *kg, ShaderData *sd, float randb, int path_flag);
	static float3 eval_background(KernelGlobals *kg, ShaderData *sd, int path_flag);
	static void eval_volume(KernelGlobals *kg, ShaderData *sd, float randb, int path_flag);
	static void eval_displacement(KernelGlobals *kg, ShaderData *sd);

	/* sample & eval */
	static int bsdf_sample(const ShaderData *sd, const ShaderClosure *sc,
	                       float randu, float randv,
	                       float3& eval, float3& omega_in, differential3& domega_in, float& pdf);
	static float3 bsdf_eval(const ShaderData *sd, const ShaderClosure *sc,
	                        const float3& omega_in, float& pdf);

	static float3 emissive_eval(const ShaderData *sd, const ShaderClosure *sc);

	static float3 volume_eval_phase(const ShaderData *sd, const ShaderClosure *sc,
	                                const float3 omega_in, const float3 omega_out);

	/* release */
	static void release(KernelGlobals *kg, const ShaderData *sd);
};

CCL_NAMESPACE_END

#endif

#endif /* __OSL_SHADER_H__ */

