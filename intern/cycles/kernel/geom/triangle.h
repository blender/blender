/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Triangle Primitive
 *
 * Basic triangle with 3 vertices is used to represent mesh surfaces. For BVH
 * ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/object.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

/* Evaluate a quantity at barycentric coordinates u, v, given the values at three triangle
 * vertices. */
template<typename T>
ccl_device_inline T
triangle_interpolate(const float u, const float v, const T f0, const T f1, const T f2)
{
  return (1.0f - u - v) * f0 + u * f1 + v * f2;
}

/* Normal on triangle. */
ccl_device_inline float3 triangle_normal(KernelGlobals kg, ccl_private ShaderData *sd)
{
  /* load triangle vertices */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);
  const float3 v0 = kernel_data_fetch(tri_verts, tri_vindex.x);
  const float3 v1 = kernel_data_fetch(tri_verts, tri_vindex.y);
  const float3 v2 = kernel_data_fetch(tri_verts, tri_vindex.z);

  /* return normal */
  if (object_negative_scale_applied(sd->object_flag)) {
    return normalize(cross(v2 - v0, v1 - v0));
  }
  return normalize(cross(v1 - v0, v2 - v0));
}

/* Point and normal on triangle. */
ccl_device_inline void triangle_point_normal(KernelGlobals kg,
                                             const int object,
                                             const int prim,
                                             const float u,
                                             const float v,
                                             ccl_private float3 *P,
                                             ccl_private float3 *Ng,
                                             ccl_private int *shader)
{
  /* load triangle vertices */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  const float3 v0 = kernel_data_fetch(tri_verts, tri_vindex.x);
  const float3 v1 = kernel_data_fetch(tri_verts, tri_vindex.y);
  const float3 v2 = kernel_data_fetch(tri_verts, tri_vindex.z);

  /* compute point */
  const float w = 1.0f - u - v;
  *P = (w * v0 + u * v1 + v * v2);
  /* get object flags */
  const int object_flag = kernel_data_fetch(object_flag, object);
  /* compute normal */
  if (object_negative_scale_applied(object_flag)) {
    *Ng = normalize(cross(v2 - v0, v1 - v0));
  }
  else {
    *Ng = normalize(cross(v1 - v0, v2 - v0));
  }
  /* shader`*/
  *shader = kernel_data_fetch(tri_shader, prim);
}

/* Triangle vertex locations */

ccl_device_inline void triangle_vertices(KernelGlobals kg, const int prim, float3 P[3])
{
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  P[0] = kernel_data_fetch(tri_verts, tri_vindex.x);
  P[1] = kernel_data_fetch(tri_verts, tri_vindex.y);
  P[2] = kernel_data_fetch(tri_verts, tri_vindex.z);
}

/* Triangle vertex locations and vertex normals */

ccl_device_inline void triangle_vertices_and_normals(KernelGlobals kg,
                                                     const int prim,
                                                     float3 P[3],
                                                     float3 N[3])
{
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  P[0] = kernel_data_fetch(tri_verts, tri_vindex.x);
  P[1] = kernel_data_fetch(tri_verts, tri_vindex.y);
  P[2] = kernel_data_fetch(tri_verts, tri_vindex.z);

  N[0] = kernel_data_fetch(tri_vnormal, tri_vindex.x);
  N[1] = kernel_data_fetch(tri_vnormal, tri_vindex.y);
  N[2] = kernel_data_fetch(tri_vnormal, tri_vindex.z);
}

/* Interpolate smooth vertex normal from vertices */

ccl_device_inline float3
triangle_smooth_normal(KernelGlobals kg, const float3 Ng, const int prim, const float u, float v)
{
  /* load triangle vertices */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);

  const float3 n0 = kernel_data_fetch(tri_vnormal, tri_vindex.x);
  const float3 n1 = kernel_data_fetch(tri_vnormal, tri_vindex.y);
  const float3 n2 = kernel_data_fetch(tri_vnormal, tri_vindex.z);

  const float3 N = safe_normalize((1.0f - u - v) * n0 + u * n1 + v * n2);

  return is_zero(N) ? Ng : N;
}

/* Compute triangle normals at the hit position, and offsetted positions in x and y direction for
 * bump mapping. */
