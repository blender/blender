/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "DNA_gpencil_legacy_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_geom.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "ED_clip.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_mask.hh"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "BIF_glutil.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "WM_types.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLF_api.hh"

#include "clip_intern.hh" /* own include */

/*********************** main area drawing *************************/

static void draw_keyframe(int frame, int cfra, int sfra, float framelen, int width, uint pos)
{
  int height = (frame == cfra) ? 22 : 10;
  int x = (frame - sfra) * framelen;

  if (width == 1) {
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, x, 0);
    immVertex2f(pos, x, height * UI_SCALE_FAC);
    immEnd();
  }
  else {
    immRectf(pos, x, 0, x + width, height * UI_SCALE_FAC);
  }
}

static int generic_track_get_markersnr(const MovieTrackingTrack *track,
                                       const MovieTrackingPlaneTrack *plane_track)
{
  if (track) {
    return track->markersnr;
  }
  if (plane_track) {
    return plane_track->markersnr;
  }

  return 0;
}

static int generic_track_get_marker_framenr(const MovieTrackingTrack *track,
                                            const MovieTrackingPlaneTrack *plane_track,
                                            const int marker_index)
{
  if (track) {
    BLI_assert(marker_index < track->markersnr);
    return track->markers[marker_index].framenr;
  }
  if (plane_track) {
    BLI_assert(marker_index < plane_track->markersnr);
    return plane_track->markers[marker_index].framenr;
  }

  return 0;
}

static bool generic_track_is_marker_enabled(const MovieTrackingTrack *track,
                                            const MovieTrackingPlaneTrack *plane_track,
                                            const int marker_index)
{
  if (track) {
    BLI_assert(marker_index < track->markersnr);
    return (track->markers[marker_index].flag & MARKER_DISABLED) == 0;
  }
  if (plane_track) {
    return true;
  }

  return false;
}

static bool generic_track_is_marker_keyframed(const MovieTrackingTrack *track,
                                              const MovieTrackingPlaneTrack *plane_track,
                                              const int marker_index)
{
  if (track) {
    BLI_assert(marker_index < track->markersnr);
    return (track->markers[marker_index].flag & MARKER_TRACKED) == 0;
  }
  if (plane_track) {
    BLI_assert(marker_index < plane_track->markersnr);
    return (plane_track->markers[marker_index].flag & PLANE_MARKER_TRACKED) == 0;
  }

  return false;
}

static void draw_movieclip_cache(SpaceClip *sc, ARegion *region, MovieClip *clip, Scene *scene)
{
  float x;
  int *points, totseg, i, a;
  float sfra = scene->r.sfra, efra = scene->r.efra, framelen = region->winx / (efra - sfra + 1);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  const MovieTrackingReconstruction *reconstruction = &tracking_object->reconstruction;

  GPU_blend(GPU_BLEND_ALPHA);

  /* cache background */
  ED_region_cache_draw_background(region);

  /* cached segments -- could be useful to debug caching strategies */
  BKE_movieclip_get_cache_segments(clip, &sc->user, &totseg, &points);
  ED_region_cache_draw_cached_segments(region, totseg, points, sfra, efra);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* track */
  if (tracking_object->active_track || tracking_object->active_plane_track) {
    const MovieTrackingTrack *active_track = tracking_object->active_track;
    const MovieTrackingPlaneTrack *active_plane_track = tracking_object->active_plane_track;

    for (i = sfra - clip->start_frame + 1, a = 0; i <= efra - clip->start_frame + 1; i++) {
      int framenr;
      const int markersnr = generic_track_get_markersnr(active_track, active_plane_track);

      while (a < markersnr) {
        int marker_framenr = generic_track_get_marker_framenr(active_track, active_plane_track, a);

        if (marker_framenr >= i) {
          break;
        }

        if (a < markersnr - 1 &&
            generic_track_get_marker_framenr(active_track, active_plane_track, a + 1) > i)
        {
          break;
        }

        a++;
      }

      a = min_ii(a, markersnr - 1);

      if (generic_track_is_marker_enabled(active_track, active_plane_track, a)) {
        framenr = generic_track_get_marker_framenr(active_track, active_plane_track, a);

        if (framenr != i) {
          immUniformColor4ub(128, 128, 0, 96);
        }
        else if (generic_track_is_marker_keyframed(active_track, active_plane_track, a)) {
          immUniformColor4ub(255, 255, 0, 196);
        }
        else {
          immUniformColor4ub(255, 255, 0, 96);
        }

        immRectf(pos,
                 (i - sfra + clip->start_frame - 1) * framelen,
                 0,
                 (i - sfra + clip->start_frame) * framelen,
                 4 * UI_SCALE_FAC);
      }
    }
  }

  /* failed frames */
  if (reconstruction->flag & TRACKING_RECONSTRUCTED) {
    int n = reconstruction->camnr;
    MovieReconstructedCamera *cameras = reconstruction->cameras;

    immUniformColor4ub(255, 0, 0, 96);

    for (i = sfra, a = 0; i <= efra; i++) {
      bool ok = false;

      while (a < n) {
        if (cameras[a].framenr == i) {
          ok = true;
          break;
        }
        if (cameras[a].framenr > i) {
          break;
        }

        a++;
      }

      if (!ok) {
        immRectf(pos,
                 (i - sfra + clip->start_frame - 1) * framelen,
                 0,
                 (i - sfra + clip->start_frame) * framelen,
                 8 * UI_SCALE_FAC);
      }
    }
  }

  GPU_blend(GPU_BLEND_NONE);

  /* current frame */
  x = (sc->user.framenr - sfra) / (efra - sfra + 1) * region->winx;

  immUniformThemeColor(TH_CFRAME);
  immRectf(pos, x, 0, x + ceilf(framelen), 8 * UI_SCALE_FAC);

  immUnbindProgram();

  ED_region_cache_draw_curfra_label(
      sc->user.framenr, x + roundf(framelen / 2), 8.0f * UI_SCALE_FAC);

  pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* solver keyframes */
  immUniformColor4ub(175, 255, 0, 255);
  draw_keyframe(
      tracking_object->keyframe1 + clip->start_frame - 1, scene->r.cfra, sfra, framelen, 2, pos);
  draw_keyframe(
      tracking_object->keyframe2 + clip->start_frame - 1, scene->r.cfra, sfra, framelen, 2, pos);

  immUnbindProgram();

  /* movie clip animation */
  if ((sc->mode == SC_MODE_MASKEDIT) && sc->mask_info.mask) {
    ED_mask_draw_frames(sc->mask_info.mask, region, scene->r.cfra, sfra, efra);
  }
}

static void draw_movieclip_notes(SpaceClip *sc, ARegion *region)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  char str[256] = {0};
  bool full_redraw = false;

  if (tracking->stats) {
    STRNCPY_UTF8(str, tracking->stats->message);
    full_redraw = true;
  }
  else {
    if (sc->flag & SC_LOCK_SELECTION) {
      STRNCPY_UTF8(str, "Locked");
    }
  }

  if (str[0]) {
    float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.6f};
    ED_region_info_draw(region, str, fill_color, full_redraw);
  }
}

static void draw_movieclip_muted(ARegion *region, int width, int height, float zoomx, float zoomy)
{
  int x, y;

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* find window pixel coordinates of origin */
  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &x, &y);

  immUniformColor3f(0.0f, 0.0f, 0.0f);
  immRectf(pos, x, y, x + zoomx * width, y + zoomy * height);

  immUnbindProgram();
}

