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

#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "clip_intern.h"  // own include

bool clip_graph_value_visible(SpaceClip *sc, eClipCurveValueSource value_source)
{
  if (ELEM(value_source, CLIP_VALUE_SOURCE_SPEED_X, CLIP_VALUE_SOURCE_SPEED_Y)) {
    if ((sc->flag & SC_SHOW_GRAPH_TRACKS_MOTION) == 0) {
      return false;
    }
  }
  else if (value_source == CLIP_VALUE_SOURCE_REPROJECTION_ERROR) {
    if ((sc->flag & SC_SHOW_GRAPH_TRACKS_ERROR) == 0) {
      return false;
    }
  }
  return true;
}

static void clip_graph_tracking_values_iterate_track_speed_values(
    SpaceClip *sc,
    MovieTrackingTrack *track,
    void *userdata,
    ClipTrackValueCallback func,
    ClipTrackValueSegmentStartCallback segment_start,
    ClipTrackValueSegmentEndCallback segment_end)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int width, height, coord;

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  for (coord = 0; coord < 2; coord++) {
    eClipCurveValueSource value_source = (coord == 0) ? CLIP_VALUE_SOURCE_SPEED_X :
                                                        CLIP_VALUE_SOURCE_SPEED_Y;
    int i, prevfra = track->markers[0].framenr;
    bool open = false;
    float prevval = 0.0f;

    for (i = 0; i < track->markersnr; i++) {
      MovieTrackingMarker *marker = &track->markers[i];
      float val;

      if (marker->flag & MARKER_DISABLED) {
        if (open) {
          if (segment_end) {
            segment_end(userdata, value_source);
          }

          open = false;
        }

        continue;
      }

      if (!open) {
        if (segment_start) {
          if ((i + 1) == track->markersnr) {
            segment_start(userdata, track, value_source, true);
          }
          else {
            segment_start(
                userdata, track, value_source, (track->markers[i + 1].flag & MARKER_DISABLED));
          }
        }

        open = true;
        prevval = marker->pos[coord];
      }

      /* value is a pixels per frame speed */
      val = (marker->pos[coord] - prevval) * ((coord == 0) ? (width) : (height));
      val /= marker->framenr - prevfra;

      if (func) {
        int scene_framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);

        func(userdata, track, marker, value_source, scene_framenr, val);
      }

      prevval = marker->pos[coord];
      prevfra = marker->framenr;
    }

    if (open) {
      if (segment_end) {
        segment_end(userdata, value_source);
      }
    }
  }
}

static float calculate_reprojection_error_at_marker(MovieClip *clip,
                                                    MovieTracking *tracking,
                                                    MovieTrackingObject *tracking_object,
                                                    MovieTrackingTrack *track,
                                                    MovieTrackingMarker *marker,
                                                    const int clip_width,
                                                    const int clip_height,
                                                    const int scene_framenr)
{
  float reprojected_position[4], bundle_position[4], marker_position[2], delta[2];
  float weight = BKE_tracking_track_get_weight_for_marker(clip, track, marker);
  const float aspy = 1.0f / tracking->camera.pixel_aspect;

  float projection_matrix[4][4];
  BKE_tracking_get_projection_matrix(
      tracking, tracking_object, scene_framenr, clip_width, clip_height, projection_matrix);

  copy_v3_v3(bundle_position, track->bundle_pos);
  bundle_position[3] = 1;

  mul_v4_m4v4(reprojected_position, projection_matrix, bundle_position);
  reprojected_position[0] = (reprojected_position[0] / (reprojected_position[3] * 2.0f) + 0.5f) *
                            clip_width;
  reprojected_position[1] = (reprojected_position[1] / (reprojected_position[3] * 2.0f) + 0.5f) *
                            clip_height * aspy;

  BKE_tracking_distort_v2(tracking, reprojected_position, reprojected_position);

  marker_position[0] = (marker->pos[0] + track->offset[0]) * clip_width;
  marker_position[1] = (marker->pos[1] + track->offset[1]) * clip_height * aspy;

  sub_v2_v2v2(delta, reprojected_position, marker_position);
  return len_v2(delta) * weight;
}

