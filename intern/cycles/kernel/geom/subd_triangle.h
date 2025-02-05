/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Functions for retrieving attributes on triangles produced from subdivision meshes */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/patch.h"
#include "kernel/geom/triangle.h"

CCL_NAMESPACE_BEGIN

/* UV coords of triangle within patch */

ccl_device_inline void subd_triangle_patch_uv(KernelGlobals kg,
                                              const ccl_private ShaderData *sd,
                                              float2 uv[3])
{
  const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);

  uv[0] = kernel_data_fetch(tri_patch_uv, tri_vindex.x);
  uv[1] = kernel_data_fetch(tri_patch_uv, tri_vindex.y);
  uv[2] = kernel_data_fetch(tri_patch_uv, tri_vindex.z);
}

/* Vertex indices of patch */

ccl_device_inline uint4 subd_triangle_patch_indices(KernelGlobals kg, const int patch)
{
  uint4 indices;

  indices.x = kernel_data_fetch(patches, patch + 0);
  indices.y = kernel_data_fetch(patches, patch + 1);
  indices.z = kernel_data_fetch(patches, patch + 2);
  indices.w = kernel_data_fetch(patches, patch + 3);

  return indices;
}

/* Originating face for patch */

ccl_device_inline uint subd_triangle_patch_face(KernelGlobals kg, const int patch)
{
  return kernel_data_fetch(patches, patch + 4);
}

/* Number of corners on originating face */

ccl_device_inline uint subd_triangle_patch_num_corners(KernelGlobals kg, const int patch)
{
  return kernel_data_fetch(patches, patch + 5) & 0xffff;
}

/* Indices of the four corners that are used by the patch */

ccl_device_inline void subd_triangle_patch_corners(KernelGlobals kg,
                                                   const int patch,
                                                   int corners[4])
{
  uint4 data;

  data.x = kernel_data_fetch(patches, patch + 4);
  data.y = kernel_data_fetch(patches, patch + 5);
  data.z = kernel_data_fetch(patches, patch + 6);
  data.w = kernel_data_fetch(patches, patch + 7);

  const int num_corners = data.y & 0xffff;

  if (num_corners == 4) {
    /* quad */
    corners[0] = data.z;
    corners[1] = data.z + 1;
    corners[2] = data.z + 2;
    corners[3] = data.z + 3;
  }
  else {
    /* ngon */
    const int c = data.y >> 16;

    corners[0] = data.z + c;
    corners[1] = data.z + mod(c + 1, num_corners);
    corners[2] = data.w;
    corners[3] = data.z + mod(c - 1, num_corners);
  }
}

template<typename T>
ccl_device_inline void subd_triangle_attribute_df(const ccl_private differential &du,
                                                  const ccl_private differential &dv,
                                                  const ccl_private T &dads,
                                                  const ccl_private T &dadt,
                                                  const float2 dpdu,
                                                  const float2 dpdv,
                                                  ccl_private T *dfdx,
                                                  ccl_private T *dfdy)
{
  if (!(dfdx || dfdy)) {
    return;
  }

  const float dsdu = dpdu.x;
  const float dtdu = dpdu.y;
  const float dsdv = dpdv.x;
  const float dtdv = dpdv.y;

  if (dfdx) {
    const float dudx = du.dx;
    const float dvdx = dv.dx;

    const float dsdx = dsdu * dudx + dsdv * dvdx;
    const float dtdx = dtdu * dudx + dtdv * dvdx;

    *dfdx = dads * dsdx + dadt * dtdx;
  }
  if (dfdy) {
    const float dudy = du.dy;
    const float dvdy = dv.dy;

    const float dsdy = dsdu * dudy + dsdv * dvdy;
    const float dtdy = dtdu * dudy + dtdv * dvdy;

    *dfdy = dads * dsdy + dadt * dtdy;
  }
}

/* Reading attributes on various subdivision triangle elements */

ccl_device_noinline float subd_triangle_attribute_float(KernelGlobals kg,
                                                        const ccl_private ShaderData *sd,
                                                        const AttributeDescriptor desc,
                                                        ccl_private float *dfdx,
                                                        ccl_private float *dfdy)
{
  const int patch = subd_triangle_patch(kg, sd->prim);

#ifdef __PATCH_EVAL__
  if (desc.flags & ATTR_SUBDIVIDED) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const float2 dpdu = uv[1] - uv[0];
    const float2 dpdv = uv[2] - uv[0];

    /* p is [s, t] */
    const float2 p = dpdu * sd->u + dpdv * sd->v + uv[0];

    float a;
    float dads;
    float dadt;
    a = patch_eval_float(kg, sd, desc.offset, patch, p.x, p.y, 0, &dads, &dadt);

#  ifdef __RAY_DIFFERENTIALS__
    subd_triangle_attribute_df(sd->du, sd->dv, dads, dadt, dpdu, dpdv, dfdx, dfdy);
#  endif

    return a;
  }
