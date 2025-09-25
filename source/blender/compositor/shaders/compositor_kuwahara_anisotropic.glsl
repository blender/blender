/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_base_lib.glsl"

/* An implementation of the Anisotropic Kuwahara filter described in the paper:
 *
 *   Kyprianidis, Jan Eric, Henry Kang, and Jurgen Dollner. "Image and video abstraction by
 *   anisotropic Kuwahara filtering." 2009.
 *
 * But with the polynomial weighting functions described in the paper:
 *
 *   Kyprianidis, Jan Eric, et al. "Anisotropic Kuwahara Filtering with Polynomial Weighting
 *   Functions." 2010.
 *
 * And the sector weight function described in the paper:
 *
 *  Kyprianidis, Jan Eric. "Image and video abstraction by multi-scale anisotropic Kuwahara
 *  filtering." 2011.
 */

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* The structure tensor is encoded in a vec4 using a column major storage order, as can be seen
   * in the compositor_kuwahara_anisotropic_compute_structure_tensor.glsl shader. */
  float4 encoded_structure_tensor = texture_load(structure_tensor_tx, texel);
  float dxdx = encoded_structure_tensor.x;
  float dxdy = encoded_structure_tensor.y;
  float dydy = encoded_structure_tensor.w;

  /* Compute the first and second eigenvalues of the structure tensor using the equations in
   * section "3.1 Orientation and Anisotropy Estimation" of the paper. */
  float eigenvalue_first_term = (dxdx + dydy) / 2.0f;
  float eigenvalue_square_root_term = sqrt(square(dxdx - dydy) + 4.0f * square(dxdy)) / 2.0f;
  float first_eigenvalue = eigenvalue_first_term + eigenvalue_square_root_term;
  float second_eigenvalue = eigenvalue_first_term - eigenvalue_square_root_term;

  /* Compute the normalized eigenvector of the structure tensor oriented in direction of the
   * minimum rate of change using the equations in section "3.1 Orientation and Anisotropy
   * Estimation" of the paper. */
  float2 eigenvector = float2(first_eigenvalue - dxdx, -dxdy);
  float eigenvector_length = length(eigenvector);
  float2 unit_eigenvector = eigenvector_length != 0.0f ? eigenvector / eigenvector_length :
                                                         float2(1.0f);

  /* Compute the amount of anisotropy using equations in section "3.1 Orientation and Anisotropy
   * Estimation" of the paper. The anisotropy ranges from 0 to 1, where 0 corresponds to isotropic
   * and 1 corresponds to entirely anisotropic regions. */
  float eigenvalue_sum = first_eigenvalue + second_eigenvalue;
  float eigenvalue_difference = first_eigenvalue - second_eigenvalue;
  float anisotropy = eigenvalue_sum > 0.0f ? eigenvalue_difference / eigenvalue_sum : 0.0f;

#if defined(VARIABLE_SIZE)
  float radius = max(0.0f, texture_load(size_tx, texel).x);
#elif defined(CONSTANT_SIZE)
  float radius = max(0.0f, size);
