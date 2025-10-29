/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <algorithm>
#include <cmath>
#include <cstring>

#include "BLF_api.hh"

#include "BLI_index_range.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "GPU_compute.hh"
#include "GPU_debug.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_primitive.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_viewport.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_space_api.hh"
#include "ED_util.hh"
#include "ED_view3d.hh"

#include "BIF_glutil.hh"

#include "SEQ_channels.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_prefetch.hh"
#include "SEQ_preview_cache.hh"
#include "SEQ_proxy.hh"
#include "SEQ_render.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "sequencer_intern.hh"
#include "sequencer_quads_batch.hh"
#include "sequencer_scopes.hh"

namespace blender::ed::vse {
static Strip *special_seq_update = nullptr;

void sequencer_special_update_set(Strip *strip)
{
  special_seq_update = strip;
}

Strip *special_preview_get()
{
  return special_seq_update;
}

void special_preview_set(bContext *C, const int mval[2])
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!seq::editing_get(scene)) {
    return;
  }

  ARegion *region = CTX_wm_region(C);
  Strip *strip = strip_under_mouse_get(scene, &region->v2d, mval);
  if (strip != nullptr && strip->type != STRIP_TYPE_SOUND_RAM) {
    sequencer_special_update_set(strip);
  }
}

void special_preview_clear()
{
  sequencer_special_update_set(nullptr);
}

ImBuf *sequencer_ibuf_get(const bContext *C, const int timeline_frame, const char *viewname)
{
  Main *bmain = CTX_data_main(C);
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  bScreen *screen = CTX_wm_screen(C);

  seq::RenderData context = {nullptr};
  ImBuf *ibuf;
  short is_break = G.is_break;
  const eSpaceSeq_Proxy_RenderSize render_size_mode = eSpaceSeq_Proxy_RenderSize(
      sseq->render_size);
  if (render_size_mode == SEQ_RENDER_SIZE_NONE) {
    return nullptr;
  }

  const float render_scale = seq::get_render_scale_factor(render_size_mode, scene->r.size);
  int rectx = roundf(render_scale * scene->r.xsch);
  int recty = roundf(render_scale * scene->r.ysch);

  seq::render_new_render_data(
      bmain, depsgraph, scene, rectx, recty, render_size_mode, false, &context);
  context.view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);
  context.use_proxies = (sseq->flag & SEQ_USE_PROXIES) != 0;
  context.is_playing = screen->animtimer != nullptr;
  context.is_scrubbing = screen->scrubbing;

  /* Sequencer could start rendering, in this case we need to be sure it wouldn't be
   * canceled by Escape pressed somewhere in the past. */
  G.is_break = false;

  GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);
  gpu::FrameBuffer *fb = GPU_framebuffer_active_get();
  if (viewport) {
    /* Unbind viewport to release the DRW context. */
    GPU_viewport_unbind(viewport);
  }
  else {
    /* Rendering can change OGL context. Save & Restore frame-buffer. */
    GPU_framebuffer_restore();
  }

  if (special_preview_get()) {
    ibuf = seq::render_give_ibuf_direct(&context, timeline_frame, special_preview_get());
  }
  else {
    ibuf = seq::render_give_ibuf(&context, timeline_frame, sseq->chanshown);
  }

  if (viewport) {
    /* Follows same logic as wm_draw_window_offscreen to make sure to restore the same
     * viewport. */
    int view = (sseq->multiview_eye == STEREO_RIGHT_ID) ? 1 : 0;
    GPU_viewport_bind(viewport, view, &region->winrct);
  }
  else if (fb) {
    GPU_framebuffer_bind(fb);
  }

  /* Restore state so real rendering would be canceled if needed. */
  G.is_break = is_break;

  return ibuf;
}

static void sequencer_display_size(const RenderData &render_data, float r_viewrect[2])
{
  r_viewrect[0] = float(render_data.xsch);
  r_viewrect[1] = float(render_data.ysch);

  r_viewrect[0] *= render_data.xasp / render_data.yasp;
}

static void sequencer_draw_gpencil_overlay(const bContext *C)
{
  /* Draw grease-pencil (image aligned). */
  ED_annotation_draw_2dimage(C);

  /* Orthographic at pixel level. */
  UI_view2d_view_restore(C);

  /* Draw grease-pencil (screen aligned). */
  ED_annotation_draw_view2d(C, false);
}

/**
 * Draw content and safety borders.
 */
static void sequencer_draw_borders_overlay(const SpaceSeq &sseq,
                                           const View2D &v2d,
                                           const Scene *scene)
{
  const float x1 = v2d.tot.xmin;
  const float y1 = v2d.tot.ymin;
  const float x2 = v2d.tot.xmax;
  const float y2 = v2d.tot.ymax;

  GPU_line_width(1.0f);

  /* Draw border. */
  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

  immUniformThemeColor(TH_BACK);
  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniform1f("dash_width", 6.0f);
  immUniform1f("udash_factor", 0.5f);

  imm_draw_box_wire_2d(shdr_pos, x1 - 0.5f, y1 - 0.5f, x2 + 0.5f, y2 + 0.5f);

  /* Draw safety border. */
  if (sseq.preview_overlay.flag & SEQ_PREVIEW_SHOW_SAFE_MARGINS) {
    immUniformThemeColorBlend(TH_VIEW_OVERLAY, TH_BACK, 0.25f);
    rctf rect;
    rect.xmin = x1;
    rect.xmax = x2;
    rect.ymin = y1;
    rect.ymax = y2;
    UI_draw_safe_areas(shdr_pos, &rect, scene->safe_areas.title, scene->safe_areas.action);

    if (sseq.preview_overlay.flag & SEQ_PREVIEW_SHOW_SAFE_CENTER) {

      UI_draw_safe_areas(
          shdr_pos, &rect, scene->safe_areas.title_center, scene->safe_areas.action_center);
    }
  }

  immUnbindProgram();
}

#if 0
void sequencer_draw_maskedit(const bContext *C, Scene *scene, ARegion *region, SpaceSeq *sseq)
{
  /* NOTE: sequencer mask editing isn't finished, the draw code is working but editing not.
   * For now just disable drawing since the strip frame will likely be offset. */

  // if (sc->mode == SC_MODE_MASKEDIT)
  if (0 && sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
    Mask *mask = SEQ_active_mask_get(scene);

    if (mask) {
      int width, height;
      float aspx = 1.0f, aspy = 1.0f;
      // ED_mask_get_size(C, &width, &height);

      // Scene *scene = CTX_data_sequencer_scene(C);
      BKE_render_resolution(&scene->r, false, &width, &height);

      ED_mask_draw_region(mask,
                          region,
                          true,
                          0,
                          0,
                          0, /* TODO */
                          width,
                          height,
                          aspx,
                          aspy,
                          false,
                          true,
                          nullptr,
                          C);
    }
  }
}
#endif

/* Force redraw, when prefetching and using cache view. */
static void seq_prefetch_wm_notify(const bContext *C, Scene *scene)
{
  if (seq::prefetch_need_redraw(C, scene)) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, nullptr);
  }
}

static void sequencer_stop_running_jobs(const bContext *C, Scene *scene)
{
  if (G.is_rendering == false && (scene->r.seq_prev_type) == OB_RENDER) {
    /* Stop all running jobs, except screen one. Currently previews frustrate Render.
     * Need to make so sequencers rendering doesn't conflict with compositor. */
    WM_jobs_kill_type(CTX_wm_manager(C), nullptr, WM_JOB_TYPE_COMPOSITE);

    /* In case of final rendering used for preview, kill all previews,
     * otherwise threading conflict will happen in rendering module. */
    WM_jobs_kill_type(CTX_wm_manager(C), nullptr, WM_JOB_TYPE_RENDER_PREVIEW);
  }
}

static void sequencer_preview_clear()
{
  UI_ThemeClearColor(TH_SEQ_PREVIEW);
}

/* Semantic utility to get a rectangle with positions that correspond to a full frame drawn in the
 * preview region. */
static rctf preview_get_full_position(const ARegion &region)
{
  return region.v2d.tot;
}

/* Semantic utility to generate rectangle with UV coordinates that cover an entire 0 .. 1
 * rectangle. */
static rctf preview_get_full_texture_coord()
{
  rctf texture_coord;
  BLI_rctf_init(&texture_coord, 0.0f, 1.0f, 0.0f, 1.0f);
  return texture_coord;
}

/* Get rectangle positions within preview region that are to be used to draw reference frame.
 *
 * If the frame overlay is set to RECTANGLE this function returns coordinates of the rectangle
 * where partial reference frame is to be drawn.
 *
 * If the frame overlay is set to REFERENCE this function returns full-frame rectangle, same as
 * preview_get_full_position().
 *
 * If the frame overlay is set to REFERENCE or is disabled the return value is valid but
 * corresponds to an undefined state.
 */
