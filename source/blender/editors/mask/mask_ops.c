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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_mask.h"

#include "DEG_depsgraph.h"

#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h" /* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_mask.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h" /* own include */

/******************** utility functions *********************/

static void mask_point_scaled_handle(/*const*/ MaskSplinePoint *point,
                                     /*const*/ eMaskWhichHandle which_handle,
                                     const float scalex,
                                     const float scaley,
                                     float handle[2])
{
  BKE_mask_point_handle(point, which_handle, handle);
  handle[0] *= scalex;
  handle[1] *= scaley;
}

MaskSplinePoint *ED_mask_point_find_nearest(const bContext *C,
                                            Mask *mask,
                                            const float normal_co[2],
                                            const float threshold,
                                            MaskLayer **masklay_r,
                                            MaskSpline **spline_r,
                                            eMaskWhichHandle *which_handle_r,
                                            float *score)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  MaskLayer *masklay;
  MaskLayer *point_masklay = NULL;
  MaskSpline *point_spline = NULL;
  MaskSplinePoint *point = NULL;
  float co[2];
  const float threshold_sq = threshold * threshold;
  float len_sq = FLT_MAX, scalex, scaley;
  eMaskWhichHandle which_handle = MASK_WHICH_HANDLE_NONE;
  int width, height;

  ED_mask_get_size(sa, &width, &height);
  ED_mask_pixelspace_factor(sa, ar, &scalex, &scaley);

  co[0] = normal_co[0] * scalex;
  co[1] = normal_co[1] * scaley;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

      int i;

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *cur_point = &spline->points[i];
        MaskSplinePoint *cur_point_deform = &points_array[i];
        eMaskWhichHandle cur_which_handle = MASK_WHICH_HANDLE_NONE;
        BezTriple *bezt = &cur_point_deform->bezt;
        float cur_len_sq, vec[2];

        vec[0] = bezt->vec[1][0] * scalex;
        vec[1] = bezt->vec[1][1] * scaley;

        cur_len_sq = len_squared_v2v2(co, vec);

        if (cur_len_sq < len_sq) {
          point_spline = spline;
          point_masklay = masklay;
          point = cur_point;
          len_sq = cur_len_sq;
          which_handle = MASK_WHICH_HANDLE_NONE;
        }

        if (BKE_mask_point_handles_mode_get(cur_point_deform) == MASK_HANDLE_MODE_STICK) {
          float handle[2];
          mask_point_scaled_handle(
              cur_point_deform, MASK_WHICH_HANDLE_STICK, scalex, scaley, handle);
          cur_len_sq = len_squared_v2v2(co, handle);
          cur_which_handle = MASK_WHICH_HANDLE_STICK;
        }
        else {
          float handle_left[2], handle_right[2];
          float len_left_sq, len_right_sq;
          mask_point_scaled_handle(
              cur_point_deform, MASK_WHICH_HANDLE_LEFT, scalex, scaley, handle_left);
          mask_point_scaled_handle(
              cur_point_deform, MASK_WHICH_HANDLE_RIGHT, scalex, scaley, handle_right);

          len_left_sq = len_squared_v2v2(co, handle_left);
          len_right_sq = len_squared_v2v2(co, handle_right);
          if (i == 0) {
            if (len_left_sq <= len_right_sq) {
              if (bezt->h1 != HD_VECT) {
                cur_which_handle = MASK_WHICH_HANDLE_LEFT;
                cur_len_sq = len_left_sq;
              }
            }
            else if (bezt->h2 != HD_VECT) {
              cur_which_handle = MASK_WHICH_HANDLE_RIGHT;
              cur_len_sq = len_right_sq;
            }
          }
          else {
            if (len_right_sq <= len_left_sq) {
              if (bezt->h2 != HD_VECT) {
                cur_which_handle = MASK_WHICH_HANDLE_RIGHT;
                cur_len_sq = len_right_sq;
              }
            }
            else if (bezt->h1 != HD_VECT) {
              cur_which_handle = MASK_WHICH_HANDLE_LEFT;
              cur_len_sq = len_left_sq;
            }
          }
        }

        if (cur_len_sq <= len_sq && cur_which_handle != MASK_WHICH_HANDLE_NONE) {
          point_masklay = masklay;
          point_spline = spline;
          point = cur_point;
          len_sq = cur_len_sq;
          which_handle = cur_which_handle;
        }
      }
    }
  }

  if (len_sq < threshold_sq) {
    if (masklay_r) {
      *masklay_r = point_masklay;
    }

    if (spline_r) {
      *spline_r = point_spline;
    }

    if (which_handle_r) {
      *which_handle_r = which_handle;
    }

    if (score) {
      *score = sqrtf(len_sq);
    }

    return point;
  }

  if (masklay_r) {
    *masklay_r = NULL;
  }

  if (spline_r) {
    *spline_r = NULL;
  }

  if (which_handle_r) {
    *which_handle_r = MASK_WHICH_HANDLE_NONE;
  }

  return NULL;
}

bool ED_mask_feather_find_nearest(const bContext *C,
                                  Mask *mask,
                                  const float normal_co[2],
                                  const float threshold,
                                  MaskLayer **masklay_r,
                                  MaskSpline **spline_r,
                                  MaskSplinePoint **point_r,
                                  MaskSplinePointUW **uw_r,
                                  float *score)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  MaskLayer *masklay, *point_masklay = NULL;
  MaskSpline *point_spline = NULL;
  MaskSplinePoint *point = NULL;
  MaskSplinePointUW *uw = NULL;
  const float threshold_sq = threshold * threshold;
  float len = FLT_MAX, co[2];
  float scalex, scaley;
  int width, height;

  ED_mask_get_size(sa, &width, &height);
  ED_mask_pixelspace_factor(sa, ar, &scalex, &scaley);

  co[0] = normal_co[0] * scalex;
  co[1] = normal_co[1] * scaley;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      // MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

      int i, tot_feather_point;
      float(*feather_points)[2], (*fp)[2];

      if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
        continue;
      }

      feather_points = fp = BKE_mask_spline_feather_points(spline, &tot_feather_point);

      for (i = 0; i < spline->tot_point; i++) {
        int j;
        MaskSplinePoint *cur_point = &spline->points[i];

        for (j = 0; j <= cur_point->tot_uw; j++) {
          float cur_len_sq, vec[2];

          vec[0] = (*fp)[0] * scalex;
          vec[1] = (*fp)[1] * scaley;

          cur_len_sq = len_squared_v2v2(vec, co);

          if (point == NULL || cur_len_sq < len) {
            if (j == 0) {
              uw = NULL;
            }
            else {
              uw = &cur_point->uw[j - 1];
            }

            point_masklay = masklay;
            point_spline = spline;
            point = cur_point;
            len = cur_len_sq;
          }

          fp++;
        }
      }

      MEM_freeN(feather_points);
    }
  }

  if (len < threshold_sq) {
    if (masklay_r) {
      *masklay_r = point_masklay;
    }

    if (spline_r) {
      *spline_r = point_spline;
    }

    if (point_r) {
      *point_r = point;
    }

    if (uw_r) {
      *uw_r = uw;
    }

    if (score) {
      *score = sqrtf(len);
    }

    return true;
  }

  if (masklay_r) {
    *masklay_r = NULL;
  }

  if (spline_r) {
    *spline_r = NULL;
  }

  if (point_r) {
    *point_r = NULL;
  }

  return false;
}

/******************** create new mask *********************/

Mask *ED_mask_new(bContext *C, const char *name)
{
  ScrArea *sa = CTX_wm_area(C);
  Main *bmain = CTX_data_main(C);
  Mask *mask;

  mask = BKE_mask_new(bmain, name);

  if (sa && sa->spacedata.first) {
    switch (sa->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = sa->spacedata.first;
        ED_space_clip_set_mask(C, sc, mask);
        break;
      }
      case SPACE_SEQ: {
        /* do nothing */
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = sa->spacedata.first;
        ED_space_image_set_mask(C, sima, mask);
        break;
      }
    }
  }

  return mask;
}

