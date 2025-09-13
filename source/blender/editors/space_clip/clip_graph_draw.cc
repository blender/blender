/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_utildefines.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "ED_anim_api.hh"
#include "ED_clip.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "clip_intern.hh" /* own include */

struct TrackMotionCurveUserData {
  SpaceClip *sc;
  MovieTrackingTrack *act_track;
  bool sel;
  float xscale, yscale, hsize;
  uint pos;
};

static void tracking_segment_point_cb(void *userdata,
                                      MovieTrackingTrack * /*track*/,
                                      MovieTrackingMarker * /*marker*/,
                                      eClipCurveValueSource value_source,
                                      int scene_framenr,
                                      float val)
{
  TrackMotionCurveUserData *data = (TrackMotionCurveUserData *)userdata;

  if (!clip_graph_value_visible(data->sc, value_source)) {
    return;
  }

  immVertex2f(data->pos, scene_framenr, val);
}

static void tracking_segment_start_cb(void *userdata,
                                      MovieTrackingTrack *track,
                                      eClipCurveValueSource value_source,
                                      bool is_point)
{
  TrackMotionCurveUserData *data = (TrackMotionCurveUserData *)userdata;
  SpaceClip *sc = data->sc;
  float col[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  if (!clip_graph_value_visible(sc, value_source)) {
    return;
  }

  switch (value_source) {
    case CLIP_VALUE_SOURCE_SPEED_X:
      UI_GetThemeColor4fv(TH_AXIS_X, col);
      break;
    case CLIP_VALUE_SOURCE_SPEED_Y:
      UI_GetThemeColor4fv(TH_AXIS_Y, col);
      break;
    case CLIP_VALUE_SOURCE_REPROJECTION_ERROR:
      UI_GetThemeColor4fv(TH_AXIS_Z, col);
      break;
  }

  if (track == data->act_track) {
    col[3] = 1.0f;
    GPU_line_width(2.0f);
  }
  else {
    col[3] = 0.5f;
    GPU_line_width(1.0f);
  }

  if (is_point) {
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
    immUniform1f("size", 3.0f);
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
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

static void tracking_segment_end_cb(void *userdata, eClipCurveValueSource value_source)
{
  TrackMotionCurveUserData *data = (TrackMotionCurveUserData *)userdata;
  SpaceClip *sc = data->sc;
  if (!clip_graph_value_visible(sc, value_source)) {
    return;
  }
  immEnd();
  immUnbindProgram();
}

static void tracking_segment_knot_cb(void *userdata,
                                     MovieTrackingTrack *track,
                                     MovieTrackingMarker *marker,
                                     eClipCurveValueSource value_source,
                                     int scene_framenr,
                                     float val)
{
  TrackMotionCurveUserData *data = (TrackMotionCurveUserData *)userdata;

  if (track != data->act_track) {
    return;
  }
  if (!ELEM(value_source, CLIP_VALUE_SOURCE_SPEED_X, CLIP_VALUE_SOURCE_SPEED_Y)) {
    return;
  }

  const int sel_flag = value_source == CLIP_VALUE_SOURCE_SPEED_X ? MARKER_GRAPH_SEL_X :
                                                                   MARKER_GRAPH_SEL_Y;
  const bool sel = (marker->flag & sel_flag) != 0;

  if (sel == data->sel) {
    GPU_line_width(1.0f);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformThemeColor(sel ? TH_HANDLE_VERTEX_SELECT : TH_HANDLE_VERTEX);

    GPU_matrix_push();
    GPU_matrix_translate_2f(scene_framenr, val);
    GPU_matrix_scale_2f(1.0f / data->xscale * data->hsize, 1.0f / data->yscale * data->hsize);

    imm_draw_circle_wire_2d(data->pos, 0, 0, 0.7, 8);

    GPU_matrix_pop();
    immUnbindProgram();
  }
}

static void draw_tracks_motion_and_error_curves(View2D *v2d, SpaceClip *sc, uint pos)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  MovieTrackingTrack *active_track = tracking_object->active_track;
  const bool draw_knots = (sc->flag & SC_SHOW_GRAPH_TRACKS_MOTION) != 0;

  int width, height;
  BKE_movieclip_get_size(clip, &sc->user, &width, &height);
  if (!width || !height) {
    return;
  }

  TrackMotionCurveUserData userdata;
  userdata.sc = sc;
  userdata.hsize = UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE);
  userdata.sel = false;
  userdata.act_track = active_track;
  userdata.pos = pos;

  /* Non-selected knot handles. */
  if (draw_knots) {
    UI_view2d_scale_get(v2d, &userdata.xscale, &userdata.yscale);
    clip_graph_tracking_values_iterate(sc,
                                       (sc->flag & SC_SHOW_GRAPH_SEL_ONLY) != 0,
                                       (sc->flag & SC_SHOW_GRAPH_HIDDEN) != 0,
                                       &userdata,
                                       tracking_segment_knot_cb,
                                       nullptr,
                                       nullptr);
  }

  /* Draw graph lines. */
  GPU_blend(GPU_BLEND_ALPHA);
  clip_graph_tracking_values_iterate(sc,
                                     (sc->flag & SC_SHOW_GRAPH_SEL_ONLY) != 0,
                                     (sc->flag & SC_SHOW_GRAPH_HIDDEN) != 0,
                                     &userdata,
                                     tracking_segment_point_cb,
                                     tracking_segment_start_cb,
                                     tracking_segment_end_cb);
  GPU_blend(GPU_BLEND_NONE);

  /* Selected knot handles on top of curves. */
  if (draw_knots) {
    userdata.sel = true;
    clip_graph_tracking_values_iterate(sc,
                                       (sc->flag & SC_SHOW_GRAPH_SEL_ONLY) != 0,
                                       (sc->flag & SC_SHOW_GRAPH_HIDDEN) != 0,
                                       &userdata,
                                       tracking_segment_knot_cb,
                                       nullptr,
                                       nullptr);
  }
}

static void draw_frame_curves(SpaceClip *sc, uint pos)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const MovieTrackingReconstruction *reconstruction = &tracking_object->reconstruction;

  int previous_frame;
  float previous_error;
  bool have_previous_point = false;

  /* Indicates whether immBegin() was called. */
  bool is_lines_segment_open = false;

  GPU_line_width(1.0f);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4f(0.0f, 0.0f, 1.0f, 1.0f);

  for (int i = 0; i < reconstruction->camnr; i++) {
    MovieReconstructedCamera *camera = &reconstruction->cameras[i];

    const int current_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, camera->framenr);
    const float current_error = camera->error;

    if (have_previous_point && current_frame != previous_frame + 1) {
      if (is_lines_segment_open) {
        immEnd();
        is_lines_segment_open = false;
      }
      have_previous_point = false;
    }

    if (have_previous_point) {
      if (!is_lines_segment_open) {
        immBeginAtMost(GPU_PRIM_LINE_STRIP, reconstruction->camnr);
        is_lines_segment_open = true;

        immVertex2f(pos, previous_frame, previous_error);
      }
      immVertex2f(pos, current_frame, current_error);
    }

    previous_frame = current_frame;
    previous_error = current_error;
    have_previous_point = true;
  }

  if (is_lines_segment_open) {
    immEnd();
  }
  immUnbindProgram();
}

void clip_draw_graph(SpaceClip *sc, ARegion *region, Scene *scene)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  View2D *v2d = &region->v2d;

  /* grid */
  UI_view2d_draw_lines_x__values(v2d, 10);
  UI_view2d_draw_lines_y__values(v2d, 10);

  if (clip) {
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    if (sc->flag & (SC_SHOW_GRAPH_TRACKS_MOTION | SC_SHOW_GRAPH_TRACKS_ERROR)) {
      draw_tracks_motion_and_error_curves(v2d, sc, pos);
    }

    if (sc->flag & SC_SHOW_GRAPH_FRAMES) {
      draw_frame_curves(sc, pos);
    }
  }

  /* Frame and preview range. */
  UI_view2d_view_ortho(v2d);
  ANIM_draw_framerange(scene, v2d);
  ANIM_draw_previewrange(scene, v2d, 0);
}
