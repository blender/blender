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

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "clip_intern.h"  // own include

typedef struct TrackMotionCurveUserData {
  MovieTrackingTrack *act_track;
  bool sel;
  float xscale, yscale, hsize;
  unsigned int pos;
} TrackMotionCurveUserData;

static void tracking_segment_point_cb(void *userdata,
                                      MovieTrackingTrack *UNUSED(track),
                                      MovieTrackingMarker *UNUSED(marker),
                                      eClipCurveValueSource UNUSED(value_source),
                                      int scene_framenr,
                                      float val)
{
  TrackMotionCurveUserData *data = (TrackMotionCurveUserData *)userdata;

  immVertex2f(data->pos, scene_framenr, val);
}

static void tracking_segment_start_cb(void *userdata,
                                      MovieTrackingTrack *track,
                                      eClipCurveValueSource value_source,
                                      bool is_point)
{
  TrackMotionCurveUserData *data = (TrackMotionCurveUserData *)userdata;
  float col[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  switch (value_source) {
    case CLIP_VALUE_SOURCE_SPEED_X:
      col[0] = 1.0f;
      break;
    case CLIP_VALUE_SOURCE_SPEED_Y:
      col[1] = 1.0f;
      break;
    default:
      return;
  }

  if (track == data->act_track) {
    col[3] = 1.0f;
    GPU_line_width(2.0f);
  }
  else {
    col[3] = 0.5f;
    GPU_line_width(1.0f);
  }

  immUniformColor4fv(col);

  if (is_point) {
    immBeginAtMost(GPU_PRIM_POINTS, 1);
  }
  else {
    /* Graph can be composed of smaller segments, if any marker is disabled */
    immBeginAtMost(GPU_PRIM_LINE_STRIP, track->markersnr);
  }
}

static void tracking_segment_end_cb(void *UNUSED(userdata),
                                    eClipCurveValueSource UNUSED(value_source))
{
  immEnd();
}

static void tracking_segment_knot_cb(void *userdata,
                                     MovieTrackingTrack *track,
                                     MovieTrackingMarker *marker,
                                     eClipCurveValueSource value_source,
                                     int scene_framenr,
                                     float val)
{
  TrackMotionCurveUserData *data = (TrackMotionCurveUserData *)userdata;
  int sel = 0, sel_flag;

  if (track != data->act_track) {
    return;
  }
  if (!ELEM(value_source, CLIP_VALUE_SOURCE_SPEED_X, CLIP_VALUE_SOURCE_SPEED_Y)) {
    return;
  }

  sel_flag = value_source == CLIP_VALUE_SOURCE_SPEED_X ? MARKER_GRAPH_SEL_X : MARKER_GRAPH_SEL_Y;
  sel = (marker->flag & sel_flag) ? 1 : 0;

  if (sel == data->sel) {
    immUniformThemeColor(sel ? TH_HANDLE_VERTEX_SELECT : TH_HANDLE_VERTEX);

    GPU_matrix_push();
    GPU_matrix_translate_2f(scene_framenr, val);
    GPU_matrix_scale_2f(1.0f / data->xscale * data->hsize, 1.0f / data->yscale * data->hsize);

    imm_draw_circle_wire_2d(data->pos, 0, 0, 0.7, 8);

    GPU_matrix_pop();
  }
}

static void draw_tracks_motion_curves(View2D *v2d, SpaceClip *sc, unsigned int pos)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
  int width, height;
  TrackMotionCurveUserData userdata;

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  if (!width || !height) {
    return;
  }

  /* non-selected knot handles */
  userdata.hsize = UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE);
  userdata.sel = false;
  userdata.act_track = act_track;
  userdata.pos = pos;
  UI_view2d_scale_get(v2d, &userdata.xscale, &userdata.yscale);
  clip_graph_tracking_values_iterate(sc,
                                     (sc->flag & SC_SHOW_GRAPH_SEL_ONLY) != 0,
                                     (sc->flag & SC_SHOW_GRAPH_HIDDEN) != 0,
                                     &userdata,
                                     tracking_segment_knot_cb,
                                     NULL,
                                     NULL);
  /* draw graph lines */
  GPU_blend(true);
  clip_graph_tracking_values_iterate(sc,
                                     (sc->flag & SC_SHOW_GRAPH_SEL_ONLY) != 0,
                                     (sc->flag & SC_SHOW_GRAPH_HIDDEN) != 0,
                                     &userdata,
                                     tracking_segment_point_cb,
                                     tracking_segment_start_cb,
                                     tracking_segment_end_cb);
  GPU_blend(false);

  /* selected knot handles on top of curves */
  userdata.sel = true;
  clip_graph_tracking_values_iterate(sc,
                                     (sc->flag & SC_SHOW_GRAPH_SEL_ONLY) != 0,
                                     (sc->flag & SC_SHOW_GRAPH_HIDDEN) != 0,
                                     &userdata,
                                     tracking_segment_knot_cb,
                                     NULL,
                                     NULL);
}

typedef struct TrackErrorCurveUserData {
  MovieClip *clip;
  MovieTracking *tracking;
  MovieTrackingObject *tracking_object;
  MovieTrackingTrack *active_track;
  bool matrix_initialized;
  int matrix_frame;
  float projection_matrix[4][4];
  int width, height;
  float aspy;
  unsigned int pos;
} TrackErrorCurveUserData;

