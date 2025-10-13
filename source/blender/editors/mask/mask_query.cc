/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BKE_context.hh"
#include "BKE_mask.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DNA_mask_types.h"
#include "DNA_screen_types.h"

#include "ED_clip.hh"
#include "ED_image.hh"
#include "ED_mask.hh" /* own include */

#include "UI_view2d.hh"

#include "mask_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Spatial Queries
 * \{ */

bool ED_mask_find_nearest_diff_point(const bContext *C,
                                     Mask *mask_orig,
                                     const float normal_co[2],
                                     int threshold,
                                     bool feather,
                                     float tangent[2],
                                     const bool use_deform,
                                     const bool use_project,
                                     MaskLayer **r_mask_layer,
                                     MaskSpline **r_spline,
                                     MaskSplinePoint **r_point,
                                     float *r_u,
                                     float *r_score)
{
  const float threshold_sq = threshold * threshold;

  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  MaskLayer *point_mask_layer;
  MaskSpline *point_spline;
  MaskSplinePoint *point = nullptr;
  float dist_best_sq = FLT_MAX, co[2];
  int width, height;
  float u = 0.0f;
  float scalex, scaley;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mask *mask_eval = DEG_get_evaluated(depsgraph, mask_orig);

  ED_mask_get_size(area, &width, &height);
  ED_mask_pixelspace_factor(area, region, &scalex, &scaley);

  co[0] = normal_co[0] * scalex;
  co[1] = normal_co[1] * scaley;

  for (MaskLayer *mask_layer_orig = static_cast<MaskLayer *>(mask_orig->masklayers.first),
                 *mask_layer_eval = static_cast<MaskLayer *>(mask_eval->masklayers.first);
       mask_layer_orig != nullptr;
       mask_layer_orig = mask_layer_orig->next, mask_layer_eval = mask_layer_eval->next)
  {
    if (mask_layer_orig->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    for (MaskSpline *spline_orig = static_cast<MaskSpline *>(mask_layer_orig->splines.first),
                    *spline_eval = static_cast<MaskSpline *>(mask_layer_eval->splines.first);
         spline_orig != nullptr;
         spline_orig = spline_orig->next, spline_eval = spline_eval->next)
    {
      int i;
      MaskSplinePoint *cur_point_eval;

      for (i = 0, cur_point_eval = use_deform ? spline_eval->points_deform : spline_eval->points;
           i < spline_eval->tot_point;
           i++, cur_point_eval++)
      {
        uint tot_diff_point;
        float *diff_points = BKE_mask_point_segment_diff(
            spline_eval, cur_point_eval, width, height, &tot_diff_point);

        if (diff_points) {
          int j, tot_point;
          uint tot_feather_point;
          float *feather_points = nullptr, *points;

          if (feather) {
            feather_points = BKE_mask_point_segment_feather_diff(
                spline_eval, cur_point_eval, width, height, &tot_feather_point);

            points = feather_points;
            tot_point = tot_feather_point;
          }
          else {
            points = diff_points;
            tot_point = tot_diff_point;
          }

          for (j = 0; j < tot_point - 1; j++) {
            float dist_sq, a[2], b[2];

            a[0] = points[2 * j] * scalex;
            a[1] = points[2 * j + 1] * scaley;

            b[0] = points[2 * j + 2] * scalex;
            b[1] = points[2 * j + 3] * scaley;

            dist_sq = dist_squared_to_line_segment_v2(co, a, b);

            if (dist_sq < dist_best_sq) {
              if (tangent) {
                sub_v2_v2v2(tangent, &diff_points[2 * j + 2], &diff_points[2 * j]);
              }

              point_mask_layer = mask_layer_orig;
              point_spline = spline_orig;
              point = use_deform ?
                          &spline_orig->points[(cur_point_eval - spline_eval->points_deform)] :
                          &spline_orig->points[(cur_point_eval - spline_eval->points)];
              dist_best_sq = dist_sq;
              u = float(j) / tot_point;
            }
          }

          if (feather_points != nullptr) {
            MEM_freeN(feather_points);
          }
          MEM_freeN(diff_points);
        }
      }
    }
  }

  if (point && dist_best_sq < threshold_sq) {
    if (r_mask_layer) {
      *r_mask_layer = point_mask_layer;
    }

    if (r_spline) {
      *r_spline = point_spline;
    }

    if (r_point) {
      *r_point = point;
    }

    if (r_u) {
      /* TODO(sergey): Projection fails in some weirdo cases. */
      if (use_project) {
        u = BKE_mask_spline_project_co(point_spline, point, u, normal_co, MASK_PROJ_ANY);
      }

      *r_u = u;
    }

    if (r_score) {
      *r_score = dist_best_sq;
    }

    return true;
  }

  if (r_mask_layer) {
    *r_mask_layer = nullptr;
  }

  if (r_spline) {
    *r_spline = nullptr;
  }

  if (r_point) {
    *r_point = nullptr;
  }

  return false;
}

static void mask_point_scaled_handle(const MaskSplinePoint *point,
                                     const eMaskWhichHandle which_handle,
                                     const float scalex,
                                     const float scaley,
                                     float handle[2])
{
  BKE_mask_point_handle(point, which_handle, handle);
  handle[0] *= scalex;
  handle[1] *= scaley;
}

MaskSplinePoint *ED_mask_point_find_nearest(const bContext *C,
                                            Mask *mask_orig,
                                            const float normal_co[2],
                                            const float threshold,
                                            MaskLayer **r_mask_layer,
                                            MaskSpline **r_spline,
                                            eMaskWhichHandle *r_which_handle,
                                            float *r_score)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  MaskLayer *point_mask_layer = nullptr;
  MaskSpline *point_spline = nullptr;
  MaskSplinePoint *point = nullptr;
  float co[2];
  const float threshold_sq = threshold * threshold;
  float len_sq = FLT_MAX, scalex, scaley;
  eMaskWhichHandle which_handle = MASK_WHICH_HANDLE_NONE;
  int width, height;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mask *mask_eval = DEG_get_evaluated(depsgraph, mask_orig);

  ED_mask_get_size(area, &width, &height);
  ED_mask_pixelspace_factor(area, region, &scalex, &scaley);

  co[0] = normal_co[0] * scalex;
  co[1] = normal_co[1] * scaley;

  for (MaskLayer *mask_layer_orig = static_cast<MaskLayer *>(mask_orig->masklayers.first),
                 *mask_layer_eval = static_cast<MaskLayer *>(mask_eval->masklayers.first);
       mask_layer_orig != nullptr;
       mask_layer_orig = mask_layer_orig->next, mask_layer_eval = mask_layer_eval->next)
  {

    if (mask_layer_orig->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    for (MaskSpline *spline_orig = static_cast<MaskSpline *>(mask_layer_orig->splines.first),
                    *spline_eval = static_cast<MaskSpline *>(mask_layer_eval->splines.first);
         spline_orig != nullptr;
         spline_orig = spline_orig->next, spline_eval = spline_eval->next)
    {
      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline_eval);

      for (int i = 0; i < spline_orig->tot_point; i++) {
        MaskSplinePoint *cur_point_orig = &spline_orig->points[i];
        const MaskSplinePoint *cur_point_deform_eval = &points_array[i];
        eMaskWhichHandle cur_which_handle = MASK_WHICH_HANDLE_NONE;
        const BezTriple *bezt = &cur_point_deform_eval->bezt;
        float cur_len_sq, vec[2];

        vec[0] = bezt->vec[1][0] * scalex;
        vec[1] = bezt->vec[1][1] * scaley;

        cur_len_sq = len_squared_v2v2(co, vec);

        if (cur_len_sq < len_sq) {
          point_spline = spline_orig;
          point_mask_layer = mask_layer_orig;
          point = cur_point_orig;
          len_sq = cur_len_sq;
          which_handle = MASK_WHICH_HANDLE_NONE;
        }

        if (BKE_mask_point_handles_mode_get(cur_point_deform_eval) == MASK_HANDLE_MODE_STICK) {
          float handle[2];
          mask_point_scaled_handle(
              cur_point_deform_eval, MASK_WHICH_HANDLE_STICK, scalex, scaley, handle);
          cur_len_sq = len_squared_v2v2(co, handle);
          cur_which_handle = MASK_WHICH_HANDLE_STICK;
        }
        else {
          float handle_left[2], handle_right[2];
          float len_left_sq, len_right_sq;
          mask_point_scaled_handle(
              cur_point_deform_eval, MASK_WHICH_HANDLE_LEFT, scalex, scaley, handle_left);
          mask_point_scaled_handle(
              cur_point_deform_eval, MASK_WHICH_HANDLE_RIGHT, scalex, scaley, handle_right);

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
          point_mask_layer = mask_layer_orig;
          point_spline = spline_orig;
          point = cur_point_orig;
          len_sq = cur_len_sq;
          which_handle = cur_which_handle;
        }
      }
    }
  }

  if (len_sq < threshold_sq) {
    if (r_mask_layer) {
      *r_mask_layer = point_mask_layer;
    }

    if (r_spline) {
      *r_spline = point_spline;
    }

    if (r_which_handle) {
      *r_which_handle = which_handle;
    }

    if (r_score) {
      *r_score = sqrtf(len_sq);
    }

    return point;
  }

  if (r_mask_layer) {
    *r_mask_layer = nullptr;
  }

  if (r_spline) {
    *r_spline = nullptr;
  }

  if (r_which_handle) {
    *r_which_handle = MASK_WHICH_HANDLE_NONE;
  }

  return nullptr;
}

bool ED_mask_feather_find_nearest(const bContext *C,
                                  Mask *mask_orig,
                                  const float normal_co[2],
                                  const float threshold,
                                  MaskLayer **r_mask_layer,
                                  MaskSpline **r_spline,
                                  MaskSplinePoint **r_point,
                                  MaskSplinePointUW **r_uw,
                                  float *r_score)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  MaskLayer *point_mask_layer = nullptr;
  MaskSpline *point_spline = nullptr;
  MaskSplinePoint *point = nullptr;
  MaskSplinePointUW *uw = nullptr;
  const float threshold_sq = threshold * threshold;
  float len = FLT_MAX, co[2];
  float scalex, scaley;
  int width, height;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mask *mask_eval = DEG_get_evaluated(depsgraph, mask_orig);

  ED_mask_get_size(area, &width, &height);
  ED_mask_pixelspace_factor(area, region, &scalex, &scaley);

  co[0] = normal_co[0] * scalex;
  co[1] = normal_co[1] * scaley;

  for (MaskLayer *mask_layer_orig = static_cast<MaskLayer *>(mask_orig->masklayers.first),
                 *mask_layer_eval = static_cast<MaskLayer *>(mask_eval->masklayers.first);
       mask_layer_orig != nullptr;
       mask_layer_orig = mask_layer_orig->next, mask_layer_eval = mask_layer_eval->next)
  {

    for (MaskSpline *spline_orig = static_cast<MaskSpline *>(mask_layer_orig->splines.first),
                    *spline_eval = static_cast<MaskSpline *>(mask_layer_eval->splines.first);
         spline_orig != nullptr;
         spline_orig = spline_orig->next, spline_eval = spline_eval->next)
    {
      // MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

      int i, tot_feather_point;
      float (*feather_points)[2], (*fp)[2];

      if (mask_layer_orig->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
        continue;
      }

      feather_points = fp = BKE_mask_spline_feather_points(spline_eval, &tot_feather_point);

      for (i = 0; i < spline_orig->tot_point; i++) {
        int j;
        MaskSplinePoint *cur_point_orig = &spline_orig->points[i];
        MaskSplinePoint *cur_point_eval = &spline_eval->points[i];

        for (j = 0; j <= cur_point_eval->tot_uw; j++) {
          float cur_len_sq, vec[2];

          vec[0] = (*fp)[0] * scalex;
          vec[1] = (*fp)[1] * scaley;

          cur_len_sq = len_squared_v2v2(vec, co);

          if (point == nullptr || cur_len_sq < len) {
            if (j == 0) {
              uw = nullptr;
            }
            else {
              uw = &cur_point_orig->uw[j - 1];
            }

            point_mask_layer = mask_layer_orig;
            point_spline = spline_orig;
            point = cur_point_orig;
            len = cur_len_sq;
          }

          fp++;
        }
      }

      MEM_freeN(feather_points);
    }
  }

  if (len < threshold_sq) {
    if (r_mask_layer) {
      *r_mask_layer = point_mask_layer;
    }

    if (r_spline) {
      *r_spline = point_spline;
    }

    if (r_point) {
      *r_point = point;
    }

    if (r_uw) {
      *r_uw = uw;
    }

    if (r_score) {
      *r_score = sqrtf(len);
    }

    return true;
  }

  if (r_mask_layer) {
    *r_mask_layer = nullptr;
  }

  if (r_spline) {
    *r_spline = nullptr;
  }

  if (r_point) {
    *r_point = nullptr;
  }

  return false;
}