static void draw_movieclip_buffer(const bContext *C,
                                  SpaceClip *sc,
                                  ARegion *region,
                                  ImBuf *ibuf,
                                  int width,
                                  int height,
                                  float zoomx,
                                  float zoomy)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  bool use_filter = true;
  int x, y;

  /* find window pixel coordinates of origin */
  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &x, &y);

  /* checkerboard for case alpha */
  if (ibuf->planes == 32) {
    GPU_blend(GPU_BLEND_ALPHA);

    imm_draw_box_checker_2d(x, y, x + zoomx * ibuf->x, y + zoomy * ibuf->y);
  }

  /* non-scaled proxy shouldn't use filtering */
  if ((clip->flag & MCLIP_USE_PROXY) == 0 ||
      ELEM(sc->user.render_size, MCLIP_PROXY_RENDER_SIZE_FULL, MCLIP_PROXY_RENDER_SIZE_100))
  {
    use_filter = false;
  }

  ED_draw_imbuf_ctx(C, ibuf, x, y, use_filter, zoomx * width / ibuf->x, zoomy * height / ibuf->y);

  if (ibuf->planes == 32) {
    GPU_blend(GPU_BLEND_NONE);
  }

  if (sc->flag & SC_SHOW_METADATA) {
    rctf frame;
    BLI_rctf_init(&frame, 0.0f, ibuf->x, 0.0f, ibuf->y);
    ED_region_image_metadata_draw(
        x, y, ibuf, &frame, zoomx * width / ibuf->x, zoomy * height / ibuf->y);
  }
}

static void draw_stabilization_border(
    SpaceClip *sc, ARegion *region, int width, int height, float zoomx, float zoomy)
{
  int x, y;
  MovieClip *clip = ED_space_clip_get_clip(sc);

  /* find window pixel coordinates of origin */
  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &x, &y);

  /* draw boundary border for frame if stabilization is enabled */
  if (sc->flag & SC_SHOW_STABLE && clip->tracking.stabilization.flag & TRACKING_2D_STABILIZATION) {
    const uint shdr_pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    /* Exclusive OR allows to get orig value when second operand is 0,
     * and negative of orig value when second operand is 1. */
    GPU_logic_op_xor_set(true);

    GPU_matrix_push();
    GPU_matrix_translate_2f(x, y);

    GPU_matrix_scale_2f(zoomx, zoomy);
    GPU_matrix_mul(sc->stabmat);

    immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f(
        "viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniformColor4f(1.0f, 1.0f, 1.0f, 0.0f);
    immUniform1f("dash_width", 6.0f);
    immUniform1f("udash_factor", 0.5f);

    imm_draw_box_wire_2d(shdr_pos, 0.0f, 0.0f, width, height);

    immUnbindProgram();

    GPU_matrix_pop();

    GPU_logic_op_xor_set(false);
  }
}

enum {
  PATH_POINT_FLAG_KEYFRAME = (1 << 0),
};

struct TrackPathPoint {
  float co[2];
  uchar flag;
};

static void marker_to_path_point(SpaceClip *sc,
                                 const MovieTrackingTrack *track,
                                 const MovieTrackingMarker *marker,
                                 TrackPathPoint *point)
{
  add_v2_v2v2(point->co, marker->pos, track->offset);
  ED_clip_point_undistorted_pos(sc, point->co, point->co);
  point->flag = 0;
  if ((marker->flag & MARKER_TRACKED) == 0) {
    point->flag |= PATH_POINT_FLAG_KEYFRAME;
  }
}

static int track_to_path_segment(SpaceClip *sc,
                                 MovieTrackingTrack *track,
                                 int direction,
                                 TrackPathPoint *path)
{
  const int count = sc->path_length;
  int current_frame = ED_space_clip_get_clip_frame_number(sc);
  const MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, current_frame);
  /* Check whether there is marker at exact current frame.
   * If not, we don't have anything to be put to path. */
  if (marker == nullptr || (marker->flag & MARKER_DISABLED)) {
    return 0;
  }
  /* Index inside of path array where we write data to. */
  int point_index = count;
  int path_length = 0;
  for (int i = 0; i < count; ++i) {
    marker_to_path_point(sc, track, marker, &path[point_index]);
    /* Move to the next marker along the path segment. */
    path_length++;
    point_index += direction;
    current_frame += direction;
    marker = BKE_tracking_marker_get_exact(track, current_frame);
    if (marker == nullptr || (marker->flag & MARKER_DISABLED)) {
      /* Reached end of tracked segment. */
      break;
    }
  }
  return path_length;
}

static void draw_track_path_points(const TrackPathPoint *path,
                                   uint position_attribute,
                                   const int start_point,
                                   const int num_points)
{
  if (num_points == 0) {
    return;
  }
  immBegin(GPU_PRIM_POINTS, num_points);
  for (int i = 0; i < num_points; i++) {
    const TrackPathPoint *point = &path[i + start_point];
    immVertex2fv(position_attribute, point->co);
  }
  immEnd();
}

static void draw_track_path_keyframe_points(const TrackPathPoint *path,
                                            uint position_attribute,
                                            const int start_point,
                                            const int num_points)
{
  immBeginAtMost(GPU_PRIM_POINTS, num_points);
  for (int i = 0; i < num_points; i++) {
    const TrackPathPoint *point = &path[i + start_point];
    if (point->flag & PATH_POINT_FLAG_KEYFRAME) {
      immVertex2fv(position_attribute, point->co);
    }
  }
  immEnd();
}

static void draw_track_path_lines(const TrackPathPoint *path,
                                  uint position_attribute,
                                  const int start_point,
                                  const int num_points)
{
  if (num_points < 2) {
    return;
  }
  immBegin(GPU_PRIM_LINE_STRIP, num_points);
  for (int i = 0; i < num_points; i++) {
    const TrackPathPoint *point = &path[i + start_point];
    immVertex2fv(position_attribute, point->co);
  }
  immEnd();
}

static void draw_track_path(SpaceClip *sc, MovieClip * /*clip*/, MovieTrackingTrack *track)
{
#define MAX_STATIC_PATH 64

  const int count = sc->path_length;
  TrackPathPoint path_static[(MAX_STATIC_PATH + 1) * 2];
  TrackPathPoint *path;
  const bool tiny = (sc->flag & SC_SHOW_TINY_MARKER) != 0;

  if (count == 0) {
    /* Early output, nothing to bother about here. */
    return;
  }

  /* Try to use stack allocated memory when possibly, only use heap allocation
   * for really long paths. */
  path = (count < MAX_STATIC_PATH) ?
             path_static :
             MEM_calloc_arrayN<TrackPathPoint>(sizeof(*path) * (count + 1) * 2, "path");
  /* Collect path information. */
  const int num_points_before = track_to_path_segment(sc, track, -1, path);
  const int num_points_after = track_to_path_segment(sc, track, 1, path);
  if (num_points_before == 0 && num_points_after == 0) {
    return;
  }

  int num_all_points = num_points_before + num_points_after;
  /* If both leading and trailing parts of the path are there the center point is counted twice. */
  if (num_points_before != 0 && num_points_after != 0) {
    num_all_points -= 1;
  }

  const int path_start_index = count - num_points_before + 1;
  const int path_center_index = count;

  const uint position_attribute = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  /* Draw path outline. */
  if (!tiny) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
      immUniformThemeColor(TH_MARKER_OUTLINE);
      immUniform1f("size", 5.0f);
      draw_track_path_points(path, position_attribute, path_start_index, num_all_points);
      immUniform1f("size", 7.0f);
      draw_track_path_keyframe_points(path, position_attribute, path_start_index, num_all_points);
      immUnbindProgram();
    }
    /* Draw darker outline for actual path, all line segments at once. */
    GPU_line_width(3.0f);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformThemeColor(TH_MARKER_OUTLINE);
    draw_track_path_lines(path, position_attribute, path_start_index, num_all_points);
    immUnbindProgram();
  }

  /* Draw all points. */
  immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
  immUniformThemeColor(TH_PATH_BEFORE);
  immUniform1f("size", 3.0f);
  draw_track_path_points(path, position_attribute, path_start_index, num_points_before);
  immUniformThemeColor(TH_PATH_AFTER);
  draw_track_path_points(path, position_attribute, path_center_index, num_points_after);
  immUnbindProgram();

  /* Connect points with color coded segments. */
  GPU_line_width(1.0f);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor(TH_PATH_BEFORE);
  draw_track_path_lines(path, position_attribute, path_start_index, num_points_before);
  immUniformThemeColor(TH_PATH_AFTER);
  draw_track_path_lines(path, position_attribute, path_center_index, num_points_after);
  immUnbindProgram();

  /* Draw all bigger points corresponding to keyframes. */
  immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
  immUniformThemeColor(TH_PATH_KEYFRAME_BEFORE);
  immUniform1f("size", 5.0f);
  draw_track_path_keyframe_points(path, position_attribute, path_start_index, num_points_before);
  immUniformThemeColor(TH_PATH_KEYFRAME_AFTER);
  draw_track_path_keyframe_points(path, position_attribute, path_center_index, num_points_after);
  immUnbindProgram();

  if (path != path_static) {
    MEM_freeN(path);
  }

