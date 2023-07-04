/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_tracking.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"
#include "ED_mask.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "DEG_depsgraph.h"

#include "clip_intern.h"         /* own include */
#include "tracking_ops_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Point track marker picking.
 * \{ */

BLI_INLINE PointTrackPick point_track_pick_make_null()
{
  PointTrackPick pick = {nullptr};

  pick.area = TRACK_AREA_NONE;
  pick.area_detail = TRACK_PICK_AREA_DETAIL_NONE;
  pick.corner_index = -1;
  pick.distance_px_squared = FLT_MAX;

  return pick;
}

static void slide_marker_tilt_slider_relative(const float pattern_corners[4][2], float r_slider[2])
{
  add_v2_v2v2(r_slider, pattern_corners[1], pattern_corners[2]);
}

static void slide_marker_tilt_slider(const float marker_pos[2],
                                     const float pattern_corners[4][2],
                                     float r_slider[2])
{
  slide_marker_tilt_slider_relative(pattern_corners, r_slider);
  add_v2_v2(r_slider, marker_pos);
}

static float mouse_to_slide_zone_distance_squared(const float co[2],
                                                  const float slide_zone[2],
                                                  int width,
                                                  int height)
{
  const float pixel_co[2] = {co[0] * width, co[1] * height},
              pixel_slide_zone[2] = {slide_zone[0] * width, slide_zone[1] * height};
  return square_f(pixel_co[0] - pixel_slide_zone[0]) + square_f(pixel_co[1] - pixel_slide_zone[1]);
}

static float mouse_to_search_corner_distance_squared(
    const MovieTrackingMarker *marker, const float co[2], int corner, int width, int height)
{
  float side_zone[2];
  if (corner == 0) {
    side_zone[0] = marker->pos[0] + marker->search_max[0];
    side_zone[1] = marker->pos[1] + marker->search_min[1];
  }
  else {
    side_zone[0] = marker->pos[0] + marker->search_min[0];
    side_zone[1] = marker->pos[1] + marker->search_max[1];
  }
  return mouse_to_slide_zone_distance_squared(co, side_zone, width, height);
}

static float mouse_to_closest_pattern_corner_distance_squared(
    const MovieTrackingMarker *marker, const float co[2], int width, int height, int *r_corner)
{
  float min_distance_squared = FLT_MAX;
  for (int i = 0; i < 4; i++) {
    float corner_co[2];
    add_v2_v2v2(corner_co, marker->pattern_corners[i], marker->pos);
    float distance_squared = mouse_to_slide_zone_distance_squared(co, corner_co, width, height);
    if (distance_squared < min_distance_squared) {
      min_distance_squared = distance_squared;
      *r_corner = i;
    }
  }
  return min_distance_squared;
}

static float mouse_to_offset_distance_squared(const MovieTrackingTrack *track,
                                              const MovieTrackingMarker *marker,
                                              const float co[2],
                                              int width,
                                              int height)
{
  float pos[2];
  add_v2_v2v2(pos, marker->pos, track->offset);
  return mouse_to_slide_zone_distance_squared(co, pos, width, height);
}

static float mouse_to_tilt_distance_squared(const MovieTrackingMarker *marker,
                                            const float co[2],
                                            int width,
                                            int height)
{
  float slider[2];
  slide_marker_tilt_slider(marker->pos, marker->pattern_corners, slider);
  return mouse_to_slide_zone_distance_squared(co, slider, width, height);
}

static float mouse_to_closest_corners_edge_distance_squared(const float co[2],
                                                            const float corners_offset[2],
                                                            const float corners[4][2],
                                                            int width,
                                                            int height)
{
  const float co_px[2] = {co[0] * width, co[1] * height};

  float prev_corner_co_px[2];
  add_v2_v2v2(prev_corner_co_px, corners_offset, corners[3]);
  prev_corner_co_px[0] *= width;
  prev_corner_co_px[1] *= height;

  float min_distance_squared = FLT_MAX;

  for (int i = 0; i < 4; ++i) {
    float corner_co_px[2];
    add_v2_v2v2(corner_co_px, corners_offset, corners[i]);
    corner_co_px[0] *= width;
    corner_co_px[1] *= height;

    const float distance_squared = dist_squared_to_line_segment_v2(
        co_px, corner_co_px, prev_corner_co_px);

    if (distance_squared < min_distance_squared) {
      min_distance_squared = distance_squared;
    }

    copy_v2_v2(prev_corner_co_px, corner_co_px);
  }

  return min_distance_squared;
}

static float mouse_to_closest_pattern_edge_distance_squared(const MovieTrackingMarker *marker,
                                                            const float co[2],
                                                            int width,
                                                            int height)
{
  return mouse_to_closest_corners_edge_distance_squared(
      co, marker->pos, marker->pattern_corners, width, height);
}

static float mouse_to_closest_search_edge_distance_squared(const MovieTrackingMarker *marker,
                                                           const float co[2],
                                                           int width,
                                                           int height)
{
  const float corners[4][2] = {
      {marker->search_min[0], marker->search_min[1]},
      {marker->search_max[0], marker->search_min[1]},
      {marker->search_max[0], marker->search_max[1]},
      {marker->search_min[0], marker->search_max[1]},
  };

  return mouse_to_closest_corners_edge_distance_squared(co, marker->pos, corners, width, height);
}

PointTrackPick ed_tracking_pick_point_track(const TrackPickOptions *options,
                                            bContext *C,
                                            const float co[2])
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);

  int width, height;
  ED_space_clip_get_size(space_clip, &width, &height);
  if (width == 0 || height == 0) {
    return point_track_pick_make_null();
  }

  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  const float distance_tolerance_px_squared = (12.0f * 12.0f) / space_clip->zoom;
  const bool are_disabled_markers_visible = (space_clip->flag & SC_HIDE_DISABLED) == 0;
  const int framenr = ED_space_clip_get_clip_frame_number(space_clip);

  PointTrackPick pick = point_track_pick_make_null();

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    const bool is_track_selected = TRACK_VIEW_SELECTED(space_clip, track);

    if (options->selected_only && !is_track_selected) {
      continue;
    }
    if (options->unlocked_only && (track->flag & TRACK_LOCKED)) {
      continue;
    }

    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
    const bool is_marker_enabled = ((marker->flag & MARKER_DISABLED) == 0);

    if (!is_marker_enabled) {
      if (options->enabled_only) {
        /* Disabled marker is requested to not be in the pick result, so skip it. */
        continue;
      }

      /* See whether the disabled marker is visible.
       *
       * If the clip editor is not hiding disabled markers, then all disabled markers are visible.
       * Otherwise only disabled marker of the active track is visible. */
      if (!are_disabled_markers_visible && track != tracking_object->active_track) {
        continue;
      }
    }

    float distance_squared;

    /* Initialize the current pick with the offset point of the track. */
    PointTrackPick current_pick = point_track_pick_make_null();
    current_pick.track = track;
    current_pick.marker = marker;
    current_pick.area = TRACK_AREA_POINT;
    current_pick.distance_px_squared = mouse_to_offset_distance_squared(
        track, marker, co, width, height);

    /* If search area is visible, check how close to its sliding zones mouse is.
     * NOTE: The search area is only visible for selected tracks. */
    if (is_track_selected && (space_clip->flag & SC_SHOW_MARKER_SEARCH)) {
      distance_squared = mouse_to_search_corner_distance_squared(marker, co, 1, width, height);
      if (distance_squared < current_pick.distance_px_squared) {
        current_pick.area = TRACK_AREA_SEARCH;
        current_pick.area_detail = TRACK_PICK_AREA_DETAIL_OFFSET;
        current_pick.distance_px_squared = distance_squared;
      }

      distance_squared = mouse_to_search_corner_distance_squared(marker, co, 0, width, height);
      if (distance_squared < current_pick.distance_px_squared) {
        current_pick.area = TRACK_AREA_SEARCH;
        current_pick.area_detail = TRACK_PICK_AREA_DETAIL_SIZE;
        current_pick.distance_px_squared = distance_squared;
      }
    }

    /* If pattern area is visible, check which corner is closest to the mouse. */
    if (space_clip->flag & SC_SHOW_MARKER_PATTERN) {
      int current_corner = -1;
      distance_squared = mouse_to_closest_pattern_corner_distance_squared(
          marker, co, width, height, &current_corner);
      if (distance_squared < current_pick.distance_px_squared) {
        current_pick.area = TRACK_AREA_PAT;
        current_pick.area_detail = TRACK_PICK_AREA_DETAIL_POSITION;
        current_pick.corner_index = current_corner;
        current_pick.distance_px_squared = distance_squared;
      }

      /* Here we also check whether the mouse is actually closer to the widget which controls scale
       * and tilt.
       * NOTE: The tilt control is only visible for selected tracks. */
      if (is_track_selected) {
        distance_squared = mouse_to_tilt_distance_squared(marker, co, width, height);
        if (distance_squared < current_pick.distance_px_squared) {
          current_pick.area = TRACK_AREA_PAT;
          current_pick.area_detail = TRACK_PICK_AREA_DETAIL_TILT_SIZE;
          current_pick.distance_px_squared = distance_squared;
        }
      }
    }

    /* Whenever a manipulation "widgets" are not within distance tolerance test the edges as well.
     * This allows to pick tracks by clicking on the pattern/search areas edges but prefer to use
     * more actionable "widget" for sliding. */
    if (current_pick.distance_px_squared > distance_tolerance_px_squared) {
      if (is_track_selected && (space_clip->flag & SC_SHOW_MARKER_SEARCH)) {
        distance_squared = mouse_to_closest_search_edge_distance_squared(
            marker, co, width, height);
        if (distance_squared < current_pick.distance_px_squared) {
          current_pick.area = TRACK_AREA_SEARCH;
          current_pick.area_detail = TRACK_PICK_AREA_DETAIL_EDGE;
          current_pick.distance_px_squared = distance_squared;
        }
      }

      if (space_clip->flag & SC_SHOW_MARKER_PATTERN) {
        distance_squared = mouse_to_closest_pattern_edge_distance_squared(
            marker, co, width, height);
        if (distance_squared < current_pick.distance_px_squared) {
          current_pick.area = TRACK_AREA_PAT;
          current_pick.area_detail = TRACK_PICK_AREA_DETAIL_EDGE;
          current_pick.distance_px_squared = distance_squared;
        }
      }
    }

    if (current_pick.distance_px_squared < pick.distance_px_squared) {
      pick = current_pick;
    }
  }

  if (pick.distance_px_squared > distance_tolerance_px_squared) {
    return point_track_pick_make_null();
  }

  return pick;
}