static rctf preview_get_reference_position(const SpaceSeq &space_sequencer,
                                           const Editing &editing,
                                           const ARegion &region)
{
  const View2D &v2d = region.v2d;

  BLI_assert(ELEM(space_sequencer.overlay_frame_type,
                  SEQ_OVERLAY_FRAME_TYPE_RECT,
                  SEQ_OVERLAY_FRAME_TYPE_REFERENCE));

  if (space_sequencer.overlay_frame_type == SEQ_OVERLAY_FRAME_TYPE_RECT) {
    rctf position;
    const float xmin = v2d.tot.xmin;
    const float ymin = v2d.tot.ymin;

    const float width = BLI_rctf_size_x(&v2d.tot);
    const float height = BLI_rctf_size_y(&v2d.tot);

    position.xmax = xmin + width * editing.overlay_frame_rect.xmax;
    position.xmin = xmin + width * editing.overlay_frame_rect.xmin;
    position.ymax = ymin + height * editing.overlay_frame_rect.ymax;
    position.ymin = ymin + height * editing.overlay_frame_rect.ymin;

    return position;
  }

  return v2d.tot;
}

/* Return rectangle with UV coordinates that are to be used to draw reference frame.
 *
 * If the frame overlay is set to rectangle the return value contains vUV coordinates of the
 * rectangle within the reference frame.
 *
 * If the frame overlay is set to REFERENCE this function returns full-frame UV rectangle, same as
 * preview_get_full_texture_coord().
 *
 * If the frame overlay is set to REFERENCE or is disabled the return value is valid but
 * corresponds to an undefined state.
 */
static rctf preview_get_reference_texture_coord(const SpaceSeq &space_sequencer,
                                                const Editing &editing)
{
  if (space_sequencer.overlay_frame_type == SEQ_OVERLAY_FRAME_TYPE_RECT) {
    return editing.overlay_frame_rect;
  }

  rctf texture_coord;
  BLI_rctf_init(&texture_coord, 0.0f, 1.0f, 0.0f, 1.0f);

  return texture_coord;
}

static void add_vertical_line(const float val,
                              const uchar4 color,
                              View2D &v2d,
                              const float text_scale_x,
                              const float text_scale_y,
                              SeqQuadsBatch &quads,
                              const rctf &area)
{
  const float x = area.xmin + (area.xmax - area.xmin) * val;

  char buf[20];
  const size_t buf_len = SNPRINTF_UTF8_RLEN(buf, "%.2f", val);
  float text_width, text_height;
  BLF_width_and_height(BLF_default(), buf, buf_len, &text_width, &text_height);
  text_width *= text_scale_x;
  text_height *= text_scale_y;
  UI_view2d_text_cache_add(
      &v2d, x - text_width / 2, area.ymax - text_height * 1.3f, buf, buf_len, color);

  quads.add_line(x, area.ymin, x, area.ymax - text_height * 1.4f, color);
}

static void draw_histogram(ARegion &region,
                           const ScopeHistogram &hist,
                           SeqQuadsBatch &quads,
                           const rctf &area)
{
  if (hist.data.is_empty()) {
    return;
  }

  /* Grid lines and labels. */
  View2D &v2d = region.v2d;
  float text_scale_x, text_scale_y;
  UI_view2d_scale_get_inverse(&v2d, &text_scale_x, &text_scale_y);

  const bool hdr = ScopeHistogram::bin_to_float(math::reduce_max(hist.max_bin)) > 1.001f;
  const float max_val = hdr ? 12.0f : 1.0f;

  /* Grid lines covering 0..1 range, with 0.25 steps. */
  const uchar col_grid[4] = {128, 128, 128, 128};
  for (float val = 0.0f; val <= 1.0f; val += 0.25f) {
    add_vertical_line(val, col_grid, v2d, text_scale_x, text_scale_y, quads, area);
  }
  /* For HDR content, more lines every 1.0 step. */
  if (hdr) {
    for (float val = 2.0f; val <= max_val; val += 1.0f) {
      add_vertical_line(val, col_grid, v2d, text_scale_x, text_scale_y, quads, area);
    }
  }
  /* Lines for maximum values. */
  const float max_val_r = ScopeHistogram::bin_to_float(hist.max_bin.x);
  const float max_val_g = ScopeHistogram::bin_to_float(hist.max_bin.y);
  const float max_val_b = ScopeHistogram::bin_to_float(hist.max_bin.z);
  add_vertical_line(max_val_r, {128, 0, 0, 128}, v2d, text_scale_x, text_scale_y, quads, area);
  add_vertical_line(max_val_g, {0, 128, 0, 128}, v2d, text_scale_x, text_scale_y, quads, area);
  add_vertical_line(max_val_b, {0, 0, 128, 128}, v2d, text_scale_x, text_scale_y, quads, area);

  /* Horizontal lines. */
  const float x_val_min = area.xmin;
  const float x_val_max = area.xmin + (area.xmax - area.xmin) * max_val;
  quads.add_line(x_val_min, area.ymin, x_val_max, area.ymin, col_grid);
  quads.add_line(x_val_min, area.ymax, x_val_max, area.ymax, col_grid);

  /* Histogram area for each R/G/B channels, additively blended. */
  quads.draw();
  GPU_blend(GPU_BLEND_ADDITIVE);
  for (int ch = 0; ch < 3; ++ch) {
    if (hist.max_value[ch] == 0) {
      continue;
    }
    uchar col_line[4] = {32, 32, 32, 255};
    uchar col_area[4] = {64, 64, 64, 128};
    col_line[ch] = 224;
    col_area[ch] = 224;
    float y_scale = (area.ymax - area.ymin) / hist.max_value[ch] * 0.95f;
    float x_scale = (area.xmax - area.xmin);
    float yb = area.ymin;
    for (int bin = 0; bin <= hist.max_bin[ch]; bin++) {
      uint bin_val = hist.data[bin][ch];
      if (bin_val == 0) {
        continue;
      }
      float f0 = ScopeHistogram::bin_to_float(bin);
      float f1 = ScopeHistogram::bin_to_float(bin + 1);
      float x0 = area.xmin + f0 * x_scale;
      float x1 = area.xmin + f1 * x_scale;

      float y = area.ymin + bin_val * y_scale;
      quads.add_quad(x0, yb, x0, y, x1, yb, x1, y, col_area);
      quads.add_line(x0, y, x1, y, col_line);
    }
  }
  quads.draw();
  GPU_blend(GPU_BLEND_ALPHA);

  UI_view2d_text_cache_draw(&region);
}

static float2 rgb_to_uv_scaled(const float3 &rgb)
{
  float y, u, v;
  rgb_to_yuv(rgb.x, rgb.y, rgb.z, &y, &u, &v, BLI_YUV_ITU_BT709);
  /* Scale to +-0.5 range. */
  u *= SeqScopes::VECSCOPE_U_SCALE;
  v *= SeqScopes::VECSCOPE_V_SCALE;
  return float2(u, v);
}

static void draw_waveform_graticule(ARegion *region, SeqQuadsBatch &quads, const rctf &area)
{
  /* Horizontal lines at 10%, 70%, 90%. */
  const float lines[3] = {0.1f, 0.7f, 0.9f};
  uchar col_grid[4] = {160, 64, 64, 128};
  const float x0 = area.xmin;
  const float x1 = area.xmax;

  for (int i = 0; i < 3; i++) {
    const float y = area.ymin + (area.ymax - area.ymin) * lines[i];
    char buf[10];
    const size_t buf_len = SNPRINTF_UTF8_RLEN(buf, "%.1f", lines[i]);
    quads.add_line(x0, y, x1, y, col_grid);
    UI_view2d_text_cache_add(&region->v2d, x0 + 8, y + 8, buf, buf_len, col_grid);
  }
  /* Border. */
  uchar col_border[4] = {64, 64, 64, 128};
  quads.add_wire_quad(x0, area.ymin, x1, area.ymax, col_border);

  quads.draw();
  UI_view2d_text_cache_draw(region);
}

