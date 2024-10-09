/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.hh"
#include "BKE_curve.hh"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_material.h"

extern "C" {
#include "curve_fit_nd.h"
}

#include "DEG_depsgraph_query.hh"

#define COORD_FITTING_INFLUENCE 20.0f

/* -------------------------------------------------------------------- */
/** \name Convert to curve object
 * \{ */

static void gpencil_editstroke_deselect_all(bGPDcurve *gpc)
{
  for (int i = 0; i < gpc->tot_curve_points; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    BezTriple *bezt = &gpc_pt->bezt;
    gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
    BEZT_DESEL_ALL(bezt);
  }
  gpc->flag &= ~GP_CURVE_SELECT;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Curve Kernel Functions
 * \{ */

static bGPDcurve *gpencil_stroke_editcurve_generate_edgecases(bGPDstroke *gps,
                                                              const float stroke_radius)
{
  BLI_assert(gps->totpoints < 3);

  if (gps->totpoints == 1) {
    bGPDcurve *editcurve = BKE_gpencil_stroke_editcurve_new(1);
    bGPDspoint *pt = &gps->points[0];
    bGPDcurve_point *cpt = &editcurve->curve_points[0];
    BezTriple *bezt = &cpt->bezt;

    /* Handles are twice as long as the radius of the point. */
    float offset = (pt->pressure * stroke_radius) * 2.0f;

    float tmp_vec[3];
    for (int j = 0; j < 3; j++) {
      copy_v3_v3(tmp_vec, &pt->x);
      /* Move handles along the x-axis away from the control point */
      tmp_vec[0] += float(j - 1) * offset;
      copy_v3_v3(bezt->vec[j], tmp_vec);
    }

    cpt->pressure = pt->pressure;
    cpt->strength = pt->strength;
    copy_v4_v4(cpt->vert_color, pt->vert_color);

    /* default handle type */
    bezt->h1 = HD_FREE;
    bezt->h2 = HD_FREE;

    cpt->point_index = 0;

    return editcurve;
  }
  if (gps->totpoints == 2) {
    bGPDcurve *editcurve = BKE_gpencil_stroke_editcurve_new(2);
    bGPDspoint *first_pt = &gps->points[0];
    bGPDspoint *last_pt = &gps->points[1];

    float length = len_v3v3(&first_pt->x, &last_pt->x);
    float offset = length / 3;
    float dir[3];
    sub_v3_v3v3(dir, &last_pt->x, &first_pt->x);

    for (int i = 0; i < 2; i++) {
      bGPDspoint *pt = &gps->points[i];
      bGPDcurve_point *cpt = &editcurve->curve_points[i];
      BezTriple *bezt = &cpt->bezt;

      float tmp_vec[3];
      for (int j = 0; j < 3; j++) {
        copy_v3_v3(tmp_vec, dir);
        normalize_v3_length(tmp_vec, float(j - 1) * offset);
        add_v3_v3v3(bezt->vec[j], &pt->x, tmp_vec);
      }

      cpt->pressure = pt->pressure;
      cpt->strength = pt->strength;
      copy_v4_v4(cpt->vert_color, pt->vert_color);

      /* default handle type */
      bezt->h1 = HD_VECT;
      bezt->h2 = HD_VECT;

      cpt->point_index = 0;
    }

    return editcurve;
  }

  return nullptr;
}

bGPDcurve *BKE_gpencil_stroke_editcurve_generate(bGPDstroke *gps,
                                                 const float error_threshold,
                                                 const float corner_angle,
                                                 const float stroke_radius)
{
  if (gps->totpoints < 3) {
    return gpencil_stroke_editcurve_generate_edgecases(gps, stroke_radius);
  }
#define POINT_DIM 9

  float *points = static_cast<float *>(
      MEM_callocN(sizeof(float) * gps->totpoints * POINT_DIM, __func__));
  float diag_length = len_v3v3(gps->boundbox_min, gps->boundbox_max);
  float tmp_vec[3];

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    int row = i * POINT_DIM;

    /* normalize coordinate to 0..1 */
    sub_v3_v3v3(tmp_vec, &pt->x, gps->boundbox_min);
    mul_v3_v3fl(&points[row], tmp_vec, COORD_FITTING_INFLUENCE / diag_length);
    points[row + 3] = pt->pressure / diag_length;

    /* strength and color are already normalized */
    points[row + 4] = pt->strength / diag_length;
    mul_v4_v4fl(&points[row + 5], pt->vert_color, 1.0f / diag_length);
  }

  uint calc_flag = CURVE_FIT_CALC_HIGH_QUALIY;
  if (gps->totpoints > 2 && gps->flag & GP_STROKE_CYCLIC) {
    calc_flag |= CURVE_FIT_CALC_CYCLIC;
  }

  float *r_cubic_array = nullptr;
  uint r_cubic_array_len = 0;
  uint *r_cubic_orig_index = nullptr;
  uint *r_corners_index_array = nullptr;
  uint r_corners_index_len = 0;
  int r = curve_fit_cubic_to_points_refit_fl(points,
                                             gps->totpoints,
                                             POINT_DIM,
                                             error_threshold,
                                             calc_flag,
                                             nullptr,
                                             0,
                                             corner_angle,
                                             &r_cubic_array,
                                             &r_cubic_array_len,
                                             &r_cubic_orig_index,
                                             &r_corners_index_array,
                                             &r_corners_index_len);

  if (r != 0 || r_cubic_array_len < 1) {
    return nullptr;
  }

  uint curve_point_size = 3 * POINT_DIM;

  bGPDcurve *editcurve = BKE_gpencil_stroke_editcurve_new(r_cubic_array_len);

  for (int i = 0; i < r_cubic_array_len; i++) {
    bGPDcurve_point *cpt = &editcurve->curve_points[i];
    BezTriple *bezt = &cpt->bezt;
    float *curve_point = &r_cubic_array[i * curve_point_size];

    for (int j = 0; j < 3; j++) {
      float *bez = &curve_point[j * POINT_DIM];
      madd_v3_v3v3fl(bezt->vec[j], gps->boundbox_min, bez, diag_length / COORD_FITTING_INFLUENCE);
    }

    float *ctrl_point = &curve_point[1 * POINT_DIM];
    cpt->pressure = ctrl_point[3] * diag_length;
    cpt->strength = ctrl_point[4] * diag_length;
    mul_v4_v4fl(cpt->vert_color, &ctrl_point[5], diag_length);

    /* default handle type */
    bezt->h1 = HD_ALIGN;
    bezt->h2 = HD_ALIGN;

    cpt->point_index = r_cubic_orig_index[i];
  }

  if (r_corners_index_len > 0 && r_corners_index_array != nullptr) {
    int start = 0, end = r_corners_index_len;
    if ((r_corners_index_len > 1) && (calc_flag & CURVE_FIT_CALC_CYCLIC) == 0) {
      start = 1;
      end = r_corners_index_len - 1;
    }
    for (int i = start; i < end; i++) {
      bGPDcurve_point *cpt = &editcurve->curve_points[r_corners_index_array[i]];
      BezTriple *bezt = &cpt->bezt;
      bezt->h1 = HD_FREE;
      bezt->h2 = HD_FREE;
    }
  }

  MEM_freeN(points);
  if (r_cubic_array) {
    free(r_cubic_array);
  }
  if (r_corners_index_array) {
    free(r_corners_index_array);
  }
  if (r_cubic_orig_index) {
    free(r_cubic_orig_index);
  }

#undef POINT_DIM
  return editcurve;
}

void BKE_gpencil_editcurve_stroke_sync_selection(bGPdata * /*gpd*/,
                                                 bGPDstroke *gps,
                                                 bGPDcurve *gpc)
{
  if (gps->flag & GP_STROKE_SELECT) {
    gpc->flag |= GP_CURVE_SELECT;

    for (int i = 0; i < gpc->tot_curve_points; i++) {
      bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
      bGPDspoint *pt = &gps->points[gpc_pt->point_index];
      if (pt->flag & GP_SPOINT_SELECT) {
        gpc_pt->flag |= GP_CURVE_POINT_SELECT;
        BEZT_SEL_ALL(&gpc_pt->bezt);
      }
      else {
        gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
        BEZT_DESEL_ALL(&gpc_pt->bezt);
      }
    }
  }
  else {
    gpc->flag &= ~GP_CURVE_SELECT;
    gpencil_editstroke_deselect_all(gpc);
  }
}

void BKE_gpencil_editcurve_recalculate_handles(bGPDstroke *gps)
{
  if (gps == nullptr || gps->editcurve == nullptr) {
    return;
  }

  bool changed = false;
  bGPDcurve *gpc = gps->editcurve;
  if (gpc->tot_curve_points < 2) {
    return;
  }

  if (gpc->tot_curve_points == 1) {
    BKE_nurb_handle_calc(
        &(gpc->curve_points[0].bezt), nullptr, &(gpc->curve_points[0].bezt), false, 0);
    gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
  }

  for (int i = 1; i < gpc->tot_curve_points - 1; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    bGPDcurve_point *gpc_pt_prev = &gpc->curve_points[i - 1];
    bGPDcurve_point *gpc_pt_next = &gpc->curve_points[i + 1];
    /* update handle if point or neighbor is selected */
    if (gpc_pt->flag & GP_CURVE_POINT_SELECT || gpc_pt_prev->flag & GP_CURVE_POINT_SELECT ||
        gpc_pt_next->flag & GP_CURVE_POINT_SELECT)
    {
      BezTriple *bezt = &gpc_pt->bezt;
      BezTriple *bezt_prev = &gpc_pt_prev->bezt;
      BezTriple *bezt_next = &gpc_pt_next->bezt;

      BKE_nurb_handle_calc(bezt, bezt_prev, bezt_next, false, 0);
      changed = true;
    }
  }

  bGPDcurve_point *gpc_first = &gpc->curve_points[0];
  bGPDcurve_point *gpc_last = &gpc->curve_points[gpc->tot_curve_points - 1];
  bGPDcurve_point *gpc_first_next = &gpc->curve_points[1];
  bGPDcurve_point *gpc_last_prev = &gpc->curve_points[gpc->tot_curve_points - 2];
  if (gps->flag & GP_STROKE_CYCLIC) {
    if (gpc_first->flag & GP_CURVE_POINT_SELECT || gpc_last->flag & GP_CURVE_POINT_SELECT) {
      BezTriple *bezt_first = &gpc_first->bezt;
      BezTriple *bezt_last = &gpc_last->bezt;
      BezTriple *bezt_first_next = &gpc_first_next->bezt;
      BezTriple *bezt_last_prev = &gpc_last_prev->bezt;

      BKE_nurb_handle_calc(bezt_first, bezt_last, bezt_first_next, false, 0);
      BKE_nurb_handle_calc(bezt_last, bezt_last_prev, bezt_first, false, 0);
      changed = true;
    }
  }
  else {
    if (gpc_first->flag & GP_CURVE_POINT_SELECT || gpc_last->flag & GP_CURVE_POINT_SELECT) {
      BezTriple *bezt_first = &gpc_first->bezt;
      BezTriple *bezt_last = &gpc_last->bezt;
      BezTriple *bezt_first_next = &gpc_first_next->bezt;
      BezTriple *bezt_last_prev = &gpc_last_prev->bezt;

      BKE_nurb_handle_calc(bezt_first, nullptr, bezt_first_next, false, 0);
      BKE_nurb_handle_calc(bezt_last, bezt_last_prev, nullptr, false, 0);
      changed = true;
    }
  }

  if (changed) {
    gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
  }
}

/* Helper: count how many new curve points must be generated. */
static int gpencil_editcurve_subdivide_count(bGPDcurve *gpc, bool is_cyclic)
{
  int count = 0;
  for (int i = 0; i < gpc->tot_curve_points - 1; i++) {
    bGPDcurve_point *cpt = &gpc->curve_points[i];
    bGPDcurve_point *cpt_next = &gpc->curve_points[i + 1];

    if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
      count++;
    }
  }

  if (is_cyclic) {
    bGPDcurve_point *cpt = &gpc->curve_points[0];
    bGPDcurve_point *cpt_next = &gpc->curve_points[gpc->tot_curve_points - 1];

    if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
      count++;
    }
  }

  return count;
}

