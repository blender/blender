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
 * The Original Code is Copyright (C) 2016, Blender Foundation
 * This is a new part of Blender
 * Operators for interpolating new Grease Pencil frames from existing strokes
 */

/** \file
 * \ingroup edgpencil
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_easing.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_color_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_report.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_gpencil.h"
#include "ED_screen.h"

#include "DEG_depsgraph.h"

#include "gpencil_intern.h"

/* Temporary interpolate operation data */
typedef struct tGPDinterpolate_layer {
  struct tGPDinterpolate_layer *next, *prev;

  /** layer */
  struct bGPDlayer *gpl;
  /** frame before current frame (interpolate-from) */
  struct bGPDframe *prevFrame;
  /** frame after current frame (interpolate-to) */
  struct bGPDframe *nextFrame;
  /** interpolated frame */
  struct bGPDframe *interFrame;
  /** interpolate factor */
  float factor;

  /* Hash tablets to create temp relationship between strokes. */
  struct GHash *used_strokes;
  struct GHash *pair_strokes;

} tGPDinterpolate_layer;

typedef struct tGPDinterpolate {
  /** Current depsgraph from context */
  struct Depsgraph *depsgraph;
  /** current scene from context */
  struct Scene *scene;
  /** area where painting originated */
  struct ScrArea *area;
  /** region where painting originated */
  struct ARegion *region;
  /** current object */
  struct Object *ob;
  /** current GP datablock */
  struct bGPdata *gpd;
  /** current material */
  struct Material *mat;
  /* Space Conversion Data */
  struct GP_SpaceConversion gsc;

  /** current frame number */
  int cframe;
  /** (tGPDinterpolate_layer) layers to be interpolated */
  ListBase ilayers;
  /** value for determining the displacement influence */
  float shift;
  /** initial interpolation factor for active layer */
  float init_factor;
  /** shift low limit (-100%) */
  float low_limit;
  /** shift upper limit (200%) */
  float high_limit;
  /** flag from toolsettings */
  int flag;
  /** Flip mode. */
  int flipmode;
  /** smooth factor */
  float smooth_factor;
  /** smooth iterations */
  int smooth_steps;

  NumInput num; /* numeric input */
} tGPDinterpolate;

typedef enum eGP_InterpolateFlipMode {
  /* No flip. */
  GP_INTERPOLATE_NOFLIP = 0,
  /* Flip always. */
  GP_INTERPOLATE_FLIP = 1,
  /* Flip if needed. */
  GP_INTERPOLATE_FLIPAUTO = 2,
} eGP_InterpolateFlipMode;

/* ************************************************ */
/* Core/Shared Utilities */

/* Poll callback for interpolation operators */
static bool gpencil_view3d_poll(bContext *C)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);

  /* only 3D view */
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype != SPACE_VIEW3D) {
    return false;
  }

  /* need data to interpolate */
  if (ELEM(NULL, gpd, gpl)) {
    return false;
  }

  return true;
}

/* Return if the stroke must be flipped or not. The logic of the calculation
 * is to check if the lines from extremes crossed. All is done in 2D. */
static bool gpencil_stroke_need_flip(Depsgraph *depsgraph,
                                     Object *ob,
                                     bGPDlayer *gpl,
                                     GP_SpaceConversion *gsc,
                                     bGPDstroke *gps_from,
                                     bGPDstroke *gps_to)
{
  float diff_mat[4][4];
  /* calculate parent matrix */
  BKE_gpencil_layer_transform_matrix_get(depsgraph, ob, gpl, diff_mat);
  bGPDspoint *pt, pt_dummy_ps;
  float v_from_start[2], v_to_start[2], v_from_end[2], v_to_end[2];

  /* Line from start of strokes. */
  pt = &gps_from->points[0];
  gpencil_point_to_parent_space(pt, diff_mat, &pt_dummy_ps);
  gpencil_point_to_xy_fl(gsc, gps_from, &pt_dummy_ps, &v_from_start[0], &v_from_start[1]);

  pt = &gps_to->points[0];
  gpencil_point_to_parent_space(pt, diff_mat, &pt_dummy_ps);
  gpencil_point_to_xy_fl(gsc, gps_from, &pt_dummy_ps, &v_to_start[0], &v_to_start[1]);

  /* Line from end of strokes. */
  pt = &gps_from->points[gps_from->totpoints - 1];
  gpencil_point_to_parent_space(pt, diff_mat, &pt_dummy_ps);
  gpencil_point_to_xy_fl(gsc, gps_from, &pt_dummy_ps, &v_from_end[0], &v_from_end[1]);

  pt = &gps_to->points[gps_to->totpoints - 1];
  gpencil_point_to_parent_space(pt, diff_mat, &pt_dummy_ps);
  gpencil_point_to_xy_fl(gsc, gps_from, &pt_dummy_ps, &v_to_end[0], &v_to_end[1]);

  const bool isect_lines = (isect_seg_seg_v2(v_from_start, v_to_start, v_from_end, v_to_end) ==
                            ISECT_LINE_LINE_CROSS);

  /* If the vectors intersect. */
  if (isect_lines) {
    /* For sharp angles, check distance between extremes. */
    float v1[2], v2[2];
    sub_v2_v2v2(v1, v_to_start, v_from_start);
    sub_v2_v2v2(v2, v_to_end, v_from_end);
    float angle = angle_v2v2(v1, v2);
    if (angle < DEG2RADF(15.0f)) {
      /* Check the original stroke orientation using a point of destination stroke
       * `(S)<--??-->(E)   <--->`. */
      float dist_start = len_squared_v2v2(v_from_start, v_to_start);
      float dist_end = len_squared_v2v2(v_from_end, v_to_start);
      /* Oriented with end nearer of destination stroke.
       * `(S)--->(E) <--->` */
      if (dist_start >= dist_end) {
        dist_start = len_squared_v2v2(v_from_end, v_to_start);
        dist_end = len_squared_v2v2(v_from_end, v_to_end);
        /* `(S)--->(E) (E)<---(S)` */
        return (dist_start >= dist_end);
      }

      /* Oriented inversed with original stroke start near of destination stroke.
       * `(E)<----(S) <--->` */
      dist_start = len_squared_v2v2(v_from_start, v_to_start);
      dist_end = len_squared_v2v2(v_from_start, v_to_end);
      /* `(E)<---(S) (S)--->(E)` */
      return (dist_start < dist_end);
    }

    return true;
  }

  /* Check that both vectors have the same direction. */
  float v1[2], v2[2];
  sub_v2_v2v2(v1, v_from_end, v_from_start);
  sub_v2_v2v2(v2, v_to_end, v_to_start);
  mul_v2_v2v2(v1, v1, v2);
  if ((v1[0] < 0.0f) && (v1[1] < 0.0f)) {
    return true;
  }

  return false;
}

