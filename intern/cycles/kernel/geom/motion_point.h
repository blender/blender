/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

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
motion_point_for_step(KernelGlobals kg, int offset, int numkeys, int numsteps, int step, int prim)
{
  if (step == numsteps) {
    /* center step: regular key location */
    return kernel_tex_fetch(__points, prim);
  }
  else {
    /* center step is not stored in this array */
    if (step > numsteps)
      step--;

    offset += step * numkeys;

    return kernel_tex_fetch(__attributes_float4, offset + prim);
  }
}

/* return 2 point key locations */
ccl_device_inline float4 motion_point(KernelGlobals kg, int object, int prim, float time)
{
  /* get motion info */
  int numsteps, numkeys;
  object_motion_info(kg, object, &numsteps, NULL, &numkeys);

  /* figure out which steps we need to fetch and their interpolation factor */
  int maxstep = numsteps * 2;
  int step = min((int)(time * maxstep), maxstep - 1);
  float t = time * maxstep - step;

  /* find attribute */
  int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_POSITION);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* fetch key coordinates */
  float4 point = motion_point_for_step(kg, offset, numkeys, numsteps, step, prim);
  float4 next_point = motion_point_for_step(kg, offset, numkeys, numsteps, step + 1, prim);

  /* interpolate between steps */
  return (1.0f - t) * point + t * next_point;
}

#endif

CCL_NAMESPACE_END
