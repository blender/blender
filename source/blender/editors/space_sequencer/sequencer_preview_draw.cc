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

#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_primitive.hh"
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
  int rectx, recty;
  double render_size;
  short is_break = G.is_break;

  if (sseq->render_size == SEQ_RENDER_SIZE_NONE) {
    return nullptr;
  }

  if (sseq->render_size == SEQ_RENDER_SIZE_SCENE) {
    render_size = scene->r.size / 100.0;
  }
  else {
    render_size = seq::rendersize_to_scale_factor(sseq->render_size);
  }

  rectx = roundf(render_size * scene->r.xsch);
  recty = roundf(render_size * scene->r.ysch);

  seq::render_new_render_data(
      bmain, depsgraph, scene, rectx, recty, sseq->render_size, false, &context);
  context.view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);
  context.use_proxies = (sseq->flag & SEQ_USE_PROXIES) != 0;
  context.is_playing = screen->animtimer != nullptr;
  context.is_scrubbing = screen->scrubbing;

  /* Sequencer could start rendering, in this case we need to be sure it wouldn't be
   * canceled by Escape pressed somewhere in the past. */
  G.is_break = false;

  GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);
  GPUFrameBuffer *fb = GPU_framebuffer_active_get();
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

static ImBuf *sequencer_make_scope(const ColorManagedViewSettings &view_settings,
                                   const ColorManagedDisplaySettings &display_settings,
                                   const ImBuf &ibuf,
                                   ImBuf *(*make_scope_fn)(const ImBuf *ibuf))
{
  ImBuf *display_ibuf = IMB_dupImBuf(&ibuf);
  ImBuf *scope;

  IMB_colormanagement_imbuf_make_display_space(display_ibuf, &view_settings, &display_settings);

  scope = make_scope_fn(display_ibuf);

  IMB_freeImBuf(display_ibuf);

  return scope;
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
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

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

static void draw_histogram(ARegion &region,
                           const ScopeHistogram &hist,
                           SeqQuadsBatch &quads,
                           const rctf &area)
{
  if (hist.data.is_empty()) {
    return;
  }

  /* Grid lines and labels. */
  uchar col_grid[4] = {128, 128, 128, 128};
  float grid_x_0 = area.xmin;
  float grid_x_1 = area.xmax;
  /* Float histograms show more than 0..1 range horizontally. */
  if (hist.is_float_hist()) {
    float ratio_0 = ratiof(ScopeHistogram::FLOAT_VAL_MIN, ScopeHistogram::FLOAT_VAL_MAX, 0.0f);
    float ratio_1 = ratiof(ScopeHistogram::FLOAT_VAL_MIN, ScopeHistogram::FLOAT_VAL_MAX, 1.0f);
    grid_x_0 = area.xmin + (area.xmax - area.xmin) * ratio_0;
    grid_x_1 = area.xmin + (area.xmax - area.xmin) * ratio_1;
  }

  View2D &v2d = region.v2d;
  float text_scale_x, text_scale_y;
  UI_view2d_scale_get_inverse(&v2d, &text_scale_x, &text_scale_y);

  for (int line = 0; line <= 4; line++) {
    float val = float(line) / 4;
    float x = grid_x_0 + (grid_x_1 - grid_x_0) * val;
    quads.add_line(x, area.ymin, x, area.ymax, col_grid);

    /* Label. */
    char buf[10];
    const size_t buf_len = SNPRINTF_UTF8_RLEN(buf, "%.2f", val);

    float text_width, text_height;
    BLF_width_and_height(BLF_default(), buf, buf_len, &text_width, &text_height);
    text_width *= text_scale_x;
    text_height *= text_scale_y;
    UI_view2d_text_cache_add(
        &v2d, x - text_width / 2, area.ymax - text_height * 1.3f, buf, buf_len, col_grid);
  }

  /* Border. */
  uchar col_border[4] = {64, 64, 64, 128};
  quads.add_wire_quad(area.xmin, area.ymin, area.xmax, area.ymax, col_border);

  /* Histogram area & line for each R/G/B channels, additively blended. */
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
    float x_scale = (area.xmax - area.xmin) / hist.data.size();
    float yb = area.ymin;
    for (int bin = 0; bin < hist.data.size() - 1; bin++) {
      float x0 = area.xmin + (bin + 0.5f) * x_scale;
      float x1 = area.xmin + (bin + 1.5f) * x_scale;

      float y0 = area.ymin + hist.data[bin][ch] * y_scale;
      float y1 = area.ymin + hist.data[bin + 1][ch] * y_scale;
      quads.add_quad(x0, yb, x0, y0, x1, yb, x1, y1, col_area);
      quads.add_line(x0, y0, x1, y1, col_line);
    }
  }
  quads.draw();
  GPU_blend(GPU_BLEND_ALPHA);

  UI_view2d_text_cache_draw(&region);
}

