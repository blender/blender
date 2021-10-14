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
 * limitations under the License.
 */

#ifndef __SVM_H__
#define __SVM_H__

/* Shader Virtual Machine
 *
 * A shader is a list of nodes to be executed. These are simply read one after
 * the other and executed, using an node counter. Each node and its associated
 * data is encoded as one or more uint4's in a 1D texture. If the data is larger
 * than an uint4, the node can increase the node counter to compensate for this.
 * Floats are encoded as int and then converted to float again.
 *
 * Nodes write their output into a stack. All stack data in the stack is
 * floats, since it's all factors, colors and vectors. The stack will be stored
 * in local memory on the GPU, as it would take too many register and indexes in
 * ways not known at compile time. This seems the only solution even though it
 * may be slow, with two positive factors. If the same shader is being executed,
 * memory access will be coalesced and cached.
 *
 * The result of shader execution will be a single closure. This means the
 * closure type, associated label, data and weight. Sampling from multiple
 * closures is supported through the mix closure node, the logic for that is
 * mostly taken care of in the SVM compiler.
 */

#include "kernel/svm/svm_types.h"

CCL_NAMESPACE_BEGIN

/* Stack */

ccl_device_inline float3 stack_load_float3(ccl_private float *stack, uint a)
{
  kernel_assert(a + 2 < SVM_STACK_SIZE);

  ccl_private float *stack_a = stack + a;
  return make_float3(stack_a[0], stack_a[1], stack_a[2]);
}

ccl_device_inline void stack_store_float3(ccl_private float *stack, uint a, float3 f)
{
  kernel_assert(a + 2 < SVM_STACK_SIZE);

  ccl_private float *stack_a = stack + a;
  stack_a[0] = f.x;
  stack_a[1] = f.y;
  stack_a[2] = f.z;
}

ccl_device_inline float stack_load_float(ccl_private float *stack, uint a)
{
  kernel_assert(a < SVM_STACK_SIZE);

  return stack[a];
}

ccl_device_inline float stack_load_float_default(ccl_private float *stack, uint a, uint value)
{
  return (a == (uint)SVM_STACK_INVALID) ? __uint_as_float(value) : stack_load_float(stack, a);
}

ccl_device_inline void stack_store_float(ccl_private float *stack, uint a, float f)
{
  kernel_assert(a < SVM_STACK_SIZE);

  stack[a] = f;
}

ccl_device_inline int stack_load_int(ccl_private float *stack, uint a)
{
  kernel_assert(a < SVM_STACK_SIZE);

  return __float_as_int(stack[a]);
}

ccl_device_inline int stack_load_int_default(ccl_private float *stack, uint a, uint value)
{
  return (a == (uint)SVM_STACK_INVALID) ? (int)value : stack_load_int(stack, a);
}

ccl_device_inline void stack_store_int(ccl_private float *stack, uint a, int i)
{
  kernel_assert(a < SVM_STACK_SIZE);

  stack[a] = __int_as_float(i);
}

ccl_device_inline bool stack_valid(uint a)
{
  return a != (uint)SVM_STACK_INVALID;
}

/* Reading Nodes */

ccl_device_inline uint4 read_node(ccl_global const KernelGlobals *kg, ccl_private int *offset)
{
  uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
  (*offset)++;
  return node;
}

ccl_device_inline float4 read_node_float(ccl_global const KernelGlobals *kg,
                                         ccl_private int *offset)
{
  uint4 node = kernel_tex_fetch(__svm_nodes, *offset);
  float4 f = make_float4(__uint_as_float(node.x),
                         __uint_as_float(node.y),
                         __uint_as_float(node.z),
                         __uint_as_float(node.w));
  (*offset)++;
  return f;
}

ccl_device_inline float4 fetch_node_float(ccl_global const KernelGlobals *kg, int offset)
{
  uint4 node = kernel_tex_fetch(__svm_nodes, offset);
  return make_float4(__uint_as_float(node.x),
                     __uint_as_float(node.y),
                     __uint_as_float(node.z),
                     __uint_as_float(node.w));
}