bool ed_tracking_point_track_pick_can_slide(const SpaceClip *space_clip,
                                            const PointTrackPick *pick)
{
  if (pick->track == nullptr) {
    return false;
  }

  BLI_assert(pick->marker != nullptr);

  if (!TRACK_VIEW_SELECTED(space_clip, pick->track)) {
    return false;
  }

  if (pick->track->flag & TRACK_LOCKED) {
    return false;
  }
  if (pick->marker->flag & MARKER_DISABLED) {
    return false;
  }

  return pick->area_detail != TRACK_PICK_AREA_DETAIL_EDGE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Plane track marker picking.
 * \{ */

BLI_INLINE PlaneTrackPick plane_track_pick_make_null()
{
  PlaneTrackPick result = {nullptr};

  result.corner_index = -1;
  result.distance_px_squared = FLT_MAX;

  return result;
}

static float mouse_to_plane_slide_zone_distance_squared(const float co[2],
                                                        const float slide_zone[2],
                                                        int width,
                                                        int height)
{
  const float pixel_co[2] = {co[0] * width, co[1] * height};
  const float pixel_slide_zone[2] = {slide_zone[0] * width, slide_zone[1] * height};
  return square_f(pixel_co[0] - pixel_slide_zone[0]) + square_f(pixel_co[1] - pixel_slide_zone[1]);
}

PlaneTrackPick ed_tracking_pick_plane_track(const TrackPickOptions *options,
                                            bContext *C,
                                            const float co[2])
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);

  int width, height;
  ED_space_clip_get_size(space_clip, &width, &height);
  if (width == 0 || height == 0) {
    return plane_track_pick_make_null();
  }

  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(space_clip);

  const float distance_tolerance_px_squared = (12.0f * 12.0f) / space_clip->zoom;
  PlaneTrackPick pick = plane_track_pick_make_null();

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    if (options->selected_only && !PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      continue;
    }

    MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

    PlaneTrackPick current_pick = plane_track_pick_make_null();
    current_pick.plane_track = plane_track;
    current_pick.plane_marker = plane_marker;

    for (int i = 0; i < 4; i++) {
      const float distance_squared = mouse_to_plane_slide_zone_distance_squared(
          co, plane_marker->corners[i], width, height);

      if (distance_squared < current_pick.distance_px_squared) {
        current_pick.corner_index = i;
        current_pick.distance_px_squared = distance_squared;
      }
    }

    if (current_pick.distance_px_squared > distance_tolerance_px_squared) {
      const float zero_offset[2] = {0.0f, 0.0f};
      const float distance_squared = mouse_to_closest_corners_edge_distance_squared(
          co, zero_offset, plane_marker->corners, width, height);
      if (distance_squared < current_pick.distance_px_squared) {
        current_pick.corner_index = -1;
        current_pick.distance_px_squared = distance_squared;
      }
    }

    if (current_pick.distance_px_squared < pick.distance_px_squared) {
      pick = current_pick;
    }
  }

  if (pick.distance_px_squared > distance_tolerance_px_squared) {
    return plane_track_pick_make_null();
  }

  return pick;
}