void ED_mask_mouse_pos(ScrArea *area, ARegion *region, const int mval[2], float r_co[2])
{
  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        ED_clip_mouse_pos(sc, region, mval, r_co);
        BKE_mask_coord_from_movieclip(sc->clip, &sc->user, r_co, r_co);
        break;
      }
      case SPACE_SEQ: {
        UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &r_co[0], &r_co[1]);
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        ED_image_mouse_pos(sima, region, mval, r_co);
        BKE_mask_coord_from_image(sima->image, &sima->iuser, r_co, r_co);
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        zero_v2(r_co);
        break;
    }
  }
  else {
    BLI_assert(0);
    zero_v2(r_co);
  }
}

void ED_mask_point_pos(ScrArea *area, ARegion *region, float x, float y, float *r_x, float *r_y)
{
  float co[2];

  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        ED_clip_point_stable_pos(sc, region, x, y, &co[0], &co[1]);
        BKE_mask_coord_from_movieclip(sc->clip, &sc->user, co, co);
        break;
      }
      case SPACE_SEQ:
        zero_v2(co); /* MASKTODO */
        break;
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        ED_image_point_pos(sima, region, x, y, &co[0], &co[1]);
        BKE_mask_coord_from_image(sima->image, &sima->iuser, co, co);
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        zero_v2(co);
        break;
    }
  }
  else {
    BLI_assert(0);
    zero_v2(co);
  }

  *r_x = co[0];
  *r_y = co[1];
}

