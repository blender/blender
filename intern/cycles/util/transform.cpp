/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "util/transform.h"
#include "util/projection.h"

#include "util/boundbox.h"
#include "util/math.h"

CCL_NAMESPACE_BEGIN

/* Transform Inverse */

static bool projection_matrix4_inverse(float R[][4], float M[][4])
{
  /* SPDX-License-Identifier: BSD-3-Clause
   * Adapted from code:
   * Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
   * Digital Ltd. LLC. All rights reserved. */

  /* forward elimination */
  for (int i = 0; i < 4; i++) {
    int pivot = i;
    float pivotsize = M[i][i];

    if (pivotsize < 0)
      pivotsize = -pivotsize;

    for (int j = i + 1; j < 4; j++) {
      float tmp = M[j][i];

      if (tmp < 0)
        tmp = -tmp;

      if (tmp > pivotsize) {
        pivot = j;
        pivotsize = tmp;
      }
    }

    if (UNLIKELY(pivotsize == 0.0f))
      return false;

    if (pivot != i) {
      for (int j = 0; j < 4; j++) {
        float tmp;

        tmp = M[i][j];
        M[i][j] = M[pivot][j];
        M[pivot][j] = tmp;

        tmp = R[i][j];
        R[i][j] = R[pivot][j];
        R[pivot][j] = tmp;
      }
    }

    for (int j = i + 1; j < 4; j++) {
      float f = M[j][i] / M[i][i];

      for (int k = 0; k < 4; k++) {
        M[j][k] -= f * M[i][k];
        R[j][k] -= f * R[i][k];
      }
    }
  }

  /* backward substitution */
  for (int i = 3; i >= 0; --i) {
    float f;

    if (UNLIKELY((f = M[i][i]) == 0.0f))
      return false;

    for (int j = 0; j < 4; j++) {
      M[i][j] /= f;
      R[i][j] /= f;
    }

    for (int j = 0; j < i; j++) {
      f = M[j][i];

      for (int k = 0; k < 4; k++) {
        M[j][k] -= f * M[i][k];
        R[j][k] -= f * R[i][k];
      }
    }
  }

  return true;
}

ProjectionTransform projection_inverse(const ProjectionTransform &tfm)
{
  ProjectionTransform tfmR = projection_identity();
  float M[4][4], R[4][4];

  memcpy(R, &tfmR, sizeof(R));
  memcpy(M, &tfm, sizeof(M));

  if (UNLIKELY(!projection_matrix4_inverse(R, M))) {
    return projection_identity();
  }

  memcpy(&tfmR.x[0], R, sizeof(R));

  return tfmR;
}

Transform transform_transposed_inverse(const Transform &tfm)
{
  ProjectionTransform iprojection(transform_inverse(tfm));
  return projection_to_transform(projection_transpose(iprojection));
}

/* Motion Transform */

float4 transform_to_quat(const Transform &tfm)
{
  double trace = (double)(tfm[0][0] + tfm[1][1] + tfm[2][2]);
  float4 qt;

  if (trace > 0.0) {
    double s = sqrt(trace + 1.0);

    qt.w = (float)(s / 2.0);
    s = 0.5 / s;

    qt.x = (float)((double)(tfm[2][1] - tfm[1][2]) * s);
    qt.y = (float)((double)(tfm[0][2] - tfm[2][0]) * s);
    qt.z = (float)((double)(tfm[1][0] - tfm[0][1]) * s);
  }
  else {
    int i = 0;

    if (tfm[1][1] > tfm[i][i])
      i = 1;
    if (tfm[2][2] > tfm[i][i])
      i = 2;

    int j = (i + 1) % 3;
    int k = (j + 1) % 3;

    double s = sqrt((double)(tfm[i][i] - (tfm[j][j] + tfm[k][k])) + 1.0);

    double q[3];
    q[i] = s * 0.5;
    if (s != 0.0)
      s = 0.5 / s;

    double w = (double)(tfm[k][j] - tfm[j][k]) * s;
    q[j] = (double)(tfm[j][i] + tfm[i][j]) * s;
    q[k] = (double)(tfm[k][i] + tfm[i][k]) * s;

    qt.x = (float)q[0];
    qt.y = (float)q[1];
    qt.z = (float)q[2];
    qt.w = (float)w;
  }

  return qt;
}

