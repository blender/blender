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

#ifndef __SVM_H__
#define __SVM_H__

/* Shader Virtual Machine
 *
 * A shader is a list of nodes to be executed. These are simply read one after
 * the other and executed, using an node counter. Each node and it's associated
 * data is encoded as one or more uint4's in a 1D texture. If the data is larger
 * than an uint4, the node can increase the node counter to compensate for this.
 * Floats are encoded as int and then converted to float again.
 *
 * Nodes write their output into a stack. All stack data in the stack is
 * floats, since it's all factors, colors and vectors. The stack will be stored
 * in local memory on the GPU, as it would take too many register and indexes in
 * ways not known at compile time. This seems the only solution even though it
 * may be slow, with two positive factors. If the same shader is being executed,
 * memory access will be coalesced, and on fermi cards, memory will actually be
 * cached.
 *
 * The result of shader execution will be a single closure. This means the
 * closure type, associated label, data and weight. Sampling from multiple
 * closures is supported through the mix closure node, the logic for that is
 * mostly taken care of in the SVM compiler.
 */

#include "svm_types.h"

CCL_NAMESPACE_BEGIN

/* Stack */

__device_inline float3 stack_load_float3(float *stack, uint a)
{
	kernel_assert(a+2 < SVM_STACK_SIZE);

	return make_float3(stack[a+0], stack[a+1], stack[a+2]);
}

__device_inline void stack_store_float3(float *stack, uint a, float3 f)
{
	kernel_assert(a+2 < SVM_STACK_SIZE);

	stack[a+0] = f.x;
	stack[a+1] = f.y;
	stack[a+2] = f.z;
}

__device_inline float stack_load_float(float *stack, uint a)
{
	kernel_assert(a < SVM_STACK_SIZE);

	return stack[a];
}

__device_inline float stack_load_float_default(float *stack, uint a, uint value)
{
	return (a == (uint)SVM_STACK_INVALID)? __int_as_float(value): stack_load_float(stack, a);
}

__device_inline void stack_store_float(float *stack, uint a, float f)
{
	kernel_assert(a < SVM_STACK_SIZE);

	stack[a] = f;
}

__device_inline bool stack_valid(uint a)
{
	return a != (uint)SVM_STACK_INVALID;
}

/* Reading Nodes */

__device_inline uint4 read_node(KernelGlobals *kg, int *offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
	(*offset)++;
	return node;
}

__device_inline float4 read_node_float(KernelGlobals *kg, int *offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
	float4 f = make_float4(__int_as_float(node.x), __int_as_float(node.y), __int_as_float(node.z), __int_as_float(node.w));
	(*offset)++;
	return f;
}

__device_inline float4 fetch_node_float(KernelGlobals *kg, int offset)
{
	uint4 node = kernel_tex_fetch(__svm_nodes, offset);
	return make_float4(__int_as_float(node.x), __int_as_float(node.y), __int_as_float(node.z), __int_as_float(node.w));
}

__device_inline void decode_node_uchar4(uint i, uint *x, uint *y, uint *z, uint *w)
{
	if(x) *x = (i & 0xFF);
	if(y) *y = ((i >> 8) & 0xFF);
	if(z) *z = ((i >> 16) & 0xFF);
	if(w) *w = ((i >> 24) & 0xFF);
}

CCL_NAMESPACE_END

/* Nodes */

#include "svm_noise.h"
#include "svm_texture.h"

#include "svm_attribute.h"
#include "svm_gradient.h"
#include "svm_closure.h"
#include "svm_noisetex.h"
#include "svm_convert.h"
#include "svm_displace.h"
#include "svm_fresnel.h"
#include "svm_camera.h"
#include "svm_geometry.h"
#include "svm_hsv.h"
#include "svm_image.h"
#include "svm_gamma.h"
#include "svm_brightness.h"
#include "svm_invert.h"
#include "svm_light_path.h"
#include "svm_magic.h"
#include "svm_mapping.h"
#include "svm_normal.h"
#include "svm_wave.h"
#include "svm_math.h"
#include "svm_mix.h"
#include "svm_ramp.h"
#include "svm_sepcomb_rgb.h"
#include "svm_musgrave.h"
#include "svm_sky.h"
#include "svm_tex_coord.h"
#include "svm_value.h"
#include "svm_voronoi.h"
#include "svm_checker.h"