#undef MAX_STATIC_PATH
}

static void draw_marker_outline(SpaceClip *sc,
                                const MovieTrackingTrack *track,
                                const MovieTrackingMarker *marker,
                                const float marker_pos[2],
                                int width,
                                int height,
                                uint position)
{
  int tiny = sc->flag & SC_SHOW_TINY_MARKER;
  bool show_search = false;
  float px[2];

  px[0] = 1.0f / width / sc->zoom;
  px[1] = 1.0f / height / sc->zoom;

  if ((marker->flag & MARKER_DISABLED) == 0) {
    float pos[2];
    float p[2];

    add_v2_v2v2(pos, marker->pos, track->offset);

    ED_clip_point_undistorted_pos(sc, pos, pos);

    sub_v2_v2v2(p, pos, marker_pos);

    if (isect_point_quad_v2(p,
                            marker->pattern_corners[0],
                            marker->pattern_corners[1],
                            marker->pattern_corners[2],
                            marker->pattern_corners[3]))
    {
      immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
      immUniform1f("size", tiny ? 3.0f : 4.0f);
      immUniformThemeColor(TH_MARKER_OUTLINE);
      immBegin(GPU_PRIM_POINTS, 1);
      immVertex2f(position, pos[0], pos[1]);
      immEnd();
      immUnbindProgram();
    }
    else {
      GPU_line_width(tiny ? 1.0f : 3.0f);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformThemeColor(TH_MARKER_OUTLINE);
      immBegin(GPU_PRIM_LINES, 8);

      immVertex2f(position, pos[0] + px[0] * 2, pos[1]);
      immVertex2f(position, pos[0] + px[0] * 8, pos[1]);

      immVertex2f(position, pos[0] - px[0] * 2, pos[1]);
      immVertex2f(position, pos[0] - px[0] * 8, pos[1]);

      immVertex2f(position, pos[0], pos[1] - px[1] * 2);
      immVertex2f(position, pos[0], pos[1] - px[1] * 8);

      immVertex2f(position, pos[0], pos[1] + px[1] * 2);
      immVertex2f(position, pos[0], pos[1] + px[1] * 8);

      immEnd();
      immUnbindProgram();
    }
  }

  /* pattern and search outline */
  GPU_line_width(tiny ? 1.0f : 3.0f);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor(TH_MARKER_OUTLINE);

  GPU_matrix_push();
  GPU_matrix_translate_2fv(marker_pos);

  if (sc->flag & SC_SHOW_MARKER_PATTERN) {
    immBegin(GPU_PRIM_LINE_LOOP, 4);
    immVertex2fv(position, marker->pattern_corners[0]);
    immVertex2fv(position, marker->pattern_corners[1]);
    immVertex2fv(position, marker->pattern_corners[2]);
    immVertex2fv(position, marker->pattern_corners[3]);
    immEnd();
  }

  show_search = (TRACK_VIEW_SELECTED(sc, track) && ((marker->flag & MARKER_DISABLED) == 0 ||
                                                    (sc->flag & SC_SHOW_MARKER_PATTERN) == 0)) !=
                0;

  if (sc->flag & SC_SHOW_MARKER_SEARCH && show_search) {
    imm_draw_box_wire_2d(position,
                         marker->search_min[0],
                         marker->search_min[1],
                         marker->search_max[0],
                         marker->search_max[1]);
  }

  GPU_matrix_pop();
  immUnbindProgram();
}

static void track_colors(const MovieTrackingTrack *track, int act, float r_col[3], float r_scol[3])
{
  if (track->flag & TRACK_CUSTOMCOLOR) {
    if (act) {
      UI_GetThemeColor3fv(TH_ACT_MARKER, r_scol);
    }
    else {
      copy_v3_v3(r_scol, track->color);
    }

    mul_v3_v3fl(r_col, track->color, 0.5f);
  }
  else {
    UI_GetThemeColor3fv(TH_MARKER, r_col);

    if (act) {
      UI_GetThemeColor3fv(TH_ACT_MARKER, r_scol);
    }
    else {
      UI_GetThemeColor3fv(TH_SEL_MARKER, r_scol);
    }
  }
}

static void set_draw_marker_area_color(const MovieTrackingTrack *track,
                                       const MovieTrackingMarker *marker,
                                       const bool is_track_active,
                                       const bool is_area_selected,
                                       const float color[3],
                                       const float selected_color[3])
{
  if (track->flag & TRACK_LOCKED) {
    if (is_track_active) {
      immUniformThemeColor(TH_ACT_MARKER);
    }
    else if (is_area_selected) {
      immUniformThemeColorShade(TH_LOCK_MARKER, 64);
    }
    else {
      immUniformThemeColor(TH_LOCK_MARKER);
    }
  }
  else if (marker->flag & MARKER_DISABLED) {
    if (is_track_active) {
      immUniformThemeColor(TH_ACT_MARKER);
    }
    else if (is_area_selected) {
      immUniformThemeColorShade(TH_DIS_MARKER, 128);
    }
    else {
      immUniformThemeColor(TH_DIS_MARKER);
    }
  }
  else {
    immUniformColor3fv(is_area_selected ? selected_color : color);
  }
}