#endif /* __PATCH_EVAL__ */
  if (desc.element == ATTR_ELEMENT_FACE) {
    if (dfdx) {
      *dfdx = 0.0f;
    }
    if (dfdy) {
      *dfdy = 0.0f;
    }

    return kernel_data_fetch(attributes_float, desc.offset + subd_triangle_patch_face(kg, patch));
  }
  if (desc.element == ATTR_ELEMENT_VERTEX || desc.element == ATTR_ELEMENT_VERTEX_MOTION) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const uint4 v = subd_triangle_patch_indices(kg, patch);

    const float f0 = kernel_data_fetch(attributes_float, desc.offset + v.x);
    float f1 = kernel_data_fetch(attributes_float, desc.offset + v.y);
    const float f2 = kernel_data_fetch(attributes_float, desc.offset + v.z);
    float f3 = kernel_data_fetch(attributes_float, desc.offset + v.w);

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_CORNER) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    int corners[4];
    subd_triangle_patch_corners(kg, patch, corners);

    const float f0 = kernel_data_fetch(attributes_float, corners[0] + desc.offset);
    float f1 = kernel_data_fetch(attributes_float, corners[1] + desc.offset);
    const float f2 = kernel_data_fetch(attributes_float, corners[2] + desc.offset);
    float f3 = kernel_data_fetch(attributes_float, corners[3] + desc.offset);

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_OBJECT || desc.element == ATTR_ELEMENT_MESH) {
    if (dfdx) {
      *dfdx = 0.0f;
    }
    if (dfdy) {
      *dfdy = 0.0f;
    }

    return kernel_data_fetch(attributes_float, desc.offset);
  }

  if (dfdx) {
    *dfdx = 0.0f;
  }
  if (dfdy) {
    *dfdy = 0.0f;
  }
  return 0.0f;
}

ccl_device_noinline float2 subd_triangle_attribute_float2(KernelGlobals kg,
                                                          const ccl_private ShaderData *sd,
                                                          const AttributeDescriptor desc,
                                                          ccl_private float2 *dfdx,
                                                          ccl_private float2 *dfdy)
{
  const int patch = subd_triangle_patch(kg, sd->prim);

#ifdef __PATCH_EVAL__
  if (desc.flags & ATTR_SUBDIVIDED) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const float2 dpdu = uv[1] - uv[0];
    const float2 dpdv = uv[2] - uv[0];

    /* p is [s, t] */
    const float2 p = dpdu * sd->u + dpdv * sd->v + uv[0];

    float2 a;
    float2 dads;
    float2 dadt;

    a = patch_eval_float2(kg, sd, desc.offset, patch, p.x, p.y, 0, &dads, &dadt);

#  ifdef __RAY_DIFFERENTIALS__
    subd_triangle_attribute_df(sd->du, sd->dv, dads, dadt, dpdu, dpdv, dfdx, dfdy);
#  endif

    return a;
  }
#endif /* __PATCH_EVAL__ */
  if (desc.element == ATTR_ELEMENT_FACE) {
    if (dfdx) {
      *dfdx = make_float2(0.0f, 0.0f);
    }
    if (dfdy) {
      *dfdy = make_float2(0.0f, 0.0f);
    }

    return kernel_data_fetch(attributes_float2, desc.offset + subd_triangle_patch_face(kg, patch));
  }
  if (desc.element == ATTR_ELEMENT_VERTEX || desc.element == ATTR_ELEMENT_VERTEX_MOTION) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const uint4 v = subd_triangle_patch_indices(kg, patch);

    const float2 f0 = kernel_data_fetch(attributes_float2, desc.offset + v.x);
    float2 f1 = kernel_data_fetch(attributes_float2, desc.offset + v.y);
    const float2 f2 = kernel_data_fetch(attributes_float2, desc.offset + v.z);
    float2 f3 = kernel_data_fetch(attributes_float2, desc.offset + v.w);

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float2 a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float2 b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float2 c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_CORNER) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    int corners[4];
    subd_triangle_patch_corners(kg, patch, corners);

    float2 f0;
    float2 f1;
    float2 f2;
    float2 f3;

    f0 = kernel_data_fetch(attributes_float2, corners[0] + desc.offset);
    f1 = kernel_data_fetch(attributes_float2, corners[1] + desc.offset);
    f2 = kernel_data_fetch(attributes_float2, corners[2] + desc.offset);
    f3 = kernel_data_fetch(attributes_float2, corners[3] + desc.offset);

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float2 a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float2 b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float2 c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_OBJECT || desc.element == ATTR_ELEMENT_MESH) {
    if (dfdx) {
      *dfdx = zero_float2();
    }
    if (dfdy) {
      *dfdy = zero_float2();
    }

    return kernel_data_fetch(attributes_float2, desc.offset);
  }

  if (dfdx) {
    *dfdx = zero_float2();
  }
  if (dfdy) {
    *dfdy = zero_float2();
  }

  return zero_float2();
}

