/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Handle curve object data bevel options,
 * both extruding
 */

#include <cstring>

#include "BLI_alloca.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_curveprofile.h"
#include "BKE_displist.h"

enum CurveBevelFillType {
  BACK = 0,
  FRONT,
  HALF,
  FULL,
};

static CurveBevelFillType curve_bevel_get_fill_type(const Curve *curve)
{
  if (!(curve->flag & (CU_FRONT | CU_BACK))) {
    return FULL;
  }
  if ((curve->flag & CU_FRONT) && (curve->flag & CU_BACK)) {
    return HALF;
  }

  return (curve->flag & CU_FRONT) ? FRONT : BACK;
}

static void bevel_quarter_fill(const Curve *curve,
                               float *quarter_coords_x,
                               float *quarter_coords_y)
{
  if (curve->bevel_mode == CU_BEV_MODE_ROUND) {
    float angle = 0.0f;
    const float dangle = float(M_PI_2) / (curve->bevresol + 1);
    for (int i = 0; i < curve->bevresol + 1; i++) {
      quarter_coords_x[i] = float(cosf(angle) * (curve->bevel_radius));
      quarter_coords_y[i] = float(sinf(angle) * (curve->bevel_radius));
      angle += dangle;
    }
  }
  else {
    /* The curve profile evaluation should be done when the resolution is set. */
    BLI_assert(curve->bevel_profile->segments != nullptr);
    BLI_assert(curve->bevel_profile->segments_len == curve->bevresol + 1);

    /* If there aren't enough samples, the curveprofile won't
     * sample the start vertex, so set it manually instead. */
    quarter_coords_x[0] = curve->bevel_radius;
    quarter_coords_y[0] = 0.0f;
    for (int i = 1; i < curve->bevresol + 1; i++) {
      quarter_coords_x[i] = float(curve->bevel_profile->segments[i].x * (curve->bevel_radius));
      quarter_coords_y[i] = float(curve->bevel_profile->segments[i].y * (curve->bevel_radius));
    }
  }
}

static void curve_bevel_make_extrude_and_fill(const Curve *cu,
                                              ListBase *disp,
                                              const bool use_extrude,
                                              const CurveBevelFillType fill_type)
{
  DispList *dl = static_cast<DispList *>(MEM_callocN(sizeof(DispList), __func__));

  /* Calculate the profile of the bevel once to reuse it for each quarter. We will need
   * to flip around the indices for every other section in order to build around the circle
   * in a consistent direction.
   *
   * These should be small enough for stack allocations because the current limit
   * for #Curve.bevresol is 32. */
  float *quarter_coords_x = static_cast<float *>(alloca(sizeof(float) * (cu->bevresol + 1)));
  float *quarter_coords_y = static_cast<float *>(alloca(sizeof(float) * (cu->bevresol + 1)));
  bevel_quarter_fill(cu, quarter_coords_x, quarter_coords_y);

  int nr;
  if (fill_type == FULL) {
    /* The full loop. */
    nr = 4 * cu->bevresol + (use_extrude ? 6 : 4);
    dl->flag = DL_FRONT_CURVE | DL_BACK_CURVE;
  }
  else if (fill_type == HALF) {
    /* Half the loop. */
    nr = 2 * (cu->bevresol + 1) + (use_extrude ? 2 : 1);
    dl->flag = DL_FRONT_CURVE | DL_BACK_CURVE;
  }
  else {
    /* One quarter of the loop (just front or back). */
    nr = use_extrude ? cu->bevresol + 3 : cu->bevresol + 2;
    dl->flag = (fill_type == FRONT) ? DL_FRONT_CURVE : DL_BACK_CURVE;
  }

  dl->verts = static_cast<float *>(MEM_malloc_arrayN(nr, sizeof(float[3]), __func__));
  BLI_addtail(disp, dl);
  /* Use a different type depending on whether the loop is complete or not. */
  dl->type = (fill_type == FULL) ? DL_POLY : DL_SEGM;
  dl->parts = 1;
  dl->nr = nr;

  float *fp = dl->verts;

  /* Build the back section. */
  if (ELEM(fill_type, BACK, HALF, FULL)) {
    /* Add the bottom vertex. */
    fp[0] = 0.0f;
    fp[1] = 0.0f;
    fp[2] = -cu->extrude - cu->bevel_radius;
    fp += 3;

    for (int i = cu->bevresol; i >= 0; i--) {
      fp[0] = 0.0f;
      fp[1] = quarter_coords_x[i];
      fp[2] = -quarter_coords_y[i] - cu->extrude;
      fp += 3;
    }
  }

  /* Add the extrusion if we're only building either the back or the front. */
  if (use_extrude && ELEM(fill_type, FRONT, BACK)) {
    fp[0] = 0.0f;
    fp[1] = cu->bevel_radius;
    fp[2] = (fill_type == FRONT) ? -cu->extrude : cu->extrude;
    fp += 3;
  }

  /* Build the front section. */
  if (ELEM(fill_type, FRONT, HALF, FULL)) {
    /* Don't duplicate the last back vertex. */
    const int front_start = (!use_extrude && ELEM(fill_type, HALF, FULL)) ? 1 : 0;
    for (int i = front_start; i < cu->bevresol + 1; i++) {
      fp[0] = 0.0f;
      fp[1] = quarter_coords_x[i];
      fp[2] = quarter_coords_y[i] + cu->extrude;
      fp += 3;
    }
    /* Add the top vertex. */
    fp[0] = 0.0f;
    fp[1] = 0.0f;
    fp[2] = cu->extrude + cu->bevel_radius;
    fp += 3;
  }

  /* Build the other half only if we're building the full loop. */
  if (fill_type == FULL) {
    for (int i = cu->bevresol; i > 0; i--) {
      fp[0] = 0.0f;
      fp[1] = -quarter_coords_x[i];
      fp[2] = quarter_coords_y[i] + cu->extrude;
      fp += 3;
    }

    if (use_extrude) {
      /* Add the extrusion. */
      fp[0] = 0.0f;
      fp[1] = -cu->bevel_radius;
      fp[2] = cu->extrude;
      fp += 3;
    }

    for (int i = 0; i < cu->bevresol + 1; i++) {
      fp[0] = 0.0f;
      fp[1] = -quarter_coords_x[i];
      fp[2] = -quarter_coords_y[i] - cu->extrude;
      fp += 3;
    }
  }
}