/* Get ative layer. Will create mask/layer to be sure there's an active layer.  */
MaskLayer *ED_mask_layer_ensure(bContext *C, bool *r_added_mask)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *mask_layer;

  if (mask == NULL) {
    /* If there's no active mask, create one. */
    mask = ED_mask_new(C, NULL);
    *r_added_mask = true;
  }

  mask_layer = BKE_mask_layer_active(mask);
  if (mask_layer == NULL) {
    /* If there's no active mask layer, create one. */
    mask_layer = BKE_mask_layer_new(mask, "");
  }

  return mask_layer;
}

static int mask_new_exec(bContext *C, wmOperator *op)
{
  char name[MAX_ID_NAME - 2];

  RNA_string_get(op->ptr, "name", name);

  ED_mask_new(C, name);

  WM_event_add_notifier(C, NC_MASK | NA_ADDED, NULL);

  return OPERATOR_FINISHED;
}

void MASK_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Mask";
  ot->description = "Create new mask";
  ot->idname = "MASK_OT_new";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = mask_new_exec;
  ot->poll = ED_operator_mask;

  /* properties */
  RNA_def_string(ot->srna, "name", NULL, MAX_ID_NAME - 2, "Name", "Name of new mask");
}

/******************** create new masklay *********************/

static int masklay_new_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  char name[MAX_ID_NAME - 2];

  RNA_string_get(op->ptr, "name", name);

  BKE_mask_layer_new(mask, name);
  mask->masklay_act = mask->masklay_tot - 1;

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return OPERATOR_FINISHED;
}

void MASK_OT_layer_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Mask Layer";
  ot->description = "Add new mask layer for masking";
  ot->idname = "MASK_OT_layer_new";

  /* api callbacks */
  ot->exec = masklay_new_exec;
  ot->poll = ED_maskedit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna, "name", NULL, MAX_ID_NAME - 2, "Name", "Name of new mask layer");
}

/******************** remove mask layer *********************/

static int masklay_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay = BKE_mask_layer_active(mask);

  if (masklay) {
    BKE_mask_layer_remove(mask, masklay);

    WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
  }

  return OPERATOR_FINISHED;
}

void MASK_OT_layer_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Mask Layer";
  ot->description = "Remove mask layer";
  ot->idname = "MASK_OT_layer_remove";

  /* api callbacks */
  ot->exec = masklay_remove_exec;
  ot->poll = ED_maskedit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** slide *********************/

enum {
  SLIDE_ACTION_NONE = 0,
  SLIDE_ACTION_POINT = 1,
  SLIDE_ACTION_HANDLE = 2,
  SLIDE_ACTION_FEATHER = 3,
  SLIDE_ACTION_SPLINE = 4,
};

typedef struct SlidePointData {
  /* Generic fields. */
  short event_invoke_type;
  int action;
  Mask *mask;
  MaskLayer *masklay;
  MaskSpline *spline, *orig_spline;
  MaskSplinePoint *point;
  MaskSplinePointUW *uw;
  eMaskWhichHandle which_handle;
  int width, height;

  float prev_mouse_coord[2];
  float no[2];

  bool is_curvature_only, is_accurate, is_initial_feather, is_overall_feather;

  bool is_sliding_new_point;

  /* Data needed to restre the state. */
  float vec[3][3];
  char old_h1, old_h2;

  /* Point sliding. */

  /* Handle sliding. */
  float orig_handle_coord[2], prev_handle_coord[2];

  /* Feather sliding. */
  float prev_feather_coord[2];
  float weight, weight_scalar;
} SlidePointData;

static void mask_point_undistort_pos(SpaceClip *sc, float r_co[2], const float co[2])
{
  BKE_mask_coord_to_movieclip(sc->clip, &sc->user, r_co, co);
  ED_clip_point_undistorted_pos(sc, r_co, r_co);
  BKE_mask_coord_from_movieclip(sc->clip, &sc->user, r_co, r_co);
}

static bool spline_under_mouse_get(const bContext *C,
                                   Mask *mask,
                                   const float co[2],
                                   MaskLayer **mask_layer_r,
                                   MaskSpline **mask_spline_r)
{
  const float threshold = 19.0f;
  ScrArea *sa = CTX_wm_area(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MaskLayer *mask_layer;
  int width, height;
  float pixel_co[2];
  float closest_dist_squared = 0.0f;
  MaskLayer *closest_layer = NULL;
  MaskSpline *closest_spline = NULL;
  bool undistort = false;
  *mask_layer_r = NULL;
  *mask_spline_r = NULL;
  ED_mask_get_size(sa, &width, &height);
  pixel_co[0] = co[0] * width;
  pixel_co[1] = co[1] * height;
  if (sc != NULL) {
    undistort = (sc->clip != NULL) && (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) != 0;
  }
  for (mask_layer = mask->masklayers.first; mask_layer != NULL; mask_layer = mask_layer->next) {
    MaskSpline *spline;
    if (mask_layer->restrictflag & MASK_RESTRICT_SELECT) {
      continue;
    }

    for (spline = mask_layer->splines.first; spline != NULL; spline = spline->next) {
      MaskSplinePoint *points_array;
      float min[2], max[2], center[2];
      float dist_squared;
      int i;
      float max_bb_side;
      if ((spline->flag & SELECT) == 0) {
        continue;
      }

      points_array = BKE_mask_spline_point_array(spline);
      INIT_MINMAX2(min, max);
      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point_deform = &points_array[i];
        BezTriple *bezt = &point_deform->bezt;

        float vert[2];

        copy_v2_v2(vert, bezt->vec[1]);

        if (undistort) {
          mask_point_undistort_pos(sc, vert, vert);
        }

        minmax_v2v2_v2(min, max, vert);
      }

      center[0] = (min[0] + max[0]) / 2.0f * width;
      center[1] = (min[1] + max[1]) / 2.0f * height;
      dist_squared = len_squared_v2v2(pixel_co, center);
      max_bb_side = min_ff((max[0] - min[0]) * width, (max[1] - min[1]) * height);
      if (dist_squared <= max_bb_side * max_bb_side * 0.5f &&
          (closest_spline == NULL || dist_squared < closest_dist_squared)) {
        closest_layer = mask_layer;
        closest_spline = spline;
        closest_dist_squared = dist_squared;
      }
    }
  }
  if (closest_dist_squared < SQUARE(threshold) && closest_spline != NULL) {
    float diff_score;
    if (ED_mask_find_nearest_diff_point(C,
                                        mask,
                                        co,
                                        threshold,
                                        false,
                                        NULL,
                                        true,
                                        false,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        &diff_score)) {
      if (SQUARE(diff_score) < closest_dist_squared) {
        return false;
      }
    }

    *mask_layer_r = closest_layer;
    *mask_spline_r = closest_spline;
    return true;
  }
  return false;
}

static bool slide_point_check_initial_feather(MaskSpline *spline)
{
  int i;

  for (i = 0; i < spline->tot_point; i++) {
    MaskSplinePoint *point = &spline->points[i];

    if (point->bezt.weight != 0.0f) {
      return false;
    }
  }

  return true;
}

