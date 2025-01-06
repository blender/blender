/* SPDX-FileCopyrightText: 2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Implements Gabor noise based on the paper:
 *
 *   Lagae, Ares, et al. "Procedural noise using sparse Gabor convolution." ACM Transactions on
 *   Graphics (TOG) 28.3 (2009): 1-10.
 *
 * But with the improvements from the paper:
 *
 *   Tavernier, Vincent, et al. "Making gabor noise fast and normalized." Eurographics 2019-40th
 *   Annual Conference of the European Association for Computer Graphics. 2019.
 *
 * And compute the Phase and Intensity of the Gabor based on the paper:
 *
 *   Tricard, Thibault, et al. "Procedural phasor noise." ACM Transactions on Graphics (TOG) 38.4
 *   (2019): 1-13.
 */

#pragma once

#include "kernel/svm/util.h"

#include "util/hash.h"

CCL_NAMESPACE_BEGIN

/* The original Gabor noise paper specifies that the impulses count for each cell should be
 * computed by sampling a Poisson distribution whose mean is the impulse density. However,
 * Tavernier's paper showed that stratified Poisson point sampling is better assuming the weights
 * are sampled using a Bernoulli distribution, as shown in Figure (3). By stratified sampling, they
 * mean a constant number of impulses per cell, so the stratification is the grid itself in that
 * sense, as described in the supplementary material of the paper. */
#define IMPULSES_COUNT 8  // NOLINT

/* Computes a 2D Gabor kernel based on Equation (6) in the original Gabor noise paper. Where the
 * frequency argument is the F_0 parameter and the orientation argument is the w_0 parameter. We
 * assume the Gaussian envelope has a unit magnitude, that is, K = 1. That is because we will
 * eventually normalize the final noise value to the unit range, so the multiplication by the
 * magnitude will be canceled by the normalization. Further, we also assume a unit Gaussian width,
 * that is, a = 1. That is because it does not provide much artistic control. It follows that the
 * Gaussian will be truncated at pi.
 *
 * To avoid the discontinuities caused by the aforementioned truncation, the Gaussian is windowed
 * using a Hann window, that is because contrary to the claim made in the original Gabor paper,
 * truncating the Gaussian produces significant artifacts especially when differentiated for bump
 * mapping. The Hann window is C1 continuous and has limited effect on the shape of the Gaussian,
 * so it felt like an appropriate choice.
 *
 * Finally, instead of computing the Gabor value directly, we instead use the complex phasor
 * formulation described in section 3.1.1 in Tricard's paper. That's done to be able to compute the
 * phase and intensity of the Gabor noise after summation based on equations (8) and (9). The
 * return value of the Gabor kernel function is then a complex number whose real value is the
 * value computed in the original Gabor noise paper, and whose imaginary part is the sine
 * counterpart of the real part, which is the only extra computation in the new formulation.
 *
 * Note that while the original Gabor noise paper uses the cosine part of the phasor, that is, the
 * real part of the phasor, we use the sine part instead, that is, the imaginary part of the
 * phasor, as suggested by Tavernier's paper in "Section 3.3. Instance stationarity and
 * normalization", to ensure a zero mean, which should help with normalization. */
ccl_device float2 compute_2d_gabor_kernel(const float2 position,
                                          const float frequency,
                                          const float orientation)
{
  const float distance_squared = dot(position, position);
  const float hann_window = 0.5f + 0.5f * cosf(M_PI_F * distance_squared);
  const float gaussian_envelop = expf(-M_PI_F * distance_squared);
  const float windowed_gaussian_envelope = gaussian_envelop * hann_window;

  const float angle = 2.0f * M_PI_F * dot(position, polar_to_cartesian(frequency, orientation));
  return polar_to_cartesian(windowed_gaussian_envelope, angle);
}

