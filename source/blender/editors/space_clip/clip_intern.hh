/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#pragma once

#include "BLI_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_tracking_types.h"

struct ARegion;
struct MovieClip;
struct MovieTrackingMarker;
struct MovieTrackingTrack;
struct Scene;
struct ScrArea;
struct SpaceClip;
struct bContext;
struct wmOperatorType;

/* channel heights */
#define CHANNEL_FIRST (-UI_TIME_SCRUB_MARGIN_Y - CHANNEL_HEIGHT_HALF - CHANNEL_SKIP)
#define CHANNEL_HEIGHT (0.8f * U.widget_unit)
#define CHANNEL_HEIGHT_HALF (0.4f * U.widget_unit)
#define CHANNEL_SKIP (0.1f * U.widget_unit)
#define CHANNEL_STEP (CHANNEL_HEIGHT + CHANNEL_SKIP)

#define CHANNEL_PAD 4

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD 100.0f

#define STRIP_HEIGHT_HALF (0.25f * UI_UNIT_Y)

/* internal exports only */

/* clip_buttons.cc */

void ED_clip_buttons_register(ARegionType *art);

/* clip_dopesheet_draw.cc */

void clip_draw_dopesheet_main(SpaceClip *sc, ARegion *region, Scene *scene);
void clip_draw_dopesheet_channels(const bContext *C, ARegion *region);

/* clip_dopesheet_ops.cc */

void CLIP_OT_dopesheet_select_channel(wmOperatorType *ot);
void CLIP_OT_dopesheet_view_all(wmOperatorType *ot);

/* clip_draw.cc */

void clip_draw_main(const bContext *C, SpaceClip *sc, ARegion *region);

/* draw grease pencil */

void clip_draw_grease_pencil(bContext *C, int onlyv2d);
void clip_draw_cache_and_notes(const bContext *C, SpaceClip *sc, ARegion *region);

/* clip_editor.cc */

void clip_start_prefetch_job(const bContext *C);

/* clip_graph_draw.cc */

void clip_draw_graph(SpaceClip *sc, ARegion *region, Scene *scene);

/* clip_graph_ops.cc */

void ED_clip_graph_center_current_frame(Scene *scene, ARegion *region);

void CLIP_OT_graph_select(wmOperatorType *ot);
void CLIP_OT_graph_select_box(wmOperatorType *ot);
void CLIP_OT_graph_select_all_markers(wmOperatorType *ot);
void CLIP_OT_graph_delete_curve(wmOperatorType *ot);
void CLIP_OT_graph_delete_knot(wmOperatorType *ot);
void CLIP_OT_graph_view_all(wmOperatorType *ot);
void CLIP_OT_graph_center_current_frame(wmOperatorType *ot);
void CLIP_OT_graph_disable_markers(wmOperatorType *ot);

/* clip_ops.cc */

void CLIP_OT_open(wmOperatorType *ot);
void CLIP_OT_reload(wmOperatorType *ot);
void CLIP_OT_view_pan(wmOperatorType *ot);
void CLIP_OT_view_zoom(wmOperatorType *ot);
void CLIP_OT_view_zoom_in(wmOperatorType *ot);
void CLIP_OT_view_zoom_out(wmOperatorType *ot);
void CLIP_OT_view_zoom_ratio(wmOperatorType *ot);
void CLIP_OT_view_all(wmOperatorType *ot);
void CLIP_OT_view_selected(wmOperatorType *ot);
void CLIP_OT_view_center_cursor(wmOperatorType *ot);
void CLIP_OT_change_frame(wmOperatorType *ot);
void CLIP_OT_rebuild_proxy(wmOperatorType *ot);
void CLIP_OT_mode_set(wmOperatorType *ot);

#ifdef WITH_INPUT_NDOF
void CLIP_OT_view_ndof(wmOperatorType *ot);
#endif

void CLIP_OT_prefetch(wmOperatorType *ot);

