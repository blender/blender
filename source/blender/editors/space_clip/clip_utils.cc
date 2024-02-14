/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh"
#include "ED_mask.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "clip_intern.h" /* own include */

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

  BKE_tracking_distort_v2(
      tracking, clip_width, clip_height, reprojected_position, reprojected_position);

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
        if (segment_end != nullptr) {
          segment_end(userdata, CLIP_VALUE_SOURCE_REPROJECTION_ERROR);
        }
        is_segment_open = false;
      }
      continue;
    }

    /* Begin new segment if it is not open yet. */
    if (!is_segment_open) {
      if (segment_start != nullptr) {
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

    if (func != nullptr) {
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

  if (is_segment_open && segment_end != nullptr) {
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
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
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
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (!include_hidden && (track->flag & TRACK_HIDDEN) != 0) {
      continue;
    }

    if (selected_only && !TRACK_SELECTED(track)) {
      continue;
    }

    for (int i = 0; i < track->markersnr; i++) {
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
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  bool has_bundle = false;
  const bool used_for_stabilization = (track->flag &
                                       (TRACK_USE_2D_STAB | TRACK_USE_2D_STAB_ROT)) != 0;
  if (track == tracking_object->active_track) {
    tracking_object->active_track = nullptr;
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
  BLI_freelinkN(&tracking_object->tracks, track);
  /* Send notifiers. */
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
  if (used_for_stabilization) {
    WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
  }
  /* Inform dependency graph. */
  DEG_id_tag_update(&clip->id, 0);
  if (has_bundle) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
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
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  if (plane_track == tracking_object->active_plane_track) {
    tracking_object->active_plane_track = nullptr;
  }

  /* Delete f-curves associated with the track (such as weight, i.e.) */
  /* Escaped object name, escaped track name, rest of the path. */
  char rna_path[MAX_NAME * 4 + 64];
  BKE_tracking_get_rna_path_for_plane_track(tracking, plane_track, rna_path, sizeof(rna_path));
  if (BKE_animdata_fix_paths_remove(&clip->id, rna_path)) {
    DEG_relations_tag_update(CTX_data_main(C));
  }
  /* Delete the plane track itself. */
  BKE_tracking_plane_track_free(plane_track);
  BLI_freelinkN(&tracking_object->plane_tracks, plane_track);
  /* TODO(sergey): Any notifiers to be sent here? */
  (void)C;
  /* Inform dependency graph. */
  DEG_id_tag_update(&clip->id, 0);
}

void clip_view_offset_for_center_to_point(
    SpaceClip *sc, const float x, const float y, float *r_offset_x, float *r_offset_y)
{
  int width, height;
  ED_space_clip_get_size(sc, &width, &height);

  float aspx, aspy;
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  *r_offset_x = (x - 0.5f) * width * aspx;
  *r_offset_y = (y - 0.5f) * height * aspy;
}

void clip_view_center_to_point(SpaceClip *sc, float x, float y)
{
  clip_view_offset_for_center_to_point(sc, x, y, &sc->xof, &sc->yof);
}

static bool selected_tracking_boundbox(SpaceClip *sc, float min[2], float max[2])
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  int width, height;
  bool ok = false;

  INIT_MINMAX2(min, max);

  ED_space_clip_get_size(sc, &width, &height);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

      if (marker) {
        float pos[3];

        pos[0] = marker->pos[0] + track->offset[0];
        pos[1] = marker->pos[1] + track->offset[1];
        pos[2] = 0.0f;

        /* undistortion happens for normalized coords */
        if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
          /* undistortion happens for normalized coords */
          ED_clip_point_undistorted_pos(sc, pos, pos);
        }

        pos[0] *= width;
        pos[1] *= height;

        mul_v3_m4v3(pos, sc->stabmat, pos);

        minmax_v2v2_v2(min, max, pos);

        ok = true;
      }
    }
  }

  return ok;
}

static bool tracking_has_selection(SpaceClip *space_clip)
{
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(space_clip);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (!TRACK_VIEW_SELECTED(space_clip, track)) {
      continue;
    }
    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
    if (marker != nullptr) {
      return true;
    }
  }

  return false;
}

