/* SPDX-FileCopyrightText: 2013 Pixar
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * Based on code from OpenSubdiv.
 */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/object.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

struct PatchHandle {
  int array_index, patch_index, vert_index;
};

ccl_device_inline int patch_map_resolve_quadrant(const float median,
                                                 ccl_private float *u,
                                                 ccl_private float *v)
{
  int quadrant = -1;

  if (*u < median) {
    if (*v < median) {
      quadrant = 0;
    }
    else {
      quadrant = 1;
      *v -= median;
    }
  }
  else {
    if (*v < median) {
      quadrant = 3;
    }
    else {
      quadrant = 2;
      *v -= median;
    }
    *u -= median;
  }

  return quadrant;
}

/* retrieve PatchHandle from patch coords */

ccl_device_inline PatchHandle
patch_map_find_patch(KernelGlobals kg, const int object, const int patch, float u, float v)
{
  PatchHandle handle;

  // TODO: temporarily disabled due to slight inaccuracies on ARM.
  // kernel_assert((u >= 0.0f) && (u <= 1.0f) && (v >= 0.0f) && (v <= 1.0f));

  int node = (object_patch_map_offset(kg, object) + patch) / 2;
  float median = 0.5f;

  for (int depth = 0; depth < 0xff; depth++) {
    const float delta = median * 0.5f;

    const int quadrant = patch_map_resolve_quadrant(median, &u, &v);
    kernel_assert(quadrant >= 0);

    const uint child = kernel_data_fetch(patches, node + quadrant);

    /* is the quadrant a hole? */
    if (!(child & PATCH_MAP_NODE_IS_SET)) {
      handle.array_index = -1;
      return handle;
    }

    const uint index = child & PATCH_MAP_NODE_INDEX_MASK;

    if (child & PATCH_MAP_NODE_IS_LEAF) {
      handle.array_index = kernel_data_fetch(patches, index + 0);
      handle.patch_index = kernel_data_fetch(patches, index + 1);
      handle.vert_index = kernel_data_fetch(patches, index + 2);

      return handle;
    }
    node = index;

    median = delta;
  }

  /* no leaf found */
  kernel_assert(0);

  handle.array_index = -1;
  return handle;
}

ccl_device_inline void patch_eval_bspline_weights(const float t,
                                                  ccl_private float *point,
                                                  ccl_private float *deriv)
{
  /* The four uniform cubic B-Spline basis functions evaluated at t */
  const float inv_6 = 1.0f / 6.0f;

  const float t2 = t * t;
  const float t3 = t * t2;

  point[0] = inv_6 * (1.0f - 3.0f * (t - t2) - t3);
  point[1] = inv_6 * (4.0f - 6.0f * t2 + 3.0f * t3);
  point[2] = inv_6 * (1.0f + 3.0f * (t + t2 - t3));
  point[3] = inv_6 * t3;

  /* Derivatives of the above four basis functions at t */
  deriv[0] = -0.5f * t2 + t - 0.5f;
  deriv[1] = 1.5f * t2 - 2.0f * t;
  deriv[2] = -1.5f * t2 + t + 0.5f;
  deriv[3] = 0.5f * t2;
}

ccl_device_inline void patch_eval_adjust_boundary_weights(const uint bits,
                                                          ccl_private float *s,
                                                          ccl_private float *t)
{
  const int boundary = ((bits >> 8) & 0xf);

  if (boundary & 1) {
    t[2] -= t[0];
    t[1] += 2 * t[0];
    t[0] = 0;
  }

  if (boundary & 2) {
    s[1] -= s[3];
    s[2] += 2 * s[3];
    s[3] = 0;
  }

  if (boundary & 4) {
    t[1] -= t[3];
    t[2] += 2 * t[3];
    t[3] = 0;
  }

  if (boundary & 8) {
    s[2] -= s[0];
    s[1] += 2 * s[0];
    s[0] = 0;
  }
}

ccl_device_inline int patch_eval_depth(const uint patch_bits)
{
  return (patch_bits & 0xf);
}

ccl_device_inline float patch_eval_param_fraction(const uint patch_bits)
{
  const bool non_quad_root = (patch_bits >> 4) & 0x1;
  const int depth = patch_eval_depth(patch_bits);

  if (non_quad_root) {
    return 1.0f / (float)(1 << (depth - 1));
  }
  return 1.0f / (float)(1 << depth);
}

ccl_device_inline void patch_eval_normalize_coords(const uint patch_bits,
                                                   ccl_private float *u,
                                                   ccl_private float *v)
{
  const float frac = patch_eval_param_fraction(patch_bits);

  const int iu = (patch_bits >> 22) & 0x3ff;
  const int iv = (patch_bits >> 12) & 0x3ff;

  /* top left corner */
  const float pu = (float)iu * frac;
  const float pv = (float)iv * frac;

  /* normalize uv coordinates */
  *u = (*u - pu) / frac;
  *v = (*v - pv) / frac;
}