bool ed_tracking_plane_track_pick_can_slide(const PlaneTrackPick *pick)
{
  if (pick->plane_track == nullptr) {
    return false;
  }

  BLI_assert(pick->plane_marker != nullptr);

  if (!PLANE_TRACK_VIEW_SELECTED(pick->plane_track)) {
    return false;
  }

  return pick->corner_index != -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pick closest point or plane track.
 * \{ */

BLI_INLINE TrackingPick tracking_pick_make_null()
{
  TrackingPick result;

  result.point_track_pick = point_track_pick_make_null();
  result.plane_track_pick = plane_track_pick_make_null();

  return result;
}

static bool tracking_should_prefer_point_track(bContext *C,
                                               const PointTrackPick *point_track_pick,
                                               const PlaneTrackPick *plane_track_pick)
{
  /* Simple case: one of the pick results is empty, so prefer the other one. */
  if (point_track_pick->track == nullptr) {
    return false;
  }
  if (plane_track_pick->plane_track == nullptr) {
    return true;
  }

  SpaceClip *space_clip = CTX_wm_space_clip(C);

  /* If one of the picks can be slid prefer it. */
  const bool can_slide_point_track = ed_tracking_point_track_pick_can_slide(space_clip,
                                                                            point_track_pick);
  const bool can_slide_plane_track = ed_tracking_plane_track_pick_can_slide(plane_track_pick);
  if (can_slide_point_track && !can_slide_plane_track) {
    return true;
  }
  else if (!can_slide_point_track && can_slide_plane_track) {
    return false;
  }

  /* Prefer the closest pick. */
  if (point_track_pick->distance_px_squared > plane_track_pick->distance_px_squared) {
    return false;
  }
  return true;
}

TrackingPick ed_tracking_pick_closest(const TrackPickOptions *options,
                                      bContext *C,
                                      const float co[2])
{
  TrackingPick pick;

  pick.point_track_pick = ed_tracking_pick_point_track(options, C, co);
  pick.plane_track_pick = ed_tracking_pick_plane_track(options, C, co);

  if (tracking_should_prefer_point_track(C, &pick.point_track_pick, &pick.plane_track_pick)) {
    pick.plane_track_pick = plane_track_pick_make_null();
  }
  else {
    pick.point_track_pick = point_track_pick_make_null();
  }

  return pick;
}

/** \} */

/********************** mouse select operator *********************/

void ed_tracking_deselect_all_tracks(ListBase *tracks_base)
{
  LISTBASE_FOREACH (MovieTrackingTrack *, track, tracks_base) {
    BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
  }
}

void ed_tracking_deselect_all_plane_tracks(ListBase *plane_tracks_base)
{
  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, plane_tracks_base) {
    plane_track->flag &= ~SELECT;
  }
}

static bool select_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc) {
    return sc->clip && sc->view == SC_VIEW_CLIP;
  }

  return false;
}