static void clip_graph_tracking_values_iterate_track_reprojection_error_values(
    SpaceClip *sc,
    MovieTrackingTrack *track,
    void *userdata,
    ClipTrackValueCallback func,
    ClipTrackValueSegmentStartCallback segment_start,
    ClipTrackValueSegmentEndCallback segment_end)
{
  /* Tracks without bundle can not have any reprojection error curve. */
  if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
    return;
  }

  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

  int clip_width, clip_height;
  BKE_movieclip_get_size(clip, &sc->user, &clip_width, &clip_height);

  /* Iterate over segments. */
  bool is_segment_open = false;
  for (int marker_index = 0; marker_index < track->markersnr; marker_index++) {
    MovieTrackingMarker *marker = &track->markers[marker_index];

    /* End of tracked segment, no reprojection error can be calculated here since the ground truth
     * 2D position is not known. */
    if (marker->flag & MARKER_DISABLED) {
      if (is_segment_open) {
        if (segment_end != NULL) {
          segment_end(userdata, CLIP_VALUE_SOURCE_REPROJECTION_ERROR);
        }
        is_segment_open = false;
      }
      continue;
    }

    /* Begin new segment if it is not open yet. */
    if (!is_segment_open) {
      if (segment_start != NULL) {
        if ((marker_index + 1) == track->markersnr) {
          segment_start(userdata, track, CLIP_VALUE_SOURCE_REPROJECTION_ERROR, true);
        }
        else {
          segment_start(userdata,
                        track,
                        CLIP_VALUE_SOURCE_REPROJECTION_ERROR,
                        (track->markers[marker_index + 1].flag & MARKER_DISABLED));
        }
      }
      is_segment_open = true;
    }

    if (func != NULL) {
      const int scene_framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);
      const float reprojection_error = calculate_reprojection_error_at_marker(
          clip, tracking, tracking_object, track, marker, clip_width, clip_height, scene_framenr);
      func(userdata,
           track,
           marker,
           CLIP_VALUE_SOURCE_REPROJECTION_ERROR,
           scene_framenr,
           reprojection_error);
    }
  }

  if (is_segment_open && segment_end != NULL) {
    segment_end(userdata, CLIP_VALUE_SOURCE_REPROJECTION_ERROR);
  }
}

void clip_graph_tracking_values_iterate_track(SpaceClip *sc,
                                              MovieTrackingTrack *track,
                                              void *userdata,
                                              ClipTrackValueCallback func,
                                              ClipTrackValueSegmentStartCallback segment_start,
                                              ClipTrackValueSegmentEndCallback segment_end)
{
  clip_graph_tracking_values_iterate_track_speed_values(
      sc, track, userdata, func, segment_start, segment_end);

  clip_graph_tracking_values_iterate_track_reprojection_error_values(
      sc, track, userdata, func, segment_start, segment_end);
}

void clip_graph_tracking_values_iterate(SpaceClip *sc,
                                        bool selected_only,
                                        bool include_hidden,
                                        void *userdata,
                                        ClipTrackValueCallback func,
                                        ClipTrackValueSegmentStartCallback segment_start,
                                        ClipTrackValueSegmentEndCallback segment_end)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  MovieTrackingTrack *track;

  for (track = tracksbase->first; track; track = track->next) {
    if (!include_hidden && (track->flag & TRACK_HIDDEN) != 0) {
      continue;
    }

    if (selected_only && !TRACK_SELECTED(track)) {
      continue;
    }

    clip_graph_tracking_values_iterate_track(
        sc, track, userdata, func, segment_start, segment_end);
  }
}

void clip_graph_tracking_iterate(SpaceClip *sc,
                                 bool selected_only,
                                 bool include_hidden,
                                 void *userdata,
                                 void (*func)(void *userdata, MovieTrackingMarker *marker))
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  MovieTrackingTrack *track;

  for (track = tracksbase->first; track; track = track->next) {
    int i;

    if (!include_hidden && (track->flag & TRACK_HIDDEN) != 0) {
      continue;
    }

    if (selected_only && !TRACK_SELECTED(track)) {
      continue;
    }

    for (i = 0; i < track->markersnr; i++) {
      MovieTrackingMarker *marker = &track->markers[i];

      if (marker->flag & MARKER_DISABLED) {
        continue;
      }

      if (func) {
        func(userdata, marker);
      }
    }
  }
}