ccl_device_noinline float3 subd_triangle_attribute_float3(KernelGlobals kg,
                                                          const ccl_private ShaderData *sd,
                                                          const AttributeDescriptor desc,
                                                          ccl_private float3 *dfdx,
                                                          ccl_private float3 *dfdy)
{
  const int patch = subd_triangle_patch(kg, sd->prim);

#ifdef __PATCH_EVAL__
  if (desc.flags & ATTR_SUBDIVIDED) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const float2 dpdu = uv[1] - uv[0];
    const float2 dpdv = uv[2] - uv[0];

    /* p is [s, t] */
    const float2 p = dpdu * sd->u + dpdv * sd->v + uv[0];

    float3 a;
    float3 dads;
    float3 dadt;
    a = patch_eval_float3(kg, sd, desc.offset, patch, p.x, p.y, 0, &dads, &dadt);

#  ifdef __RAY_DIFFERENTIALS__
    subd_triangle_attribute_df(sd->du, sd->dv, dads, dadt, dpdu, dpdv, dfdx, dfdy);
#  endif

    return a;
  }
#endif /* __PATCH_EVAL__ */
  if (desc.element == ATTR_ELEMENT_FACE) {
    if (dfdx) {
      *dfdx = zero_float3();
    }
    if (dfdy) {
      *dfdy = zero_float3();
    }

    return kernel_data_fetch(attributes_float3, desc.offset + subd_triangle_patch_face(kg, patch));
  }
  if (desc.element == ATTR_ELEMENT_VERTEX || desc.element == ATTR_ELEMENT_VERTEX_MOTION) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const uint4 v = subd_triangle_patch_indices(kg, patch);

    const float3 f0 = kernel_data_fetch(attributes_float3, desc.offset + v.x);
    float3 f1 = kernel_data_fetch(attributes_float3, desc.offset + v.y);
    const float3 f2 = kernel_data_fetch(attributes_float3, desc.offset + v.z);
    float3 f3 = kernel_data_fetch(attributes_float3, desc.offset + v.w);

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float3 a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float3 b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float3 c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_CORNER) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    int corners[4];
    subd_triangle_patch_corners(kg, patch, corners);

    float3 f0;
    float3 f1;
    float3 f2;
    float3 f3;

    f0 = kernel_data_fetch(attributes_float3, corners[0] + desc.offset);
    f1 = kernel_data_fetch(attributes_float3, corners[1] + desc.offset);
    f2 = kernel_data_fetch(attributes_float3, corners[2] + desc.offset);
    f3 = kernel_data_fetch(attributes_float3, corners[3] + desc.offset);

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float3 a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float3 b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float3 c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_OBJECT || desc.element == ATTR_ELEMENT_MESH) {
    if (dfdx) {
      *dfdx = zero_float3();
    }
    if (dfdy) {
      *dfdy = zero_float3();
    }

    return kernel_data_fetch(attributes_float3, desc.offset);
  }

  if (dfdx) {
    *dfdx = zero_float3();
  }
  if (dfdy) {
    *dfdy = zero_float3();
  }
  return zero_float3();
}

