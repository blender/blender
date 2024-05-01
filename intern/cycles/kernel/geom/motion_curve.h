/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Motion Curve Primitive
 *
 * These are stored as regular curves, plus extra positions and radii at times
 * other than the frame center. Computing the curve keys at a given ray time is
 * a matter of interpolation of the two steps between which the ray time lies.
 *
 * The extra curve keys are stored as ATTR_STD_MOTION_VERTEX_POSITION.
 */

#ifdef __HAIR__

ccl_device_inline void motion_curve_keys_for_step_linear(KernelGlobals kg,
                                                         int offset,
                                                         int numverts,
                                                         int numsteps,
                                                         int step,
                                                         int k0,
                                                         int k1,
                                                         float4 keys[2])
{
  if (step == numsteps) {
    /* center step: regular key location */
    keys[0] = kernel_data_fetch(curve_keys, k0);
    keys[1] = kernel_data_fetch(curve_keys, k1);
  }
  else {
    /* center step is not stored in this array */
    if (step > numsteps)
      step--;

    offset += step * numverts;

    keys[0] = kernel_data_fetch(attributes_float4, offset + k0);
    keys[1] = kernel_data_fetch(attributes_float4, offset + k1);
  }
}

/* return 2 curve key locations */
ccl_device_inline void motion_curve_keys_linear(
    KernelGlobals kg, int object, float time, int k0, int k1, float4 keys[2])
{
  /* get motion info */
  const int numsteps = kernel_data_fetch(objects, object).numsteps;
  const int numverts = kernel_data_fetch(objects, object).numverts;

  /* figure out which steps we need to fetch and their interpolation factor */
  const int maxstep = numsteps * 2;
  const int step = min((int)(time * maxstep), maxstep - 1);
  const float t = time * maxstep - step;

  /* find attribute */
  const int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_POSITION);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* fetch key coordinates */
  float4 next_keys[2];

  motion_curve_keys_for_step_linear(kg, offset, numverts, numsteps, step, k0, k1, keys);
  motion_curve_keys_for_step_linear(kg, offset, numverts, numsteps, step + 1, k0, k1, next_keys);

  /* interpolate between steps */
  keys[0] = (1.0f - t) * keys[0] + t * next_keys[0];
  keys[1] = (1.0f - t) * keys[1] + t * next_keys[1];
}

ccl_device_inline void motion_curve_keys_for_step(KernelGlobals kg,
                                                  int offset,
                                                  int numverts,
                                                  int numsteps,
                                                  int step,
                                                  int k0,
                                                  int k1,
                                                  int k2,
                                                  int k3,
                                                  float4 keys[4])
{
  if (step == numsteps) {
    /* center step: regular key location */
    keys[0] = kernel_data_fetch(curve_keys, k0);
    keys[1] = kernel_data_fetch(curve_keys, k1);
    keys[2] = kernel_data_fetch(curve_keys, k2);
    keys[3] = kernel_data_fetch(curve_keys, k3);
  }
  else {
    /* center step is not stored in this array */
    if (step > numsteps)
      step--;

    offset += step * numverts;

    keys[0] = kernel_data_fetch(attributes_float4, offset + k0);
    keys[1] = kernel_data_fetch(attributes_float4, offset + k1);
    keys[2] = kernel_data_fetch(attributes_float4, offset + k2);
    keys[3] = kernel_data_fetch(attributes_float4, offset + k3);
  }
}

/* return 2 curve key locations */
ccl_device_inline void motion_curve_keys(
    KernelGlobals kg, int object, float time, int k0, int k1, int k2, int k3, float4 keys[4])
{
  /* get motion info */
  const int numsteps = kernel_data_fetch(objects, object).numsteps;
  const int numverts = kernel_data_fetch(objects, object).numverts;

  /* figure out which steps we need to fetch and their interpolation factor */
  const int maxstep = numsteps * 2;
  const int step = min((int)(time * maxstep), maxstep - 1);
  const float t = time * maxstep - step;

  /* find attribute */
  const int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_POSITION);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* fetch key coordinates */
  float4 next_keys[4];

  motion_curve_keys_for_step(kg, offset, numverts, numsteps, step, k0, k1, k2, k3, keys);
  motion_curve_keys_for_step(kg, offset, numverts, numsteps, step + 1, k0, k1, k2, k3, next_keys);

  /* interpolate between steps */
  keys[0] = (1.0f - t) * keys[0] + t * next_keys[0];
  keys[1] = (1.0f - t) * keys[1] + t * next_keys[1];
  keys[2] = (1.0f - t) * keys[2] + t * next_keys[2];
  keys[3] = (1.0f - t) * keys[3] + t * next_keys[3];
}

#endif

CCL_NAMESPACE_END
