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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edcurve
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_layer.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_curve.h"
#include "ED_view3d.h"

#include "curve_intern.h"

/* -------------------------------------------------------------------- */
/** \name Cursor Picking API
 * \{ */

static void ED_curve_pick_vert__do_closest(void *userData,
                                           Nurb *nu,
                                           BPoint *bp,
                                           BezTriple *bezt,
                                           int beztindex,
                                           bool handles_visible,
                                           const float screen_co[2])
{
  struct {
    BPoint *bp;
    BezTriple *bezt;
    Nurb *nurb;
    float dist;
    int hpoint, select;
    float mval_fl[2];
    bool is_changed;
  } *data = userData;

  uint8_t flag;
  float dist_test;

  if (bp) {
    flag = bp->f1;
  }
  else {
    BLI_assert(handles_visible || beztindex == 1);

    if (beztindex == 0) {
      flag = bezt->f1;
    }
    else if (beztindex == 1) {
      flag = bezt->f2;
    }
    else {
      flag = bezt->f3;
    }
  }

  dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);
  if ((flag & SELECT) == data->select) {
    dist_test += 5.0f;
  }
  if (bezt && beztindex == 1) {
    dist_test += 3.0f; /* middle points get a small disadvantage */
  }

  if (dist_test < data->dist) {
    data->dist = dist_test;

    data->bp = bp;
    data->bezt = bezt;
    data->nurb = nu;
    data->hpoint = bezt ? beztindex : 0;
    data->is_changed = true;
  }

  UNUSED_VARS_NDEBUG(handles_visible);
}

bool ED_curve_pick_vert(ViewContext *vc,
                        short sel,
                        Nurb **r_nurb,
                        BezTriple **r_bezt,
                        BPoint **r_bp,
                        short *r_handle,
                        Base **r_base)
{
  /* (sel == 1): selected gets a disadvantage */
  /* in nurb and bezt or bp the nearest is written */
  /* return 0 1 2: handlepunt */
  struct {
    BPoint *bp;
    BezTriple *bezt;
    Nurb *nurb;
    float dist;
    int hpoint, select;
    float mval_fl[2];
    bool is_changed;
  } data = {NULL};

  data.dist = ED_view3d_select_dist_px();
  data.hpoint = 0;
  data.select = sel;
  data.mval_fl[0] = vc->mval[0];
  data.mval_fl[1] = vc->mval[1];

  uint bases_len;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc->view_layer, vc->v3d, &bases_len);
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Base *base = bases[base_index];
    data.is_changed = false;

    ED_view3d_viewcontext_init_object(vc, base->object);
    ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
    nurbs_foreachScreenVert(vc, ED_curve_pick_vert__do_closest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

    if (r_base && data.is_changed) {
      *r_base = base;
    }
  }
  MEM_freeN(bases);

  *r_nurb = data.nurb;
  *r_bezt = data.bezt;
  *r_bp = data.bp;

  if (r_handle) {
    *r_handle = data.hpoint;
  }

  return (data.bezt || data.bp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection Queries
 * \{ */

void ED_curve_nurb_vert_selected_find(
    Curve *cu, View3D *v3d, Nurb **r_nu, BezTriple **r_bezt, BPoint **r_bp)
{
  /* in nu and (bezt or bp) selected are written if there's 1 sel.  */
  /* if more points selected in 1 spline: return only nu, bezt and bp are 0 */
  ListBase *editnurb = &cu->editnurb->nurbs;
  BezTriple *bezt1;
  BPoint *bp1;
  int a;

  *r_nu = NULL;
  *r_bezt = NULL;
  *r_bp = NULL;

  LISTBASE_FOREACH (Nurb *, nu1, editnurb) {
    if (nu1->type == CU_BEZIER) {
      bezt1 = nu1->bezt;
      a = nu1->pntsu;
      while (a--) {
        if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt1)) {
          if (!ELEM(*r_nu, NULL, nu1)) {
            *r_nu = NULL;
            *r_bp = NULL;
            *r_bezt = NULL;
            return;
          }

          if (*r_bezt || *r_bp) {
            *r_bp = NULL;
            *r_bezt = NULL;
          }
          else {
            *r_bezt = bezt1;
            *r_nu = nu1;
          }
        }
        bezt1++;
      }
    }
    else {
      bp1 = nu1->bp;
      a = nu1->pntsu * nu1->pntsv;
      while (a--) {
        if (bp1->f1 & SELECT) {
          if (!ELEM(*r_nu, NULL, nu1)) {
            *r_bp = NULL;
            *r_bezt = NULL;
            *r_nu = NULL;
            return;
          }

          if (*r_bezt || *r_bp) {
            *r_bp = NULL;
            *r_bezt = NULL;
          }
          else {
            *r_bp = bp1;
            *r_nu = nu1;
          }
        }
        bp1++;
      }
    }
  }
}

bool ED_curve_active_center(Curve *cu, float center[3])
{
  Nurb *nu = NULL;
  void *vert = NULL;

  if (!BKE_curve_nurb_vert_active_get(cu, &nu, &vert)) {
    return false;
  }

  if (nu->type == CU_BEZIER) {
    BezTriple *bezt = (BezTriple *)vert;
    copy_v3_v3(center, bezt->vec[1]);
  }
  else {
    BPoint *bp = (BPoint *)vert;
    copy_v3_v3(center, bp->vec);
  }

  return true;
}

/** \} */