static void transform_decompose(DecomposedTransform *decomp, const Transform *tfm)
{
  /* extract translation */
  decomp->y = make_float4(tfm->x.w, tfm->y.w, tfm->z.w, 0.0f);

  /* extract rotation */
  Transform M = *tfm;
  M.x.w = 0.0f;
  M.y.w = 0.0f;
  M.z.w = 0.0f;

#if 0
  Transform R = M;
  float norm;
  int iteration = 0;

  do {
    Transform Rnext;
    Transform Rit = transform_transposed_inverse(R);

    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 4; j++)
        Rnext[i][j] = 0.5f * (R[i][j] + Rit[i][j]);

    norm = 0.0f;
    for (int i = 0; i < 3; i++) {
      norm = max(norm,
                 fabsf(R[i][0] - Rnext[i][0]) + fabsf(R[i][1] - Rnext[i][1]) +
                     fabsf(R[i][2] - Rnext[i][2]));
    }

    R = Rnext;
    iteration++;
  } while (iteration < 100 && norm > 1e-4f);

  if (transform_negative_scale(R))
    R = R * transform_scale(-1.0f, -1.0f, -1.0f);

  decomp->x = transform_to_quat(R);

  /* extract scale and pack it */
  Transform scale = transform_inverse(R) * M;
  decomp->y.w = scale.x.x;
  decomp->z = make_float4(scale.x.y, scale.x.z, scale.y.x, scale.y.y);
  decomp->w = make_float4(scale.y.z, scale.z.x, scale.z.y, scale.z.z);
#else
  float3 colx = transform_get_column(&M, 0);
  float3 coly = transform_get_column(&M, 1);
  float3 colz = transform_get_column(&M, 2);

  /* extract scale and shear first */
  float3 scale, shear;
  scale.x = len(colx);
  colx = safe_divide(colx, scale.x);
  shear.z = dot(colx, coly);
  coly -= shear.z * colx;
  scale.y = len(coly);
  coly = safe_divide(coly, scale.y);
  shear.y = dot(colx, colz);
  colz -= shear.y * colx;
  shear.x = dot(coly, colz);
  colz -= shear.x * coly;
  scale.z = len(colz);
  colz = safe_divide(colz, scale.z);

  transform_set_column(&M, 0, colx);
  transform_set_column(&M, 1, coly);
  transform_set_column(&M, 2, colz);

  if (transform_negative_scale(M)) {
    scale *= -1.0f;
    M = M * transform_scale(-1.0f, -1.0f, -1.0f);
  }

  decomp->x = transform_to_quat(M);

  decomp->y.w = scale.x;
  decomp->z = make_float4(shear.z, shear.y, 0.0f, scale.y);
  decomp->w = make_float4(shear.x, 0.0f, 0.0f, scale.z);
#endif
}

void transform_motion_decompose(DecomposedTransform *decomp, const Transform *motion, size_t size)
{
  /* Decompose and correct rotation. */
  for (size_t i = 0; i < size; i++) {
    transform_decompose(decomp + i, motion + i);

    if (i > 0) {
      /* Ensure rotation around shortest angle, negated quaternions are the same
       * but this means we don't have to do the check in quat_interpolate */
      if (dot(decomp[i - 1].x, decomp[i].x) < 0.0f)
        decomp[i].x = -decomp[i].x;
    }
  }

  /* Copy rotation to decomposed transform where scale is degenerate. This avoids weird object
   * rotation interpolation when the scale goes to 0 for a time step.
   *
   * Note that this is very simple and naive implementation, which only deals with degenerated
   * scale happening only on one frame. It is possible to improve it further by interpolating
   * rotation into s degenerated range using rotation from time-steps from adjacent non-degenerated
   * time steps. */
  for (size_t i = 0; i < size; i++) {
    const float3 scale = make_float3(decomp[i].y.w, decomp[i].z.w, decomp[i].w.w);
    if (!is_zero(scale)) {
      continue;
    }

    if (i > 0) {
      decomp[i].x = decomp[i - 1].x;
    }
    else if (i < size - 1) {
      decomp[i].x = decomp[i + 1].x;
    }
  }
}

Transform transform_from_viewplane(BoundBox2D &viewplane)
{
  return transform_scale(1.0f / (viewplane.right - viewplane.left),
                         1.0f / (viewplane.top - viewplane.bottom),
                         1.0f) *
         transform_translate(-viewplane.left, -viewplane.bottom, 0.0f);
}

CCL_NAMESPACE_END
