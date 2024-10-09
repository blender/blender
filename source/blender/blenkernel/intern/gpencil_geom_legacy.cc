/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_heap.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_span.hh"
#include "BLI_string_utils.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_material.h"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "DEG_depsgraph_query.hh"

using blender::float3;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Grease Pencil Object: Bound-box Support
 * \{ */

bool BKE_gpencil_stroke_minmax(const bGPDstroke *gps,
                               const bool use_select,
                               float r_min[3],
                               float r_max[3])
{
  if (gps == nullptr) {
    return false;
  }

  bool changed = false;
  if (use_select) {
    for (const bGPDspoint &pt : Span(gps->points, gps->totpoints)) {
      if (pt.flag & GP_SPOINT_SELECT) {
        minmax_v3v3_v3(r_min, r_max, &pt.x);
        changed = true;
      }
    }
  }
  else {
    for (const bGPDspoint &pt : Span(gps->points, gps->totpoints)) {
      minmax_v3v3_v3(r_min, r_max, &pt.x);
      changed = true;
    }
  }

  return changed;
}

std::optional<blender::Bounds<blender::float3>> BKE_gpencil_data_minmax(const bGPdata *gpd)
{
  bool changed = false;

  float3 min;
  float3 max;
  INIT_MINMAX(min, max);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = gpl->actframe;

    if (gpf != nullptr) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        changed |= BKE_gpencil_stroke_minmax(gps, false, min, max);
      }
    }
  }

  if (!changed) {
    return std::nullopt;
  }

  return blender::Bounds<blender::float3>{min, max};
}

void BKE_gpencil_centroid_3d(bGPdata *gpd, float r_centroid[3])
{
  using namespace blender;
  const Bounds<float3> bounds = BKE_gpencil_data_minmax(gpd).value_or(Bounds(float3(0)));
  copy_v3_v3(r_centroid, math::midpoint(bounds.min, bounds.max));
}