#endif
  if (radius == 0) {
    imageStore(output_img, texel, texture_load(input_tx, texel));
    return;
  }

  /* Compute the width and height of an ellipse that is more width-elongated for high anisotropy
   * and more circular for low anisotropy, controlled using the eccentricity factor. Since the
   * anisotropy is in the [0, 1] range, the width factor tends to 1 as the eccentricity tends to
   * infinity and tends to infinity when the eccentricity tends to zero. This is based on the
   * equations in section "3.2. Anisotropic Kuwahara Filtering" of the paper. */
  float ellipse_width_factor = (eccentricity + anisotropy) / eccentricity;
  float ellipse_width = ellipse_width_factor * radius;
  float ellipse_height = radius / ellipse_width_factor;

  /* Compute the cosine and sine of the angle that the eigenvector makes with the x axis. Since the
   * eigenvector is normalized, its x and y components are the cosine and sine of the angle it
   * makes with the x axis. */
  float cosine = unit_eigenvector.x;
  float sine = unit_eigenvector.y;

  /* Compute an inverse transformation matrix that represents an ellipse of the given width and
   * height and makes and an angle with the x axis of the given cosine and sine. This is an inverse
   * matrix, so it transforms the ellipse into a disk of unit radius. */
  float2x2 inverse_ellipse_matrix = float2x2(cosine / ellipse_width,
                                             -sine / ellipse_height,
                                             sine / ellipse_width,
                                             cosine / ellipse_height);

  /* Compute the bounding box of a zero centered ellipse whose major axis is aligned with the
   * eigenvector and has the given width and height. This is based on the equations described in:
   *
   *   https://iquilezles.org/articles/ellipses/
   *
   * Notice that we only compute the upper bound, the lower bound is just negative that since the
   * ellipse is zero centered. Also notice that we take the ceiling of the bounding box, just to
   * ensure the filter window is at least 1x1. */
  float2 ellipse_major_axis = ellipse_width * unit_eigenvector;
  float2 ellipse_minor_axis = ellipse_height * unit_eigenvector.yx * float2(-1, 1);
  int2 ellipse_bounds = int2(ceil(sqrt(square(ellipse_major_axis) + square(ellipse_minor_axis))));

  /* Compute the overlap polynomial parameters for 8-sector ellipse based on the equations in
   * section "3 Alternative Weighting Functions" of the polynomial weights paper. More on this
   * later in the code. */
  constexpr int number_of_sectors = 8;
  float sector_center_overlap_parameter = 2.0f / radius;
  float sector_envelope_angle = ((3.0f / 2.0f) * M_PI) / number_of_sectors;
  float cross_sector_overlap_parameter = (sector_center_overlap_parameter +
                                          cos(sector_envelope_angle)) /
                                         square(sin(sector_envelope_angle));

  /* We need to compute the weighted mean of color and squared color of each of the 8 sectors of
   * the ellipse, so we declare arrays for accumulating those and initialize them in the next code
   * section. */
  float4 weighted_mean_of_squared_color_of_sectors[8];
  float4 weighted_mean_of_color_of_sectors[8];
  float sum_of_weights_of_sectors[8];

  /* The center pixel (0, 0) is exempt from the main loop below for reasons that are explained in
   * the first if statement in the loop, so we need to accumulate its color, squared color, and
   * weight separately first. Luckily, the zero coordinates of the center pixel zeros out most of
   * the complex computations below, and it can easily be shown that the weight for the center
   * pixel in all sectors is simply (1 / number_of_sectors). */
  float4 center_color = texture_load(input_tx, texel);
  float4 center_color_squared = center_color * center_color;
  float center_weight = 1.0f / number_of_sectors;
  float4 weighted_center_color = center_color * center_weight;
  float4 weighted_center_color_squared = center_color_squared * center_weight;
  for (int i = 0; i < number_of_sectors; i++) {
    weighted_mean_of_squared_color_of_sectors[i] = weighted_center_color_squared;
    weighted_mean_of_color_of_sectors[i] = weighted_center_color;
    sum_of_weights_of_sectors[i] = center_weight;
  }

  /* Loop over the window of pixels inside the bounding box of the ellipse. However, we utilize the
   * fact that ellipses are mirror symmetric along the horizontal axis, so we reduce the window to
   * only the upper two quadrants, and compute each two mirrored pixels at the same time using the
   * same weight as an optimization. */
  for (int j = 0; j <= ellipse_bounds.y; j++) {
    for (int i = -ellipse_bounds.x; i <= ellipse_bounds.x; i++) {
      /* Since we compute each two mirrored pixels at the same time, we need to also exempt the
       * pixels whose x coordinates are negative and their y coordinates are zero, that's because
       * those are mirrored versions of the pixels whose x coordinates are positive and their y
       * coordinates are zero, and we don't want to compute and accumulate them twice. Moreover, we
       * also need to exempt the center pixel with zero coordinates for the same reason, however,
       * since the mirror of the center pixel is itself, it need to be accumulated separately,
       * hence why we did that in the code section just before this loop. */
      if (j == 0 && i <= 0) {
        continue;
      }

      /* Map the pixels of the ellipse into a unit disk, exempting any points that are not part of
       * the ellipse or disk. */
      float2 disk_point = inverse_ellipse_matrix * float2(i, j);
      float disk_point_length_squared = dot(disk_point, disk_point);
      if (disk_point_length_squared > 1.0f) {
        continue;
      }

      /* While each pixel belongs to a single sector in the ellipse, we expand the definition of
       * a sector a bit to also overlap with other sectors as illustrated in Figure 8 of the
       * polynomial weights paper. So each pixel may contribute to multiple sectors, and thus we
       * compute its weight in each of the 8 sectors. */
      float sector_weights[8];

      /* We evaluate the weighting polynomial at each of the 8 sectors by rotating the disk point
       * by 45 degrees and evaluating the weighting polynomial at each incremental rotation. To
       * avoid potentially expensive rotations, we utilize the fact that rotations by 90 degrees
       * are simply swapping of the coordinates and negating the x component. We also note that
       * since the y term of the weighting polynomial is squared, it is not affected by the sign
       * and can be computed once for the x and once for the y coordinates. So we compute every
       * other even-indexed 4 weights by successive 90 degree rotations as discussed. */
      float2 polynomial = sector_center_overlap_parameter -
                          cross_sector_overlap_parameter * square(disk_point);
      sector_weights[0] = square(max(0.0f, disk_point.y + polynomial.x));
      sector_weights[2] = square(max(0.0f, -disk_point.x + polynomial.y));
      sector_weights[4] = square(max(0.0f, -disk_point.y + polynomial.x));
      sector_weights[6] = square(max(0.0f, disk_point.x + polynomial.y));

      /* Then we rotate the disk point by 45 degrees, which is a simple expression involving a
       * constant as can be demonstrated by applying a 45 degree rotation matrix. */
      float2 rotated_disk_point = M_SQRT1_2 *
                                  float2(disk_point.x - disk_point.y, disk_point.x + disk_point.y);

      /* Finally, we compute every other odd-index 4 weights starting from the 45 degrees rotated
       * disk point. */
      float2 rotated_polynomial = sector_center_overlap_parameter -
                                  cross_sector_overlap_parameter * square(rotated_disk_point);
      sector_weights[1] = square(max(0.0f, rotated_disk_point.y + rotated_polynomial.x));
      sector_weights[3] = square(max(0.0f, -rotated_disk_point.x + rotated_polynomial.y));
      sector_weights[5] = square(max(0.0f, -rotated_disk_point.y + rotated_polynomial.x));
      sector_weights[7] = square(max(0.0f, rotated_disk_point.x + rotated_polynomial.y));

      /* We compute a radial Gaussian weighting component such that pixels further away from the
       * sector center gets attenuated, and we also divide by the sum of sector weights to
       * normalize them, since the radial weight will eventually be multiplied to the sector weight
       * below. */
      float sector_weights_sum = sector_weights[0] + sector_weights[1] + sector_weights[2] +
                                 sector_weights[3] + sector_weights[4] + sector_weights[5] +
                                 sector_weights[6] + sector_weights[7];
      float radial_gaussian_weight = exp(-M_PI * disk_point_length_squared) / sector_weights_sum;

      /* Load the color of the pixel and its mirrored pixel and compute their square. */
      float4 upper_color = texture_load(input_tx, texel + int2(i, j));
      float4 lower_color = texture_load(input_tx, texel - int2(i, j));
      float4 upper_color_squared = upper_color * upper_color;
      float4 lower_color_squared = lower_color * lower_color;

      for (int k = 0; k < number_of_sectors; k++) {
        float weight = sector_weights[k] * radial_gaussian_weight;

        /* Accumulate the pixel to each of the sectors multiplied by the sector weight. */
        int upper_index = k;
        sum_of_weights_of_sectors[upper_index] += weight;
        weighted_mean_of_color_of_sectors[upper_index] += upper_color * weight;
        weighted_mean_of_squared_color_of_sectors[upper_index] += upper_color_squared * weight;

        /* Accumulate the mirrored pixel to each of the sectors multiplied by the sector weight. */
        int lower_index = (k + number_of_sectors / 2) % number_of_sectors;
        sum_of_weights_of_sectors[lower_index] += weight;
        weighted_mean_of_color_of_sectors[lower_index] += lower_color * weight;
        weighted_mean_of_squared_color_of_sectors[lower_index] += lower_color_squared * weight;
      }
    }
  }

  /* Compute the weighted sum of mean of sectors, such that sectors with lower standard deviation
   * gets more significant weight than sectors with higher standard deviation. */
  float sum_of_weights = 0.0f;
  float4 weighted_sum = float4(0.0f);
  for (int i = 0; i < number_of_sectors; i++) {
    weighted_mean_of_color_of_sectors[i] /= sum_of_weights_of_sectors[i];
    weighted_mean_of_squared_color_of_sectors[i] /= sum_of_weights_of_sectors[i];

    float4 color_mean = weighted_mean_of_color_of_sectors[i];
    float4 squared_color_mean = weighted_mean_of_squared_color_of_sectors[i];
    float4 color_variance = abs(squared_color_mean - color_mean * color_mean);

    float standard_deviation = dot(sqrt(color_variance.rgb), float3(1.0f));

    /* Compute the sector weight based on the weight function introduced in section "3.3.1
     * Single-scale Filtering" of the multi-scale paper. Use a threshold of 0.02 to avoid zero
     * division and avoid artifacts in homogeneous regions as demonstrated in the paper. */
    float weight = 1.0f / pow(max(0.02f, standard_deviation), sharpness);

    sum_of_weights += weight;
    weighted_sum += color_mean * weight;
  }

  /* Fallback to the original color if all sector weights are zero due to very high standard
   * deviation and sharpness. */
  if (sum_of_weights == 0.0f) {
    weighted_sum = center_color;
  }
  else {
    weighted_sum /= sum_of_weights;
  }

  imageStore(output_img, texel, weighted_sum);
}
