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

ccl_device_inline int find_attribute_curve_motion(KernelGlobals *kg,
                                                  int object,
                                                  uint id,
                                                  AttributeElement *elem)
{
  /* todo: find a better (faster) solution for this, maybe store offset per object.
   *
   * NOTE: currently it's not a bottleneck because in test scenes the loop below runs
   * zero iterations and rendering is really slow with motion curves. For until other
   * areas are speed up it's probably not so crucial to optimize this out.
   */
  uint attr_offset = object_attribute_map_offset(kg, object) + ATTR_PRIM_GEOMETRY;
  uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);

  while (attr_map.x != id) {
    attr_offset += ATTR_PRIM_TYPES;
    attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
  }

  *elem = (AttributeElement)attr_map.y;

  /* return result */
  return (attr_map.y == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : (int)attr_map.z;
}

ccl_device_inline void motion_curve_keys_for_step(KernelGlobals *kg,
                                                  int offset,
                                                  int numkeys,
                                                  int numsteps,
                                                  int step,
                                                  int k0,
                                                  int k1,
                                                  float4 keys[2])
{
  if (step == numsteps) {
    /* center step: regular key location */
    keys[0] = kernel_tex_fetch(__curve_keys, k0);
    keys[1] = kernel_tex_fetch(__curve_keys, k1);
  }
  else {
    /* center step is not stored in this array */
    if (step > numsteps)
      step--;

    offset += step * numkeys;

    keys[0] = kernel_tex_fetch(__attributes_float3, offset + k0);
    keys[1] = kernel_tex_fetch(__attributes_float3, offset + k1);
  }
}

/* return 2 curve key locations */
ccl_device_inline void motion_curve_keys(
    KernelGlobals *kg, int object, int prim, float time, int k0, int k1, float4 keys[2])
{
  /* get motion info */
  int numsteps, numkeys;
  object_motion_info(kg, object, &numsteps, NULL, &numkeys);

  /* figure out which steps we need to fetch and their interpolation factor */
  int maxstep = numsteps * 2;
  int step = min((int)(time * maxstep), maxstep - 1);
  float t = time * maxstep - step;

  /* find attribute */
  AttributeElement elem;
  int offset = find_attribute_curve_motion(kg, object, ATTR_STD_MOTION_VERTEX_POSITION, &elem);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* fetch key coordinates */
  float4 next_keys[2];

  motion_curve_keys_for_step(kg, offset, numkeys, numsteps, step, k0, k1, keys);
  motion_curve_keys_for_step(kg, offset, numkeys, numsteps, step + 1, k0, k1, next_keys);

  /* interpolate between steps */
  keys[0] = (1.0f - t) * keys[0] + t * next_keys[0];
  keys[1] = (1.0f - t) * keys[1] + t * next_keys[1];
}

ccl_device_inline void motion_curve_keys_for_step(KernelGlobals *kg,
                                                  int offset,
                                                  int numkeys,
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
    keys[0] = kernel_tex_fetch(__curve_keys, k0);
    keys[1] = kernel_tex_fetch(__curve_keys, k1);
    keys[2] = kernel_tex_fetch(__curve_keys, k2);
    keys[3] = kernel_tex_fetch(__curve_keys, k3);
  }
  else {
    /* center step is not stored in this array */
    if (step > numsteps)
      step--;

    offset += step * numkeys;

    keys[0] = kernel_tex_fetch(__attributes_float3, offset + k0);
    keys[1] = kernel_tex_fetch(__attributes_float3, offset + k1);
    keys[2] = kernel_tex_fetch(__attributes_float3, offset + k2);
    keys[3] = kernel_tex_fetch(__attributes_float3, offset + k3);
  }
}

/* return 2 curve key locations */
ccl_device_inline void motion_curve_keys(KernelGlobals *kg,
                                         int object,
                                         int prim,
                                         float time,
                                         int k0,
                                         int k1,
                                         int k2,
                                         int k3,
                                         float4 keys[4])
{
  /* get motion info */
  int numsteps, numkeys;
  object_motion_info(kg, object, &numsteps, NULL, &numkeys);

  /* figure out which steps we need to fetch and their interpolation factor */
  int maxstep = numsteps * 2;
  int step = min((int)(time * maxstep), maxstep - 1);
  float t = time * maxstep - step;

  /* find attribute */
  AttributeElement elem;
  int offset = find_attribute_curve_motion(kg, object, ATTR_STD_MOTION_VERTEX_POSITION, &elem);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* fetch key coordinates */
  float4 next_keys[4];

  motion_curve_keys_for_step(kg, offset, numkeys, numsteps, step, k0, k1, k2, k3, keys);
  motion_curve_keys_for_step(kg, offset, numkeys, numsteps, step + 1, k0, k1, k2, k3, next_keys);

  /* interpolate between steps */
  keys[0] = (1.0f - t) * keys[0] + t * next_keys[0];
  keys[1] = (1.0f - t) * keys[1] + t * next_keys[1];
  keys[2] = (1.0f - t) * keys[2] + t * next_keys[2];
  keys[3] = (1.0f - t) * keys[3] + t * next_keys[3];
}

#endif

CCL_NAMESPACE_END