static int select_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");

  float co[2];
  RNA_float_get_array(op->ptr, "location", co);

  const TrackPickOptions options = ed_tracking_pick_options_defaults();
  const TrackingPick pick = ed_tracking_pick_closest(&options, C, co);

  /* Special code which allows to slide a marker which belongs to currently selected but not yet
   * active track. If such track is found activate it and return pass-though so that marker slide
   * operator can be used immediately after.
   * This logic makes it convenient to slide markers when left mouse selection is used. Without it
   * selection will be lost which causes inconvenience for the VFX artist. */
  const bool activate_selected = !extend;
  if (activate_selected && ed_tracking_pick_can_slide(sc, &pick)) {
    if (pick.point_track_pick.track != nullptr) {
      tracking_object->active_track = pick.point_track_pick.track;
      tracking_object->active_plane_track = nullptr;
    }
    else {
      tracking_object->active_track = nullptr;
      tracking_object->active_plane_track = pick.plane_track_pick.plane_track;
    }

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
    DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);

    return OPERATOR_PASS_THROUGH;
  }

  ClipViewLockState lock_state;
  ED_clip_view_lock_state_store(C, &lock_state);

  if (pick.point_track_pick.track != nullptr) {
    if (!extend) {
      ed_tracking_deselect_all_plane_tracks(&tracking_object->plane_tracks);
    }

    MovieTrackingTrack *track = pick.point_track_pick.track;
    int area = pick.point_track_pick.area;

    if (!extend || !TRACK_VIEW_SELECTED(sc, track)) {
      area = TRACK_AREA_ALL;
    }

    if (extend && TRACK_AREA_SELECTED(track, area)) {
      if (track == tracking_object->active_track) {
        BKE_tracking_track_deselect(track, area);
      }
      else {
        tracking_object->active_track = track;
        tracking_object->active_plane_track = nullptr;
      }
    }
    else {
      if (area == TRACK_AREA_POINT) {
        area = TRACK_AREA_ALL;
      }

      BKE_tracking_track_select(&tracking_object->tracks, track, area, extend);
      tracking_object->active_track = track;
      tracking_object->active_plane_track = nullptr;
    }
  }
  else if (pick.plane_track_pick.plane_track != nullptr) {
    if (!extend) {
      ed_tracking_deselect_all_tracks(&tracking_object->tracks);
    }

    MovieTrackingPlaneTrack *plane_track = pick.plane_track_pick.plane_track;

    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      if (extend) {
        plane_track->flag &= ~SELECT;
      }
    }
    else {
      plane_track->flag |= SELECT;
    }

    tracking_object->active_track = nullptr;
    tracking_object->active_plane_track = plane_track;
  }
  else if (deselect_all) {
    ed_tracking_deselect_all_tracks(&tracking_object->tracks);
    ed_tracking_deselect_all_plane_tracks(&tracking_object->plane_tracks);
  }

  ED_clip_view_lock_state_restore_no_jump(C, &lock_state);

  BKE_tracking_dopesheet_tag_update(tracking);

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);

  /* This is a bit implicit, but when the selection operator is used from a LMB Add Marker and
   * tweak tool we do not want the pass-through here and only want selection to happen. This way
   * the selection operator will not fall-through to Add Marker operator. */
  if (activate_selected) {
    if (ed_tracking_pick_can_slide(sc, &pick)) {
      return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
    }

    if (ed_tracking_pick_empty(&pick)) {
      /* When nothing was selected pass-though and allow Add Marker part of the keymap to add new
       * marker at the position. */
      return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
    }

    return OPERATOR_FINISHED;
  }

  /* Pass-through + finished to allow tweak to transform. */
  return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