static void draw_marker_areas(SpaceClip *sc,
                              const MovieTrackingTrack *track,
                              const MovieTrackingMarker *marker,
                              const float marker_pos[2],
                              int width,
                              int height,
                              int act,
                              int sel,
                              const uint shdr_pos)
{
  int tiny = sc->flag & SC_SHOW_TINY_MARKER;
  bool show_search = false;
  float col[3], scol[3];
  blender::float4 color;
  float px[2];

  track_colors(track, act, col, scol);

  px[0] = 1.0f / width / sc->zoom;
  px[1] = 1.0f / height / sc->zoom;

  float viewport[4];
  GPU_viewport_size_get_f(viewport);

  /* marker position and offset position */
  if ((track->flag & SELECT) == sel && (marker->flag & MARKER_DISABLED) == 0) {
    float pos[2], p[2];

    if (track->flag & TRACK_LOCKED) {
      if (act) {
        UI_GetThemeColor4fv(TH_ACT_MARKER, color);
      }
      else if (track->flag & SELECT) {
        UI_GetThemeColorShade4fv(TH_LOCK_MARKER, 64, color);
      }
      else {
        UI_GetThemeColor4fv(TH_LOCK_MARKER, color);
      }
    }
    else {
      if (bool(track->flag & SELECT)) {
        color = blender::float4(blender::float3(scol), 1.0);
      }
      else {
        color = blender::float4(blender::float3(col), 1.0);
      }
    }

    add_v2_v2v2(pos, marker->pos, track->offset);
    ED_clip_point_undistorted_pos(sc, pos, pos);

    sub_v2_v2v2(p, pos, marker_pos);

    if (isect_point_quad_v2(p,
                            marker->pattern_corners[0],
                            marker->pattern_corners[1],
                            marker->pattern_corners[2],
                            marker->pattern_corners[3]))
    {
      immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
      immUniformColor4fv(color);
      immUniform1f("size", tiny ? 1.0f : 2.0f);
      immBegin(GPU_PRIM_POINTS, 1);
      immVertex2f(shdr_pos, pos[0], pos[1]);
      immEnd();
      immUnbindProgram();
    }
    else {
      immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
      immUniform2f("viewport_size", viewport[2] / UI_SCALE_FAC, viewport[3] / UI_SCALE_FAC);
      immUniform1i("colors_len", 0);
      immUniformColor4fv(color);
      immUniform1f("udash_factor", 2.0f); /* Solid line */

      immBegin(GPU_PRIM_LINES, 8);

      immVertex2f(shdr_pos, pos[0] + px[0] * 3, pos[1]);
      immVertex2f(shdr_pos, pos[0] + px[0] * 7, pos[1]);

      immVertex2f(shdr_pos, pos[0] - px[0] * 3, pos[1]);
      immVertex2f(shdr_pos, pos[0] - px[0] * 7, pos[1]);

      immVertex2f(shdr_pos, pos[0], pos[1] - px[1] * 3);
      immVertex2f(shdr_pos, pos[0], pos[1] - px[1] * 7);

      immVertex2f(shdr_pos, pos[0], pos[1] + px[1] * 3);
      immVertex2f(shdr_pos, pos[0], pos[1] + px[1] * 7);

      immEnd();

      immUniformColor4f(1.0f, 1.0f, 1.0f, 0.0f);
      immUniform1f("dash_width", 6.0f);
      immUniform1f("udash_factor", 0.5f);

      GPU_logic_op_xor_set(true);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2fv(shdr_pos, pos);
      immVertex2fv(shdr_pos, marker_pos);
      immEnd();

      GPU_logic_op_xor_set(false);
      immUnbindProgram();
    }
  }

  /* pattern */
  GPU_matrix_push();
  GPU_matrix_translate_2fv(marker_pos);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
  immUniform2f("viewport_size", viewport[2] / UI_SCALE_FAC, viewport[3] / UI_SCALE_FAC);
  immUniform1i("colors_len", 0);
  set_draw_marker_area_color(track, marker, act, track->pat_flag & SELECT, col, scol);
  if (tiny) {
    immUniform1f("dash_width", 6.0f);
    immUniform1f("udash_factor", 0.5f);
  }
  else {
    immUniform1f("udash_factor", 2.0f); /* Solid line */
  }

  if ((track->pat_flag & SELECT) == sel && (sc->flag & SC_SHOW_MARKER_PATTERN)) {
    immBegin(GPU_PRIM_LINE_LOOP, 4);
    immVertex2fv(shdr_pos, marker->pattern_corners[0]);
    immVertex2fv(shdr_pos, marker->pattern_corners[1]);
    immVertex2fv(shdr_pos, marker->pattern_corners[2]);
    immVertex2fv(shdr_pos, marker->pattern_corners[3]);
    immEnd();
  }

  /* search */
  show_search = (TRACK_VIEW_SELECTED(sc, track) && ((marker->flag & MARKER_DISABLED) == 0 ||
                                                    (sc->flag & SC_SHOW_MARKER_PATTERN) == 0)) !=
                0;

  if ((track->search_flag & SELECT) == sel && (sc->flag & SC_SHOW_MARKER_SEARCH) && show_search) {
    set_draw_marker_area_color(track, marker, act, track->search_flag & SELECT, col, scol);

    imm_draw_box_wire_2d(shdr_pos,
                         marker->search_min[0],
                         marker->search_min[1],
                         marker->search_max[0],
                         marker->search_max[1]);
  }

  GPU_matrix_pop();

  immUnbindProgram();
  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  BLI_assert(pos == shdr_pos);
  UNUSED_VARS_NDEBUG(pos);
}

static float get_shortest_pattern_side(const MovieTrackingMarker *marker)
{
  float len_sq = FLT_MAX;

  for (int i = 0; i < 4; i++) {
    int next = (i + 1) % 4;
    float cur_len = len_squared_v2v2(marker->pattern_corners[i], marker->pattern_corners[next]);
    len_sq = min_ff(cur_len, len_sq);
  }

  return sqrtf(len_sq);
}

static void draw_marker_slide_square(
    float x, float y, float dx, float dy, int outline, const float px[2], uint pos)
{
  float tdx, tdy;

  tdx = dx;
  tdy = dy;

  if (outline) {
    tdx += px[0];
    tdy += px[1];
  }

  immRectf(pos, x - tdx, y - tdy, x + tdx, y + tdy);
}

static void draw_marker_slide_triangle(
    float x, float y, float dx, float dy, int outline, const float px[2], uint pos)
{
  float tdx, tdy;

  tdx = dx * 2.0f;
  tdy = dy * 2.0f;

  if (outline) {
    tdx += px[0];
    tdy += px[1];
  }

  immBegin(GPU_PRIM_TRIS, 3);
  immVertex2f(pos, x, y);
  immVertex2f(pos, x - tdx, y);
  immVertex2f(pos, x, y + tdy);
  immEnd();
}

static void draw_marker_slide_zones(SpaceClip *sc,
                                    const MovieTrackingTrack *track,
                                    const MovieTrackingMarker *marker,
                                    const float marker_pos[2],
                                    int outline,
                                    int sel,
                                    int act,
                                    int width,
                                    int height,
                                    uint pos)
{
  float dx, dy, patdx, patdy, searchdx, searchdy;
  int tiny = sc->flag & SC_SHOW_TINY_MARKER;
  float col[3], scol[3], px[2], side;

  if ((tiny && outline) || (marker->flag & MARKER_DISABLED)) {
    return;
  }

  if (!TRACK_VIEW_SELECTED(sc, track) || track->flag & TRACK_LOCKED) {
    return;
  }

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  track_colors(track, act, col, scol);

  if (outline) {
    immUniformThemeColor(TH_MARKER_OUTLINE);
  }

  GPU_matrix_push();
  GPU_matrix_translate_2fv(marker_pos);

  dx = 6.0f / width / sc->zoom;
  dy = 6.0f / height / sc->zoom;

  side = get_shortest_pattern_side(marker);
  patdx = min_ff(dx * 2.0f / 3.0f, side / 6.0f) * UI_SCALE_FAC;
  patdy = min_ff(dy * 2.0f / 3.0f, side * width / height / 6.0f) * UI_SCALE_FAC;

  searchdx = min_ff(dx, (marker->search_max[0] - marker->search_min[0]) / 6.0f) * UI_SCALE_FAC;
  searchdy = min_ff(dy, (marker->search_max[1] - marker->search_min[1]) / 6.0f) * UI_SCALE_FAC;

  px[0] = 1.0f / sc->zoom / width / sc->scale;
  px[1] = 1.0f / sc->zoom / height / sc->scale;

  if ((sc->flag & SC_SHOW_MARKER_SEARCH) && ((track->search_flag & SELECT) == sel || outline)) {
    if (!outline) {
      immUniformColor3fv((track->search_flag & SELECT) ? scol : col);
    }

    /* search offset square */
    draw_marker_slide_square(
        marker->search_min[0], marker->search_max[1], searchdx, searchdy, outline, px, pos);

    /* search re-sizing triangle */
    draw_marker_slide_triangle(
        marker->search_max[0], marker->search_min[1], searchdx, searchdy, outline, px, pos);
  }

  if ((sc->flag & SC_SHOW_MARKER_PATTERN) && ((track->pat_flag & SELECT) == sel || outline)) {
    float pat_min[2], pat_max[2];
    // float dx = 12.0f / width, dy = 12.0f / height; /*UNUSED*/
    float tilt_ctrl[2];

    if (!outline) {
      immUniformColor3fv((track->pat_flag & SELECT) ? scol : col);
    }

    /* pattern's corners sliding squares */
    for (int i = 0; i < 4; i++) {
      draw_marker_slide_square(marker->pattern_corners[i][0],
                               marker->pattern_corners[i][1],
                               patdx / 1.5f,
                               patdy / 1.5f,
                               outline,
                               px,
                               pos);
    }

    /* ** sliders to control overall pattern  ** */
    add_v2_v2v2(tilt_ctrl, marker->pattern_corners[1], marker->pattern_corners[2]);

    BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

    GPU_line_width(outline ? 3.0f : 1.0f);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, 0.0f, 0.0f);
    immVertex2fv(pos, tilt_ctrl);
    immEnd();

    /* slider to control pattern tilt */
    draw_marker_slide_square(tilt_ctrl[0], tilt_ctrl[1], patdx, patdy, outline, px, pos);
  }

  GPU_matrix_pop();
  immUnbindProgram();
}

