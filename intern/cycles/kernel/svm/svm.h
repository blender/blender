/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

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

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/svm/types.h"
#include "kernel/svm/util.h"

/* Nodes */

#include "kernel/svm/aov.h"
#include "kernel/svm/attribute.h"
#include "kernel/svm/blackbody.h"
#include "kernel/svm/brick.h"
#include "kernel/svm/brightness.h"
#include "kernel/svm/bump.h"
#include "kernel/svm/camera.h"
#include "kernel/svm/checker.h"
#include "kernel/svm/clamp.h"
#include "kernel/svm/closure.h"
#include "kernel/svm/convert.h"
#include "kernel/svm/displace.h"
#include "kernel/svm/fresnel.h"
#include "kernel/svm/gabor.h"
#include "kernel/svm/gamma.h"
#include "kernel/svm/geometry.h"
#include "kernel/svm/gradient.h"
#include "kernel/svm/hsv.h"
#include "kernel/svm/ies.h"
#include "kernel/svm/image.h"
#include "kernel/svm/invert.h"
#include "kernel/svm/light_path.h"
#include "kernel/svm/magic.h"
#include "kernel/svm/map_range.h"
#include "kernel/svm/mapping.h"
#include "kernel/svm/math.h"
#include "kernel/svm/mix.h"
#include "kernel/svm/noisetex.h"
#include "kernel/svm/normal.h"
#include "kernel/svm/radial_tiling.h"
#include "kernel/svm/ramp.h"
#include "kernel/svm/sepcomb_color.h"
#include "kernel/svm/sepcomb_vector.h"
#include "kernel/svm/sky.h"
#include "kernel/svm/tex_coord.h"
#include "kernel/svm/value.h"
#include "kernel/svm/vector_rotate.h"
#include "kernel/svm/vector_transform.h"
#include "kernel/svm/vertex_color.h"
#include "kernel/svm/voronoi.h"
#include "kernel/svm/wave.h"
#include "kernel/svm/wavelength.h"
#include "kernel/svm/white_noise.h"
#include "kernel/svm/wireframe.h"
#include "util/defines.h"