ccl_device_noinline float4 subd_triangle_attribute_float4(KernelGlobals kg,
                                                          const ccl_private ShaderData *sd,
                                                          const AttributeDescriptor desc,
                                                          ccl_private float4 *dfdx,
                                                          ccl_private float4 *dfdy)
{
  const int patch = subd_triangle_patch(kg, sd->prim);

#ifdef __PATCH_EVAL__
  if (desc.flags & ATTR_SUBDIVIDED) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const float2 dpdu = uv[1] - uv[0];
    const float2 dpdv = uv[2] - uv[0];

    /* p is [s, t] */
    const float2 p = dpdu * sd->u + dpdv * sd->v + uv[0];

    float4 a;
    float4 dads;
    float4 dadt;
    if (desc.type == NODE_ATTR_RGBA) {
      a = patch_eval_uchar4(kg, sd, desc.offset, patch, p.x, p.y, 0, &dads, &dadt);
    }
    else {
      a = patch_eval_float4(kg, sd, desc.offset, patch, p.x, p.y, 0, &dads, &dadt);
    }

#  ifdef __RAY_DIFFERENTIALS__
    subd_triangle_attribute_df(sd->du, sd->dv, dads, dadt, dpdu, dpdv, dfdx, dfdy);
#  endif

    return a;
  }
#endif /* __PATCH_EVAL__ */
  if (desc.element == ATTR_ELEMENT_FACE) {
    if (dfdx) {
      *dfdx = zero_float4();
    }
    if (dfdy) {
      *dfdy = zero_float4();
    }

    return kernel_data_fetch(attributes_float4, desc.offset + subd_triangle_patch_face(kg, patch));
  }
  if (desc.element == ATTR_ELEMENT_VERTEX || desc.element == ATTR_ELEMENT_VERTEX_MOTION) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    const uint4 v = subd_triangle_patch_indices(kg, patch);

    const float4 f0 = kernel_data_fetch(attributes_float4, desc.offset + v.x);
    float4 f1 = kernel_data_fetch(attributes_float4, desc.offset + v.y);
    const float4 f2 = kernel_data_fetch(attributes_float4, desc.offset + v.z);
    float4 f3 = kernel_data_fetch(attributes_float4, desc.offset + v.w);

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float4 a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float4 b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float4 c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_CORNER || desc.element == ATTR_ELEMENT_CORNER_BYTE) {
    float2 uv[3];
    subd_triangle_patch_uv(kg, sd, uv);

    int corners[4];
    subd_triangle_patch_corners(kg, patch, corners);

    float4 f0;
    float4 f1;
    float4 f2;
    float4 f3;

    if (desc.element == ATTR_ELEMENT_CORNER_BYTE) {
      f0 = color_srgb_to_linear_v4(
          color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, corners[0] + desc.offset)));
      f1 = color_srgb_to_linear_v4(
          color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, corners[1] + desc.offset)));
      f2 = color_srgb_to_linear_v4(
          color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, corners[2] + desc.offset)));
      f3 = color_srgb_to_linear_v4(
          color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, corners[3] + desc.offset)));
    }
    else {
      f0 = kernel_data_fetch(attributes_float4, corners[0] + desc.offset);
      f1 = kernel_data_fetch(attributes_float4, corners[1] + desc.offset);
      f2 = kernel_data_fetch(attributes_float4, corners[2] + desc.offset);
      f3 = kernel_data_fetch(attributes_float4, corners[3] + desc.offset);
    }

    if (subd_triangle_patch_num_corners(kg, patch) != 4) {
      f1 = (f1 + f0) * 0.5f;
      f3 = (f3 + f0) * 0.5f;
    }

    const float4 a = mix(mix(f0, f1, uv[0].x), mix(f3, f2, uv[0].x), uv[0].y);
    const float4 b = mix(mix(f0, f1, uv[1].x), mix(f3, f2, uv[1].x), uv[1].y);
    const float4 c = mix(mix(f0, f1, uv[2].x), mix(f3, f2, uv[2].x), uv[2].y);

#ifdef __RAY_DIFFERENTIALS__
    if (dfdx) {
      *dfdx = triangle_attribute_dfdx(sd->du, sd->dv, a, b, c);
    }
    if (dfdy) {
      *dfdy = triangle_attribute_dfdy(sd->du, sd->dv, a, b, c);
    }
#endif

    return sd->u * b + sd->v * c + (1.0f - sd->u - sd->v) * a;
  }
  if (desc.element == ATTR_ELEMENT_OBJECT || desc.element == ATTR_ELEMENT_MESH) {
    if (dfdx) {
      *dfdx = zero_float4();
    }
    if (dfdy) {
      *dfdy = zero_float4();
    }

    return kernel_data_fetch(attributes_float4, desc.offset);
  }

  if (dfdx) {
    *dfdx = zero_float4();
  }
  if (dfdy) {
    *dfdy = zero_float4();
  }
  return zero_float4();
}

CCL_NAMESPACE_END