ccl_device_forceinline void svm_unpack_node_uchar2(uint i,
                                                   ccl_private uint *x,
                                                   ccl_private uint *y)
{
  *x = (i & 0xFF);
  *y = ((i >> 8) & 0xFF);
}

ccl_device_forceinline void svm_unpack_node_uchar3(uint i,
                                                   ccl_private uint *x,
                                                   ccl_private uint *y,
                                                   ccl_private uint *z)
{
  *x = (i & 0xFF);
  *y = ((i >> 8) & 0xFF);
  *z = ((i >> 16) & 0xFF);
}

ccl_device_forceinline void svm_unpack_node_uchar4(
    uint i, ccl_private uint *x, ccl_private uint *y, ccl_private uint *z, ccl_private uint *w)
{
  *x = (i & 0xFF);
  *y = ((i >> 8) & 0xFF);
  *z = ((i >> 16) & 0xFF);
  *w = ((i >> 24) & 0xFF);
}

CCL_NAMESPACE_END

/* Nodes */

#include "kernel/svm/svm_noise.h"
#include "svm_fractal_noise.h"

#include "kernel/svm/svm_color_util.h"
#include "kernel/svm/svm_mapping_util.h"
#include "kernel/svm/svm_math_util.h"

#include "kernel/svm/svm_aov.h"
#include "kernel/svm/svm_attribute.h"
#include "kernel/svm/svm_blackbody.h"
#include "kernel/svm/svm_brick.h"
#include "kernel/svm/svm_brightness.h"
#include "kernel/svm/svm_bump.h"
#include "kernel/svm/svm_camera.h"
#include "kernel/svm/svm_checker.h"
#include "kernel/svm/svm_clamp.h"
#include "kernel/svm/svm_closure.h"
#include "kernel/svm/svm_convert.h"
#include "kernel/svm/svm_displace.h"
#include "kernel/svm/svm_fresnel.h"
#include "kernel/svm/svm_gamma.h"
#include "kernel/svm/svm_geometry.h"
#include "kernel/svm/svm_gradient.h"
#include "kernel/svm/svm_hsv.h"
#include "kernel/svm/svm_ies.h"
#include "kernel/svm/svm_image.h"
#include "kernel/svm/svm_invert.h"
#include "kernel/svm/svm_light_path.h"
#include "kernel/svm/svm_magic.h"
#include "kernel/svm/svm_map_range.h"
#include "kernel/svm/svm_mapping.h"
#include "kernel/svm/svm_math.h"
#include "kernel/svm/svm_mix.h"
#include "kernel/svm/svm_musgrave.h"
#include "kernel/svm/svm_noisetex.h"
#include "kernel/svm/svm_normal.h"
#include "kernel/svm/svm_ramp.h"
#include "kernel/svm/svm_sepcomb_hsv.h"
#include "kernel/svm/svm_sepcomb_vector.h"
#include "kernel/svm/svm_sky.h"
#include "kernel/svm/svm_tex_coord.h"
#include "kernel/svm/svm_value.h"
#include "kernel/svm/svm_vector_rotate.h"
#include "kernel/svm/svm_vector_transform.h"
#include "kernel/svm/svm_vertex_color.h"
#include "kernel/svm/svm_voronoi.h"
#include "kernel/svm/svm_voxel.h"
#include "kernel/svm/svm_wave.h"
#include "kernel/svm/svm_wavelength.h"
#include "kernel/svm/svm_white_noise.h"
#include "kernel/svm/svm_wireframe.h"

#ifdef __SHADER_RAYTRACE__
#  include "kernel/svm/svm_ao.h"
#  include "kernel/svm/svm_bevel.h"
#endif

CCL_NAMESPACE_BEGIN