void ED_mask_point_pos__reverse(
    ScrArea *area, ARegion *region, float x, float y, float *r_x, float *r_y)
{
  float co[2];

  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        co[0] = x;
        co[1] = y;
        BKE_mask_coord_to_movieclip(sc->clip, &sc->user, co, co);
        ED_clip_point_stable_pos__reverse(sc, region, co, co);
        break;
      }
      case SPACE_SEQ:
        zero_v2(co); /* MASKTODO */
        break;
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        co[0] = x;
        co[1] = y;
        BKE_mask_coord_to_image(sima->image, &sima->iuser, co, co);
        ED_image_point_pos__reverse(sima, region, co, co);
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        zero_v2(co);
        break;
    }
  }
  else {
    BLI_assert(0);
    zero_v2(co);
  }

  *r_x = co[0];
  *r_y = co[1];
}

static void handle_position_for_minmax(const MaskSplinePoint *point,
                                       eMaskWhichHandle which_handle,
                                       bool handles_as_control_point,
                                       float r_handle[2])
{
  if (handles_as_control_point) {
    copy_v2_v2(r_handle, point->bezt.vec[1]);
    return;
  }
  BKE_mask_point_handle(point, which_handle, r_handle);
}

bool ED_mask_selected_minmax(const bContext *C,
                             float min[2],
                             float max[2],
                             bool handles_as_control_point)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Mask *mask = CTX_data_edit_mask(C);

  bool ok = false;

  if (mask == nullptr) {
    return ok;
  }

  /* Use evaluated mask to take animation into account.
   * The animation of splines is not "flushed" back to original, so need to explicitly
   * use evaluated data-block here. */
  Mask *mask_eval = DEG_get_evaluated(depsgraph, mask);

  INIT_MINMAX2(min, max);
  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask_eval->masklayers) {
    if (mask_layer->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }
    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);
      for (int i = 0; i < spline->tot_point; i++) {
        const MaskSplinePoint *point = &spline->points[i];
        const MaskSplinePoint *deform_point = &points_array[i];
        const BezTriple *bezt = &point->bezt;
        float handle[2];
        if (!MASKPOINT_ISSEL_ANY(point)) {
          continue;
        }
        if (bezt->f2 & SELECT) {
          minmax_v2v2_v2(min, max, deform_point->bezt.vec[1]);
          ok = true;
        }

        if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
          handle_position_for_minmax(
              deform_point, MASK_WHICH_HANDLE_STICK, handles_as_control_point, handle);
          minmax_v2v2_v2(min, max, handle);
          ok = true;
        }
        else {
          if ((bezt->f1 & SELECT) && (bezt->h1 != HD_VECT)) {
            handle_position_for_minmax(
                deform_point, MASK_WHICH_HANDLE_LEFT, handles_as_control_point, handle);
            minmax_v2v2_v2(min, max, handle);
            ok = true;
          }
          if ((bezt->f3 & SELECT) && (bezt->h2 != HD_VECT)) {
            handle_position_for_minmax(
                deform_point, MASK_WHICH_HANDLE_RIGHT, handles_as_control_point, handle);
            minmax_v2v2_v2(min, max, handle);
            ok = true;
          }
        }
      }
    }
  }
  return ok;
}