/* retrieve patch control indices */

ccl_device_inline int patch_eval_indices(KernelGlobals kg,
                                         const ccl_private PatchHandle *handle,
                                         int /*channel*/,
                                         int indices[PATCH_MAX_CONTROL_VERTS])
{
  const int index_base = kernel_data_fetch(patches, handle->array_index + 2) + handle->vert_index;

  /* XXX: regular patches only */
  for (int i = 0; i < 16; i++) {
    indices[i] = kernel_data_fetch(patches, index_base + i);
  }

  return 16;
}

/* evaluate patch basis functions */

ccl_device_inline void patch_eval_basis(KernelGlobals kg,
                                        const ccl_private PatchHandle *handle,
                                        float u,
                                        float v,
                                        float weights[PATCH_MAX_CONTROL_VERTS],
                                        float weights_du[PATCH_MAX_CONTROL_VERTS],
                                        float weights_dv[PATCH_MAX_CONTROL_VERTS])
{
  const uint patch_bits = kernel_data_fetch(patches,
                                            handle->patch_index + 1); /* read patch param */
  float d_scale = 1 << patch_eval_depth(patch_bits);

  const bool non_quad_root = (patch_bits >> 4) & 0x1;
  if (non_quad_root) {
    d_scale *= 0.5f;
  }

  patch_eval_normalize_coords(patch_bits, &u, &v);

  /* XXX: regular patches only for now. */

  float s[4];
  float t[4];
  float ds[4];
  float dt[4];

  patch_eval_bspline_weights(u, s, ds);
  patch_eval_bspline_weights(v, t, dt);

  patch_eval_adjust_boundary_weights(patch_bits, s, t);
  patch_eval_adjust_boundary_weights(patch_bits, ds, dt);

  for (int k = 0; k < 4; k++) {
    for (int l = 0; l < 4; l++) {
      weights[4 * k + l] = s[l] * t[k];
      weights_du[4 * k + l] = ds[l] * t[k] * d_scale;
      weights_dv[4 * k + l] = s[l] * dt[k] * d_scale;
    }
  }
}

/* generic function for evaluating indices and weights from patch coords */

ccl_device_inline int patch_eval_control_verts(KernelGlobals kg,
                                               const int object,
                                               const int patch,
                                               const float u,
                                               const float v,
                                               const int channel,
                                               int indices[PATCH_MAX_CONTROL_VERTS],
                                               float weights[PATCH_MAX_CONTROL_VERTS],
                                               float weights_du[PATCH_MAX_CONTROL_VERTS],
                                               float weights_dv[PATCH_MAX_CONTROL_VERTS])
{
  const PatchHandle handle = patch_map_find_patch(kg, object, patch, u, v);
  kernel_assert(handle.array_index >= 0);

  const int num_control = patch_eval_indices(kg, &handle, channel, indices);
  patch_eval_basis(kg, &handle, u, v, weights, weights_du, weights_dv);

  return num_control;
}

/* functions for evaluating attributes on patches */

ccl_device float patch_eval_float(KernelGlobals kg,
                                  const ccl_private ShaderData *sd,
                                  const int offset,
                                  const int patch,
                                  const float u,
                                  const float v,
                                  const int channel,
                                  ccl_private float *du,
                                  ccl_private float *dv)
{
  int indices[PATCH_MAX_CONTROL_VERTS];
  float weights[PATCH_MAX_CONTROL_VERTS];
  float weights_du[PATCH_MAX_CONTROL_VERTS];
  float weights_dv[PATCH_MAX_CONTROL_VERTS];

  const int num_control = patch_eval_control_verts(
      kg, sd->object, patch, u, v, channel, indices, weights, weights_du, weights_dv);

  float val = 0.0f;
  if (du) {
    *du = 0.0f;
  }
  if (dv) {
    *dv = 0.0f;
  }

  for (int i = 0; i < num_control; i++) {
    const float v = kernel_data_fetch(attributes_float, offset + indices[i]);

    val += v * weights[i];
    if (du) {
      *du += v * weights_du[i];
    }
    if (dv) {
      *dv += v * weights_dv[i];
    }
  }

  return val;
}

