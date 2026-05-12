/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/camera/camera.h"

#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/object.h"
#include "kernel/geom/primitive.h"

#include "kernel/svm/attribute.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/types.h"
#include "kernel/svm/util.h"
#include "util/math_base.h"

CCL_NAMESPACE_BEGIN

/* Smooth normal with screen-space derivatives for texture coordinate use.
 * Returns the interpolated normal in object space, with dx/dy representing
 * the per-pixel change from ray differentials. */
ccl_device_inline dual3 svm_texco_smooth_normal(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  if ((sd->type & PRIMITIVE_TRIANGLE) && (sd->shader & SHADER_SMOOTH_NORMAL)) {
    float3 N_x, N_y;
    float3 N;
    if (sd->type == PRIMITIVE_TRIANGLE) {
      N = triangle_smooth_normal(kg,
                                 sd->Ng,
                                 sd->object,
                                 sd->object_flag,
                                 sd->prim,
                                 sd->u,
                                 sd->v,
                                 sd->du,
                                 sd->dv,
                                 N_x,
                                 N_y);
    }
    else {
      N = motion_triangle_smooth_normal(
          kg, sd->Ng, sd->object, sd->prim, sd->time, sd->u, sd->v, sd->du, sd->dv, N_x, N_y);
    }
    if (sd->flag & SD_BACKFACING) {
      N = -N;
      N_x = -N_x;
      N_y = -N_y;
    }
    if (sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED) {
      object_inverse_normal_transform(kg, sd, &N);
      object_inverse_normal_transform(kg, sd, &N_x);
      object_inverse_normal_transform(kg, sd, &N_y);
    }
    return dual3(N, N_x - N, N_y - N);
  }

  /* Flat normal or non-triangle: no derivative. */
  float3 N = sd->N;
  object_inverse_normal_transform(kg, sd, &N);
  return dual3(N);
}

/* Texture Coordinate Node */

template<typename Float3Type>
ccl_device_inline Float3Type svm_texco_reflection(const ccl_private ShaderData *sd)
{
  Float3Type data = shading_incoming<Float3Type>(sd);
  if (sd->object != OBJECT_NONE) {
    data = -reflect(data, sd->N);
  }
  return data;
}

template<typename Float3Type>
ccl_device_inline Float3Type svm_texco_camera(KernelGlobals kg,
                                              const ccl_private ShaderData *sd,
                                              const ccl_private Float3Type &P)
{
  Float3Type data(P);
  const Transform tfm = kernel_data.cam.worldtocamera;
  if (sd->object == OBJECT_NONE) {
    data = data + camera_position(kg);
  }
  data = transform_point(&tfm, data);
  return data;
}