static void curve_bevel_make_full_circle(const Curve *cu, ListBase *disp)
{
  const int nr = 4 + 2 * cu->bevresol;

  DispList *dl = static_cast<DispList *>(MEM_callocN(sizeof(DispList), __func__));
  dl->verts = static_cast<float *>(MEM_malloc_arrayN(nr, sizeof(float[3]), __func__));
  BLI_addtail(disp, dl);
  dl->type = DL_POLY;
  dl->parts = 1;
  dl->flag = DL_BACK_CURVE;
  dl->nr = nr;

  float *fp = dl->verts;
  const float dangle = (2.0f * float(M_PI) / (nr));
  float angle = -(nr - 1) * dangle;

  for (int i = 0; i < nr; i++) {
    fp[0] = 0.0;
    fp[1] = (cosf(angle) * (cu->bevel_radius));
    fp[2] = (sinf(angle) * (cu->bevel_radius)) - cu->extrude;
    angle += dangle;
    fp += 3;
  }
}

static void curve_bevel_make_only_extrude(const Curve *cu, ListBase *disp)
{
  DispList *dl = static_cast<DispList *>(MEM_callocN(sizeof(DispList), __func__));
  dl->verts = static_cast<float *>(MEM_malloc_arrayN(2, sizeof(float[3]), __func__));
  BLI_addtail(disp, dl);
  dl->type = DL_SEGM;
  dl->parts = 1;
  dl->flag = DL_FRONT_CURVE | DL_BACK_CURVE;
  dl->nr = 2;

  float *fp = dl->verts;
  fp[0] = fp[1] = 0.0;
  fp[2] = -cu->extrude;
  fp[3] = fp[4] = 0.0;
  fp[5] = cu->extrude;
}

static void curve_bevel_make_from_object(const Curve *cu, ListBase *disp)
{
  if (cu->bevobj == nullptr) {
    return;
  }
  if (cu->bevobj->type != OB_CURVES_LEGACY) {
    return;
  }

  Curve *bevcu = static_cast<Curve *>(cu->bevobj->data);
  if (bevcu->extrude == 0.0f && bevcu->bevel_radius == 0.0f) {
    ListBase bevdisp = {nullptr, nullptr};
    float facx = cu->bevobj->scale[0];
    float facy = cu->bevobj->scale[1];

    DispList *dl;
    if (cu->bevobj->runtime.curve_cache) {
      dl = static_cast<DispList *>(cu->bevobj->runtime.curve_cache->disp.first);
    }
    else {
      BLI_assert(cu->bevobj->runtime.curve_cache != nullptr);
      dl = nullptr;
    }

    while (dl) {
      if (ELEM(dl->type, DL_POLY, DL_SEGM)) {
        DispList *dlnew = static_cast<DispList *>(MEM_mallocN(sizeof(DispList), __func__));
        *dlnew = *dl;
        dlnew->verts = static_cast<float *>(
            MEM_malloc_arrayN(dl->parts * dl->nr, sizeof(float[3]), __func__));
        memcpy(dlnew->verts, dl->verts, sizeof(float[3]) * dl->parts * dl->nr);

        if (dlnew->type == DL_SEGM) {
          dlnew->flag |= (DL_FRONT_CURVE | DL_BACK_CURVE);
        }

        BLI_addtail(disp, dlnew);
        float *fp = dlnew->verts;
        int nr = dlnew->parts * dlnew->nr;
        while (nr--) {
          fp[2] = fp[1] * facy;
          fp[1] = -fp[0] * facx;
          fp[0] = 0.0;
          fp += 3;
        }
      }
      dl = dl->next;
    }

    BKE_displist_free(&bevdisp);
  }
}

ListBase BKE_curve_bevel_make(const Curve *curve)
{
  ListBase bevel_shape = {nullptr, nullptr};

  if (curve->bevel_mode == CU_BEV_MODE_OBJECT) {
    if (curve->bevobj != nullptr) {
      curve_bevel_make_from_object(curve, &bevel_shape);
    }
  }
  else {
    const bool use_extrude = curve->extrude != 0.0f;
    const bool use_bevel = curve->bevel_radius != 0.0f;
    /* Pass. */
    if (use_extrude && !use_bevel) {
      curve_bevel_make_only_extrude(curve, &bevel_shape);
    }
    else if (use_extrude || use_bevel) {
      CurveBevelFillType fill_type = curve_bevel_get_fill_type(curve);

      if (!use_extrude && fill_type == FULL && curve->bevel_mode == CU_BEV_MODE_ROUND) {
        curve_bevel_make_full_circle(curve, &bevel_shape);
      }
      else {
        /* The general case for nonzero extrusion or an incomplete loop. */
        curve_bevel_make_extrude_and_fill(curve, &bevel_shape, use_extrude, fill_type);
      }
    }
  }

  return bevel_shape;
}
