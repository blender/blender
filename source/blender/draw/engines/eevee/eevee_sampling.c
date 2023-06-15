/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup EEVEE
 */

#include "eevee_private.h"

#include "BLI_rand.h"

void EEVEE_sample_ball(int sample_ofs, float radius, float rsample[3])
{
  double ht_point[3];
  double ht_offset[3] = {0.0, 0.0, 0.0};
  const uint ht_primes[3] = {2, 3, 7};

  BLI_halton_3d(ht_primes, ht_offset, sample_ofs, ht_point);

  /* De-correlate AA and shadow samples. (see #68594) */
  ht_point[0] = fmod(ht_point[0] * 1151.0, 1.0);
  ht_point[1] = fmod(ht_point[1] * 1069.0, 1.0);
  ht_point[2] = fmod(ht_point[2] * 1151.0, 1.0);

  float omega = ht_point[1] * 2.0f * M_PI;

  rsample[2] = ht_point[0] * 2.0f - 1.0f; /* cos theta */

  float r = sqrtf(fmaxf(0.0f, 1.0f - rsample[2] * rsample[2])); /* sin theta */

  rsample[0] = r * cosf(omega);
  rsample[1] = r * sinf(omega);

  radius *= sqrt(sqrt(ht_point[2]));
  mul_v3_fl(rsample, radius);
}

void EEVEE_sample_rectangle(int sample_ofs,
                            const float x_axis[3],
                            const float y_axis[3],
                            float size_x,
                            float size_y,
                            float rsample[3])
{
  double ht_point[2];
  double ht_offset[2] = {0.0, 0.0};
  const uint ht_primes[2] = {2, 3};

  BLI_halton_2d(ht_primes, ht_offset, sample_ofs, ht_point);

  /* De-correlate AA and shadow samples. (see #68594) */
  ht_point[0] = fmod(ht_point[0] * 1151.0, 1.0);
  ht_point[1] = fmod(ht_point[1] * 1069.0, 1.0);

  /* Change distribution center to be 0,0 */
  ht_point[0] = (ht_point[0] > 0.5f) ? ht_point[0] - 1.0f : ht_point[0];
  ht_point[1] = (ht_point[1] > 0.5f) ? ht_point[1] - 1.0f : ht_point[1];

  zero_v3(rsample);
  madd_v3_v3fl(rsample, x_axis, (ht_point[0] * 2.0f) * size_x);
  madd_v3_v3fl(rsample, y_axis, (ht_point[1] * 2.0f) * size_y);
}

void EEVEE_sample_ellipse(int sample_ofs,
                          const float x_axis[3],
                          const float y_axis[3],
                          float size_x,
                          float size_y,
                          float rsample[3])
{
  double ht_point[2];
  double ht_offset[2] = {0.0, 0.0};
  const uint ht_primes[2] = {2, 3};

  BLI_halton_2d(ht_primes, ht_offset, sample_ofs, ht_point);

  /* Decorrelate AA and shadow samples. (see #68594) */

  ht_point[0] = fmod(ht_point[0] * 1151.0, 1.0);
  ht_point[1] = fmod(ht_point[1] * 1069.0, 1.0);

  /* Uniform disc sampling. */
  float omega = ht_point[1] * 2.0f * M_PI;
  float r = sqrtf(ht_point[0]);
  ht_point[0] = r * cosf(omega) * size_x;
  ht_point[1] = r * sinf(omega) * size_y;

  zero_v3(rsample);
  madd_v3_v3fl(rsample, x_axis, ht_point[0]);
  madd_v3_v3fl(rsample, y_axis, ht_point[1]);
}

void EEVEE_random_rotation_m4(int sample_ofs, float scale, float r_mat[4][4])
{
  double ht_point[3];
  double ht_offset[3] = {0.0, 0.0, 0.0};
  const uint ht_primes[3] = {2, 3, 5};

  BLI_halton_3d(ht_primes, ht_offset, sample_ofs, ht_point);

  /* Decorrelate AA and shadow samples. (see #68594) */
  ht_point[0] = fmod(ht_point[0] * 1151.0, 1.0);
  ht_point[1] = fmod(ht_point[1] * 1069.0, 1.0);
  ht_point[2] = fmod(ht_point[2] * 1151.0, 1.0);

  rotate_m4(r_mat, 'X', ht_point[0] * scale);
  rotate_m4(r_mat, 'Y', ht_point[1] * scale);
  rotate_m4(r_mat, 'Z', ht_point[2] * scale);
}