static void draw_marker_texts(SpaceClip *sc,
                              const MovieTrackingTrack *track,
                              const MovieTrackingMarker *marker,
                              const float marker_pos[2],
                              int act,
                              int width,
                              int height,
                              float zoomx,
                              float zoomy)
{
  char str[128] = {0}, state[64] = {0};
  float dx = 0.0f, dy = 0.0f, fontsize, pos[3];
  uiStyle *style = static_cast<uiStyle *>(U.uistyles.first);
  int fontid = style->widget.uifont_id;

  if (!TRACK_VIEW_SELECTED(sc, track)) {
    return;
  }

  BLF_size(fontid, 11.0f * UI_SCALE_FAC);
  fontsize = BLF_height_max(fontid);

  if (marker->flag & MARKER_DISABLED) {
    if (act) {
      UI_FontThemeColor(fontid, TH_ACT_MARKER);
    }
    else {
      uchar color[4];
      UI_GetThemeColorShade4ubv(TH_DIS_MARKER, 128, color);
      BLF_color4ubv(fontid, color);
    }
  }
  else {
    UI_FontThemeColor(fontid, act ? TH_ACT_MARKER : TH_SEL_MARKER);
  }

  if ((sc->flag & SC_SHOW_MARKER_SEARCH) &&
      ((marker->flag & MARKER_DISABLED) == 0 || (sc->flag & SC_SHOW_MARKER_PATTERN) == 0))
  {
    dx = marker->search_min[0];
    dy = marker->search_min[1];
  }
  else if (sc->flag & SC_SHOW_MARKER_PATTERN) {
    float pat_min[2], pat_max[2];

    BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);
    dx = pat_min[0];
    dy = pat_min[1];
  }

  pos[0] = (marker_pos[0] + dx) * width;
  pos[1] = (marker_pos[1] + dy) * height;
  pos[2] = 0.0f;

  mul_m4_v3(sc->stabmat, pos);

  pos[0] = pos[0] * zoomx;
  pos[1] = pos[1] * zoomy - fontsize;

  if (marker->flag & MARKER_DISABLED) {
    STRNCPY_UTF8(state, "disabled");
  }
  else if (marker->framenr != ED_space_clip_get_clip_frame_number(sc)) {
    STRNCPY_UTF8(state, "estimated");
  }
  else if (marker->flag & MARKER_TRACKED) {
    STRNCPY_UTF8(state, "tracked");
  }
  else {
    STRNCPY_UTF8(state, "keyframed");
  }

  if (state[0]) {
    SNPRINTF_UTF8(str, "%s: %s", track->name, state);
  }
  else {
    STRNCPY_UTF8(str, track->name);
  }

  BLF_position(fontid, pos[0], pos[1], 0.0f);
  BLF_draw(fontid, str, sizeof(str));
  pos[1] -= fontsize;

  if (track->flag & TRACK_HAS_BUNDLE) {
    SNPRINTF_UTF8(str, "Average error: %.2f px", track->error);
    BLF_position(fontid, pos[0], pos[1], 0.0f);
    BLF_draw(fontid, str, sizeof(str));
    pos[1] -= fontsize;
  }

  if (track->flag & TRACK_LOCKED) {
    BLF_position(fontid, pos[0], pos[1], 0.0f);
    BLF_draw(fontid, "locked", 6);
  }
}

static void plane_track_colors(bool is_active, float r_color[3], float r_selected_color[3])
{
  UI_GetThemeColor3fv(TH_MARKER, r_color);

  UI_GetThemeColor3fv(is_active ? TH_ACT_MARKER : TH_SEL_MARKER, r_selected_color);
}

static void getArrowEndPoint(const int width,
                             const int height,
                             const float zoom,
                             const float start_corner[2],
                             const float end_corner[2],
                             float end_point[2])
{
  float direction[2];
  float max_length;

  sub_v2_v2v2(direction, end_corner, start_corner);

  direction[0] *= width;
  direction[1] *= height;
  max_length = normalize_v2(direction);
  mul_v2_fl(direction, min_ff(32.0f / zoom, max_length));
  direction[0] /= width;
  direction[1] /= height;

  add_v2_v2v2(end_point, start_corner, direction);
}

static void homogeneous_2d_to_gl_matrix(/*const*/ float matrix[3][3], float gl_matrix[4][4])
{
  gl_matrix[0][0] = matrix[0][0];
  gl_matrix[0][1] = matrix[0][1];
  gl_matrix[0][2] = 0.0f;
  gl_matrix[0][3] = matrix[0][2];

  gl_matrix[1][0] = matrix[1][0];
  gl_matrix[1][1] = matrix[1][1];
  gl_matrix[1][2] = 0.0f;
  gl_matrix[1][3] = matrix[1][2];

  gl_matrix[2][0] = 0.0f;
  gl_matrix[2][1] = 0.0f;
  gl_matrix[2][2] = 1.0f;
  gl_matrix[2][3] = 0.0f;

  gl_matrix[3][0] = matrix[2][0];
  gl_matrix[3][1] = matrix[2][1];
  gl_matrix[3][2] = 0.0f;
  gl_matrix[3][3] = matrix[2][2];
}

