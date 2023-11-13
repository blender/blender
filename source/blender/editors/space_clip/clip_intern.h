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

#ifdef __cplusplus
extern "C" {
#endif

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

void ED_clip_buttons_register(struct ARegionType *art);

/* clip_dopesheet_draw.cc */

void clip_draw_dopesheet_main(struct SpaceClip *sc, struct ARegion *region, struct Scene *scene);
void clip_draw_dopesheet_channels(const struct bContext *C, struct ARegion *region);

/* clip_dopesheet_ops.cc */

void CLIP_OT_dopesheet_select_channel(struct wmOperatorType *ot);
void CLIP_OT_dopesheet_view_all(struct wmOperatorType *ot);

/* clip_draw.cc */

void clip_draw_main(const struct bContext *C, struct SpaceClip *sc, struct ARegion *region);

/* draw grease pencil */

void clip_draw_grease_pencil(struct bContext *C, int onlyv2d);
void clip_draw_cache_and_notes(const bContext *C, SpaceClip *sc, ARegion *region);

/* clip_editor.cc */

void clip_start_prefetch_job(const struct bContext *C);

/* clip_graph_draw.cc */

void clip_draw_graph(struct SpaceClip *sc, struct ARegion *region, struct Scene *scene);

/* clip_graph_ops.cc */

void ED_clip_graph_center_current_frame(struct Scene *scene, struct ARegion *region);

void CLIP_OT_graph_select(struct wmOperatorType *ot);
void CLIP_OT_graph_select_box(struct wmOperatorType *ot);
void CLIP_OT_graph_select_all_markers(struct wmOperatorType *ot);
void CLIP_OT_graph_delete_curve(struct wmOperatorType *ot);
void CLIP_OT_graph_delete_knot(struct wmOperatorType *ot);
void CLIP_OT_graph_view_all(struct wmOperatorType *ot);
void CLIP_OT_graph_center_current_frame(struct wmOperatorType *ot);
void CLIP_OT_graph_disable_markers(struct wmOperatorType *ot);

/* clip_ops.cc */

void CLIP_OT_open(struct wmOperatorType *ot);
void CLIP_OT_reload(struct wmOperatorType *ot);
void CLIP_OT_view_pan(struct wmOperatorType *ot);
void CLIP_OT_view_zoom(wmOperatorType *ot);
void CLIP_OT_view_zoom_in(struct wmOperatorType *ot);
void CLIP_OT_view_zoom_out(struct wmOperatorType *ot);
void CLIP_OT_view_zoom_ratio(struct wmOperatorType *ot);
void CLIP_OT_view_all(struct wmOperatorType *ot);
void CLIP_OT_view_selected(struct wmOperatorType *ot);
void CLIP_OT_view_center_cursor(struct wmOperatorType *ot);
void CLIP_OT_change_frame(wmOperatorType *ot);
void CLIP_OT_rebuild_proxy(struct wmOperatorType *ot);
void CLIP_OT_mode_set(struct wmOperatorType *ot);

#ifdef WITH_INPUT_NDOF
void CLIP_OT_view_ndof(struct wmOperatorType *ot);
#endif

void CLIP_OT_prefetch(struct wmOperatorType *ot);

void CLIP_OT_set_scene_frames(wmOperatorType *ot);

void CLIP_OT_cursor_set(struct wmOperatorType *ot);

void CLIP_OT_lock_selection_toggle(struct wmOperatorType *ot);

/* clip_utils.cc */

typedef enum {
  CLIP_VALUE_SOURCE_SPEED_X,
  CLIP_VALUE_SOURCE_SPEED_Y,
  CLIP_VALUE_SOURCE_REPROJECTION_ERROR,
} eClipCurveValueSource;

typedef void (*ClipTrackValueCallback)(void *userdata,
                                       struct MovieTrackingTrack *track,
                                       struct MovieTrackingMarker *marker,
                                       eClipCurveValueSource value_source,
                                       int scene_framenr,
                                       float val);

typedef void (*ClipTrackValueSegmentStartCallback)(void *userdata,
                                                   struct MovieTrackingTrack *track,
                                                   eClipCurveValueSource value_source,
                                                   bool is_point);

typedef void (*ClipTrackValueSegmentEndCallback)(void *userdata,
                                                 eClipCurveValueSource value_source);

bool clip_graph_value_visible(struct SpaceClip *sc, eClipCurveValueSource value_source);

void clip_graph_tracking_values_iterate_track(struct SpaceClip *sc,
                                              struct MovieTrackingTrack *track,
                                              void *userdata,
                                              ClipTrackValueCallback func,
                                              ClipTrackValueSegmentStartCallback segment_start,
                                              ClipTrackValueSegmentEndCallback segment_end);

void clip_graph_tracking_values_iterate(struct SpaceClip *sc,
                                        bool selected_only,
                                        bool include_hidden,
                                        void *userdata,
                                        ClipTrackValueCallback func,
                                        ClipTrackValueSegmentStartCallback segment_start,
                                        ClipTrackValueSegmentEndCallback segment_end);

void clip_graph_tracking_iterate(struct SpaceClip *sc,
                                 bool selected_only,
                                 bool include_hidden,
                                 void *userdata,
                                 void (*func)(void *userdata, struct MovieTrackingMarker *marker));

void clip_delete_track(struct bContext *C,
                       struct MovieClip *clip,
                       struct MovieTrackingTrack *track);
void clip_delete_marker(struct bContext *C,
                        struct MovieClip *clip,
                        struct MovieTrackingTrack *track,
                        struct MovieTrackingMarker *marker);

void clip_delete_plane_track(struct bContext *C,
                             struct MovieClip *clip,
                             struct MovieTrackingPlaneTrack *plane_track);

/**
 * Calculate space clip offset to be centered at the given point.
 */
void clip_view_offset_for_center_to_point(
    SpaceClip *sc, float x, float y, float *r_offset_x, float *r_offset_y);
void clip_view_center_to_point(SpaceClip *sc, float x, float y);

bool clip_view_calculate_view_selection(
    const struct bContext *C, bool fit, float *r_offset_x, float *r_offset_y, float *r_zoom);

/**
 * Returns truth if lock-to-selection is enabled and possible.
 * Locking to selection is not possible if there is no selection.
 */
bool clip_view_has_locked_selection(const struct bContext *C);

void clip_draw_sfra_efra(struct View2D *v2d, struct Scene *scene);

/* tracking_ops.cc */

/* Find track which can be slid in a proximity of the given event.
 * Uses the same distance tolerance rule as the "Slide Marker" operator. */
struct MovieTrackingTrack *tracking_find_slidable_track_in_proximity(struct bContext *C,
                                                                     const float co[2]);

void CLIP_OT_add_marker(struct wmOperatorType *ot);
void CLIP_OT_add_marker_at_click(struct wmOperatorType *ot);
void CLIP_OT_delete_track(struct wmOperatorType *ot);
void CLIP_OT_delete_marker(struct wmOperatorType *ot);

void CLIP_OT_track_markers(struct wmOperatorType *ot);
void CLIP_OT_refine_markers(struct wmOperatorType *ot);
void CLIP_OT_solve_camera(struct wmOperatorType *ot);
void CLIP_OT_clear_solution(struct wmOperatorType *ot);

void CLIP_OT_clear_track_path(struct wmOperatorType *ot);
void CLIP_OT_join_tracks(struct wmOperatorType *ot);
void CLIP_OT_average_tracks(struct wmOperatorType *ot);

void CLIP_OT_disable_markers(struct wmOperatorType *ot);
void CLIP_OT_hide_tracks(struct wmOperatorType *ot);
void CLIP_OT_hide_tracks_clear(struct wmOperatorType *ot);
void CLIP_OT_lock_tracks(struct wmOperatorType *ot);

void CLIP_OT_set_solver_keyframe(struct wmOperatorType *ot);

void CLIP_OT_set_origin(struct wmOperatorType *ot);
void CLIP_OT_set_plane(struct wmOperatorType *ot);
void CLIP_OT_set_axis(struct wmOperatorType *ot);
void CLIP_OT_set_scale(struct wmOperatorType *ot);
void CLIP_OT_set_solution_scale(struct wmOperatorType *ot);
void CLIP_OT_apply_solution_scale(struct wmOperatorType *ot);

void CLIP_OT_slide_marker(struct wmOperatorType *ot);

void CLIP_OT_frame_jump(struct wmOperatorType *ot);
void CLIP_OT_track_copy_color(struct wmOperatorType *ot);

void CLIP_OT_detect_features(struct wmOperatorType *ot);

void CLIP_OT_stabilize_2d_add(struct wmOperatorType *ot);
void CLIP_OT_stabilize_2d_remove(struct wmOperatorType *ot);
void CLIP_OT_stabilize_2d_select(struct wmOperatorType *ot);

void CLIP_OT_stabilize_2d_rotation_add(struct wmOperatorType *ot);
void CLIP_OT_stabilize_2d_rotation_remove(struct wmOperatorType *ot);
void CLIP_OT_stabilize_2d_rotation_select(struct wmOperatorType *ot);

void CLIP_OT_clean_tracks(struct wmOperatorType *ot);

void CLIP_OT_tracking_object_new(struct wmOperatorType *ot);
void CLIP_OT_tracking_object_remove(struct wmOperatorType *ot);

void CLIP_OT_copy_tracks(struct wmOperatorType *ot);
void CLIP_OT_paste_tracks(struct wmOperatorType *ot);

void CLIP_OT_create_plane_track(struct wmOperatorType *ot);
void CLIP_OT_slide_plane_marker(struct wmOperatorType *ot);

void CLIP_OT_keyframe_insert(struct wmOperatorType *ot);
void CLIP_OT_keyframe_delete(struct wmOperatorType *ot);

void CLIP_OT_new_image_from_plane_marker(struct wmOperatorType *ot);
void CLIP_OT_update_image_from_plane_marker(struct wmOperatorType *ot);

/* tracking_select.cc */

void CLIP_OT_select(struct wmOperatorType *ot);
void CLIP_OT_select_all(struct wmOperatorType *ot);
void CLIP_OT_select_box(struct wmOperatorType *ot);
void CLIP_OT_select_lasso(struct wmOperatorType *ot);
void CLIP_OT_select_circle(struct wmOperatorType *ot);
void CLIP_OT_select_grouped(struct wmOperatorType *ot);

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

#ifdef __cplusplus
}
#endif
