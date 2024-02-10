/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cmath>
#include <cstring>

#include "BLF_api.hh"

#include "BLI_blenlib.h"
#include "BLI_math_rotation.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_scene.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_viewport.h"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_space_api.hh"
#include "ED_util.hh"

#include "BIF_glutil.hh"

#include "SEQ_channels.hh"
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

static Sequence *special_seq_update = nullptr;

void sequencer_special_update_set(Sequence *seq)
{
  special_seq_update = seq;
}

Sequence *ED_sequencer_special_preview_get()
{
  return special_seq_update;
}

void ED_sequencer_special_preview_set(bContext *C, const int mval[2])
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  int hand_dummy;
  Sequence *seq = find_nearest_seq(scene, &region->v2d, mval, &hand_dummy);
  sequencer_special_update_set(seq);
}

void ED_sequencer_special_preview_clear()
{
  sequencer_special_update_set(nullptr);
}

ImBuf *sequencer_ibuf_get(Main *bmain,
                          ARegion *region,
                          Depsgraph *depsgraph,
                          Scene *scene,
                          SpaceSeq *sseq,
                          int timeline_frame,
                          int frame_ofs,
                          const char *viewname)
{
  SeqRenderData context = {nullptr};
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
    render_size = SEQ_rendersize_to_scale_factor(sseq->render_size);
  }

  rectx = roundf(render_size * scene->r.xsch);
  recty = roundf(render_size * scene->r.ysch);

  SEQ_render_new_render_data(
      bmain, depsgraph, scene, rectx, recty, sseq->render_size, false, &context);
  context.view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);
  context.use_proxies = (sseq->flag & SEQ_USE_PROXIES) != 0;

  /* Sequencer could start rendering, in this case we need to be sure it wouldn't be canceled
   * by Escape pressed somewhere in the past. */
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

  if (ED_sequencer_special_preview_get()) {
    ibuf = SEQ_render_give_ibuf_direct(
        &context, timeline_frame + frame_ofs, ED_sequencer_special_preview_get());
  }
  else {
    ibuf = SEQ_render_give_ibuf(&context, timeline_frame + frame_ofs, sseq->chanshown);
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

static ImBuf *sequencer_make_scope(Scene *scene,
                                   ImBuf *ibuf,
                                   ImBuf *(*make_scope_fn)(const ImBuf *ibuf))
{
  ImBuf *display_ibuf = IMB_dupImBuf(ibuf);
  ImBuf *scope;

  IMB_colormanagement_imbuf_make_display_space(
      display_ibuf, &scene->view_settings, &scene->display_settings);

  scope = make_scope_fn(display_ibuf);
  IMB_rectfill_alpha(scope, 1.0f);

  IMB_freeImBuf(display_ibuf);

  return scope;
}

static void sequencer_display_size(Scene *scene, float r_viewrect[2])
{
  r_viewrect[0] = float(scene->r.xsch);
  r_viewrect[1] = float(scene->r.ysch);

  r_viewrect[0] *= scene->r.xasp / scene->r.yasp;
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
static void sequencer_draw_borders_overlay(const SpaceSeq *sseq,
                                           const View2D *v2d,
                                           const Scene *scene)
{
  float x1 = v2d->tot.xmin;
  float y1 = v2d->tot.ymin;
  float x2 = v2d->tot.xmax;
  float y2 = v2d->tot.ymax;

  GPU_line_width(1.0f);

  /* Draw border. */
  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

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
  if (sseq->preview_overlay.flag & SEQ_PREVIEW_SHOW_SAFE_MARGINS) {
    immUniformThemeColorBlend(TH_VIEW_OVERLAY, TH_BACK, 0.25f);
    rctf rect;
    rect.xmin = x1;
    rect.xmax = x2;
    rect.ymin = y1;
    rect.ymax = y2;
    UI_draw_safe_areas(shdr_pos, &rect, scene->safe_areas.title, scene->safe_areas.action);

    if (sseq->preview_overlay.flag & SEQ_PREVIEW_SHOW_SAFE_CENTER) {

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

      // Scene *scene = CTX_data_scene(C);
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
  if (SEQ_prefetch_need_redraw(CTX_data_main(C), scene)) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, nullptr);
  }
}

static void *sequencer_OCIO_transform_ibuf(const bContext *C,
                                           ImBuf *ibuf,
                                           bool *r_glsl_used,
                                           eGPUTextureFormat *r_format,
                                           eGPUDataFormat *r_data,
                                           void **r_buffer_cache_handle)
{
  void *display_buffer;
  bool force_fallback = false;
  *r_glsl_used = false;
  force_fallback |= (ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL);
  force_fallback |= (ibuf->dither != 0.0f);

  /* Default */
  *r_format = GPU_RGBA8;
  *r_data = GPU_DATA_UBYTE;

  /* Fallback to CPU based color space conversion. */
  if (force_fallback) {
    *r_glsl_used = false;
    display_buffer = nullptr;
  }
  else if (ibuf->float_buffer.data) {
    display_buffer = ibuf->float_buffer.data;

    *r_data = GPU_DATA_FLOAT;
    if (ibuf->channels == 4) {
      *r_format = GPU_RGBA16F;
    }
    else if (ibuf->channels == 3) {
      /* Alpha is implicitly 1. */
      *r_format = GPU_RGB16F;
    }
    else {
      BLI_assert_msg(0, "Incompatible number of channels for float buffer in sequencer");
      *r_format = GPU_RGBA16F;
      display_buffer = nullptr;
    }

    if (ibuf->float_buffer.colorspace) {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space_ctx(
          C, ibuf->float_buffer.colorspace, ibuf->dither, true);
    }
    else {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw_ctx(C, ibuf->dither, true);
    }
  }
  else if (ibuf->byte_buffer.data) {
    display_buffer = ibuf->byte_buffer.data;

    *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space_ctx(
        C, ibuf->byte_buffer.colorspace, ibuf->dither, false);
  }
  else {
    display_buffer = nullptr;
  }

  /* There is data to be displayed, but GLSL is not initialized
   * properly, in this case we fallback to CPU-based display transform. */
  if ((ibuf->byte_buffer.data || ibuf->float_buffer.data) && !*r_glsl_used) {
    display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, r_buffer_cache_handle);
    *r_format = GPU_RGBA8;
    *r_data = GPU_DATA_UBYTE;
  }

  return display_buffer;
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

static void sequencer_preview_get_rect(rctf *preview,
                                       Scene *scene,
                                       ARegion *region,
                                       SpaceSeq *sseq,
                                       bool draw_overlay,
                                       bool draw_backdrop)
{
  View2D *v2d = &region->v2d;
  float viewrect[2];

  sequencer_display_size(scene, viewrect);
  BLI_rctf_init(preview, -1.0f, 1.0f, -1.0f, 1.0f);

  if (draw_overlay && (sseq->overlay_frame_type == SEQ_OVERLAY_FRAME_TYPE_RECT)) {
    preview->xmax = v2d->tot.xmin +
                    (fabsf(BLI_rctf_size_x(&v2d->tot)) * scene->ed->overlay_frame_rect.xmax);
    preview->xmin = v2d->tot.xmin +
                    (fabsf(BLI_rctf_size_x(&v2d->tot)) * scene->ed->overlay_frame_rect.xmin);
    preview->ymax = v2d->tot.ymin +
                    (fabsf(BLI_rctf_size_y(&v2d->tot)) * scene->ed->overlay_frame_rect.ymax);
    preview->ymin = v2d->tot.ymin +
                    (fabsf(BLI_rctf_size_y(&v2d->tot)) * scene->ed->overlay_frame_rect.ymin);
  }
  else if (draw_backdrop) {
    float aspect = BLI_rcti_size_x(&region->winrct) / float(BLI_rcti_size_y(&region->winrct));
    float image_aspect = viewrect[0] / viewrect[1];

    if (aspect >= image_aspect) {
      preview->xmax = image_aspect / aspect;
      preview->xmin = -preview->xmax;
    }
    else {
      preview->ymax = aspect / image_aspect;
      preview->ymin = -preview->ymax;
    }
  }
  else {
    *preview = v2d->tot;
  }
}

static void sequencer_draw_display_buffer(const bContext *C,
                                          Scene *scene,
                                          ARegion *region,
                                          SpaceSeq *sseq,
                                          ImBuf *ibuf,
                                          bool draw_overlay,
                                          bool draw_backdrop)
{
  void *buffer_cache_handle = nullptr;

  if (sseq->mainb == SEQ_DRAW_IMG_IMBUF && sseq->flag & SEQ_USE_ALPHA) {
    GPU_blend(GPU_BLEND_ALPHA);
  }

  /* Format needs to be created prior to any #immBindShader call.
   * Do it here because OCIO binds its own shader. */
  eGPUTextureFormat format;
  eGPUDataFormat data;
  bool glsl_used = false;
  GPUVertFormat *imm_format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(imm_format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint texCoord = GPU_vertformat_attr_add(
      imm_format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  void *display_buffer = sequencer_OCIO_transform_ibuf(
      C, ibuf, &glsl_used, &format, &data, &buffer_cache_handle);

  if (draw_backdrop) {
    GPU_matrix_push();
    GPU_matrix_identity_set();
    GPU_matrix_push_projection();
    GPU_matrix_identity_projection_set();
  }
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
  GPUTexture *texture = GPU_texture_create_2d(
      "seq_display_buf", ibuf->x, ibuf->y, 1, format, usage, nullptr);
  GPU_texture_update(texture, data, display_buffer);
  GPU_texture_filter_mode(texture, false);

  GPU_texture_bind(texture, 0);

  if (!glsl_used) {
    immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);
    immUniformColor3f(1.0f, 1.0f, 1.0f);
  }

  immBegin(GPU_PRIM_TRI_FAN, 4);

  rctf preview;
  rctf canvas;
  sequencer_preview_get_rect(&preview, scene, region, sseq, draw_overlay, draw_backdrop);

  if (draw_overlay && (sseq->overlay_frame_type == SEQ_OVERLAY_FRAME_TYPE_RECT)) {
    canvas = scene->ed->overlay_frame_rect;
  }
  else {
    BLI_rctf_init(&canvas, 0.0f, 1.0f, 0.0f, 1.0f);
  }

  immAttr2f(texCoord, canvas.xmin, canvas.ymin);
  immVertex2f(pos, preview.xmin, preview.ymin);

  immAttr2f(texCoord, canvas.xmin, canvas.ymax);
  immVertex2f(pos, preview.xmin, preview.ymax);

  immAttr2f(texCoord, canvas.xmax, canvas.ymax);
  immVertex2f(pos, preview.xmax, preview.ymax);

  immAttr2f(texCoord, canvas.xmax, canvas.ymin);
  immVertex2f(pos, preview.xmax, preview.ymin);

  immEnd();

  GPU_texture_unbind(texture);
  GPU_texture_free(texture);

  if (!glsl_used) {
    immUnbindProgram();
  }
  else {
    IMB_colormanagement_finish_glsl_draw();
  }

  if (buffer_cache_handle) {
    IMB_display_buffer_release(buffer_cache_handle);
  }

  if (sseq->mainb == SEQ_DRAW_IMG_IMBUF && sseq->flag & SEQ_USE_ALPHA) {
    GPU_blend(GPU_BLEND_NONE);
  }

  if (draw_backdrop) {
    GPU_matrix_pop();
    GPU_matrix_pop_projection();
  }
}

static void draw_histogram(ARegion *region,
                           const blender::ed::seq::ScopeHistogram &hist,
                           SeqQuadsBatch &quads,
                           const rctf &area)
{
  using namespace blender::ed::seq;
  if (hist.data.is_empty()) {
    return;
  }

  /* Background. */
  uchar col_bg[4] = {0, 0, 0, 255};
  quads.add_quad(area.xmin, area.ymin, area.xmax, area.ymax, col_bg);

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

  View2D *v2d = &region->v2d;
  float text_scale_x, text_scale_y;
  UI_view2d_scale_get_inverse(v2d, &text_scale_x, &text_scale_y);

  for (int line = 0; line <= 4; line++) {
    float val = float(line) / 4;
    float x = grid_x_0 + (grid_x_1 - grid_x_0) * val;
    quads.add_line(x, area.ymin, x, area.ymax, col_grid);

    /* Label. */
    char buf[10];
    BLI_snprintf(buf, sizeof(buf), "%.2f", val);
    size_t buf_len = strlen(buf);

    float text_width, text_height;
    BLF_width_and_height(BLF_default(), buf, buf_len, &text_width, &text_height);
    text_width *= text_scale_x;
    text_height *= text_scale_y;
    UI_view2d_text_cache_add(
        v2d, x - text_width / 2, area.ymax - text_height * 1.3f, buf, buf_len, col_grid);
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

  UI_view2d_text_cache_draw(region);
}

static blender::float2 rgb_to_uv(const blender::float3 &rgb)
{
  float y, u, v;
  rgb_to_yuv(rgb.x, rgb.y, rgb.z, &y, &u, &v, BLI_YUV_ITU_BT709);
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
    SNPRINTF(buf, "%.1f", lines[i]);
    quads.add_line(x0, y, x1, y, col_grid);
    UI_view2d_text_cache_add(&region->v2d, x0 + 8, y + 8, buf, strlen(buf), col_grid);
  }
  /* Border. */
  uchar col_border[4] = {64, 64, 64, 128};
  quads.add_wire_quad(x0, area.ymin, x1, area.ymax, col_border);

  quads.draw();
  UI_view2d_text_cache_draw(region);
}

static void draw_vectorscope_graticule(ARegion *region, SeqQuadsBatch &quads, const rctf &area)
{
  using namespace blender;

  const float skin_rad = DEG2RADF(123.0f); /* angle in radians of the skin tone line */

  const float w = BLI_rctf_size_x(&area);
  const float h = BLI_rctf_size_y(&area);
  const float2 center{BLI_rctf_cent_x(&area), BLI_rctf_cent_y(&area)};
  /* Vector-scope image is scaled over UV range (+/-0.615). */
  const float radius = ((w < h) ? w : h) * 0.5f * (0.5f / 0.615f);

  /* Precalculate circle points/colors. */
  constexpr int circle_delta = 6;
  constexpr int num_circle_points = 360 / circle_delta;
  float2 circle_pos[num_circle_points];
  float3 circle_col[num_circle_points];
  for (int i = 0; i < num_circle_points; i++) {
    float a = DEG2RADF(i * circle_delta);
    float u = cosf(a);
    float v = sinf(a);
    circle_pos[i] = float2(u, v);

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
    float2 pos = center + rgb_to_uv(safe) * (radius * 2);
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

static void sequencer_draw_scopes(Scene *scene, ARegion *region, SpaceSeq *sseq)
{
  using namespace blender::ed::seq;

  /* Figure out draw coordinates. */
  rctf preview;
  sequencer_preview_get_rect(&preview, scene, region, sseq, false, false);

  rctf uv;
  BLI_rctf_init(&uv, 0.0f, 1.0f, 0.0f, 1.0f);
  const bool keep_aspect = sseq->mainb == SEQ_DRAW_IMG_VECTORSCOPE;
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
  SeqScopes *scopes = &sseq->runtime->scopes;

  /* Draw scope image if there is one. */
  bool use_blend = sseq->mainb == SEQ_DRAW_IMG_IMBUF && sseq->flag & SEQ_USE_ALPHA;
  ImBuf *scope_image = nullptr;
  if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
    scope_image = scopes->zebra_ibuf;
  }
  else if (sseq->mainb == SEQ_DRAW_IMG_WAVEFORM) {
    scope_image = (sseq->flag & SEQ_DRAW_COLOR_SEPARATED) != 0 ? scopes->sep_waveform_ibuf :
                                                                 scopes->waveform_ibuf;
  }
  else if (sseq->mainb == SEQ_DRAW_IMG_VECTORSCOPE) {
    scope_image = scopes->vector_ibuf;
  }
  else if (sseq->mainb == SEQ_DRAW_IMG_HISTOGRAM) {
    use_blend = true;
  }

  if (use_blend) {
    GPU_blend(GPU_BLEND_ALPHA);
  }

  if (scope_image != nullptr) {
    if (scope_image->float_buffer.data && scope_image->byte_buffer.data == nullptr) {
      IMB_rect_from_float(scope_image);
    }

    eGPUTextureFormat format = GPU_RGBA8;
    eGPUDataFormat data = GPU_DATA_UBYTE;
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    GPUTexture *texture = GPU_texture_create_2d(
        "seq_display_buf", scope_image->x, scope_image->y, 1, format, usage, nullptr);
    GPU_texture_update(texture, data, scope_image->byte_buffer.data);
    GPU_texture_filter_mode(texture, false);
    GPU_texture_extend_mode(texture, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);

    GPU_texture_bind(texture, 0);

    GPUVertFormat *imm_format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(imm_format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint texCoord = GPU_vertformat_attr_add(
        imm_format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
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

  if (sseq->mainb == SEQ_DRAW_IMG_HISTOGRAM) {
    draw_histogram(region, scopes->histogram, quads, preview);
  }
  if (sseq->mainb == SEQ_DRAW_IMG_WAVEFORM) {
    use_blend = true;
    draw_waveform_graticule(region, quads, preview);
  }
  if (sseq->mainb == SEQ_DRAW_IMG_VECTORSCOPE) {
    use_blend = true;
    draw_vectorscope_graticule(region, quads, preview);
  }

  quads.draw();

  if (use_blend) {
    GPU_blend(GPU_BLEND_NONE);
  }
}

static bool sequencer_calc_scopes(Scene *scene, SpaceSeq *sseq, ImBuf *ibuf, bool draw_backdrop)
{
  using namespace blender::ed::seq;

  if (draw_backdrop || (sseq->mainb == SEQ_DRAW_IMG_IMBUF && sseq->zebra == 0)) {
    return false; /* Not drawing any scopes. */
  }

  SeqScopes *scopes = &sseq->runtime->scopes;
  if (scopes->reference_ibuf != ibuf) {
    scopes->cleanup();
  }

  switch (sseq->mainb) {
    case SEQ_DRAW_IMG_IMBUF:
      if (!scopes->zebra_ibuf) {

        if (ibuf->float_buffer.data) {
          ImBuf *display_ibuf = IMB_dupImBuf(ibuf);
          IMB_colormanagement_imbuf_make_display_space(
              display_ibuf, &scene->view_settings, &scene->display_settings);
          scopes->zebra_ibuf = make_zebra_view_from_ibuf(display_ibuf, sseq->zebra);
          IMB_freeImBuf(display_ibuf);
        }
        else {
          scopes->zebra_ibuf = make_zebra_view_from_ibuf(ibuf, sseq->zebra);
        }
      }
      break;
    case SEQ_DRAW_IMG_WAVEFORM:
      if ((sseq->flag & SEQ_DRAW_COLOR_SEPARATED) != 0) {
        if (!scopes->sep_waveform_ibuf) {
          scopes->sep_waveform_ibuf = sequencer_make_scope(
              scene, ibuf, make_sep_waveform_view_from_ibuf);
        }
      }
      else {
        if (!scopes->waveform_ibuf) {
          scopes->waveform_ibuf = sequencer_make_scope(scene, ibuf, make_waveform_view_from_ibuf);
        }
      }
      break;
    case SEQ_DRAW_IMG_VECTORSCOPE:
      if (!scopes->vector_ibuf) {
        scopes->vector_ibuf = sequencer_make_scope(scene, ibuf, make_vectorscope_view_from_ibuf);
      }
      break;
    case SEQ_DRAW_IMG_HISTOGRAM: {
      ImBuf *display_ibuf = IMB_dupImBuf(ibuf);
      IMB_colormanagement_imbuf_make_display_space(
          display_ibuf, &scene->view_settings, &scene->display_settings);
      scopes->histogram.calc_from_ibuf(display_ibuf);
      IMB_freeImBuf(display_ibuf);
    } break;
    default: /* Future files might have scopes we don't know about. */
      return false;
  }
  scopes->reference_ibuf = ibuf;
  return true;
}

bool sequencer_draw_get_transform_preview(SpaceSeq *sseq, Scene *scene)
{
  Sequence *last_seq = SEQ_select_active_get(scene);
  if (last_seq == nullptr) {
    return false;
  }

  return (G.moving & G_TRANSFORM_SEQ) && (last_seq->flag & SELECT) &&
         ((last_seq->flag & SEQ_LEFTSEL) || (last_seq->flag & SEQ_RIGHTSEL)) &&
         (sseq->draw_flag & SEQ_DRAW_TRANSFORM_PREVIEW);
}

int sequencer_draw_get_transform_preview_frame(Scene *scene)
{
  Sequence *last_seq = SEQ_select_active_get(scene);
  /* #sequencer_draw_get_transform_preview must already have been called. */
  BLI_assert(last_seq != nullptr);
  int preview_frame;

  if (last_seq->flag & SEQ_RIGHTSEL) {
    preview_frame = SEQ_time_right_handle_frame_get(scene, last_seq) - 1;
  }
  else {
    preview_frame = SEQ_time_left_handle_frame_get(scene, last_seq);
  }

  return preview_frame;
}

static void seq_draw_image_origin_and_outline(const bContext *C, Sequence *seq, bool is_active_seq)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  const ARegion *region = CTX_wm_region(C);
  if (region->regiontype == RGN_TYPE_PREVIEW && !sequencer_view_preview_only_poll(C)) {
    return;
  }
  if ((seq->flag & SELECT) == 0) {
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
  if (ELEM(sseq->mainb, SEQ_DRAW_IMG_WAVEFORM, SEQ_DRAW_IMG_VECTORSCOPE, SEQ_DRAW_IMG_HISTOGRAM)) {
    return;
  }

  float origin[2];
  SEQ_image_transform_origin_offset_pixelspace_get(CTX_data_scene(C), seq, origin);

  /* Origin. */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
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
  float seq_image_quad[4][2];
  SEQ_image_transform_final_quad_get(CTX_data_scene(C), seq, seq_image_quad);

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
  immUniform1f("lineWidth", U.pixelsize);
  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos, seq_image_quad[0][0], seq_image_quad[0][1]);
  immVertex2f(pos, seq_image_quad[1][0], seq_image_quad[1][1]);
  immVertex2f(pos, seq_image_quad[2][0], seq_image_quad[2][1]);
  immVertex2f(pos, seq_image_quad[3][0], seq_image_quad[3][1]);
  immEnd();
  immUnbindProgram();
  GPU_line_width(1);
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

void sequencer_draw_preview(const bContext *C,
                            Scene *scene,
                            ARegion *region,
                            SpaceSeq *sseq,
                            int timeline_frame,
                            int offset,
                            bool draw_overlay,
                            bool draw_backdrop)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  View2D *v2d = &region->v2d;
  ImBuf *ibuf = nullptr;
  float viewrect[2];
  const bool show_imbuf = ED_space_sequencer_check_show_imbuf(sseq);
  const bool draw_gpencil = ((sseq->preview_overlay.flag & SEQ_PREVIEW_SHOW_GPENCIL) && sseq->gpd);
  const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

  sequencer_stop_running_jobs(C, scene);
  if (G.is_rendering) {
    return;
  }

  int preview_frame = timeline_frame;
  if (sequencer_draw_get_transform_preview(sseq, scene)) {
    preview_frame = sequencer_draw_get_transform_preview_frame(scene);
  }

  /* Get image. */
  ibuf = sequencer_ibuf_get(
      bmain, region, depsgraph, scene, sseq, preview_frame, offset, names[sseq->multiview_eye]);

  /* Setup off-screen buffers. */
  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  GPUFrameBuffer *framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);
  GPU_depth_test(GPU_DEPTH_NONE);

  if (sseq->render_size == SEQ_RENDER_SIZE_NONE) {
    sequencer_preview_clear();
    return;
  }

  /* Setup view. */
  sequencer_display_size(scene, viewrect);
  UI_view2d_totRect_set(v2d, roundf(viewrect[0] + 0.5f), roundf(viewrect[1] + 0.5f));
  UI_view2d_curRect_validate(v2d);
  UI_view2d_view_ortho(v2d);

  /* Draw background. */
  if (!draw_backdrop &&
      (!draw_overlay || (sseq->overlay_frame_type == SEQ_OVERLAY_FRAME_TYPE_REFERENCE)))
  {
    sequencer_preview_clear();

    if (sseq->flag & SEQ_USE_ALPHA) {
      imm_draw_box_checker_2d(v2d->tot.xmin, v2d->tot.ymin, v2d->tot.xmax, v2d->tot.ymax);
    }
  }

  if (ibuf) {
    bool has_scope = sequencer_calc_scopes(scene, sseq, ibuf, draw_backdrop);
    if (has_scope) {
      /* Draw scope. */
      sequencer_draw_scopes(scene, region, sseq);
    }
    else {
      /* Draw image. */
      sequencer_draw_display_buffer(C, scene, region, sseq, ibuf, draw_overlay, draw_backdrop);
    }

    /* Draw metadata. */
    if (sseq->preview_overlay.flag & SEQ_PREVIEW_SHOW_METADATA && sseq->flag & SEQ_SHOW_OVERLAY) {
      ED_region_image_metadata_draw(0.0, 0.0, ibuf, &v2d->tot, 1.0, 1.0);
    }
  }

  if (show_imbuf && (sseq->flag & SEQ_SHOW_OVERLAY)) {
    sequencer_draw_borders_overlay(sseq, v2d, scene);
  }

  if (!draw_backdrop && scene->ed != nullptr) {
    Editing *ed = SEQ_editing_get(scene);
    ListBase *channels = SEQ_channels_displayed_get(ed);
    blender::VectorSet strips = SEQ_query_rendered_strips(
        scene, channels, ed->seqbasep, timeline_frame, 0);
    Sequence *active_seq = SEQ_select_active_get(scene);
    for (Sequence *seq : strips) {
      seq_draw_image_origin_and_outline(C, seq, seq == active_seq);
    }
  }

  if (draw_gpencil && show_imbuf && (sseq->flag & SEQ_SHOW_OVERLAY)) {
    sequencer_draw_gpencil_overlay(C);
  }

#if 0
  sequencer_draw_maskedit(C, scene, region, sseq);
#endif

  /* Draw registered callbacks. */
  GPU_framebuffer_bind(framebuffer_overlay);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);

  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }

  UI_view2d_view_restore(C);
  seq_prefetch_wm_notify(C, scene);
}