ccl_device float2 patch_eval_float2(KernelGlobals kg,
                                    const ccl_private ShaderData *sd,
                                    const int offset,
                                    const int patch,
                                    const float u,
                                    const float v,
                                    const int channel,
                                    ccl_private float2 *du,
                                    ccl_private float2 *dv)
{
  int indices[PATCH_MAX_CONTROL_VERTS];
  float weights[PATCH_MAX_CONTROL_VERTS];
  float weights_du[PATCH_MAX_CONTROL_VERTS];
  float weights_dv[PATCH_MAX_CONTROL_VERTS];

  const int num_control = patch_eval_control_verts(
      kg, sd->object, patch, u, v, channel, indices, weights, weights_du, weights_dv);

  float2 val = make_float2(0.0f, 0.0f);
  if (du) {
    *du = make_float2(0.0f, 0.0f);
  }
  if (dv) {
    *dv = make_float2(0.0f, 0.0f);
  }

  for (int i = 0; i < num_control; i++) {
    const float2 v = kernel_data_fetch(attributes_float2, offset + indices[i]);

    val += v * weights[i];
    if (du) {
      *du += v * weights_du[i];
    }
    if (dv) {
      *dv += v * weights_dv[i];
    }
  }

  return val;
}

ccl_device float3 patch_eval_float3(KernelGlobals kg,
                                    const ccl_private ShaderData *sd,
                                    const int offset,
                                    const int patch,
                                    const float u,
                                    const float v,
                                    const int channel,
                                    ccl_private float3 *du,
                                    ccl_private float3 *dv)
{
  int indices[PATCH_MAX_CONTROL_VERTS];
  float weights[PATCH_MAX_CONTROL_VERTS];
  float weights_du[PATCH_MAX_CONTROL_VERTS];
  float weights_dv[PATCH_MAX_CONTROL_VERTS];

  const int num_control = patch_eval_control_verts(
      kg, sd->object, patch, u, v, channel, indices, weights, weights_du, weights_dv);

  float3 val = make_float3(0.0f, 0.0f, 0.0f);
  if (du) {
    *du = make_float3(0.0f, 0.0f, 0.0f);
  }
  if (dv) {
    *dv = make_float3(0.0f, 0.0f, 0.0f);
  }

  for (int i = 0; i < num_control; i++) {
    const float3 v = kernel_data_fetch(attributes_float3, offset + indices[i]);

    val += v * weights[i];
    if (du) {
      *du += v * weights_du[i];
    }
    if (dv) {
      *dv += v * weights_dv[i];
    }
  }

  return val;
}

ccl_device float4 patch_eval_float4(KernelGlobals kg,
                                    const ccl_private ShaderData *sd,
                                    const int offset,
                                    const int patch,
                                    const float u,
                                    const float v,
                                    const int channel,
                                    ccl_private float4 *du,
                                    ccl_private float4 *dv)
{
  int indices[PATCH_MAX_CONTROL_VERTS];
  float weights[PATCH_MAX_CONTROL_VERTS];
  float weights_du[PATCH_MAX_CONTROL_VERTS];
  float weights_dv[PATCH_MAX_CONTROL_VERTS];

  const int num_control = patch_eval_control_verts(
      kg, sd->object, patch, u, v, channel, indices, weights, weights_du, weights_dv);

  float4 val = zero_float4();
  if (du) {
    *du = zero_float4();
  }
  if (dv) {
    *dv = zero_float4();
  }

  for (int i = 0; i < num_control; i++) {
    const float4 v = kernel_data_fetch(attributes_float4, offset + indices[i]);

    val += v * weights[i];
    if (du) {
      *du += v * weights_du[i];
    }
    if (dv) {
      *dv += v * weights_dv[i];
    }
  }

  return val;
}

ccl_device float4 patch_eval_uchar4(KernelGlobals kg,
                                    const ccl_private ShaderData *sd,
                                    const int offset,
                                    const int patch,
                                    const float u,
                                    const float v,
                                    const int channel,
                                    ccl_private float4 *du,
                                    ccl_private float4 *dv)
{
  int indices[PATCH_MAX_CONTROL_VERTS];
  float weights[PATCH_MAX_CONTROL_VERTS];
  float weights_du[PATCH_MAX_CONTROL_VERTS];
  float weights_dv[PATCH_MAX_CONTROL_VERTS];

  const int num_control = patch_eval_control_verts(
      kg, sd->object, patch, u, v, channel, indices, weights, weights_du, weights_dv);

  float4 val = zero_float4();
  if (du) {
    *du = zero_float4();
  }
  if (dv) {
    *dv = zero_float4();
  }

  for (int i = 0; i < num_control; i++) {
    const float4 v = color_srgb_to_linear_v4(
        color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, offset + indices[i])));

    val += v * weights[i];
    if (du) {
      *du += v * weights_du[i];
    }
    if (dv) {
      *dv += v * weights_dv[i];
    }
  }

  return val;
}

CCL_NAMESPACE_END
