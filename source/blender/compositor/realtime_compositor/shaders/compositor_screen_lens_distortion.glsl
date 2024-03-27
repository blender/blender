/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* A model that approximates lens distortion parameterized by a distortion parameter and dependent
 * on the squared distance to the center of the image. The distorted pixel is then computed as the
 * scalar multiplication of the pixel coordinates with the value returned by this model. See the
 * compute_distorted_uv function for more details. */
float compute_distortion_scale(float distortion, float distance_squared)
{
  return 1.0 / (1.0 + sqrt(max(0.0, 1.0 - distortion * distance_squared)));
}

/* A vectorized version of compute_distortion_scale that is applied on the chromatic distortion
 * parameters passed to the shader. */
vec3 compute_chromatic_distortion_scale(float distance_squared)
{
  return 1.0 / (1.0 + sqrt(max(vec3(0.0), 1.0 - chromatic_distortion * distance_squared)));
}

/* Compute the image coordinates after distortion by the given distortion scale computed by the
 * compute_distortion_scale function. Note that the function expects centered normalized UV
 * coordinates but outputs non-centered image coordinates. */
vec2 compute_distorted_uv(vec2 uv, float uv_scale)
{
  return (uv * uv_scale + 0.5) * vec2(texture_size(input_tx));
}

/* Compute the number of integration steps that should be used to approximate the distorted pixel
 * using a heuristic, see the compute_number_of_steps function for more details. The numbers of
 * steps is proportional to the number of pixels spanned by the distortion amount. For jitter
 * distortion, the square root of the distortion amount plus 1 is used with a minimum of 2 steps.
 * For non-jitter distortion, the distortion amount plus 1 is used as the number of steps */
int compute_number_of_integration_steps_heuristic(float distortion)
{
#if defined(JITTER)
  return distortion < 4.0 ? 2 : int(sqrt(distortion + 1.0));
#else
  return int(distortion + 1.0);
#endif
}

/* Compute the number of integration steps that should be used to compute each channel of the
 * distorted pixel. Each of the channels are distorted by their respective chromatic distortion
 * amount, then the amount of distortion between each two consecutive channels is computed, this
 * amount is then used to heuristically infer the number of needed integration steps, see the
 * integrate_distortion function for more information. */
ivec3 compute_number_of_integration_steps(vec2 uv, float distance_squared)
{
  /* Distort each channel by its respective chromatic distortion amount. */
  vec3 distortion_scale = compute_chromatic_distortion_scale(distance_squared);
  vec2 distorted_uv_red = compute_distorted_uv(uv, distortion_scale.r);
  vec2 distorted_uv_green = compute_distorted_uv(uv, distortion_scale.g);
  vec2 distorted_uv_blue = compute_distorted_uv(uv, distortion_scale.b);

  /* Infer the number of needed integration steps to compute the distorted red channel starting
   * from the green channel. */
  float distortion_red = distance(distorted_uv_red, distorted_uv_green);
  int steps_red = compute_number_of_integration_steps_heuristic(distortion_red);

  /* Infer the number of needed integration steps to compute the distorted blue channel starting
   * from the green channel. */
  float distortion_blue = distance(distorted_uv_green, distorted_uv_blue);
  int steps_blue = compute_number_of_integration_steps_heuristic(distortion_blue);

  /* The number of integration steps used to compute the green channel is the sum of both the red
   * and the blue channel steps because it is computed once with each of them. */
  return ivec3(steps_red, steps_red + steps_blue, steps_blue);
}

/* Returns a random jitter amount, which is essentially a random value in the [0, 1] range. If
 * jitter is not enabled, return a constant 0.5 value instead. */
float get_jitter(int seed)
{
#if defined(JITTER)
  return hash_uint3_to_float(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, seed);
#else
  return 0.5;
#endif
}

/* Each color channel may have a different distortion with the guarantee that the red will have the
 * lowest distortion while the blue will have the highest one. If each channel is distorted
 * independently, the image will look disintegrated, with each channel seemingly merely shifted.
 * Consequently, the distorted pixels needs to be computed by integrating along the path of change
 * of distortion starting from one channel to another. For instance, to compute the distorted red
 * from the distorted green, we accumulate the color of the distorted pixel starting from the
 * distortion of the red, taking small steps until we reach the distortion of the green. The pixel
 * color is weighted such that it is maximum at the start distortion and zero at the end distortion
 * in an arithmetic progression. The integration steps can be augmented with random values to
 * simulate lens jitter. Finally, it should be noted that this function integrates both the start
 * and end channels in reverse directions for more efficient computation. */
vec3 integrate_distortion(int start, int end, float distance_squared, vec2 uv, int steps)
{
  vec3 accumulated_color = vec3(0.0);
  float distortion_amount = chromatic_distortion[end] - chromatic_distortion[start];
  for (int i = 0; i < steps; i++) {
    /* The increment will be in the [0, 1) range across iterations. */
    float increment = (i + get_jitter(i)) / steps;
    float distortion = chromatic_distortion[start] + increment * distortion_amount;
    float distortion_scale = compute_distortion_scale(distortion, distance_squared);

    /* Sample the color at the distorted coordinates and accumulate it weighted by the increment
     * value for both the start and end channels. */
    vec2 distorted_uv = compute_distorted_uv(uv, distortion_scale);
    vec4 color = texture(input_tx, distorted_uv / vec2(texture_size(input_tx)));
    accumulated_color[start] += (1.0 - increment) * color[start];
    accumulated_color[end] += increment * color[end];
  }
  return accumulated_color;
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Compute the UV image coordinates in the range [-1, 1] as well as the squared distance to the
   * center of the image, which is at (0, 0) in the UV coordinates. */
  vec2 center = vec2(texture_size(input_tx)) / 2.0;
  vec2 uv = scale * (vec2(texel) + vec2(0.5) - center) / center;
  float distance_squared = dot(uv, uv);

  /* If any of the color channels will get distorted outside of the screen beyond what is possible,
   * write a zero transparent color and return. */
  if (any(greaterThan(chromatic_distortion * distance_squared, vec3(1.0)))) {
    imageStore(output_img, texel, vec4(0.0));
    return;
  }

  /* Compute the number of integration steps that should be used to compute each channel of the
   * distorted pixel. */
  ivec3 number_of_steps = compute_number_of_integration_steps(uv, distance_squared);

  /* Integrate the distortion of the red and green, then the green and blue channels. That means
   * the green will be integrated twice, but this is accounted for in the number of steps which the
   * color will later be divided by. See the compute_number_of_integration_steps function for more
   * details. */
  vec3 color = vec3(0.0);
  color += integrate_distortion(0, 1, distance_squared, uv, number_of_steps.r);
  color += integrate_distortion(1, 2, distance_squared, uv, number_of_steps.b);

  /* The integration above performed weighted accumulation, and thus the color needs to be divided
   * by the sum of the weights. Assuming no jitter, the weights are generated as an arithmetic
   * progression starting from (0.5 / n) to ((n - 0.5) / n) for n terms. The sum of an arithmetic
   * progression can be computed as (n * (start + end) / 2), which when subsisting the start and
   * end reduces to (n / 2). So the color should be multiplied by 2 / n. The jitter sequence
   * approximately sums to the same value because it is a uniform random value whose mean value is
   * 0.5, so the expression doesn't change regardless of jitter. */
  color *= 2.0 / vec3(number_of_steps);

  imageStore(output_img, texel, vec4(color, 1.0));
}