static int select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  float co[2];
  ED_clip_mouse_pos(sc, region, event->mval, co);
  RNA_float_set_array(op->ptr, "location", co);

  return select_exec(C, op);
}

void CLIP_OT_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select";
  ot->description = "Select tracking markers";
  ot->idname = "CLIP_OT_select";

  /* api callbacks */
  ot->exec = select_exec;
  ot->invoke = select_invoke;
  ot->poll = select_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         0,
                         "Extend",
                         "Extend selection rather than clearing the existing selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
}

bool ED_clip_can_select(bContext *C)
{
  /* To avoid conflicts with mask select deselect all in empty space. */
  return select_poll(C);
}

/********************** box select operator *********************/

static int box_select_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  rcti rect;
  rctf rectf;
  bool changed = false;
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  /* get rectangle from operator */
  WM_operator_properties_border_to_rcti(op, &rect);

  ED_clip_point_stable_pos(sc, region, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
  ED_clip_point_stable_pos(sc, region, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_clip_select_all(sc, SEL_DESELECT, nullptr);
    changed = true;
  }

  /* do actual selection */
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (track->flag & TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

    if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
      if (BLI_rctf_isect_pt_v(&rectf, marker->pos)) {
        if (select) {
          BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
        }
        else {
          BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
        }
      }
      changed = true;
    }
  }

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    if (plane_track->flag & PLANE_TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track,
                                                                                 framenr);

    for (int i = 0; i < 4; i++) {
      if (BLI_rctf_isect_pt_v(&rectf, plane_marker->corners[i])) {
        if (select) {
          plane_track->flag |= SELECT;
        }
        else {
          plane_track->flag &= ~SELECT;
        }
      }
    }
    changed = true;
  }

  if (changed) {
    BKE_tracking_dopesheet_tag_update(&clip->tracking);

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
    DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void CLIP_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Select markers using box selection";
  ot->idname = "CLIP_OT_select_box";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/********************** lasso select operator *********************/

static int do_lasso_select_marker(bContext *C,
                                  const int mcoords[][2],
                                  const int mcoords_len,
                                  bool select)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  rcti rect;
  bool changed = false;
  const int framenr = ED_space_clip_get_clip_frame_number(sc);

  /* get rectangle from operator */
  BLI_lasso_boundbox(&rect, mcoords, mcoords_len);

  /* do actual selection */
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (track->flag & TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

    if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
      float screen_co[2];

      /* marker in screen coords */
      ED_clip_point_stable_pos__reverse(sc, region, marker->pos, screen_co);

      if (BLI_rcti_isect_pt(&rect, screen_co[0], screen_co[1]) &&
          BLI_lasso_is_point_inside(
              mcoords, mcoords_len, screen_co[0], screen_co[1], V2D_IS_CLIPPED))
      {
        if (select) {
          BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
        }
        else {
          BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
        }
      }

      changed = true;
    }
  }

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    if (plane_track->flag & PLANE_TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track,
                                                                                 framenr);

    for (int i = 0; i < 4; i++) {
      float screen_co[2];

      /* marker in screen coords */
      ED_clip_point_stable_pos__reverse(sc, region, plane_marker->corners[i], screen_co);

      if (BLI_rcti_isect_pt(&rect, screen_co[0], screen_co[1]) &&
          BLI_lasso_is_point_inside(
              mcoords, mcoords_len, screen_co[0], screen_co[1], V2D_IS_CLIPPED))
      {
        if (select) {
          plane_track->flag |= SELECT;
        }
        else {
          plane_track->flag &= ~SELECT;
        }
      }
    }

    changed = true;
  }

  if (changed) {
    BKE_tracking_dopesheet_tag_update(&clip->tracking);

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
    DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);
  }

  return changed;
}

