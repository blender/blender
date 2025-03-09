/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Parts adapted from code in the public domain in NVidia Mesh Tools. */

#include "subd/patch.h"

#include "util/math.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* De Casteljau Evaluation */

static void decasteljau_cubic(float3 *P, float3 *dt, const float t, const float3 cp[4])
{
  float3 d0 = cp[0] + t * (cp[1] - cp[0]);
  float3 d1 = cp[1] + t * (cp[2] - cp[1]);
  const float3 d2 = cp[2] + t * (cp[3] - cp[2]);

  d0 += t * (d1 - d0);
  d1 += t * (d2 - d1);

  *P = d0 + t * (d1 - d0);
  if (dt) {
    *dt = d1 - d0;
  }
}

static void decasteljau_bicubic(
    float3 *P, float3 *du, float3 *dv, const float3 cp[16], float u, const float v)
{
  float3 ucp[4];
  float3 utn[4];

  /* interpolate over u */
  decasteljau_cubic(ucp + 0, utn + 0, u, cp);
  decasteljau_cubic(ucp + 1, utn + 1, u, cp + 4);
  decasteljau_cubic(ucp + 2, utn + 2, u, cp + 8);
  decasteljau_cubic(ucp + 3, utn + 3, u, cp + 12);

  /* interpolate over v */
  decasteljau_cubic(P, dv, v, ucp);
  if (du) {
    decasteljau_cubic(du, nullptr, v, utn);
  }
}

/* Linear Quad Patch */

void LinearQuadPatch::eval(
    float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) const
{
  const float3 d0 = interp(hull[0], hull[1], u);
  const float3 d1 = interp(hull[2], hull[3], u);

  *P = interp(d0, d1, v);

  if (N || (dPdu && dPdv)) {
    const float3 dPdu_ = interp(hull[1] - hull[0], hull[3] - hull[2], v);
    const float3 dPdv_ = interp(hull[2] - hull[0], hull[3] - hull[1], u);

    if (dPdu && dPdv) {
      *dPdu = dPdu_;
      *dPdv = dPdv_;
    }

    if (N) {
      *N = normalize(cross(dPdu_, dPdv_));
    }
  }
}

BoundBox LinearQuadPatch::bound()
{
  BoundBox bbox = BoundBox::empty;

  for (int i = 0; i < 4; i++) {
    bbox.grow(hull[i]);
  }

  return bbox;
}

/* Bicubic Patch */

void BicubicPatch::eval(
    float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, const float v) const
{
  if (N) {
    float3 dPdu_;
    float3 dPdv_;
    decasteljau_bicubic(P, &dPdu_, &dPdv_, hull, u, v);

    if (dPdu && dPdv) {
      *dPdu = dPdu_;
      *dPdv = dPdv_;
    }

    *N = normalize(cross(dPdu_, dPdv_));
  }
  else {
    decasteljau_bicubic(P, dPdu, dPdv, hull, u, v);
  }
}

BoundBox BicubicPatch::bound()
{
  BoundBox bbox = BoundBox::empty;

  for (int i = 0; i < 16; i++) {
    bbox.grow(hull[i]);
  }

  return bbox;
}

CCL_NAMESPACE_END