static void draw_plane_marker_image(Scene *scene,
                                    MovieTrackingPlaneTrack *plane_track,
                                    MovieTrackingPlaneMarker *plane_marker)
{
  Image *image = plane_track->image;
  ImBuf *ibuf;
  void *lock;

  if (image == nullptr) {
    return;
  }

  ibuf = BKE_image_acquire_ibuf(image, nullptr, &lock);

  if (ibuf) {
    void *cache_handle;
    const uchar *display_buffer = IMB_display_buffer_acquire(
        ibuf, &scene->view_settings, &scene->display_settings, &cache_handle);

    if (display_buffer) {
      float frame_corners[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
      float perspective_matrix[3][3];
      float gl_matrix[4][4];
      bool transparent = false;
      BKE_tracking_homography_between_two_quads(
          frame_corners, plane_marker->corners, perspective_matrix);

      homogeneous_2d_to_gl_matrix(perspective_matrix, gl_matrix);

      if (plane_track->image_opacity != 1.0f || ibuf->planes == 32) {
        transparent = true;
        GPU_blend(GPU_BLEND_ALPHA);
      }

      blender::gpu::Texture *texture = GPU_texture_create_2d(
          "plane_marker_image",
          ibuf->x,
          ibuf->y,
          1,
          blender::gpu::TextureFormat::UNORM_8_8_8_8,
          GPU_TEXTURE_USAGE_SHADER_READ,
          nullptr);
      GPU_texture_update(texture, GPU_DATA_UBYTE, display_buffer);
      GPU_texture_filter_mode(texture, false);

      GPU_matrix_push();
      GPU_matrix_mul(gl_matrix);

      GPUVertFormat *imm_format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(
          imm_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
      uint texCoord = GPU_vertformat_attr_add(
          imm_format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);

      /* Use 3D image for correct display of planar tracked images. */
      immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);

      immBindTexture("image", texture);
      immUniformColor4f(1.0f, 1.0f, 1.0f, plane_track->image_opacity);

      immBegin(GPU_PRIM_TRI_FAN, 4);

      immAttr2f(texCoord, 0.0f, 0.0f);
      immVertex3f(pos, 0.0f, 0.0f, 0.0f);

      immAttr2f(texCoord, 1.0f, 0.0f);
      immVertex3f(pos, 1.0f, 0.0f, 0.0f);

      immAttr2f(texCoord, 1.0f, 1.0f);
      immVertex3f(pos, 1.0f, 1.0f, 0.0f);

      immAttr2f(texCoord, 0.0f, 1.0f);
      immVertex3f(pos, 0.0f, 1.0f, 0.0f);

      immEnd();

      immUnbindProgram();

      GPU_matrix_pop();

      GPU_texture_unbind(texture);
      GPU_texture_free(texture);

      if (transparent) {
        GPU_blend(GPU_BLEND_NONE);
      }
    }

    IMB_display_buffer_release(cache_handle);
  }

  BKE_image_release_ibuf(image, ibuf, lock);
}

static void draw_plane_marker_ex(SpaceClip *sc,
                                 Scene *scene,
                                 MovieTrackingPlaneTrack *plane_track,
                                 MovieTrackingPlaneMarker *plane_marker,
                                 bool is_active_track,
                                 bool draw_outline,
                                 int width,
                                 int height)
{
  bool tiny = (sc->flag & SC_SHOW_TINY_MARKER) != 0;
  bool is_selected_track = (plane_track->flag & SELECT) != 0;
  const bool has_image = plane_track->image != nullptr &&
                         BKE_image_has_ibuf(plane_track->image, nullptr);
  const bool draw_plane_quad = !has_image || plane_track->image_opacity == 0.0f;
  float px[2];
  float color[3], selected_color[3];

  px[0] = 1.0f / width / sc->zoom;
  px[1] = 1.0f / height / sc->zoom;

  /* Draw image */
  if (draw_outline == false) {
    draw_plane_marker_image(scene, plane_track, plane_marker);
  }

  if (draw_plane_quad || is_selected_track) {
    const uint shdr_pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f(
        "viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

    immUniform1i("colors_len", 0); /* "simple" mode */

    if (is_selected_track) {
      plane_track_colors(is_active_track, color, selected_color);
    }

    if (draw_plane_quad) {
      const bool stipple = !draw_outline && tiny;
      const bool thick = draw_outline && !tiny;

      GPU_line_width(thick ? 3.0f : 1.0f);

      if (stipple) {
        immUniform1f("dash_width", 6.0f);
        immUniform1f("udash_factor", 0.5f);
      }
      else {
        immUniform1f("udash_factor", 2.0f); /* Solid line */
      }

      if (draw_outline) {
        immUniformThemeColor(TH_MARKER_OUTLINE);
      }
      else {
        immUniformColor3fv(is_selected_track ? selected_color : color);
      }

      /* Draw rectangle itself. */
      immBegin(GPU_PRIM_LINE_LOOP, 4);
      immVertex2fv(shdr_pos, plane_marker->corners[0]);
      immVertex2fv(shdr_pos, plane_marker->corners[1]);
      immVertex2fv(shdr_pos, plane_marker->corners[2]);
      immVertex2fv(shdr_pos, plane_marker->corners[3]);
      immEnd();

      /* Draw axis. */
      if (!draw_outline) {
        float end_point[2];

        immUniformColor3f(1.0f, 0.0f, 0.0f);

        immBegin(GPU_PRIM_LINES, 2);

        getArrowEndPoint(width,
                         height,
                         sc->zoom,
                         plane_marker->corners[0],
                         plane_marker->corners[1],
                         end_point);
        immVertex2fv(shdr_pos, plane_marker->corners[0]);
        immVertex2fv(shdr_pos, end_point);

        immEnd();

        immUniformColor3f(0.0f, 1.0f, 0.0f);

        immBegin(GPU_PRIM_LINES, 2);

        getArrowEndPoint(width,
                         height,
                         sc->zoom,
                         plane_marker->corners[0],
                         plane_marker->corners[3],
                         end_point);
        immVertex2fv(shdr_pos, plane_marker->corners[0]);
        immVertex2fv(shdr_pos, end_point);

        immEnd();
      }
    }
    immUnbindProgram();

    /* Draw sliders. */
    if (is_selected_track) {
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      if (draw_outline) {
        immUniformThemeColor(TH_MARKER_OUTLINE);
      }
      else {
        immUniformColor3fv(selected_color);
      }

      for (int i = 0; i < 4; i++) {
        draw_marker_slide_square(plane_marker->corners[i][0],
                                 plane_marker->corners[i][1],
                                 3.0f * px[0],
                                 3.0f * px[1],
                                 draw_outline,
                                 px,
                                 shdr_pos);
      }
      immUnbindProgram();
    }
  }
}

static void draw_plane_marker_outline(SpaceClip *sc,
                                      Scene *scene,
                                      MovieTrackingPlaneTrack *plane_track,
                                      MovieTrackingPlaneMarker *plane_marker,
                                      int width,
                                      int height)
{
  draw_plane_marker_ex(sc, scene, plane_track, plane_marker, false, true, width, height);
}

static void draw_plane_marker(SpaceClip *sc,
                              Scene *scene,
                              MovieTrackingPlaneTrack *plane_track,
                              MovieTrackingPlaneMarker *plane_marker,
                              bool is_active_track,
                              int width,
                              int height)
{
  draw_plane_marker_ex(
      sc, scene, plane_track, plane_marker, is_active_track, false, width, height);
}

static void draw_plane_track(SpaceClip *sc,
                             Scene *scene,
                             MovieTrackingPlaneTrack *plane_track,
                             int framenr,
                             bool is_active_track,
                             int width,
                             int height)
{
  MovieTrackingPlaneMarker *plane_marker;

  plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

  draw_plane_marker_outline(sc, scene, plane_track, plane_marker, width, height);
  draw_plane_marker(sc, scene, plane_track, plane_marker, is_active_track, width, height);
}

/* Draw all kind of tracks. */
static void draw_tracking_tracks(SpaceClip *sc,
                                 Scene *scene,
                                 ARegion *region,
                                 MovieClip *clip,
                                 int width,
                                 int height,
                                 float zoomx,
                                 float zoomy)
{
  float x, y;
  /*const*/ MovieTracking *tracking = &clip->tracking;
  /*const*/ MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  /*const*/ MovieTrackingTrack *active_track = tracking_object->active_track;
  /*const*/ MovieTrackingPlaneTrack *active_plane_track = tracking_object->active_plane_track;
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  const int undistort = sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT;
  float *marker_pos = nullptr, *fp, *active_pos = nullptr, cur_pos[2];

  /* ** find window pixel coordinates of origin ** */

  /* UI_view2d_view_to_region_no_clip return integer values, this could
   * lead to 1px flickering when view is locked to selection during playback.
   * to avoid this flickering, calculate base point in the same way as it happens
   * in UI_view2d_view_to_region_no_clip, but do it in floats here */

  UI_view2d_view_to_region_fl(&region->v2d, 0.0f, 0.0f, &x, &y);

  GPU_matrix_push();
  GPU_matrix_translate_2f(x, y);

  GPU_matrix_push();
  GPU_matrix_scale_2f(zoomx, zoomy);
  GPU_matrix_mul(sc->stabmat);
  GPU_matrix_scale_2f(width, height);

  /* Draw plane tracks */
  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
    if ((plane_track->flag & PLANE_TRACK_HIDDEN) == 0) {
      draw_plane_track(
          sc, scene, plane_track, framenr, plane_track == active_plane_track, width, height);
    }
  }

  if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
    int count = 0;

    /* count */
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if (track->flag & TRACK_HIDDEN) {
        continue;
      }

      const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

      if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
        count++;
      }
    }

    /* undistort */
    if (count) {
      marker_pos = MEM_calloc_arrayN<float>(2 * count, "draw_tracking_tracks marker_pos");

      fp = marker_pos;
      LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
        if (track->flag & TRACK_HIDDEN) {
          continue;
        }

        const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

        if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
          ED_clip_point_undistorted_pos(sc, marker->pos, fp);

          if (track == active_track) {
            active_pos = fp;
          }

          fp += 2;
        }
      }
    }
  }

  if (sc->flag & SC_SHOW_TRACK_PATH) {
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if ((track->flag & TRACK_HIDDEN) == 0) {
        draw_track_path(sc, clip, track);
      }
    }
  }

  const uint position = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  /* markers outline and non-selected areas */
  fp = marker_pos;
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (track->flag & TRACK_HIDDEN) {
      continue;
    }

    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

    if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
      copy_v2_v2(cur_pos, fp ? fp : marker->pos);

      draw_marker_outline(sc, track, marker, cur_pos, width, height, position);
      draw_marker_areas(sc, track, marker, cur_pos, width, height, 0, 0, position);
      draw_marker_slide_zones(sc, track, marker, cur_pos, 1, 0, 0, width, height, position);
      draw_marker_slide_zones(sc, track, marker, cur_pos, 0, 0, 0, width, height, position);

      if (fp) {
        fp += 2;
      }
    }
  }

  /* selected areas only, so selection wouldn't be overlapped by non-selected areas */
  fp = marker_pos;
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (track->flag & TRACK_HIDDEN) {
      continue;
    }
    const int act = track == active_track;
    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

    if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
      if (!act) {
        copy_v2_v2(cur_pos, fp ? fp : marker->pos);

        draw_marker_areas(sc, track, marker, cur_pos, width, height, 0, 1, position);
        draw_marker_slide_zones(sc, track, marker, cur_pos, 0, 1, 0, width, height, position);
      }

      if (fp) {
        fp += 2;
      }
    }
  }

  /* active marker would be displayed on top of everything else */
  if (active_track && (active_track->flag & TRACK_HIDDEN) == 0) {
    const MovieTrackingMarker *marker = BKE_tracking_marker_get(active_track, framenr);

    if (ED_space_clip_marker_is_visible(sc, tracking_object, active_track, marker)) {
      copy_v2_v2(cur_pos, active_pos ? active_pos : marker->pos);

      draw_marker_areas(sc, active_track, marker, cur_pos, width, height, 1, 1, position);
      draw_marker_slide_zones(sc, active_track, marker, cur_pos, 0, 1, 1, width, height, position);
    }
  }

  if (sc->flag & SC_SHOW_BUNDLES) {
    float pos[4], vec[4], mat[4][4], aspy;

    aspy = 1.0f / clip->tracking.camera.pixel_aspect;
    BKE_tracking_get_projection_matrix(tracking, tracking_object, framenr, width, height, mat);

    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
    immUniform1f("size", 3.0f);
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if (track->flag & TRACK_HIDDEN || (track->flag & TRACK_HAS_BUNDLE) == 0) {
        continue;
      }

      const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

      if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
        float npos[2];
        copy_v3_v3(vec, track->bundle_pos);
        vec[3] = 1;

        mul_v4_m4v4(pos, mat, vec);

        pos[0] = (pos[0] / (pos[3] * 2.0f) + 0.5f) * width;
        pos[1] = (pos[1] / (pos[3] * 2.0f) + 0.5f) * height * aspy;

        BKE_tracking_distort_v2(tracking, width, height, pos, npos);

        if (npos[0] >= 0.0f && npos[1] >= 0.0f && npos[0] <= width && npos[1] <= height * aspy) {
          vec[0] = (marker->pos[0] + track->offset[0]) * width;
          vec[1] = (marker->pos[1] + track->offset[1]) * height * aspy;

          sub_v2_v2(vec, npos);

          immUniformColor4fv((len_squared_v2(vec) < (3.0f * 3.0f)) ?
                                 blender::float4(0.0f, 1.0f, 0.0f, 1.0f) :
                                 blender::float4(1.0f, 0.0f, 0.0f, 1.0f));

          immBegin(GPU_PRIM_POINTS, 1);

          if (undistort) {
            immVertex2f(position, pos[0] / width, pos[1] / (height * aspy));
          }
          else {
            immVertex2f(position, npos[0] / width, npos[1] / (height * aspy));
          }

          immEnd();
        }
      }
    }
    immUnbindProgram();
  }

  GPU_matrix_pop();

  if (sc->flag & SC_SHOW_NAMES) {
    /* scaling should be cleared before drawing texts, otherwise font would also be scaled */
    fp = marker_pos;
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if (track->flag & TRACK_HIDDEN) {
        continue;
      }

      const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

      if (ED_space_clip_marker_is_visible(sc, tracking_object, track, marker)) {
        const int act = track == active_track;

        copy_v2_v2(cur_pos, fp ? fp : marker->pos);

        draw_marker_texts(sc, track, marker, cur_pos, act, width, height, zoomx, zoomy);

        if (fp) {
          fp += 2;
        }
      }
    }
  }

  GPU_matrix_pop();

  if (marker_pos) {
    MEM_freeN(marker_pos);
  }
}