static void select_sliding_point(Mask *mask,
                                 MaskLayer *mask_layer,
                                 MaskSpline *spline,
                                 MaskSplinePoint *point,
                                 eMaskWhichHandle which_handle)
{
  ED_mask_select_toggle_all(mask, SEL_DESELECT);

  switch (which_handle) {
    case MASK_WHICH_HANDLE_NONE:
      BKE_mask_point_select_set(point, true);
      break;
    case MASK_WHICH_HANDLE_LEFT:
      point->bezt.f1 |= SELECT;
      break;
    case MASK_WHICH_HANDLE_RIGHT:
      point->bezt.f3 |= SELECT;
      break;
    case MASK_WHICH_HANDLE_STICK:
      point->bezt.f1 |= SELECT;
      point->bezt.f3 |= SELECT;
      break;
    default:
      BLI_assert(!"Unexpected situation in select_sliding_point()");
  }

  mask_layer->act_spline = spline;
  mask_layer->act_point = point;
  ED_mask_select_flush_all(mask);
}

static void check_sliding_handle_type(MaskSplinePoint *point, eMaskWhichHandle which_handle)
{
  BezTriple *bezt = &point->bezt;

  if (which_handle == MASK_WHICH_HANDLE_LEFT) {
    if (bezt->h1 == HD_VECT) {
      bezt->h1 = HD_FREE;
    }
    else if (bezt->h1 == HD_AUTO) {
      bezt->h1 = HD_ALIGN_DOUBLESIDE;
      bezt->h2 = HD_ALIGN_DOUBLESIDE;
    }
  }
  else if (which_handle == MASK_WHICH_HANDLE_RIGHT) {
    if (bezt->h2 == HD_VECT) {
      bezt->h2 = HD_FREE;
    }
    else if (bezt->h2 == HD_AUTO) {
      bezt->h1 = HD_ALIGN_DOUBLESIDE;
      bezt->h2 = HD_ALIGN_DOUBLESIDE;
    }
  }
}

static void *slide_point_customdata(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  Mask *mask = CTX_data_edit_mask(C);
  SlidePointData *customdata = NULL;
  MaskLayer *masklay, *cv_masklay, *feather_masklay;
  MaskSpline *spline, *cv_spline, *feather_spline;
  MaskSplinePoint *point, *cv_point, *feather_point;
  MaskSplinePointUW *uw = NULL;
  int width, height, action = SLIDE_ACTION_NONE;
  const bool slide_feather = RNA_boolean_get(op->ptr, "slide_feather");
  float co[2], cv_score, feather_score;
  const float threshold = 19;
  eMaskWhichHandle which_handle;

  ED_mask_mouse_pos(sa, ar, event->mval, co);
  ED_mask_get_size(sa, &width, &height);

  cv_point = ED_mask_point_find_nearest(
      C, mask, co, threshold, &cv_masklay, &cv_spline, &which_handle, &cv_score);

  if (ED_mask_feather_find_nearest(C,
                                   mask,
                                   co,
                                   threshold,
                                   &feather_masklay,
                                   &feather_spline,
                                   &feather_point,
                                   &uw,
                                   &feather_score)) {
    if (slide_feather || !cv_point || feather_score < cv_score) {
      action = SLIDE_ACTION_FEATHER;

      masklay = feather_masklay;
      spline = feather_spline;
      point = feather_point;
    }
  }

  if (cv_point && action == SLIDE_ACTION_NONE) {
    if (which_handle != MASK_WHICH_HANDLE_NONE) {
      action = SLIDE_ACTION_HANDLE;
    }
    else {
      action = SLIDE_ACTION_POINT;
    }

    masklay = cv_masklay;
    spline = cv_spline;
    point = cv_point;
  }

  if (action == SLIDE_ACTION_NONE) {
    if (spline_under_mouse_get(C, mask, co, &masklay, &spline)) {
      action = SLIDE_ACTION_SPLINE;
      point = NULL;
    }
  }

  if (action != SLIDE_ACTION_NONE) {
    customdata = MEM_callocN(sizeof(SlidePointData), "mask slide point data");
    customdata->event_invoke_type = event->type;
    customdata->mask = mask;
    customdata->masklay = masklay;
    customdata->spline = spline;
    customdata->point = point;
    customdata->width = width;
    customdata->height = height;
    customdata->action = action;
    customdata->uw = uw;

    customdata->is_sliding_new_point = RNA_boolean_get(op->ptr, "is_new_point");

    if (customdata->action != SLIDE_ACTION_SPLINE) {
      customdata->old_h1 = point->bezt.h1;
      customdata->old_h2 = point->bezt.h2;
      select_sliding_point(mask, masklay, spline, point, which_handle);
      check_sliding_handle_type(point, which_handle);
    }

    if (uw) {
      float co_uw[2];
      float weight_scalar = BKE_mask_point_weight_scalar(spline, point, uw->u);

      customdata->weight = uw->w;
      customdata->weight_scalar = weight_scalar;
      BKE_mask_point_segment_co(spline, point, uw->u, co_uw);
      BKE_mask_point_normal(spline, point, uw->u, customdata->no);

      madd_v2_v2v2fl(customdata->prev_feather_coord, co_uw, customdata->no, uw->w * weight_scalar);
    }
    else if (customdata->action != SLIDE_ACTION_SPLINE) {
      BezTriple *bezt = &point->bezt;

      customdata->weight = bezt->weight;
      customdata->weight_scalar = 1.0f;
      BKE_mask_point_normal(spline, point, 0.0f, customdata->no);

      madd_v2_v2v2fl(customdata->prev_feather_coord, bezt->vec[1], customdata->no, bezt->weight);
    }

    if (customdata->action == SLIDE_ACTION_FEATHER) {
      customdata->is_initial_feather = slide_point_check_initial_feather(spline);
    }

    if (customdata->action != SLIDE_ACTION_SPLINE) {
      copy_m3_m3(customdata->vec, point->bezt.vec);
      if (which_handle != MASK_WHICH_HANDLE_NONE) {
        BKE_mask_point_handle(point, which_handle, customdata->orig_handle_coord);
        copy_v2_v2(customdata->prev_handle_coord, customdata->orig_handle_coord);
      }
    }
    customdata->which_handle = which_handle;

    ED_mask_mouse_pos(sa, ar, event->mval, customdata->prev_mouse_coord);
  }

  return customdata;
}

