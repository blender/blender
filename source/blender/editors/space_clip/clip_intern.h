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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#ifndef __CLIP_INTERN_H__
#define __CLIP_INTERN_H__

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

/* clip_buttons.c */
void ED_clip_buttons_register(struct ARegionType *art);

/* clip_dopesheet_draw.c */
void clip_draw_dopesheet_main(struct SpaceClip *sc, struct ARegion *ar, struct Scene *scene);
void clip_draw_dopesheet_channels(const struct bContext *C, struct ARegion *ar);

/* clip_dopesheet_ops.c */
void CLIP_OT_dopesheet_select_channel(struct wmOperatorType *ot);
void CLIP_OT_dopesheet_view_all(struct wmOperatorType *ot);

/* clip_draw.c */
void clip_draw_main(const struct bContext *C, struct SpaceClip *sc, struct ARegion *ar);
void clip_draw_grease_pencil(struct bContext *C, int onlyv2d);
void clip_draw_cache_and_notes(const bContext *C, SpaceClip *sc, ARegion *ar);

/* clip_editor.c */
void clip_start_prefetch_job(const struct bContext *C);

/* clip_graph_draw.c */
void clip_draw_graph(struct SpaceClip *sc, struct ARegion *ar, struct Scene *scene);

/* clip_graph_ops.c */
void ED_clip_graph_center_current_frame(struct Scene *scene, struct ARegion *ar);

void CLIP_OT_graph_select(struct wmOperatorType *ot);
void CLIP_OT_graph_select_box(struct wmOperatorType *ot);
void CLIP_OT_graph_select_all_markers(struct wmOperatorType *ot);
void CLIP_OT_graph_delete_curve(struct wmOperatorType *ot);
void CLIP_OT_graph_delete_knot(struct wmOperatorType *ot);
void CLIP_OT_graph_view_all(struct wmOperatorType *ot);
void CLIP_OT_graph_center_current_frame(struct wmOperatorType *ot);
void CLIP_OT_graph_disable_markers(struct wmOperatorType *ot);

/* clip_ops.c */
void CLIP_OT_open(struct wmOperatorType *ot);
void CLIP_OT_reload(struct wmOperatorType *ot);
void CLIP_OT_view_pan(struct wmOperatorType *ot);
void CLIP_OT_view_zoom(wmOperatorType *ot);
void CLIP_OT_view_zoom_in(struct wmOperatorType *ot);
void CLIP_OT_view_zoom_out(struct wmOperatorType *ot);
void CLIP_OT_view_zoom_ratio(struct wmOperatorType *ot);
void CLIP_OT_view_all(struct wmOperatorType *ot);
void CLIP_OT_view_selected(struct wmOperatorType *ot);
void CLIP_OT_change_frame(wmOperatorType *ot);
void CLIP_OT_rebuild_proxy(struct wmOperatorType *ot);
void CLIP_OT_mode_set(struct wmOperatorType *ot);

#ifdef WITH_INPUT_NDOF
void CLIP_OT_view_ndof(struct wmOperatorType *ot);
#endif

void CLIP_OT_prefetch(struct wmOperatorType *ot);

void CLIP_OT_set_scene_frames(wmOperatorType *ot);

void CLIP_OT_cursor_set(struct wmOperatorType *ot);

/* clip_toolbar.c */
struct ARegion *ED_clip_has_properties_region(struct ScrArea *sa);

/* clip_utils.c */

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

void clip_view_center_to_point(SpaceClip *sc, float x, float y);

void clip_draw_sfra_efra(struct View2D *v2d, struct Scene *scene);

/* tracking_ops.c */
struct MovieTrackingTrack *tracking_marker_check_slide(
    struct bContext *C, const struct wmEvent *event, int *area_r, int *action_r, int *corner_r);

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

void CLIP_OT_set_center_principal(struct wmOperatorType *ot);

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

/* tracking_select.c */
void CLIP_OT_select(struct wmOperatorType *ot);
void CLIP_OT_select_all(struct wmOperatorType *ot);
void CLIP_OT_select_box(struct wmOperatorType *ot);
void CLIP_OT_select_lasso(struct wmOperatorType *ot);
void CLIP_OT_select_circle(struct wmOperatorType *ot);
void CLIP_OT_select_grouped(struct wmOperatorType *ot);

#endif /* __CLIP_INTERN_H__ */