static bool mask_has_selection(const bContext *C)
{
  Mask *mask = CTX_data_edit_mask(C);
  if (mask == nullptr) {
    return false;
  }

  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    if (mask_layer->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }
    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      for (int i = 0; i < spline->tot_point; i++) {
        const MaskSplinePoint *point = &spline->points[i];
        const BezTriple *bezt = &point->bezt;
        if (!MASKPOINT_ISSEL_ANY(point)) {
          continue;
        }
        if (bezt->f2 & SELECT) {
          return true;
        }

        if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
          return true;
        }

        if ((bezt->f1 & SELECT) && (bezt->h1 != HD_VECT)) {
          return true;
        }
        if ((bezt->f3 & SELECT) && (bezt->h2 != HD_VECT)) {
          return true;
        }
      }
    }
  }

  return false;
}

static bool selected_boundbox(const bContext *C,
                              float min[2],
                              float max[2],
                              bool handles_as_control_point)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  if (sc->mode == SC_MODE_TRACKING) {
    return selected_tracking_boundbox(sc, min, max);
  }

  if (ED_mask_selected_minmax(C, min, max, handles_as_control_point)) {
    MovieClip *clip = ED_space_clip_get_clip(sc);
    int width, height;
    ED_space_clip_get_size(sc, &width, &height);
    BKE_mask_coord_to_movieclip(clip, &sc->user, min, min);
    BKE_mask_coord_to_movieclip(clip, &sc->user, max, max);
    min[0] *= width;
    min[1] *= height;
    max[0] *= width;
    max[1] *= height;
    return true;
  }
  return false;
}

bool clip_view_calculate_view_selection(
    const bContext *C, bool fit, float *r_offset_x, float *r_offset_y, float *r_zoom)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  int frame_width, frame_height;
  ED_space_clip_get_size(sc, &frame_width, &frame_height);

  if ((frame_width == 0) || (frame_height == 0) || (sc->clip == nullptr)) {
    return false;
  }

  /* NOTE: The `fit` argument is set to truth when doing "View to Selected" operator, and it set to
   * false when this function is used for Lock-to-Selection functionality. When locking to
   * selection the handles are to use control point position. So we can derive the
   * `handles_as_control_point` from `fit`.
   *
   * TODO(sergey): Make such decision more explicit. Maybe pass use-case for the calculation to
   * tell operator from lock-to-selection apart. */
  float min[2], max[2];
  if (!selected_boundbox(C, min, max, !fit)) {
    return false;
  }

  /* center view */
  clip_view_offset_for_center_to_point(sc,
                                       (max[0] + min[0]) / (2 * frame_width),
                                       (max[1] + min[1]) / (2 * frame_height),
                                       r_offset_x,
                                       r_offset_y);

  const int w = max[0] - min[0];
  const int h = max[1] - min[1];

  /* set zoom to see all selection */
  *r_zoom = sc->zoom;
  if (w > 0 && h > 0) {
    ARegion *region = CTX_wm_region(C);

    int width, height;
    float zoomx, zoomy, newzoom, aspx, aspy;

    ED_space_clip_get_aspect(sc, &aspx, &aspy);

    width = BLI_rcti_size_x(&region->winrct) + 1;
    height = BLI_rcti_size_y(&region->winrct) + 1;

    zoomx = float(width) / w / aspx;
    zoomy = float(height) / h / aspy;

    newzoom = 1.0f / power_of_2(1.0f / min_ff(zoomx, zoomy));

    if (fit) {
      *r_zoom = newzoom;
    }
  }

  return true;
}

bool clip_view_has_locked_selection(const bContext *C)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);

  if ((space_clip->flag & SC_LOCK_SELECTION) == 0) {
    return false;
  }

  if (space_clip->mode == SC_MODE_TRACKING) {
    return tracking_has_selection(space_clip);
  }

  return mask_has_selection(C);
}

void clip_draw_sfra_efra(View2D *v2d, Scene *scene)
{
  UI_view2d_view_ortho(v2d);

  /* currently clip editor supposes that editing clip length is equal to scene frame range */
  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4f(0.0f, 0.0f, 0.0f, 0.4f);
  immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, float(scene->r.sfra), v2d->cur.ymax);
  immRectf(pos, float(scene->r.efra), v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);

  GPU_blend(GPU_BLEND_NONE);

  immUniformThemeColorShade(TH_BACK, -60);

  /* thin lines where the actual frames are */
  GPU_line_width(1.0f);

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, float(scene->r.sfra), v2d->cur.ymin);
  immVertex2f(pos, float(scene->r.sfra), v2d->cur.ymax);
  immVertex2f(pos, float(scene->r.efra), v2d->cur.ymin);
  immVertex2f(pos, float(scene->r.efra), v2d->cur.ymax);
  immEnd();

  immUnbindProgram();
}
