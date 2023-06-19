/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/curves.h"
#include "device/device.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"

#include "util/foreach.h"
#include "util/map.h"
#include "util/progress.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Curve functions */

void curvebounds(float *lower, float *upper, float3 *p, int dim)
{
  float *p0 = &p[0].x;
  float *p1 = &p[1].x;
  float *p2 = &p[2].x;
  float *p3 = &p[3].x;

  /* Catmull-Rom weights. */
  float curve_coef[4];
  curve_coef[0] = p1[dim];
  curve_coef[1] = 0.5f * (-p0[dim] + p2[dim]);
  curve_coef[2] = 0.5f * (2 * p0[dim] - 5 * p1[dim] + 4 * p2[dim] - p3[dim]);
  curve_coef[3] = 0.5f * (-p0[dim] + 3 * p1[dim] - 3 * p2[dim] + p3[dim]);

  float discroot = curve_coef[2] * curve_coef[2] - 3 * curve_coef[3] * curve_coef[1];
  float ta = -1.0f;
  float tb = -1.0f;

  if (discroot >= 0) {
    discroot = sqrtf(discroot);
    ta = (-curve_coef[2] - discroot) / (3 * curve_coef[3]);
    tb = (-curve_coef[2] + discroot) / (3 * curve_coef[3]);
    ta = (ta > 1.0f || ta < 0.0f) ? -1.0f : ta;
    tb = (tb > 1.0f || tb < 0.0f) ? -1.0f : tb;
  }

  *upper = max(p1[dim], p2[dim]);
  *lower = min(p1[dim], p2[dim]);

  float exa = p1[dim];
  float exb = p2[dim];

  if (ta >= 0.0f) {
    float t2 = ta * ta;
    float t3 = t2 * ta;
    exa = curve_coef[3] * t3 + curve_coef[2] * t2 + curve_coef[1] * ta + curve_coef[0];
  }
  if (tb >= 0.0f) {
    float t2 = tb * tb;
    float t3 = t2 * tb;
    exb = curve_coef[3] * t3 + curve_coef[2] * t2 + curve_coef[1] * tb + curve_coef[0];
  }

  *upper = max(*upper, max(exa, exb));
  *lower = min(*lower, min(exa, exb));
}

CCL_NAMESPACE_END