static void draw_vectorscope_graticule(ARegion *region, SeqQuadsBatch &quads, const rctf &area)
{
  const float skin_rad = DEG2RADF(123.0f); /* angle in radians of the skin tone line */

  const float w = BLI_rctf_size_x(&area);
  const float h = BLI_rctf_size_y(&area);
  const float2 center{BLI_rctf_cent_x(&area), BLI_rctf_cent_y(&area)};
  const float radius = ((w < h) ? w : h) * 0.5f;

  /* Precalculate circle points/colors. */
  constexpr int circle_delta = 6;
  constexpr int num_circle_points = 360 / circle_delta;
  float2 circle_pos[num_circle_points];
  float3 circle_col[num_circle_points];
  for (int i = 0; i < num_circle_points; i++) {
    float a = DEG2RADF(i * circle_delta);
    float x = cosf(a);
    float y = sinf(a);
    circle_pos[i] = float2(x, y);
    float u = x / SeqScopes::VECSCOPE_U_SCALE;
    float v = y / SeqScopes::VECSCOPE_V_SCALE;

    float3 col;
    yuv_to_rgb(0.5f, u, v, &col.x, &col.y, &col.z, BLI_YUV_ITU_BT709);
    circle_col[i] = col;
  }

  /* Draw colored background and outer ring, additively blended
   * since vectorscope image is already drawn. */
  GPU_blend(GPU_BLEND_ADDITIVE);

  constexpr float alpha_f = 0.8f;
  constexpr uchar alpha_b = uchar(alpha_f * 255.0f);
  const uchar4 col_center(50, 50, 50, alpha_b);

  uchar4 col1(0, 0, 0, alpha_b);
  uchar4 col2(0, 0, 0, alpha_b);
  uchar4 col3(0, 0, 0, alpha_b);

  /* Background: since the quads batch utility draws quads, draw two
   * segments of the circle (two triangles) in one iteration. */
  constexpr float mul_background = 0.2f;
  for (int i = 0; i < num_circle_points; i += 2) {
    int idx1 = i;
    int idx2 = (i + 1) % num_circle_points;
    int idx3 = (i + 2) % num_circle_points;
    float2 pt1 = center + circle_pos[idx1] * radius;
    float2 pt2 = center + circle_pos[idx2] * radius;
    float2 pt3 = center + circle_pos[idx3] * radius;
    float3 rgb1 = circle_col[idx1] * mul_background;
    float3 rgb2 = circle_col[idx2] * mul_background;
    float3 rgb3 = circle_col[idx3] * mul_background;
    rgb_float_to_uchar(col1, rgb1);
    rgb_float_to_uchar(col2, rgb2);
    rgb_float_to_uchar(col3, rgb3);
    quads.add_quad(pt1.x,
                   pt1.y,
                   pt2.x,
                   pt2.y,
                   center.x,
                   center.y,
                   pt3.x,
                   pt3.y,
                   col1,
                   col2,
                   col_center,
                   col3);
  }

  /* Outer ring. */
  const float outer_radius = radius * 1.02f;
  for (int i = 0; i < num_circle_points; i++) {
    int idx1 = i;
    int idx2 = (i + 1) % num_circle_points;
    float2 pt1a = center + circle_pos[idx1] * radius;
    float2 pt2a = center + circle_pos[idx2] * radius;
    float2 pt1b = center + circle_pos[idx1] * outer_radius;
    float2 pt2b = center + circle_pos[idx2] * outer_radius;
    float3 rgb1 = circle_col[idx1];
    float3 rgb2 = circle_col[idx2];
    rgb_float_to_uchar(col1, rgb1);
    rgb_float_to_uchar(col2, rgb2);
    quads.add_quad(
        pt1a.x, pt1a.y, pt1b.x, pt1b.y, pt2a.x, pt2a.y, pt2b.x, pt2b.y, col1, col1, col2, col2);
  }

  quads.draw();

  /* Draw grid and other labels using regular alpha blending. */
  GPU_blend(GPU_BLEND_ALPHA);
  const uchar4 col_grid(128, 128, 128, 128);

  /* Cross. */
  quads.add_line(center.x - radius, center.y, center.x + radius, center.y, col_grid);
  quads.add_line(center.x, center.y - radius, center.x, center.y + radius, col_grid);

  /* Inner circles. */
  for (int j = 1; j < 5; j++) {
    float r = radius * j * 0.2f;
    for (int i = 0; i < num_circle_points; i++) {
      int idx1 = i;
      int idx2 = (i + 1) % num_circle_points;
      float2 pt1 = center + circle_pos[idx1] * r;
      float2 pt2 = center + circle_pos[idx2] * r;
      quads.add_line(pt1.x, pt1.y, pt2.x, pt2.y, col_grid);
    }
  }

  /* "Safe" (0.75 saturation) primary color locations and labels. */
  const float3 primaries[6] = {
      {1, 0, 0},
      {1, 1, 0},
      {0, 1, 0},
      {0, 1, 1},
      {0, 0, 1},
      {1, 0, 1},
  };
  const char *names = "RYGCBM";

  /* Calculate size of single text letter. */
  char buf[2] = {'M', 0};
  float text_scale_x, text_scale_y;
  UI_view2d_scale_get_inverse(&region->v2d, &text_scale_x, &text_scale_y);
  float text_width, text_height;
  BLF_width_and_height(BLF_default(), buf, 1, &text_width, &text_height);
  text_width *= text_scale_x;
  text_height *= text_scale_y;

  const uchar4 col_target(128, 128, 128, 192);
  const float delta = radius * 0.01f;
  for (int i = 0; i < 6; i++) {
    float3 safe = primaries[i] * 0.75f;
    float2 pos = center + rgb_to_uv_scaled(safe) * (radius * 2);
    quads.add_wire_quad(pos.x - delta, pos.y - delta, pos.x + delta, pos.y + delta, col_target);

    buf[0] = names[i];
    UI_view2d_text_cache_add(&region->v2d,
                             pos.x + delta * 1.2f + text_width / 4,
                             pos.y - text_height / 2,
                             buf,
                             1,
                             col_target);
  }

  /* Skin tone line. */
  const uchar4 col_tone(255, 102, 0, 128);
  quads.add_line(center.x,
                 center.y,
                 center.x + cosf(skin_rad) * radius,
                 center.y + sinf(skin_rad) * radius,
                 col_tone);

  quads.draw();
  UI_view2d_text_cache_draw(region);
}

static const char *get_scope_debug_name(eSpaceSeq_RegionType type)
{
  switch (type) {
    case SEQ_DRAW_IMG_VECTORSCOPE:
      return "VSE Vectorscope";
    case SEQ_DRAW_IMG_WAVEFORM:
      return "VSE Waveform";
    case SEQ_DRAW_IMG_RGBPARADE:
      return "VSE Parade";
    case SEQ_DRAW_IMG_HISTOGRAM:
      return "VSE Histogram";
    case SEQ_DRAW_IMG_IMBUF:
      return "VSE Overexposed";
    default:
      return "VSE Scope";
  }
}