void BKE_gpencil_stroke_boundingbox_calc(bGPDstroke *gps)
{
  INIT_MINMAX(gps->boundbox_min, gps->boundbox_max);
  BKE_gpencil_stroke_minmax(gps, false, gps->boundbox_min, gps->boundbox_max);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth Positions
 * \{ */

bool BKE_gpencil_stroke_smooth_point(bGPDstroke *gps,
                                     int point_index,
                                     float influence,
                                     int iterations,
                                     const bool smooth_caps,
                                     const bool keep_shape,
                                     bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* - Overview of the algorithm here and in the following smooth functions:
   *
   *   The smooth functions return the new attribute in question for a single point.
   *   The result is stored in r_gps->points[point_index], while the data is read from gps.
   *   To get a correct result, duplicate the stroke point data and read from the copy,
   *   while writing to the real stroke. Not doing that will result in acceptable, but
   *   asymmetric results.
   *
   * This algorithm works as long as all points are being smoothed. If there is
   * points that should not get smoothed, use the old repeat smooth pattern with
   * the parameter "iterations" set to 1 or 2. (2 matches the old algorithm).
   */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  /* If smooth_caps is false, the caps will not be translated by smoothing. */
  if (!smooth_caps && !is_cyclic && ELEM(point_index, 0, gps->totpoints - 1)) {
    copy_v3_v3(&r_gps->points[point_index].x, &pt->x);
    return true;
  }

  /* This function uses a binomial kernel, which is the discrete version of gaussian blur.
   * The weight for a vertex at the relative index point_index is
   * `w = nCr(n, j + n/2) / 2^n = (n/1 * (n-1)/2 * ... * (n-j-n/2)/(j+n/2)) / 2^n`
   * All weights together sum up to 1
   * This is equivalent to doing multiple iterations of averaging neighbors,
   * where n = iterations * 2 and -n/2 <= j <= n/2
   *
   * Now the problem is that `nCr(n, j + n/2)` is very hard to compute for `n > 500`, since even
   * double precision isn't sufficient. A very good robust approximation for n > 20 is
   * `nCr(n, j + n/2) / 2^n = sqrt(2/(pi*n)) * exp(-2*j*j/n)`
   *
   * There is one more problem left: The old smooth algorithm was doing a more aggressive
   * smooth. To solve that problem, choose a different n/2, which does not match the range and
   * normalize the weights on finish. This may cause some artifacts at low values.
   *
   * keep_shape is a new option to stop the stroke from severely deforming.
   * It uses different partially negative weights.
   * w = `2 * (nCr(n, j + n/2) / 2^n) - (nCr(3*n, j + n) / 2^(3*n))`
   *   ~ `2 * sqrt(2/(pi*n)) * exp(-2*j*j/n) - sqrt(2/(pi*3*n)) * exp(-2*j*j/(3*n))`
   * All weights still sum up to 1.
   * Note these weights only work because the averaging is done in relative coordinates.
   */
  float sco[3] = {0.0f, 0.0f, 0.0f};
  float tmp[3];
  const int n_half = keep_shape ? (iterations * iterations) / 8 + iterations :
                                  (iterations * iterations) / 4 + 2 * iterations + 12;
  double w = keep_shape ? 2.0 : 1.0;
  double w2 = keep_shape ?
                  (1.0 / M_SQRT3) * exp((2 * iterations * iterations) / double(n_half * 3)) :
                  0.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w - w2);
    float w_after = float(w - w2);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      if (before < 0) {
        if (!smooth_caps) {
          w_before *= -before / float(point_index);
        }
        before = 0;
      }
      if (after > gps->totpoints - 1) {
        if (!smooth_caps) {
          w_after *= (after - (gps->totpoints - 1)) / float(gps->totpoints - 1 - point_index);
        }
        after = gps->totpoints - 1;
      }
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    sub_v3_v3v3(tmp, &gps->points[before].x, &pt->x);
    madd_v3_v3fl(sco, tmp, w_before);
    sub_v3_v3v3(tmp, &gps->points[after].x, &pt->x);
    madd_v3_v3fl(sco, tmp, w_after);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
    w2 *= (n_half * 3 + step) / double(n_half * 3 + 1 - step);
  }
  total_w += w - w2;
  /* The accumulated weight total_w should be
   * `~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100`
   * here, but sometimes not quite. */
  mul_v3_fl(sco, float(1.0 / total_w));
  /* Shift back to global coordinates. */
  add_v3_v3(sco, &pt->x);

  /* Based on influence factor, blend between original and optimal smoothed coordinate. */
  interp_v3_v3v3(&r_gps->points[point_index].x, &pt->x, sco, influence);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth Strength
 * \{ */

bool BKE_gpencil_stroke_smooth_strength(
    bGPDstroke *gps, int point_index, float influence, int iterations, bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* See BKE_gpencil_stroke_smooth_point for details on the algorithm. */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  float strength = 0.0f;
  const int n_half = (iterations * iterations) / 4 + iterations;
  double w = 1.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w);
    float w_after = float(w);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      CLAMP_MIN(before, 0);
      CLAMP_MAX(after, gps->totpoints - 1);
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    strength += w_before * (gps->points[before].strength - pt->strength);
    strength += w_after * (gps->points[after].strength - pt->strength);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
  }
  total_w += w;
  /* The accumulated weight total_w should be
   * ~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100
   * here, but sometimes not quite. */
  strength /= total_w;

  /* Based on influence factor, blend between original and optimal smoothed value. */
  r_gps->points[point_index].strength = pt->strength + strength * influence;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth Thickness
 * \{ */

bool BKE_gpencil_stroke_smooth_thickness(
    bGPDstroke *gps, int point_index, float influence, int iterations, bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* See BKE_gpencil_stroke_smooth_point for details on the algorithm. */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  float pressure = 0.0f;
  const int n_half = (iterations * iterations) / 4 + iterations;
  double w = 1.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w);
    float w_after = float(w);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      CLAMP_MIN(before, 0);
      CLAMP_MAX(after, gps->totpoints - 1);
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    pressure += w_before * (gps->points[before].pressure - pt->pressure);
    pressure += w_after * (gps->points[after].pressure - pt->pressure);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
  }
  total_w += w;
  /* The accumulated weight total_w should be
   * ~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100
   * here, but sometimes not quite. */
  pressure /= total_w;

  /* Based on influence factor, blend between original and optimal smoothed value. */
  r_gps->points[point_index].pressure = pt->pressure + pressure * influence;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth UV
 * \{ */

bool BKE_gpencil_stroke_smooth_uv(
    bGPDstroke *gps, int point_index, float influence, int iterations, bGPDstroke *r_gps)
{
  /* If nothing to do, return early */
  if (gps->totpoints <= 2 || iterations <= 0) {
    return false;
  }

  /* See BKE_gpencil_stroke_smooth_point for details on the algorithm. */

  const bGPDspoint *pt = &gps->points[point_index];
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;

  /* If don't change the caps. */
  if (!is_cyclic && ELEM(point_index, 0, gps->totpoints - 1)) {
    r_gps->points[point_index].uv_rot = pt->uv_rot;
    r_gps->points[point_index].uv_fac = pt->uv_fac;
    return true;
  }

  float uv_rot = 0.0f;
  float uv_fac = 0.0f;
  const int n_half = iterations * iterations + iterations;
  double w = 1.0;
  double total_w = 0.0;
  for (int step = iterations; step > 0; step--) {
    int before = point_index - step;
    int after = point_index + step;
    float w_before = float(w);
    float w_after = float(w);

    if (is_cyclic) {
      before = (before % gps->totpoints + gps->totpoints) % gps->totpoints;
      after = after % gps->totpoints;
    }
    else {
      if (before < 0) {
        w_before *= -before / float(point_index);
        before = 0;
      }
      if (after > gps->totpoints - 1) {
        w_after *= (after - (gps->totpoints - 1)) / float(gps->totpoints - 1 - point_index);
        after = gps->totpoints - 1;
      }
    }

    /* Add both these points in relative coordinates to the weighted average sum. */
    uv_rot += w_before * (gps->points[before].uv_rot - pt->uv_rot);
    uv_rot += w_after * (gps->points[after].uv_rot - pt->uv_rot);
    uv_fac += w_before * (gps->points[before].uv_fac - pt->uv_fac);
    uv_fac += w_after * (gps->points[after].uv_fac - pt->uv_fac);

    total_w += w_before;
    total_w += w_after;

    w *= (n_half + step) / double(n_half + 1 - step);
  }
  total_w += w;
  /* The accumulated weight total_w should be
   * ~sqrt(M_PI * n_half) * exp((iterations * iterations) / n_half) < 100
   * here, but sometimes not quite. */
  uv_rot /= total_w;
  uv_fac /= total_w;

  /* Based on influence factor, blend between original and optimal smoothed value. */
  r_gps->points[point_index].uv_rot = pt->uv_rot + uv_rot * influence;
  r_gps->points[point_index].uv_fac = pt->uv_fac + uv_fac * influence;

  return true;
}

void BKE_gpencil_stroke_smooth(bGPDstroke *gps,
                               const float influence,
                               const int iterations,
                               const bool smooth_position,
                               const bool smooth_strength,
                               const bool smooth_thickness,
                               const bool smooth_uv,
                               const bool keep_shape,
                               const float *weights)
{
  if (influence <= 0 || iterations <= 0) {
    return;
  }

  /* Make a copy of the point data to avoid directionality of the smooth operation. */
  bGPDstroke gps_old = blender::dna::shallow_copy(*gps);
  gps_old.points = (bGPDspoint *)MEM_dupallocN(gps->points);

  /* Smooth stroke. */
  for (int i = 0; i < gps->totpoints; i++) {
    float val = influence;
    if (weights != nullptr) {
      val *= weights[i];
      if (val <= 0.0f) {
        continue;
      }
    }

    /* TODO: Currently the weights only control the influence, but is would be much better if they
     * would control the distribution used in smooth, similar to how the ends are handled. */

    /* Perform smoothing. */
    if (smooth_position) {
      BKE_gpencil_stroke_smooth_point(&gps_old, i, val, iterations, false, keep_shape, gps);
    }
    if (smooth_strength) {
      BKE_gpencil_stroke_smooth_strength(&gps_old, i, val, iterations, gps);
    }
    if (smooth_thickness) {
      BKE_gpencil_stroke_smooth_thickness(&gps_old, i, val, iterations, gps);
    }
    if (smooth_uv) {
      BKE_gpencil_stroke_smooth_uv(&gps_old, i, val, iterations, gps);
    }
  }

  /* Free the copied points array. */
  MEM_freeN(gps_old.points);
}

void BKE_gpencil_stroke_2d_flat(const bGPDspoint *points,
                                int totpoints,
                                float (*points2d)[2],
                                int *r_direction)
{
  BLI_assert(totpoints >= 2);

  const bGPDspoint *pt0 = &points[0];
  const bGPDspoint *pt1 = &points[1];
  const bGPDspoint *pt3 = &points[int(totpoints * 0.75)];

  float locx[3];
  float locy[3];
  float loc3[3];
  float normal[3];

  /* local X axis (p0 -> p1) */
  sub_v3_v3v3(locx, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  float v3[3];
  if (totpoints == 2) {
    mul_v3_v3fl(v3, &pt3->x, 0.001f);
  }
  else {
    copy_v3_v3(v3, &pt3->x);
  }

  sub_v3_v3v3(loc3, v3, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(normal, locx, loc3);

  /* local Y axis (cross to normal/x axis) */
  cross_v3_v3v3(locy, normal, locx);

  /* Normalize vectors */
  normalize_v3(locx);
  normalize_v3(locy);

  /* Calculate last point first. */
  const bGPDspoint *pt_last = &points[totpoints - 1];
  float tmp[3];
  sub_v3_v3v3(tmp, &pt_last->x, &pt0->x);

  points2d[totpoints - 1][0] = dot_v3v3(tmp, locx);
  points2d[totpoints - 1][1] = dot_v3v3(tmp, locy);

  /* Calculate the scalar cross product of the 2d points. */
  float cross = 0.0f;
  float *co_curr;
  float *co_prev = (float *)&points2d[totpoints - 1];

  /* Get all points in local space */
  for (int i = 0; i < totpoints - 1; i++) {
    const bGPDspoint *pt = &points[i];
    float loc[3];

    /* Get local space using first point as origin */
    sub_v3_v3v3(loc, &pt->x, &pt0->x);

    points2d[i][0] = dot_v3v3(loc, locx);
    points2d[i][1] = dot_v3v3(loc, locy);

    /* Calculate cross product. */
    co_curr = (float *)&points2d[i][0];
    cross += (co_curr[0] - co_prev[0]) * (co_curr[1] + co_prev[1]);
    co_prev = (float *)&points2d[i][0];
  }

  /* Concave (-1), Convex (1) */
  *r_direction = (cross >= 0.0f) ? 1 : -1;
}

void BKE_gpencil_stroke_2d_flat_ref(const bGPDspoint *ref_points,
                                    int ref_totpoints,
                                    const bGPDspoint *points,
                                    int totpoints,
                                    float (*points2d)[2],
                                    const float scale,
                                    int *r_direction)
{
  BLI_assert(totpoints >= 2);

  const bGPDspoint *pt0 = &ref_points[0];
  const bGPDspoint *pt1 = &ref_points[1];
  const bGPDspoint *pt3 = &ref_points[int(ref_totpoints * 0.75)];

  float locx[3];
  float locy[3];
  float loc3[3];
  float normal[3];

  /* local X axis (p0 -> p1) */
  sub_v3_v3v3(locx, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  float v3[3];
  if (totpoints == 2) {
    mul_v3_v3fl(v3, &pt3->x, 0.001f);
  }
  else {
    copy_v3_v3(v3, &pt3->x);
  }

  sub_v3_v3v3(loc3, v3, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(normal, locx, loc3);

  /* local Y axis (cross to normal/x axis) */
  cross_v3_v3v3(locy, normal, locx);

  /* Normalize vectors */
  normalize_v3(locx);
  normalize_v3(locy);

  /* Get all points in local space */
  for (int i = 0; i < totpoints; i++) {
    const bGPDspoint *pt = &points[i];
    float loc[3];
    float v1[3];
    float vn[3] = {0.0f, 0.0f, 0.0f};

    /* apply scale to extremes of the stroke to get better collision detection
     * the scale is divided to get more control in the UI parameter
     */
    /* first point */
    if (i == 0) {
      const bGPDspoint *pt_next = &points[i + 1];
      sub_v3_v3v3(vn, &pt->x, &pt_next->x);
      normalize_v3(vn);
      mul_v3_fl(vn, scale / 10.0f);
      add_v3_v3v3(v1, &pt->x, vn);
    }
    /* last point */
    else if (i == totpoints - 1) {
      const bGPDspoint *pt_prev = &points[i - 1];
      sub_v3_v3v3(vn, &pt->x, &pt_prev->x);
      normalize_v3(vn);
      mul_v3_fl(vn, scale / 10.0f);
      add_v3_v3v3(v1, &pt->x, vn);
    }
    else {
      copy_v3_v3(v1, &pt->x);
    }

    /* Get local space using first point as origin (ref stroke) */
    sub_v3_v3v3(loc, v1, &pt0->x);

    points2d[i][0] = dot_v3v3(loc, locx);
    points2d[i][1] = dot_v3v3(loc, locy);
  }

  /* Concave (-1), Convex (1), or Auto-detect (0)? */
  *r_direction = int(locy[2]);
}

/* Calc texture coordinates using flat projected points. */
static void gpencil_calc_stroke_fill_uv(const float (*points2d)[2],
                                        bGPDstroke *gps,
                                        const float minv[2],
                                        const float maxv[2],
                                        float (*r_uv)[2])
{
  const float s = sin(gps->uv_rotation);
  const float c = cos(gps->uv_rotation);

  /* Calc center for rotation. */
  float center[2] = {0.5f, 0.5f};
  float d[2];
  d[0] = maxv[0] - minv[0];
  d[1] = maxv[1] - minv[1];
  for (int i = 0; i < gps->totpoints; i++) {
    r_uv[i][0] = (points2d[i][0] - minv[0]) / d[0];
    r_uv[i][1] = (points2d[i][1] - minv[1]) / d[1];

    /* Apply translation. */
    add_v2_v2(r_uv[i], gps->uv_translation);

    /* Apply Rotation. */
    r_uv[i][0] -= center[0];
    r_uv[i][1] -= center[1];

    float x = r_uv[i][0] * c - r_uv[i][1] * s;
    float y = r_uv[i][0] * s + r_uv[i][1] * c;

    r_uv[i][0] = x + center[0];
    r_uv[i][1] = y + center[1];

    /* Apply scale. */
    if (gps->uv_scale != 0.0f) {
      mul_v2_fl(r_uv[i], 1.0f / gps->uv_scale);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Fill Triangulate
 * \{ */

void BKE_gpencil_stroke_fill_triangulate(bGPDstroke *gps)
{
  BLI_assert(gps->totpoints >= 3);

  /* allocate memory for temporary areas */
  gps->tot_triangles = gps->totpoints - 2;
  uint(*tmp_triangles)[3] = (uint(*)[3])MEM_mallocN(sizeof(*tmp_triangles) * gps->tot_triangles,
                                                    "GP Stroke temp triangulation");
  float(*points2d)[2] = (float(*)[2])MEM_mallocN(sizeof(*points2d) * gps->totpoints,
                                                 "GP Stroke temp 2d points");
  float(*uv)[2] = (float(*)[2])MEM_mallocN(sizeof(*uv) * gps->totpoints,
                                           "GP Stroke temp 2d uv data");

  int direction = 0;

  /* convert to 2d and triangulate */
  BKE_gpencil_stroke_2d_flat(gps->points, gps->totpoints, points2d, &direction);
  BLI_polyfill_calc(points2d, uint(gps->totpoints), direction, tmp_triangles);

  /* calc texture coordinates automatically */
  float minv[2];
  float maxv[2];
  /* first needs bounding box data */
  ARRAY_SET_ITEMS(minv, -1.0f, -1.0f);
  ARRAY_SET_ITEMS(maxv, 1.0f, 1.0f);

  /* calc uv data */
  gpencil_calc_stroke_fill_uv(points2d, gps, minv, maxv, uv);

  /* Save triangulation data. */
  if (gps->tot_triangles > 0) {
    MEM_SAFE_FREE(gps->triangles);
    gps->triangles = (bGPDtriangle *)MEM_callocN(sizeof(*gps->triangles) * gps->tot_triangles,
                                                 "GP Stroke triangulation");

    for (int i = 0; i < gps->tot_triangles; i++) {
      memcpy(gps->triangles[i].verts, tmp_triangles[i], sizeof(uint[3]));
    }

    /* Copy UVs to bGPDspoint. */
    for (int i = 0; i < gps->totpoints; i++) {
      copy_v2_v2(gps->points[i].uv_fill, uv[i]);
    }
  }
  else {
    /* No triangles needed - Free anything allocated previously */
    if (gps->triangles) {
      MEM_freeN(gps->triangles);
    }

    gps->triangles = nullptr;
  }

  /* clear memory */
  MEM_SAFE_FREE(tmp_triangles);
  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(uv);
}

void BKE_gpencil_stroke_uv_update(bGPDstroke *gps)
{
  if (gps == nullptr || gps->totpoints == 0) {
    return;
  }

  bGPDspoint *pt = gps->points;
  float totlen = 0.0f;
  pt[0].uv_fac = totlen;
  for (int i = 1; i < gps->totpoints; i++) {
    totlen += len_v3v3(&pt[i - 1].x, &pt[i].x);
    pt[i].uv_fac = totlen;
  }
}

void BKE_gpencil_stroke_geometry_update(bGPdata * /*gpd*/, bGPDstroke *gps)
{
  if (gps == nullptr) {
    return;
  }

  if (gps->totpoints > 2) {
    BKE_gpencil_stroke_fill_triangulate(gps);
  }
  else {
    gps->tot_triangles = 0;
    MEM_SAFE_FREE(gps->triangles);
  }

  /* calc uv data along the stroke */
  BKE_gpencil_stroke_uv_update(gps);

  /* Calc stroke bounding box. */
  BKE_gpencil_stroke_boundingbox_calc(gps);
}

float BKE_gpencil_stroke_length(const bGPDstroke *gps, bool use_3d)
{
  if (!gps->points || gps->totpoints < 2) {
    return 0.0f;
  }
  float *last_pt = &gps->points[0].x;
  float total_length = 0.0f;
  for (int i = 1; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    if (use_3d) {
      total_length += len_v3v3(&pt->x, last_pt);
    }
    else {
      total_length += len_v2v2(&pt->x, last_pt);
    }
    last_pt = &pt->x;
  }
  return total_length;
}

float BKE_gpencil_stroke_segment_length(const bGPDstroke *gps,
                                        const int start_index,
                                        const int end_index,
                                        bool use_3d)
{
  if (!gps->points || gps->totpoints < 2 || end_index <= start_index) {
    return 0.0f;
  }

  int index = std::max(start_index, 0) + 1;
  int last_index = std::min(end_index, gps->totpoints - 1) + 1;

  float *last_pt = &gps->points[index - 1].x;
  float total_length = 0.0f;
  for (int i = index; i < last_index; i++) {
    bGPDspoint *pt = &gps->points[i];
    if (use_3d) {
      total_length += len_v3v3(&pt->x, last_pt);
    }
    else {
      total_length += len_v2v2(&pt->x, last_pt);
    }
    last_pt = &pt->x;
  }
  return total_length;
}

bool BKE_gpencil_stroke_trim(bGPdata *gpd, bGPDstroke *gps)
{
  if (gps->totpoints < 4) {
    return false;
  }
  bool intersect = false;
  int start = 0;
  int end = 0;
  float point[3];
  /* loop segments from start until we have an intersection */
  for (int i = 0; i < gps->totpoints - 2; i++) {
    start = i;
    bGPDspoint *a = &gps->points[start];
    bGPDspoint *b = &gps->points[start + 1];
    for (int j = start + 2; j < gps->totpoints - 1; j++) {
      end = j + 1;
      bGPDspoint *c = &gps->points[j];
      bGPDspoint *d = &gps->points[end];
      float pointb[3];
      /* get intersection */
      if (isect_line_line_v3(&a->x, &b->x, &c->x, &d->x, point, pointb)) {
        if (len_v3(point) > 0.0f) {
          float closest[3];
          /* check intersection is on both lines */
          float lambda = closest_to_line_v3(closest, point, &a->x, &b->x);
          if ((lambda <= 0.0f) || (lambda >= 1.0f)) {
            continue;
          }
          lambda = closest_to_line_v3(closest, point, &c->x, &d->x);
          if ((lambda <= 0.0f) || (lambda >= 1.0f)) {
            continue;
          }

          intersect = true;
          break;
        }
      }
    }
    if (intersect) {
      break;
    }
  }

  /* trim unwanted points */
  if (intersect) {

    /* save points */
    bGPDspoint *old_points = (bGPDspoint *)MEM_dupallocN(gps->points);
    MDeformVert *old_dvert = nullptr;
    MDeformVert *dvert_src = nullptr;

    if (gps->dvert != nullptr) {
      old_dvert = (MDeformVert *)MEM_dupallocN(gps->dvert);
    }

    /* resize gps */
    int newtot = end - start + 1;

    gps->points = (bGPDspoint *)MEM_recallocN(gps->points, sizeof(*gps->points) * newtot);
    if (gps->dvert != nullptr) {
      gps->dvert = (MDeformVert *)MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * newtot);
    }

    for (int i = 0; i < newtot; i++) {
      int idx = start + i;
      bGPDspoint *pt_src = &old_points[idx];
      bGPDspoint *pt_new = &gps->points[i];
      *pt_new = blender::dna::shallow_copy(*pt_src);
      if (gps->dvert != nullptr) {
        dvert_src = &old_dvert[idx];
        MDeformVert *dvert = &gps->dvert[i];
        memcpy(dvert, dvert_src, sizeof(MDeformVert));
        if (dvert_src->dw) {
          memcpy(dvert->dw, dvert_src->dw, sizeof(MDeformWeight));
        }
      }
      if (ELEM(idx, start, end)) {
        copy_v3_v3(&pt_new->x, point);
      }
    }

    gps->totpoints = newtot;

    MEM_SAFE_FREE(old_points);
    MEM_SAFE_FREE(old_dvert);
  }

  BKE_gpencil_stroke_geometry_update(gpd, gps);

  return intersect;
}

bool BKE_gpencil_stroke_close(bGPDstroke *gps)
{
  bGPDspoint *pt1 = nullptr;
  bGPDspoint *pt2 = nullptr;

  /* Only can close a stroke with 3 points or more. */
  if (gps->totpoints < 3) {
    return false;
  }

  /* Calc average distance between points to get same level of sampling. */
  float dist_tot = 0.0f;
  for (int i = 0; i < gps->totpoints - 1; i++) {
    pt1 = &gps->points[i];
    pt2 = &gps->points[i + 1];
    dist_tot += len_v3v3(&pt1->x, &pt2->x);
  }
  /* Calc the average distance. */
  float dist_avg = dist_tot / (gps->totpoints - 1);

  /* Calc distance between last and first point. */
  pt1 = &gps->points[gps->totpoints - 1];
  pt2 = &gps->points[0];
  float dist_close = len_v3v3(&pt1->x, &pt2->x);

  /* if the distance to close is very small, don't need add points and just enable cyclic. */
  if (dist_close <= dist_avg) {
    gps->flag |= GP_STROKE_CYCLIC;
    return true;
  }

  /* Calc number of points required using the average distance. */
  int tot_newpoints = std::max<int>(dist_close / dist_avg, 1);

  /* Resize stroke array. */
  int old_tot = gps->totpoints;
  gps->totpoints += tot_newpoints;
  gps->points = (bGPDspoint *)MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
  if (gps->dvert != nullptr) {
    gps->dvert = (MDeformVert *)MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
  }

  /* Generate new points */
  pt1 = &gps->points[old_tot - 1];
  pt2 = &gps->points[0];
  bGPDspoint *pt = &gps->points[old_tot];
  for (int i = 1; i < tot_newpoints + 1; i++, pt++) {
    float step = (tot_newpoints > 1) ? (float(i) / float(tot_newpoints)) : 0.99f;
    /* Clamp last point to be near, but not on top of first point. */
    if ((tot_newpoints > 1) && (i == tot_newpoints)) {
      step *= 0.99f;
    }

    /* Average point. */
    interp_v3_v3v3(&pt->x, &pt1->x, &pt2->x, step);
    pt->pressure = interpf(pt2->pressure, pt1->pressure, step);
    pt->strength = interpf(pt2->strength, pt1->strength, step);
    pt->flag = 0;
    interp_v4_v4v4(pt->vert_color, pt1->vert_color, pt2->vert_color, step);
    /* Set point as selected. */
    if (gps->flag & GP_STROKE_SELECT) {
      pt->flag |= GP_SPOINT_SELECT;
    }

    /* Set weights. */
    if (gps->dvert != nullptr) {
      MDeformVert *dvert1 = &gps->dvert[old_tot - 1];
      MDeformWeight *dw1 = BKE_defvert_ensure_index(dvert1, 0);
      float weight_1 = dw1 ? dw1->weight : 0.0f;

      MDeformVert *dvert2 = &gps->dvert[0];
      MDeformWeight *dw2 = BKE_defvert_ensure_index(dvert2, 0);
      float weight_2 = dw2 ? dw2->weight : 0.0f;

      MDeformVert *dvert_final = &gps->dvert[old_tot + i - 1];
      dvert_final->totweight = 0;
      MDeformWeight *dw = BKE_defvert_ensure_index(dvert_final, 0);
      if (dvert_final->dw) {
        dw->weight = interpf(weight_2, weight_1, step);
      }
    }
  }

  /* Enable cyclic flag. */
  gps->flag |= GP_STROKE_CYCLIC;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normal Calculation
 * \{ */

void BKE_gpencil_stroke_normal(const bGPDstroke *gps, float r_normal[3])
{
  if (gps->totpoints < 3) {
    zero_v3(r_normal);
    return;
  }

  bGPDspoint *points = gps->points;
  int totpoints = gps->totpoints;

  const bGPDspoint *pt0 = &points[0];
  const bGPDspoint *pt1 = &points[1];
  const bGPDspoint *pt3 = &points[int(totpoints * 0.75)];

  float vec1[3];
  float vec2[3];

  /* initial vector (p0 -> p1) */
  sub_v3_v3v3(vec1, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  sub_v3_v3v3(vec2, &pt3->x, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(r_normal, vec1, vec2);

  /* Normalize vector */
  normalize_v3(r_normal);
}

/** \} */

void BKE_gpencil_transform(bGPdata *gpd, const float mat[4][4])
{
  if (gpd == nullptr) {
    return;
  }

  const float scalef = mat4_to_scale(mat);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          mul_m4_v3(mat, &pt->x);
          pt->pressure *= scalef;
        }

        /* Distortion may mean we need to re-triangulate. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
  }
}

int BKE_gpencil_stroke_point_count(const bGPdata *gpd)
{
  int total_points = 0;

  if (gpd == nullptr) {
    return 0;
  }

  LISTBASE_FOREACH (const bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (const bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        total_points += gps->totpoints;
      }
    }
  }
  return total_points;
}

void BKE_gpencil_point_coords_get(bGPdata *gpd, GPencilPointCoordinates *elem_data)
{
  if (gpd == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          copy_v3_v3(elem_data->co, &pt->x);
          elem_data->pressure = pt->pressure;
          elem_data++;
        }
      }
    }
  }
}

void BKE_gpencil_point_coords_apply(bGPdata *gpd, const GPencilPointCoordinates *elem_data)
{
  if (gpd == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          copy_v3_v3(&pt->x, elem_data->co);
          pt->pressure = elem_data->pressure;
          elem_data++;
        }

        /* Distortion may mean we need to re-triangulate. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
  }
}

void BKE_gpencil_point_coords_apply_with_mat4(bGPdata *gpd,
                                              const GPencilPointCoordinates *elem_data,
                                              const float mat[4][4])
{
  if (gpd == nullptr) {
    return;
  }

  const float scalef = mat4_to_scale(mat);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* FIXME: For now, we just skip parented layers.
     * Otherwise, we have to update each frame to find
     * the current parent position/effects.
     */
    if (gpl->parent) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        for (pt = gps->points, i = 0; i < gps->totpoints; pt++, i++) {
          mul_v3_m4v3(&pt->x, mat, elem_data->co);
          pt->pressure = elem_data->pressure * scalef;
          elem_data++;
        }

        /* Distortion may mean we need to re-triangulate. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
  }
}

void BKE_gpencil_stroke_set_random_color(bGPDstroke *gps)
{
  BLI_assert(gps->totpoints > 0);

  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  bGPDspoint *pt = &gps->points[0];
  color[0] *= BLI_hash_int_01(BLI_hash_int_2d(gps->totpoints / 5, pt->x + pt->z));
  color[1] *= BLI_hash_int_01(BLI_hash_int_2d(gps->totpoints + pt->x, pt->y * pt->z + pt->x));
  color[2] *= BLI_hash_int_01(BLI_hash_int_2d(gps->totpoints - pt->x, pt->z * pt->x + pt->y));
  for (int i = 0; i < gps->totpoints; i++) {
    pt = &gps->points[i];
    copy_v4_v4(pt->vert_color, color);
  }
}

void BKE_gpencil_stroke_flip(bGPDstroke *gps)
{
  /* Reverse points. */
  BLI_array_reverse(gps->points, gps->totpoints);

  /* Reverse vertex groups if available. */
  if (gps->dvert) {
    BLI_array_reverse(gps->dvert, gps->totpoints);
  }
}

/* Temp data for storing information about an "island" of points
 * that should be kept when splitting up a stroke. Used in:
 * gpencil_stroke_delete_tagged_points()
 */
struct tGPDeleteIsland {
  int start_idx;
  int end_idx;
};

static void gpencil_stroke_join_islands(bGPdata *gpd,
                                        bGPDframe *gpf,
                                        bGPDstroke *gps_first,
                                        bGPDstroke *gps_last)
{
  bGPDspoint *pt = nullptr;
  bGPDspoint *pt_final = nullptr;
  const int totpoints = gps_first->totpoints + gps_last->totpoints;

  /* create new stroke */
  bGPDstroke *join_stroke = BKE_gpencil_stroke_duplicate(gps_first, false, true);

  join_stroke->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * totpoints, __func__);
  join_stroke->totpoints = totpoints;
  join_stroke->flag &= ~GP_STROKE_CYCLIC;

  /* copy points (last before) */
  int e1 = 0;
  int e2 = 0;
  float delta = 0.0f;

  for (int i = 0; i < totpoints; i++) {
    pt_final = &join_stroke->points[i];
    if (i < gps_last->totpoints) {
      pt = &gps_last->points[e1];
      e1++;
    }
    else {
      pt = &gps_first->points[e2];
      e2++;
    }

    /* copy current point */
    copy_v3_v3(&pt_final->x, &pt->x);
    pt_final->pressure = pt->pressure;
    pt_final->strength = pt->strength;
    pt_final->time = delta;
    pt_final->flag = pt->flag;
    copy_v4_v4(pt_final->vert_color, pt->vert_color);

    /* retiming with fixed time interval (we cannot determine real time) */
    delta += 0.01f;
  }

  /* Copy over vertex weight data (if available) */
  if ((gps_first->dvert != nullptr) || (gps_last->dvert != nullptr)) {
    join_stroke->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * totpoints, __func__);
    MDeformVert *dvert_src = nullptr;
    MDeformVert *dvert_dst = nullptr;

    /* Copy weights (last before). */
    e1 = 0;
    e2 = 0;
    for (int i = 0; i < totpoints; i++) {
      dvert_dst = &join_stroke->dvert[i];
      dvert_src = nullptr;
      if (i < gps_last->totpoints) {
        if (gps_last->dvert) {
          dvert_src = &gps_last->dvert[e1];
          e1++;
        }
      }
      else {
        if (gps_first->dvert) {
          dvert_src = &gps_first->dvert[e2];
          e2++;
        }
      }

      if ((dvert_src) && (dvert_src->dw)) {
        dvert_dst->dw = (MDeformWeight *)MEM_dupallocN(dvert_src->dw);
      }
    }
  }

  /* add new stroke at head */
  BLI_addhead(&gpf->strokes, join_stroke);
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, join_stroke);

  /* remove first stroke */
  BLI_remlink(&gpf->strokes, gps_first);
  BKE_gpencil_free_stroke(gps_first);

  /* remove last stroke */
  BLI_remlink(&gpf->strokes, gps_last);
  BKE_gpencil_free_stroke(gps_last);
}

bGPDstroke *BKE_gpencil_stroke_delete_tagged_points(bGPdata *gpd,
                                                    bGPDframe *gpf,
                                                    bGPDstroke *gps,
                                                    bGPDstroke *next_stroke,
                                                    int tag_flags,
                                                    const bool select,
                                                    const bool flat_cap,
                                                    const int limit)
{
  /* The algorithm used here is as follows:
   * 1) We firstly identify the number of "islands" of non-tagged points
   *    which will all end up being in new strokes.
   *    - In the most extreme case (i.e. every other vert is a 1-vert island),
   *      we have at most `n / 2` islands
   *    - Once we start having larger islands than that, the number required
   *      becomes much less
   * 2) Each island gets converted to a new stroke
   * If the number of points is <= limit, the stroke is deleted. */

  tGPDeleteIsland *islands = (tGPDeleteIsland *)MEM_callocN(
      sizeof(tGPDeleteIsland) * (gps->totpoints + 1) / 2, "gp_point_islands");
  bool in_island = false;
  int num_islands = 0;

  bGPDstroke *new_stroke = nullptr;
  bGPDstroke *gps_first = nullptr;
  const bool is_cyclic = bool(gps->flag & GP_STROKE_CYCLIC);

  /* First Pass: Identify start/end of islands */
  bGPDspoint *pt = gps->points;
  for (int i = 0; i < gps->totpoints; i++, pt++) {
    if (pt->flag & tag_flags) {
      /* selected - stop accumulating to island */
      in_island = false;
    }
    else {
      /* unselected - start of a new island? */
      int idx;

      if (in_island) {
        /* extend existing island */
        idx = num_islands - 1;
        islands[idx].end_idx = i;
      }
      else {
        /* start of new island */
        in_island = true;
        num_islands++;

        idx = num_islands - 1;
        islands[idx].start_idx = islands[idx].end_idx = i;
      }
    }
  }

  /* Watch out for special case where No islands = All points selected = Delete Stroke only */
  if (num_islands) {
    /* There are islands, so create a series of new strokes,
     * adding them before the "next" stroke. */
    int idx;

    /* Create each new stroke... */
    for (idx = 0; idx < num_islands; idx++) {
      tGPDeleteIsland *island = &islands[idx];
      new_stroke = BKE_gpencil_stroke_duplicate(gps, false, true);
      if (flat_cap) {
        new_stroke->caps[1 - (idx % 2)] = GP_STROKE_CAP_FLAT;
      }

      /* if cyclic and first stroke, save to join later */
      if ((is_cyclic) && (gps_first == nullptr)) {
        gps_first = new_stroke;
      }

      new_stroke->flag &= ~GP_STROKE_CYCLIC;

      /* Compute new buffer size (+ 1 needed as the endpoint index is "inclusive") */
      new_stroke->totpoints = island->end_idx - island->start_idx + 1;

      /* Copy over the relevant point data */
      new_stroke->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * new_stroke->totpoints,
                                                     "gp delete stroke fragment");
      memcpy(static_cast<void *>(new_stroke->points),
             gps->points + island->start_idx,
             sizeof(bGPDspoint) * new_stroke->totpoints);

      /* Copy over vertex weight data (if available) */
      if (gps->dvert != nullptr) {
        /* Copy over the relevant vertex-weight points */
        new_stroke->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * new_stroke->totpoints,
                                                       "gp delete stroke fragment weight");
        memcpy(new_stroke->dvert,
               gps->dvert + island->start_idx,
               sizeof(MDeformVert) * new_stroke->totpoints);

        /* Copy weights */
        int e = island->start_idx;
        for (int i = 0; i < new_stroke->totpoints; i++) {
          MDeformVert *dvert_src = &gps->dvert[e];
          MDeformVert *dvert_dst = &new_stroke->dvert[i];
          if (dvert_src->dw) {
            dvert_dst->dw = (MDeformWeight *)MEM_dupallocN(dvert_src->dw);
          }
          e++;
        }
      }
      /* Each island corresponds to a new stroke.
       * We must adjust the timings of these new strokes:
       *
       * Each point's timing data is a delta from stroke's inittime, so as we erase some points
       * from the start of the stroke, we have to offset this inittime and all remaining points'
       * delta values. This way we get a new stroke with exactly the same timing as if user had
       * started drawing from the first non-removed point.
       */
      {
        bGPDspoint *pts;
        float delta = gps->points[island->start_idx].time;
        int j;

        new_stroke->inittime += double(delta);

        pts = new_stroke->points;
        for (j = 0; j < new_stroke->totpoints; j++, pts++) {
          /* Some points have time = 0, so check to not get negative time values. */
          pts->time = max_ff(pts->time - delta, 0.0f);
          /* set flag for select again later */
          if (select == true) {
            pts->flag &= ~GP_SPOINT_SELECT;
            pts->flag |= GP_SPOINT_TAG;
          }
        }
      }

      /* Add new stroke to the frame or delete if below limit */
      if ((limit > 0) && (new_stroke->totpoints <= limit)) {
        if (gps_first == new_stroke) {
          gps_first = nullptr;
        }
        BKE_gpencil_free_stroke(new_stroke);
      }
      else {
        /* Calc geometry data. */
        BKE_gpencil_stroke_geometry_update(gpd, new_stroke);

        if (next_stroke) {
          BLI_insertlinkbefore(&gpf->strokes, next_stroke, new_stroke);
        }
        else {
          BLI_addtail(&gpf->strokes, new_stroke);
        }
      }
    }
    /* if cyclic, need to join last stroke with first stroke */
    if ((is_cyclic) && (gps_first != nullptr) && (gps_first != new_stroke)) {
      gpencil_stroke_join_islands(gpd, gpf, gps_first, new_stroke);
    }
  }

  /* free islands */
  MEM_freeN(islands);

  /* Delete the old stroke */
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);

  return new_stroke;
}

/* Helper: copy point between strokes */
static void gpencil_stroke_copy_point(bGPDstroke *gps,
                                      MDeformVert *dvert,
                                      bGPDspoint *point,
                                      const float delta[3],
                                      float pressure,
                                      float strength,
                                      float deltatime)
{
  bGPDspoint *newpoint;

  gps->points = (bGPDspoint *)MEM_reallocN(gps->points, sizeof(bGPDspoint) * (gps->totpoints + 1));
  if (gps->dvert != nullptr) {
    gps->dvert = (MDeformVert *)MEM_reallocN(gps->dvert,
                                             sizeof(MDeformVert) * (gps->totpoints + 1));
  }
  else {
    /* If destination has weight add weight to origin. */
    if (dvert != nullptr) {
      gps->dvert = (MDeformVert *)MEM_callocN(sizeof(MDeformVert) * (gps->totpoints + 1),
                                              __func__);
    }
  }

  gps->totpoints++;
  newpoint = &gps->points[gps->totpoints - 1];

  newpoint->x = point->x * delta[0];
  newpoint->y = point->y * delta[1];
  newpoint->z = point->z * delta[2];
  newpoint->flag = point->flag;
  newpoint->pressure = pressure;
  newpoint->strength = strength;
  newpoint->time = point->time + deltatime;
  copy_v4_v4(newpoint->vert_color, point->vert_color);

  if (gps->dvert != nullptr) {
    MDeformVert *newdvert = &gps->dvert[gps->totpoints - 1];

    if (dvert != nullptr) {
      newdvert->totweight = dvert->totweight;
      newdvert->dw = (MDeformWeight *)MEM_dupallocN(dvert->dw);
    }
    else {
      newdvert->totweight = 0;
      newdvert->dw = nullptr;
    }
  }
}

void BKE_gpencil_stroke_join(bGPDstroke *gps_a,
                             bGPDstroke *gps_b,
                             const bool leave_gaps,
                             const bool fit_thickness,
                             const bool smooth,
                             bool auto_flip)
{
  bGPDspoint point;
  bGPDspoint *pt;
  int i;
  const float delta[3] = {1.0f, 1.0f, 1.0f};
  float deltatime = 0.0f;

  /* sanity checks */
  if (ELEM(nullptr, gps_a, gps_b)) {
    return;
  }

  if ((gps_a->totpoints == 0) || (gps_b->totpoints == 0)) {
    return;
  }

  if (auto_flip) {
    /* define start and end points of each stroke */
    float start_a[3], start_b[3], end_a[3], end_b[3];
    pt = &gps_a->points[0];
    copy_v3_v3(start_a, &pt->x);

    pt = &gps_a->points[gps_a->totpoints - 1];
    copy_v3_v3(end_a, &pt->x);

    pt = &gps_b->points[0];
    copy_v3_v3(start_b, &pt->x);

    pt = &gps_b->points[gps_b->totpoints - 1];
    copy_v3_v3(end_b, &pt->x);

    /* Check if need flip strokes. */
    float dist = len_squared_v3v3(end_a, start_b);
    bool flip_a = false;
    bool flip_b = false;
    float lowest = dist;

    dist = len_squared_v3v3(end_a, end_b);
    if (dist < lowest) {
      lowest = dist;
      flip_a = false;
      flip_b = true;
    }

    dist = len_squared_v3v3(start_a, start_b);
    if (dist < lowest) {
      lowest = dist;
      flip_a = true;
      flip_b = false;
    }

    dist = len_squared_v3v3(start_a, end_b);
    if (dist < lowest) {
      lowest = dist;
      flip_a = true;
      flip_b = true;
    }

    if (flip_a) {
      BKE_gpencil_stroke_flip(gps_a);
    }
    if (flip_b) {
      BKE_gpencil_stroke_flip(gps_b);
    }
  }

  /* don't visibly link the first and last points? */
  if (leave_gaps) {
    /* 1st: add one tail point to start invisible area */
    point = blender::dna::shallow_copy(gps_a->points[gps_a->totpoints - 1]);
    deltatime = point.time;

    gpencil_stroke_copy_point(gps_a, nullptr, &point, delta, 0.0f, 0.0f, 0.0f);

    /* 2nd: add one head point to finish invisible area */
    point = blender::dna::shallow_copy(gps_b->points[0]);
    gpencil_stroke_copy_point(gps_a, nullptr, &point, delta, 0.0f, 0.0f, deltatime);
  }

  /* Ratio to apply in the points to keep the same thickness in the joined stroke using the
   * destination stroke thickness. */
  const float ratio = (fit_thickness && gps_a->thickness > 0.0f) ?
                          float(gps_b->thickness) / float(gps_a->thickness) :
                          1.0f;

  /* 3rd: add all points */
  const int totpoints_a = gps_a->totpoints;
  for (i = 0, pt = gps_b->points; i < gps_b->totpoints && pt; i++, pt++) {
    MDeformVert *dvert = (gps_b->dvert) ? &gps_b->dvert[i] : nullptr;
    gpencil_stroke_copy_point(
        gps_a, dvert, pt, delta, pt->pressure * ratio, pt->strength, deltatime);
  }
  /* Smooth the join to avoid hard thickness changes. */
  if (smooth) {
    const int sample_points = 8;
    /* Get the segment to smooth using n points on each side of the join. */
    int start = std::max(0, totpoints_a - sample_points);
    int end = std::min(gps_a->totpoints - 1, start + (sample_points * 2));
    const int len = (end - start);
    float step = 1.0f / ((len / 2) + 1);

    /* Calc the average pressure. */
    float avg_pressure = 0.0f;
    for (i = start; i < end; i++) {
      pt = &gps_a->points[i];
      avg_pressure += pt->pressure;
    }
    avg_pressure = avg_pressure / len;

    /* Smooth segment thickness and position. */
    float ratio = step;
    for (i = start; i < end; i++) {
      pt = &gps_a->points[i];
      pt->pressure += (avg_pressure - pt->pressure) * ratio;
      BKE_gpencil_stroke_smooth_point(gps_a, i, ratio * 0.6f, 2, false, true, gps_a);

      ratio += step;
      /* In the center, reverse the ratio. */
      if (ratio > 1.0f) {
        ratio = ratio - step - step;
        step *= -1.0f;
      }
    }
  }
}

/** \} */
void BKE_gpencil_stroke_to_view_space(bGPDstroke *gps,
                                      float viewmat[4][4],
                                      const float diff_mat[4][4])
{
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    /* Point to parent space. */
    mul_v3_m4v3(&pt->x, diff_mat, &pt->x);
    /* point to view space */
    mul_m4_v3(viewmat, &pt->x);
  }
}

void BKE_gpencil_stroke_from_view_space(bGPDstroke *gps,
                                        float viewinv[4][4],
                                        const float diff_mat[4][4])
{
  float inverse_diff_mat[4][4];
  invert_m4_m4(inverse_diff_mat, diff_mat);

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    mul_v3_m4v3(&pt->x, viewinv, &pt->x);
    mul_m4_v3(inverse_diff_mat, &pt->x);
  }
}

/** \} */

float BKE_gpencil_stroke_average_pressure_get(bGPDstroke *gps)
{

  if (gps->totpoints == 1) {
    return gps->points[0].pressure;
  }

  float tot = 0.0f;
  for (int i = 0; i < gps->totpoints; i++) {
    const bGPDspoint *pt = &gps->points[i];
    tot += pt->pressure;
  }

  return tot / float(gps->totpoints);
}

bool BKE_gpencil_stroke_is_pressure_constant(bGPDstroke *gps)
{
  if (gps->totpoints == 1) {
    return true;
  }

  const float first_pressure = gps->points[0].pressure;
  for (int i = 0; i < gps->totpoints; i++) {
    const bGPDspoint *pt = &gps->points[i];
    if (pt->pressure != first_pressure) {
      return false;
    }
  }

  return true;
}

/** \} */