static void tracking_error_segment_point_cb(void *userdata,
                                            MovieTrackingTrack *track,
                                            MovieTrackingMarker *marker,
                                            eClipCurveValueSource value_source,
                                            int scene_framenr,
                                            float UNUSED(value))
{
  if (value_source == CLIP_VALUE_SOURCE_SPEED_Y) {
    TrackErrorCurveUserData *data = (TrackErrorCurveUserData *)userdata;
    float reprojected_position[4], bundle_position[4], marker_position[2], delta[2];
    float reprojection_error;
    float weight = BKE_tracking_track_get_weight_for_marker(data->clip, track, marker);

    if (!data->matrix_initialized || data->matrix_frame != scene_framenr) {
      BKE_tracking_get_projection_matrix(data->tracking,
                                         data->tracking_object,
                                         scene_framenr,
                                         data->width,
                                         data->height,
                                         data->projection_matrix);
    }

    copy_v3_v3(bundle_position, track->bundle_pos);
    bundle_position[3] = 1;

    mul_v4_m4v4(reprojected_position, data->projection_matrix, bundle_position);
    reprojected_position[0] = (reprojected_position[0] / (reprojected_position[3] * 2.0f) + 0.5f) *
                              data->width;
    reprojected_position[1] = (reprojected_position[1] / (reprojected_position[3] * 2.0f) + 0.5f) *
                              data->height * data->aspy;

    BKE_tracking_distort_v2(data->tracking, reprojected_position, reprojected_position);

    marker_position[0] = (marker->pos[0] + track->offset[0]) * data->width;
    marker_position[1] = (marker->pos[1] + track->offset[1]) * data->height * data->aspy;

    sub_v2_v2v2(delta, reprojected_position, marker_position);
    reprojection_error = len_v2(delta) * weight;

    immVertex2f(data->pos, scene_framenr, reprojection_error);
  }
}

static void tracking_error_segment_start_cb(void *userdata,
                                            MovieTrackingTrack *track,
                                            eClipCurveValueSource value_source,
                                            bool is_point)
{
  if (value_source == CLIP_VALUE_SOURCE_SPEED_Y) {
    TrackErrorCurveUserData *data = (TrackErrorCurveUserData *)userdata;
    float col[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    if (track == data->active_track) {
      col[3] = 1.0f;
      GPU_line_width(2.0f);
    }
    else {
      col[3] = 0.5f;
      GPU_line_width(1.0f);
    }

    immUniformColor4fv(col);

    if (is_point) { /* This probably never happens here, but just in case... */
      immBeginAtMost(GPU_PRIM_POINTS, 1);
    }
    else {
      /* Graph can be composed of smaller segments, if any marker is disabled */
      immBeginAtMost(GPU_PRIM_LINE_STRIP, track->markersnr);
    }
  }
}

static void tracking_error_segment_end_cb(void *UNUSED(userdata),
                                          eClipCurveValueSource value_source)
{
  if (value_source == CLIP_VALUE_SOURCE_SPEED_Y) {
    immEnd();
  }
}

static void draw_tracks_error_curves(SpaceClip *sc, unsigned int pos)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  TrackErrorCurveUserData data;

  data.clip = clip;
  data.tracking = tracking;
  data.tracking_object = BKE_tracking_object_get_active(tracking);
  data.active_track = BKE_tracking_track_get_active(tracking);
  data.matrix_initialized = false;
  data.pos = pos;
  BKE_movieclip_get_size(clip, &sc->user, &data.width, &data.height);
  data.aspy = 1.0f / tracking->camera.pixel_aspect;

  if (!data.width || !data.height) {
    return;
  }

  clip_graph_tracking_values_iterate(sc,
                                     (sc->flag & SC_SHOW_GRAPH_SEL_ONLY) != 0,
                                     (sc->flag & SC_SHOW_GRAPH_HIDDEN) != 0,
                                     &data,
                                     tracking_error_segment_point_cb,
                                     tracking_error_segment_start_cb,
                                     tracking_error_segment_end_cb);
}

static void draw_frame_curves(SpaceClip *sc, unsigned int pos)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(tracking);
  int i, lines = 0, prevfra = 0;

  immUniformColor3f(0.0f, 0.0f, 1.0f);

  for (i = 0; i < reconstruction->camnr; i++) {
    MovieReconstructedCamera *camera = &reconstruction->cameras[i];
    int framenr;

    if (lines && camera->framenr != prevfra + 1) {
      immEnd();
      lines = 0;
    }

    if (!lines) {
      immBeginAtMost(GPU_PRIM_LINE_STRIP, reconstruction->camnr);
      lines = 1;
    }

    framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, camera->framenr);
    immVertex2f(pos, framenr, camera->error);

    prevfra = camera->framenr;
  }

  if (lines) {
    immEnd();
  }
}

void clip_draw_graph(SpaceClip *sc, ARegion *ar, Scene *scene)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  View2D *v2d = &ar->v2d;

  /* grid */
  UI_view2d_draw_lines_x__values(v2d);
  UI_view2d_draw_lines_y__values(v2d);

  if (clip) {
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    GPU_point_size(3.0f);

    if (sc->flag & SC_SHOW_GRAPH_TRACKS_MOTION) {
      draw_tracks_motion_curves(v2d, sc, pos);
    }

    if (sc->flag & SC_SHOW_GRAPH_TRACKS_ERROR) {
      draw_tracks_error_curves(sc, pos);
    }

    if (sc->flag & SC_SHOW_GRAPH_FRAMES) {
      draw_frame_curves(sc, pos);
    }

    immUnbindProgram();
  }

  /* frame range */
  clip_draw_sfra_efra(v2d, scene);
}