#ifdef __SHADER_RAYTRACE__
#  include "kernel/svm/ao.h"
#  include "kernel/svm/bevel.h"
#  include "kernel/svm/raycast.h"
#endif

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_USE_DATA_CONSTANTS__
#  define SVM_CASE(node) \
    case node: \
      if (!kernel_data_svm_usage_##node) \
        break;
#else
#  define SVM_CASE(node) case node:
#endif

/* Main Interpreter Loop */
template<uint node_feature_mask, ShaderType type, typename ConstIntegratorGenericState>
ccl_device void svm_eval_nodes(KernelGlobals kg,
                               ConstIntegratorGenericState state,
                               ccl_private ShaderData *sd,
                               ccl_global float *render_buffer,
                               const uint32_t path_flag)
{
  float stack[SVM_STACK_SIZE];
  /* Initialize to silence (false positive?) warning about uninitialized use on Windows. */
  Spectrum closure_weight = zero_spectrum();
  int offset = (sd->shader & SHADER_MASK) * (1 + sizeof(SVMNodeShaderJump) / sizeof(uint));

  while (true) {
    const uint node_type = kernel_data_fetch(svm_nodes, offset++);

    switch (node_type) {
      SVM_CASE(NODE_END)
      return;
      SVM_CASE(NODE_SHADER_JUMP)
      {
        const SVMNodeShaderJump jump = svm_node_get<SVMNodeShaderJump>(kg, &offset);
        if (type == SHADER_TYPE_SURFACE) {
          offset = jump.offset_surface;
        }
        else if (type == SHADER_TYPE_VOLUME) {
          offset = jump.offset_volume;
        }
        else if (type == SHADER_TYPE_DISPLACEMENT) {
          offset = jump.offset_displacement;
        }
        else {
          return;
        }
        break;
      }
      SVM_CASE(NODE_CLOSURE_BSDF)
      {
        const ccl_global SVMNodeClosureBsdf &bsdf_node = svm_node_get<SVMNodeClosureBsdf>(kg,
                                                                                          &offset);
        offset = svm_node_closure_bsdf<node_feature_mask, type>(
            kg, sd, stack, closure_weight, bsdf_node, path_flag, offset);
      }
      break;
      SVM_CASE(NODE_CLOSURE_EMISSION)
      IF_KERNEL_NODES_FEATURE(EMISSION)
      {
        svm_node_closure_emission(
            kg, sd, stack, closure_weight, svm_node_get<SVMNodeClosureEmission>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_CLOSURE_BACKGROUND)
      IF_KERNEL_NODES_FEATURE(EMISSION)
      {
        svm_node_closure_background(
            sd, stack, closure_weight, svm_node_get<SVMNodeClosureBackground>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_CLOSURE_SET_WEIGHT)
      svm_node_closure_set_weight(&closure_weight,
                                  svm_node_get<SVMNodeClosureSetWeight>(kg, &offset));
      break;
      SVM_CASE(NODE_CLOSURE_WEIGHT)
      svm_node_closure_weight(
          stack, &closure_weight, svm_node_get<SVMNodeClosureWeight>(kg, &offset));
      break;
      SVM_CASE(NODE_EMISSION_WEIGHT)
      IF_KERNEL_NODES_FEATURE(EMISSION)
      {
        svm_node_emission_weight(
            stack, &closure_weight, svm_node_get<SVMNodeEmissionWeight>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_MIX_CLOSURE)
      svm_node_mix_closure(stack, svm_node_get<SVMNodeMixClosure>(kg, &offset));
      break;
      SVM_CASE(NODE_JUMP_IF_ZERO)
      {
        const SVMNodeJumpIfZero jump = svm_node_get<SVMNodeJumpIfZero>(kg, &offset);
        if (stack_load_float(stack, jump.stack_offset) <= 0.0f) {
          offset += jump.jump_offset;
        }
      }
      break;
      SVM_CASE(NODE_JUMP_IF_ONE)
      {
        const SVMNodeJumpIfOne jump = svm_node_get<SVMNodeJumpIfOne>(kg, &offset);
        if (stack_load_float(stack, jump.stack_offset) >= 1.0f) {
          offset += jump.jump_offset;
        }
      }
      break;
      SVM_CASE(NODE_GEOMETRY)
      svm_node_geometry<float3>(kg, sd, stack, svm_node_get<SVMNodeGeometry>(kg, &offset));
      break;
      SVM_CASE(NODE_GEOMETRY_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_geometry<dual3>(kg, sd, stack, svm_node_get<SVMNodeGeometry>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_CONVERT)
      svm_node_convert<float, float3>(kg, stack, svm_node_get<SVMNodeConvert>(kg, &offset));
      break;
      SVM_CASE(NODE_CONVERT_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_convert<dual1, dual3>(kg, stack, svm_node_get<SVMNodeConvert>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_TEX_COORD)
      {
        const ccl_global auto &node = svm_node_get<SVMNodeTexCoord>(kg, &offset);
        offset = svm_node_tex_coord(kg, sd, path_flag, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_TEX_COORD_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        const ccl_global auto &node = svm_node_get<SVMNodeTexCoord>(kg, &offset);
        offset = svm_node_tex_coord_derivative(kg, sd, path_flag, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_VALUE_F)
      svm_node_value_f<float>(stack, svm_node_get<SVMNodeValueF>(kg, &offset));
      break;
      SVM_CASE(NODE_VALUE_F_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_value_f<dual1>(stack, svm_node_get<SVMNodeValueF>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_VALUE_V)
      svm_node_value_v<float3>(stack, svm_node_get<SVMNodeValueV>(kg, &offset));
      break;
      SVM_CASE(NODE_VALUE_V_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_value_v<dual3>(stack, svm_node_get<SVMNodeValueV>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_ATTR)
      IF_KERNEL_NODES_FEATURE(VOLUME)
      {
#ifdef __VOLUME__
        svm_node_attr_volume(kg, sd, stack, svm_node_get<SVMNodeAttr>(kg, &offset));
#endif
      }
      else {
        svm_node_attr_surface(kg, sd, stack, svm_node_get<SVMNodeAttr>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_ATTR_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_attr_derivative(kg, sd, stack, svm_node_get<SVMNodeAttr>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_VERTEX_COLOR)
      svm_node_vertex_color(kg, sd, stack, svm_node_get<SVMNodeVertexColor>(kg, &offset));
      break;
      SVM_CASE(NODE_VERTEX_COLOR_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_vertex_color_derivative(
            kg, sd, stack, svm_node_get<SVMNodeVertexColor>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_SET_DISPLACEMENT)
      svm_node_set_displacement<node_feature_mask>(
          sd, stack, svm_node_get<SVMNodeSetDisplacement>(kg, &offset));
      break;
      SVM_CASE(NODE_DISPLACEMENT)
      svm_node_displacement<node_feature_mask>(
          kg, sd, stack, svm_node_get<SVMNodeDisplacement>(kg, &offset));
      break;
      SVM_CASE(NODE_VECTOR_DISPLACEMENT)
      svm_node_vector_displacement<node_feature_mask>(
          kg, sd, stack, svm_node_get<SVMNodeVectorDisplacement>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_IMAGE)
      svm_node_tex_image<float3>(kg, sd, stack, svm_node_get<SVMNodeTexImage>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_IMAGE_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_tex_image<dual3>(kg, sd, stack, svm_node_get<SVMNodeTexImage>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_TEX_IMAGE_BOX)
      svm_node_tex_image_box<float3>(kg, sd, stack, svm_node_get<SVMNodeTexImageBox>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_IMAGE_BOX_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_tex_image_box<dual3>(
            kg, sd, stack, svm_node_get<SVMNodeTexImageBox>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_TEX_NOISE)
      svm_node_tex_noise(stack, svm_node_get<SVMNodeTexNoise>(kg, &offset));
      break;
      SVM_CASE(NODE_SET_BUMP)
      svm_node_set_bump<node_feature_mask>(
          kg, sd, stack, svm_node_get<SVMNodeSetBump>(kg, &offset));
      break;
      SVM_CASE(NODE_CLOSURE_SET_NORMAL)
      IF_KERNEL_NODES_FEATURE(BUMP)
      {
        svm_node_set_normal(sd, stack, svm_node_get<SVMNodeClosureSetNormal>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_ENTER_BUMP_EVAL)
      IF_KERNEL_NODES_FEATURE(BUMP_STATE)
      {
        svm_node_enter_bump_eval(kg, sd, stack, svm_node_get<SVMNodeEnterBumpEval>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_LEAVE_BUMP_EVAL)
      IF_KERNEL_NODES_FEATURE(BUMP_STATE)
      {
        svm_node_leave_bump_eval(sd, stack, svm_node_get<SVMNodeLeaveBumpEval>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_HSV)
      svm_node_hsv(stack, svm_node_get<SVMNodeHSV>(kg, &offset));
      break;
      SVM_CASE(NODE_CLOSURE_HOLDOUT)
      svm_node_closure_holdout(
          sd, stack, closure_weight, svm_node_get<SVMNodeClosureHoldout>(kg, &offset));
      break;
      SVM_CASE(NODE_FRESNEL)
      svm_node_fresnel(sd, stack, svm_node_get<SVMNodeFresnel>(kg, &offset));
      break;
      SVM_CASE(NODE_LAYER_WEIGHT)
      svm_node_layer_weight(sd, stack, svm_node_get<SVMNodeLayerWeight>(kg, &offset));
      break;
      SVM_CASE(NODE_CLOSURE_VOLUME)
      IF_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_closure_volume<type>(
            kg, sd, stack, closure_weight, svm_node_get<SVMNodeClosureVolume>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_VOLUME_COEFFICIENTS)
      IF_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_volume_coefficients<type>(kg,
                                           sd,
                                           stack,
                                           closure_weight,
                                           svm_node_get<SVMNodeVolumeCoefficients>(kg, &offset),
                                           path_flag);
      }
      break;
      SVM_CASE(NODE_PRINCIPLED_VOLUME)
      IF_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_principled_volume<type>(kg,
                                         sd,
                                         stack,
                                         closure_weight,
                                         svm_node_get<SVMNodePrincipledVolume>(kg, &offset),
                                         path_flag);
      }
      break;
      SVM_CASE(NODE_MATH)
      svm_node_math(stack, svm_node_get<SVMNodeMath>(kg, &offset));
      break;
      SVM_CASE(NODE_VECTOR_MATH)
      svm_node_vector_math<float3>(stack, svm_node_get<SVMNodeVectorMath>(kg, &offset));
      break;
      SVM_CASE(NODE_VECTOR_MATH_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_vector_math<dual3>(stack, svm_node_get<SVMNodeVectorMath>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_RGB_RAMP)
      {
        const ccl_global auto &node = svm_node_get<SVMNodeRGBRamp>(kg, &offset);
        offset = svm_node_rgb_ramp(kg, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_GAMMA)
      svm_node_gamma(stack, svm_node_get<SVMNodeGamma>(kg, &offset));
      break;
      SVM_CASE(NODE_BRIGHTCONTRAST)
      svm_node_brightness(stack, svm_node_get<SVMNodeBrightContrast>(kg, &offset));
      break;
      SVM_CASE(NODE_LIGHT_PATH)
      svm_node_light_path<node_feature_mask>(
          kg, state, sd, stack, svm_node_get<SVMNodeLightPath>(kg, &offset), path_flag);
      break;
      SVM_CASE(NODE_OBJECT_INFO)
      svm_node_object_info(kg, sd, stack, svm_node_get<SVMNodeObjectInfo>(kg, &offset));
      break;
      SVM_CASE(NODE_PARTICLE_INFO)
      svm_node_particle_info(kg, sd, stack, svm_node_get<SVMNodeParticleInfo>(kg, &offset));
      break;
#if defined(__HAIR__)
      SVM_CASE(NODE_HAIR_INFO)
      svm_node_hair_info(kg, sd, stack, svm_node_get<SVMNodeHairInfo>(kg, &offset));
      break;
#endif
#if defined(__POINTCLOUD__)
      SVM_CASE(NODE_POINT_INFO)
      svm_node_point_info(kg, sd, stack, svm_node_get<SVMNodePointInfo>(kg, &offset));
      break;
#endif
      SVM_CASE(NODE_TEXTURE_MAPPING)
      svm_node_texture_mapping(stack, svm_node_get<SVMNodeTextureMapping>(kg, &offset));
      break;
      SVM_CASE(NODE_MAPPING)
      svm_node_mapping<float3>(stack, svm_node_get<SVMNodeMapping>(kg, &offset));
      break;
      SVM_CASE(NODE_MAPPING_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_mapping<dual3>(stack, svm_node_get<SVMNodeMapping>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_MIN_MAX)
      svm_node_min_max(stack, svm_node_get<SVMNodeMinMax>(kg, &offset));
      break;
      SVM_CASE(NODE_CAMERA)
      svm_node_camera(kg, sd, stack, svm_node_get<SVMNodeCamera>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_ENVIRONMENT)
      svm_node_tex_environment<float3>(
          kg, sd, stack, svm_node_get<SVMNodeTexEnvironment>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_ENVIRONMENT_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_tex_environment<dual3>(
            kg, sd, stack, svm_node_get<SVMNodeTexEnvironment>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_TEX_SKY)
      {
        const ccl_global auto &node = svm_node_get<SVMNodeTexSky>(kg, &offset);
        offset = svm_node_tex_sky(kg, sd, path_flag, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_TEX_GRADIENT)
      svm_node_tex_gradient(stack, svm_node_get<SVMNodeTexGradient>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_VORONOI)
      svm_node_tex_voronoi<node_feature_mask>(stack, svm_node_get<SVMNodeTexVoronoi>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_GABOR)
      svm_node_tex_gabor(stack, svm_node_get<SVMNodeTexGabor>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_WAVE)
      svm_node_tex_wave(stack, svm_node_get<SVMNodeTexWave>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_MAGIC)
      svm_node_tex_magic(stack, svm_node_get<SVMNodeTexMagic>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_CHECKER)
      svm_node_tex_checker(stack, svm_node_get<SVMNodeTexChecker>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_BRICK)
      svm_node_tex_brick(stack, svm_node_get<SVMNodeTexBrick>(kg, &offset));
      break;
      SVM_CASE(NODE_TEX_WHITE_NOISE)
      svm_node_tex_white_noise(stack, svm_node_get<SVMNodeTexWhiteNoise>(kg, &offset));
      break;
      SVM_CASE(NODE_NORMAL)
      svm_node_normal(stack, svm_node_get<SVMNodeNormal>(kg, &offset));
      break;
      SVM_CASE(NODE_LIGHT_FALLOFF)
      svm_node_light_falloff(sd, stack, svm_node_get<SVMNodeLightFalloff>(kg, &offset));
      break;
      SVM_CASE(NODE_IES)
      svm_node_ies(kg, sd, stack, svm_node_get<SVMNodeIES>(kg, &offset));
      break;
      SVM_CASE(NODE_CURVES)
      {
        const ccl_global auto &node = svm_node_get<SVMNodeCurves>(kg, &offset);
        offset = svm_node_curves(kg, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_TANGENT)
      svm_node_tangent<float3>(kg, sd, stack, svm_node_get<SVMNodeTangent>(kg, &offset));
      break;
      SVM_CASE(NODE_TANGENT_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_tangent<dual3>(kg, sd, stack, svm_node_get<SVMNodeTangent>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_NORMAL_MAP)
      svm_node_normal_map(kg, sd, stack, svm_node_get<SVMNodeNormalMap>(kg, &offset));
      break;
      SVM_CASE(NODE_RADIAL_TILING)
      svm_node_radial_tiling<node_feature_mask>(stack,
                                                svm_node_get<SVMNodeRadialTiling>(kg, &offset));
      break;
      SVM_CASE(NODE_INVERT)
      svm_node_invert(stack, svm_node_get<SVMNodeInvert>(kg, &offset));
      break;
      SVM_CASE(NODE_MIX)
      svm_node_mix(stack, svm_node_get<SVMNodeMix>(kg, &offset));
      break;
      SVM_CASE(NODE_SEPARATE_COLOR)
      svm_node_separate_color(stack, svm_node_get<SVMNodeSeparateColor>(kg, &offset));
      break;
      SVM_CASE(NODE_COMBINE_COLOR)
      svm_node_combine_color(stack, svm_node_get<SVMNodeCombineColor>(kg, &offset));
      break;
      SVM_CASE(NODE_SEPARATE_VECTOR)
      svm_node_separate_vector<float3>(stack, svm_node_get<SVMNodeSeparateVector>(kg, &offset));
      break;
      SVM_CASE(NODE_SEPARATE_VECTOR_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_separate_vector<dual3>(stack, svm_node_get<SVMNodeSeparateVector>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_COMBINE_VECTOR)
      svm_node_combine_vector<float3>(stack, svm_node_get<SVMNodeCombineVector>(kg, &offset));
      break;
      SVM_CASE(NODE_COMBINE_VECTOR_DERIVATIVE)
      IF_NOT_KERNEL_NODES_FEATURE(VOLUME)
      {
        svm_node_combine_vector<dual3>(stack, svm_node_get<SVMNodeCombineVector>(kg, &offset));
      }
      break;
      SVM_CASE(NODE_VECTOR_ROTATE)
      svm_node_vector_rotate(stack, svm_node_get<SVMNodeVectorRotate>(kg, &offset));
      break;
      SVM_CASE(NODE_VECTOR_TRANSFORM)
      svm_node_vector_transform(kg, sd, stack, svm_node_get<SVMNodeVectorTransform>(kg, &offset));
      break;
      SVM_CASE(NODE_WIREFRAME)
      svm_node_wireframe(kg, sd, stack, svm_node_get<SVMNodeWireframe>(kg, &offset));
      break;
      SVM_CASE(NODE_WAVELENGTH)
      svm_node_wavelength(kg, stack, svm_node_get<SVMNodeWavelength>(kg, &offset));
      break;
      SVM_CASE(NODE_BLACKBODY)
      svm_node_blackbody(kg, stack, svm_node_get<SVMNodeBlackbody>(kg, &offset));
      break;
      SVM_CASE(NODE_MAP_RANGE)
      svm_node_map_range(stack, svm_node_get<SVMNodeMapRange>(kg, &offset));
      break;
      SVM_CASE(NODE_VECTOR_MAP_RANGE)
      svm_node_vector_map_range(stack, svm_node_get<SVMNodeVectorMapRange>(kg, &offset));
      break;
      SVM_CASE(NODE_CLAMP)
      svm_node_clamp(stack, svm_node_get<SVMNodeClamp>(kg, &offset));
      break;
#ifdef __SHADER_RAYTRACE__
      SVM_CASE(NODE_BEVEL)
      svm_node_bevel<node_feature_mask>(
          kg, state, sd, stack, svm_node_get<SVMNodeBevel>(kg, &offset));
      break;
      SVM_CASE(NODE_AMBIENT_OCCLUSION)
      svm_node_ao<node_feature_mask>(
          kg, state, sd, stack, svm_node_get<SVMNodeAmbientOcclusion>(kg, &offset));
      break;
      SVM_CASE(NODE_RAYCAST)
      {
        const ccl_global auto &node = svm_node_get<SVMNodeRaycast>(kg, &offset);
        offset = svm_node_raycast<node_feature_mask>(kg, state, sd, stack, node, offset);
      }
      break;
#endif
      SVM_CASE(NODE_AOV_START)
      if (!svm_node_aov_check(path_flag, render_buffer)) {
        return;
      }
      break;
      SVM_CASE(NODE_AOV_COLOR)
      svm_node_aov_color<node_feature_mask>(
          kg, sd, state, stack, svm_node_get<SVMNodeAOVColor>(kg, &offset), render_buffer);
      break;
      SVM_CASE(NODE_AOV_VALUE)
      svm_node_aov_value<node_feature_mask>(
          kg, sd, state, stack, svm_node_get<SVMNodeAOVValue>(kg, &offset), render_buffer);
      break;
      SVM_CASE(NODE_FLOAT_CURVE)
      {
        const ccl_global auto &node = svm_node_get<SVMNodeFloatCurve>(kg, &offset);
        offset = svm_node_curve(kg, stack, node, offset);
      }
      break;
      SVM_CASE(NODE_MIX_COLOR)
      svm_node_mix_color(stack, svm_node_get<SVMNodeMixColor>(kg, &offset));
      break;
      SVM_CASE(NODE_MIX_FLOAT)
      svm_node_mix_float(stack, svm_node_get<SVMNodeMixFloat>(kg, &offset));
      break;
      SVM_CASE(NODE_MIX_VECTOR)
      svm_node_mix_vector(stack, svm_node_get<SVMNodeMixVector>(kg, &offset));
      break;
      SVM_CASE(NODE_MIX_VECTOR_NON_UNIFORM)
      svm_node_mix_vector_non_uniform(stack,
                                      svm_node_get<SVMNodeMixVectorNonUniform>(kg, &offset));
      break;
      default:
        kernel_assert(!"Unknown node type was passed to the SVM machine");
        return;
    }
  }
}

CCL_NAMESPACE_END