/* Return the stroke related to the selection index, returning the stroke with
 * the smallest selection index greater than reference index. */
static bGPDstroke *gpencil_stroke_get_related(GHash *used_strokes,
                                              bGPDframe *gpf,
                                              const int reference_index)
{
  bGPDstroke *gps_found = NULL;
  int lower_index = INT_MAX;
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    if (gps->select_index > reference_index) {
      if (!BLI_ghash_haskey(used_strokes, gps)) {
        if (gps->select_index < lower_index) {
          lower_index = gps->select_index;
          gps_found = gps;
        }
      }
    }
  }

  /* Set as used. */
  if (gps_found) {
    BLI_ghash_insert(used_strokes, gps_found, gps_found);
  }

  return gps_found;
}

/* Load a Hash with the relationship between strokes. */
static void gpencil_stroke_pair_table(bContext *C,
                                      tGPDinterpolate *tgpi,
                                      tGPDinterpolate_layer *tgpil)
{
  bGPdata *gpd = tgpi->gpd;
  const bool only_selected = ((GPENCIL_EDIT_MODE(gpd)) &&
                              ((tgpi->flag & GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED) != 0));
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

  /* Create hash tablets with relationship between strokes. */
  tgpil->used_strokes = BLI_ghash_ptr_new(__func__);
  tgpil->pair_strokes = BLI_ghash_ptr_new(__func__);

  /* Create a table with source and target pair of strokes. */
  LISTBASE_FOREACH (bGPDstroke *, gps_from, &tgpil->prevFrame->strokes) {
    bGPDstroke *gps_to = NULL;
    /* only selected */
    if ((GPENCIL_EDIT_MODE(gpd)) && (only_selected) &&
        ((gps_from->flag & GP_STROKE_SELECT) == 0)) {
      continue;
    }
    /* skip strokes that are invalid for current view */
    if (ED_gpencil_stroke_can_use(C, gps_from) == false) {
      continue;
    }
    /* Check if the material is editable. */
    if (ED_gpencil_stroke_material_editable(tgpi->ob, tgpil->gpl, gps_from) == false) {
      continue;
    }
    /* Try to get the related stroke. */
    if ((is_multiedit) && (gps_from->select_index > 0)) {
      gps_to = gpencil_stroke_get_related(
          tgpil->used_strokes, tgpil->nextFrame, gps_from->select_index);
    }
    /* If not found, get final stroke to interpolate using position in the array. */
    if (gps_to == NULL) {
      int fFrame = BLI_findindex(&tgpil->prevFrame->strokes, gps_from);
      gps_to = BLI_findlink(&tgpil->nextFrame->strokes, fFrame);
    }

    if (ELEM(NULL, gps_from, gps_to)) {
      continue;
    }
    /* Insert the pair entry in the hash table. */
    BLI_ghash_insert(tgpil->pair_strokes, gps_from, gps_to);
  }
}

static void gpencil_interpolate_smooth_stroke(bGPDstroke *gps,
                                              float smooth_factor,
                                              int smooth_steps)
{
  if (smooth_factor == 0.0f) {
    return;
  }

  float reduce = 0.0f;
  for (int r = 0; r < smooth_steps; r++) {
    for (int i = 0; i < gps->totpoints - 1; i++) {
      BKE_gpencil_stroke_smooth(gps, i, smooth_factor - reduce);
      BKE_gpencil_stroke_smooth_strength(gps, i, smooth_factor);
    }
    reduce += 0.25f; /* reduce the factor */
  }
}
/* Perform interpolation */
static void gpencil_interpolate_update_points(const bGPDstroke *gps_from,
                                              const bGPDstroke *gps_to,
                                              bGPDstroke *new_stroke,
                                              float factor)
{
  /* update points */
  for (int i = 0; i < new_stroke->totpoints; i++) {
    const bGPDspoint *prev = &gps_from->points[i];
    const bGPDspoint *next = &gps_to->points[i];
    bGPDspoint *pt = &new_stroke->points[i];

    /* Interpolate all values */
    interp_v3_v3v3(&pt->x, &prev->x, &next->x, factor);
    pt->pressure = interpf(prev->pressure, next->pressure, 1.0f - factor);
    pt->strength = interpf(prev->strength, next->strength, 1.0f - factor);
    CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
  }
}

/* ****************** Interpolate Interactive *********************** */
/* Helper: free all temp strokes for display. */
static void gpencil_interpolate_free_tagged_strokes(bGPDframe *gpf)
{
  if (gpf == NULL) {
    return;
  }

  LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
    if (gps->flag & GP_STROKE_TAG) {
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
    }
  }
}

/* Helper: Untag all strokes. */
static void gpencil_interpolate_untag_strokes(bGPDlayer *gpl)
{
  if (gpl == NULL) {
    return;
  }

  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (gps->flag & GP_STROKE_TAG) {
        gps->flag &= ~GP_STROKE_TAG;
      }
    }
  }
}

/* Helper: Update all strokes interpolated */
static void gpencil_interpolate_update_strokes(bContext *C, tGPDinterpolate *tgpi)
{
  bGPdata *gpd = tgpi->gpd;
  const float shift = tgpi->shift;

  LISTBASE_FOREACH (tGPDinterpolate_layer *, tgpil, &tgpi->ilayers) {
    const float factor = tgpil->factor + shift;

    bGPDframe *gpf = tgpil->gpl->actframe;
    /* Free temp strokes used for display. */
    gpencil_interpolate_free_tagged_strokes(gpf);

    /* Clear previous interpolations. */
    gpencil_interpolate_free_tagged_strokes(tgpil->interFrame);

    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, tgpil->pair_strokes) {
      bGPDstroke *gps_from = (bGPDstroke *)BLI_ghashIterator_getKey(&gh_iter);
      bGPDstroke *gps_to = (bGPDstroke *)BLI_ghashIterator_getValue(&gh_iter);
      /* Create new stroke. */
      bGPDstroke *new_stroke = BKE_gpencil_stroke_duplicate(gps_from, true, true);
      new_stroke->flag |= GP_STROKE_TAG;
      new_stroke->select_index = 0;

      /* Update points position. */
      gpencil_interpolate_update_points(gps_from, gps_to, new_stroke, factor);

      /* Calc geometry data. */
      BKE_gpencil_stroke_geometry_update(gpd, new_stroke);
      /* Add to strokes. */
      BLI_addtail(&tgpil->interFrame->strokes, new_stroke);

      /* Add temp strokes to display. */
      if (gpf) {
        bGPDstroke *gps_eval = BKE_gpencil_stroke_duplicate(new_stroke, true, true);
        gps_eval->flag |= GP_STROKE_TAG;
        BLI_addtail(&gpf->strokes, gps_eval);
      }
    }
  }

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
}