static int slide_point_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Mask *mask = CTX_data_edit_mask(C);
  SlidePointData *slidedata;

  if (mask == NULL) {
    return OPERATOR_PASS_THROUGH;
  }

  slidedata = slide_point_customdata(C, op, event);

  if (slidedata) {
    op->customdata = slidedata;

    WM_event_add_modal_handler(C, op);

    slidedata->masklay->act_spline = slidedata->spline;
    slidedata->masklay->act_point = slidedata->point;

    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

static void slide_point_delta_all_feather(SlidePointData *data, float delta)
{
  int i;

  for (i = 0; i < data->spline->tot_point; i++) {
    MaskSplinePoint *point = &data->spline->points[i];
    MaskSplinePoint *orig_point = &data->orig_spline->points[i];

    point->bezt.weight = orig_point->bezt.weight + delta;
    if (point->bezt.weight < 0.0f) {
      point->bezt.weight = 0.0f;
    }
  }
}

static void slide_point_restore_spline(SlidePointData *data)
{
  int i;

  for (i = 0; i < data->spline->tot_point; i++) {
    MaskSplinePoint *point = &data->spline->points[i];
    MaskSplinePoint *orig_point = &data->orig_spline->points[i];
    int j;

    point->bezt = orig_point->bezt;

    for (j = 0; j < point->tot_uw; j++) {
      point->uw[j] = orig_point->uw[j];
    }
  }
}

static void cancel_slide_point(SlidePointData *data)
{
  /* cancel sliding */

  if (data->orig_spline) {
    slide_point_restore_spline(data);
  }
  else {
    if (data->action == SLIDE_ACTION_FEATHER) {
      if (data->uw) {
        data->uw->w = data->weight;
      }
      else {
        data->point->bezt.weight = data->weight;
      }
    }
    else if (data->action != SLIDE_ACTION_SPLINE) {
      copy_m3_m3(data->point->bezt.vec, data->vec);
      data->point->bezt.h1 = data->old_h1;
      data->point->bezt.h2 = data->old_h2;
    }
  }
}

static void free_slide_point_data(SlidePointData *data)
{
  if (data->orig_spline) {
    BKE_mask_spline_free(data->orig_spline);
  }

  MEM_freeN(data);
}

static int slide_point_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SlidePointData *data = (SlidePointData *)op->customdata;
  BezTriple *bezt = &data->point->bezt;
  float co[2];

  switch (event->type) {
    case LEFTALTKEY:
    case RIGHTALTKEY:
    case LEFTSHIFTKEY:
    case RIGHTSHIFTKEY:
      if (ELEM(event->type, LEFTALTKEY, RIGHTALTKEY)) {
        if (data->action == SLIDE_ACTION_FEATHER) {
          data->is_overall_feather = (event->val == KM_PRESS);
        }
        else {
          data->is_curvature_only = (event->val == KM_PRESS);
        }
      }

      if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY)) {
        data->is_accurate = (event->val == KM_PRESS);
      }

      ATTR_FALLTHROUGH; /* update CV position */
    case MOUSEMOVE: {
      ScrArea *sa = CTX_wm_area(C);
      ARegion *ar = CTX_wm_region(C);
      float delta[2];

      ED_mask_mouse_pos(sa, ar, event->mval, co);
      sub_v2_v2v2(delta, co, data->prev_mouse_coord);
      if (data->is_accurate) {
        mul_v2_fl(delta, 0.2f);
      }
      copy_v2_v2(data->prev_mouse_coord, co);

      if (data->action == SLIDE_ACTION_HANDLE) {
        float new_handle[2];

        if (data->is_sliding_new_point && data->which_handle == MASK_WHICH_HANDLE_STICK) {
          if (ELEM(data->point,
                   &data->spline->points[0],
                   &data->spline->points[data->spline->tot_point - 1])) {
            SWAP(float, delta[0], delta[1]);
            delta[1] *= -1;

            /* flip last point */
            if (data->point != &data->spline->points[0]) {
              negate_v2(delta);
            }
          }
        }

        add_v2_v2v2(new_handle, data->prev_handle_coord, delta);

        BKE_mask_point_set_handle(data->point,
                                  data->which_handle,
                                  new_handle,
                                  data->is_curvature_only,
                                  data->orig_handle_coord,
                                  data->vec);
        BKE_mask_point_handle(data->point, data->which_handle, data->prev_handle_coord);

        if (data->is_sliding_new_point) {
          if (ELEM(data->which_handle, MASK_WHICH_HANDLE_LEFT, MASK_WHICH_HANDLE_RIGHT)) {
            float vec[2];
            short self_handle = (data->which_handle == MASK_WHICH_HANDLE_LEFT) ? 0 : 2;
            short other_handle = (data->which_handle == MASK_WHICH_HANDLE_LEFT) ? 2 : 0;

            sub_v2_v2v2(vec, bezt->vec[1], bezt->vec[self_handle]);
            add_v2_v2v2(bezt->vec[other_handle], bezt->vec[1], vec);
          }
        }
      }
      else if (data->action == SLIDE_ACTION_POINT) {
        add_v2_v2(bezt->vec[0], delta);
        add_v2_v2(bezt->vec[1], delta);
        add_v2_v2(bezt->vec[2], delta);
      }
      else if (data->action == SLIDE_ACTION_FEATHER) {
        float vec[2], no[2], p[2], c[2], w, offco[2];
        float *weight = NULL;
        float weight_scalar = 1.0f;
        bool is_overall_feather = data->is_overall_feather || data->is_initial_feather;

        add_v2_v2v2(offco, data->prev_feather_coord, delta);

        if (data->uw) {
          /* project on both sides and find the closest one,
           * prevents flickering when projecting onto both sides can happen */
          const float u_pos = BKE_mask_spline_project_co(
              data->spline, data->point, data->uw->u, offco, MASK_PROJ_NEG);
          const float u_neg = BKE_mask_spline_project_co(
              data->spline, data->point, data->uw->u, offco, MASK_PROJ_POS);
          float dist_pos = FLT_MAX;
          float dist_neg = FLT_MAX;
          float co_pos[2];
          float co_neg[2];
          float u;

          if (u_pos > 0.0f && u_pos < 1.0f) {
            BKE_mask_point_segment_co(data->spline, data->point, u_pos, co_pos);
            dist_pos = len_squared_v2v2(offco, co_pos);
          }

          if (u_neg > 0.0f && u_neg < 1.0f) {
            BKE_mask_point_segment_co(data->spline, data->point, u_neg, co_neg);
            dist_neg = len_squared_v2v2(offco, co_neg);
          }

          u = dist_pos < dist_neg ? u_pos : u_neg;

          if (u > 0.0f && u < 1.0f) {
            data->uw->u = u;

            data->uw = BKE_mask_point_sort_uw(data->point, data->uw);
            weight = &data->uw->w;
            weight_scalar = BKE_mask_point_weight_scalar(data->spline, data->point, u);
            if (weight_scalar != 0.0f) {
              weight_scalar = 1.0f / weight_scalar;
            }

            BKE_mask_point_normal(data->spline, data->point, data->uw->u, no);
            BKE_mask_point_segment_co(data->spline, data->point, data->uw->u, p);
          }
        }
        else {
          weight = &bezt->weight;
          /* weight_scalar = 1.0f; keep as is */
          copy_v2_v2(no, data->no);
          copy_v2_v2(p, bezt->vec[1]);
        }

        if (weight) {
          sub_v2_v2v2(c, offco, p);
          project_v2_v2v2_normalized(vec, c, no);

          w = len_v2(vec);

          if (is_overall_feather) {
            float w_delta;

            if (dot_v2v2(no, vec) <= 0.0f) {
              w = -w;
            }

            w_delta = w - data->weight * data->weight_scalar;

            if (data->orig_spline == NULL) {
              /* restore weight for currently sliding point, so orig_spline would be created
               * with original weights used
               */
              *weight = data->weight;

              data->orig_spline = BKE_mask_spline_copy(data->spline);
            }

            if (data->is_initial_feather) {
              *weight = w * weight_scalar;
            }

            slide_point_delta_all_feather(data, w_delta);
          }
          else {
            if (dot_v2v2(no, vec) <= 0.0f) {
              w = 0.0f;
            }

            if (data->orig_spline) {
              /* restore possible overall feather changes */
              slide_point_restore_spline(data);

              BKE_mask_spline_free(data->orig_spline);
              data->orig_spline = NULL;
            }

            if (weight_scalar != 0.0f) {
              *weight = w * weight_scalar;
            }
          }

          copy_v2_v2(data->prev_feather_coord, offco);
        }
      }
      else if (data->action == SLIDE_ACTION_SPLINE) {
        int i;

        if (data->orig_spline == NULL) {
          data->orig_spline = BKE_mask_spline_copy(data->spline);
        }

        for (i = 0; i < data->spline->tot_point; i++) {
          MaskSplinePoint *point = &data->spline->points[i];
          add_v2_v2(point->bezt.vec[0], delta);
          add_v2_v2(point->bezt.vec[1], delta);
          add_v2_v2(point->bezt.vec[2], delta);
        }
      }

      WM_event_add_notifier(C, NC_MASK | NA_EDITED, data->mask);
      DEG_id_tag_update(&data->mask->id, 0);

      break;
    }

    case LEFTMOUSE:
    case RIGHTMOUSE:
      if (event->type == data->event_invoke_type && event->val == KM_RELEASE) {
        Scene *scene = CTX_data_scene(C);

        /* dont key sliding feather uw's */
        if ((data->action == SLIDE_ACTION_FEATHER && data->uw) == false) {
          if (IS_AUTOKEY_ON(scene)) {
            ED_mask_layer_shape_auto_key(data->masklay, CFRA);
          }
        }

        if (data->is_sliding_new_point) {
          if (len_squared_v2v2(bezt->vec[0], bezt->vec[1]) < FLT_EPSILON) {
            bezt->h1 = HD_VECT;
          }
          if (len_squared_v2v2(bezt->vec[2], bezt->vec[1]) < FLT_EPSILON) {
            bezt->h2 = HD_VECT;
          }
        }

        WM_event_add_notifier(C, NC_MASK | NA_EDITED, data->mask);
        DEG_id_tag_update(&data->mask->id, 0);

        free_slide_point_data(op->customdata); /* keep this last! */
        return OPERATOR_FINISHED;
      }
      else if (event->type != data->event_invoke_type && event->val == KM_PRESS) {
        /* pass to ESCKEY */
      }
      else {
        break;
      }

    case ESCKEY:
      cancel_slide_point(op->customdata);

      WM_event_add_notifier(C, NC_MASK | NA_EDITED, data->mask);
      DEG_id_tag_update(&data->mask->id, 0);

      free_slide_point_data(op->customdata); /* keep this last! */
      return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