void ED_mask_center_from_pivot_ex(
    const bContext *C, ScrArea *area, float r_center[2], char mode, bool *r_has_select)
{
  float min[2], max[2];
  const bool mask_selected = ED_mask_selected_minmax(C, min, max, false);

  switch (mode) {
    case V3D_AROUND_CURSOR:
      ED_mask_cursor_location_get(area, r_center);
      break;
    default:
      mid_v2_v2v2(r_center, min, max);
      break;
  }
  if (r_has_select != nullptr) {
    *r_has_select = mask_selected;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic 2D View Queries
 * \{ */

void ED_mask_get_size(ScrArea *area, int *r_width, int *r_height)
{
  if (area && area->spacedata.first) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        ED_space_clip_get_size(sc, r_width, r_height);
        break;
      }
      case SPACE_SEQ: {
        // Scene *scene = CTX_data_scene(C);
        // BKE_render_resolution(&scene->r, false, r_width, r_height);
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        ED_space_image_get_size(sima, r_width, r_height);
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        *r_width = 0;
        *r_height = 0;
        break;
    }
  }
  else {
    BLI_assert(0);
    *r_width = 0;
    *r_height = 0;
  }
}

void ED_mask_zoom(ScrArea *area, ARegion *region, float *r_zoomx, float *r_zoomy)
{
  if (area && area->spacedata.first) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        ED_space_clip_get_zoom(sc, region, r_zoomx, r_zoomy);
        break;
      }
      case SPACE_SEQ: {
        *r_zoomx = *r_zoomy = 1.0f;
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        ED_space_image_get_zoom(sima, region, r_zoomx, r_zoomy);
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        *r_zoomx = *r_zoomy = 1.0f;
        break;
    }
  }
  else {
    BLI_assert(0);
    *r_zoomx = *r_zoomy = 1.0f;
  }
}

