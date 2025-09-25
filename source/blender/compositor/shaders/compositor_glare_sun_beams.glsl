/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"

/* Returns an index for a position along the path between the texel and the source.
 *
 * When jitter is enabled, the position index is computed using the Global Shift
 * sampling technique: a hash-based global shift is applied to the indices which is then
 * factored to cover the range [0, steps].
 * Without jitter, the integer index `i` is returned
 * directly.
 */
float get_sample_position(int i, float random_offset)
{
#if defined(JITTER)
  return safe_divide(i + random_offset, 1.0f - jitter_factor);
#else
  return i;
#endif
}

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float2 input_size = float2(texture_size(input_tx));

  /* The number of steps is the distance in pixels from the source to the current texel. With at
   * least a single step and at most the user specified maximum ray length, which is proportional
   * to the diagonal pixel count. */
  float unbounded_steps = max(1.0f, distance(float2(texel), source * input_size));
  int steps = min(max_steps, int(unbounded_steps));

  /* We integrate from the current pixel to the source pixel, so compute the start coordinates and
   * step vector in the direction to source. Notice that the step vector is still computed from the
   * unbounded steps, such that the total integration length becomes limited by the bounded steps,
   * and thus by the maximum ray length. */
  float2 coordinates = (float2(texel) + float2(0.5f)) / input_size;
  float2 vector_to_source = source - coordinates;
  float2 step_vector = vector_to_source / unbounded_steps;

  float accumulated_weight = 0.0f;
  float4 accumulated_color = float4(0.0f);

#if defined(JITTER)
  int number_of_steps = int((1.0f - jitter_factor) * steps);
#else
  int number_of_steps = steps;
#endif
  float random_offset = hash_uint2_to_float(uint(texel.x), uint(texel.y));

  for (int i = 0; i <= number_of_steps; i++) {
    float position_index = get_sample_position(i, random_offset);
    float2 position = coordinates + position_index * step_vector;

    /* We are already past the image boundaries, and any future steps are also past the image
     * boundaries, so break. */
    if (any(lessThan(position, float2(0.0f))) || any(greaterThan(position, float2(1.0f)))) {
      break;
    }

    float4 sample_color = texture(input_tx, position);

    /* Attenuate the contributions of pixels that are further away from the source using a
     * quadratic falloff. */
    float weight = square(1.0f - position_index / float(steps));

    accumulated_weight += weight;
    accumulated_color += sample_color * weight;
  }

  if (accumulated_weight != 0.0f) {
    accumulated_color /= accumulated_weight;
  }
  else {
    accumulated_color = texture(input_tx, coordinates);
  }
  imageStore(output_img, texel, accumulated_color);
}
