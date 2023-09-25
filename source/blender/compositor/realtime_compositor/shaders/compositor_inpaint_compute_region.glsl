/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Fill the inpainting region by sampling the color of the nearest boundary pixel if it is not
 * further than the user supplied distance. Additionally, apply a lateral blur in the tangential
 * path to the inpainting boundary to smooth out the inpainted region. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_jump_flooding_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 color = texture_load(input_tx, texel);

  /* An opaque pixel, no inpainting needed. */
  if (color.a == 1.0) {
    imageStore(output_img, texel, color);
    return;
  }

  vec4 flooding_value = texture_load(flooded_boundary_tx, texel);
  float distance_to_boundary = extract_jump_flooding_distance_to_closest_seed(flooding_value);

  /* Further than the user supplied distance, write a transparent color. */
  if (distance_to_boundary > distance) {
    imageStore(output_img, texel, vec4(0.0));
    return;
  }

  /* We set the blur radius to be proportional to the distance to the boundary. */
  int blur_radius = int(ceil(distance_to_boundary));

  /* Laterally blur by accumulate the boundary pixels nearest to the pixels along the tangential
   * path in both directions starting from the current pixel, noting that the weights texture only
   * stores the weights for the left half, but since the Gaussian is symmetric, the same weight is
   * used for the right half and we add both of their contributions. */
  vec2 left_texel = vec2(texel);
  vec2 right_texel = vec2(texel);
  float accumulated_weight = 0.0;
  vec4 accumulated_color = vec4(0.0);
  for (int i = 0; i < blur_radius; i++) {
    float weight = texture(gaussian_weights_tx, float(i / (blur_radius - 1))).x;

    {
      vec4 flooding_value = texture_load(flooded_boundary_tx, ivec2(left_texel));
      ivec2 boundary_texel = extract_jump_flooding_closest_seed_texel(flooding_value);
      accumulated_color += texture_load(input_tx, boundary_texel) * weight;
      accumulated_weight += weight;

      /* Move the left texel one pixel in the clockwise tangent to the boundary. */
      left_texel += normalize((left_texel - vec2(boundary_texel)).yx * vec2(-1.0, 1.0));
    }

    /* When i is zero, we are accumulating the center pixel, which was already accumulated as the
     * left texel above, so no need to accumulate it again. */
    if (i != 0) {
      vec4 flooding_value = texture_load(flooded_boundary_tx, ivec2(right_texel));
      ivec2 boundary_texel = extract_jump_flooding_closest_seed_texel(flooding_value);
      accumulated_color += texture_load(input_tx, boundary_texel) * weight;
      accumulated_weight += weight;

      /* Move the left texel one pixel in the anti-clockwise tangent to the boundary. */
      right_texel += normalize((right_texel - vec2(boundary_texel)).yx * vec2(1.0, -1.0));
    }
  }

  imageStore(output_img, texel, accumulated_color / accumulated_weight);
}