static int clip_lasso_select_exec(bContext *C, wmOperator *op)
{
  int mcoords_len;
  const int(*mcoords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcoords_len);

  if (mcoords) {
    const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
    const bool select = (sel_op != SEL_OP_SUB);
    if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
      SpaceClip *sc = CTX_wm_space_clip(C);
      ED_clip_select_all(sc, SEL_DESELECT, nullptr);
    }

    do_lasso_select_marker(C, mcoords, mcoords_len, select);

    MEM_freeN((void *)mcoords);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_select_lasso(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lasso Select";
  ot->description = "Select markers using lasso selection";
  ot->idname = "CLIP_OT_select_lasso";

  /* api callbacks */
  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = clip_lasso_select_exec;
  ot->poll = ED_space_clip_tracking_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/********************** circle select operator *********************/

static int point_inside_ellipse(const float point[2],
                                const float offset[2],
                                const float ellipse[2])
{
  /* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
  float x, y;

  x = (point[0] - offset[0]) * ellipse[0];
  y = (point[1] - offset[1]) * ellipse[1];

  return x * x + y * y < 1.0f;
}

static int marker_inside_ellipse(const MovieTrackingMarker *marker,
                                 const float offset[2],
                                 const float ellipse[2])
{
  return point_inside_ellipse(marker->pos, offset, ellipse);
}

static int circle_select_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);

  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  int width, height;
  bool changed = false;
  float zoomx, zoomy, offset[2], ellipse[2];
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  /* get operator properties */
  const int x = RNA_int_get(op->ptr, "x");
  const int y = RNA_int_get(op->ptr, "y");
  const int radius = RNA_int_get(op->ptr, "radius");

  const eSelectOp sel_op = ED_select_op_modal(
      eSelectOp(RNA_enum_get(op->ptr, "mode")),
      WM_gesture_is_modal_first(static_cast<wmGesture *>(op->customdata)));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_clip_select_all(sc, SEL_DESELECT, nullptr);
    changed = true;
  }

  /* compute ellipse and position in unified coordinates */
  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_zoom(sc, region, &zoomx, &zoomy);

  ellipse[0] = width * zoomx / radius;
  ellipse[1] = height * zoomy / radius;

  ED_clip_point_stable_pos(sc, region, x, y, &offset[0], &offset[1]);

  /* do selection */
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (track->flag & TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

    if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker) &&
        marker_inside_ellipse(marker, offset, ellipse))
    {
      if (select) {
        BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
      }
      else {
        BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
      }
      changed = true;
    }
  }

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    if (plane_track->flag & PLANE_TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track,
                                                                                 framenr);

    for (int i = 0; i < 4; i++) {
      if (point_inside_ellipse(plane_marker->corners[i], offset, ellipse)) {
        if (select) {
          plane_track->flag |= SELECT;
        }
        else {
          plane_track->flag &= ~SELECT;
        }
      }
    }

    changed = true;
  }

  if (changed) {
    BKE_tracking_dopesheet_tag_update(&clip->tracking);

    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
    DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void CLIP_OT_select_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Circle Select";
  ot->description = "Select markers using circle selection";
  ot->idname = "CLIP_OT_select_circle";

  /* api callbacks */
  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = circle_select_exec;
  ot->poll = ED_space_clip_tracking_poll;
  ot->get_name = ED_select_circle_get_name;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/********************** select all operator *********************/

static int select_all_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;

  const int action = RNA_enum_get(op->ptr, "action");

  ClipViewLockState lock_state;
  ED_clip_view_lock_state_store(C, &lock_state);

  bool has_selection = false;
  ED_clip_select_all(sc, action, &has_selection);

  if (has_selection) {
    ED_clip_view_lock_state_restore_no_jump(C, &lock_state);
  }

  BKE_tracking_dopesheet_tag_update(tracking);

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);

  return OPERATOR_FINISHED;
}