/**
 * Computes the approximate standard deviation of the zero mean normal distribution representing
 * the amplitude distribution of the noise based on Equation (9) in the original Gabor noise paper.
 * For simplicity, the Hann window is ignored and the orientation is fixed since the variance is
 * orientation invariant. We start integrating the squared Gabor kernel with respect to x:
 *
 * \code{.tex}
 * \int_{-\infty}^{-\infty} (e^{- \pi (x^2 + y^2)} cos(2 \pi f_0 x))^2 dx
 * \endcode
 *
 * Which gives:
 *
 * \code{.tex}
 * \frac{(e^{2 \pi f_0^2}-1) e^{-2 \pi y^2 - 2 pi f_0^2}}{2^\frac{3}{2}}
 * \endcode
 *
 * Then we similarly integrate with respect to y to get:
 *
 * \code{.tex}
 * \frac{1 - e^{-2 \pi f_0^2}}{4}
 * \endcode
 *
 * Secondly, we note that the second moment of the weights distribution is 0.5 since it is a
 * fair Bernoulli distribution. So the final standard deviation expression is square root the
 * integral multiplied by the impulse density multiplied by the second moment.
 *
 * Note however that the integral is almost constant for all frequencies larger than one, and
 * converges to an upper limit as the frequency approaches infinity, so we replace the expression
 * with the following limit:
 *
 * \code{.tex}
 * \lim_{x \to \infty} \frac{1 - e^{-2 \pi f_0^2}}{4}
 * \endcode
 *
 * To get an approximation of 0.25.
 */
ccl_device float compute_2d_gabor_standard_deviation()
{
  const float integral_of_gabor_squared = 0.25f;
  const float second_moment = 0.5f;
  return sqrtf(IMPULSES_COUNT * second_moment * integral_of_gabor_squared);
}

/* Computes the Gabor noise value at the given position for the given cell. This is essentially the
 * sum in Equation (8) in the original Gabor noise paper, where we sum Gabor kernels sampled at a
 * random position with a random weight. The orientation of the kernel is constant for anisotropic
 * noise while it is random for isotropic noise. The original Gabor noise paper mentions that the
 * weights should be uniformly distributed in the [-1, 1] range, however, Tavernier's paper showed
 * that using a Bernoulli distribution yields better results, so that is what we do. */
ccl_device float2 compute_2d_gabor_noise_cell(float2 cell,
                                              const float2 position,
                                              const float frequency,
                                              const float isotropy,
                                              const float base_orientation)

{
  float2 noise = make_float2(0.0f, 0.0f);
  for (int i = 0; i < IMPULSES_COUNT; ++i) {
    /* Compute unique seeds for each of the needed random variables. */
    const float3 seed_for_orientation = make_float3(cell.x, cell.y, i * 3);
    const float3 seed_for_kernel_center = make_float3(cell.x, cell.y, i * 3 + 1);
    const float3 seed_for_weight = make_float3(cell.x, cell.y, i * 3 + 2);

    /* For isotropic noise, add a random orientation amount, while for anisotropic noise, use the
     * base orientation. Linearly interpolate between the two cases using the isotropy factor. Note
     * that the random orientation range spans pi as opposed to two pi, that's because the Gabor
     * kernel is symmetric around pi. */
    const float random_orientation = (hash_float3_to_float(seed_for_orientation) - 0.5f) * M_PI_F;
    const float orientation = base_orientation + random_orientation * isotropy;

    const float2 kernel_center = hash_float3_to_float2(seed_for_kernel_center);
    const float2 position_in_kernel_space = position - kernel_center;

    /* The kernel is windowed beyond the unit distance, so early exit with a zero for points that
     * are further than a unit radius. */
    if (dot(position_in_kernel_space, position_in_kernel_space) >= 1.0f) {
      continue;
    }

    /* We either add or subtract the Gabor kernel based on a Bernoulli distribution of equal
     * probability. */
    const float weight = hash_float3_to_float(seed_for_weight) < 0.5f ? -1.0f : 1.0f;

    noise += weight * compute_2d_gabor_kernel(position_in_kernel_space, frequency, orientation);
  }
  return noise;
}

/* Computes the Gabor noise value by dividing the space into a grid and evaluating the Gabor noise
 * in the space of each cell of the 3x3 cell neighborhood. */
ccl_device float2 compute_2d_gabor_noise(const float2 coordinates,
                                         const float frequency,
                                         const float isotropy,
                                         const float base_orientation)
{
  const float2 cell_position = floor(coordinates);
  const float2 local_position = coordinates - cell_position;

  float2 sum = make_float2(0.0f, 0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const float2 cell_offset = make_float2(i, j);
      const float2 current_cell_position = cell_position + cell_offset;
      const float2 position_in_cell_space = local_position - cell_offset;
      sum += compute_2d_gabor_noise_cell(
          current_cell_position, position_in_cell_space, frequency, isotropy, base_orientation);
    }
  }

  return sum;
}