static void sequencer_draw_scopes(Scene *scene,
                                  const SpaceSeq &space_sequencer,
                                  ARegion &region,
                                  int timeline_frame,
                                  int image_width,
                                  int image_height,
                                  bool premultiplied)
{
  GPU_debug_group_begin(get_scope_debug_name(eSpaceSeq_RegionType(space_sequencer.mainb)));

  gpu::Texture *input_texture = seq::preview_cache_get_gpu_display_texture(
      scene, timeline_frame, 0);
  if (input_texture == nullptr) {
    input_texture = seq::preview_cache_get_gpu_texture(
        scene, timeline_frame, space_sequencer.chanshown);
  }

  SeqQuadsBatch quads;
  const SeqScopes *scopes = &space_sequencer.runtime->scopes;

  bool use_blend = (space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF &&
                    space_sequencer.flag & SEQ_USE_ALPHA) ||
                   (space_sequencer.mainb != SEQ_DRAW_IMG_IMBUF);

  const rctf preview = preview_get_full_position(region);

  /* Draw black rectangle over scopes area. */
  if (space_sequencer.mainb != SEQ_DRAW_IMG_IMBUF) {
    GPU_blend(GPU_BLEND_NONE);
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
    uchar black[4] = {0, 0, 0, 255};
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4ubv(black);
    immRectf(pos, preview.xmin, preview.ymin, preview.xmax, preview.ymax);
    immUnbindProgram();
  }

  if (use_blend) {
    GPU_blend(GPU_BLEND_ALPHA);
  }

  if (input_texture) {
    if (space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF) {
      /* Draw overexposed overlay. */
      GPU_blend(GPU_BLEND_NONE);
      GPUVertFormat *imm_format = immVertexFormat();
      const uint pos = GPU_vertformat_attr_add(imm_format, "pos", gpu::VertAttrType::SFLOAT_32_32);
      const uint tex_coord = GPU_vertformat_attr_add(
          imm_format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);

      immBindBuiltinProgram(GPU_SHADER_SEQUENCER_ZEBRA);
      immUniform1i("img_premultiplied", premultiplied ? 1 : 0);
      immUniform1f("zebra_limit", space_sequencer.zebra / 100.0f);
      immUniformColor3f(1.0f, 1.0f, 1.0f);

      GPU_texture_bind(input_texture, 0);
      rctf uv;
      BLI_rctf_init(&uv, 0.0f, 1.0f, 0.0f, 1.0f);
      immRectf_with_texco(pos, tex_coord, preview, uv);
      GPU_texture_unbind(input_texture);
      immUnbindProgram();
    }
    else if (space_sequencer.mainb != SEQ_DRAW_IMG_HISTOGRAM) {
      /* Draw point-based scopes using a compute shader based rasterizer (using
       * regular GPU pipeline to draw many points, where thousands of them can
       * hit the same pixels, is very inefficient, especially on tile-based GPUs).
       *
       * Compute shader rasterizer does atomic adds of fixed point colors into
       * a screen size buffer, then a fragment shader resolve pass outputs the
       * final colors. */
      const float point_size = (BLI_rcti_size_x(&region.v2d.mask) + 1) /
                               BLI_rctf_size_x(&region.v2d.cur);
      float3 coeffs;
      IMB_colormanagement_get_luminance_coefficients(coeffs);

      int viewport_size_i[4];
      GPU_viewport_size_get_i(viewport_size_i);
      const int2 viewport_size = int2(viewport_size_i[2], viewport_size_i[3]);
      const int2 image_size = int2(image_width, image_height);
      gpu::StorageBuf *raster_ssbo = GPU_storagebuf_create_ex(viewport_size.x * viewport_size.y *
                                                                  sizeof(SeqScopeRasterData),
                                                              nullptr,
                                                              GPU_USAGE_DEVICE_ONLY,
                                                              "Scopes Raster");
      GPU_storagebuf_clear_to_zero(raster_ssbo);
      /* Compute shader rasterization. */
      {
        gpu::Shader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_SEQUENCER_SCOPE_RASTER);
        BLI_assert(shader);
        GPU_shader_bind(shader);

        const int raster_ssbo_location = GPU_shader_get_ssbo_binding(shader, "raster_buf");
        GPU_storagebuf_bind(raster_ssbo, raster_ssbo_location);
        const int image_location = GPU_shader_get_sampler_binding(shader, "image");
        GPU_texture_bind(input_texture, image_location);

        GPU_shader_uniform_1i(shader, "view_width", viewport_size.x);
        GPU_shader_uniform_1i(shader, "view_height", viewport_size.y);
        GPU_shader_uniform_3fv(shader, "luma_coeffs", coeffs);
        GPU_shader_uniform_1f(shader, "scope_point_size", point_size);
        GPU_shader_uniform_1b(shader, "img_premultiplied", premultiplied);
        GPU_shader_uniform_1i(shader, "image_width", image_width);
        GPU_shader_uniform_1i(shader, "image_height", image_height);
        GPU_shader_uniform_1i(shader, "scope_mode", space_sequencer.mainb);

        const int2 groups_to_dispatch = math::divide_ceil(image_size, int2(16));
        GPU_compute_dispatch(shader, groups_to_dispatch.x, groups_to_dispatch.y, 1);

        GPU_shader_unbind();
        GPU_storagebuf_unbind(raster_ssbo);
        /* Make computed results consistently visible in the following resolve pass. */
        GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
      }

      /* Resolve pass. */
      {
        if (use_blend) {
          GPU_blend(GPU_BLEND_ALPHA);
        }

        /* Depending on resolution of the image, different amounts of pixels are expected
         * to hit the same locations of the scope. Adjust the scope transparency mapping
         * exponent so that the scope has decent visibility without saturating or being too dark:
         * 0.07 at height=2160 (4K) and up, 0.5 at height=360 and below, and interpolating between
         * those. */
        float alpha = math::clamp(ratiof(360.0f, 2160.0f, image_height), 0.0f, 1.0f);
        float exponent = math::interpolate(0.5f, 0.07f, alpha);

        gpu::Shader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_SEQUENCER_SCOPE_RESOLVE);
        BLI_assert(shader);

        const int raster_ssbo_location = GPU_shader_get_ssbo_binding(shader, "raster_buf");
        GPU_storagebuf_bind(raster_ssbo, raster_ssbo_location);

        gpu::Batch *batch = GPU_batch_create_procedural(GPU_PRIM_TRIS, 3);

        GPU_batch_set_shader(batch, shader);
        GPU_batch_uniform_1i(batch, "view_width", viewport_size.x);
        GPU_batch_uniform_1i(batch, "view_height", viewport_size.y);
        GPU_batch_uniform_1f(batch, "alpha_exponent", exponent);
        GPU_batch_draw(batch);

        GPU_batch_discard(batch);
        GPU_storagebuf_unbind(raster_ssbo);
      }

      GPU_storagebuf_free(raster_ssbo);
    }
  }

  /* Draw scope graticules. */
  if (use_blend) {
    GPU_blend(GPU_BLEND_ALPHA);
  }

  if (space_sequencer.mainb == SEQ_DRAW_IMG_HISTOGRAM) {
    draw_histogram(region, scopes->histogram, quads, preview);
  }
  if (ELEM(space_sequencer.mainb, SEQ_DRAW_IMG_WAVEFORM, SEQ_DRAW_IMG_RGBPARADE)) {
    use_blend = true;
    draw_waveform_graticule(&region, quads, preview);
  }
  if (space_sequencer.mainb == SEQ_DRAW_IMG_VECTORSCOPE) {
    use_blend = true;
    draw_vectorscope_graticule(&region, quads, preview);
  }

  quads.draw();

  if (use_blend) {
    GPU_blend(GPU_BLEND_NONE);
  }
  GPU_debug_group_end();
}

static void update_gpu_scopes(const ImBuf *input_ibuf,
                              gpu::Texture *input_texture,
                              const ColorManagedViewSettings &view_settings,
                              const ColorManagedDisplaySettings &display_settings,
                              const SpaceSeq &space_sequencer,
                              Scene *scene,
                              int timeline_frame)
{
  BLI_assert(input_ibuf && input_texture);

  /* No need for GPU texture transformed to display space: can use input texture as-is. */
  if (!IMB_colormanagement_display_processor_needed(input_ibuf, &view_settings, &display_settings))
  {
    return;
  }

  /* Display space GPU texture is already calculated. */
  gpu::Texture *display_texture = seq::preview_cache_get_gpu_display_texture(
      scene, timeline_frame, space_sequencer.chanshown);
  if (display_texture != nullptr) {
    return;
  }

  /* Create GPU texture. */
  const int width = GPU_texture_width(input_texture);
  const int height = GPU_texture_height(input_texture);
  const eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
  const gpu::TextureFormat format = gpu::TextureFormat::SFLOAT_16_16_16_16;
  display_texture = GPU_texture_create_2d(
      "seq_scope_display_buf", width, height, 1, format, usage, nullptr);
  if (display_texture == nullptr) {
    return;
  }
  GPU_texture_filter_mode(display_texture, false);

  GPU_matrix_push();
  GPU_matrix_push_projection();
  GPU_matrix_ortho_set(0.0f, 1.0f, 0.0f, 1.0f, -1.0, 1.0f);
  GPU_matrix_identity_set();

  gpu::FrameBuffer *fb = nullptr;
  GPU_framebuffer_ensure_config(&fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(display_texture)});
  GPU_framebuffer_bind(fb);

  GPUVertFormat *imm_format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(imm_format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  const uint tex_coord = GPU_vertformat_attr_add(
      imm_format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);

  const ColorSpace *input_colorspace = input_ibuf->float_buffer.data ?
                                           input_ibuf->float_buffer.colorspace :
                                           input_ibuf->byte_buffer.colorspace;
  const bool predivide = input_ibuf->float_buffer.data != nullptr;
  if (IMB_colormanagement_setup_glsl_draw_from_space(
          &view_settings, &display_settings, input_colorspace, 0.0f, predivide, false))
  {
    GPU_texture_bind(input_texture, 0);
    const rctf position{0.0f, 1.0f, 0.0f, 1.0f};
    const rctf texture_coord{0.0f, 1.0f, 0.0f, 1.0f};
    immRectf_with_texco(pos, tex_coord, position, texture_coord);
    GPU_texture_unbind(input_texture);
    IMB_colormanagement_finish_glsl_draw();
  }

  GPU_FRAMEBUFFER_FREE_SAFE(fb);

  GPU_matrix_pop();
  GPU_matrix_pop_projection();

  seq::preview_cache_set_gpu_display_texture(
      scene, timeline_frame, space_sequencer.chanshown, display_texture);
}

static void update_cpu_scopes(const SpaceSeq &space_sequencer,
                              const ColorManagedViewSettings &view_settings,
                              const ColorManagedDisplaySettings &display_settings,
                              const ImBuf &ibuf,
                              const int timeline_frame)