void CLIP_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "Change selection of all tracking markers";
  ot->idname = "CLIP_OT_select_all";

  /* api callbacks */
  ot->exec = select_all_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/********************** select grouped operator *********************/

static int select_grouped_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int group = RNA_enum_get(op->ptr, "group");
  const int framenr = ED_space_clip_get_clip_frame_number(sc);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    bool ok = false;

    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

    if (group == 0) { /* Keyframed */
      ok = marker->framenr == framenr && (marker->flag & MARKER_TRACKED) == 0;
    }
    else if (group == 1) { /* Estimated */
      ok = marker->framenr != framenr;
    }
    else if (group == 2) { /* tracked */
      ok = marker->framenr == framenr && (marker->flag & MARKER_TRACKED);
    }
    else if (group == 3) { /* locked */
      ok = track->flag & TRACK_LOCKED;
    }
    else if (group == 4) { /* disabled */
      ok = marker->flag & MARKER_DISABLED;
    }
    else if (group == 5) { /* color */
      const MovieTrackingTrack *act_track = tracking_object->active_track;

      if (act_track) {
        ok = (track->flag & TRACK_CUSTOMCOLOR) == (act_track->flag & TRACK_CUSTOMCOLOR);

        if (ok && track->flag & TRACK_CUSTOMCOLOR) {
          ok = equals_v3v3(track->color, act_track->color);
        }
      }
    }
    else if (group == 6) { /* failed */
      ok = (track->flag & TRACK_HAS_BUNDLE) == 0;
    }

    if (ok) {
      track->flag |= SELECT;
      if (sc->flag & SC_SHOW_MARKER_PATTERN) {
        track->pat_flag |= SELECT;
      }
      if (sc->flag & SC_SHOW_MARKER_SEARCH) {
        track->search_flag |= SELECT;
      }
    }
  }

  BKE_tracking_dopesheet_tag_update(&clip->tracking);

  WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
  DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);

  return OPERATOR_FINISHED;
}

void CLIP_OT_select_grouped(wmOperatorType *ot)
{
  static const EnumPropertyItem select_group_items[] = {
      {0, "KEYFRAMED", 0, "Keyframed Tracks", "Select all keyframed tracks"},
      {1, "ESTIMATED", 0, "Estimated Tracks", "Select all estimated tracks"},
      {2, "TRACKED", 0, "Tracked Tracks", "Select all tracked tracks"},
      {3, "LOCKED", 0, "Locked Tracks", "Select all locked tracks"},
      {4, "DISABLED", 0, "Disabled Tracks", "Select all disabled tracks"},
      {5,
       "COLOR",
       0,
       "Tracks with Same Color",
       "Select all tracks with same color as active track"},
      {6, "FAILED", 0, "Failed Tracks", "Select all tracks which failed to be reconstructed"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Select Grouped";
  ot->description = "Select all tracks from specified group";
  ot->idname = "CLIP_OT_select_grouped";

  /* api callbacks */
  ot->exec = select_grouped_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "group",
               select_group_items,
               TRACK_CLEAR_REMAINED,
               "Action",
               "Clear action to execute");
}
