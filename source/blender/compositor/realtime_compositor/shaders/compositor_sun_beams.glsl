/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec2 input_size = vec2(texture_size(input_tx));

  /* The number of steps is the distance in pixels from the source to the current texel. With at
   * least a single step and at most the user specified maximum ray length, which is proportional
   * to the diagonal pixel count. */
  float unbounded_steps = max(1.0, distance(vec2(texel), source * input_size));
  int steps = min(max_steps, int(unbounded_steps));

  /* We integrate from the current pixel to the source pixel, so compute the start coordinates and
   * step vector in the direction to source. Notice that the step vector is still computed from the
   * unbounded steps, such that the total integration length becomes limited by the bounded steps,
   * and thus by the maximum ray length. */
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / input_size;
  vec2 vector_to_source = source - coordinates;
  vec2 step_vector = vector_to_source / unbounded_steps;

  float accumulated_weight = 0.0;
  vec4 accumulated_color = vec4(0.0);
  for (int i = 0; i <= steps; i++) {
    vec2 position = coordinates + i * step_vector;

    /* We are already past the image boundaries, and any future steps are also past the image
     * boundaries, so break. */
    if (any(lessThan(position, vec2(0.0))) || any(greaterThan(position, vec2(1.0)))) {
      break;
    }

    vec4 sample_color = texture(input_tx, position);

    /* Attenuate the contributions of pixels that are further away from the source using a
     * quadratic falloff. Also weight by the alpha to give more significance to opaque pixels. */
    float weight = (square(1.0 - i / float(steps))) * sample_color.a;

    accumulated_weight += weight;
    accumulated_color += sample_color * weight;
  }

  accumulated_color /= accumulated_weight != 0.0 ? accumulated_weight : 1.0;
  imageStore(output_img, texel, accumulated_color);
}