{
  SeqScopes &scopes = space_sequencer.runtime->scopes;
  if (scopes.last_ibuf == &ibuf && scopes.last_timeline_frame == timeline_frame) {
    /* Nothing to do: scopes already calculated for this image/frame. */
    return;
  }

  scopes.cleanup();
  if (space_sequencer.mainb == SEQ_DRAW_IMG_HISTOGRAM) {
    scopes.histogram.calc_from_ibuf(&ibuf, view_settings, display_settings);
  }
  scopes.last_ibuf = &ibuf;
  scopes.last_timeline_frame = timeline_frame;
}

static bool sequencer_draw_get_transform_preview(const SpaceSeq &sseq, const Scene &scene)
{
  Strip *last_seq = seq::select_active_get(&scene);
  if (last_seq == nullptr) {
    return false;
  }

  return (G.moving & G_TRANSFORM_SEQ) && (last_seq->flag & SELECT) &&
         ((last_seq->flag & SEQ_LEFTSEL) || (last_seq->flag & SEQ_RIGHTSEL)) &&
         (sseq.draw_flag & SEQ_DRAW_TRANSFORM_PREVIEW);
}

static int sequencer_draw_get_transform_preview_frame(const Scene *scene)
{
  Strip *last_seq = seq::select_active_get(scene);
  /* #sequencer_draw_get_transform_preview must already have been called. */
  BLI_assert(last_seq != nullptr);
  int preview_frame;

  if (last_seq->flag & SEQ_RIGHTSEL) {
    preview_frame = seq::time_right_handle_frame_get(scene, last_seq) - 1;
  }
  else {
    preview_frame = seq::time_left_handle_frame_get(scene, last_seq);
  }

  return preview_frame;
}

static void strip_draw_image_origin_and_outline(const bContext *C,
                                                Strip *strip,
                                                bool is_active_seq)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  const ARegion *region = CTX_wm_region(C);
  if (region->regiontype == RGN_TYPE_PREVIEW && !sequencer_view_preview_only_poll(C)) {
    return;
  }
  if ((strip->flag & SELECT) == 0) {
    return;
  }
  if (ED_screen_animation_no_scrub(CTX_wm_manager(C))) {
    return;
  }
  if ((sseq->flag & SEQ_SHOW_OVERLAY) == 0 ||
      (sseq->preview_overlay.flag & SEQ_PREVIEW_SHOW_OUTLINE_SELECTED) == 0)
  {
    return;
  }
  if (ELEM(sseq->mainb,
           SEQ_DRAW_IMG_WAVEFORM,
           SEQ_DRAW_IMG_RGBPARADE,
           SEQ_DRAW_IMG_VECTORSCOPE,
           SEQ_DRAW_IMG_HISTOGRAM))
  {
    return;
  }

  const float2 origin = seq::image_transform_origin_offset_pixelspace_get(
      CTX_data_sequencer_scene(C), strip);

  /* Origin. */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);
  immUniform1f("outlineWidth", 1.5f);
  immUniformColor3f(1.0f, 1.0f, 1.0f);
  immUniform4f("outlineColor", 0.0f, 0.0f, 0.0f, 1.0f);
  immUniform1f("size", 15.0f * U.pixelsize);
  immBegin(GPU_PRIM_POINTS, 1);
  immVertex2f(pos, origin[0], origin[1]);
  immEnd();
  immUnbindProgram();

  /* Outline. */
  const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(
      CTX_data_sequencer_scene(C), strip);

  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_width(2);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  float col[3];
  if (is_active_seq) {
    UI_GetThemeColor3fv(TH_SEQ_ACTIVE, col);
  }
  else {
    UI_GetThemeColor3fv(TH_SEQ_SELECTED, col);
  }
  immUniformColor3fv(col);
  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos, strip_image_quad[0].x, strip_image_quad[0].y);
  immVertex2f(pos, strip_image_quad[1].x, strip_image_quad[1].y);
  immVertex2f(pos, strip_image_quad[2].x, strip_image_quad[2].y);
  immVertex2f(pos, strip_image_quad[3].x, strip_image_quad[3].y);
  immEnd();
  immUnbindProgram();
  GPU_line_width(1);
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

static void text_selection_draw(const bContext *C, const Strip *strip, uint pos)
{
  const TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const TextVarsRuntime *text = data->runtime;
  const Scene *scene = CTX_data_sequencer_scene(C);

  if (data->selection_start_offset == -1 || strip_text_selection_range_get(data).is_empty()) {
    return;
  }

  const IndexRange sel_range = strip_text_selection_range_get(data);
  const int2 selection_start = strip_text_cursor_offset_to_position(text, sel_range.first());
  const int2 selection_end = strip_text_cursor_offset_to_position(text, sel_range.last());
  const int line_start = selection_start.y;
  const int line_end = selection_end.y;

  for (int line_index = line_start; line_index <= line_end; line_index++) {
    const seq::LineInfo line = text->lines[line_index];
    seq::CharInfo character_start = line.characters.first();
    seq::CharInfo character_end = line.characters.last();

    if (line_index == selection_start.y) {
      character_start = line.characters[selection_start.x];
    }
    if (line_index == selection_end.y) {
      character_end = line.characters[selection_end.x];
    }

    const float line_y = character_start.position.y + text->font_descender;

    const float2 view_offs{-scene->r.xsch / 2.0f, -scene->r.ysch / 2.0f};
    const float view_aspect = scene->r.xasp / scene->r.yasp;
    float3x3 transform_mat = seq::image_transform_matrix_get(scene, strip);
    float2 selection_quad[4] = {
        {character_start.position.x, line_y},
        {character_start.position.x, line_y + text->line_height},
        {character_end.position.x + character_end.advance_x, line_y + text->line_height},
        {character_end.position.x + character_end.advance_x, line_y},
    };

    immBegin(GPU_PRIM_TRIS, 6);
    immUniformThemeColor(TH_SEQ_SELECTED_TEXT);

    for (int i : IndexRange(0, 4)) {
      selection_quad[i] += view_offs;
      selection_quad[i] = math::transform_point(transform_mat, selection_quad[i]);
      selection_quad[i].x *= view_aspect;
    }
    for (int i : {0, 1, 2, 2, 3, 0}) {
      immVertex2f(pos, selection_quad[i][0], selection_quad[i][1]);
    }

    immEnd();
  }
}

static float2 coords_region_view_align(const View2D *v2d, const float2 coords)
{
  int2 coords_view;
  UI_view2d_view_to_region(v2d, coords.x, coords.y, &coords_view.x, &coords_view.y);
  coords_view.x = std::round(coords_view.x);
  coords_view.y = std::round(coords_view.y);
  float2 coords_region_aligned;
  UI_view2d_region_to_view(
      v2d, coords_view.x, coords_view.y, &coords_region_aligned.x, &coords_region_aligned.y);
  return coords_region_aligned;
}

static void text_edit_draw_cursor(const bContext *C, const Strip *strip, uint pos)
{
  const TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const TextVarsRuntime *text = data->runtime;
  const Scene *scene = CTX_data_sequencer_scene(C);

  const float2 view_offs{-scene->r.xsch / 2.0f, -scene->r.ysch / 2.0f};
  const float view_aspect = scene->r.xasp / scene->r.yasp;
  float3x3 transform_mat = seq::image_transform_matrix_get(scene, strip);
  const int2 cursor_position = strip_text_cursor_offset_to_position(text, data->cursor_offset);
  const float cursor_width = 10;
  float2 cursor_coords = text->lines[cursor_position.y].characters[cursor_position.x].position;
  /* Clamp cursor coords to be inside of text boundbox. Compensate for cursor width, but also line
   * width hardcoded in shader. */
  const float bound_left = float(text->text_boundbox.xmin) + U.pixelsize;
  const float bound_right = float(text->text_boundbox.xmax) - (cursor_width + U.pixelsize);
  /* Note: do not use std::clamp since due to math above left can become larger than right. */
  cursor_coords.x = std::max(cursor_coords.x, bound_left);
  cursor_coords.x = std::min(cursor_coords.x, bound_right);

  cursor_coords = coords_region_view_align(UI_view2d_fromcontext(C), cursor_coords);

  float2 cursor_quad[4] = {
      {cursor_coords.x, cursor_coords.y},
      {cursor_coords.x, cursor_coords.y + text->line_height},
      {cursor_coords.x + cursor_width, cursor_coords.y + text->line_height},
      {cursor_coords.x + cursor_width, cursor_coords.y},
  };
  const float2 descender_offs{0.0f, float(text->font_descender)};

  immBegin(GPU_PRIM_TRIS, 6);
  immUniformThemeColor(TH_SEQ_TEXT_CURSOR);

  for (int i : IndexRange(0, 4)) {
    cursor_quad[i] += descender_offs + view_offs;
    cursor_quad[i] = math::transform_point(transform_mat, cursor_quad[i]);
    cursor_quad[i].x *= view_aspect;
  }
  for (int i : {0, 1, 2, 2, 3, 0}) {
    immVertex2f(pos, cursor_quad[i][0], cursor_quad[i][1]);
  }

  immEnd();
}