/* Helper: Get previous keyframe. */
static bGPDframe *gpencil_get_previous_keyframe(bGPDlayer *gpl, int cfra)
{
  if (gpl->actframe != NULL && gpl->actframe->framenum < cfra &&
      gpl->actframe->key_type == BEZT_KEYTYPE_KEYFRAME) {
    return gpl->actframe;
  }

  LISTBASE_FOREACH_BACKWARD (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->key_type != BEZT_KEYTYPE_KEYFRAME) {
      continue;
    }
    if (gpf->framenum >= cfra) {
      continue;
    }
    return gpf;
  }

  return NULL;
}

/* Helper: Get next keyframe. */
static bGPDframe *gpencil_get_next_keyframe(bGPDlayer *gpl, int cfra)
{
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->key_type != BEZT_KEYTYPE_KEYFRAME) {
      continue;
    }
    if (gpf->framenum <= cfra) {
      continue;
    }
    return gpf;
  }

  return NULL;
}

/* Helper: Create internal strokes interpolated */
static void gpencil_interpolate_set_points(bContext *C, tGPDinterpolate *tgpi)
{
  Scene *scene = tgpi->scene;
  bGPdata *gpd = tgpi->gpd;
  bGPDlayer *active_gpl = CTX_data_active_gpencil_layer(C);
  bGPDframe *actframe = active_gpl->actframe;

  /* save initial factor for active layer to define shift limits */
  tgpi->init_factor = (float)(tgpi->cframe - actframe->framenum) /
                      (actframe->next->framenum - actframe->framenum + 1);

  /* limits are 100% below 0 and 100% over the 100% */
  tgpi->low_limit = -1.0f - tgpi->init_factor;
  tgpi->high_limit = 2.0f - tgpi->init_factor;

  /* set layers */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    tGPDinterpolate_layer *tgpil;
    /* all layers or only active */
    if (!(tgpi->flag & GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS) && (gpl != active_gpl)) {
      continue;
    }
    /* only editable and visible layers are considered */
    if (!BKE_gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }
    if ((gpl->actframe == NULL) || (gpl->actframe->next == NULL)) {
      continue;
    }

    /* create temp data for each layer */
    tgpil = MEM_callocN(sizeof(tGPDinterpolate_layer), "GPencil Interpolate Layer");

    tgpil->gpl = gpl;
    bGPDframe *gpf = gpencil_get_previous_keyframe(gpl, CFRA);
    tgpil->prevFrame = BKE_gpencil_frame_duplicate(gpf, true);

    gpf = gpencil_get_next_keyframe(gpl, CFRA);
    tgpil->nextFrame = BKE_gpencil_frame_duplicate(gpf, true);

    BLI_addtail(&tgpi->ilayers, tgpil);

    /* Create a new temporary frame. */
    tgpil->interFrame = MEM_callocN(sizeof(bGPDframe), "bGPDframe");
    tgpil->interFrame->framenum = tgpi->cframe;

    /* get interpolation factor by layer (usually must be equal for all layers, but not sure) */
    tgpil->factor = (float)(tgpi->cframe - tgpil->prevFrame->framenum) /
                    (tgpil->nextFrame->framenum - tgpil->prevFrame->framenum + 1);

    /* Load the relationship between frames. */
    gpencil_stroke_pair_table(C, tgpi, tgpil);

    /* Create new strokes data with interpolated points reading original stroke. */
    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, tgpil->pair_strokes) {
      bGPDstroke *gps_from = (bGPDstroke *)BLI_ghashIterator_getKey(&gh_iter);
      bGPDstroke *gps_to = (bGPDstroke *)BLI_ghashIterator_getValue(&gh_iter);

      /* If destination stroke is smaller, resize new_stroke to size of gps_to stroke. */
      if (gps_from->totpoints > gps_to->totpoints) {
        BKE_gpencil_stroke_uniform_subdivide(gpd, gps_to, gps_from->totpoints, true);
      }
      if (gps_to->totpoints > gps_from->totpoints) {
        BKE_gpencil_stroke_uniform_subdivide(gpd, gps_from, gps_to->totpoints, true);
      }

      /* Flip stroke. */
      if (tgpi->flipmode == GP_INTERPOLATE_FLIP) {
        BKE_gpencil_stroke_flip(gps_to);
      }
      else if (tgpi->flipmode == GP_INTERPOLATE_FLIPAUTO) {
        if (gpencil_stroke_need_flip(
                tgpi->depsgraph, tgpi->ob, gpl, &tgpi->gsc, gps_from, gps_to)) {
          BKE_gpencil_stroke_flip(gps_to);
        }
      }

      /* Create new stroke. */
      bGPDstroke *new_stroke = BKE_gpencil_stroke_duplicate(gps_from, true, true);
      new_stroke->flag |= GP_STROKE_TAG;
      new_stroke->select_index = 0;

      /* Update points position. */
      gpencil_interpolate_update_points(gps_from, gps_to, new_stroke, tgpil->factor);
      gpencil_interpolate_smooth_stroke(new_stroke, tgpi->smooth_factor, tgpi->smooth_steps);

      /* Calc geometry data. */
      BKE_gpencil_stroke_geometry_update(gpd, new_stroke);
      /* add to strokes */
      BLI_addtail(&tgpil->interFrame->strokes, new_stroke);
    }
  }
}

/* ----------------------- */

/* Helper: calculate shift based on position of mouse (we only use x-axis for now.
 * since this is more convenient for users to do), and store new shift value
 */
static void gpencil_mouse_update_shift(tGPDinterpolate *tgpi, wmOperator *op, const wmEvent *event)
{
  float mid = (float)(tgpi->region->winx - tgpi->region->winrct.xmin) / 2.0f;
  float mpos = event->x - tgpi->region->winrct.xmin;

  if (mpos >= mid) {
    tgpi->shift = ((mpos - mid) * tgpi->high_limit) / mid;
  }
  else {
    tgpi->shift = tgpi->low_limit - ((mpos * tgpi->low_limit) / mid);
  }

  CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
  RNA_float_set(op->ptr, "shift", tgpi->shift);
}

/* Helper: Draw status message while the user is running the operator */
static void gpencil_interpolate_status_indicators(bContext *C, tGPDinterpolate *p)
{
  Scene *scene = p->scene;
  char status_str[UI_MAX_DRAW_STR];
  char msg_str[UI_MAX_DRAW_STR];

  BLI_strncpy(msg_str, TIP_("GPencil Interpolation: "), UI_MAX_DRAW_STR);

  if (hasNumInput(&p->num)) {
    char str_ofs[NUM_STR_REP_LEN];

    outputNumInput(&p->num, str_ofs, &scene->unit);
    BLI_snprintf(status_str, sizeof(status_str), "%s%s", msg_str, str_ofs);
  }
  else {
    BLI_snprintf(status_str,
                 sizeof(status_str),
                 "%s%d %%",
                 msg_str,
                 (int)((p->init_factor + p->shift) * 100.0f));
  }

  ED_area_status_text(p->area, status_str);
  ED_workspace_status_text(
      C, TIP_("ESC/RMB to cancel, Enter/LMB to confirm, WHEEL/MOVE to adjust factor"));
}

