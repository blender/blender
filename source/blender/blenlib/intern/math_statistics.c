/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_statistics.h"
#include "BLI_math_vector.h"

#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

/********************************** Covariance Matrices *********************************/

typedef struct CovarianceData {
  const float *cos_vn;
  const float *center;
  float *r_covmat;
  float covfac;
  int n;
  int cos_vn_num;
} CovarianceData;

static void covariance_m_vn_ex_task_cb(void *__restrict userdata,
                                       const int a,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  CovarianceData *data = userdata;
  const float *cos_vn = data->cos_vn;
  const float *center = data->center;
  float *r_covmat = data->r_covmat;
  const int n = data->n;
  const int cos_vn_num = data->cos_vn_num;

  int k;

  /* Covariance matrices are always symmetrical, so we can compute only one half of it,
   * and mirror it to the other half (at the end of the func).
   *
   * This allows using a flat loop of n*n with same results as imbricated one over half the matrix:
   *
   *     for (i = 0; i < n; i++) {
   *         for (j = i; j < n; j++) {
   *             ...
   *         }
   *      }
   */
  const int i = a / n;
  const int j = a % n;
  if (j < i) {
    return;
  }

  if (center) {
    for (k = 0; k < cos_vn_num; k++) {
      r_covmat[a] += (cos_vn[k * n + i] - center[i]) * (cos_vn[k * n + j] - center[j]);
    }
  }
  else {
    for (k = 0; k < cos_vn_num; k++) {
      r_covmat[a] += cos_vn[k * n + i] * cos_vn[k * n + j];
    }
  }
  r_covmat[a] *= data->covfac;
  if (j != i) {
    /* Mirror result to other half... */
    r_covmat[j * n + i] = r_covmat[a];
  }
}

void BLI_covariance_m_vn_ex(const int n,
                            const float *cos_vn,
                            const int cos_vn_num,
                            const float *center,
                            const bool use_sample_correction,
                            float *r_covmat)
{
  /* Note about that division: see https://en.wikipedia.org/wiki/Bessel%27s_correction.
   * In a nutshell, it must be 1 / (n - 1) for 'sample data', and 1 / n for 'population data'...
   */
  const float covfac = 1.0f / (float)(use_sample_correction ? cos_vn_num - 1 : cos_vn_num);

  memset(r_covmat, 0, sizeof(*r_covmat) * (size_t)(n * n));

  CovarianceData data = {
      .cos_vn = cos_vn,
      .center = center,
      .r_covmat = r_covmat,
      .covfac = covfac,
      .n = n,
      .cos_vn_num = cos_vn_num,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = ((cos_vn_num * n * n) >= 10000);
  BLI_task_parallel_range(0, n * n, &data, covariance_m_vn_ex_task_cb, &settings);
}

void BLI_covariance_m3_v3n(const float (*cos_v3)[3],
                           const int cos_v3_num,
                           const bool use_sample_correction,
                           float r_covmat[3][3],
                           float r_center[3])
{
  float center[3];
  const float mean_fac = 1.0f / (float)cos_v3_num;
  int i;

  zero_v3(center);
  for (i = 0; i < cos_v3_num; i++) {
    /* Applying mean_fac here rather than once at the end reduce compute errors... */
    madd_v3_v3fl(center, cos_v3[i], mean_fac);
  }

  if (r_center) {
    copy_v3_v3(r_center, center);
  }

  BLI_covariance_m_vn_ex(
      3, (const float *)cos_v3, cos_v3_num, center, use_sample_correction, (float *)r_covmat);
}
