/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float2 input_size = float2(texture_size(input_tx));

  float2 coordinates = float2(texel) + float2(0.5f);

  float current_sin = 0.0f;
  float current_cos = 1.0f;
  float current_scale = 1.0f;
  float2 current_translation = float2(0.0f);

  /* For each iteration, accumulate the input at the transformed coordinates, then increment the
   * transformations for the next iteration. */
  float4 accumulated_color = float4(0.0f);
  for (int i = 0; i < iterations; i++) {
    /* Transform the coordinates by first offsetting the origin, scaling, translating, rotating,
     * then finally restoring the origin. Notice that we do the inverse of each of the transforms,
     * since we are transforming the coordinates, not the image. */
    float2 transformed_coordinates = coordinates;
    transformed_coordinates -= origin;
    transformed_coordinates /= current_scale;
    transformed_coordinates -= current_translation;
    transformed_coordinates *= float2x2(current_cos, current_sin, -current_sin, current_cos);
    transformed_coordinates += origin;

    accumulated_color += texture(input_tx, transformed_coordinates / input_size);

    current_scale += delta_scale;
    current_translation += delta_translation;

    /* Those are the sine and cosine addition identities. Used to avoid computing sine and cosine
     * at each iteration. */
    float new_sin = current_sin * delta_rotation_cos + current_cos * delta_rotation_sin;
    current_cos = current_cos * delta_rotation_cos - current_sin * delta_rotation_sin;
    current_sin = new_sin;
  }

  imageStore(output_img, texel, accumulated_color / iterations);
}