void MASK_OT_slide_point(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Slide Point";
  ot->description = "Slide control points";
  ot->idname = "MASK_OT_slide_point";

  /* api callbacks */
  ot->invoke = slide_point_invoke;
  ot->modal = slide_point_modal;
  ot->poll = ED_operator_mask;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "slide_feather",
                  0,
                  "Slide Feather",
                  "First try to slide feather instead of vertex");

  prop = RNA_def_boolean(
      ot->srna, "is_new_point", 0, "Slide New Point", "Newly created vertex is being slid");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/******************** slide spline curvature *********************/

typedef struct SlideSplineCurvatureData {
  short event_invoke_type;

  Mask *mask;
  MaskLayer *mask_layer;
  MaskSpline *spline;
  MaskSplinePoint *point;
  float u;
  bool accurate;

  BezTriple *adjust_bezt, *other_bezt;
  BezTriple bezt_backup, other_bezt_backup;

  float prev_mouse_coord[2];
  float prev_spline_coord[2];

  float P0[2], P1[2], P2[2], P3[3];
} SlideSplineCurvatureData;

static void cancel_slide_spline_curvature(SlideSplineCurvatureData *slide_data)
{
  *slide_data->adjust_bezt = slide_data->bezt_backup;
  *slide_data->other_bezt = slide_data->other_bezt_backup;
}

static void free_slide_spline_curvature_data(SlideSplineCurvatureData *slide_data)
{
  MEM_freeN(slide_data);
}

static bool slide_spline_curvature_check(bContext *C, const wmEvent *event)
{
  Mask *mask = CTX_data_edit_mask(C);
  float co[2];
  const float threshold = 19.0f;

  ED_mask_mouse_pos(CTX_wm_area(C), CTX_wm_region(C), event->mval, co);

  if (ED_mask_point_find_nearest(C, mask, co, threshold, NULL, NULL, NULL, NULL)) {
    return false;
  }

  if (ED_mask_feather_find_nearest(C, mask, co, threshold, NULL, NULL, NULL, NULL, NULL)) {
    return false;
  }

  return true;
}

static SlideSplineCurvatureData *slide_spline_curvature_customdata(bContext *C,
                                                                   const wmEvent *event)
{
  const float threshold = 19.0f;

  Mask *mask = CTX_data_edit_mask(C);
  SlideSplineCurvatureData *slide_data;
  MaskLayer *mask_layer;
  MaskSpline *spline;
  MaskSplinePoint *point;
  float u, co[2];
  BezTriple *next_bezt;

  ED_mask_mouse_pos(CTX_wm_area(C), CTX_wm_region(C), event->mval, co);

  if (!ED_mask_find_nearest_diff_point(C,
                                       mask,
                                       co,
                                       threshold,
                                       false,
                                       NULL,
                                       true,
                                       false,
                                       &mask_layer,
                                       &spline,
                                       &point,
                                       &u,
                                       NULL)) {
    return NULL;
  }

  next_bezt = BKE_mask_spline_point_next_bezt(spline, spline->points, point);
  if (next_bezt == NULL) {
    return NULL;
  }

  slide_data = MEM_callocN(sizeof(SlideSplineCurvatureData), "slide curvature slide");
  slide_data->event_invoke_type = event->type;
  slide_data->mask = mask;
  slide_data->mask_layer = mask_layer;
  slide_data->spline = spline;
  slide_data->point = point;
  slide_data->u = u;

  copy_v2_v2(slide_data->prev_mouse_coord, co);
  BKE_mask_point_segment_co(spline, point, u, slide_data->prev_spline_coord);

  copy_v2_v2(slide_data->P0, point->bezt.vec[1]);
  copy_v2_v2(slide_data->P1, point->bezt.vec[2]);
  copy_v2_v2(slide_data->P2, next_bezt->vec[0]);
  copy_v2_v2(slide_data->P3, next_bezt->vec[1]);

  /* Depending to which end we're closer to adjust either left or right side of the spline. */
  if (u <= 0.5f) {
    slide_data->adjust_bezt = &point->bezt;
    slide_data->other_bezt = next_bezt;
  }
  else {
    slide_data->adjust_bezt = next_bezt;
    slide_data->other_bezt = &point->bezt;
  }

  /* Data needed for restoring state. */
  slide_data->bezt_backup = *slide_data->adjust_bezt;
  slide_data->other_bezt_backup = *slide_data->other_bezt;

  /* Let's dont touch other side of the point for now, so set handle to FREE. */
  if (u < 0.5f) {
    if (slide_data->adjust_bezt->h2 <= HD_VECT) {
      slide_data->adjust_bezt->h2 = HD_FREE;
    }
  }
  else {
    if (slide_data->adjust_bezt->h1 <= HD_VECT) {
      slide_data->adjust_bezt->h1 = HD_FREE;
    }
  }

  /* Change selection */
  ED_mask_select_toggle_all(mask, SEL_DESELECT);
  slide_data->adjust_bezt->f2 |= SELECT;
  slide_data->other_bezt->f2 |= SELECT;
  if (u < 0.5f) {
    slide_data->adjust_bezt->f3 |= SELECT;
    slide_data->other_bezt->f1 |= SELECT;
  }
  else {
    slide_data->adjust_bezt->f1 |= SELECT;
    slide_data->other_bezt->f3 |= SELECT;
  }
  mask_layer->act_spline = spline;
  mask_layer->act_point = point;
  ED_mask_select_flush_all(mask);

  return slide_data;
}