/* Identical to compute_2d_gabor_kernel, except it is evaluated in 3D space. Notice that Equation
 * (6) in the original Gabor noise paper computes the frequency vector using (cos(w_0), sin(w_0)),
 * which we also do in the 2D variant, however, for 3D, the orientation is already a unit frequency
 * vector, so we just need to scale it by the frequency value. */
ccl_device float2 compute_3d_gabor_kernel(const float3 position,
                                          const float frequency,
                                          const float3 orientation)
{
  const float distance_squared = dot(position, position);
  const float hann_window = 0.5f + 0.5f * cosf(M_PI_F * distance_squared);
  const float gaussian_envelop = expf(-M_PI_F * distance_squared);
  const float windowed_gaussian_envelope = gaussian_envelop * hann_window;

  const float3 frequency_vector = frequency * orientation;
  const float angle = 2.0f * M_PI_F * dot(position, frequency_vector);
  return polar_to_cartesian(windowed_gaussian_envelope, angle);
}

/* Identical to compute_2d_gabor_standard_deviation except we do triple integration in 3D. The only
 * difference is the denominator in the integral expression, which is `2^{5 / 2}` for the 3D case
 * instead of 4 for the 2D case. Similarly, the limit evaluates to `1 / (4 * sqrt(2))`. */
ccl_device float compute_3d_gabor_standard_deviation()
{
  const float integral_of_gabor_squared = 1.0f / (4.0f * M_SQRT2_F);
  const float second_moment = 0.5f;
  return sqrtf(IMPULSES_COUNT * second_moment * integral_of_gabor_squared);
}

/* Computes the orientation of the Gabor kernel such that it is constant for anisotropic
 * noise while it is random for isotropic noise. We randomize in spherical coordinates for a
 * uniform distribution. */
ccl_device float3 compute_3d_orientation(const float3 orientation,
                                         const float isotropy,
                                         const float4 seed)
{
  /* Return the base orientation in case we are completely anisotropic. */
  if (isotropy == 0.0f) {
    return orientation;
  }

  /* Compute the orientation in spherical coordinates. */
  float inclination = acosf(orientation.z);
  float azimuth = (orientation.y < 0.0f ? -1.0f : 1.0f) *
                  acosf(orientation.x / len(make_float2(orientation.x, orientation.y)));

  /* For isotropic noise, add a random orientation amount, while for anisotropic noise, use the
   * base orientation. Linearly interpolate between the two cases using the isotropy factor. Note
   * that the random orientation range is to pi as opposed to two pi, that's because the Gabor
   * kernel is symmetric around pi. */
  const float2 random_angles = hash_float4_to_float2(seed) * M_PI_F;
  inclination += random_angles.x * isotropy;
  azimuth += random_angles.y * isotropy;

  /* Convert back to Cartesian coordinates. */
  return spherical_to_direction(inclination, azimuth);
}

ccl_device float2 compute_3d_gabor_noise_cell(float3 cell,
                                              const float3 position,
                                              const float frequency,
                                              const float isotropy,
                                              const float3 base_orientation)

{
  float2 noise = make_float2(0.0f, 0.0f);
  for (int i = 0; i < IMPULSES_COUNT; ++i) {
    /* Compute unique seeds for each of the needed random variables. */
    const float4 seed_for_orientation = make_float4(cell, i * 3);
    const float4 seed_for_kernel_center = make_float4(cell, i * 3 + 1);
    const float4 seed_for_weight = make_float4(cell, i * 3 + 2);

    const float3 orientation = compute_3d_orientation(
        base_orientation, isotropy, seed_for_orientation);

    const float3 kernel_center = hash_float4_to_float3(seed_for_kernel_center);
    const float3 position_in_kernel_space = position - kernel_center;

    /* The kernel is windowed beyond the unit distance, so early exit with a zero for points that
     * are further than a unit radius. */
    if (dot(position_in_kernel_space, position_in_kernel_space) >= 1.0f) {
      continue;
    }

    /* We either add or subtract the Gabor kernel based on a Bernoulli distribution of equal
     * probability. */
    const float weight = hash_float4_to_float(seed_for_weight) < 0.5f ? -1.0f : 1.0f;

    noise += weight * compute_3d_gabor_kernel(position_in_kernel_space, frequency, orientation);
  }
  return noise;
}