/* Update screen and stroke */
static void gpencil_interpolate_update(bContext *C, wmOperator *op, tGPDinterpolate *tgpi)
{
  /* update shift indicator in header */
  gpencil_interpolate_status_indicators(C, tgpi);
  /* apply... */
  tgpi->shift = RNA_float_get(op->ptr, "shift");
  /* update points position */
  gpencil_interpolate_update_strokes(C, tgpi);
}

/* ----------------------- */

/* Exit and free memory */
static void gpencil_interpolate_exit(bContext *C, wmOperator *op)
{
  tGPDinterpolate *tgpi = op->customdata;
  bGPdata *gpd = tgpi->gpd;

  /* don't assume that operator data exists at all */
  if (tgpi) {
    /* clear status message area */
    ED_area_status_text(tgpi->area, NULL);
    ED_workspace_status_text(C, NULL);

    /* Clear any temp stroke. */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        gpencil_interpolate_free_tagged_strokes(gpf);
      }
    }

    /* finally, free memory used by temp data */
    LISTBASE_FOREACH (tGPDinterpolate_layer *, tgpil, &tgpi->ilayers) {
      BKE_gpencil_free_strokes(tgpil->prevFrame);
      BKE_gpencil_free_strokes(tgpil->nextFrame);
      BKE_gpencil_free_strokes(tgpil->interFrame);
      MEM_SAFE_FREE(tgpil->prevFrame);
      MEM_SAFE_FREE(tgpil->nextFrame);
      MEM_SAFE_FREE(tgpil->interFrame);

      /* Free Hash tablets. */
      if (tgpil->used_strokes != NULL) {
        BLI_ghash_free(tgpil->used_strokes, NULL, NULL);
      }
      if (tgpil->pair_strokes != NULL) {
        BLI_ghash_free(tgpil->pair_strokes, NULL, NULL);
      }
    }

    BLI_freelistN(&tgpi->ilayers);

    MEM_SAFE_FREE(tgpi);
  }
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* clear pointer */
  op->customdata = NULL;
}

/* Init new temporary interpolation data */
static bool gpencil_interpolate_set_init_values(bContext *C, wmOperator *op, tGPDinterpolate *tgpi)
{
  /* set current scene and window */
  tgpi->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  tgpi->scene = CTX_data_scene(C);
  tgpi->area = CTX_wm_area(C);
  tgpi->region = CTX_wm_region(C);
  tgpi->ob = CTX_data_active_object(C);
  /* Setup space conversions. */
  gpencil_point_conversion_init(C, &tgpi->gsc);

  /* set current frame number */
  tgpi->cframe = tgpi->scene->r.cfra;

  /* set GP datablock */
  tgpi->gpd = tgpi->ob->data;
  /* set interpolation weight */
  tgpi->shift = RNA_float_get(op->ptr, "shift");
  SET_FLAG_FROM_TEST(
      tgpi->flag, (RNA_enum_get(op->ptr, "layers") == 1), GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS);
  SET_FLAG_FROM_TEST(
      tgpi->flag,
      ((GPENCIL_EDIT_MODE(tgpi->gpd)) && (RNA_boolean_get(op->ptr, "interpolate_selected_only"))),
      GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED);

  tgpi->flipmode = RNA_enum_get(op->ptr, "flip");

  tgpi->smooth_factor = RNA_float_get(op->ptr, "smooth_factor");
  tgpi->smooth_steps = RNA_int_get(op->ptr, "smooth_steps");

  /* Untag strokes to be sure nothing is pending due any canceled process. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &tgpi->gpd->layers) {
    gpencil_interpolate_untag_strokes(gpl);
  }

  /* Set layers */
  gpencil_interpolate_set_points(C, tgpi);

  return 1;
}

/* Allocate memory and initialize values */
static tGPDinterpolate *gpencil_session_init_interpolation(bContext *C, wmOperator *op)
{
  tGPDinterpolate *tgpi = MEM_callocN(sizeof(tGPDinterpolate), "GPencil Interpolate Data");

  /* define initial values */
  gpencil_interpolate_set_init_values(C, op, tgpi);

  /* return context data for running operator */
  return tgpi;
}

/* Init interpolation: Allocate memory and set init values */
static int gpencil_interpolate_init(bContext *C, wmOperator *op)
{
  tGPDinterpolate *tgpi;

  /* check context */
  tgpi = op->customdata = gpencil_session_init_interpolation(C, op);
  if (tgpi == NULL) {
    /* something wasn't set correctly in context */
    gpencil_interpolate_exit(C, op);
    return 0;
  }

  /* everything is now setup ok */
  return 1;
}

/* ----------------------- */

/* Invoke handler: Initialize the operator */
static int gpencil_interpolate_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  wmWindow *win = CTX_wm_window(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
  Scene *scene = CTX_data_scene(C);
  tGPDinterpolate *tgpi = NULL;

  /* cannot interpolate if not between 2 frames */
  int cfra = CFRA;
  bGPDframe *gpf_prv = gpencil_get_previous_keyframe(gpl, cfra);
  bGPDframe *gpf_next = gpencil_get_next_keyframe(gpl, cfra);
  if (ELEM(NULL, gpf_prv, gpf_next)) {
    BKE_report(
        op->reports,
        RPT_ERROR,
        "Cannot find a pair of grease pencil frames to interpolate between in active layer");
    return OPERATOR_CANCELLED;
  }

  if (GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot interpolate in curve edit mode");
    return OPERATOR_CANCELLED;
  }
  /* try to initialize context data needed */
  if (!gpencil_interpolate_init(C, op)) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }
    return OPERATOR_CANCELLED;
  }
  tgpi = op->customdata;

  /* set cursor to indicate modal */
  WM_cursor_modal_set(win, WM_CURSOR_EW_SCROLL);

  /* update shift indicator in header */
  gpencil_interpolate_status_indicators(C, tgpi);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* add a modal handler for this operator */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* Modal handler: Events handling during interactive part */