static void text_edit_draw(const bContext *C)
{
  if (!sequencer_text_editing_active_poll(const_cast<bContext *>(C))) {
    return;
  }
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  if (!seq::effects_can_render_text(strip)) {
    return;
  }

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  text_selection_draw(C, strip, pos);
  text_edit_draw_cursor(C, strip, pos);

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

/* Draw empty preview region.
 * The entire region is cleared with the TH_SEQ_PREVIEW color.
 *
 * Used in cases when there is no editing, or when the display is set to NONE. */
static void sequencer_preview_draw_empty(ARegion &region)
{
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(&region);
  BLI_assert(viewport);

  gpu::FrameBuffer *overlay_fb = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(overlay_fb);

  sequencer_preview_clear();
}

/* Begin drawing the sequence preview region.
 * Initializes the drawing state which is common for color render and overlay drawing.
 *
 * #preview_draw_end() is to be called after drawing is done. */
static void preview_draw_begin(const bContext *C,
                               const RenderData &render_data,
                               const ColorManagedViewSettings &view_settings,
                               const ColorManagedDisplaySettings &display_settings,
                               ARegion &region,
                               eSpaceSeq_RegionType preview_type)
{
  sequencer_stop_running_jobs(C, CTX_data_sequencer_scene(C));

  GPUViewport *viewport = WM_draw_region_get_bound_viewport(&region);
  BLI_assert(viewport);

  /* Configure color space used by the viewport.
   * This also checks for HDR support and enables it for the viewport when found and needed. */
  GPU_viewport_colorspace_set(
      viewport, &view_settings, &display_settings, render_data.dither_intensity);

  GPU_depth_test(GPU_DEPTH_NONE);

  /* Setup view. */
  View2D &v2d = region.v2d;
  float viewrect[2];
  /* For histogram and wave/parade scopes, allow arbitrary zoom. */
  if (ELEM(preview_type, SEQ_DRAW_IMG_HISTOGRAM, SEQ_DRAW_IMG_WAVEFORM, SEQ_DRAW_IMG_RGBPARADE)) {
    v2d.keepzoom &= ~(V2D_KEEPASPECT | V2D_KEEPZOOM);
  }
  else {
    v2d.keepzoom |= V2D_KEEPASPECT | V2D_KEEPZOOM;
  }
  sequencer_display_size(render_data, viewrect);
  UI_view2d_totRect_set(&v2d, roundf(viewrect[0]), roundf(viewrect[1]));
  UI_view2d_curRect_validate(&v2d);
  UI_view2d_view_ortho(&v2d);
}

static void preview_draw_end(const bContext *C)
{
  UI_view2d_view_restore(C);
  seq_prefetch_wm_notify(C, CTX_data_sequencer_scene(C));
}

/* Configure current GPU state to draw on the color render frame-buffer of the viewport. */
static void preview_draw_color_render_begin(ARegion &region)
{
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(&region);
  BLI_assert(viewport);

  gpu::FrameBuffer *render_fb = GPU_viewport_framebuffer_render_get(viewport);
  GPU_framebuffer_bind(render_fb);

  float col[4] = {0, 0, 0, 0};
  GPU_framebuffer_clear_color(render_fb, col);
}

/* Configure current GPU state to draw on the overlay frame-buffer of the viewport. */
static void preview_draw_overlay_begin(ARegion &region)
{
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(&region);
  BLI_assert(viewport);

  gpu::FrameBuffer *overlay_fb = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(overlay_fb);

  sequencer_preview_clear();
}

/* Draw the given texture on the currently bound frame-buffer without any changes to its pixels
 * colors.
 *
 * The position denotes coordinates of a rectangle used to display the texture.
 * The texture_coord contains UV coordinates of the input texture which are mapped to the corners
 * of the rectangle. */
static void preview_draw_texture_simple(gpu::Texture &texture,
                                        const rctf &position,
                                        const rctf &texture_coord)
{
  GPUVertFormat *imm_format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(imm_format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  const uint tex_coord = GPU_vertformat_attr_add(
      imm_format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);
  immUniformColor3f(1.0f, 1.0f, 1.0f);

  GPU_texture_bind(&texture, 0);

  immRectf_with_texco(pos, tex_coord, position, texture_coord);

  GPU_texture_unbind(&texture);
  immUnbindProgram();
}

/* Draw the given texture on the currently bound frame-buffer and convert its colors to linear
 * space in the fragment shader. This makes it suitable to be further processed by a GPUViewport
 *
 * The position denotes coordinates of a rectangle used to display the texture.
 * The texture_coord contains UV coordinates of the input texture which are mapped to the corners
 * of the rectangle. */
static void preview_draw_texture_to_linear(gpu::Texture &texture,
                                           const char *texture_colorspace_name,
                                           const bool predivide,
                                           const rctf &position,
                                           const rctf &texture_coord)
{
  GPUVertFormat *imm_format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(imm_format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  const uint tex_coord = GPU_vertformat_attr_add(
      imm_format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);

  if (!IMB_colormanagement_setup_glsl_draw_to_scene_linear(texture_colorspace_name, predivide)) {
    /* An error happened when configuring GPU side color space conversion. Return and allow the
     * view to be black, so that it is obvious something went wrong and that a bug report is to
     * be submitted.
     *
     * Note that fallback OCIO implementation is handled on a higher level. */
    return;
  }

  GPU_texture_bind(&texture, 0);

  immRectf_with_texco(pos, tex_coord, position, texture_coord);

  GPU_texture_unbind(&texture);

  IMB_colormanagement_finish_glsl_draw();
}

/* Draw overlays for the currently displayed images in the preview. */
static void preview_draw_all_image_overlays(const bContext *C,
                                            const Scene *scene,
                                            const Editing &editing,
                                            const int timeline_frame)
{
  ListBase *channels = seq::channels_displayed_get(&editing);
  VectorSet strips = seq::query_rendered_strips(
      scene, channels, editing.current_strips(), timeline_frame, 0);
  Strip *active_seq = seq::select_active_get(scene);
  for (Strip *strip : strips) {
    /* TODO(sergey): Avoid having per-strip strip-independent checks. */
    strip_draw_image_origin_and_outline(C, strip, strip == active_seq);
    text_edit_draw(C);
  }
}

static bool is_cursor_visible(const SpaceSeq &sseq)
{
  if (G.moving & G_TRANSFORM_CURSOR) {
    return true;
  }

  if ((sseq.flag & SEQ_SHOW_OVERLAY) &&
      (sseq.preview_overlay.flag & SEQ_PREVIEW_SHOW_2D_CURSOR) != 0)
  {
    return true;
  }
  return false;
}

/**
 * We may want to move this into a more general location.
 */
static void draw_cursor_2d(const ARegion *region, const float2 &cursor)
{
  int co[2];
  UI_view2d_view_to_region(&region->v2d, cursor[0], cursor[1], &co[0], &co[1]);

  /* Draw nice Anti Aliased cursor. */
  GPU_blend(GPU_BLEND_ALPHA);

  /* Draw lines */
  float original_proj[4][4];
  GPU_matrix_projection_get(original_proj);
  GPU_matrix_push();
  ED_region_pixelspace(region);
  GPU_matrix_translate_2f(co[0] + 0.5f, co[1] + 0.5f);
  GPU_matrix_scale_2f(U.widget_unit, U.widget_unit);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);

  GPUVertFormat *format = immVertexFormat();
  struct {
    uint pos, col;
  } attr_id{};
  attr_id.pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  attr_id.col = GPU_vertformat_attr_add(format, "color", gpu::VertAttrType::SFLOAT_32_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_FLAT_COLOR);
  immUniform2fv("viewportSize", &viewport[2]);
  immUniform1f("lineWidth", U.pixelsize);

  const float f5 = 0.25f;
  const float f10 = 0.5f;
  const float f20 = 1.0f;

  const float red[3] = {1.0f, 0.0f, 0.0f};
  const float white[3] = {1.0f, 1.0f, 1.0f};

  const int segments = 16;
  immBegin(GPU_PRIM_LINE_STRIP, segments + 1);
  for (int i = 0; i < segments + 1; i++) {
    float angle = float(2 * M_PI) * (float(i) / float(segments));
    float x = f10 * cosf(angle);
    float y = f10 * sinf(angle);

    immAttr3fv(attr_id.col, (i % 2 == 0) ? red : white);
    immVertex2f(attr_id.pos, x, y);
  }
  immEnd();

  float crosshair_color[3];
  UI_GetThemeColor3fv(TH_VIEW_OVERLAY, crosshair_color);

  immBegin(GPU_PRIM_LINES, 8);
  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, -f20, 0);
  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, -f5, 0);

  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, +f20, 0);
  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, +f5, 0);

  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, 0, -f20);
  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, 0, -f5);

  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, 0, +f20);
  immAttr3fv(attr_id.col, crosshair_color);
  immVertex2f(attr_id.pos, 0, +f5);
  immEnd();

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);

  GPU_matrix_pop();
  GPU_matrix_projection_set(original_proj);
}

