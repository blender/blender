/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

ivec3 compute_saturation_indices(vec3 v)
{
  int index_of_max = ((v.x > v.y) ? ((v.x > v.z) ? 0 : 2) : ((v.y > v.z) ? 1 : 2));
  ivec2 other_indices = (ivec2(index_of_max) + ivec2(1, 2)) % 3;
  int min_index = min(other_indices.x, other_indices.y);
  int max_index = max(other_indices.x, other_indices.y);
  return ivec3(index_of_max, max_index, min_index);
}

float compute_saturation(vec4 color, ivec3 indices)
{
  float weighted_average = mix(color[indices.y], color[indices.z], key_balance);
  return (color[indices.x] - weighted_average) * abs(1.0 - weighted_average);
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 input_color = texture_load(input_tx, texel);

  /* We assume that the keying screen will not be overexposed in the image, so if the input
   * brightness is high, we assume the pixel is opaque. */
  if (min_v3(input_color) > 1.0f) {
    imageStore(output_img, texel, vec4(1.0));
    return;
  }

  vec4 key_color = texture_load(key_tx, texel);
  ivec3 key_saturation_indices = compute_saturation_indices(key_color.rgb);
  float input_saturation = compute_saturation(input_color, key_saturation_indices);
  float key_saturation = compute_saturation(key_color, key_saturation_indices);

  float matte;
  if (input_saturation < 0) {
    /* Means main channel of pixel is different from screen, assume this is completely a
     * foreground. */
    matte = 1.0f;
  }
  else if (input_saturation >= key_saturation) {
    /* Matched main channels and higher saturation on pixel is treated as completely background. */
    matte = 0.0f;
  }
  else {
    matte = 1.0f - clamp(input_saturation / key_saturation, 0.0, 1.0);
  }

  imageStore(output_img, texel, vec4(matte));
}
