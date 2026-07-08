/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Primitive Utilities
 *
 * Generic functions to look up mesh, curve and volume primitive attributes for
 * shading and render passes. */

#pragma once

#include "kernel/globals.h"

#include "kernel/camera/camera.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/curve.h"
#include "kernel/geom/object.h"
#include "kernel/geom/point.h"
#include "kernel/geom/triangle.h"
#include "kernel/geom/volume.h"

CCL_NAMESPACE_BEGIN

/* Surface Attributes
 *
 * Read geometry attributes for surface shading. This is distinct from volume
 * attributes for performance, mainly for GPU performance to avoid bringing in
 * heavy volume interpolation code. */

template<typename T>
ccl_device_forceinline T primitive_surface_attribute(KernelGlobals kg,
                                                     const ccl_private ShaderData *sd,
                                                     const AttributeDescriptor desc)
{
  using BaseT = dual_base_t<T>;

  if (desc.element & (ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
    return T(attribute_data_fetch<BaseT>(kg, desc.element, desc.offset));
  }

  if (sd->type & PRIMITIVE_TRIANGLE) {
    return triangle_attribute<T>(kg, sd, desc);
  }
#ifdef __HAIR__
  if (sd->type & PRIMITIVE_CURVE) {
    return curve_attribute<T>(kg, sd, desc);
  }
#endif
#ifdef __POINTCLOUD__
  else if (sd->type & PRIMITIVE_POINT) {
    return point_attribute<T>(kg, sd, desc);
  }
#endif
  else {
    return make_zero<T>();
  }
}

/* Set sd->N to the undisplaced normal. For smooth shading, use the stored undisplaced
 * normal attribute. For flat shading, compute the geometric face normal from undisplaced
 * triangle positions. */
ccl_device void primitive_normal_set_undisplaced(KernelGlobals kg,
                                                 ccl_private ShaderData *sd,
                                                 const int position_undisplaced_offset)
{
  float3 N;

  if (sd->shader & SHADER_SMOOTH_NORMAL) {
    const AttributeDescriptor ndesc = find_attribute(kg, sd, ATTR_STD_NORMAL_UNDISPLACED);
    if (!is_attribute_found(ndesc)) {
      return;
    }
    N = safe_normalize(primitive_surface_attribute<float3>(kg, sd, ndesc));
  }
  else {
    N = triangle_face_normal_undisplaced(kg, sd, position_undisplaced_offset);
  }

  object_normal_transform(kg, sd, &N);
  sd->N = (sd->flag & SD_BACKFACING) ? -N : N;
}

#ifdef __VOLUME__
/* Volume Attributes
 *
 * Read geometry attributes for volume shading. This is distinct from surface
 * attributes for performance, mainly for GPU performance to avoid bringing in
 * heavy volume interpolation code. */

ccl_device_forceinline bool primitive_is_volume_attribute(const ccl_private ShaderData *sd)
{
  return sd->type == PRIMITIVE_VOLUME;
}

template<typename T>
ccl_device_inline T primitive_volume_attribute(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               const AttributeDescriptor desc,
                                               const bool stochastic)
{
  if (primitive_is_volume_attribute(sd)) {
    return volume_attribute_value<T>(volume_attribute_float4(kg, sd, desc, stochastic));
  }
  return make_zero<T>();
}
#endif

/* Default UV coordinate */

ccl_device_forceinline float3 primitive_uv(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_UV);

  if (!is_attribute_found(desc)) {
    return make_float3(0.0f, 0.0f, 0.0f);
  }

  const float2 uv = primitive_surface_attribute<float2>(kg, sd, desc);
  return make_float3(uv.x, uv.y, 1.0f);
}

/* PTEX coordinates. */

ccl_device bool primitive_ptex(KernelGlobals kg,
                               ccl_private ShaderData *sd,
                               ccl_private float2 *uv,
                               ccl_private int *face_id)
{
  /* storing ptex data as attributes is not memory efficient but simple for tests */
  const AttributeDescriptor desc_face_id = find_attribute(kg, sd, ATTR_STD_PTEX_FACE_ID);
  const AttributeDescriptor desc_uv = find_attribute(kg, sd, ATTR_STD_PTEX_UV);

  if (!is_attribute_found(desc_face_id) || !is_attribute_found(desc_uv)) {
    return false;
  }

  const float3 uv3 = primitive_surface_attribute<float3>(kg, sd, desc_uv);
  const float face_id_f = primitive_surface_attribute<float>(kg, sd, desc_face_id);

  *uv = make_float2(uv3.x, uv3.y);
  *face_id = (int)face_id_f;

  return true;
}

