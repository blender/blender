/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 *
 * Handle curve object data bevel options,
 * both extruding
 */

#include <string.h>

#include "BLI_listbase.h"
#include "BLI_math_base.h"

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_displist.h"

typedef enum CurveBevelFillType {
  BACK = 0,
  FRONT,
  HALF,
  FULL,
} CurveBevelFillType;

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

static void curve_bevel_make_extrude_and_fill(Curve *cu,
                                              ListBase *disp,
                                              const bool use_extrude,
                                              const CurveBevelFillType fill_type)
{
  DispList *dl = MEM_callocN(sizeof(DispList), __func__);

  int nr;
  if (fill_type == FULL) {
    /* The full loop. */
    nr = 4 * cu->bevresol + 6;
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

  dl->verts = MEM_malloc_arrayN(nr, sizeof(float[3]), __func__);
  BLI_addtail(disp, dl);
  /* Use a different type depending on whether the loop is complete or not. */
  dl->type = (fill_type == FULL) ? DL_POLY : DL_SEGM;
  dl->parts = 1;
  dl->nr = nr;

  float *fp = dl->verts;
  const float dangle = (float)M_PI_2 / (cu->bevresol + 1);
  float angle = 0.0f;

  /* Build the back section. */
  if (ELEM(fill_type, BACK, HALF, FULL)) {
    angle = (float)M_PI_2 * 3.0f;
    for (int i = 0; i < cu->bevresol + 2; i++) {
      fp[0] = 0.0f;
      fp[1] = (float)(cosf(angle) * (cu->ext2));
      fp[2] = (float)(sinf(angle) * (cu->ext2)) - cu->ext1;
      angle += dangle;
      fp += 3;
    }
    if (use_extrude && fill_type == BACK) {
      /* Add the extrusion if we're only building the back. */
      fp[0] = 0.0f;
      fp[1] = cu->ext2;
      fp[2] = cu->ext1;
    }
  }

  /* Build the front section. */
  if (ELEM(fill_type, FRONT, HALF, FULL)) {
    if (use_extrude && fill_type == FRONT) {
      /* Add the extrusion if we're only building the front. */
      fp[0] = 0.0f;
      fp[1] = cu->ext2;
      fp[2] = -cu->ext1;
      fp += 3;
    }
    /* Don't duplicate the last back vertex. */
    angle = (!use_extrude && ELEM(fill_type, HALF, FULL)) ? dangle : 0;
    int front_len = (!use_extrude && ELEM(fill_type, HALF, FULL)) ? cu->bevresol + 1 :
                                                                    cu->bevresol + 2;
    for (int i = 0; i < front_len; i++) {
      fp[0] = 0.0f;
      fp[1] = (float)(cosf(angle) * (cu->ext2));
      fp[2] = (float)(sinf(angle) * (cu->ext2)) + cu->ext1;
      angle += dangle;
      fp += 3;
    }
  }

  /* Build the other half only if we're building the full loop. */
  if (fill_type == FULL) {
    for (int i = 0; i < cu->bevresol + 1; i++) {
      fp[0] = 0.0f;
      fp[1] = (float)(cosf(angle) * (cu->ext2));
      fp[2] = (float)(sinf(angle) * (cu->ext2)) + cu->ext1;
      angle += dangle;
      fp += 3;
    }

    angle = (float)M_PI;
    for (int i = 0; i < cu->bevresol + 1; i++) {
      fp[0] = 0.0f;
      fp[1] = (float)(cosf(angle) * (cu->ext2));
      fp[2] = (float)(sinf(angle) * (cu->ext2)) - cu->ext1;
      angle += dangle;
      fp += 3;
    }
  }
}

static void curve_bevel_make_full_circle(Curve *cu, ListBase *disp)
{
  const int nr = 4 + 2 * cu->bevresol;

  DispList *dl = MEM_callocN(sizeof(DispList), __func__);
  dl->verts = MEM_malloc_arrayN(nr, sizeof(float[3]), __func__);
  BLI_addtail(disp, dl);
  dl->type = DL_POLY;
  dl->parts = 1;
  dl->flag = DL_BACK_CURVE;
  dl->nr = nr;

  float *fp = dl->verts;
  const float dangle = (2.0f * (float)M_PI / (nr));
  float angle = -(nr - 1) * dangle;

  for (int i = 0; i < nr; i++) {
    fp[0] = 0.0;
    fp[1] = (cosf(angle) * (cu->ext2));
    fp[2] = (sinf(angle) * (cu->ext2)) - cu->ext1;
    angle += dangle;
    fp += 3;
  }
}

static void curve_bevel_make_only_extrude(Curve *cu, ListBase *disp)
{
  DispList *dl = MEM_callocN(sizeof(DispList), __func__);
  dl->verts = MEM_malloc_arrayN(2, sizeof(float[3]), __func__);
  BLI_addtail(disp, dl);
  dl->type = DL_SEGM;
  dl->parts = 1;
  dl->flag = DL_FRONT_CURVE | DL_BACK_CURVE;
  dl->nr = 2;

  float *fp = dl->verts;
  fp[0] = fp[1] = 0.0;
  fp[2] = -cu->ext1;
  fp[3] = fp[4] = 0.0;
  fp[5] = cu->ext1;
}

static void curve_bevel_make_from_object(Curve *cu, ListBase *disp)
{
  if (cu->bevobj->type != OB_CURVE) {
    return;
  }

  Curve *bevcu = cu->bevobj->data;
  if (bevcu->ext1 == 0.0f && bevcu->ext2 == 0.0f) {
    ListBase bevdisp = {NULL, NULL};
    float facx = cu->bevobj->scale[0];
    float facy = cu->bevobj->scale[1];

    DispList *dl;
    if (cu->bevobj->runtime.curve_cache) {
      dl = cu->bevobj->runtime.curve_cache->disp.first;
    }
    else {
      BLI_assert(cu->bevobj->runtime.curve_cache != NULL);
      dl = NULL;
    }

    while (dl) {
      if (ELEM(dl->type, DL_POLY, DL_SEGM)) {
        DispList *dlnew = MEM_mallocN(sizeof(DispList), __func__);
        *dlnew = *dl;
        dlnew->verts = MEM_malloc_arrayN(dl->parts * dl->nr, sizeof(float[3]), __func__);
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

void BKE_curve_bevel_make(Object *ob, ListBase *disp)
{
  Curve *curve = ob->data;

  const bool use_extrude = curve->ext1 != 0.0f;
  const bool use_bevel = curve->ext2 != 0.0f;

  BLI_listbase_clear(disp);

  if (curve->bevobj) {
    curve_bevel_make_from_object(curve, disp);
  }
  else if (!(use_extrude || use_bevel)) {
    /* Pass. */
  }
  else if (use_extrude && !use_bevel) {
    curve_bevel_make_only_extrude(curve, disp);
  }
  else {
    CurveBevelFillType fill_type = curve_bevel_get_fill_type(curve);

    if (!use_extrude && fill_type == FULL) {
      curve_bevel_make_full_circle(curve, disp);
    }
    else {
      /* The general case for nonzero extrusion or an incomplete loop. */
      curve_bevel_make_extrude_and_fill(curve, disp, use_extrude, fill_type);
    }
  }
}