ccl_device_inline float3 triangle_smooth_normal(KernelGlobals kg,
                                                const float3 Ng,
                                                const int prim,
                                                const float u,
                                                float v,
                                                const differential du,
                                                const differential dv,
                                                ccl_private float3 &N_x,
                                                ccl_private float3 &N_y)
{
  /* Load triangle vertices. */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);

  const float3 n0 = kernel_data_fetch(tri_vnormal, tri_vindex.x);
  const float3 n1 = kernel_data_fetch(tri_vnormal, tri_vindex.y);
  const float3 n2 = kernel_data_fetch(tri_vnormal, tri_vindex.z);

  const float3 N = safe_normalize(triangle_interpolate(u, v, n0, n1, n2));
  N_x = safe_normalize(triangle_interpolate(u + du.dx, v + dv.dx, n0, n1, n2));
  N_y = safe_normalize(triangle_interpolate(u + du.dy, v + dv.dy, n0, n1, n2));

  N_x = is_zero(N_x) ? Ng : N_x;
  N_y = is_zero(N_y) ? Ng : N_y;
  return is_zero(N) ? Ng : N;
}

ccl_device_inline float3 triangle_smooth_normal_unnormalized(KernelGlobals kg,
                                                             const ccl_private ShaderData *sd,
                                                             const float3 Ng,
                                                             const int prim,
                                                             const float u,
                                                             float v)
{
  /* load triangle vertices */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);

  float3 n0 = kernel_data_fetch(tri_vnormal, tri_vindex.x);
  float3 n1 = kernel_data_fetch(tri_vnormal, tri_vindex.y);
  float3 n2 = kernel_data_fetch(tri_vnormal, tri_vindex.z);

  /* ensure that the normals are in object space */
  if (sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED) {
    object_inverse_normal_transform(kg, sd, &n0);
    object_inverse_normal_transform(kg, sd, &n1);
    object_inverse_normal_transform(kg, sd, &n2);
  }

  const float3 N = (1.0f - u - v) * n0 + u * n1 + v * n2;

  return is_zero(N) ? Ng : N;
}

/* Ray differentials on triangle */

ccl_device_inline void triangle_dPdudv(KernelGlobals kg,
                                       const int prim,
                                       ccl_private float3 *dPdu,
                                       ccl_private float3 *dPdv)
{
  /* fetch triangle vertex coordinates */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  const float3 p0 = kernel_data_fetch(tri_verts, tri_vindex.x);
  const float3 p1 = kernel_data_fetch(tri_verts, tri_vindex.y);
  const float3 p2 = kernel_data_fetch(tri_verts, tri_vindex.z);

  /* compute derivatives of P w.r.t. uv */
  *dPdu = (p1 - p0);
  *dPdv = (p2 - p0);
}

/* Partial derivative of f w.r.t. x, namely ∂f/∂x.
 * f is a function of barycentric coordinates u, v, given by
 *       f(u, v) = f1 * u + f2 * v + f0 * (1 - u - v),
 * the derivatives are
 *           ∂f/∂u = (f1 - f0), ∂f/∂v = (f2 - f0).
 * The partial derivative in x is
 *    ∂f/∂x = ∂f/∂u * ∂u/∂x + ∂f/∂v * ∂v/∂x
 *          = (f1 - f0) * du.dx + (f2 - f0) * dv.dx. */
template<typename T>
ccl_device_inline T triangle_attribute_dfdx(const ccl_private differential &du,
                                            const ccl_private differential &dv,
                                            const ccl_private T &f0,
                                            const ccl_private T &f1,
                                            const ccl_private T &f2)
{
  return du.dx * f1 + dv.dx * f2 - (du.dx + dv.dx) * f0;
}

/* Partial derivative of f w.r.t. in x, namely ∂f/∂y, similarly computed as ∂f/∂x above. */
template<typename T>
ccl_device_inline T triangle_attribute_dfdy(const ccl_private differential &du,
                                            const ccl_private differential &dv,
                                            const ccl_private T &f0,
                                            const ccl_private T &f1,
                                            const ccl_private T &f2)
{
  return du.dy * f1 + dv.dy * f2 - (du.dy + dv.dy) * f0;
}

/* Read attributes on various triangle elements, and compute the partial derivatives if requested.
 */
ccl_device float triangle_attribute_float(KernelGlobals kg,
                                          const ccl_private ShaderData *sd,
                                          const AttributeDescriptor desc,
                                          ccl_private float *dfdx,
                                          ccl_private float *dfdy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER)) {
    float f0;
    float f1;
    float f2;

    if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION)) {
      const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);

      f0 = kernel_data_fetch(attributes_float, desc.offset + tri_vindex.x);
      f1 = kernel_data_fetch(attributes_float, desc.offset + tri_vindex.y);
      f2 = kernel_data_fetch(attributes_float, desc.offset + tri_vindex.z);
    }
    else {
      const int tri = desc.offset + sd->prim * 3;
      f0 = kernel_data_fetch(attributes_float, tri + 0);
      f1 = kernel_data_fetch(attributes_float, tri + 1);
      f2 = kernel_data_fetch(attributes_float, tri + 2);
    }

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, f0, f1, f2);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, f0, f1, f2);
    }
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
#ifdef __RAY_DIFFERENTIALS__
  if (dfdx) {
    *dfdx = 0.0f;
  }
  if (dfdy) {
    *dfdy = 0.0f;
  }