/* Get offset in frame numbers of the reference frame relative to the current frame. */
static int get_reference_frame_offset(const Editing &editing, const RenderData &render_data)
{
  if (editing.overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_ABS) {
    return editing.overlay_frame_abs - render_data.cfra;
  }
  return editing.overlay_frame_ofs;
}

/* Create Texture from the given image buffer for drawing rendered sequencer frame on
 * the color render frame buffer.
 *
 * The texture format and color space matches the CPU-side buffer.
 *
 * If both float and byte buffers are missing nullptr is returned.
 * If channel configuration is incompatible with the texture nullptr is returned. */
static gpu::Texture *create_texture(const ImBuf &ibuf)
{
  const eGPUTextureUsage texture_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                         GPU_TEXTURE_USAGE_ATTACHMENT;

  gpu::Texture *texture = nullptr;

  if (ibuf.float_buffer.data) {
    gpu::TextureFormat texture_format;
    switch (ibuf.channels) {
      case 1:
        texture_format = gpu::TextureFormat::SFLOAT_32;
        break;
      case 3:
        texture_format = gpu::TextureFormat::SFLOAT_32_32_32;
        break;
      case 4:
        texture_format = gpu::TextureFormat::SFLOAT_32_32_32_32;
        break;
      default:
        BLI_assert_msg(0, "Incompatible number of channels for float buffer in sequencer");
        return nullptr;
    }

    texture = GPU_texture_create_2d(
        "seq_display_buf", ibuf.x, ibuf.y, 1, texture_format, texture_usage, nullptr);
    if (texture) {
      GPU_texture_update(texture, GPU_DATA_FLOAT, ibuf.float_buffer.data);
    }
  }
  else if (ibuf.byte_buffer.data) {
    texture = GPU_texture_create_2d("seq_display_buf",
                                    ibuf.x,
                                    ibuf.y,
                                    1,
                                    gpu::TextureFormat::UNORM_8_8_8_8,
                                    texture_usage,
                                    nullptr);
    if (texture) {
      GPU_texture_update(texture, GPU_DATA_UBYTE, ibuf.byte_buffer.data);
    }
  }

  if (texture) {
    GPU_texture_filter_mode(texture, false);
  }

  return texture;
}

/* Get colorspace name of the image buffer used to create GPU texture.
 *
 * Needs to be kept in sync with create_texture() w.r.t which buffers are used to create the
 * texture. If the image buffer does not specify color space explicitly scene linear is returned if
 * there is a float buffer, and default byte space is returned if there is a byte buffer.
 *
 * If there are no buffers at all scene linear space is returned. */
static const char *get_texture_colorspace_name(const ImBuf &ibuf)
{
  if (ibuf.float_buffer.data) {
    if (ibuf.float_buffer.colorspace) {
      return IMB_colormanagement_colorspace_get_name(ibuf.float_buffer.colorspace);
    }
    return IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);
  }

  if (ibuf.byte_buffer.data) {
    if (ibuf.byte_buffer.colorspace) {
      return IMB_colormanagement_colorspace_get_name(ibuf.byte_buffer.colorspace);
    }
    return IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);
  }

  /* Fail-safe fallback. */
  return IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);
}

/* Part of the sequencer preview region drawing which renders images to the viewport's color render
 * frame-buffer. */
static void sequencer_preview_draw_color_render(const SpaceSeq &space_sequencer,
                                                const Editing &editing,
                                                ARegion &region,
                                                const ImBuf *current_ibuf,
                                                gpu::Texture *current_texture,
                                                const ImBuf *reference_ibuf,
                                                gpu::Texture *reference_texture)
{
  preview_draw_color_render_begin(region);

  if (current_texture) {
    BLI_assert(current_ibuf);
    const rctf position = preview_get_full_position(region);
    const rctf texture_coord = preview_get_full_texture_coord();
    const char *texture_colorspace = get_texture_colorspace_name(*current_ibuf);
    const bool predivide = (current_ibuf->float_buffer.data != nullptr);
    preview_draw_texture_to_linear(
        *current_texture, texture_colorspace, predivide, position, texture_coord);
  }

  if (reference_texture) {
    BLI_assert(reference_ibuf);
    const rctf position = preview_get_reference_position(space_sequencer, editing, region);
    const rctf texture_coord = preview_get_reference_texture_coord(space_sequencer, editing);
    const char *texture_colorspace = get_texture_colorspace_name(*reference_ibuf);
    const bool predivide = (reference_ibuf->float_buffer.data != nullptr);
    preview_draw_texture_to_linear(
        *reference_texture, texture_colorspace, predivide, position, texture_coord);
  }
}

static void draw_registered_callbacks(const bContext *C, ARegion &region)
{
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(&region);
  BLI_assert(viewport);

  gpu::FrameBuffer *overlay_fb = GPU_viewport_framebuffer_overlay_get(viewport);

  GPU_framebuffer_bind(overlay_fb);
  ED_region_draw_cb_draw(C, &region, REGION_DRAW_POST_VIEW);
  GPU_framebuffer_bind_no_srgb(overlay_fb);
}

static bool check_scope_needs_input_texture(const SpaceSeq &sseq)
{
  return (sseq.mainb != SEQ_DRAW_IMG_HISTOGRAM) &&
         ELEM(sseq.view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW);
}

/* Part of the sequencer preview region drawing which renders information overlays to the
 * viewport's overlay frame-buffer. */
