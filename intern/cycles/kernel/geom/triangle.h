/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

/* Triangle Primitive
 *
 * Basic triangle with 3 vertices is used to represent mesh surfaces. For BVH
 * ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage */

#pragma once

CCL_NAMESPACE_BEGIN

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
  else {
    return normalize(cross(v1 - v0, v2 - v0));
  }
}

/* Point and normal on triangle. */
ccl_device_inline void triangle_point_normal(KernelGlobals kg,
                                             int object,
                                             int prim,
                                             float u,
                                             float v,
                                             ccl_private float3 *P,
                                             ccl_private float3 *Ng,
                                             ccl_private int *shader)
{
  /* load triangle vertices */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  float3 v0 = kernel_data_fetch(tri_verts, tri_vindex.x);
  float3 v1 = kernel_data_fetch(tri_verts, tri_vindex.y);
  float3 v2 = kernel_data_fetch(tri_verts, tri_vindex.z);

  /* compute point */
  float w = 1.0f - u - v;
  *P = (w * v0 + u * v1 + v * v2);
  /* get object flags */
  int object_flag = kernel_data_fetch(object_flag, object);
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

ccl_device_inline void triangle_vertices(KernelGlobals kg, int prim, float3 P[3])
{
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);
  P[0] = kernel_data_fetch(tri_verts, tri_vindex.x);
  P[1] = kernel_data_fetch(tri_verts, tri_vindex.y);
  P[2] = kernel_data_fetch(tri_verts, tri_vindex.z);
}

/* Triangle vertex locations and vertex normals */

ccl_device_inline void triangle_vertices_and_normals(KernelGlobals kg,
                                                     int prim,
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
triangle_smooth_normal(KernelGlobals kg, float3 Ng, int prim, float u, float v)
{
  /* load triangle vertices */
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, prim);

  float3 n0 = kernel_data_fetch(tri_vnormal, tri_vindex.x);
  float3 n1 = kernel_data_fetch(tri_vnormal, tri_vindex.y);
  float3 n2 = kernel_data_fetch(tri_vnormal, tri_vindex.z);

  float3 N = safe_normalize((1.0f - u - v) * n0 + u * n1 + v * n2);

  return is_zero(N) ? Ng : N;
}

ccl_device_inline float3 triangle_smooth_normal_unnormalized(
    KernelGlobals kg, ccl_private const ShaderData *sd, float3 Ng, int prim, float u, float v)
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

  float3 N = (1.0f - u - v) * n0 + u * n1 + v * n2;

  return is_zero(N) ? Ng : N;
}

/* Ray differentials on triangle */

ccl_device_inline void triangle_dPdudv(KernelGlobals kg,
                                       int prim,
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

/* Reading attributes on various triangle elements */

ccl_device float triangle_attribute_float(KernelGlobals kg,
                                          ccl_private const ShaderData *sd,
                                          const AttributeDescriptor desc,
                                          ccl_private float *dx,
                                          ccl_private float *dy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER)) {
    float f0, f1, f2;

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
    if (dx)
      *dx = sd->du.dx * f1 + sd->dv.dx * f2 - (sd->du.dx + sd->dv.dx) * f0;
    if (dy)
      *dy = sd->du.dy * f1 + sd->dv.dy * f2 - (sd->du.dy + sd->dv.dy) * f0;
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
  else {
#ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = 0.0f;
    if (dy)
      *dy = 0.0f;
#endif

    if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim :
                                                               desc.offset;
      return kernel_data_fetch(attributes_float, offset);
    }
    else {
      return 0.0f;
    }
  }
}

ccl_device float2 triangle_attribute_float2(KernelGlobals kg,
                                            ccl_private const ShaderData *sd,
                                            const AttributeDescriptor desc,
                                            ccl_private float2 *dx,
                                            ccl_private float2 *dy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER)) {
    float2 f0, f1, f2;

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
    if (dx)
      *dx = sd->du.dx * f1 + sd->dv.dx * f2 - (sd->du.dx + sd->dv.dx) * f0;
    if (dy)
      *dy = sd->du.dy * f1 + sd->dv.dy * f2 - (sd->du.dy + sd->dv.dy) * f0;
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
  else {
#ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = make_float2(0.0f, 0.0f);
    if (dy)
      *dy = make_float2(0.0f, 0.0f);
#endif

    if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim :
                                                               desc.offset;
      return kernel_data_fetch(attributes_float2, offset);
    }
    else {
      return make_float2(0.0f, 0.0f);
    }
  }
}

ccl_device float3 triangle_attribute_float3(KernelGlobals kg,
                                            ccl_private const ShaderData *sd,
                                            const AttributeDescriptor desc,
                                            ccl_private float3 *dx,
                                            ccl_private float3 *dy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER)) {
    float3 f0, f1, f2;

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
    if (dx)
      *dx = sd->du.dx * f1 + sd->dv.dx * f2 - (sd->du.dx + sd->dv.dx) * f0;
    if (dy)
      *dy = sd->du.dy * f1 + sd->dv.dy * f2 - (sd->du.dy + sd->dv.dy) * f0;
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
  else {
#ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = make_float3(0.0f, 0.0f, 0.0f);
    if (dy)
      *dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

    if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim :
                                                               desc.offset;
      return kernel_data_fetch(attributes_float3, offset);
    }
    else {
      return make_float3(0.0f, 0.0f, 0.0f);
    }
  }
}

ccl_device float4 triangle_attribute_float4(KernelGlobals kg,
                                            ccl_private const ShaderData *sd,
                                            const AttributeDescriptor desc,
                                            ccl_private float4 *dx,
                                            ccl_private float4 *dy)
{
  if (desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_VERTEX_MOTION | ATTR_ELEMENT_CORNER |
                      ATTR_ELEMENT_CORNER_BYTE))
  {
    float4 f0, f1, f2;

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
    if (dx)
      *dx = sd->du.dx * f1 + sd->dv.dx * f2 - (sd->du.dx + sd->dv.dx) * f0;
    if (dy)
      *dy = sd->du.dy * f1 + sd->dv.dy * f2 - (sd->du.dy + sd->dv.dy) * f0;
#endif

    return sd->u * f1 + sd->v * f2 + (1.0f - sd->u - sd->v) * f0;
  }
  else {
#ifdef __RAY_DIFFERENTIALS__
    if (dx)
      *dx = zero_float4();
    if (dy)
      *dy = zero_float4();
#endif

    if (desc.element & (ATTR_ELEMENT_FACE | ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
      const int offset = (desc.element == ATTR_ELEMENT_FACE) ? desc.offset + sd->prim :
                                                               desc.offset;
      return kernel_data_fetch(attributes_float4, offset);
    }
    else {
      return zero_float4();
    }
  }
}

CCL_NAMESPACE_END