static int gpencil_interpolate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPDinterpolate *tgpi = op->customdata;
  wmWindow *win = CTX_wm_window(C);
  bGPDframe *gpf_dst;
  bGPDstroke *gps_dst;
  const bool has_numinput = hasNumInput(&tgpi->num);

  switch (event->type) {
    case LEFTMOUSE: /* confirm */
    case EVT_PADENTER:
    case EVT_RETKEY: {
      /* return to normal cursor and header status */
      ED_area_status_text(tgpi->area, NULL);
      ED_workspace_status_text(C, NULL);
      WM_cursor_modal_restore(win);

      /* insert keyframes as required... */
      LISTBASE_FOREACH (tGPDinterpolate_layer *, tgpil, &tgpi->ilayers) {
        gpf_dst = BKE_gpencil_layer_frame_get(tgpil->gpl, tgpi->cframe, GP_GETFRAME_ADD_NEW);
        gpf_dst->key_type = BEZT_KEYTYPE_BREAKDOWN;

        /* Copy strokes. */
        LISTBASE_FOREACH (bGPDstroke *, gps_src, &tgpil->interFrame->strokes) {
          if (gps_src->totpoints == 0) {
            continue;
          }

          /* make copy of source stroke, then adjust pointer to points too */
          gps_dst = BKE_gpencil_stroke_duplicate(gps_src, true, true);
          gps_dst->flag &= ~GP_STROKE_TAG;

          /* Calc geometry data. */
          BKE_gpencil_stroke_geometry_update(tgpi->gpd, gps_dst);

          BLI_addtail(&gpf_dst->strokes, gps_dst);
        }
      }

      /* clean up temp data */
      gpencil_interpolate_exit(C, op);

      /* done! */
      return OPERATOR_FINISHED;
    }

    case EVT_ESCKEY: /* cancel */
    case RIGHTMOUSE: {
      /* return to normal cursor and header status */
      ED_area_status_text(tgpi->area, NULL);
      ED_workspace_status_text(C, NULL);
      WM_cursor_modal_restore(win);

      /* clean up temp data */
      gpencil_interpolate_exit(C, op);

      /* canceled! */
      return OPERATOR_CANCELLED;
    }

    case WHEELUPMOUSE: {
      tgpi->shift = tgpi->shift + 0.01f;
      CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
      RNA_float_set(op->ptr, "shift", tgpi->shift);

      /* update screen */
      gpencil_interpolate_update(C, op, tgpi);
      break;
    }
    case WHEELDOWNMOUSE: {
      tgpi->shift = tgpi->shift - 0.01f;
      CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
      RNA_float_set(op->ptr, "shift", tgpi->shift);

      /* update screen */
      gpencil_interpolate_update(C, op, tgpi);
      break;
    }
    case MOUSEMOVE: /* calculate new position */
    {
      /* only handle mousemove if not doing numinput */
      if (has_numinput == false) {
        /* update shift based on position of mouse */
        gpencil_mouse_update_shift(tgpi, op, event);

        /* update screen */
        gpencil_interpolate_update(C, op, tgpi);
      }
      break;
    }
    default: {
      if ((event->val == KM_PRESS) && handleNumInput(C, &tgpi->num, event)) {
        const float factor = tgpi->init_factor;
        float value;

        /* Grab shift from numeric input, and store this new value (the user see an int) */
        value = (factor + tgpi->shift) * 100.0f;
        applyNumInput(&tgpi->num, &value);
        tgpi->shift = value / 100.0f;

        /* recalculate the shift to get the right value in the frame scale */
        tgpi->shift = tgpi->shift - factor;

        CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
        RNA_float_set(op->ptr, "shift", tgpi->shift);

        /* update screen */
        gpencil_interpolate_update(C, op, tgpi);

        break;
      }
      /* unhandled event - allow to pass through */
      return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
    }
  }

  /* still running... */
  return OPERATOR_RUNNING_MODAL;
}

/* Cancel handler */
static void gpencil_interpolate_cancel(bContext *C, wmOperator *op)
{
  /* this is just a wrapper around exit() */
  gpencil_interpolate_exit(C, op);
}