static void draw_distortion(SpaceClip *sc,
                            ARegion *region,
                            MovieClip *clip,
                            int width,
                            int height,
                            float zoomx,
                            float zoomy)
{
  float x, y;
  const int n = 10;
  float tpos[2], grid[11][11][2];
  MovieTracking *tracking = &clip->tracking;
  bGPdata *gpd = nullptr;
  float aspy = 1.0f / tracking->camera.pixel_aspect;
  float dx = float(width) / n, dy = float(height) / n * aspy;
  float offsx = 0.0f, offsy = 0.0f;

  if (!tracking->camera.focal) {
    return;
  }

  if ((sc->flag & SC_SHOW_GRID) == 0 && (sc->flag & SC_MANUAL_CALIBRATION) == 0) {
    return;
  }

  UI_view2d_view_to_region_fl(&region->v2d, 0.0f, 0.0f, &x, &y);

  GPU_matrix_push();
  GPU_matrix_translate_2f(x, y);
  GPU_matrix_scale_2f(zoomx, zoomy);
  GPU_matrix_mul(sc->stabmat);
  GPU_matrix_scale_2f(width, height);

  uint position = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* grid */
  if (sc->overlay.flag & SC_SHOW_OVERLAYS && sc->flag & SC_SHOW_GRID) {
    float val[4][2], idx[4][2];
    float min[2], max[2];

    for (int a = 0; a < 4; a++) {
      if (a < 2) {
        val[a][a % 2] = FLT_MAX;
      }
      else {
        val[a][a % 2] = -FLT_MAX;
      }
    }

    for (int i = 0; i <= n; i++) {
      for (int j = 0; j <= n; j++) {
        if (i == 0 || j == 0 || i == n || j == n) {
          const float pos[2] = {dx * j, dy * i};
          BKE_tracking_distort_v2(tracking, width, height, pos, tpos);

          for (int a = 0; a < 4; a++) {
            int ok;

            if (a < 2) {
              ok = tpos[a % 2] < val[a][a % 2];
            }
            else {
              ok = tpos[a % 2] > val[a][a % 2];
            }

            if (ok) {
              copy_v2_v2(val[a], tpos);
              idx[a][0] = j;
              idx[a][1] = i;
            }
          }
        }
      }
    }

    INIT_MINMAX2(min, max);

    for (int a = 0; a < 4; a++) {
      const float pos[2] = {idx[a][0] * dx, idx[a][1] * dy};

      BKE_tracking_undistort_v2(tracking, width, height, pos, tpos);

      minmax_v2v2_v2(min, max, tpos);
    }

    dx = (max[0] - min[0]) / n;
    dy = (max[1] - min[1]) / n;

    for (int i = 0; i <= n; i++) {
      for (int j = 0; j <= n; j++) {
        const float pos[2] = {min[0] + dx * j, min[1] + dy * i};

        BKE_tracking_distort_v2(tracking, width, height, pos, grid[i][j]);

        grid[i][j][0] /= width;
        grid[i][j][1] /= height * aspy;
      }
    }

    immUniformColor3f(1.0f, 0.0f, 0.0f);

    for (int i = 0; i <= n; i++) {
      immBegin(GPU_PRIM_LINE_STRIP, n + 1);

      for (int j = 0; j <= n; j++) {
        immVertex2fv(position, grid[i][j]);
      }

      immEnd();
    }

    for (int j = 0; j <= n; j++) {
      immBegin(GPU_PRIM_LINE_STRIP, n + 1);

      for (int i = 0; i <= n; i++) {
        immVertex2fv(position, grid[i][j]);
      }

      immEnd();
    }
  }

  if (sc->gpencil_src != SC_GPENCIL_SRC_TRACK) {
    gpd = clip->gpd;
  }

  if (sc->flag & SC_MANUAL_CALIBRATION && gpd) {
    bGPDlayer *layer = static_cast<bGPDlayer *>(gpd->layers.first);

    while (layer) {
      bGPDframe *frame = static_cast<bGPDframe *>(layer->frames.first);

      if (layer->flag & GP_LAYER_HIDE) {
        layer = layer->next;
        continue;
      }

      immUniformColor4fv(layer->color);

      GPU_line_width(layer->thickness);
      GPU_point_size(float(layer->thickness + 2));

      while (frame) {
        bGPDstroke *stroke = static_cast<bGPDstroke *>(frame->strokes.first);

        while (stroke) {
          if (stroke->flag & GP_STROKE_2DSPACE) {
            if (stroke->totpoints > 1) {
              for (int i = 0; i < stroke->totpoints - 1; i++) {
                float pos[2], npos[2], dpos[2], len;
                int steps;

                pos[0] = (stroke->points[i].x + offsx) * width;
                pos[1] = (stroke->points[i].y + offsy) * height * aspy;

                npos[0] = (stroke->points[i + 1].x + offsx) * width;
                npos[1] = (stroke->points[i + 1].y + offsy) * height * aspy;

                len = len_v2v2(pos, npos);
                steps = ceil(len / 5.0f);

                /* we want to distort only long straight lines */
                if (stroke->totpoints == 2) {
                  BKE_tracking_undistort_v2(tracking, width, height, pos, pos);
                  BKE_tracking_undistort_v2(tracking, width, height, npos, npos);
                }

                sub_v2_v2v2(dpos, npos, pos);
                mul_v2_fl(dpos, 1.0f / steps);

                immBegin(GPU_PRIM_LINE_STRIP, steps + 1);

                for (int j = 0; j <= steps; j++) {
                  BKE_tracking_distort_v2(tracking, width, height, pos, tpos);
                  immVertex2f(position, tpos[0] / width, tpos[1] / (height * aspy));

                  add_v2_v2(pos, dpos);
                }

                immEnd();
              }
            }
            else if (stroke->totpoints == 1) {
              immBegin(GPU_PRIM_POINTS, 1);
              immVertex2f(position, stroke->points[0].x + offsx, stroke->points[0].y + offsy);
              immEnd();
            }
          }

          stroke = stroke->next;
        }

        frame = frame->next;
      }

      layer = layer->next;
    }
  }

  immUnbindProgram();

  GPU_matrix_pop();
}

