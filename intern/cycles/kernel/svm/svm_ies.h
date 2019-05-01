/*
 * Copyright 2011-2013 Blender Foundation
 *
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

/* IES Light */

ccl_device_inline float interpolate_ies_vertical(
    KernelGlobals *kg, int ofs, int v, int v_num, float v_frac, int h)
{
  /* Since lookups are performed in spherical coordinates, clamping the coordinates at the low end
   * of v (corresponding to the north pole) would result in artifacts. The proper way of dealing
   * with this would be to lookup the corresponding value on the other side of the pole, but since
   * the horizontal coordinates might be nonuniform, this would require yet another interpolation.
   * Therefore, the assumtion is made that the light is going to be symmetrical, which means that
   * we can just take the corresponding value at the current horizontal coordinate. */

#define IES_LOOKUP(v) kernel_tex_fetch(__ies, ofs + h * v_num + (v))
  /* If v is zero, assume symmetry and read at v=1 instead of v=-1. */
  float a = IES_LOOKUP((v == 0) ? 1 : v - 1);
  float b = IES_LOOKUP(v);
  float c = IES_LOOKUP(v + 1);
  float d = IES_LOOKUP(min(v + 2, v_num - 1));
#undef IES_LOOKUP

  return cubic_interp(a, b, c, d, v_frac);
}

ccl_device_inline float kernel_ies_interp(KernelGlobals *kg,
                                          int slot,
                                          float h_angle,
                                          float v_angle)
{
  /* Find offset of the IES data in the table. */
  int ofs = __float_as_int(kernel_tex_fetch(__ies, slot));
  if (ofs == -1) {
    return 100.0f;
  }

  int h_num = __float_as_int(kernel_tex_fetch(__ies, ofs++));
  int v_num = __float_as_int(kernel_tex_fetch(__ies, ofs++));

#define IES_LOOKUP_ANGLE_H(h) kernel_tex_fetch(__ies, ofs + (h))
#define IES_LOOKUP_ANGLE_V(v) kernel_tex_fetch(__ies, ofs + h_num + (v))

  /* Check whether the angle is within the bounds of the IES texture. */
  if (v_angle >= IES_LOOKUP_ANGLE_V(v_num - 1)) {
    return 0.0f;
  }
  kernel_assert(v_angle >= IES_LOOKUP_ANGLE_V(0));
  kernel_assert(h_angle >= IES_LOOKUP_ANGLE_H(0));
  kernel_assert(h_angle <= IES_LOOKUP_ANGLE_H(h_num - 1));

  /* Lookup the angles to find the table position. */
  int h_i, v_i;
  /* TODO(lukas): Consider using bisection.
   * Probably not worth it for the vast majority of IES files. */
  for (h_i = 0; IES_LOOKUP_ANGLE_H(h_i + 1) < h_angle; h_i++)
    ;
  for (v_i = 0; IES_LOOKUP_ANGLE_V(v_i + 1) < v_angle; v_i++)
    ;

  float h_frac = inverse_lerp(IES_LOOKUP_ANGLE_H(h_i), IES_LOOKUP_ANGLE_H(h_i + 1), h_angle);
  float v_frac = inverse_lerp(IES_LOOKUP_ANGLE_V(v_i), IES_LOOKUP_ANGLE_V(v_i + 1), v_angle);

#undef IES_LOOKUP_ANGLE_H
#undef IES_LOOKUP_ANGLE_V

  /* Skip forward to the actual intensity data. */
  ofs += h_num + v_num;

  /* Perform cubic interpolation along the horizontal coordinate to get the intensity value.
   * If h_i is zero, just wrap around since the horizontal angles always go over the full circle.
   * However, the last entry (360°) equals the first one, so we need to wrap around to the one
   * before that. */
  float a = interpolate_ies_vertical(
      kg, ofs, v_i, v_num, v_frac, (h_i == 0) ? h_num - 2 : h_i - 1);
  float b = interpolate_ies_vertical(kg, ofs, v_i, v_num, v_frac, h_i);
  float c = interpolate_ies_vertical(kg, ofs, v_i, v_num, v_frac, h_i + 1);
  /* Same logic here, wrap around to the second element if necessary. */
  float d = interpolate_ies_vertical(
      kg, ofs, v_i, v_num, v_frac, (h_i + 2 == h_num) ? 1 : h_i + 2);

  /* Cubic interpolation can result in negative values, so get rid of them. */
  return max(cubic_interp(a, b, c, d, h_frac), 0.0f);
}

ccl_device void svm_node_ies(
    KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
  uint vector_offset, strength_offset, fac_offset, dummy, slot = node.z;
  decode_node_uchar4(node.y, &strength_offset, &vector_offset, &fac_offset, &dummy);

  float3 vector = stack_load_float3(stack, vector_offset);
  float strength = stack_load_float_default(stack, strength_offset, node.w);

  vector = normalize(vector);
  float v_angle = safe_acosf(-vector.z);
  float h_angle = atan2f(vector.x, vector.y) + M_PI_F;

  float fac = strength * kernel_ies_interp(kg, slot, h_angle, v_angle);

  if (stack_valid(fac_offset)) {
    stack_store_float(stack, fac_offset, fac);
  }
}

CCL_NAMESPACE_END