template<typename Float3Type>
ccl_device_noinline Float3Type svm_node_tex_coord_eval(KernelGlobals kg,
                                                       ccl_private ShaderData *sd,
                                                       const uint32_t path_flag,
                                                       const NodeTexCoord type,
                                                       ccl_private int *offset)
{
  Float3Type data;

  switch (type) {
    case NODE_TEXCO_OBJECT:
    case NODE_TEXCO_OBJECT_WITH_TRANSFORM: {
      data = shading_position<Float3Type>(sd);
      if (type == NODE_TEXCO_OBJECT) {
        object_inverse_position_transform_if_object(kg, sd, &data);
      }
      else {
        const Transform tfm = make_transform(svm_node_get<PackedTransform>(kg, offset));
        data = transform_point(&tfm, data);
      }
      break;
    }
    case NODE_TEXCO_NORMAL: {
      if constexpr (is_dual_v<Float3Type>) {
        data = svm_texco_smooth_normal(kg, sd);
      }
      else {
        data = sd->N;
        object_inverse_normal_transform(kg, sd, &data);
      }
      break;
    }
    case NODE_TEXCO_CAMERA: {
      const Float3Type P = shading_position<Float3Type>(sd);
      data = svm_texco_camera<Float3Type>(kg, sd, P);
      break;
    }
    case NODE_TEXCO_WINDOW: {
      if ((path_flag & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE &&
          kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
      {
        data = Float3Type(camera_world_to_ndc(kg, sd, sd->ray_P));
      }
      else {
        data = Float3Type(camera_world_to_ndc(kg, sd, sd->P));
        if constexpr (is_dual_v<Float3Type>) {
          data.dx.x = 1.0f / kernel_data.cam.width;
          data.dy.y = 1.0f / kernel_data.cam.height;
        }
      }
      if constexpr (is_dual_v<Float3Type>) {
        data.val.z = 0.0f;
      }
      else {
        data.z = 0.0f;
      }
      break;
    }
    case NODE_TEXCO_REFLECTION: {
      data = svm_texco_reflection<Float3Type>(sd);
      break;
    }
    case NODE_TEXCO_DUPLI_GENERATED: {
      data = Float3Type(object_dupli_generated(kg, sd->object));
      break;
    }
    case NODE_TEXCO_DUPLI_UV: {
      data = Float3Type(object_dupli_uv(kg, sd->object));
      break;
    }
    case NODE_TEXCO_VOLUME_GENERATED: {
      data = shading_position<Float3Type>(sd);

#ifdef __VOLUME__
      if (sd->object != OBJECT_NONE) {
        data = volume_normalized_position<Float3Type>(kg, sd, data);
      }
#endif
      break;
    }
    default:
      data = make_zero<Float3Type>();
      break;
  }

  return data;
}

ccl_device_noinline int svm_node_tex_coord(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           const uint32_t path_flag,
                                           ccl_private float *ccl_restrict stack,
                                           const ccl_global SVMNodeTexCoord &ccl_restrict node,
                                           int offset)
{
  const float3 data = svm_node_tex_coord_eval<float3>(kg, sd, path_flag, node.texco_type, &offset);
  stack_store(stack, node.out_offset, data);
  return offset;
}

ccl_device_noinline int svm_node_tex_coord_derivative(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    const uint32_t path_flag,
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeTexCoord &ccl_restrict node,
    int offset)
{
  dual3 data = svm_node_tex_coord_eval<dual3>(kg, sd, path_flag, node.texco_type, &offset);
  if (node.bump_offset == NODE_BUMP_OFFSET_DX) {
    data.val += data.dx * node.bump_filter_width;
  }
  else if (node.bump_offset == NODE_BUMP_OFFSET_DY) {
    data.val += data.dy * node.bump_filter_width;
  }
  /* Normal texture coordinate must be normalized after bump offset, matching OSL. */
  if (node.texco_type == NODE_TEXCO_NORMAL) {
    data = safe_normalize(data);
  }
  if (node.store_derivatives) {
    stack_store(stack, node.out_offset, data);
  }
  else {
    stack_store(stack, node.out_offset, data.val);
  }
  return offset;
}

ccl_device_noinline void svm_node_normal_map(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *ccl_restrict stack,
                                             const ccl_global SVMNodeNormalMap &ccl_restrict node)
{
  float3 color = stack_load(stack, node.color);
  color = 2.0f * make_float3(color.x - 0.5f, color.y - 0.5f, color.z - 0.5f);

  if (node.invert_green) {
    color.y = -color.y;
  }

  const bool is_backfacing = (sd->flag & SD_BACKFACING) != 0;
  float3 N;
  float strength = stack_load(stack, node.strength);
  bool linear_interpolate_strength = false;

  if (node.space == NODE_NORMAL_MAP_TANGENT) {
    /* tangent space */
    if (sd->object == OBJECT_NONE || (sd->type & PRIMITIVE_TRIANGLE) == 0) {
      /* Fall back to unperturbed normal. */
      stack_store_float3(stack, node.normal_offset, sd->N);
      return;
    }

    /* first try to get tangent attribute */
    const AttributeDescriptor attr = find_attribute(kg, sd, node.attr);
    const AttributeDescriptor attr_sign = find_attribute(kg, sd, node.attr_sign);

    if (!is_attribute_found(attr) || !is_attribute_found(attr_sign)) {
      /* Fall back to unperturbed normal. */
      stack_store_float3(stack, node.normal_offset, sd->N);
      return;
    }

    /* get _unnormalized_ interpolated normal and tangent */
    const float3 tangent = primitive_surface_attribute<float3>(kg, sd, attr);
    const float sign = primitive_surface_attribute<float>(kg, sd, attr_sign);
    float3 normal;

    if (sd->shader & SHADER_SMOOTH_NORMAL) {
      const AttributeDescriptor attr_undisplaced_normal =
          (node.use_original_base) ?
              find_attribute(kg, sd->object, sd->prim, ATTR_STD_NORMAL_UNDISPLACED) :
              attribute_not_found();
      if (is_attribute_found(attr_undisplaced_normal)) {
        normal = primitive_surface_attribute<float3>(kg, sd, attr_undisplaced_normal);
        /* Can't interpolate in tangent space as the displaced normal is not used
         * for the tangent frame. */
        linear_interpolate_strength = true;
      }
      else {
        normal = triangle_smooth_normal_unnormalized_object_space(kg, sd);
      }
    }
    else {
      normal = sd->Ng;

      /* the normal is already inverted, which is too soon for the math here */
      if (is_backfacing) {
        normal = -normal;
      }

      object_inverse_normal_transform(kg, sd, &normal);
    }
    /* Apply strength in the tangent case. */
    if (!linear_interpolate_strength) {
      color.x *= strength;
      color.y *= strength;
      color.z = mix(1.0f, color.z, saturatef(strength));
    }

    /* apply normal map */
    const float3 B = sign * cross(normal, tangent);
    N = safe_normalize(to_global(color, tangent, B, normal));

    /* transform to world space */
    object_normal_transform(kg, sd, &N);

    /* invert normal for backfacing polygons */
    if (is_backfacing) {
      N = -N;
    }
  }
  else {
    linear_interpolate_strength = true;

    /* strange blender convention */
    if (node.space == NODE_NORMAL_MAP_BLENDER_OBJECT ||
        node.space == NODE_NORMAL_MAP_BLENDER_WORLD)
    {
      color.y = -color.y;
      color.z = -color.z;
    }

    /* object, world space */
    N = color;

    if (node.space == NODE_NORMAL_MAP_OBJECT || node.space == NODE_NORMAL_MAP_BLENDER_OBJECT) {
      object_normal_transform(kg, sd, &N);
    }
    else {
      N = safe_normalize(N);
    }

    /* invert normal for backfacing polygons */
    if (is_backfacing) {
      N = -N;
    }
  }

  /* Use simple linear interpolation if we can't do it in tangent space. */
  if (linear_interpolate_strength && strength != 1.0f) {
    strength = max(strength, 0.0f);
    N = safe_normalize(sd->N + (N - sd->N) * strength);
  }

  if (is_zero(N) || !isfinite_safe(N)) {
    N = sd->N;
  }

  stack_store_float3(stack, node.normal_offset, N);
}

template<typename Float3Type>
ccl_device_noinline void svm_node_tangent(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          const ccl_global SVMNodeTangent &ccl_restrict node)
{
  const AttributeDescriptor desc = find_attribute(kg, sd, node.attr);

  Float3Type tangent;
  if (node.direction_type == NODE_TANGENT_UVMAP) {
    /* UV map */
    if (!is_attribute_found(desc)) {
      stack_store(stack, node.tangent_offset, Float3Type());
      return;
    }
    if (desc.type == NODE_ATTR_FLOAT2) {
      if constexpr (is_dual_v<Float3Type>) {
        tangent = make_float3(primitive_surface_attribute<dual2>(kg, sd, desc));
      }
      else {
        tangent = make_float3(primitive_surface_attribute<float2>(kg, sd, desc));
      }
    }
    else {
      tangent = primitive_surface_attribute<Float3Type>(kg, sd, desc);
    }
  }
  else {
    /* radial */
    Float3Type generated;
    if (!is_attribute_found(desc)) {
      generated = shading_position<Float3Type>(sd);
    }
    else if (desc.type == NODE_ATTR_FLOAT2) {
      if constexpr (is_dual_v<Float3Type>) {
        generated = make_float3(primitive_surface_attribute<dual2>(kg, sd, desc));
      }
      else {
        generated = make_float3(primitive_surface_attribute<float2>(kg, sd, desc));
      }
    }
    else {
      generated = primitive_surface_attribute<Float3Type>(kg, sd, desc);
    }

    if constexpr (is_dual_v<Float3Type>) {
      using FloatType = dual_scalar_t<Float3Type>;
      if (node.axis == NODE_TANGENT_AXIS_X) {
        tangent = make_float3(FloatType(), -(generated.z() - 0.5f), (generated.y() - 0.5f));
      }
      else if (node.axis == NODE_TANGENT_AXIS_Y) {
        tangent = make_float3(-(generated.z() - 0.5f), FloatType(), (generated.x() - 0.5f));
      }
      else {
        tangent = make_float3(-(generated.y() - 0.5f), (generated.x() - 0.5f), FloatType());
      }
    }
    else {
      if (node.axis == NODE_TANGENT_AXIS_X) {
        tangent = make_float3(0.0f, -(generated.z - 0.5f), (generated.y - 0.5f));
      }
      else if (node.axis == NODE_TANGENT_AXIS_Y) {
        tangent = make_float3(-(generated.z - 0.5f), 0.0f, (generated.x - 0.5f));
      }
      else {
        tangent = make_float3(-(generated.y - 0.5f), (generated.x - 0.5f), 0.0f);
      }
    }
  }

  object_normal_transform(kg, sd, &tangent);
  tangent = cross(sd->N, normalize(cross(tangent, sd->N)));
  stack_store(stack, node.tangent_offset, tangent);
}

CCL_NAMESPACE_END