void clip_delete_track(bContext *C, MovieClip *clip, MovieTrackingTrack *track)
{
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  bool has_bundle = false;
  const bool used_for_stabilization = (track->flag &
                                       (TRACK_USE_2D_STAB | TRACK_USE_2D_STAB_ROT)) != 0;
  if (track == act_track) {
    tracking->act_track = NULL;
  }
  /* Handle reconstruction display in 3d viewport. */
  if (track->flag & TRACK_HAS_BUNDLE) {
    has_bundle = true;
  }
  /* Make sure no plane will use freed track */
  BKE_tracking_plane_tracks_remove_point_track(tracking, track);
  /* Delete f-curves associated with the track (such as weight, i.e.) */
  /* Escaped object name, escaped track name, rest of the path. */
  char rna_path[MAX_NAME * 4 + 64];
  BKE_tracking_get_rna_path_for_track(tracking, track, rna_path, sizeof(rna_path));
  if (BKE_animdata_fix_paths_remove(&clip->id, rna_path)) {
    DEG_relations_tag_update(CTX_data_main(C));
  }
  /* Delete track itself. */
  BKE_tracking_track_free(track);
  BLI_freelinkN(tracksbase, track);
  /* Send notifiers. */
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
  if (used_for_stabilization) {
    WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
  }
  /* Inform dependency graph. */
  DEG_id_tag_update(&clip->id, 0);
  if (has_bundle) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  }
}

void clip_delete_marker(bContext *C,
                        MovieClip *clip,
                        MovieTrackingTrack *track,
                        MovieTrackingMarker *marker)
{
  if (track->markersnr == 1) {
    clip_delete_track(C, clip, track);
  }
  else {
    BKE_tracking_marker_delete(track, marker->framenr);

    WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
  }
}

void clip_delete_plane_track(bContext *C, MovieClip *clip, MovieTrackingPlaneTrack *plane_track)
{
  MovieTracking *tracking = &clip->tracking;
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
  /* Delete f-curves associated with the track (such as weight, i.e.) */
  /* Escaped object name, escaped track name, rest of the path. */
  char rna_path[MAX_NAME * 4 + 64];
  BKE_tracking_get_rna_path_for_plane_track(tracking, plane_track, rna_path, sizeof(rna_path));
  if (BKE_animdata_fix_paths_remove(&clip->id, rna_path)) {
    DEG_relations_tag_update(CTX_data_main(C));
  }
  /* Delete the plane track itself. */
  BKE_tracking_plane_track_free(plane_track);
  BLI_freelinkN(plane_tracks_base, plane_track);
  /* TODO(sergey): Any notifiers to be sent here? */
  (void)C;
  /* Inform dependency graph. */
  DEG_id_tag_update(&clip->id, 0);
}

void clip_view_center_to_point(SpaceClip *sc, float x, float y)
{
  int width, height;
  float aspx, aspy;

  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  sc->xof = (x - 0.5f) * width * aspx;
  sc->yof = (y - 0.5f) * height * aspy;
}

void clip_draw_sfra_efra(View2D *v2d, Scene *scene)
{
  UI_view2d_view_ortho(v2d);

  /* currently clip editor supposes that editing clip length is equal to scene frame range */
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4f(0.0f, 0.0f, 0.0f, 0.4f);
  immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, (float)SFRA, v2d->cur.ymax);
  immRectf(pos, (float)EFRA, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);

  GPU_blend(false);

  immUniformThemeColorShade(TH_BACK, -60);

  /* thin lines where the actual frames are */
  GPU_line_width(1.0f);

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, (float)SFRA, v2d->cur.ymin);
  immVertex2f(pos, (float)SFRA, v2d->cur.ymax);
  immVertex2f(pos, (float)EFRA, v2d->cur.ymin);
  immVertex2f(pos, (float)EFRA, v2d->cur.ymax);
  immEnd();

  immUnbindProgram();
}