void CLIP_OT_set_scene_frames(wmOperatorType *ot);

void CLIP_OT_cursor_set(wmOperatorType *ot);

void CLIP_OT_lock_selection_toggle(wmOperatorType *ot);

/* clip_utils.cc */

enum eClipCurveValueSource {
  CLIP_VALUE_SOURCE_SPEED_X,
  CLIP_VALUE_SOURCE_SPEED_Y,
  CLIP_VALUE_SOURCE_REPROJECTION_ERROR,
};

using ClipTrackValueCallback = void (*)(void *userdata,
                                        MovieTrackingTrack *track,
                                        MovieTrackingMarker *marker,
                                        eClipCurveValueSource value_source,
                                        int scene_framenr,
                                        float val);

using ClipTrackValueSegmentStartCallback = void (*)(void *userdata,
                                                    MovieTrackingTrack *track,
                                                    eClipCurveValueSource value_source,
                                                    bool is_point);

using ClipTrackValueSegmentEndCallback = void (*)(void *userdata,
                                                  eClipCurveValueSource value_source);

bool clip_graph_value_visible(SpaceClip *sc, eClipCurveValueSource value_source);

void clip_graph_tracking_values_iterate_track(SpaceClip *sc,
                                              MovieTrackingTrack *track,
                                              void *userdata,
                                              ClipTrackValueCallback func,
                                              ClipTrackValueSegmentStartCallback segment_start,
                                              ClipTrackValueSegmentEndCallback segment_end);

void clip_graph_tracking_values_iterate(SpaceClip *sc,
                                        bool selected_only,
                                        bool include_hidden,
                                        void *userdata,
                                        ClipTrackValueCallback func,
                                        ClipTrackValueSegmentStartCallback segment_start,
                                        ClipTrackValueSegmentEndCallback segment_end);

void clip_graph_tracking_iterate(SpaceClip *sc,
                                 bool selected_only,
                                 bool include_hidden,
                                 void *userdata,
                                 void (*func)(void *userdata, MovieTrackingMarker *marker));

void clip_delete_track(bContext *C, MovieClip *clip, MovieTrackingTrack *track);
void clip_delete_marker(bContext *C,
                        MovieClip *clip,
                        MovieTrackingTrack *track,
                        MovieTrackingMarker *marker);

void clip_delete_plane_track(bContext *C, MovieClip *clip, MovieTrackingPlaneTrack *plane_track);

/**
 * Calculate space clip offset to be centered at the given point.
 */
void clip_view_offset_for_center_to_point(
    SpaceClip *sc, float x, float y, float *r_offset_x, float *r_offset_y);
void clip_view_center_to_point(SpaceClip *sc, float x, float y);

bool clip_view_calculate_view_selection(
    const bContext *C, bool fit, float *r_offset_x, float *r_offset_y, float *r_zoom);

/**
 * Returns truth if lock-to-selection is enabled and possible.
 * Locking to selection is not possible if there is no selection.
 */
bool clip_view_has_locked_selection(const bContext *C);

void clip_draw_sfra_efra(View2D *v2d, Scene *scene);

/* tracking_ops.cc */

/* Find track which can be slid in a proximity of the given event.
 * Uses the same distance tolerance rule as the "Slide Marker" operator. */
MovieTrackingTrack *tracking_find_slidable_track_in_proximity(bContext *C, const float co[2]);

void CLIP_OT_add_marker(wmOperatorType *ot);
void CLIP_OT_add_marker_at_click(wmOperatorType *ot);
void CLIP_OT_delete_track(wmOperatorType *ot);
void CLIP_OT_delete_marker(wmOperatorType *ot);

void CLIP_OT_track_markers(wmOperatorType *ot);
void CLIP_OT_refine_markers(wmOperatorType *ot);
void CLIP_OT_solve_camera(wmOperatorType *ot);
void CLIP_OT_clear_solution(wmOperatorType *ot);