void ED_mask_get_aspect(ScrArea *area, ARegion * /*region*/, float *r_aspx, float *r_aspy)
{
  if (area && area->spacedata.first) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        ED_space_clip_get_aspect(sc, r_aspx, r_aspy);
        break;
      }
      case SPACE_SEQ: {
        *r_aspx = *r_aspy = 1.0f; /* MASKTODO - render aspect? */
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        ED_space_image_get_aspect(sima, r_aspx, r_aspy);
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        *r_aspx = *r_aspy = 1.0f;
        break;
    }
  }
  else {
    BLI_assert(0);
    *r_aspx = *r_aspy = 1.0f;
  }
}

void ED_mask_pixelspace_factor(ScrArea *area, ARegion *region, float *r_scalex, float *r_scaley)
{
  if (area && area->spacedata.first) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        float aspx, aspy;

        UI_view2d_scale_get(&region->v2d, r_scalex, r_scaley);
        ED_space_clip_get_aspect(sc, &aspx, &aspy);

        *r_scalex *= aspx;
        *r_scaley *= aspy;
        break;
      }
      case SPACE_SEQ: {
        *r_scalex = *r_scaley = 1.0f; /* MASKTODO? */
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        float aspx, aspy;

        UI_view2d_scale_get(&region->v2d, r_scalex, r_scaley);
        ED_space_image_get_aspect(sima, &aspx, &aspy);

        *r_scalex *= aspx;
        *r_scaley *= aspy;
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        *r_scalex = *r_scaley = 1.0f;
        break;
    }
  }
  else {
    BLI_assert(0);
    *r_scalex = *r_scaley = 1.0f;
  }
}

void ED_mask_cursor_location_get(ScrArea *area, float cursor[2])
{
  if (area) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *space_clip = static_cast<SpaceClip *>(area->spacedata.first);
        copy_v2_v2(cursor, space_clip->cursor);
        break;
      }
      case SPACE_SEQ: {
        zero_v2(cursor);
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *space_image = static_cast<SpaceImage *>(area->spacedata.first);
        copy_v2_v2(cursor, space_image->cursor);
        break;
      }
      default:
        /* possible other spaces from which mask editing is available */
        BLI_assert(0);
        zero_v2(cursor);
        break;
    }
  }
  else {
    BLI_assert(0);
    zero_v2(cursor);
  }
}

/** \} */