static void gpencil_editcurve_subdivide_curve_segment(bGPDcurve_point *cpt_start,
                                                      bGPDcurve_point *cpt_end,
                                                      bGPDcurve_point *cpt_new)
{
  BezTriple *bezt_start = &cpt_start->bezt;
  BezTriple *bezt_end = &cpt_end->bezt;
  BezTriple *bezt_new = &cpt_new->bezt;
  for (int axis = 0; axis < 3; axis++) {
    float p0, p1, p2, p3, m0, m1, q0, q1, b;
    p0 = bezt_start->vec[1][axis];
    p1 = bezt_start->vec[2][axis];
    p2 = bezt_end->vec[0][axis];
    p3 = bezt_end->vec[1][axis];

    m0 = (p0 + p1) / 2;
    q0 = (p0 + 2 * p1 + p2) / 4;
    b = (p0 + 3 * p1 + 3 * p2 + p3) / 8;
    q1 = (p1 + 2 * p2 + p3) / 4;
    m1 = (p2 + p3) / 2;

    bezt_new->vec[0][axis] = q0;
    bezt_new->vec[2][axis] = q1;
    bezt_new->vec[1][axis] = b;

    bezt_start->vec[2][axis] = m0;
    bezt_end->vec[0][axis] = m1;
  }

  cpt_new->pressure = interpf(cpt_end->pressure, cpt_start->pressure, 0.5f);
  cpt_new->strength = interpf(cpt_end->strength, cpt_start->strength, 0.5f);
  interp_v4_v4v4(cpt_new->vert_color, cpt_start->vert_color, cpt_end->vert_color, 0.5f);
}

