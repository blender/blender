/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/camera/camera.h"
#include "kernel/geom/geom.h"
#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

/* Texture Coordinate Node */

ccl_device_noinline int svm_node_tex_coord(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           uint32_t path_flag,
                                           ccl_private float *stack,
                                           uint4 node,
                                           int offset)
{
  float3 data = zero_float3();
  uint type = node.y;
  uint out_offset = node.z;

  switch (type) {
    case NODE_TEXCO_OBJECT: {
      data = sd->P;
      if (node.w == 0) {
        if (sd->object != OBJECT_NONE) {
          object_inverse_position_transform(kg, sd, &data);
        }
      }
      else {
        Transform tfm;
        tfm.x = read_node_float(kg, &offset);
        tfm.y = read_node_float(kg, &offset);
        tfm.z = read_node_float(kg, &offset);
        data = transform_point(&tfm, data);
      }
      break;
    }
    case NODE_TEXCO_NORMAL: {
      data = sd->N;
      object_inverse_normal_transform(kg, sd, &data);
      break;
    }
    case NODE_TEXCO_CAMERA: {
      Transform tfm = kernel_data.cam.worldtocamera;

      if (sd->object != OBJECT_NONE)
        data = transform_point(&tfm, sd->P);
      else
        data = transform_point(&tfm, sd->P + camera_position(kg));
      break;
    }
    case NODE_TEXCO_WINDOW: {
      if ((path_flag & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE &&
          kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
        data = camera_world_to_ndc(kg, sd, sd->ray_P);
      else
        data = camera_world_to_ndc(kg, sd, sd->P);
      data.z = 0.0f;
      break;
    }
    case NODE_TEXCO_REFLECTION: {
      if (sd->object != OBJECT_NONE)
        data = 2.0f * dot(sd->N, sd->wi) * sd->N - sd->wi;
      else
        data = sd->wi;
      break;
    }
    case NODE_TEXCO_DUPLI_GENERATED: {
      data = object_dupli_generated(kg, sd->object);
      break;
    }
    case NODE_TEXCO_DUPLI_UV: {
      data = object_dupli_uv(kg, sd->object);
      break;
    }
    case NODE_TEXCO_VOLUME_GENERATED: {
      data = sd->P;

#ifdef __VOLUME__
      if (sd->object != OBJECT_NONE)
        data = volume_normalized_position(kg, sd, data);
#endif
      break;
    }
  }

  stack_store_float3(stack, out_offset, data);
  return offset;
}

ccl_device_noinline int svm_node_tex_coord_bump_dx(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   uint32_t path_flag,
                                                   ccl_private float *stack,
                                                   uint4 node,
                                                   int offset)
{
#ifdef __RAY_DIFFERENTIALS__
  float3 data = zero_float3();
  uint type = node.y;
  uint out_offset = node.z;

  switch (type) {
    case NODE_TEXCO_OBJECT: {
      data = svm_node_bump_P_dx(sd);
      if (node.w == 0) {
        if (sd->object != OBJECT_NONE) {
          object_inverse_position_transform(kg, sd, &data);
        }
      }
      else {
        Transform tfm;
        tfm.x = read_node_float(kg, &offset);
        tfm.y = read_node_float(kg, &offset);
        tfm.z = read_node_float(kg, &offset);
        data = transform_point(&tfm, data);
      }
      break;
    }
    case NODE_TEXCO_NORMAL: {
      data = sd->N;
      object_inverse_normal_transform(kg, sd, &data);
      break;
    }
    case NODE_TEXCO_CAMERA: {
      Transform tfm = kernel_data.cam.worldtocamera;

      if (sd->object != OBJECT_NONE)
        data = transform_point(&tfm, svm_node_bump_P_dx(sd));
      else
        data = transform_point(&tfm, svm_node_bump_P_dx(sd) + camera_position(kg));
      break;
    }
    case NODE_TEXCO_WINDOW: {
      if ((path_flag & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE &&
          kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
        data = camera_world_to_ndc(kg, sd, sd->ray_P);
      else
        data = camera_world_to_ndc(kg, sd, svm_node_bump_P_dx(sd));
      data.z = 0.0f;
      break;
    }
    case NODE_TEXCO_REFLECTION: {
      if (sd->object != OBJECT_NONE)
        data = 2.0f * dot(sd->N, sd->wi) * sd->N - sd->wi;
      else
        data = sd->wi;
      break;
    }
    case NODE_TEXCO_DUPLI_GENERATED: {
      data = object_dupli_generated(kg, sd->object);
      break;
    }
    case NODE_TEXCO_DUPLI_UV: {
      data = object_dupli_uv(kg, sd->object);
      break;
    }
    case NODE_TEXCO_VOLUME_GENERATED: {
      data = svm_node_bump_P_dx(sd);

#  ifdef __VOLUME__
      if (sd->object != OBJECT_NONE)
        data = volume_normalized_position(kg, sd, data);
#  endif
      break;
    }
  }

  stack_store_float3(stack, out_offset, data);
  return offset;
#else
  return svm_node_tex_coord(kg, sd, path_flag, stack, node, offset);
#endif
}

ccl_device_noinline int svm_node_tex_coord_bump_dy(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   uint32_t path_flag,
                                                   ccl_private float *stack,
                                                   uint4 node,
                                                   int offset)
{
#ifdef __RAY_DIFFERENTIALS__
  float3 data = zero_float3();
  uint type = node.y;
  uint out_offset = node.z;

  switch (type) {
    case NODE_TEXCO_OBJECT: {
      data = svm_node_bump_P_dy(sd);
      if (node.w == 0) {
        if (sd->object != OBJECT_NONE) {
          object_inverse_position_transform(kg, sd, &data);
        }
      }
      else {
        Transform tfm;
        tfm.x = read_node_float(kg, &offset);
        tfm.y = read_node_float(kg, &offset);
        tfm.z = read_node_float(kg, &offset);
        data = transform_point(&tfm, data);
      }
      break;
    }
    case NODE_TEXCO_NORMAL: {
      data = sd->N;
      object_inverse_normal_transform(kg, sd, &data);
      break;
    }
    case NODE_TEXCO_CAMERA: {
      Transform tfm = kernel_data.cam.worldtocamera;

      if (sd->object != OBJECT_NONE)
        data = transform_point(&tfm, svm_node_bump_P_dy(sd));
      else
        data = transform_point(&tfm, svm_node_bump_P_dy(sd) + camera_position(kg));
      break;
    }
    case NODE_TEXCO_WINDOW: {
      if ((path_flag & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE &&
          kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
        data = camera_world_to_ndc(kg, sd, sd->ray_P);
      else
        data = camera_world_to_ndc(kg, sd, svm_node_bump_P_dy(sd));
      data.z = 0.0f;
      break;
    }
    case NODE_TEXCO_REFLECTION: {
      if (sd->object != OBJECT_NONE)
        data = 2.0f * dot(sd->N, sd->wi) * sd->N - sd->wi;
      else
        data = sd->wi;
      break;
    }
    case NODE_TEXCO_DUPLI_GENERATED: {
      data = object_dupli_generated(kg, sd->object);
      break;
    }
    case NODE_TEXCO_DUPLI_UV: {
      data = object_dupli_uv(kg, sd->object);
      break;
    }
    case NODE_TEXCO_VOLUME_GENERATED: {
      data = svm_node_bump_P_dy(sd);

#  ifdef __VOLUME__
      if (sd->object != OBJECT_NONE)
        data = volume_normalized_position(kg, sd, data);
#  endif
      break;
    }
  }

  stack_store_float3(stack, out_offset, data);
  return offset;
#else
  return svm_node_tex_coord(kg, sd, path_flag, stack, node, offset);
#endif
}

ccl_device_noinline void svm_node_normal_map(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             uint4 node)
{
  uint color_offset, strength_offset, normal_offset, space;
  svm_unpack_node_uchar4(node.y, &color_offset, &strength_offset, &normal_offset, &space);

  float3 color = stack_load_float3(stack, color_offset);
  color = 2.0f * make_float3(color.x - 0.5f, color.y - 0.5f, color.z - 0.5f);

  bool is_backfacing = (sd->flag & SD_BACKFACING) != 0;
  float3 N;
  float strength = stack_load_float(stack, strength_offset);
  if (space == NODE_NORMAL_MAP_TANGENT) {
    /* tangent space */
    if (sd->object == OBJECT_NONE || (sd->type & PRIMITIVE_TRIANGLE) == 0) {
      /* Fallback to unperturbed normal. */
      stack_store_float3(stack, normal_offset, sd->N);
      return;
    }

    /* first try to get tangent attribute */
    const AttributeDescriptor attr = find_attribute(kg, sd, node.z);
    const AttributeDescriptor attr_sign = find_attribute(kg, sd, node.w);

    if (attr.offset == ATTR_STD_NOT_FOUND || attr_sign.offset == ATTR_STD_NOT_FOUND) {
      /* Fallback to unperturbed normal. */
      stack_store_float3(stack, normal_offset, sd->N);
      return;
    }

    /* get _unnormalized_ interpolated normal and tangent */
    float3 tangent = primitive_surface_attribute_float3(kg, sd, attr, NULL, NULL);
    float sign = primitive_surface_attribute_float(kg, sd, attr_sign, NULL, NULL);
    float3 normal;

    if (sd->shader & SHADER_SMOOTH_NORMAL) {
      normal = triangle_smooth_normal_unnormalized(kg, sd, sd->Ng, sd->prim, sd->u, sd->v);
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
    color.x *= strength;
    color.y *= strength;
    color.z = mix(1.0f, color.z, saturatef(strength));

    /* apply normal map */
    float3 B = sign * cross(normal, tangent);
    N = safe_normalize(color.x * tangent + color.y * B + color.z * normal);

    /* transform to world space */
    object_normal_transform(kg, sd, &N);
  }
  else {
    /* strange blender convention */
    if (space == NODE_NORMAL_MAP_BLENDER_OBJECT || space == NODE_NORMAL_MAP_BLENDER_WORLD) {
      color.y = -color.y;
      color.z = -color.z;
    }

    /* object, world space */
    N = color;

    if (space == NODE_NORMAL_MAP_OBJECT || space == NODE_NORMAL_MAP_BLENDER_OBJECT)
      object_normal_transform(kg, sd, &N);
    else
      N = safe_normalize(N);
    /* Apply strength in all but tangent space. */
    if (strength != 1.0f) {
      strength = max(strength, 0.0f);
      N = safe_normalize(sd->N + (N - sd->N) * strength);
    }
  }

  /* invert normal for backfacing polygons */
  if (is_backfacing) {
    N = -N;
  }

  if (is_zero(N)) {
    N = sd->N;
  }

  stack_store_float3(stack, normal_offset, N);
}

ccl_device_noinline void svm_node_tangent(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          uint4 node)
{
  uint tangent_offset, direction_type, axis;
  svm_unpack_node_uchar3(node.y, &tangent_offset, &direction_type, &axis);

  float3 tangent;
  float3 attribute_value;
  const AttributeDescriptor desc = find_attribute(kg, sd, node.z);
  if (desc.offset != ATTR_STD_NOT_FOUND) {
    if (desc.type == NODE_ATTR_FLOAT2) {
      float2 value = primitive_surface_attribute_float2(kg, sd, desc, NULL, NULL);
      attribute_value.x = value.x;
      attribute_value.y = value.y;
      attribute_value.z = 0.0f;
    }
    else {
      attribute_value = primitive_surface_attribute_float3(kg, sd, desc, NULL, NULL);
    }
  }

  if (direction_type == NODE_TANGENT_UVMAP) {
    /* UV map */
    if (desc.offset == ATTR_STD_NOT_FOUND) {
      stack_store_float3(stack, tangent_offset, zero_float3());
      return;
    }
    else {
      tangent = attribute_value;
    }
  }
  else {
    /* radial */
    float3 generated;

    if (desc.offset == ATTR_STD_NOT_FOUND)
      generated = sd->P;
    else
      generated = attribute_value;

    if (axis == NODE_TANGENT_AXIS_X)
      tangent = make_float3(0.0f, -(generated.z - 0.5f), (generated.y - 0.5f));
    else if (axis == NODE_TANGENT_AXIS_Y)
      tangent = make_float3(-(generated.z - 0.5f), 0.0f, (generated.x - 0.5f));
    else
      tangent = make_float3(-(generated.y - 0.5f), (generated.x - 0.5f), 0.0f);
  }

  object_normal_transform(kg, sd, &tangent);
  tangent = cross(sd->N, normalize(cross(tangent, sd->N)));
  stack_store_float3(stack, tangent_offset, tangent);
}

CCL_NAMESPACE_END