void GPENCIL_OT_interpolate(wmOperatorType *ot)
{
  static const EnumPropertyItem flip_modes[] = {
      {GP_INTERPOLATE_NOFLIP, "NOFLIP", 0, "No Flip", ""},
      {GP_INTERPOLATE_FLIP, "FLIP", 0, "Flip", ""},
      {GP_INTERPOLATE_FLIPAUTO, "AUTO", 0, "Automatic", ""},
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Grease Pencil Interpolation";
  ot->idname = "GPENCIL_OT_interpolate";
  ot->description = "Interpolate grease pencil strokes between frames";

  /* callbacks */
  ot->invoke = gpencil_interpolate_invoke;
  ot->modal = gpencil_interpolate_modal;
  ot->cancel = gpencil_interpolate_cancel;
  ot->poll = gpencil_view3d_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  static const EnumPropertyItem gpencil_interpolation_layer_items[] = {
      {0, "ACTIVE", 0, "Active", ""},
      {1, "ALL", 0, "All Layers", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* properties */
  RNA_def_float_factor(
      ot->srna,
      "shift",
      0.0f,
      -1.0f,
      1.0f,
      "Shift",
      "Bias factor for which frame has more influence on the interpolated strokes",
      -0.9f,
      0.9f);

  RNA_def_enum(ot->srna,
               "layers",
               gpencil_interpolation_layer_items,
               0,
               "Layer",
               "Layers included in the interpolation");

  RNA_def_boolean(ot->srna,
                  "interpolate_selected_only",
                  0,
                  "Only Selected",
                  "Interpolate only selected strokes");

  RNA_def_enum(ot->srna,
               "flip",
               flip_modes,
               GP_INTERPOLATE_FLIPAUTO,
               "Flip Mode",
               "Invert destination stroke to match start and end with source stroke");

  RNA_def_int(ot->srna,
              "smooth_steps",
              1,
              1,
              3,
              "Iterations",
              "Number of times to smooth newly created strokes",
              1,
              3);

  RNA_def_float(ot->srna,
                "smooth_factor",
                0.0f,
                0.0f,
                2.0f,
                "Smooth",
                "Amount of smoothing to apply to interpolated strokes, to reduce jitter/noise",
                0.0f,
                2.0f);

  prop = RNA_def_boolean(ot->srna, "release_confirm", 0, "Confirm on Release", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* ****************** Interpolate Sequence *********************** */

/* Helper: Perform easing equation calculations for GP interpolation operator */
static float gpencil_interpolate_seq_easing_calc(wmOperator *op, float time)
{
  const float begin = 0.0f;
  const float change = 1.0f;
  const float duration = 1.0f;

  const float back = RNA_float_get(op->ptr, "back");
  const float amplitude = RNA_float_get(op->ptr, "amplitude");
  const float period = RNA_float_get(op->ptr, "period");
  const eBezTriple_Easing easing = RNA_enum_get(op->ptr, "easing");
  const eGP_Interpolate_Type type = RNA_enum_get(op->ptr, "type");
  float result = time;

  switch (type) {
    case GP_IPO_BACK:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_back_ease_in(time, begin, change, duration, back);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_back_ease_out(time, begin, change, duration, back);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_back_ease_in_out(time, begin, change, duration, back);
          break;

        default: /* default/auto: same as ease out */
          result = BLI_easing_back_ease_out(time, begin, change, duration, back);
          break;
      }
      break;

    case GP_IPO_BOUNCE:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_bounce_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_bounce_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_bounce_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease out */
          result = BLI_easing_bounce_ease_out(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_CIRC:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_circ_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_circ_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_circ_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_circ_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_CUBIC:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_cubic_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_cubic_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_cubic_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_cubic_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_ELASTIC:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_elastic_ease_in(time, begin, change, duration, amplitude, period);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_elastic_ease_in_out(
              time, begin, change, duration, amplitude, period);
          break;

        default: /* default/auto: same as ease out */
          result = BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
          break;
      }
      break;

    case GP_IPO_EXPO:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_expo_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_expo_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_expo_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_expo_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_QUAD:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_quad_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_quad_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_quad_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_quad_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_QUART:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_quart_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_quart_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_quart_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_quart_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_QUINT:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_quint_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_quint_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_quint_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_quint_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_SINE:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_sine_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_sine_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_sine_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_sine_ease_in(time, begin, change, duration);
          break;
      }
      break;

    default:
      printf("%s: Unknown interpolation type\n", __func__);
      break;
  }

  return result;
}

static int gpencil_interpolate_seq_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ob->data;
  bGPDlayer *active_gpl = CTX_data_active_gpencil_layer(C);
  /* Setup space conversions. */
  GP_SpaceConversion gsc;
  gpencil_point_conversion_init(C, &gsc);

  int cfra = CFRA;

  GP_Interpolate_Settings *ipo_settings = &ts->gp_interpolate;
  const int step = RNA_int_get(op->ptr, "step");
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const bool all_layers = (bool)(RNA_enum_get(op->ptr, "layers") == 1);
  const bool only_selected = ((GPENCIL_EDIT_MODE(gpd)) &&
                              (RNA_boolean_get(op->ptr, "interpolate_selected_only") != 0));

  eGP_InterpolateFlipMode flipmode = RNA_enum_get(op->ptr, "flip");

  const float smooth_factor = RNA_float_get(op->ptr, "smooth_factor");
  const int smooth_steps = RNA_int_get(op->ptr, "smooth_steps");

  const eGP_Interpolate_Type type = RNA_enum_get(op->ptr, "type");

  if (ipo_settings->custom_ipo == NULL) {
    ipo_settings->custom_ipo = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  }
  BKE_curvemapping_init(ipo_settings->custom_ipo);

  /* Cannot interpolate if not between 2 frames. */
  bGPDframe *gpf_prv = gpencil_get_previous_keyframe(active_gpl, cfra);
  bGPDframe *gpf_next = gpencil_get_next_keyframe(active_gpl, cfra);
  if (ELEM(NULL, gpf_prv, gpf_next)) {
    BKE_report(
        op->reports,
        RPT_ERROR,
        "Cannot find a pair of grease pencil frames to interpolate between in active layer");
    return OPERATOR_CANCELLED;
  }

  if (GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot interpolate in curve edit mode");
    return OPERATOR_CANCELLED;
  }

  /* loop all layer to check if need interpolation */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* all layers or only active */
    if ((!all_layers) && (gpl != active_gpl)) {
      continue;
    }
    /* only editable and visible layers are considered */
    if (!BKE_gpencil_layer_is_editable(gpl)) {
      continue;
    }
    gpf_prv = gpencil_get_previous_keyframe(gpl, cfra);
    gpf_next = gpencil_get_next_keyframe(gpl, cfra);

    /* Need a set of frames to interpolate. */
    if ((gpf_prv == NULL) || (gpf_next == NULL)) {
      continue;
    }

    /* Store extremes. */
    bGPDframe *prevFrame = BKE_gpencil_frame_duplicate(gpf_prv, true);
    bGPDframe *nextFrame = BKE_gpencil_frame_duplicate(gpf_next, true);

    /* Create a table with source and target pair of strokes. */
    GHash *used_strokes = BLI_ghash_ptr_new(__func__);
    GHash *pair_strokes = BLI_ghash_ptr_new(__func__);

    LISTBASE_FOREACH (bGPDstroke *, gps_from, &prevFrame->strokes) {
      bGPDstroke *gps_to = NULL;
      /* Only selected. */
      if ((GPENCIL_EDIT_MODE(gpd)) && (only_selected) &&
          ((gps_from->flag & GP_STROKE_SELECT) == 0)) {
        continue;
      }
      /* Skip strokes that are invalid for current view. */
      if (ED_gpencil_stroke_can_use(C, gps_from) == false) {
        continue;
      }
      /* Check if the material is editable. */
      if (ED_gpencil_stroke_material_editable(ob, gpl, gps_from) == false) {
        continue;
      }
      /* Try to get the related stroke. */
      if ((is_multiedit) && (gps_from->select_index > 0)) {
        gps_to = gpencil_stroke_get_related(used_strokes, nextFrame, gps_from->select_index);
      }
      /* If not found, get final stroke to interpolate using position in the array. */
      if (gps_to == NULL) {
        int fFrame = BLI_findindex(&prevFrame->strokes, gps_from);
        gps_to = BLI_findlink(&nextFrame->strokes, fFrame);
      }

      if (ELEM(NULL, gps_from, gps_to)) {
        continue;
      }

      /* if destination stroke is smaller, resize new_stroke to size of gps_to stroke */
      if (gps_from->totpoints > gps_to->totpoints) {
        BKE_gpencil_stroke_uniform_subdivide(gpd, gps_to, gps_from->totpoints, true);
      }
      if (gps_to->totpoints > gps_from->totpoints) {
        BKE_gpencil_stroke_uniform_subdivide(gpd, gps_from, gps_to->totpoints, true);
      }

      /* Flip stroke. */
      if (flipmode == GP_INTERPOLATE_FLIP) {
        BKE_gpencil_stroke_flip(gps_to);
      }
      else if (flipmode == GP_INTERPOLATE_FLIPAUTO) {
        if (gpencil_stroke_need_flip(depsgraph, ob, gpl, &gsc, gps_from, gps_to)) {
          BKE_gpencil_stroke_flip(gps_to);
        }
      }

      /* Insert the pair entry in the hash table. */
      BLI_ghash_insert(pair_strokes, gps_from, gps_to);
    }

    /* Loop over intermediary frames and create the interpolation. */
    for (int cframe = prevFrame->framenum + step; cframe < nextFrame->framenum; cframe += step) {
      /* Get interpolation factor. */
      float framerange = nextFrame->framenum - prevFrame->framenum;
      CLAMP_MIN(framerange, 1.0f);
      float factor = (float)(cframe - prevFrame->framenum) / framerange;

      if (type == GP_IPO_CURVEMAP) {
        /* custom curvemap */
        if (ipo_settings->custom_ipo) {
          factor = BKE_curvemapping_evaluateF(ipo_settings->custom_ipo, 0, factor);
        }
        else {
          BKE_report(op->reports, RPT_ERROR, "Custom interpolation curve does not exist");
          continue;
        }
      }
      else if (type >= GP_IPO_BACK) {
        /* easing equation... */
        factor = gpencil_interpolate_seq_easing_calc(op, factor);
      }

      /* Apply the factor to all pair of strokes. */
      GHashIterator gh_iter;
      GHASH_ITER (gh_iter, pair_strokes) {
        bGPDstroke *gps_from = (bGPDstroke *)BLI_ghashIterator_getKey(&gh_iter);
        bGPDstroke *gps_to = (bGPDstroke *)BLI_ghashIterator_getValue(&gh_iter);

        /* Create new stroke. */
        bGPDstroke *new_stroke = BKE_gpencil_stroke_duplicate(gps_from, true, true);
        new_stroke->flag |= GP_STROKE_TAG;
        new_stroke->select_index = 0;

        /* Update points position. */
        gpencil_interpolate_update_points(gps_from, gps_to, new_stroke, factor);
        gpencil_interpolate_smooth_stroke(new_stroke, smooth_factor, smooth_steps);

        /* Calc geometry data. */
        BKE_gpencil_stroke_geometry_update(gpd, new_stroke);

        /* Add strokes to frame. */
        bGPDframe *interFrame = BKE_gpencil_layer_frame_get(gpl, cframe, GP_GETFRAME_ADD_NEW);
        interFrame->key_type = BEZT_KEYTYPE_BREAKDOWN;

        BLI_addtail(&interFrame->strokes, new_stroke);
      }
    }

    /* Free Hash tablets. */
    if (used_strokes != NULL) {
      BLI_ghash_free(used_strokes, NULL, NULL);
    }
    if (pair_strokes != NULL) {
      BLI_ghash_free(pair_strokes, NULL, NULL);
    }

    BKE_gpencil_free_strokes(prevFrame);
    BKE_gpencil_free_strokes(nextFrame);
    MEM_SAFE_FREE(prevFrame);
    MEM_SAFE_FREE(nextFrame);
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void gpencil_interpolate_seq_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *col, *row;
  PointerRNA ptr;

  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);

  const eGP_Interpolate_Type type = RNA_enum_get(op->ptr, "type");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, &ptr, "step", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "layers", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "interpolate_selected_only", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "flip", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "smooth_factor", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "smooth_steps", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "type", 0, NULL, ICON_NONE);

  if (type == GP_IPO_CURVEMAP) {
    /* Get an RNA pointer to ToolSettings to give to the custom curve. */
    Scene *scene = CTX_data_scene(C);
    ToolSettings *ts = scene->toolsettings;
    PointerRNA gpsettings_ptr;
    RNA_pointer_create(
        &scene->id, &RNA_GPencilInterpolateSettings, &ts->gp_interpolate, &gpsettings_ptr);
    uiTemplateCurveMapping(
        layout, &gpsettings_ptr, "interpolation_curve", 0, false, true, true, false);
  }
  else if (type != GP_IPO_LINEAR) {
    row = uiLayoutRow(layout, false);
    uiItemR(row, &ptr, "easing", 0, NULL, ICON_NONE);
    if (type == GP_IPO_BACK) {
      row = uiLayoutRow(layout, false);
      uiItemR(row, &ptr, "back", 0, NULL, ICON_NONE);
    }
    else if (type == GP_IPO_ELASTIC) {
      row = uiLayoutRow(layout, false);
      uiItemR(row, &ptr, "amplitude", 0, NULL, ICON_NONE);
      row = uiLayoutRow(layout, false);
      uiItemR(row, &ptr, "period", 0, NULL, ICON_NONE);
    }
  }
}

