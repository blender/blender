/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

CCL_NAMESPACE_BEGIN

/* Motion Point Primitive
 *
 * These are stored as regular points, plus extra positions and radii at times
 * other than the frame center. Computing the point at a given ray time is
 * a matter of interpolation of the two steps between which the ray time lies.
 *
 * The extra points are stored as ATTR_STD_MOTION_VERTEX_POSITION.
 */

#ifdef __POINTCLOUD__

ccl_device_inline float4
motion_point_for_step(KernelGlobals kg, int offset, int numverts, int numsteps, int step, int prim)
{
  if (step == numsteps) {
    /* center step: regular key location */
    return kernel_data_fetch(points, prim);
  }
  else {
    /* center step is not stored in this array */
    if (step > numsteps)
      step--;

    offset += step * numverts;

    return kernel_data_fetch(attributes_float4, offset + prim);
  }
}

/* return 2 point key locations */
ccl_device_inline float4 motion_point(KernelGlobals kg, int object, int prim, float time)
{
  /* get motion info */
  const int numsteps = kernel_data_fetch(objects, object).numsteps;
  const int numverts = kernel_data_fetch(objects, object).numverts;

  /* figure out which steps we need to fetch and their interpolation factor */
  int maxstep = numsteps * 2;
  int step = min((int)(time * maxstep), maxstep - 1);
  float t = time * maxstep - step;

  /* find attribute */
  int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_POSITION);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* fetch key coordinates */
  float4 point = motion_point_for_step(kg, offset, numverts, numsteps, step, prim);
  float4 next_point = motion_point_for_step(kg, offset, numverts, numsteps, step + 1, prim);

  /* interpolate between steps */
  return (1.0f - t) * point + t * next_point;
}

#endif

CCL_NAMESPACE_END