static blender::float2 rgb_to_uv_scaled(const blender::float3 &rgb)
{
  float y, u, v;
  rgb_to_yuv(rgb.x, rgb.y, rgb.z, &y, &u, &v, BLI_YUV_ITU_BT709);
  /* Scale to +-0.5 range. */
  u *= SeqScopes::VECSCOPE_U_SCALE;
  v *= SeqScopes::VECSCOPE_V_SCALE;
  return blender::float2(u, v);
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

static void sequencer_draw_scopes(const SpaceSeq &space_sequencer, ARegion &region)
{
  /* Figure out draw coordinates. */
  const rctf preview = preview_get_full_position(region);

  rctf uv;
  BLI_rctf_init(&uv, 0.0f, 1.0f, 0.0f, 1.0f);
  const bool keep_aspect = space_sequencer.mainb == SEQ_DRAW_IMG_VECTORSCOPE;
  float vecscope_aspect = 1.0f;
  if (keep_aspect) {
    float width = std::max(BLI_rctf_size_x(&preview), 0.1f);
    float height = std::max(BLI_rctf_size_y(&preview), 0.1f);
    vecscope_aspect = width / height;
    if (vecscope_aspect >= 1.0f) {
      BLI_rctf_resize_x(&uv, vecscope_aspect);
    }
    else {
      BLI_rctf_resize_y(&uv, 1.0f / vecscope_aspect);
    }
  }

  SeqQuadsBatch quads;
  const SeqScopes *scopes = &space_sequencer.runtime->scopes;

  bool use_blend = space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF &&
                   space_sequencer.flag & SEQ_USE_ALPHA;

  /* Draw black rectangle over scopes area. */
  if (space_sequencer.mainb != SEQ_DRAW_IMG_IMBUF) {
    GPU_blend(GPU_BLEND_NONE);
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    uchar black[4] = {0, 0, 0, 255};
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4ubv(black);
    immRectf(pos, preview.xmin, preview.ymin, preview.xmax, preview.ymax);
    immUnbindProgram();
  }

  /* Draw scope image if there is one. */
  ImBuf *scope_image = nullptr;
  if (space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF) {
    scope_image = scopes->zebra_ibuf;
  }
  else if (space_sequencer.mainb == SEQ_DRAW_IMG_WAVEFORM) {
    scope_image = scopes->waveform_ibuf;
  }
  else if (space_sequencer.mainb == SEQ_DRAW_IMG_VECTORSCOPE) {
    scope_image = scopes->vector_ibuf;
  }
  else if (space_sequencer.mainb == SEQ_DRAW_IMG_RGBPARADE) {
    scope_image = scopes->sep_waveform_ibuf;
  }

  if (use_blend) {
    GPU_blend(GPU_BLEND_ALPHA);
  }

  if (scope_image != nullptr) {
    if (scope_image->float_buffer.data && scope_image->byte_buffer.data == nullptr) {
      IMB_byte_from_float(scope_image);
    }

    blender::gpu::TextureFormat format = blender::gpu::TextureFormat::UNORM_8_8_8_8;
    eGPUDataFormat data = GPU_DATA_UBYTE;
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    blender::gpu::Texture *texture = GPU_texture_create_2d(
        "seq_display_buf", scope_image->x, scope_image->y, 1, format, usage, nullptr);
    GPU_texture_update(texture, data, scope_image->byte_buffer.data);
    GPU_texture_filter_mode(texture, false);
    GPU_texture_extend_mode(texture, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);

    GPU_texture_bind(texture, 0);

    GPUVertFormat *imm_format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(
        imm_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    uint texCoord = GPU_vertformat_attr_add(
        imm_format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);
    immUniformColor3f(1.0f, 1.0f, 1.0f);

    immBegin(GPU_PRIM_TRI_FAN, 4);

    immAttr2f(texCoord, uv.xmin, uv.ymin);
    immVertex2f(pos, preview.xmin, preview.ymin);

    immAttr2f(texCoord, uv.xmin, uv.ymax);
    immVertex2f(pos, preview.xmin, preview.ymax);

    immAttr2f(texCoord, uv.xmax, uv.ymax);
    immVertex2f(pos, preview.xmax, preview.ymax);

    immAttr2f(texCoord, uv.xmax, uv.ymin);
    immVertex2f(pos, preview.xmax, preview.ymin);

    immEnd();

    GPU_texture_unbind(texture);
    GPU_texture_free(texture);

    immUnbindProgram();
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
}

static bool sequencer_calc_scopes(const SpaceSeq &space_sequencer,
                                  const ColorManagedViewSettings &view_settings,
                                  const ColorManagedDisplaySettings &display_settings,
                                  const ImBuf &ibuf)

{
  if (space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF && space_sequencer.zebra == 0) {
    return false; /* Not drawing any scopes. */
  }

  SeqScopes *scopes = &space_sequencer.runtime->scopes;
  if (scopes->reference_ibuf != &ibuf) {
    scopes->cleanup();
  }

  switch (space_sequencer.mainb) {
    case SEQ_DRAW_IMG_IMBUF:
      if (!scopes->zebra_ibuf) {
        if (ibuf.float_buffer.data) {
          ImBuf *display_ibuf = IMB_dupImBuf(&ibuf);
          IMB_colormanagement_imbuf_make_display_space(
              display_ibuf, &view_settings, &display_settings);
          scopes->zebra_ibuf = make_zebra_view_from_ibuf(display_ibuf, space_sequencer.zebra);
          IMB_freeImBuf(display_ibuf);
        }
        else {
          scopes->zebra_ibuf = make_zebra_view_from_ibuf(&ibuf, space_sequencer.zebra);
        }
      }
      break;
    case SEQ_DRAW_IMG_WAVEFORM:
      if (!scopes->waveform_ibuf) {
        scopes->waveform_ibuf = sequencer_make_scope(
            view_settings, display_settings, ibuf, make_waveform_view_from_ibuf);
      }
      break;
    case SEQ_DRAW_IMG_VECTORSCOPE:
      if (!scopes->vector_ibuf) {
        scopes->vector_ibuf = sequencer_make_scope(
            view_settings, display_settings, ibuf, make_vectorscope_view_from_ibuf);
      }
      break;
    case SEQ_DRAW_IMG_HISTOGRAM: {
      scopes->histogram.calc_from_ibuf(&ibuf, view_settings, display_settings);
    } break;
    case SEQ_DRAW_IMG_RGBPARADE:
      if (!scopes->sep_waveform_ibuf) {
        scopes->sep_waveform_ibuf = sequencer_make_scope(
            view_settings, display_settings, ibuf, make_sep_waveform_view_from_ibuf);
      }
      break;
    default: /* Future files might have scopes we don't know about. */
      return false;
  }
  scopes->reference_ibuf = &ibuf;
  return true;
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

int sequencer_draw_get_transform_preview_frame(const Scene *scene)
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

  const blender::float2 origin = seq::image_transform_origin_offset_pixelspace_get(
      CTX_data_sequencer_scene(C), strip);

  /* Origin. */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
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
  const blender::Array<blender::float2> strip_image_quad = seq::image_transform_final_quad_get(
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

  const blender::IndexRange sel_range = strip_text_selection_range_get(data);
  const blender::int2 selection_start = strip_text_cursor_offset_to_position(text,
                                                                             sel_range.first());
  const blender::int2 selection_end = strip_text_cursor_offset_to_position(text, sel_range.last());
  const int line_start = selection_start.y;
  const int line_end = selection_end.y;

  for (int line_index = line_start; line_index <= line_end; line_index++) {
    const blender::seq::LineInfo line = text->lines[line_index];
    blender::seq::CharInfo character_start = line.characters.first();
    blender::seq::CharInfo character_end = line.characters.last();

    if (line_index == selection_start.y) {
      character_start = line.characters[selection_start.x];
    }
    if (line_index == selection_end.y) {
      character_end = line.characters[selection_end.x];
    }

    const float line_y = character_start.position.y + text->font_descender;

    const blender::float2 view_offs{-scene->r.xsch / 2.0f, -scene->r.ysch / 2.0f};
    const float view_aspect = scene->r.xasp / scene->r.yasp;
    blender::float3x3 transform_mat = seq::image_transform_matrix_get(scene, strip);
    blender::float4x2 selection_quad{
        {character_start.position.x, line_y},
        {character_start.position.x, line_y + text->line_height},
        {character_end.position.x + character_end.advance_x, line_y + text->line_height},
        {character_end.position.x + character_end.advance_x, line_y},
    };

    immBegin(GPU_PRIM_TRIS, 6);
    immUniformThemeColor(TH_SEQ_SELECTED_TEXT);

    for (int i : blender::IndexRange(0, 4)) {
      selection_quad[i] += view_offs;
      selection_quad[i] = blender::math::transform_point(transform_mat, selection_quad[i]);
      selection_quad[i].x *= view_aspect;
    }
    for (int i : blender::Vector<int>{0, 1, 2, 2, 3, 0}) {
      immVertex2f(pos, selection_quad[i][0], selection_quad[i][1]);
    }

    immEnd();
  }
}

static blender::float2 coords_region_view_align(const View2D *v2d, const blender::float2 coords)
{
  blender::int2 coords_view;
  UI_view2d_view_to_region(v2d, coords.x, coords.y, &coords_view.x, &coords_view.y);
  coords_view.x = std::round(coords_view.x);
  coords_view.y = std::round(coords_view.y);
  blender::float2 coords_region_aligned;
  UI_view2d_region_to_view(
      v2d, coords_view.x, coords_view.y, &coords_region_aligned.x, &coords_region_aligned.y);
  return coords_region_aligned;
}

static void text_edit_draw_cursor(const bContext *C, const Strip *strip, uint pos)
{
  const TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const TextVarsRuntime *text = data->runtime;
  const Scene *scene = CTX_data_sequencer_scene(C);

  const blender::float2 view_offs{-scene->r.xsch / 2.0f, -scene->r.ysch / 2.0f};
  const float view_aspect = scene->r.xasp / scene->r.yasp;
  blender::float3x3 transform_mat = seq::image_transform_matrix_get(scene, strip);
  const blender::int2 cursor_position = strip_text_cursor_offset_to_position(text,
                                                                             data->cursor_offset);
  const float cursor_width = 10;
  blender::float2 cursor_coords =
      text->lines[cursor_position.y].characters[cursor_position.x].position;
  /* Clamp cursor coords to be inside of text boundbox. Compensate for cursor width, but also line
   * width hardcoded in shader. */
  const float bound_left = float(text->text_boundbox.xmin) + U.pixelsize;
  const float bound_right = float(text->text_boundbox.xmax) - (cursor_width + U.pixelsize);
  /* Note: do not use std::clamp since due to math above left can become larger than right. */
  cursor_coords.x = std::max(cursor_coords.x, bound_left);
  cursor_coords.x = std::min(cursor_coords.x, bound_right);

  cursor_coords = coords_region_view_align(UI_view2d_fromcontext(C), cursor_coords);

  blender::float4x2 cursor_quad{
      {cursor_coords.x, cursor_coords.y},
      {cursor_coords.x, cursor_coords.y + text->line_height},
      {cursor_coords.x + cursor_width, cursor_coords.y + text->line_height},
      {cursor_coords.x + cursor_width, cursor_coords.y},
  };
  const blender::float2 descender_offs{0.0f, float(text->font_descender)};

  immBegin(GPU_PRIM_TRIS, 6);
  immUniformThemeColor(TH_SEQ_TEXT_CURSOR);

  for (int i : blender::IndexRange(0, 4)) {
    cursor_quad[i] += descender_offs + view_offs;
    cursor_quad[i] = blender::math::transform_point(transform_mat, cursor_quad[i]);
    cursor_quad[i].x *= view_aspect;
  }
  for (int i : blender::Vector<int>{0, 1, 2, 2, 3, 0}) {
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
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
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

  GPUFrameBuffer *overlay_fb = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(overlay_fb);

  sequencer_preview_clear();
}

/* Begin drawing the sequence preview region.
 * Initializes the drawing state which is common for color render and overlay drawing.
 *
 * Returns true if the region is to be drawn. Example is when it is not to be drawn is when there
 * is ongoing offline rendering (to avoid possible threading conflict).
 *
 * If the function returns true preview_draw_end() is to be called after drawing is done. */
static bool preview_draw_begin(const bContext *C,
                               const RenderData &render_data,
                               const ColorManagedViewSettings &view_settings,
                               const ColorManagedDisplaySettings &display_settings,
                               ARegion &region)
{
  sequencer_stop_running_jobs(C, CTX_data_sequencer_scene(C));
  if (G.is_rendering) {
    return false;
  }

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
  sequencer_display_size(render_data, viewrect);
  UI_view2d_totRect_set(&v2d, roundf(viewrect[0]), roundf(viewrect[1]));
  UI_view2d_curRect_validate(&v2d);
  UI_view2d_view_ortho(&v2d);

  return true;
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

  GPUFrameBuffer *render_fb = GPU_viewport_framebuffer_render_get(viewport);
  GPU_framebuffer_bind(render_fb);

  float col[4] = {0, 0, 0, 0};
  GPU_framebuffer_clear_color(render_fb, col);
}

/* Configure current GPU state to draw on the overlay frame-buffer of the viewport. */
static void preview_draw_overlay_begin(ARegion &region)
{
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(&region);
  BLI_assert(viewport);

  GPUFrameBuffer *overlay_fb = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(overlay_fb);

  sequencer_preview_clear();
}

/* Draw the given texture on the currently bound frame-buffer without any changes to its pixels
 * colors.
 *
 * The position denotes coordinates of a rectangle used to display the texture.
 * The texture_coord contains UV coordinates of the input texture which are mapped to the corners
 * of the rectangle. */
static void preview_draw_texture_simple(blender::gpu::Texture &texture,
                                        const rctf &position,
                                        const rctf &texture_coord)
{
  GPUVertFormat *imm_format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      imm_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  const uint tex_coord = GPU_vertformat_attr_add(
      imm_format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);

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
static void preview_draw_texture_to_linear(blender::gpu::Texture &texture,
                                           const char *texture_colorspace_name,
                                           const bool predivide,
                                           const rctf &position,
                                           const rctf &texture_coord)
{
  GPUVertFormat *imm_format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      imm_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  const uint tex_coord = GPU_vertformat_attr_add(
      imm_format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);

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
      scene, channels, editing.seqbasep, timeline_frame, 0);
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
static void draw_cursor_2d(const ARegion *region, const blender::float2 &cursor)
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
  attr_id.pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  attr_id.col = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32);
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

/* Create blender::gpu::Texture from the given image buffer for drawing rendered sequencer frame on
 * the color render frame buffer.
 *
 * The texture format and color space matches the CPU-side buffer.
 *
 * If both float and byte buffers are missing nullptr is returned.
 * If channel configuration is incompatible with the texture nullptr is returned. */
static blender::gpu::Texture *create_texture(const ImBuf &ibuf)
{
  const eGPUTextureUsage texture_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                         GPU_TEXTURE_USAGE_ATTACHMENT;

  blender::gpu::Texture *texture = nullptr;

  if (ibuf.float_buffer.data) {
    blender::gpu::TextureFormat texture_format;
    switch (ibuf.channels) {
      case 1:
        texture_format = blender::gpu::TextureFormat::SFLOAT_32;
        break;
      case 3:
        texture_format = blender::gpu::TextureFormat::SFLOAT_32_32_32;
        break;
      case 4:
        texture_format = blender::gpu::TextureFormat::SFLOAT_32_32_32_32;
        break;
      default:
        BLI_assert_msg(0, "Incompatible number of channels for float buffer in sequencer");
        return nullptr;
    }

    texture = GPU_texture_create_2d(
        "seq_display_buf", ibuf.x, ibuf.y, 1, texture_format, texture_usage, nullptr);
    GPU_texture_update(texture, GPU_DATA_FLOAT, ibuf.float_buffer.data);
  }
  else if (ibuf.byte_buffer.data) {
    texture = GPU_texture_create_2d("seq_display_buf",
                                    ibuf.x,
                                    ibuf.y,
                                    1,
                                    blender::gpu::TextureFormat::UNORM_8_8_8_8,
                                    texture_usage,
                                    nullptr);
    GPU_texture_update(texture, GPU_DATA_UBYTE, ibuf.byte_buffer.data);
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
    if (ibuf.byte_buffer.colorspace) {
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
                                                blender::gpu::Texture *current_texture,
                                                const ImBuf *reference_ibuf,
                                                blender::gpu::Texture *reference_texture)
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

  GPUFrameBuffer *overlay_fb = GPU_viewport_framebuffer_overlay_get(viewport);

  GPU_framebuffer_bind(overlay_fb);
  ED_region_draw_cb_draw(C, &region, REGION_DRAW_POST_VIEW);
  GPU_framebuffer_bind_no_srgb(overlay_fb);
}

/* Part of the sequencer preview region drawing which renders information overlays to the
 * viewport's overlay frame-buffer. */
static void sequencer_preview_draw_overlays(const bContext *C,
                                            const wmWindowManager &wm,
                                            const Scene *scene,
                                            const SpaceSeq &space_sequencer,
                                            const Editing &editing,
                                            const ColorManagedViewSettings &view_settings,
                                            const ColorManagedDisplaySettings &display_settings,
                                            ARegion &region,
                                            blender::gpu::Texture *current_texture,
                                            blender::gpu::Texture *reference_texture,
                                            const ImBuf *overlay_ibuf,
                                            const int timeline_frame)
{
  const bool is_playing = ED_screen_animation_playing(&wm);
  const bool show_imbuf = check_show_imbuf(space_sequencer);

  preview_draw_overlay_begin(region);

  bool has_scopes = false;
  if (overlay_ibuf &&
      sequencer_calc_scopes(space_sequencer, view_settings, display_settings, *overlay_ibuf))
  {
    /* Draw scope. */
    sequencer_draw_scopes(space_sequencer, region);
    has_scopes = true;
  }
  else if (space_sequencer.flag & SEQ_USE_ALPHA) {
    /* Draw checked-board. */
    const View2D &v2d = region.v2d;
    imm_draw_box_checker_2d(v2d.tot.xmin, v2d.tot.ymin, v2d.tot.xmax, v2d.tot.ymax);

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
    const uint pos = GPU_vertformat_attr_add(
        imm_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    GPU_blend(GPU_BLEND_OVERLAY_MASK_FROM_ALPHA);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor3f(-.0f, 1.0f, 1.0f);
    immRectf(pos, position.xmin, position.ymin, position.xmax, position.ymax);
    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
  }

  /* Draw metadata. */
  if (!has_scopes && overlay_ibuf) {
    if ((space_sequencer.preview_overlay.flag & SEQ_PREVIEW_SHOW_METADATA) &&
        (space_sequencer.flag & SEQ_SHOW_OVERLAY))
    {
      const View2D &v2d = region.v2d;
      ED_region_image_metadata_draw(0.0, 0.0, overlay_ibuf, &v2d.tot, 1.0, 1.0);
    }
  }

  if (show_imbuf && (space_sequencer.flag & SEQ_SHOW_OVERLAY)) {
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
  if ((is_playing == false) && (space_sequencer.mainb == SEQ_DRAW_IMG_IMBUF) &&
      is_cursor_visible(space_sequencer))
  {
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
  const char *view_names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

  const ScrArea *area = CTX_wm_area(C);
  const SpaceSeq &space_sequencer = *static_cast<const SpaceSeq *>(area->spacedata.first);
  const Scene *scene = CTX_data_sequencer_scene(C);

  if (!scene->ed || space_sequencer.render_size == SEQ_RENDER_SIZE_NONE) {
    sequencer_preview_draw_empty(*region);
    return;
  }

  const Editing &editing = *scene->ed;
  const RenderData &render_data = scene->r;

  if (!preview_draw_begin(C, render_data, scene->view_settings, scene->display_settings, *region))
  {
    sequencer_preview_draw_empty(*region);
    return;
  }

  const bool show_imbuf = check_show_imbuf(space_sequencer);

  const bool draw_overlay = (space_sequencer.flag & SEQ_SHOW_OVERLAY);
  const bool draw_frame_overlay = (editing.overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_SHOW) &&
                                  draw_overlay;
  const bool need_current_frame = !(draw_frame_overlay && (space_sequencer.overlay_frame_type ==
                                                           SEQ_OVERLAY_FRAME_TYPE_REFERENCE));
  const bool need_reference_frame = draw_frame_overlay && space_sequencer.overlay_frame_type !=
                                                              SEQ_OVERLAY_FRAME_TYPE_CURRENT;

  int timeline_frame = render_data.cfra;
  if (sequencer_draw_get_transform_preview(space_sequencer, *scene)) {
    timeline_frame = sequencer_draw_get_transform_preview_frame(scene);
  }

  /* GPU textures for the current and reference frames.
   *
   * When non-nullptr they are to be drawn (in other words, when they are non-nullptr the
   * corresponding draw_current_frame and draw_reference_frame is true). */
  blender::gpu::Texture *current_texture = nullptr;
  blender::gpu::Texture *reference_texture = nullptr;

  /* Get image buffers before setting up GPU state for drawing.  This is because
   * sequencer_ibuf_get() might not properly restore the state.
   * Additionally, some image buffers might be needed for both color render and overlay drawing. */
  ImBuf *current_ibuf = nullptr;
  ImBuf *reference_ibuf = nullptr;
  if (need_current_frame) {
    current_ibuf = sequencer_ibuf_get(
        C, timeline_frame, view_names[space_sequencer.multiview_eye]);
    if (show_imbuf && current_ibuf) {
      current_texture = create_texture(*current_ibuf);
    }
  }
  if (need_reference_frame) {
    const int offset = get_reference_frame_offset(editing, render_data);
    reference_ibuf = sequencer_ibuf_get(
        C, timeline_frame + offset, view_names[space_sequencer.multiview_eye]);
    if (show_imbuf && reference_ibuf) {
      reference_texture = create_texture(*reference_ibuf);
    }
  }

  /* Image buffer used for overlays: scopes, metadata etc. */
  ImBuf *overlay_ibuf = need_current_frame ? current_ibuf : reference_ibuf;

  /* Draw parts of the preview region to the corresponding frame buffers. */
  sequencer_preview_draw_color_render(space_sequencer,
                                      editing,
                                      *region,
                                      current_ibuf,
                                      current_texture,
                                      reference_ibuf,
                                      reference_texture);
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

  /* Free textures. */
  if (current_texture) {
    GPU_texture_free(current_texture);
  }
  if (reference_texture) {
    GPU_texture_free(reference_texture);
  }

  /* Free CPU side resources. */
  IMB_freeImBuf(current_ibuf);
  IMB_freeImBuf(reference_ibuf);

  preview_draw_end(C);
}

}  // namespace blender::ed::vse