static int slide_spline_curvature_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Mask *mask = CTX_data_edit_mask(C);
  SlideSplineCurvatureData *slide_data;

  if (mask == NULL) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Be sure we don't conflict with point slide here. */
  if (!slide_spline_curvature_check(C, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  slide_data = slide_spline_curvature_customdata(C, event);
  if (slide_data != NULL) {
    op->customdata = slide_data;
    WM_event_add_modal_handler(C, op);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);
    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

static void slide_spline_solve_P1(const float u,
                                  const float B[2],
                                  const float P0[2],
                                  const float P2[2],
                                  const float P3[2],
                                  float solution[2])
{
  const float u2 = u * u, u3 = u * u * u;
  const float v = 1.0f - u;
  const float v2 = v * v, v3 = v * v * v;
  const float inv_divider = 1.0f / (3.0f * v2 * u);
  const float t = 3.0f * v * u2;
  solution[0] = -(v3 * P0[0] + t * P2[0] + u3 * P3[0] - B[0]) * inv_divider;
  solution[1] = -(v3 * P0[1] + t * P2[1] + u3 * P3[1] - B[1]) * inv_divider;
}

static void slide_spline_solve_P2(const float u,
                                  const float B[2],
                                  const float P0[2],
                                  const float P1[2],
                                  const float P3[2],
                                  float solution[2])
{
  const float u2 = u * u, u3 = u * u * u;
  const float v = 1.0f - u;
  const float v2 = v * v, v3 = v * v * v;
  const float inv_divider = 1.0f / (3.0f * v * u2);
  const float t = 3.0f * v2 * u;
  solution[0] = -(v3 * P0[0] + t * P1[0] + u3 * P3[0] - B[0]) * inv_divider;
  solution[1] = -(v3 * P0[1] + t * P1[1] + u3 * P3[1] - B[1]) * inv_divider;
}

static int slide_spline_curvature_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  const float margin = 0.2f;
  SlideSplineCurvatureData *slide_data = (SlideSplineCurvatureData *)op->customdata;
  float u = slide_data->u;

  switch (event->type) {
    case LEFTSHIFTKEY:
    case RIGHTSHIFTKEY:
    case LEFTCTRLKEY:
    case RIGHTCTRLKEY:
      if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY)) {
        slide_data->accurate = (event->val == KM_PRESS);
      }

      if (ELEM(event->type, LEFTCTRLKEY, RIGHTCTRLKEY)) {
        if (event->val == KM_PRESS) {
          slide_data->adjust_bezt->h1 = slide_data->adjust_bezt->h2 = HD_FREE;
          if ((u > margin && u < 0.5f) || (u >= 0.5f && u < 1.0f - margin)) {
            slide_data->other_bezt->h1 = slide_data->other_bezt->h2 = HD_FREE;
          }
        }
        else if (event->val == KM_RELEASE) {
          slide_data->adjust_bezt->h1 = slide_data->bezt_backup.h1;
          slide_data->adjust_bezt->h2 = slide_data->bezt_backup.h2;
          slide_data->other_bezt->h1 = slide_data->other_bezt_backup.h1;
          slide_data->other_bezt->h2 = slide_data->other_bezt_backup.h2;
        }

        if (u < 0.5f) {
          copy_v2_v2(slide_data->adjust_bezt->vec[0], slide_data->bezt_backup.vec[0]);
          copy_v2_v2(slide_data->other_bezt->vec[2], slide_data->other_bezt_backup.vec[2]);
        }
        else {
          copy_v2_v2(slide_data->adjust_bezt->vec[2], slide_data->bezt_backup.vec[2]);
          copy_v2_v2(slide_data->other_bezt->vec[0], slide_data->other_bezt_backup.vec[0]);
        }
      }

      ATTR_FALLTHROUGH; /* update CV position */
    case MOUSEMOVE: {
      float B[2], mouse_coord[2], delta[2];

      /* Get coordinate spline is expected to go through. */
      ED_mask_mouse_pos(CTX_wm_area(C), CTX_wm_region(C), event->mval, mouse_coord);
      sub_v2_v2v2(delta, mouse_coord, slide_data->prev_mouse_coord);
      if (slide_data->accurate) {
        mul_v2_fl(delta, 0.2f);
      }
      add_v2_v2v2(B, slide_data->prev_spline_coord, delta);
      copy_v2_v2(slide_data->prev_spline_coord, B);
      copy_v2_v2(slide_data->prev_mouse_coord, mouse_coord);

      if (u < 0.5f) {
        float oldP2[2];
        bool need_restore_P2 = false;

        if (u > margin) {
          float solution[2];
          float x = (u - margin) * 0.5f / (0.5f - margin);
          float weight = (3 * x * x - 2 * x * x * x);

          slide_spline_solve_P2(u, B, slide_data->P0, slide_data->P1, slide_data->P3, solution);

          copy_v2_v2(oldP2, slide_data->P2);
          interp_v2_v2v2(slide_data->P2, slide_data->P2, solution, weight);
          copy_v2_v2(slide_data->other_bezt->vec[0], slide_data->P2);
          need_restore_P2 = true;

          /* Tweak handle type in order to be able to apply the delta. */
          if (weight > 0.0f) {
            if (slide_data->other_bezt->h1 <= HD_VECT) {
              slide_data->other_bezt->h1 = HD_FREE;
            }
          }
        }

        slide_spline_solve_P1(
            u, B, slide_data->P0, slide_data->P2, slide_data->P3, slide_data->adjust_bezt->vec[2]);

        if (need_restore_P2) {
          copy_v2_v2(slide_data->P2, oldP2);
        }
      }
      else {
        float oldP1[2];
        bool need_restore_P1 = false;

        if (u < 1.0f - margin) {
          float solution[2];
          float x = ((1.0f - u) - margin) * 0.5f / (0.5f - margin);
          float weight = 3 * x * x - 2 * x * x * x;

          slide_spline_solve_P1(u, B, slide_data->P0, slide_data->P2, slide_data->P3, solution);

          copy_v2_v2(oldP1, slide_data->P1);
          interp_v2_v2v2(slide_data->P1, slide_data->P1, solution, weight);
          copy_v2_v2(slide_data->other_bezt->vec[2], slide_data->P1);
          need_restore_P1 = true;

          /* Tweak handle type in order to be able to apply the delta. */
          if (weight > 0.0f) {
            if (slide_data->other_bezt->h2 <= HD_VECT) {
              slide_data->other_bezt->h2 = HD_FREE;
            }
          }
        }

        slide_spline_solve_P2(
            u, B, slide_data->P0, slide_data->P1, slide_data->P3, slide_data->adjust_bezt->vec[0]);

        if (need_restore_P1) {
          copy_v2_v2(slide_data->P1, oldP1);
        }
      }

      WM_event_add_notifier(C, NC_MASK | NA_EDITED, slide_data->mask);
      DEG_id_tag_update(&slide_data->mask->id, 0);

      break;
    }

    case LEFTMOUSE:
    case RIGHTMOUSE:
      if (event->type == slide_data->event_invoke_type && event->val == KM_RELEASE) {
        /* dont key sliding feather uw's */
        if (IS_AUTOKEY_ON(scene)) {
          ED_mask_layer_shape_auto_key(slide_data->mask_layer, CFRA);
        }

        WM_event_add_notifier(C, NC_MASK | NA_EDITED, slide_data->mask);
        DEG_id_tag_update(&slide_data->mask->id, 0);

        free_slide_spline_curvature_data(slide_data); /* keep this last! */
        return OPERATOR_FINISHED;
      }

      break;

    case ESCKEY:
      cancel_slide_spline_curvature(slide_data);

      WM_event_add_notifier(C, NC_MASK | NA_EDITED, slide_data->mask);
      DEG_id_tag_update(&slide_data->mask->id, 0);

      free_slide_spline_curvature_data(op->customdata); /* keep this last! */
      return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

void MASK_OT_slide_spline_curvature(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Slide Spline Curvature";
  ot->description = "Slide a point on the spline to define it's curvature";
  ot->idname = "MASK_OT_slide_spline_curvature";

  /* api callbacks */
  ot->invoke = slide_spline_curvature_invoke;
  ot->modal = slide_spline_curvature_modal;
  ot->poll = ED_operator_mask;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** toggle cyclic *********************/

static int cyclic_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      if (ED_mask_spline_select_check(spline)) {
        spline->flag ^= MASK_SPLINE_CYCLIC;
      }
    }
  }

  DEG_id_tag_update(&mask->id, 0);
  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return OPERATOR_FINISHED;
}

void MASK_OT_cyclic_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Cyclic";
  ot->description = "Toggle cyclic for selected splines";
  ot->idname = "MASK_OT_cyclic_toggle";

  /* api callbacks */
  ot->exec = cyclic_toggle_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** delete *********************/