void GPENCIL_OT_interpolate_sequence(wmOperatorType *ot)
{
  static const EnumPropertyItem gpencil_interpolation_layer_items[] = {
      {0, "ACTIVE", 0, "Active", ""},
      {1, "ALL", 0, "All Layers", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /**
   * \note this is a near exact duplicate of #rna_enum_beztriple_interpolation_mode_items,
   * Changes here will likely apply there too.
   */
  static const EnumPropertyItem gpencil_interpolation_type_items[] = {
      /* interpolation */
      {0, "", 0, N_("Interpolation"), "Standard transitions between keyframes"},
      {GP_IPO_LINEAR,
       "LINEAR",
       ICON_IPO_LINEAR,
       "Linear",
       "Straight-line interpolation between A and B (i.e. no ease in/out)"},
      {GP_IPO_CURVEMAP,
       "CUSTOM",
       ICON_IPO_BEZIER,
       "Custom",
       "Custom interpolation defined using a curve map"},

      /* easing */
      {0,
       "",
       0,
       N_("Easing (by strength)"),
       "Predefined inertial transitions, useful for motion graphics (from least to most "
       "''dramatic'')"},
      {GP_IPO_SINE,
       "SINE",
       ICON_IPO_SINE,
       "Sinusoidal",
       "Sinusoidal easing (weakest, almost linear but with a slight curvature)"},
      {GP_IPO_QUAD, "QUAD", ICON_IPO_QUAD, "Quadratic", "Quadratic easing"},
      {GP_IPO_CUBIC, "CUBIC", ICON_IPO_CUBIC, "Cubic", "Cubic easing"},
      {GP_IPO_QUART, "QUART", ICON_IPO_QUART, "Quartic", "Quartic easing"},
      {GP_IPO_QUINT, "QUINT", ICON_IPO_QUINT, "Quintic", "Quintic easing"},
      {GP_IPO_EXPO, "EXPO", ICON_IPO_EXPO, "Exponential", "Exponential easing (dramatic)"},
      {GP_IPO_CIRC,
       "CIRC",
       ICON_IPO_CIRC,
       "Circular",
       "Circular easing (strongest and most dynamic)"},

      {0, "", 0, N_("Dynamic Effects"), "Simple physics-inspired easing effects"},
      {GP_IPO_BACK, "BACK", ICON_IPO_BACK, "Back", "Cubic easing with overshoot and settle"},
      {GP_IPO_BOUNCE,
       "BOUNCE",
       ICON_IPO_BOUNCE,
       "Bounce",
       "Exponentially decaying parabolic bounce, like when objects collide"},
      {GP_IPO_ELASTIC,
       "ELASTIC",
       ICON_IPO_ELASTIC,
       "Elastic",
       "Exponentially decaying sine wave, like an elastic band"},

      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem gpencil_interpolation_easing_items[] = {
      {BEZT_IPO_EASE_AUTO,
       "AUTO",
       ICON_IPO_EASE_IN_OUT,
       "Automatic Easing",
       "Easing type is chosen automatically based on what the type of interpolation used "
       "(e.g. 'Ease In' for transitional types, and 'Ease Out' for dynamic effects)"},

      {BEZT_IPO_EASE_IN,
       "EASE_IN",
       ICON_IPO_EASE_IN,
       "Ease In",
       "Only on the end closest to the next keyframe"},
      {BEZT_IPO_EASE_OUT,
       "EASE_OUT",
       ICON_IPO_EASE_OUT,
       "Ease Out",
       "Only on the end closest to the first keyframe"},
      {BEZT_IPO_EASE_IN_OUT,
       "EASE_IN_OUT",
       ICON_IPO_EASE_IN_OUT,
       "Ease In and Out",
       "Segment between both keyframes"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem flip_modes[] = {
      {GP_INTERPOLATE_NOFLIP, "NOFLIP", 0, "No Flip", ""},
      {GP_INTERPOLATE_FLIP, "FLIP", 0, "Flip", ""},
      {GP_INTERPOLATE_FLIPAUTO, "AUTO", 0, "Automatic", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Interpolate Sequence";
  ot->idname = "GPENCIL_OT_interpolate_sequence";
  ot->description = "Generate 'in-betweens' to smoothly interpolate between Grease Pencil frames";

  /* api callbacks */
  ot->exec = gpencil_interpolate_seq_exec;
  ot->poll = gpencil_view3d_poll;
  ot->ui = gpencil_interpolate_seq_ui;

  RNA_def_int(ot->srna,
              "step",
              1,
              1,
              MAXFRAME,
              "Step",
              "Number of frames between generated interpolated frames",
              1,
              MAXFRAME);

  RNA_def_enum(ot->srna,
               "layers",
               gpencil_interpolation_layer_items,
               0,
               "Layer",
               "Layers included in the interpolation");

  RNA_def_boolean(ot->srna,
                  "interpolate_selected_only",
                  0,
                  "Only Selected",
                  "Interpolate only selected strokes");

  RNA_def_enum(ot->srna,
               "flip",
               flip_modes,
               GP_INTERPOLATE_FLIPAUTO,
               "Flip Mode",
               "Invert destination stroke to match start and end with source stroke");

  RNA_def_int(ot->srna,
              "smooth_steps",
              1,
              1,
              3,
              "Iterations",
              "Number of times to smooth newly created strokes",
              1,
              3);

  RNA_def_float(ot->srna,
                "smooth_factor",
                0.0f,
                0.0f,
                2.0f,
                "Smooth",
                "Amount of smoothing to apply to interpolated strokes, to reduce jitter/noise",
                0.0f,
                2.0f);

  RNA_def_enum(ot->srna,
               "type",
               gpencil_interpolation_type_items,
               0,
               "Type",
               "Interpolation method to use the next time 'Interpolate Sequence' is run");

  RNA_def_enum(
      ot->srna,
      "easing",
      gpencil_interpolation_easing_items,
      0,
      "Easing",
      "Which ends of the segment between the preceding and following grease pencil frames "
      "easing interpolation is applied to");

  RNA_def_float(ot->srna,
                "back",
                1.702f,
                0.0f,
                FLT_MAX,
                "Back",
                "Amount of overshoot for 'back' easing",
                0.0f,
                FLT_MAX);

  RNA_def_float(ot->srna,
                "amplitude",
                0.15f,
                0.0f,
                FLT_MAX,
                "Amplitude",
                "Amount to boost elastic bounces for 'elastic' easing",
                0.0f,
                FLT_MAX);

  RNA_def_float(ot->srna,
                "period",
                0.15f,
                -FLT_MAX,
                FLT_MAX,
                "Period",
                "Time between bounces for elastic easing",
                -FLT_MAX,
                FLT_MAX);

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Remove Breakdowns ************************ */

static bool gpencil_interpolate_reverse_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area == NULL) {
    return false;
  }
  if ((area->spacetype != SPACE_VIEW3D) && (area->spacetype != SPACE_ACTION)) {
    return false;
  }

  bGPdata *gpd = ED_gpencil_data_get_active(C);
  if (gpd == NULL) {
    return false;
  }
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  if (gpl == NULL) {
    return false;
  }

  /* need to be on a breakdown frame */
  if ((gpl->actframe == NULL) || (gpl->actframe->key_type != BEZT_KEYTYPE_BREAKDOWN)) {
    CTX_wm_operator_poll_msg_set(C, "Expected current frame to be a breakdown");
    return false;
  }

  return true;
}

static int gpencil_interpolate_reverse_exec(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* Go through each layer, deleting the breakdowns around the current frame,
   * but only if there is a keyframe nearby to stop at
   */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* only editable and visible layers are considered */
    if (!BKE_gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }
    bGPDframe *start_key = NULL;
    bGPDframe *end_key = NULL;
    bGPDframe *gpf, *gpfn;

    /* Only continue if we're currently on a breakdown keyframe */
    if ((gpl->actframe == NULL) || (gpl->actframe->key_type != BEZT_KEYTYPE_BREAKDOWN)) {
      continue;
    }

    /* Search left for "start_key" (i.e. the first breakdown to remove) */
    gpf = gpl->actframe;
    while (gpf) {
      if (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN) {
        /* A breakdown... keep going left */
        start_key = gpf;
        gpf = gpf->prev;
      }
      else {
        /* Not a breakdown (may be a key, or an extreme,
         * or something else that wasn't generated)... stop */
        break;
      }
    }

    /* Search right for "end_key" (i.e. the last breakdown to remove) */
    gpf = gpl->actframe;
    while (gpf) {
      if (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN) {
        /* A breakdown... keep going right */
        end_key = gpf;
        gpf = gpf->next;
      }
      else {
        /* Not a breakdown... stop */
        break;
      }
    }

    /* Did we find anything? */
    /* NOTE: We should only proceed if there's something before/after these extents...
     * Otherwise, there's just an extent of breakdowns with no keys to interpolate between
     */
    if ((start_key && end_key) && ELEM(NULL, start_key->prev, end_key->next) == false) {
      /* Set actframe to the key before start_key, since the keys have been removed now */
      gpl->actframe = start_key->prev;

      /* Free each frame we're removing (except the last one) */
      for (gpf = start_key; gpf && gpf != end_key; gpf = gpfn) {
        gpfn = gpf->next;

        /* free strokes and their associated memory */
        BKE_gpencil_free_strokes(gpf);
        BLI_freelinkN(&gpl->frames, gpf);
      }

      /* Now free the last one... */
      BKE_gpencil_free_strokes(end_key);
      BLI_freelinkN(&gpl->frames, end_key);
    }
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_interpolate_reverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Breakdowns";
  ot->idname = "GPENCIL_OT_interpolate_reverse";
  ot->description =
      "Remove breakdown frames generated by interpolating between two Grease Pencil frames";

  /* callbacks */
  ot->exec = gpencil_interpolate_reverse_exec;
  ot->poll = gpencil_interpolate_reverse_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
