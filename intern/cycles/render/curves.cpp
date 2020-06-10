/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/curves.h"
#include "device/device.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"

#include "util/util_foreach.h"
#include "util/util_map.h"
#include "util/util_progress.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

/* Curve functions */

void curvebounds(float *lower, float *upper, float3 *p, int dim)
{
  float *p0 = &p[0].x;
  float *p1 = &p[1].x;
  float *p2 = &p[2].x;
  float *p3 = &p[3].x;

  float fc = 0.71f;
  float curve_coef[4];
  curve_coef[0] = p1[dim];
  curve_coef[1] = -fc * p0[dim] + fc * p2[dim];
  curve_coef[2] = 2.0f * fc * p0[dim] + (fc - 3.0f) * p1[dim] + (3.0f - 2.0f * fc) * p2[dim] -
                  fc * p3[dim];
  curve_coef[3] = -fc * p0[dim] + (2.0f - fc) * p1[dim] + (fc - 2.0f) * p2[dim] + fc * p3[dim];

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

/* Hair System Manager */

CurveSystemManager::CurveSystemManager()
{
  curve_shape = CURVE_THICK;
  subdivisions = 3;

  use_curves = true;

  need_update = true;
  need_mesh_update = false;
}

CurveSystemManager::~CurveSystemManager()
{
}

void CurveSystemManager::device_update(Device *device,
                                       DeviceScene *dscene,
                                       Scene * /*scene*/,
                                       Progress &progress)
{
  if (!need_update)
    return;

  device_free(device, dscene);

  progress.set_status("Updating Hair settings", "Copying Hair settings to device");

  KernelCurves *kcurve = &dscene->data.curve;

  kcurve->curveflags = 0;

  if (use_curves) {
    if (curve_shape == CURVE_RIBBON) {
      kcurve->curveflags |= CURVE_KN_RIBBONS;
    }

    kcurve->subdivisions = subdivisions;
  }

  if (progress.get_cancel())
    return;

  need_update = false;
}

void CurveSystemManager::device_free(Device * /*device*/, DeviceScene * /*dscene*/)
{
}

bool CurveSystemManager::modified(const CurveSystemManager &CurveSystemManager)
{
  return !(use_curves == CurveSystemManager.use_curves &&
           subdivisions == CurveSystemManager.subdivisions);
}

bool CurveSystemManager::modified_mesh(const CurveSystemManager &CurveSystemManager)
{
  return !(use_curves == CurveSystemManager.use_curves);
}

void CurveSystemManager::tag_update(Scene * /*scene*/)
{
  need_update = true;
}

void CurveSystemManager::tag_update_mesh()
{
  need_mesh_update = true;
}
CCL_NAMESPACE_END