/* Identical to compute_2d_gabor_noise but works in the 3D neighborhood of the noise. */
ccl_device float2 compute_3d_gabor_noise(const float3 coordinates,
                                         const float frequency,
                                         const float isotropy,
                                         const float3 base_orientation)
{
  const float3 cell_position = floor(coordinates);
  const float3 local_position = coordinates - cell_position;

  float2 sum = make_float2(0.0f, 0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        const float3 cell_offset = make_float3(i, j, k);
        const float3 current_cell_position = cell_position + cell_offset;
        const float3 position_in_cell_space = local_position - cell_offset;
        sum += compute_3d_gabor_noise_cell(
            current_cell_position, position_in_cell_space, frequency, isotropy, base_orientation);
      }
    }
  }

  return sum;
}

ccl_device_noinline int svm_node_tex_gabor(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           const uint type,
                                           const uint stack_offsets_1,
                                           const uint stack_offsets_2,
                                           int offset)
{
  uint coordinates_stack_offset;
  uint scale_stack_offset;
  uint frequency_stack_offset;
  uint anisotropy_stack_offset;
  uint orientation_2d_stack_offset;
  uint orientation_3d_stack_offset;

  svm_unpack_node_uchar4(stack_offsets_1,
                         &coordinates_stack_offset,
                         &scale_stack_offset,
                         &frequency_stack_offset,
                         &anisotropy_stack_offset);
  svm_unpack_node_uchar2(
      stack_offsets_2, &orientation_2d_stack_offset, &orientation_3d_stack_offset);

  const float3 coordinates = stack_load_float3(stack, coordinates_stack_offset);

  uint value_stack_offset;
  uint phase_stack_offset;
  uint intensity_stack_offset;

  const uint4 node_1 = read_node(kg, &offset);
  svm_unpack_node_uchar3(
      node_1.x, &value_stack_offset, &phase_stack_offset, &intensity_stack_offset);
  const float scale = stack_load_float_default(stack, scale_stack_offset, node_1.y);
  float frequency = stack_load_float_default(stack, frequency_stack_offset, node_1.z);
  const float anisotropy = stack_load_float_default(stack, anisotropy_stack_offset, node_1.w);

  const uint4 node_2 = read_node(kg, &offset);
  const float orientation_2d = stack_load_float_default(
      stack, orientation_2d_stack_offset, node_2.x);
  const float3 orientation_3d = stack_load_float3(stack, orientation_3d_stack_offset);

  const float3 scaled_coordinates = coordinates * scale;
  const float isotropy = 1.0f - clamp(anisotropy, 0.0f, 1.0f);
  frequency = max(0.001f, frequency);

  float2 phasor = make_float2(0.0f, 0.0f);
  float standard_deviation = 1.0f;
  switch ((NodeGaborType)type) {
    case NODE_GABOR_TYPE_2D: {
      phasor = compute_2d_gabor_noise(make_float2(scaled_coordinates.x, scaled_coordinates.y),
                                      frequency,
                                      isotropy,
                                      orientation_2d);
      standard_deviation = compute_2d_gabor_standard_deviation();
      break;
    }
    case NODE_GABOR_TYPE_3D: {
      const float3 orientation = normalize(orientation_3d);
      phasor = compute_3d_gabor_noise(scaled_coordinates, frequency, isotropy, orientation);
      standard_deviation = compute_3d_gabor_standard_deviation();
      break;
    }
  }

  /* Normalize the noise by dividing by six times the standard deviation, which was determined
   * empirically. */
  const float normalization_factor = 6.0f * standard_deviation;

  /* As discussed in compute_2d_gabor_kernel, we use the imaginary part of the phasor as the Gabor
   * value. But remap to [0, 1] from [-1, 1]. */
  if (stack_valid(value_stack_offset)) {
    stack_store_float(stack, value_stack_offset, (phasor.y / normalization_factor) * 0.5f + 0.5f);
  }

  /* Compute the phase based on equation (9) in Tricard's paper. But remap the phase into the
   * [0, 1] range. */
  if (stack_valid(phase_stack_offset)) {
    const float phase = (atan2f(phasor.y, phasor.x) + M_PI_F) / (2.0f * M_PI_F);
    stack_store_float(stack, phase_stack_offset, phase);
  }

  /* Compute the intensity based on equation (8) in Tricard's paper. */
  if (stack_valid(intensity_stack_offset)) {
    stack_store_float(stack, intensity_stack_offset, len(phasor) / normalization_factor);
  }

  return offset;
}

CCL_NAMESPACE_END
