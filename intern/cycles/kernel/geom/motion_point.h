/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/bvh/util.h"

CCL_NAMESPACE_BEGIN

/* Motion Point Primitive
 *
 * These are stored as regular points, plus extra positions and radii at times
 * other than the frame center. Computing the point at a given ray time is
 * a matter of interpolation of the two steps between which the ray time lies.
 *
 * The extra points are stored as additional motion steps in ATTR_STD_POSITION.
 */

#ifdef __POINTCLOUD__

ccl_device_inline float4 motion_point_for_step(
    KernelGlobals kg, int offset, const int numverts, const int numsteps, int step, const int prim)
{
  const int center_step = (numsteps - 1) / 2;
  if (step == center_step) {
    /* Center step: first in the array. */
  }
  else {
    /* Non-center step, stored after center  with center index skipped. */
    if (step < center_step) {
      step++;
    }
    offset += step * numverts;
  }

  return kernel_data_fetch(attributes_float4, offset + prim);
}

/* return 2 point key locations */
ccl_device_inline float4 motion_point(KernelGlobals kg,
                                      const int object,
                                      const int prim,
                                      const float time)
{
  /* get motion info */
  const int numsteps = kernel_data_fetch(objects, object).num_geom_steps;
  const int numverts = kernel_data_fetch(objects, object).numverts;

  /* figure out which steps we need to fetch and their interpolation factor */
  const int maxstep = numsteps - 1;
  const int step = min((int)(time * maxstep), maxstep - 1);
  const float t = time * maxstep - step;

  /* fetch key coordinates */
  const int offset = kernel_data_fetch(objects, object).position_offset;
  const float4 point = motion_point_for_step(kg, offset, numverts, numsteps, step, prim);
  const float4 next_point = motion_point_for_step(kg, offset, numverts, numsteps, step + 1, prim);

  /* interpolate between steps */
  return (1.0f - t) * point + t * next_point;
}

#endif

CCL_NAMESPACE_END