#endif

  if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
    const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim : desc.offset;
    return kernel_data_fetch(attributes_float, offset);
  }
  return 0.0f;
}

ccl_device float2 triangle_attribute_float2(KernelGlobals kg,
                                            const ccl_private ShaderData *sd,
                                            const AttributeDescriptor desc,
                                            ccl_private float2 *dfdx,
                                            ccl_private float2 *dfdy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER)) {
    float2 f0;
    float2 f1;
    float2 f2;

    if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION)) {
      const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);

      f0 = kernel_data_fetch(attributes_float2, desc.offset + tri_vindex.x);
      f1 = kernel_data_fetch(attributes_float2, desc.offset + tri_vindex.y);
      f2 = kernel_data_fetch(attributes_float2, desc.offset + tri_vindex.z);
    }
    else {
      const int tri = desc.offset + sd->prim * 3;
      f0 = kernel_data_fetch(attributes_float2, tri + 0);
      f1 = kernel_data_fetch(attributes_float2, tri + 1);
      f2 = kernel_data_fetch(attributes_float2, tri + 2);
    }

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, f0, f1, f2);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, f0, f1, f2);
    }
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
#ifdef __RAY_DIFFERENTIALS__
  if (dfdx) {
    *dfdx = zero_float2();
  }
  if (dfdy) {
    *dfdy = zero_float2();
  }
#endif

  if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
    const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim : desc.offset;
    return kernel_data_fetch(attributes_float2, offset);
  }
  return zero_float2();
}

ccl_device float3 triangle_attribute_float3(KernelGlobals kg,
                                            const ccl_private ShaderData *sd,
                                            const AttributeDescriptor desc,
                                            ccl_private float3 *dfdx,
                                            ccl_private float3 *dfdy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER)) {
    float3 f0;
    float3 f1;
    float3 f2;

    if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION)) {
      const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);

      f0 = kernel_data_fetch(attributes_float3, desc.offset + tri_vindex.x);
      f1 = kernel_data_fetch(attributes_float3, desc.offset + tri_vindex.y);
      f2 = kernel_data_fetch(attributes_float3, desc.offset + tri_vindex.z);
    }
    else {
      const int tri = desc.offset + sd->prim * 3;
      f0 = kernel_data_fetch(attributes_float3, tri + 0);
      f1 = kernel_data_fetch(attributes_float3, tri + 1);
      f2 = kernel_data_fetch(attributes_float3, tri + 2);
    }

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, f0, f1, f2);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, f0, f1, f2);
    }
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
#ifdef __RAY_DIFFERENTIALS__
  if (dfdx) {
    *dfdx = zero_float3();
  }
  if (dfdy) {
    *dfdy = zero_float3();
  }
#endif

  if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
    const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim : desc.offset;
    return kernel_data_fetch(attributes_float3, offset);
  }
  return zero_float3();
}

ccl_device float4 triangle_attribute_float4(KernelGlobals kg,
                                            const ccl_private ShaderData *sd,
                                            const AttributeDescriptor desc,
                                            ccl_private float4 *dfdx,
                                            ccl_private float4 *dfdy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER |
                      ATTR_ELEMENT_CORNER_BYTE))
  {
    float4 f0;
    float4 f1;
    float4 f2;

    if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION)) {
      const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);

      f0 = kernel_data_fetch(attributes_float4, desc.offset + tri_vindex.x);
      f1 = kernel_data_fetch(attributes_float4, desc.offset + tri_vindex.y);
      f2 = kernel_data_fetch(attributes_float4, desc.offset + tri_vindex.z);
    }
    else {
      const int tri = desc.offset + sd->prim * 3;
      if (desc.element == ATTR_ELEMENT_CORNER) {
        f0 = kernel_data_fetch(attributes_float4, tri + 0);
        f1 = kernel_data_fetch(attributes_float4, tri + 1);
        f2 = kernel_data_fetch(attributes_float4, tri + 2);
      }
      else {
        f0 = color_srgb_to_linear_v4(
            color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, tri + 0)));
        f1 = color_srgb_to_linear_v4(
            color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, tri + 1)));
        f2 = color_srgb_to_linear_v4(
            color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, tri + 2)));
      }
    }

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, f0, f1, f2);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, f0, f1, f2);
    }
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
#ifdef __RAY_DIFFERENTIALS__
  if (dfdx) {
    *dfdx = zero_float4();
  }
  if (dfdy) {
    *dfdy = zero_float4();
  }
#endif

  if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
    const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim : desc.offset;
    return kernel_data_fetch(attributes_float4, offset);
  }
  return zero_float4();
}

CCL_NAMESPACE_END