void BKE_gpencil_editcurve_subdivide(bGPDstroke *gps, const int cuts)
{
  bGPDcurve *gpc = gps->editcurve;
  if (gpc == nullptr || gpc->tot_curve_points < 2) {
    return;
  }
  bool is_cyclic = gps->flag & GP_STROKE_CYCLIC;

  /* repeat for number of cuts */
  for (int s = 0; s < cuts; s++) {
    int old_tot_curve_points = gpc->tot_curve_points;
    int new_num_curve_points = gpencil_editcurve_subdivide_count(gpc, is_cyclic);
    if (new_num_curve_points == 0) {
      break;
    }
    int new_tot_curve_points = old_tot_curve_points + new_num_curve_points;

    bGPDcurve_point *temp_curve_points = (bGPDcurve_point *)MEM_callocN(
        sizeof(bGPDcurve_point) * new_tot_curve_points, __func__);

    bool prev_subdivided = false;
    int j = 0;
    for (int i = 0; i < old_tot_curve_points - 1; i++, j++) {
      bGPDcurve_point *cpt = &gpc->curve_points[i];
      bGPDcurve_point *cpt_next = &gpc->curve_points[i + 1];

      if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
        bGPDcurve_point *cpt_new = &temp_curve_points[j + 1];
        gpencil_editcurve_subdivide_curve_segment(cpt, cpt_next, cpt_new);

        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
        memcpy(&temp_curve_points[j + 2], cpt_next, sizeof(bGPDcurve_point));

        cpt_new->flag |= GP_CURVE_POINT_SELECT;
        cpt_new->bezt.h1 = HD_ALIGN;
        cpt_new->bezt.h2 = HD_ALIGN;
        BEZT_SEL_ALL(&cpt_new->bezt);

        prev_subdivided = true;
        j++;
      }
      else if (!prev_subdivided) {
        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
        prev_subdivided = false;
      }
      else {
        prev_subdivided = false;
      }
    }

    if (is_cyclic) {
      bGPDcurve_point *cpt = &gpc->curve_points[old_tot_curve_points - 1];
      bGPDcurve_point *cpt_next = &gpc->curve_points[0];

      if (cpt->flag & GP_CURVE_POINT_SELECT && cpt_next->flag & GP_CURVE_POINT_SELECT) {
        bGPDcurve_point *cpt_new = &temp_curve_points[j + 1];
        gpencil_editcurve_subdivide_curve_segment(cpt, cpt_next, cpt_new);

        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
        memcpy(&temp_curve_points[0], cpt_next, sizeof(bGPDcurve_point));

        cpt_new->flag |= GP_CURVE_POINT_SELECT;
        cpt_new->bezt.h1 = HD_ALIGN;
        cpt_new->bezt.h2 = HD_ALIGN;
        BEZT_SEL_ALL(&cpt_new->bezt);
      }
      else if (!prev_subdivided) {
        memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
      }
    }
    else {
      bGPDcurve_point *cpt = &gpc->curve_points[old_tot_curve_points - 1];
      memcpy(&temp_curve_points[j], cpt, sizeof(bGPDcurve_point));
    }

    MEM_freeN(gpc->curve_points);
    gpc->curve_points = temp_curve_points;
    gpc->tot_curve_points = new_tot_curve_points;
  }
}

/** \} */