static void delete_feather_points(MaskSplinePoint *point)
{
  int i, count = 0;

  if (!point->tot_uw) {
    return;
  }

  for (i = 0; i < point->tot_uw; i++) {
    if ((point->uw[i].flag & SELECT) == 0) {
      count++;
    }
  }

  if (count == 0) {
    MEM_freeN(point->uw);
    point->uw = NULL;
    point->tot_uw = 0;
  }
  else {
    MaskSplinePointUW *new_uw;
    int j = 0;

    new_uw = MEM_callocN(count * sizeof(MaskSplinePointUW), "new mask uw points");

    for (i = 0; i < point->tot_uw; i++) {
      if ((point->uw[i].flag & SELECT) == 0) {
        new_uw[j++] = point->uw[i];
      }
    }

    MEM_freeN(point->uw);

    point->uw = new_uw;
    point->tot_uw = count;
  }
}

static int delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  bool changed = false;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;
    int mask_layer_shape_ofs = 0;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    spline = masklay->splines.first;

    while (spline) {
      const int tot_point_orig = spline->tot_point;
      int i, count = 0;
      MaskSpline *next_spline = spline->next;

      /* count unselected points */
      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (!MASKPOINT_ISSEL_ANY(point)) {
          count++;
        }
      }

      if (count == 0) {
        /* delete the whole spline */
        BLI_remlink(&masklay->splines, spline);
        BKE_mask_spline_free(spline);

        if (spline == masklay->act_spline) {
          masklay->act_spline = NULL;
          masklay->act_point = NULL;
        }

        BKE_mask_layer_shape_changed_remove(masklay, mask_layer_shape_ofs, tot_point_orig);
      }
      else {
        MaskSplinePoint *new_points;
        int j;

        new_points = MEM_callocN(count * sizeof(MaskSplinePoint), "deleteMaskPoints");

        for (i = 0, j = 0; i < tot_point_orig; i++) {
          MaskSplinePoint *point = &spline->points[i];

          if (!MASKPOINT_ISSEL_ANY(point)) {
            if (point == masklay->act_point) {
              masklay->act_point = &new_points[j];
            }

            delete_feather_points(point);

            new_points[j] = *point;
            j++;
          }
          else {
            if (point == masklay->act_point) {
              masklay->act_point = NULL;
            }

            BKE_mask_point_free(point);
            spline->tot_point--;

            BKE_mask_layer_shape_changed_remove(masklay, mask_layer_shape_ofs + j, 1);
          }
        }

        mask_layer_shape_ofs += spline->tot_point;

        MEM_freeN(spline->points);
        spline->points = new_points;

        ED_mask_select_flush_all(mask);
      }

      changed = true;
      spline = next_spline;
    }

    /* not essential but confuses users when there are keys with no data!
     * assume if they delete all data from the layer they also dont care about keys */
    if (BLI_listbase_is_empty(&masklay->splines)) {
      BKE_mask_layer_free_shapes(masklay);
    }
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  /* TODO: only update edited splines */
  BKE_mask_update_display(mask, CFRA);

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return OPERATOR_FINISHED;
}

void MASK_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Delete selected control points or splines";
  ot->idname = "MASK_OT_delete";

  /* api callbacks */
  ot->invoke = WM_operator_confirm;
  ot->exec = delete_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *** switch direction *** */
static int mask_switch_direction_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;

  bool changed = false;

  /* do actual selection */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;
    bool changed_layer = false;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      if (ED_mask_spline_select_check(spline)) {
        BKE_mask_spline_direction_switch(masklay, spline);
        changed = true;
        changed_layer = true;
      }
    }

    if (changed_layer) {
      if (IS_AUTOKEY_ON(scene)) {
        ED_mask_layer_shape_auto_key(masklay, CFRA);
      }
    }
  }

  if (changed) {
    /* TODO: only update this spline */
    BKE_mask_update_display(mask, CFRA);

    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);
    WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void MASK_OT_switch_direction(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Switch Direction";
  ot->description = "Switch direction of selected splines";
  ot->idname = "MASK_OT_switch_direction";

  /* api callbacks */
  ot->exec = mask_switch_direction_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *** recalc normals *** */
static int mask_normals_make_consistent_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  int i;

  bool changed = false;

  /* do actual selection */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;
    bool changed_layer = false;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(point)) {
          BKE_mask_calc_handle_point_auto(spline, point, false);
          changed = true;
          changed_layer = true;
        }
      }
    }

    if (changed_layer) {
      if (IS_AUTOKEY_ON(scene)) {
        ED_mask_layer_shape_auto_key(masklay, CFRA);
      }
    }
  }

  if (changed) {
    /* TODO: only update this spline */
    BKE_mask_update_display(mask, CFRA);

    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);
    WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

/* named to match mesh recalc normals */
void MASK_OT_normals_make_consistent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Recalc Normals";
  ot->description = "Re-calculate the direction of selected handles";
  ot->idname = "MASK_OT_normals_make_consistent";

  /* api callbacks */
  ot->exec = mask_normals_make_consistent_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** set handle type *********************/

static int set_handle_type_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  int handle_type = RNA_enum_get(op->ptr, "type");

  bool changed = false;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;
    int i;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(point)) {
          BezTriple *bezt = &point->bezt;

          if (bezt->f2 & SELECT) {
            bezt->h1 = handle_type;
            bezt->h2 = handle_type;
          }
          else {
            if (bezt->f1 & SELECT) {
              bezt->h1 = handle_type;
            }
            if (bezt->f3 & SELECT) {
              bezt->h2 = handle_type;
            }
          }

          if (handle_type == HD_ALIGN) {
            float vec[3];
            sub_v3_v3v3(vec, bezt->vec[0], bezt->vec[1]);
            add_v3_v3v3(bezt->vec[2], bezt->vec[1], vec);
          }

          changed = true;
        }
      }
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_MASK | ND_DATA, mask);
    DEG_id_tag_update(&mask->id, 0);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void MASK_OT_handle_type_set(wmOperatorType *ot)
{
  static const EnumPropertyItem editcurve_handle_type_items[] = {
      {HD_AUTO, "AUTO", 0, "Auto", ""},
      {HD_VECT, "VECTOR", 0, "Vector", ""},
      {HD_ALIGN, "ALIGNED", 0, "Aligned Single", ""},
      {HD_ALIGN_DOUBLESIDE, "ALIGNED_DOUBLESIDE", 0, "Aligned", ""},
      {HD_FREE, "FREE", 0, "Free", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set Handle Type";
  ot->description = "Set type of handles for selected control points";
  ot->idname = "MASK_OT_handle_type_set";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = set_handle_type_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", editcurve_handle_type_items, 1, "Type", "Spline type");
}

/* ********* clear/set restrict view *********/
static int mask_hide_view_clear_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  bool changed = false;
  const bool select = RNA_boolean_get(op->ptr, "select");

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {

    if (masklay->restrictflag & OB_RESTRICT_VIEW) {
      ED_mask_layer_select_set(masklay, select);
      masklay->restrictflag &= ~OB_RESTRICT_VIEW;
      changed = true;
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_MASK | ND_DRAW, mask);
    DEG_id_tag_update(&mask->id, 0);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void MASK_OT_hide_view_clear(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Clear Restrict View";
  ot->description = "Reveal the layer by setting the hide flag";
  ot->idname = "MASK_OT_hide_view_clear";

  /* api callbacks */
  ot->exec = mask_hide_view_clear_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

static int mask_hide_view_set_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");
  bool changed = false;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {

    if (masklay->restrictflag & MASK_RESTRICT_SELECT) {
      continue;
    }

    if (!unselected) {
      if (ED_mask_layer_select_check(masklay)) {
        ED_mask_layer_select_set(masklay, false);

        masklay->restrictflag |= OB_RESTRICT_VIEW;
        changed = true;
        if (masklay == BKE_mask_layer_active(mask)) {
          BKE_mask_layer_active_set(mask, NULL);
        }
      }
    }
    else {
      if (!ED_mask_layer_select_check(masklay)) {
        masklay->restrictflag |= OB_RESTRICT_VIEW;
        changed = true;
        if (masklay == BKE_mask_layer_active(mask)) {
          BKE_mask_layer_active_set(mask, NULL);
        }
      }
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_MASK | ND_DRAW, mask);
    DEG_id_tag_update(&mask->id, 0);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void MASK_OT_hide_view_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Restrict View";
  ot->description = "Hide the layer by setting the hide flag";
  ot->idname = "MASK_OT_hide_view_set";

  /* api callbacks */
  ot->exec = mask_hide_view_set_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected layers");
}

static int mask_feather_weight_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  bool changed = false;
  int i;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_SELECT | MASK_RESTRICT_VIEW)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(point)) {
          BezTriple *bezt = &point->bezt;
          bezt->weight = 0.0f;
          changed = true;
        }
      }
    }
  }

  if (changed) {
    /* TODO: only update edited splines */
    BKE_mask_update_display(mask, CFRA);

    WM_event_add_notifier(C, NC_MASK | ND_DRAW, mask);
    DEG_id_tag_update(&mask->id, 0);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void MASK_OT_feather_weight_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Feather Weight";
  ot->description = "Reset the feather weight to zero";
  ot->idname = "MASK_OT_feather_weight_clear";

  /* api callbacks */
  ot->exec = mask_feather_weight_clear_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** move mask layer operator *********************/

static bool mask_layer_move_poll(bContext *C)
{
  if (ED_maskedit_mask_poll(C)) {
    Mask *mask = CTX_data_edit_mask(C);

    return mask->masklay_tot > 0;
  }

  return false;
}

static int mask_layer_move_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *mask_layer = BLI_findlink(&mask->masklayers, mask->masklay_act);
  MaskLayer *mask_layer_other;
  int direction = RNA_enum_get(op->ptr, "direction");

  if (!mask_layer) {
    return OPERATOR_CANCELLED;
  }

  if (direction == -1) {
    mask_layer_other = mask_layer->prev;

    if (!mask_layer_other) {
      return OPERATOR_CANCELLED;
    }

    BLI_remlink(&mask->masklayers, mask_layer);
    BLI_insertlinkbefore(&mask->masklayers, mask_layer_other, mask_layer);
    mask->masklay_act--;
  }
  else if (direction == 1) {
    mask_layer_other = mask_layer->next;

    if (!mask_layer_other) {
      return OPERATOR_CANCELLED;
    }

    BLI_remlink(&mask->masklayers, mask_layer);
    BLI_insertlinkafter(&mask->masklayers, mask_layer_other, mask_layer);
    mask->masklay_act++;
  }

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return OPERATOR_FINISHED;
}