/* Surface tangent */

template<typename Float3Type>
ccl_device Float3Type primitive_tangent(KernelGlobals kg, ccl_private ShaderData *sd)
{
#if defined(__HAIR__) || defined(__POINTCLOUD__)
  if (sd->type & (PRIMITIVE_CURVE | PRIMITIVE_POINT)) {
#  ifdef __DPDU__
    return Float3Type(normalize(sd->dPdu));
  }
#  else
    return make_zero<Float3Type>();
#  endif
#endif

  /* try to create spherical tangent from generated coordinates */
  const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_GENERATED);

  if (is_attribute_found(desc)) {
    if constexpr (is_dual_v<Float3Type>) {
      dual3 data = primitive_surface_attribute<dual3>(kg, sd, desc);
      data = make_float3(-(data.y() - 0.5f), (data.x() - 0.5f), dual1());
      object_normal_transform(kg, sd, &data);
      return cross(sd->N, normalize(cross(data, sd->N)));
    }
    else {
      float3 data = primitive_surface_attribute<float3>(kg, sd, desc);
      data = make_float3(-(data.y - 0.5f), (data.x - 0.5f), 0.0f);
      object_normal_transform(kg, sd, &data);
      return cross(sd->N, normalize(cross(data, sd->N)));
    }
  }
  /* otherwise use surface derivatives */
#ifdef __DPDU__
  return Float3Type(normalize(sd->dPdu));
#else
  return make_zero<Float3Type>();
#endif
}

/* Motion vector common */

ccl_device_inline float3 primitive_motion_position(KernelGlobals kg,
                                                   const ccl_private ShaderData *sd,
                                                   const int offset)
{
#if defined(__HAIR__)
  if (sd->type & PRIMITIVE_CURVE) {
    const KernelCurve curve = kernel_data_fetch(curves, sd->prim);
    const int k0 = curve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
    const int k1 = k0 + 1;
    const float4 f0 = kernel_data_fetch(curve_keys, offset + k0);
    const float4 f1 = kernel_data_fetch(curve_keys, offset + k1);
    return make_float3(mix(f0, f1, sd->u));
  }
#endif
#if defined(__POINTCLOUD__)
  if (sd->type & PRIMITIVE_POINT) {
    return make_float3(kernel_data_fetch(points, offset + sd->prim));
  }
#endif
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);
  const float3 v0 = kernel_data_fetch(tri_verts, offset + tri_vindex.x);
  const float3 v1 = kernel_data_fetch(tri_verts, offset + tri_vindex.y);
  const float3 v2 = kernel_data_fetch(tri_verts, offset + tri_vindex.z);
  return triangle_interpolate(sd->u, sd->v, v0, v1, v2);
}

ccl_device_forceinline void primitive_motion_data_without_camera(KernelGlobals kg,
                                                                 const ccl_private ShaderData *sd,
                                                                 ccl_private float3 *motion_center,
                                                                 ccl_private float3 *motion_pre,
                                                                 ccl_private float3 *motion_post)
{
#if defined(__HAIR__) || defined(__POINTCLOUD__)
  const bool is_curve_or_point = sd->type & (PRIMITIVE_CURVE | PRIMITIVE_POINT);
  if (is_curve_or_point) {
    *motion_center = make_float3(0.0f, 0.0f, 0.0f);

    if (sd->type & PRIMITIVE_CURVE) {
#  if defined(__HAIR__)
      *motion_center = curve_motion_center_location(kg, sd);
#  endif
    }
    else if (sd->type & PRIMITIVE_POINT) {
#  if defined(__POINTCLOUD__)
      *motion_center = point_motion_center_location(kg, sd);
#  endif
    }

    if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      object_position_transform(kg, sd, motion_center);
    }
  }
  else