/* Main Interpreter Loop */
template<uint node_feature_mask, ShaderType type>
ccl_device void svm_eval_nodes(INTEGRATOR_STATE_CONST_ARGS,
                               ShaderData *sd,
                               ccl_global float *render_buffer,
                               int path_flag)
{
  float stack[SVM_STACK_SIZE];
  int offset = sd->shader & SHADER_MASK;

  while (1) {
    uint4 node = read_node(kg, &offset);

    switch (node.x) {
      case NODE_END:
        return;
      case NODE_SHADER_JUMP: {
        if (type == SHADER_TYPE_SURFACE)
          offset = node.y;
        else if (type == SHADER_TYPE_VOLUME)
          offset = node.z;
        else if (type == SHADER_TYPE_DISPLACEMENT)
          offset = node.w;
        else
          return;
        break;
      }
      case NODE_CLOSURE_BSDF:
        offset = svm_node_closure_bsdf<node_feature_mask, type>(
            kg, sd, stack, node, path_flag, offset);
        break;
      case NODE_CLOSURE_EMISSION:
        if (KERNEL_NODES_FEATURE(EMISSION)) {
          svm_node_closure_emission(sd, stack, node);
        }
        break;
      case NODE_CLOSURE_BACKGROUND:
        if (KERNEL_NODES_FEATURE(EMISSION)) {
          svm_node_closure_background(sd, stack, node);
        }
        break;
      case NODE_CLOSURE_SET_WEIGHT:
        svm_node_closure_set_weight(sd, node.y, node.z, node.w);
        break;
      case NODE_CLOSURE_WEIGHT:
        svm_node_closure_weight(sd, stack, node.y);
        break;
      case NODE_EMISSION_WEIGHT:
        if (KERNEL_NODES_FEATURE(EMISSION)) {
          svm_node_emission_weight(kg, sd, stack, node);
        }
        break;
      case NODE_MIX_CLOSURE:
        svm_node_mix_closure(sd, stack, node);
        break;
      case NODE_JUMP_IF_ZERO:
        if (stack_load_float(stack, node.z) == 0.0f)
          offset += node.y;
        break;
      case NODE_JUMP_IF_ONE:
        if (stack_load_float(stack, node.z) == 1.0f)
          offset += node.y;
        break;
      case NODE_GEOMETRY:
        svm_node_geometry(kg, sd, stack, node.y, node.z);
        break;
      case NODE_CONVERT:
        svm_node_convert(kg, sd, stack, node.y, node.z, node.w);
        break;
      case NODE_TEX_COORD:
        offset = svm_node_tex_coord(kg, sd, path_flag, stack, node, offset);
        break;
      case NODE_VALUE_F:
        svm_node_value_f(kg, sd, stack, node.y, node.z);
        break;
      case NODE_VALUE_V:
        offset = svm_node_value_v(kg, sd, stack, node.y, offset);
        break;
      case NODE_ATTR:
        svm_node_attr<node_feature_mask>(kg, sd, stack, node);
        break;
      case NODE_VERTEX_COLOR:
        svm_node_vertex_color(kg, sd, stack, node.y, node.z, node.w);
        break;
      case NODE_GEOMETRY_BUMP_DX:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_geometry_bump_dx(kg, sd, stack, node.y, node.z);
        }
        break;
      case NODE_GEOMETRY_BUMP_DY:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_geometry_bump_dy(kg, sd, stack, node.y, node.z);
        }
        break;
      case NODE_SET_DISPLACEMENT:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_set_displacement(kg, sd, stack, node.y);
        }
        break;
      case NODE_DISPLACEMENT:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_displacement(kg, sd, stack, node);
        }
        break;
      case NODE_VECTOR_DISPLACEMENT:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          offset = svm_node_vector_displacement(kg, sd, stack, node, offset);
        }
        break;
      case NODE_TEX_IMAGE:
        offset = svm_node_tex_image(kg, sd, stack, node, offset);
        break;
      case NODE_TEX_IMAGE_BOX:
        svm_node_tex_image_box(kg, sd, stack, node);
        break;
      case NODE_TEX_NOISE:
        offset = svm_node_tex_noise(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_SET_BUMP:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_set_bump(kg, sd, stack, node);
        }
        break;
      case NODE_ATTR_BUMP_DX:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_attr_bump_dx(kg, sd, stack, node);
        }
        break;
      case NODE_ATTR_BUMP_DY:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_attr_bump_dy(kg, sd, stack, node);
        }
        break;
      case NODE_VERTEX_COLOR_BUMP_DX:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_vertex_color_bump_dx(kg, sd, stack, node.y, node.z, node.w);
        }
        break;
      case NODE_VERTEX_COLOR_BUMP_DY:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_vertex_color_bump_dy(kg, sd, stack, node.y, node.z, node.w);
        }
        break;
      case NODE_TEX_COORD_BUMP_DX:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          offset = svm_node_tex_coord_bump_dx(kg, sd, path_flag, stack, node, offset);
        }
        break;
      case NODE_TEX_COORD_BUMP_DY:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          offset = svm_node_tex_coord_bump_dy(kg, sd, path_flag, stack, node, offset);
        }
        break;
      case NODE_CLOSURE_SET_NORMAL:
        if (KERNEL_NODES_FEATURE(BUMP)) {
          svm_node_set_normal(kg, sd, stack, node.y, node.z);
        }
        break;
      case NODE_ENTER_BUMP_EVAL:
        if (KERNEL_NODES_FEATURE(BUMP_STATE)) {
          svm_node_enter_bump_eval(kg, sd, stack, node.y);
        }
        break;
      case NODE_LEAVE_BUMP_EVAL:
        if (KERNEL_NODES_FEATURE(BUMP_STATE)) {
          svm_node_leave_bump_eval(kg, sd, stack, node.y);
        }
        break;
      case NODE_HSV:
        svm_node_hsv(kg, sd, stack, node);
        break;

      case NODE_CLOSURE_HOLDOUT:
        svm_node_closure_holdout(sd, stack, node);
        break;
      case NODE_FRESNEL:
        svm_node_fresnel(sd, stack, node.y, node.z, node.w);
        break;
      case NODE_LAYER_WEIGHT:
        svm_node_layer_weight(sd, stack, node);
        break;
      case NODE_CLOSURE_VOLUME:
        if (KERNEL_NODES_FEATURE(VOLUME)) {
          svm_node_closure_volume<type>(kg, sd, stack, node);
        }
        break;
      case NODE_PRINCIPLED_VOLUME:
        if (KERNEL_NODES_FEATURE(VOLUME)) {
          offset = svm_node_principled_volume<type>(kg, sd, stack, node, path_flag, offset);
        }
        break;
      case NODE_MATH:
        svm_node_math(kg, sd, stack, node.y, node.z, node.w);
        break;
      case NODE_VECTOR_MATH:
        offset = svm_node_vector_math(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_RGB_RAMP:
        offset = svm_node_rgb_ramp(kg, sd, stack, node, offset);
        break;
      case NODE_GAMMA:
        svm_node_gamma(sd, stack, node.y, node.z, node.w);
        break;
      case NODE_BRIGHTCONTRAST:
        svm_node_brightness(sd, stack, node.y, node.z, node.w);
        break;
      case NODE_LIGHT_PATH:
        svm_node_light_path(INTEGRATOR_STATE_PASS, sd, stack, node.y, node.z, path_flag);
        break;
      case NODE_OBJECT_INFO:
        svm_node_object_info(kg, sd, stack, node.y, node.z);
        break;
      case NODE_PARTICLE_INFO:
        svm_node_particle_info(kg, sd, stack, node.y, node.z);
        break;
#if defined(__HAIR__)
      case NODE_HAIR_INFO:
        if (KERNEL_NODES_FEATURE(HAIR)) {
          svm_node_hair_info(kg, sd, stack, node.y, node.z);
        }
        break;
#endif

      case NODE_TEXTURE_MAPPING:
        offset = svm_node_texture_mapping(kg, sd, stack, node.y, node.z, offset);
        break;
      case NODE_MAPPING:
        svm_node_mapping(kg, sd, stack, node.y, node.z, node.w);
        break;
      case NODE_MIN_MAX:
        offset = svm_node_min_max(kg, sd, stack, node.y, node.z, offset);
        break;
      case NODE_CAMERA:
        svm_node_camera(kg, sd, stack, node.y, node.z, node.w);
        break;
      case NODE_TEX_ENVIRONMENT:
        svm_node_tex_environment(kg, sd, stack, node);
        break;
      case NODE_TEX_SKY:
        offset = svm_node_tex_sky(kg, sd, stack, node, offset);
        break;
      case NODE_TEX_GRADIENT:
        svm_node_tex_gradient(sd, stack, node);
        break;
      case NODE_TEX_VORONOI:
        offset = svm_node_tex_voronoi<node_feature_mask>(
            kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_TEX_MUSGRAVE:
        offset = svm_node_tex_musgrave(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_TEX_WAVE:
        offset = svm_node_tex_wave(kg, sd, stack, node, offset);
        break;
      case NODE_TEX_MAGIC:
        offset = svm_node_tex_magic(kg, sd, stack, node, offset);
        break;
      case NODE_TEX_CHECKER:
        svm_node_tex_checker(kg, sd, stack, node);
        break;
      case NODE_TEX_BRICK:
        offset = svm_node_tex_brick(kg, sd, stack, node, offset);
        break;
      case NODE_TEX_WHITE_NOISE:
        svm_node_tex_white_noise(kg, sd, stack, node.y, node.z, node.w);
        break;
      case NODE_NORMAL:
        offset = svm_node_normal(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_LIGHT_FALLOFF:
        svm_node_light_falloff(sd, stack, node);
        break;
      case NODE_IES:
        svm_node_ies(kg, sd, stack, node);
        break;
      case NODE_RGB_CURVES:
      case NODE_VECTOR_CURVES:
        offset = svm_node_curves(kg, sd, stack, node, offset);
        break;
      case NODE_FLOAT_CURVE:
        offset = svm_node_curve(kg, sd, stack, node, offset);
        break;
      case NODE_TANGENT:
        svm_node_tangent(kg, sd, stack, node);
        break;
      case NODE_NORMAL_MAP:
        svm_node_normal_map(kg, sd, stack, node);
        break;
      case NODE_INVERT:
        svm_node_invert(sd, stack, node.y, node.z, node.w);
        break;
      case NODE_MIX:
        offset = svm_node_mix(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_SEPARATE_VECTOR:
        svm_node_separate_vector(sd, stack, node.y, node.z, node.w);
        break;
      case NODE_COMBINE_VECTOR:
        svm_node_combine_vector(sd, stack, node.y, node.z, node.w);
        break;
      case NODE_SEPARATE_HSV:
        offset = svm_node_separate_hsv(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_COMBINE_HSV:
        offset = svm_node_combine_hsv(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_VECTOR_ROTATE:
        svm_node_vector_rotate(sd, stack, node.y, node.z, node.w);
        break;
      case NODE_VECTOR_TRANSFORM:
        svm_node_vector_transform(kg, sd, stack, node);
        break;
      case NODE_WIREFRAME:
        svm_node_wireframe(kg, sd, stack, node);
        break;
      case NODE_WAVELENGTH:
        svm_node_wavelength(kg, sd, stack, node.y, node.z);
        break;
      case NODE_BLACKBODY:
        svm_node_blackbody(kg, sd, stack, node.y, node.z);
        break;
      case NODE_MAP_RANGE:
        offset = svm_node_map_range(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
      case NODE_CLAMP:
        offset = svm_node_clamp(kg, sd, stack, node.y, node.z, node.w, offset);
        break;
#ifdef __SHADER_RAYTRACE__
      case NODE_BEVEL:
        svm_node_bevel<node_feature_mask>(INTEGRATOR_STATE_PASS, sd, stack, node);
        break;
      case NODE_AMBIENT_OCCLUSION:
        svm_node_ao<node_feature_mask>(INTEGRATOR_STATE_PASS, sd, stack, node);
        break;
#endif

      case NODE_TEX_VOXEL:
        if (KERNEL_NODES_FEATURE(VOLUME)) {
          offset = svm_node_tex_voxel(kg, sd, stack, node, offset);
        }
        break;
      case NODE_AOV_START:
        if (!svm_node_aov_check(path_flag, render_buffer)) {
          return;
        }
        break;
      case NODE_AOV_COLOR:
        svm_node_aov_color(INTEGRATOR_STATE_PASS, sd, stack, node, render_buffer);
        break;
      case NODE_AOV_VALUE:
        svm_node_aov_value(INTEGRATOR_STATE_PASS, sd, stack, node, render_buffer);
        break;
      default:
        kernel_assert(!"Unknown node type was passed to the SVM machine");
        return;
    }
  }
}

CCL_NAMESPACE_END

#endif /* __SVM_H__ */