static void sequencer_preview_draw_overlays(const bContext *C,
                                            const wmWindowManager &wm,
                                            Scene *scene,
                                            const SpaceSeq &space_sequencer,
                                            const Editing &editing,
                                            const ColorManagedViewSettings &view_settings,
                                            const ColorManagedDisplaySettings &display_settings,
                                            ARegion &region,
                                            gpu::Texture *current_texture,
                                            gpu::Texture *reference_texture,
                                            const ImBuf *input_ibuf,
                                            const int timeline_frame)
{
  const bool is_playing = ED_screen_animation_playing(&wm);
  const bool show_preview_image = space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF;
  const bool has_cpu_scope = input_ibuf && space_sequencer.mainb == SEQ_DRAW_IMG_HISTOGRAM;
  const bool has_gpu_scope = input_ibuf && current_texture &&
                             ((space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF &&
                               space_sequencer.zebra != 0) ||
                              ELEM(space_sequencer.mainb,
                                   SEQ_DRAW_IMG_WAVEFORM,
                                   SEQ_DRAW_IMG_RGBPARADE,
                                   SEQ_DRAW_IMG_VECTORSCOPE));

  /* Update scopes before starting regular draw (GPU scopes update changes framebuffer, etc.). */
  space_sequencer.runtime->scopes.last_ibuf_float = input_ibuf &&
                                                    input_ibuf->float_buffer.data != nullptr;
  if (has_cpu_scope) {
    update_cpu_scopes(
        space_sequencer, view_settings, display_settings, *input_ibuf, timeline_frame);
  }
  if (has_gpu_scope) {
    update_gpu_scopes(input_ibuf,
                      current_texture,
                      view_settings,
                      display_settings,
                      space_sequencer,
                      scene,
                      timeline_frame);
  }

  preview_draw_overlay_begin(region);

  if (has_cpu_scope || has_gpu_scope) {
    /* Draw scope. */
    sequencer_draw_scopes(scene,
                          space_sequencer,
                          region,
                          timeline_frame,
                          input_ibuf->x,
                          input_ibuf->y,
                          input_ibuf->float_buffer.data != nullptr);
  }
  else if (space_sequencer.flag & SEQ_USE_ALPHA) {
    /* Draw checked-board. */
    const View2D &v2d = region.v2d;
    imm_draw_box_checker_2d(v2d.tot.xmin, v2d.tot.ymin, v2d.tot.xmax, v2d.tot.ymax, true);

    /* Draw current and preview textures in a special way to pierce a hole in the overlay to make
     * the actual image visible. */
    GPU_blend(GPU_BLEND_OVERLAY_MASK_FROM_ALPHA);
    if (current_texture) {
      const rctf position = preview_get_full_position(region);
      const rctf texture_coord = preview_get_full_texture_coord();
      preview_draw_texture_simple(*current_texture, position, texture_coord);
    }
    if (reference_texture) {
      const rctf position = preview_get_reference_position(space_sequencer, editing, region);
      const rctf texture_coord = preview_get_reference_texture_coord(space_sequencer, editing);
      preview_draw_texture_simple(*reference_texture, position, texture_coord);
    }
    GPU_blend(GPU_BLEND_NONE);
  }
  else {
    /* The overlay framebuffer is fully cleared. Need to draw a full-frame transparent rectangle in
     * it to make sequencer result visible. */

    const rctf position = preview_get_full_position(region);

    GPUVertFormat *imm_format = immVertexFormat();
    const uint pos = GPU_vertformat_attr_add(imm_format, "pos", gpu::VertAttrType::SFLOAT_32_32);

    GPU_blend(GPU_BLEND_OVERLAY_MASK_FROM_ALPHA);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor3f(1.0f, 1.0f, 1.0f);
    immRectf(pos, position.xmin, position.ymin, position.xmax, position.ymax);
    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
  }

  /* Draw metadata. */
  if (show_preview_image && input_ibuf) {
    if ((space_sequencer.preview_overlay.flag & SEQ_PREVIEW_SHOW_METADATA) &&
        (space_sequencer.flag & SEQ_SHOW_OVERLAY))
    {
      const View2D &v2d = region.v2d;
      ED_region_image_metadata_draw(0.0, 0.0, input_ibuf, &v2d.tot, 1.0, 1.0);
    }
  }

  if (show_preview_image && (space_sequencer.flag & SEQ_SHOW_OVERLAY)) {
    sequencer_draw_borders_overlay(space_sequencer, region.v2d, scene);

    /* Various overlays like strip selection and text editing. */
    preview_draw_all_image_overlays(C, scene, editing, timeline_frame);

    if ((space_sequencer.preview_overlay.flag & SEQ_PREVIEW_SHOW_GPENCIL) && space_sequencer.gpd) {
      sequencer_draw_gpencil_overlay(C);
    }
  }

  draw_registered_callbacks(C, region);

  UI_view2d_view_restore(C);

  /* No need to show the cursor for scopes. */
  if ((is_playing == false) && show_preview_image && is_cursor_visible(space_sequencer)) {
    GPU_color_mask(true, true, true, true);
    GPU_depth_mask(false);
    GPU_depth_test(GPU_DEPTH_NONE);

    const float2 cursor_pixel = seq::image_preview_unit_to_px(scene, space_sequencer.cursor);
    draw_cursor_2d(&region, cursor_pixel);
  }

  /* Gizmos. */
  if ((is_playing == false) && (space_sequencer.gizmo_flag & SEQ_GIZMO_HIDE) == 0) {
    WM_gizmomap_draw(region.runtime->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);
  }

  /* FPS counter. */
  if ((U.uiflag & USER_SHOW_FPS) && ED_screen_animation_no_scrub(&wm)) {
    const rcti *rect = ED_region_visible_rect(&region);
    int xoffset = rect->xmin + U.widget_unit;
    int yoffset = rect->ymax;

    /* #ED_scene_draw_fps does not set text/shadow colors, except when frame-rate is too low, then
     * it sets text color to red. Make sure the "normal case" also has legible colors. */
    const int font_id = BLF_default();
    float text_color[4] = {1, 1, 1, 1}, shadow_color[4] = {0, 0, 0, 0.8f};
    BLF_color4fv(font_id, text_color);
    BLF_enable(font_id, BLF_SHADOW);
    BLF_shadow_offset(font_id, 0, 0);
    BLF_shadow(font_id, FontShadowType::Outline, shadow_color);

    ED_scene_draw_fps(scene, xoffset, &yoffset);

    BLF_disable(font_id, BLF_SHADOW);
  }
}

void sequencer_preview_region_draw(const bContext *C, ARegion *region)
{
  const ScrArea *area = CTX_wm_area(C);
  const SpaceSeq &space_sequencer = *static_cast<const SpaceSeq *>(area->spacedata.first);
  Scene *scene = CTX_data_sequencer_scene(C);

  /* Check if preview needs to be drawn at all. Note: do not draw preview region when
   * there is ongoing offline rendering, to avoid threading conflicts. */
  if (G.is_rendering || !scene || !scene->ed ||
      space_sequencer.render_size == SEQ_RENDER_SIZE_NONE)
  {
    sequencer_preview_draw_empty(*region);
    return;
  }

  const Editing &editing = *scene->ed;
  const RenderData &render_data = scene->r;

  preview_draw_begin(C,
                     render_data,
                     scene->view_settings,
                     scene->display_settings,
                     *region,
                     eSpaceSeq_RegionType(space_sequencer.mainb));

  const bool show_imbuf = check_show_imbuf(space_sequencer);
  const bool use_gpu_texture = show_imbuf || check_scope_needs_input_texture(space_sequencer);

  const bool draw_overlay = (space_sequencer.flag & SEQ_SHOW_OVERLAY);
  const bool draw_frame_overlay = (editing.overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_SHOW) &&
                                  draw_overlay;
  const bool need_current_frame = !(draw_frame_overlay && (space_sequencer.overlay_frame_type ==
                                                           SEQ_OVERLAY_FRAME_TYPE_REFERENCE));
  const bool need_reference_frame = show_imbuf && draw_frame_overlay &&
                                    space_sequencer.overlay_frame_type !=
                                        SEQ_OVERLAY_FRAME_TYPE_CURRENT;

  int timeline_frame = render_data.cfra;
  if (sequencer_draw_get_transform_preview(space_sequencer, *scene)) {
    timeline_frame = sequencer_draw_get_transform_preview_frame(scene);
  }

  /* GPU textures for the current and reference frames.
   *
   * When non-nullptr they are to be drawn (in other words, when they are non-nullptr the
   * corresponding draw_current_frame and draw_reference_frame is true). */
  gpu::Texture *current_texture = nullptr;
  gpu::Texture *reference_texture = nullptr;

  /* Get image buffers before setting up GPU state for drawing.  This is because
   * sequencer_ibuf_get() might not properly restore the state.
   * Additionally, some image buffers might be needed for both color render and overlay drawing. */
  ImBuf *current_ibuf = nullptr;
  ImBuf *reference_ibuf = nullptr;
  const char *view_names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
  if (need_reference_frame) {
    const int offset = get_reference_frame_offset(editing, render_data);
    reference_ibuf = sequencer_ibuf_get(
        C, timeline_frame + offset, view_names[space_sequencer.multiview_eye]);
    if (show_imbuf && reference_ibuf) {
      reference_texture = create_texture(*reference_ibuf);
    }
  }
  if (need_current_frame) {
    current_ibuf = sequencer_ibuf_get(
        C, timeline_frame, view_names[space_sequencer.multiview_eye]);
    if (use_gpu_texture && current_ibuf) {
      current_texture = seq::preview_cache_get_gpu_texture(
          scene, timeline_frame, space_sequencer.chanshown);
      if (current_texture == nullptr) {
        current_texture = create_texture(*current_ibuf);
        seq::preview_cache_set_gpu_texture(
            scene, timeline_frame, space_sequencer.chanshown, current_texture);
      }
    }
  }

  /* Image buffer used for overlays: scopes, metadata etc. */
  ImBuf *overlay_ibuf = need_current_frame ? current_ibuf : reference_ibuf;

  /* Draw parts of the preview region to the corresponding frame buffers. */
  sequencer_preview_draw_color_render(space_sequencer,
                                      editing,
                                      *region,
                                      current_ibuf,
                                      show_imbuf ? current_texture : nullptr,
                                      reference_ibuf,
                                      show_imbuf ? reference_texture : nullptr);
  sequencer_preview_draw_overlays(C,
                                  *CTX_wm_manager(C),
                                  scene,
                                  space_sequencer,
                                  editing,
                                  scene->view_settings,
                                  scene->display_settings,
                                  *region,
                                  current_texture,
                                  reference_texture,
                                  overlay_ibuf,
                                  timeline_frame);

#if 0
  sequencer_draw_maskedit(C, scene, region, sseq);
#endif

  /* Free GPU textures. Note that the #current_texture is kept around via #preview_set_gpu_texture,
   * for other preview areas or frames if nothing changes between them. */
  if (reference_texture) {
    GPU_texture_free(reference_texture);
  }

  /* Free CPU side resources. */
  IMB_freeImBuf(current_ibuf);
  IMB_freeImBuf(reference_ibuf);

  preview_draw_end(C);
}

}  // namespace blender::ed::vse