void clip_draw_main(const bContext *C, SpaceClip *sc, ARegion *region)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  Scene *scene = CTX_data_scene(C);
  ImBuf *ibuf = nullptr;
  int width, height;
  float zoomx, zoomy;

  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_zoom(sc, region, &zoomx, &zoomy);

  /* if no clip, nothing to do */
  if (!clip) {
    ED_region_grid_draw(region, zoomx, zoomy, 0.0f, 0.0f);
    return;
  }

  if (sc->flag & SC_SHOW_STABLE) {
    float translation[2];
    float aspect = clip->tracking.camera.pixel_aspect;
    float smat[4][4], ismat[4][4];

    if ((sc->flag & SC_MUTE_FOOTAGE) == 0) {
      ibuf = ED_space_clip_get_stable_buffer(sc, sc->loc, &sc->scale, &sc->angle);
    }

    if (ibuf != nullptr && width != ibuf->x) {
      mul_v2_v2fl(translation, sc->loc, float(width) / ibuf->x);
    }
    else {
      copy_v2_v2(translation, sc->loc);
    }

    BKE_tracking_stabilization_data_to_mat4(
        width, height, aspect, translation, sc->scale, sc->angle, sc->stabmat);

    unit_m4(smat);
    smat[0][0] = 1.0f / width;
    smat[1][1] = 1.0f / height;
    invert_m4_m4(ismat, smat);

    mul_m4_series(sc->unistabmat, smat, sc->stabmat, ismat);
  }
  else if ((sc->flag & SC_MUTE_FOOTAGE) == 0) {
    ibuf = ED_space_clip_get_buffer(sc);

    zero_v2(sc->loc);
    sc->scale = 1.0f;
    unit_m4(sc->stabmat);
    unit_m4(sc->unistabmat);
  }

  if (ibuf) {
    draw_movieclip_buffer(C, sc, region, ibuf, width, height, zoomx, zoomy);
    IMB_freeImBuf(ibuf);
  }
  else if (sc->flag & SC_MUTE_FOOTAGE) {
    draw_movieclip_muted(region, width, height, zoomx, zoomy);
  }
  else {
    ED_region_grid_draw(region, zoomx, zoomy, 0.0f, 0.0f);
  }

  if (width && height) {
    draw_stabilization_border(sc, region, width, height, zoomx, zoomy);
    if (sc->overlay.flag & SC_SHOW_OVERLAYS)
      draw_tracking_tracks(sc, scene, region, clip, width, height, zoomx, zoomy);
    draw_distortion(sc, region, clip, width, height, zoomx, zoomy);
  }
}

void clip_draw_cache_and_notes(const bContext *C, SpaceClip *sc, ARegion *region)
{
  Scene *scene = CTX_data_scene(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  if (clip) {
    draw_movieclip_cache(sc, region, clip, scene);
    draw_movieclip_notes(sc, region);
  }
}

void clip_draw_grease_pencil(bContext *C, int onlyv2d)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);

  if (!clip) {
    return;
  }

  if (onlyv2d) {
    bool is_track_source = sc->gpencil_src == SC_GPENCIL_SRC_TRACK;
    /* if manual calibration is used then grease pencil data
     * associated with the clip is already drawn in draw_distortion
     */
    if ((sc->flag & SC_MANUAL_CALIBRATION) == 0 || is_track_source) {
      GPU_matrix_push();
      GPU_matrix_mul(sc->unistabmat);

      if (is_track_source) {
        const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(
            &clip->tracking);
        MovieTrackingTrack *active_track = tracking_object->active_track;

        if (active_track) {
          const int framenr = ED_space_clip_get_clip_frame_number(sc);
          MovieTrackingMarker *marker = BKE_tracking_marker_get(active_track, framenr);

          GPU_matrix_translate_2fv(marker->pos);
        }
      }

      ED_annotation_draw_2dimage(C);

      GPU_matrix_pop();
    }
  }
  else {
    ED_annotation_draw_view2d(C, false);
  }
}
