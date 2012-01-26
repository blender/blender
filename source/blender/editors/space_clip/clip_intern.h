/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_intern.h
 *  \ingroup spclip
 */

#ifndef ED_CLIP_INTERN_H
#define ED_CLIP_INTERN_H

struct bContext;
struct ARegion;
struct MovieClip;
struct MovieTrackingMarker;
struct MovieTrackingTrack;
struct Scene;
struct SpaceClip;
struct wmOperatorType;

/* internal exports only */

/* clip_buttons.c */
void ED_clip_buttons_register(struct ARegionType *art);

/* clip_draw.c */
void clip_draw_main(struct SpaceClip *sc, struct ARegion *ar, struct Scene *scene);
void clip_draw_grease_pencil(struct bContext *C, int onlyv2d);
void clip_draw_curfra_label(struct SpaceClip *sc, float x, float y);

/* clip_graph_draw.c */
void clip_draw_graph(struct SpaceClip *sc, struct ARegion *ar, struct Scene *scene);

/* clip_graph_ops.c */
void CLIP_OT_graph_select(struct wmOperatorType *ot);
void CLIP_OT_graph_delete_curve(struct wmOperatorType *ot);
void CLIP_OT_graph_delete_knot(struct wmOperatorType *ot);
void CLIP_OT_graph_view_all(struct wmOperatorType *ot);
void CLIP_OT_graph_jump_to_current_frame(struct wmOperatorType *ot);

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

/* clip_toolbar.c */
void CLIP_OT_tools(struct wmOperatorType *ot);
void CLIP_OT_properties(struct wmOperatorType *ot);
void ED_clip_tool_props_register(struct ARegionType *art);

/* clip_utils.c */
void clip_graph_tracking_values_iterate_track(struct SpaceClip *sc, struct MovieTrackingTrack *track, void *userdata,
			void (*func) (void *userdata, struct MovieTrackingTrack *track, struct MovieTrackingMarker *marker, int coord, float val),
			void (*segment_start) (void *userdata, struct MovieTrackingTrack *track, int coord),
			void (*segment_end) (void *userdata));

void clip_graph_tracking_values_iterate(struct SpaceClip *sc, void *userdata,
			void (*func) (void *userdata, struct MovieTrackingTrack *track, struct MovieTrackingMarker *marker, int coord, float val),
			void (*segment_start) (void *userdata, struct MovieTrackingTrack *track, int coord),
			void (*segment_end) (void *userdata));

void clip_graph_tracking_iterate(struct SpaceClip *sc, void *userdata,
			void (*func) (void *userdata, struct MovieTrackingMarker *marker));

void clip_delete_track(struct bContext *C, struct MovieClip *clip, struct ListBase *tracksbase, struct MovieTrackingTrack *track);
void clip_delete_marker(struct bContext *C, struct MovieClip *clip, struct ListBase *tracksbase, struct MovieTrackingTrack *track, struct MovieTrackingMarker *marker);

void clip_view_center_to_point(struct SpaceClip *sc, float x, float y);

/* tracking_ops.c */
void CLIP_OT_select(struct wmOperatorType *ot);
void CLIP_OT_select_all(struct wmOperatorType *ot);
void CLIP_OT_select_border(struct wmOperatorType *ot);
void CLIP_OT_select_circle(struct wmOperatorType *ot);
void CLIP_OT_select_grouped(struct wmOperatorType *ot);

void CLIP_OT_add_marker(struct wmOperatorType *ot);
void CLIP_OT_delete_track(struct wmOperatorType *ot);
void CLIP_OT_delete_marker(struct wmOperatorType *ot);

void CLIP_OT_track_markers(struct wmOperatorType *ot);
void CLIP_OT_solve_camera(struct wmOperatorType *ot);
void CLIP_OT_clear_solution(struct wmOperatorType *ot);

void CLIP_OT_clear_track_path(struct wmOperatorType *ot);
void CLIP_OT_join_tracks(struct wmOperatorType *ot);

void CLIP_OT_disable_markers(struct wmOperatorType *ot);
void CLIP_OT_hide_tracks(struct wmOperatorType *ot);
void CLIP_OT_hide_tracks_clear(struct wmOperatorType *ot);
void CLIP_OT_lock_tracks(struct wmOperatorType *ot);

void CLIP_OT_set_origin(struct wmOperatorType *ot);
void CLIP_OT_set_floor(struct wmOperatorType *ot);
void CLIP_OT_set_axis(struct wmOperatorType *ot);
void CLIP_OT_set_scale(struct wmOperatorType *ot);
void CLIP_OT_set_solution_scale(struct wmOperatorType *ot);

void CLIP_OT_set_center_principal(struct wmOperatorType *ot);

void CLIP_OT_slide_marker(struct wmOperatorType *ot);

void CLIP_OT_frame_jump(struct wmOperatorType *ot);
void CLIP_OT_track_copy_color(struct wmOperatorType *ot);

void CLIP_OT_detect_features(struct wmOperatorType *ot);

void CLIP_OT_stabilize_2d_add(struct wmOperatorType *ot);
void CLIP_OT_stabilize_2d_remove(struct wmOperatorType *ot);
void CLIP_OT_stabilize_2d_select(struct wmOperatorType *ot);
void CLIP_OT_stabilize_2d_set_rotation(struct wmOperatorType *ot);

void CLIP_OT_clean_tracks(struct wmOperatorType *ot);

void CLIP_OT_tracking_object_new(struct wmOperatorType *ot);
void CLIP_OT_tracking_object_remove(struct wmOperatorType *ot);

void CLIP_OT_copy_tracks(struct wmOperatorType *ot);
void CLIP_OT_paste_tracks(struct wmOperatorType *ot);

#endif /* ED_CLIP_INTERN_H */