void MASK_OT_layer_move(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Layer";
  ot->description = "Move the active layer up/down in the list";
  ot->idname = "MASK_OT_layer_move";

  /* api callbacks */
  ot->exec = mask_layer_move_exec;
  ot->poll = mask_layer_move_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "direction",
               direction_items,
               0,
               "Direction",
               "Direction to move the active layer");
}

/******************** duplicate *********************/

static int mask_duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *mask_layer;

  for (mask_layer = mask->masklayers.first; mask_layer; mask_layer = mask_layer->next) {
    MaskSpline *spline;

    for (spline = mask_layer->splines.last; spline; spline = spline->prev) {
      MaskSplinePoint *point = spline->points;
      int i = 0;
      while (i < spline->tot_point) {
        int start = i, end = -1;
        /* Find next selected segment. */
        while (MASKPOINT_ISSEL_ANY(point)) {
          BKE_mask_point_select_set(point, false);
          end = i;
          if (i >= spline->tot_point - 1) {
            break;
          }
          i++;
          point++;
        }
        if (end >= start) {
          int tot_point;
          int tot_point_shape_start = 0;
          MaskSpline *new_spline = BKE_mask_spline_add(mask_layer);
          MaskSplinePoint *new_point;
          int b;

          /* BKE_mask_spline_add might allocate the points,
           * need to free them in this case. */
          if (new_spline->points) {
            MEM_freeN(new_spline->points);
          }

          /* Copy options from old spline. */
          new_spline->flag = spline->flag;
          new_spline->offset_mode = spline->offset_mode;
          new_spline->weight_interp = spline->weight_interp;
          new_spline->parent = spline->parent;

          /* Allocate new points and copy them from old spline. */
          new_spline->tot_point = end - start + 1;
          new_spline->points = MEM_mallocN(sizeof(MaskSplinePoint) * new_spline->tot_point,
                                           "duplicated mask points");

          memcpy(new_spline->points,
                 spline->points + start,
                 new_spline->tot_point * sizeof(MaskSplinePoint));

          tot_point = new_spline->tot_point;

          /* animation requires points added one by one */
          if (mask_layer->splines_shapes.first) {
            new_spline->tot_point = 0;
            tot_point_shape_start = BKE_mask_layer_shape_spline_to_index(mask_layer, new_spline);
          }

          /* Select points and duplicate their UWs (if needed). */
          for (b = 0, new_point = new_spline->points; b < tot_point; b++, new_point++) {
            if (new_point->uw) {
              new_point->uw = MEM_dupallocN(new_point->uw);
            }
            BKE_mask_point_select_set(new_point, true);

            if (mask_layer->splines_shapes.first) {
              new_spline->tot_point++;
              BKE_mask_layer_shape_changed_add(mask_layer, tot_point_shape_start + b, true, false);
            }
          }

          /* Clear cyclic flag if we didn't copy the whole spline. */
          if (new_spline->flag & MASK_SPLINE_CYCLIC) {
            if (start != 0 || end != spline->tot_point - 1) {
              new_spline->flag &= ~MASK_SPLINE_CYCLIC;
            }
          }

          /* Flush selection to splines. */
          new_spline->flag |= SELECT;
          spline->flag &= ~SELECT;

          mask_layer->act_spline = new_spline;
        }
        i++;
        point++;
      }
    }
  }

  /* TODO: only update edited splines */
  BKE_mask_update_display(mask, CFRA);

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return OPERATOR_FINISHED;
}

void MASK_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Mask";
  ot->description = "Duplicate selected control points and segments between them";
  ot->idname = "MASK_OT_duplicate";

  /* api callbacks */
  ot->exec = mask_duplicate_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** copy splines to clipboard operator *********************/

static int copy_splines_exec(bContext *C, wmOperator *UNUSED(op))
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *mask_layer = BKE_mask_layer_active(mask);

  if (mask_layer == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_mask_clipboard_copy_from_layer(mask_layer);

  return OPERATOR_FINISHED;
}

void MASK_OT_copy_splines(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Splines";
  ot->description = "Copy selected splines to clipboard";
  ot->idname = "MASK_OT_copy_splines";

  /* api callbacks */
  ot->exec = copy_splines_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/********************** paste tracks from clipboard operator *********************/

static bool paste_splines_poll(bContext *C)
{
  if (ED_maskedit_mask_poll(C)) {
    return BKE_mask_clipboard_is_empty() == false;
  }

  return 0;
}

static int paste_splines_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *mask_layer = BKE_mask_layer_active(mask);

  if (mask_layer == NULL) {
    mask_layer = BKE_mask_layer_new(mask, "");
  }

  BKE_mask_clipboard_paste_to_layer(CTX_data_main(C), mask_layer);

  /* TODO: only update edited splines */
  BKE_mask_update_display(mask, CFRA);

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return OPERATOR_FINISHED;
}

void MASK_OT_paste_splines(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Splines";
  ot->description = "Paste splines from clipboard";
  ot->idname = "MASK_OT_paste_splines";

  /* api callbacks */
  ot->exec = paste_splines_exec;
  ot->poll = paste_splines_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