#endif
  {
    *motion_center = sd->P;
  }

  *motion_pre = *motion_center;
  *motion_post = *motion_center;

  /* deformation motion */
  const ccl_global KernelObject &kobject = kernel_data_fetch(objects, sd->object);
  const int pos_offset = kobject.position_offset;
  const int numverts = kobject.numverts;
  const int num_motion_steps = kobject.num_geom_steps;

  if (sd->object_flag & SD_OBJECT_HAS_VERTEX_MOTION) {
    /* Motion steps are stored after the center position in the dedicated position arrays. */
    int offset = pos_offset + numverts;
    *motion_pre = primitive_motion_position(kg, sd, offset);
    if (num_motion_steps > 2) {
      offset += numverts;
      *motion_post = primitive_motion_position(kg, sd, offset);
    }
    else {
      object_inverse_position_transform(kg, sd, motion_post);
    }
  }

  /* object motion. note that depending on the mesh having motion vectors, this
   * transformation was set match the world/object space of motion_pre/post */
  Transform tfm;

  tfm = object_fetch_motion_pass_transform(kg, sd->object, OBJECT_PASS_MOTION_PRE);
  *motion_pre = transform_point(&tfm, *motion_pre);

  tfm = object_fetch_motion_pass_transform(kg, sd->object, OBJECT_PASS_MOTION_POST);
  *motion_post = transform_point(&tfm, *motion_post);
}

/* Motion vector for motion pass */

ccl_device_forceinline float4 primitive_motion_vector(KernelGlobals kg,
                                                      const ccl_private ShaderData *sd)
{
  float3 motion_center, motion_pre, motion_post;
  primitive_motion_data_without_camera(kg, sd, &motion_center, &motion_pre, &motion_post);

  return camera_motion_vector(kg, motion_center, motion_pre, motion_post);
}

/* Motion vector for denoising backward motion pass */

ccl_device_forceinline float3
primitive_motion_vector_backward_depth_delta(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  Transform tfm;
  float3 motion_center, motion_pre, motion_post;
  primitive_motion_data_without_camera(kg, sd, &motion_center, &motion_pre, &motion_post);

  /* Get camera-space vectors for linear depth delta. */
  tfm = kernel_data.cam.worldtocamera;
  float3 motion_center_cam = transform_point(&tfm, motion_center);
  tfm = kernel_data.cam.motion_pass_pre;
  float3 motion_pre_cam = transform_point(&tfm, motion_pre);

  float4 motion = camera_motion_vector(kg, motion_center, motion_pre, motion_post);

  float linear_depth_delta_pre = motion_pre_cam.z - motion_center_cam.z;

  return make_float3(motion.x, motion.y, linear_depth_delta_pre);
}

/* Motion vector for reflections */

ccl_device_forceinline float3 project_reflection(const float3 reflector_P,
                                                 const float3 reflector_N,
                                                 const float3 P)
{
  /* See Ray Tracing Gems chapter 32.4.3 Reflection Motion Vectors */

  float3 reflector_to_o = P - reflector_P;
  float3 N = dot(reflector_N, reflector_to_o) < 0.0f ? -reflector_N : reflector_N;
  float3 T, B;
  make_orthonormals(N, &T, &B);

  float3 P_o = to_local(reflector_to_o, T, B, N);

  float z_o = max(P_o.z, 1e-5f);
  float x_o = P_o.x;
  float y_o = P_o.y;
  const float inverse_f = 0.0f;
  float z_i = 1.0f / (inverse_f - 1.0f / z_o);
  float x_i = -(z_i / z_o) * x_o;
  float y_i = -(z_i / z_o) * y_o;

  float3 P_i = reflector_P + to_global(make_float3(x_i, y_i, z_i), T, B, N);
  return P_i;
}

ccl_device_forceinline float4 primitive_motion_vector_reflection(KernelGlobals kg,
                                                                 const float3 reflector_P,
                                                                 const float3 reflector_N,
                                                                 const ccl_private ShaderData *sd)
{
  float3 motion_center, motion_pre, motion_post;
  primitive_motion_data_without_camera(kg, sd, &motion_center, &motion_pre, &motion_post);

  /* TODO: This does not handle cases where the reflector is moving. */
  motion_center = project_reflection(reflector_P, reflector_N, motion_center);
  motion_pre = project_reflection(reflector_P, reflector_N, motion_pre);
  motion_post = project_reflection(reflector_P, reflector_N, motion_post);

  return camera_motion_vector(kg, motion_center, motion_pre, motion_post);
}

CCL_NAMESPACE_END
