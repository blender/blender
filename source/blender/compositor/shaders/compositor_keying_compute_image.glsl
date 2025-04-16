/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

int3 compute_saturation_indices(float3 v)
{
  int index_of_max = ((v.x > v.y) ? ((v.x > v.z) ? 0 : 2) : ((v.y > v.z) ? 1 : 2));
  int2 other_indices = (int2(index_of_max) + int2(1, 2)) % 3;
  int min_index = min(other_indices.x, other_indices.y);
  int max_index = max(other_indices.x, other_indices.y);
  return int3(index_of_max, max_index, min_index);
}

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 key = texture_load(key_tx, texel);
  float4 color = texture_load(input_tx, texel);
  float matte = texture_load(matte_tx, texel).x;

  /* Alpha multiply the matte to the image. */
  color *= matte;

  /* Color despill. */
  int3 indices = compute_saturation_indices(key.rgb);
  float weighted_average = mix(color[indices.y], color[indices.z], despill_balance);
  color[indices.x] -= max(0.0f, (color[indices.x] - weighted_average) * despill_factor);

  imageStore(output_img, texel, color);
}