void CLIP_OT_clear_track_path(wmOperatorType *ot);
void CLIP_OT_join_tracks(wmOperatorType *ot);
void CLIP_OT_average_tracks(wmOperatorType *ot);

void CLIP_OT_disable_markers(wmOperatorType *ot);
void CLIP_OT_hide_tracks(wmOperatorType *ot);
void CLIP_OT_hide_tracks_clear(wmOperatorType *ot);
void CLIP_OT_lock_tracks(wmOperatorType *ot);

void CLIP_OT_set_solver_keyframe(wmOperatorType *ot);

void CLIP_OT_set_origin(wmOperatorType *ot);
void CLIP_OT_set_plane(wmOperatorType *ot);
void CLIP_OT_set_axis(wmOperatorType *ot);
void CLIP_OT_set_scale(wmOperatorType *ot);
void CLIP_OT_set_solution_scale(wmOperatorType *ot);
void CLIP_OT_apply_solution_scale(wmOperatorType *ot);

void CLIP_OT_slide_marker(wmOperatorType *ot);

void CLIP_OT_frame_jump(wmOperatorType *ot);
void CLIP_OT_track_copy_color(wmOperatorType *ot);

void CLIP_OT_detect_features(wmOperatorType *ot);

void CLIP_OT_stabilize_2d_add(wmOperatorType *ot);
void CLIP_OT_stabilize_2d_remove(wmOperatorType *ot);
void CLIP_OT_stabilize_2d_select(wmOperatorType *ot);

void CLIP_OT_stabilize_2d_rotation_add(wmOperatorType *ot);
void CLIP_OT_stabilize_2d_rotation_remove(wmOperatorType *ot);
void CLIP_OT_stabilize_2d_rotation_select(wmOperatorType *ot);

void CLIP_OT_clean_tracks(wmOperatorType *ot);

void CLIP_OT_tracking_object_new(wmOperatorType *ot);
void CLIP_OT_tracking_object_remove(wmOperatorType *ot);

void CLIP_OT_copy_tracks(wmOperatorType *ot);
void CLIP_OT_paste_tracks(wmOperatorType *ot);

void CLIP_OT_create_plane_track(wmOperatorType *ot);
void CLIP_OT_slide_plane_marker(wmOperatorType *ot);

void CLIP_OT_keyframe_insert(wmOperatorType *ot);
void CLIP_OT_keyframe_delete(wmOperatorType *ot);

void CLIP_OT_new_image_from_plane_marker(wmOperatorType *ot);
void CLIP_OT_update_image_from_plane_marker(wmOperatorType *ot);

/* tracking_select.cc */

void CLIP_OT_select(wmOperatorType *ot);
void CLIP_OT_select_all(wmOperatorType *ot);
void CLIP_OT_select_box(wmOperatorType *ot);
void CLIP_OT_select_lasso(wmOperatorType *ot);
void CLIP_OT_select_circle(wmOperatorType *ot);
void CLIP_OT_select_grouped(wmOperatorType *ot);

/* -------------------------------------------------------------------- */
/** \name Inlined utilities.
 * \{ */

/* Check whether the marker can is visible within the given context.
 * The track must be visible, and no restrictions from the clip editor are to be in effect on the
 * disabled marker visibility (unless the track is active). */
BLI_INLINE bool ED_space_clip_marker_is_visible(const SpaceClip *space_clip,
                                                const MovieTrackingObject *tracking_object,
                                                const MovieTrackingTrack *track,
                                                const MovieTrackingMarker *marker)
{
  if (track->flag & TRACK_HIDDEN) {
    return false;
  }

  if ((marker->flag & MARKER_DISABLED) == 0) {
    return true;
  }

  if ((space_clip->flag & SC_HIDE_DISABLED) == 0) {
    return true;
  }

  return track == tracking_object->active_track;
}

/** \} */