CCL_NAMESPACE_BEGIN

/* Main Interpreter Loop */

__device_noinline void svm_eval_nodes(KernelGlobals *kg, ShaderData *sd, ShaderType type, float randb, int path_flag)
{
	float stack[SVM_STACK_SIZE];
	float closure_weight = 1.0f;
	int offset = sd->shader & SHADER_MASK;

#ifdef __MULTI_CLOSURE__
	sd->num_closure = 0;
	sd->randb_closure = randb;
#else
	sd->closure.type = NBUILTIN_CLOSURES;
#endif

	while(1) {
		uint4 node = read_node(kg, &offset);

		switch(node.x) {
			case NODE_SHADER_JUMP: {
				if(type == SHADER_TYPE_SURFACE) offset = node.y;
				else if(type == SHADER_TYPE_VOLUME) offset = node.z;
				else if(type == SHADER_TYPE_DISPLACEMENT) offset = node.w;
				else return;
				break;
			}
			case NODE_CLOSURE_BSDF:
				svm_node_closure_bsdf(kg, sd, stack, node, randb, path_flag);
				break;
			case NODE_CLOSURE_EMISSION:
				svm_node_closure_emission(sd, stack, node);
				break;
			case NODE_CLOSURE_BACKGROUND:
				svm_node_closure_background(sd, stack, node);
				break;
			case NODE_CLOSURE_HOLDOUT:
				svm_node_closure_holdout(sd, stack, node);
				break;
			case NODE_CLOSURE_VOLUME:
				svm_node_closure_volume(kg, sd, stack, node, path_flag);
				break;
			case NODE_CLOSURE_SET_WEIGHT:
				svm_node_closure_set_weight(sd, node.y, node.z, node.w);
				break;
			case NODE_CLOSURE_WEIGHT:
				svm_node_closure_weight(sd, stack, node.y);
				break;
			case NODE_EMISSION_WEIGHT:
				svm_node_emission_weight(kg, sd, stack, node);
				break;
			case NODE_MIX_CLOSURE:
				svm_node_mix_closure(sd, stack, node, &offset, &randb);
				break;
			case NODE_ADD_CLOSURE:
				svm_node_add_closure(sd, stack, node.y, node.z, &offset, &randb, &closure_weight);
				break;
			case NODE_JUMP:
				offset = node.y;
				break;
#ifdef __IMAGE_TEXTURES__
			case NODE_TEX_IMAGE:
				svm_node_tex_image(kg, sd, stack, node);
				break;
			case NODE_TEX_ENVIRONMENT:
				svm_node_tex_environment(kg, sd, stack, node);
				break;
#endif
#ifdef __PROCEDURAL_TEXTURES__
			case NODE_TEX_SKY:
				svm_node_tex_sky(kg, sd, stack, node.y, node.z);
				break;
			case NODE_TEX_GRADIENT:
				svm_node_tex_gradient(sd, stack, node);
				break;
			case NODE_TEX_NOISE:
				svm_node_tex_noise(kg, sd, stack, node, &offset);
				break;
			case NODE_TEX_VORONOI:
				svm_node_tex_voronoi(kg, sd, stack, node, &offset);
				break;
			case NODE_TEX_MUSGRAVE:
				svm_node_tex_musgrave(kg, sd, stack, node, &offset);
				break;
			case NODE_TEX_WAVE:
				svm_node_tex_wave(kg, sd, stack, node, &offset);
				break;
			case NODE_TEX_MAGIC:
				svm_node_tex_magic(kg, sd, stack, node, &offset);
				break;
			case NODE_TEX_CHECKER:
				svm_node_tex_checker(kg, sd, stack, node, &offset);
				break;
#endif
			case NODE_CAMERA:
				svm_node_camera(kg, sd, stack, node.y, node.z, node.w);
				break;
			case NODE_GEOMETRY:
				svm_node_geometry(sd, stack, node.y, node.z);
				break;
#ifdef __EXTRA_NODES__
			case NODE_GEOMETRY_BUMP_DX:
				svm_node_geometry_bump_dx(sd, stack, node.y, node.z);
				break;
			case NODE_GEOMETRY_BUMP_DY:
				svm_node_geometry_bump_dy(sd, stack, node.y, node.z);
				break;
			case NODE_LIGHT_PATH:
				svm_node_light_path(sd, stack, node.y, node.z, path_flag);
				break;
#endif
			case NODE_CONVERT:
				svm_node_convert(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_VALUE_F:
				svm_node_value_f(kg, sd, stack, node.y, node.z);
				break;
			case NODE_VALUE_V:
				svm_node_value_v(kg, sd, stack, node.y, &offset);
				break;
#ifdef __EXTRA_NODES__
			case NODE_INVERT:
				svm_node_invert(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_GAMMA:
				svm_node_gamma(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_BRIGHTCONTRAST:
				svm_node_brightness(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_MIX:
				svm_node_mix(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
			case NODE_SEPARATE_RGB:
				svm_node_separate_rgb(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_COMBINE_RGB:
				svm_node_combine_rgb(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_HSV:
				svm_node_hsv(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
			case NODE_ATTR:
				svm_node_attr(kg, sd, stack, node);
				break;
#ifdef __EXTRA_NODES__
			case NODE_ATTR_BUMP_DX:
				svm_node_attr_bump_dx(kg, sd, stack, node);
				break;
			case NODE_ATTR_BUMP_DY:
				svm_node_attr_bump_dy(kg, sd, stack, node);
				break;
#endif
			case NODE_FRESNEL:
				svm_node_fresnel(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_LAYER_WEIGHT:
				svm_node_layer_weight(sd, stack, node);
				break;
#ifdef __EXTRA_NODES__
			case NODE_SET_DISPLACEMENT:
				svm_node_set_displacement(sd, stack, node.y);
				break;
			case NODE_SET_BUMP:
				svm_node_set_bump(sd, stack, node.y, node.z, node.w);
				break;
			case NODE_MATH:
				svm_node_math(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
			case NODE_VECTOR_MATH:
				svm_node_vector_math(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
			case NODE_NORMAL:
				svm_node_normal(kg, sd, stack, node.y, node.z, node.w, &offset);
				break;
#endif
			case NODE_MAPPING:
				svm_node_mapping(kg, sd, stack, node.y, node.z, &offset);
				break;
			case NODE_MIN_MAX:
				svm_node_min_max(kg, sd, stack, node.y, node.z, &offset);
				break;
			case NODE_TEX_COORD:
				svm_node_tex_coord(kg, sd, stack, node.y, node.z);
				break;
#ifdef __EXTRA_NODES__
			case NODE_TEX_COORD_BUMP_DX:
				svm_node_tex_coord_bump_dx(kg, sd, stack, node.y, node.z);
				break;
			case NODE_TEX_COORD_BUMP_DY:
				svm_node_tex_coord_bump_dy(kg, sd, stack, node.y, node.z);
				break;
#endif
			case NODE_EMISSION_SET_WEIGHT_TOTAL:
				svm_node_emission_set_weight_total(kg, sd, node.y, node.z, node.w);
				break;
#ifdef __EXTRA_NODES__
			case NODE_RGB_RAMP:
				svm_node_rgb_ramp(kg, sd, stack, node, &offset);
				break;
			case NODE_RGB_CURVES:
				svm_node_rgb_curves(kg, sd, stack, node, &offset);
				break;
			case NODE_LIGHT_FALLOFF:
				svm_node_light_falloff(sd, stack, node);
				break;
#endif
			case NODE_END:
			default:
#ifndef __MULTI_CLOSURE__
				sd->closure.weight *= closure_weight;
#endif
				return;
		}
	}
}

CCL_NAMESPACE_END

#endif /* __SVM_H__ */

