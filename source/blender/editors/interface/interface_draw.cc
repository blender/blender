/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cmath>
#include <cstring>

#include "DNA_color_types.h"
#include "DNA_curve_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_screen_types.h"

#include "BLI_math_rotation.h"
#include "BLI_polyfill_2d.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_curveprofile.h"
#include "BKE_tracking.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BIF_glutil.hh"

#include "BLF_api.hh"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_uniform_buffer.hh"

/* own include */
#include "interface_intern.hh"

static int roundboxtype = UI_CNR_ALL;

void UI_draw_roundbox_corner_set(int type)
{
  /* Not sure the roundbox function is the best place to change this
   * if this is undone, it's not that big a deal, only makes curves edges square. */
  roundboxtype = type;
}

#if 0 /* unused */
int UI_draw_roundbox_corner_get()
{
  return roundboxtype;
}
#endif

void UI_draw_roundbox_4fv_ex(const rctf *rect,
                             const float inner1[4],
                             const float inner2[4],
                             float shade_dir,
                             const float outline[4],
                             float outline_width,
                             float rad)
{
  /* WATCH: This is assuming the ModelViewProjectionMatrix is area pixel space.
   * If it has been scaled, then it's no longer valid. */
  uiWidgetBaseParameters widget_params{};
  widget_params.recti.xmin = rect->xmin + outline_width;
  widget_params.recti.ymin = rect->ymin + outline_width;
  widget_params.recti.xmax = rect->xmax - outline_width;
  widget_params.recti.ymax = rect->ymax - outline_width;
  widget_params.rect = *rect;
  widget_params.radi = rad;
  widget_params.rad = rad;
  widget_params.round_corners[0] = (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 1.0f : 0.0f;
  widget_params.round_corners[1] = (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 1.0f : 0.0f;
  widget_params.round_corners[2] = (roundboxtype & UI_CNR_TOP_RIGHT) ? 1.0f : 0.0f;
  widget_params.round_corners[3] = (roundboxtype & UI_CNR_TOP_LEFT) ? 1.0f : 0.0f;
  widget_params.color_inner1[0] = inner1 ? inner1[0] : 0.0f;
  widget_params.color_inner1[1] = inner1 ? inner1[1] : 0.0f;
  widget_params.color_inner1[2] = inner1 ? inner1[2] : 0.0f;
  widget_params.color_inner1[3] = inner1 ? inner1[3] : 0.0f;
  widget_params.color_inner2[0] = inner2 ? inner2[0] : inner1 ? inner1[0] : 0.0f;
  widget_params.color_inner2[1] = inner2 ? inner2[1] : inner1 ? inner1[1] : 0.0f;
  widget_params.color_inner2[2] = inner2 ? inner2[2] : inner1 ? inner1[2] : 0.0f;
  widget_params.color_inner2[3] = inner2 ? inner2[3] : inner1 ? inner1[3] : 0.0f;
  widget_params.color_outline[0] = outline ? outline[0] : inner1 ? inner1[0] : 0.0f;
  widget_params.color_outline[1] = outline ? outline[1] : inner1 ? inner1[1] : 0.0f;
  widget_params.color_outline[2] = outline ? outline[2] : inner1 ? inner1[2] : 0.0f;
  widget_params.color_outline[3] = outline ? outline[3] : inner1 ? inner1[3] : 0.0f;
  widget_params.shade_dir = shade_dir;
  widget_params.alpha_discard = 1.0f;

  blender::gpu::Batch *batch = ui_batch_roundbox_widget_get();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE);
  GPU_batch_uniform_4fv_array(batch, "parameters", 11, (const float (*)[4]) & widget_params);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_batch_draw(batch);
  GPU_blend(GPU_BLEND_NONE);
}

void UI_draw_roundbox_3ub_alpha(
    const rctf *rect, bool filled, float rad, const uchar col[3], uchar alpha)
{
  const float colv[4] = {
      float(col[0]) / 255.0f,
      float(col[1]) / 255.0f,
      float(col[2]) / 255.0f,
      float(alpha) / 255.0f,
  };
  UI_draw_roundbox_4fv_ex(rect, (filled) ? colv : nullptr, nullptr, 1.0f, colv, U.pixelsize, rad);
}

void UI_draw_roundbox_3fv_alpha(
    const rctf *rect, bool filled, float rad, const float col[3], float alpha)
{
  const float colv[4] = {col[0], col[1], col[2], alpha};
  UI_draw_roundbox_4fv_ex(rect, (filled) ? colv : nullptr, nullptr, 1.0f, colv, U.pixelsize, rad);
}

void UI_draw_roundbox_aa(const rctf *rect, bool filled, float rad, const float color[4])
{
  /* XXX this is to emulate previous behavior of semitransparent fills but that's was a side effect
   * of the previous AA method. Better fix the callers. */
  float colv[4] = {color[0], color[1], color[2], color[3]};
  if (filled) {
    colv[3] *= 0.65f;
  }

  UI_draw_roundbox_4fv_ex(rect, (filled) ? colv : nullptr, nullptr, 1.0f, colv, U.pixelsize, rad);
}

void UI_draw_roundbox_4fv(const rctf *rect, bool filled, float rad, const float col[4])
{
  /* Exactly the same as UI_draw_roundbox_aa but does not do the legacy transparency. */
  UI_draw_roundbox_4fv_ex(rect, (filled) ? col : nullptr, nullptr, 1.0f, col, U.pixelsize, rad);
}

void ui_draw_rounded_corners_inverted(const rcti &rect,
                                      const float rad,
                                      const blender::float4 color)
{
  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  float vec[4][2] = {
      {0.195, 0.02},
      {0.55, 0.169},
      {0.831, 0.45},
      {0.98, 0.805},
  };
  for (int a = 0; a < 4; a++) {
    mul_v2_fl(vec[a], rad);
  }

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(color);

  if (roundboxtype & UI_CNR_TOP_LEFT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmin, rect.ymax);
    immVertex2f(pos, rect.xmin, rect.ymax - rad);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmin + vec[a][1], rect.ymax - rad + vec[a][0]);
    }
    immVertex2f(pos, rect.xmin + rad, rect.ymax);
    immEnd();
  }

  if (roundboxtype & UI_CNR_TOP_RIGHT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmax, rect.ymax);
    immVertex2f(pos, rect.xmax - rad, rect.ymax);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmax - rad + vec[a][0], rect.ymax - vec[a][1]);
    }
    immVertex2f(pos, rect.xmax, rect.ymax - rad);
    immEnd();
  }

  if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmax, rect.ymin);
    immVertex2f(pos, rect.xmax, rect.ymin + rad);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmax - vec[a][1], rect.ymin + rad - vec[a][0]);
    }
    immVertex2f(pos, rect.xmax - rad, rect.ymin);
    immEnd();
  }

  if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmin, rect.ymin);
    immVertex2f(pos, rect.xmin + rad, rect.ymin);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmin + rad - vec[a][0], rect.ymin + vec[a][1]);
    }
    immVertex2f(pos, rect.xmin, rect.ymin + rad);
    immEnd();
  }

  immUnbindProgram();
}

void UI_draw_text_underline(int pos_x, int pos_y, int len, int height, const float color[4])
{
  const int ofs_y = 4 * U.pixelsize;

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(color);

  immRectf(pos, pos_x, pos_y - ofs_y, pos_x + len, pos_y - ofs_y + (height * U.pixelsize));
  immUnbindProgram();
}

/* ************** SPECIAL BUTTON DRAWING FUNCTIONS ************* */

void ui_draw_but_TAB_outline(const rcti *rect,
                             float rad,
                             uchar highlight[3],
                             uchar highlight_fade[3])
{
  /* NOTE: based on `UI_draw_roundbox` functions
   * check on making a version which allows us to skip some sides. */

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  const uint col = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::UNORM_8_8_8_8);
  /* add a 1px offset, looks nicer */
  const int minx = rect->xmin + U.pixelsize, maxx = rect->xmax - U.pixelsize;
  const int miny = rect->ymin + U.pixelsize, maxy = rect->ymax - U.pixelsize;
  int a;
  float vec[4][2] = {
      {0.195, 0.02},
      {0.55, 0.169},
      {0.831, 0.45},
      {0.98, 0.805},
  };

  /* Multiply. */
  for (a = 0; a < 4; a++) {
    mul_v2_fl(vec[a], rad);
  }

  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);
  immBeginAtMost(GPU_PRIM_LINE_STRIP, 25);

  immAttr4ub(col, UNPACK3(highlight), 255);

  /* start with corner left-top */
  if (roundboxtype & UI_CNR_TOP_LEFT) {
    immVertex2f(pos, minx, maxy - rad);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, minx + vec[a][1], maxy - rad + vec[a][0]);
    }
    immVertex2f(pos, minx + rad, maxy);
  }
  else {
    immVertex2f(pos, minx, maxy);
  }

  /* corner right-top */
  if (roundboxtype & UI_CNR_TOP_RIGHT) {
    immVertex2f(pos, maxx - rad, maxy);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, maxx - rad + vec[a][0], maxy - vec[a][1]);
    }
    immVertex2f(pos, maxx, maxy - rad);
  }
  else {
    immVertex2f(pos, maxx, maxy);
  }

  immAttr4ub(col, UNPACK3(highlight_fade), 255);

  /* corner right-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
    immVertex2f(pos, maxx, miny + rad);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, maxx - vec[a][1], miny + rad - vec[a][0]);
    }
    immVertex2f(pos, maxx - rad, miny);
  }
  else {
    immVertex2f(pos, maxx, miny);
  }

  /* corner left-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
    immVertex2f(pos, minx + rad, miny);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, minx + rad - vec[a][0], miny + vec[a][1]);
    }
    immVertex2f(pos, minx, miny + rad);
  }
  else {
    immVertex2f(pos, minx, miny);
  }

  immAttr4ub(col, UNPACK3(highlight), 255);

  /* back to corner left-top */
  immVertex2f(pos, minx, (roundboxtype & UI_CNR_TOP_LEFT) ? (maxy - rad) : maxy);

  immEnd();
  immUnbindProgram();
}

void ui_draw_but_IMAGE(ARegion * /*region*/,
                       uiBut *but,
                       const uiWidgetColors * /*wcol*/,
                       const rcti *rect)
{
#ifdef WITH_HEADLESS
  (void)rect;
  (void)but;
#else
  ImBuf *ibuf = (ImBuf *)but->poin;

  if (!ibuf) {
    return;
  }

  const int w = BLI_rcti_size_x(rect);
  const int h = BLI_rcti_size_y(rect);

  /* scissor doesn't seem to be doing the right thing...? */
#  if 0
  /* prevent drawing outside widget area */
  int scissor[4];
  GPU_scissor_get(scissor);
  GPU_scissor(rect->xmin, rect->ymin, w, h);
#  endif

  /* Combine with premultiplied alpha. */
  GPU_blend(GPU_BLEND_ALPHA_PREMULT);

  if (w != ibuf->x || h != ibuf->y) {
    /* We scale the bitmap, rather than have OGL do a worse job. */
    IMB_scale(ibuf, w, h, IMBScaleFilter::Box, false);
  }

  float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  if (but->col[3] != 0) {
    /* Optionally use uiBut's col to recolor the image. */
    rgba_uchar_to_float(col, but->col);
  }

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
  immDrawPixelsTexTiled(&state,
                        float(rect->xmin),
                        float(rect->ymin),
                        ibuf->x,
                        ibuf->y,
                        blender::gpu::TextureFormat::UNORM_8_8_8_8,
                        false,
                        ibuf->byte_buffer.data,
                        1.0f,
                        1.0f,
                        col);

  GPU_blend(GPU_BLEND_NONE);

#  if 0
  /* Restore scissor-test. */
  GPU_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);
#  endif

#endif
}

void UI_draw_safe_areas(uint pos,
                        const rctf *rect,
                        const float title_aspect[2],
                        const float action_aspect[2])
{
  const float size_x_half = (rect->xmax - rect->xmin) * 0.5f;
  const float size_y_half = (rect->ymax - rect->ymin) * 0.5f;

  const float *safe_areas[] = {title_aspect, action_aspect};
  const int safe_len = ARRAY_SIZE(safe_areas);

  for (int i = 0; i < safe_len; i++) {
    if (safe_areas[i][0] || safe_areas[i][1]) {
      const float margin_x = safe_areas[i][0] * size_x_half;
      const float margin_y = safe_areas[i][1] * size_y_half;

      const float minx = rect->xmin + margin_x;
      const float miny = rect->ymin + margin_y;
      const float maxx = rect->xmax - margin_x;
      const float maxy = rect->ymax - margin_y;

      imm_draw_box_wire_2d(pos, minx, miny, maxx, maxy);
    }
  }
}

static void draw_scope_end(const rctf *rect)
{
  GPU_blend(GPU_BLEND_ALPHA);

  /* outline */
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  const float color[4] = {0.0f, 0.0f, 0.0f, 0.5f};
  rctf box_rect{};
  box_rect.xmin = rect->xmin - 1;
  box_rect.xmax = rect->xmax + 1;
  box_rect.ymin = rect->ymin;
  box_rect.ymax = rect->ymax + 1;
  UI_draw_roundbox_4fv(&box_rect, false, 3.0f, color);
}

static void histogram_draw_one(float r,
                               float g,
                               float b,
                               float alpha,
                               float x,
                               float y,
                               float w,
                               float h,
                               const float *data,
                               int res,
                               const bool is_line,
                               uint pos_attr)
{
  const float color[4] = {r, g, b, alpha};

  /* that can happen */
  if (res == 0) {
    return;
  }

  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ADDITIVE);

  immUniformColor4fv(color);

  if (is_line) {
    /* curve outline */
    GPU_line_width(1.5);

    immBegin(GPU_PRIM_LINE_STRIP, res);
    for (int i = 0; i < res; i++) {
      const float x2 = x + i * (w / float(res));
      immVertex2f(pos_attr, x2, y + (data[i] * h));
    }
    immEnd();

    GPU_line_width(1.0f);
  }
  else {
    /* under the curve */
    immBegin(GPU_PRIM_TRI_STRIP, res * 2);
    immVertex2f(pos_attr, x, y);
    immVertex2f(pos_attr, x, y + (data[0] * h));
    for (int i = 1; i < res; i++) {
      const float x2 = x + i * (w / float(res));
      immVertex2f(pos_attr, x2, y + (data[i] * h));
      immVertex2f(pos_attr, x2, y);
    }
    immEnd();

    /* curve outline */
    immUniformColor4f(0.0f, 0.0f, 0.0f, 0.25f);

    GPU_blend(GPU_BLEND_ALPHA);
    immBegin(GPU_PRIM_LINE_STRIP, res);
    for (int i = 0; i < res; i++) {
      const float x2 = x + i * (w / float(res));
      immVertex2f(pos_attr, x2, y + (data[i] * h));
    }
    immEnd();
  }

  GPU_line_smooth(false);
}

#define HISTOGRAM_TOT_GRID_LINES 4

void ui_draw_but_HISTOGRAM(ARegion *region,
                           uiBut *but,
                           const uiWidgetColors * /*wcol*/,
                           const rcti *recti)
{
  Histogram *hist = (Histogram *)but->poin;
  const int res = hist->x_resolution;
  const bool is_line = (hist->flag & HISTO_FLAG_LINE) != 0;

  rctf rect{};
  rect.xmin = float(recti->xmin + 1);
  rect.xmax = float(recti->xmax - 1);
  rect.ymin = float(recti->ymin + 1);
  rect.ymax = float(recti->ymax - 1);

  const float w = BLI_rctf_size_x(&rect);
  const float h = BLI_rctf_size_y(&rect) * hist->ymax;

  GPU_blend(GPU_BLEND_ALPHA);

  float color[4];
  UI_GetThemeColor4fv(TH_PREVIEW_BACK, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  rctf back_rect{};
  back_rect.xmin = rect.xmin - 1;
  back_rect.xmax = rect.xmax + 1;
  back_rect.ymin = rect.ymin - 1;
  back_rect.ymax = rect.ymax + 1;

  UI_draw_roundbox_4fv(&back_rect, true, 3.0f, color);

  /* need scissor test, histogram can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get(scissor);
  rcti scissor_new{};
  scissor_new.xmin = rect.xmin;
  scissor_new.ymin = rect.ymin;
  scissor_new.xmax = rect.xmax;
  scissor_new.ymax = rect.ymax;
  const rcti scissor_region = {0, region->winx, 0, region->winy};
  BLI_rcti_isect(&scissor_new, &scissor_region, &scissor_new);
  GPU_scissor(scissor_new.xmin,
              scissor_new.ymin,
              BLI_rcti_size_x(&scissor_new),
              BLI_rcti_size_y(&scissor_new));

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.08f);
  /* draw grid lines here */
  for (int i = 1; i <= HISTOGRAM_TOT_GRID_LINES; i++) {
    const float fac = float(i) / float(HISTOGRAM_TOT_GRID_LINES);

    /* so we can tell the 1.0 color point */
    if (i == HISTOGRAM_TOT_GRID_LINES) {
      immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    }

    immBegin(GPU_PRIM_LINES, 4);

    immVertex2f(pos, rect.xmin, rect.ymin + fac * h);
    immVertex2f(pos, rect.xmax, rect.ymin + fac * h);

    immVertex2f(pos, rect.xmin + fac * w, rect.ymin);
    immVertex2f(pos, rect.xmin + fac * w, rect.ymax);

    immEnd();
  }

  if (hist->mode == HISTO_MODE_LUMA) {
    histogram_draw_one(
        1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_luma, res, is_line, pos);
  }
  else if (hist->mode == HISTO_MODE_ALPHA) {
    histogram_draw_one(
        1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_a, res, is_line, pos);
  }
  else {
    if (ELEM(hist->mode, HISTO_MODE_RGB, HISTO_MODE_R)) {
      histogram_draw_one(
          1.0, 0.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_r, res, is_line, pos);
    }
    if (ELEM(hist->mode, HISTO_MODE_RGB, HISTO_MODE_G)) {
      histogram_draw_one(
          0.0, 1.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_g, res, is_line, pos);
    }
    if (ELEM(hist->mode, HISTO_MODE_RGB, HISTO_MODE_B)) {
      histogram_draw_one(
          0.0, 0.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_b, res, is_line, pos);
    }
  }

  immUnbindProgram();

  /* Restore scissor test. */
  GPU_scissor(UNPACK4(scissor));

  /* outline */
  draw_scope_end(&rect);
}

#undef HISTOGRAM_TOT_GRID_LINES

static void waveform_draw_one(const float *waveform, int waveform_num, const float col[3])
{
  BLI_assert_msg(
      !immIsShaderBound(),
      "It is not allowed to draw a batch when immediate mode has a shader bound. It will "
      "use the incorrect shader and is hard to discover.");
  GPUVertFormat format = {0};
  const uint pos_id = GPU_vertformat_attr_add(
      &format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, waveform_num);

  GPU_vertbuf_attr_fill(vbo, pos_id, waveform);

  /* TODO: store the #blender::gpu::Batch inside the scope. */
  blender::gpu::Batch *batch = GPU_batch_create_ex(
      GPU_PRIM_POINTS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
  GPU_batch_uniform_4f(batch, "color", col[0], col[1], col[2], 1.0f);
  GPU_batch_uniform_1f(batch, "size", 1.0f);
  GPU_batch_draw(batch);

  GPU_batch_discard(batch);
}

struct WaveformColorVertex {
  blender::float2 pos;
  blender::float4 color;
};
static_assert(sizeof(WaveformColorVertex) == 24);

static void waveform_draw_rgb(const float *waveform,
                              int waveform_num,
                              const float *col,
                              float alpha)
{
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  GPU_vertformat_attr_add(&format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);

  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);

  GPU_vertbuf_data_alloc(*vbo, waveform_num);
  WaveformColorVertex *data = vbo->data<WaveformColorVertex>().data();
  for (int i = 0; i < waveform_num; i++) {
    memcpy(reinterpret_cast<void *>(&data->pos), waveform, sizeof(data->pos));
    memcpy(reinterpret_cast<void *>(&data->color), col, sizeof(float) * 3);
    data->color.w = alpha;
    waveform += 2;
    col += 3;
    data++;
  }
  GPU_vertbuf_tag_dirty(vbo);
  GPU_vertbuf_use(vbo);

  blender::gpu::Batch *batch = GPU_batch_create_ex(
      GPU_PRIM_POINTS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_POINT_FLAT_COLOR);
  GPU_batch_uniform_1f(batch, "size", 1.0f);
  GPU_batch_draw(batch);
  GPU_batch_discard(batch);
}

static void circle_draw_rgb(float *points, int tot_points, const float *col, GPUPrimType prim)
{
  GPUVertFormat format = {0};
  const uint pos_id = GPU_vertformat_attr_add(
      &format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  const uint col_id = GPU_vertformat_attr_add(
      &format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);

  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);

  GPU_vertbuf_data_alloc(*vbo, tot_points);
  GPU_vertbuf_attr_fill(vbo, pos_id, points);
  GPU_vertbuf_attr_fill(vbo, col_id, col);

  blender::gpu::Batch *batch = GPU_batch_create_ex(prim, vbo, nullptr, GPU_BATCH_OWNS_VBO);

  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_SMOOTH_COLOR);
  GPU_batch_draw(batch);
  GPU_batch_discard(batch);
}

void ui_draw_but_WAVEFORM(ARegion *region,
                          uiBut *but,
                          const uiWidgetColors * /*wcol*/,
                          const rcti *recti)
{
  Scopes *scopes = (Scopes *)but->poin;
  int scissor[4];
  float colors[3][3];
  const float colorsycc[3][3] = {{1, 0, 1}, {1, 1, 0}, {0, 1, 1}};
  /* Colors pre-multiplied by alpha for speed up. */
  float colors_alpha[3][3], colorsycc_alpha[3][3];
  float min, max;

  if (scopes == nullptr) {
    return;
  }

  rctf rect{};
  rect.xmin = float(recti->xmin + 1);
  rect.xmax = float(recti->xmax - 1);
  rect.ymin = float(recti->ymin + 1);
  rect.ymax = float(recti->ymax - 1);

  if (scopes->wavefrm_yfac < 0.5f) {
    scopes->wavefrm_yfac = 0.98f;
  }
  const float w = BLI_rctf_size_x(&rect) - 7;
  const float h = BLI_rctf_size_y(&rect) * scopes->wavefrm_yfac;
  const float yofs = rect.ymin + (BLI_rctf_size_y(&rect) - h) * 0.5f;
  const float w3 = w / 3.0f;

  /* log scale for alpha */
  const float alpha = scopes->wavefrm_alpha * scopes->wavefrm_alpha;

  unit_m3(colors);

  for (int c = 0; c < 3; c++) {
    for (int i = 0; i < 3; i++) {
      colors_alpha[c][i] = colors[c][i] * alpha;
      colorsycc_alpha[c][i] = colorsycc[c][i] * alpha;
    }
  }

  /* Flush text cache before changing scissors. */
  BLF_batch_draw_flush();

  GPU_blend(GPU_BLEND_ALPHA);

  float color[4];
  UI_GetThemeColor4fv(TH_PREVIEW_BACK, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  rctf back_rect{};
  back_rect.xmin = rect.xmin - 1.0f;
  back_rect.xmax = rect.xmax + 1.0f;
  back_rect.ymin = rect.ymin - 1.0f;
  back_rect.ymax = rect.ymax + 1.0f;
  UI_draw_roundbox_4fv(&back_rect, true, 3.0f, color);

  /* need scissor test, waveform can draw outside of boundary */
  GPU_scissor_get(scissor);
  rcti scissor_new{};
  scissor_new.xmin = rect.xmin;
  scissor_new.ymin = rect.ymin;
  scissor_new.xmax = rect.xmax;
  scissor_new.ymax = rect.ymax;
  const rcti scissor_region = {0, region->winx, 0, region->winy};
  BLI_rcti_isect(&scissor_new, &scissor_region, &scissor_new);
  GPU_scissor(scissor_new.xmin,
              scissor_new.ymin,
              BLI_rcti_size_x(&scissor_new),
              BLI_rcti_size_y(&scissor_new));

  /* draw scale numbers first before binding any shader */
  for (int i = 0; i < 6; i++) {
    char str[4];
    SNPRINTF_UTF8(str, "%-3d", i * 20);
    str[3] = '\0';
    BLF_color4f(BLF_default(), 1.0f, 1.0f, 1.0f, 0.08f);
    BLF_draw_default(rect.xmin + 1, yofs - 5 + (i * 0.2f) * h, 0, str, sizeof(str) - 1);
  }

  /* Flush text cache before drawing things on top. */
  BLF_batch_draw_flush();

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.08f);

  /* draw grid lines here */
  immBegin(GPU_PRIM_LINES, 12);

  for (int i = 0; i < 6; i++) {
    immVertex2f(pos, rect.xmin + 22, yofs + (i * 0.2f) * h);
    immVertex2f(pos, rect.xmax + 1, yofs + (i * 0.2f) * h);
  }

  immEnd();

  /* 3 vertical separation */
  if (scopes->wavefrm_mode != SCOPES_WAVEFRM_LUMA) {
    immBegin(GPU_PRIM_LINES, 4);

    for (int i = 1; i < 3; i++) {
      immVertex2f(pos, rect.xmin + i * w3, rect.ymin);
      immVertex2f(pos, rect.xmin + i * w3, rect.ymax);
    }

    immEnd();
  }

  /* separate min max zone on the right */
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, rect.xmin + w, rect.ymin);
  immVertex2f(pos, rect.xmin + w, rect.ymax);
  immEnd();

  /* 16-235-240 level in case of ITU-R BT601/709 */
  immUniformColor4f(1.0f, 0.4f, 0.0f, 0.2f);
  if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_YCC_601, SCOPES_WAVEFRM_YCC_709)) {
    immBegin(GPU_PRIM_LINES, 8);

    immVertex2f(pos, rect.xmin + 22, yofs + h * 16.0f / 255.0f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 16.0f / 255.0f);

    immVertex2f(pos, rect.xmin + 22, yofs + h * 235.0f / 255.0f);
    immVertex2f(pos, rect.xmin + w3, yofs + h * 235.0f / 255.0f);

    immVertex2f(pos, rect.xmin + 3 * w3, yofs + h * 235.0f / 255.0f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 235.0f / 255.0f);

    immVertex2f(pos, rect.xmin + w3, yofs + h * 240.0f / 255.0f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 240.0f / 255.0f);

    immEnd();
  }
  /* 7.5 IRE black point level for NTSC */
  if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA) {
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, rect.xmin, yofs + h * 0.075f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 0.075f);
    immEnd();
  }

  if (scopes->ok && scopes->waveform_1 != nullptr) {
    GPU_blend(GPU_BLEND_ADDITIVE);
    GPU_point_size(1.0);

    /* LUMA (1 channel) */
    if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA) {
      const float col[3] = {alpha, alpha, alpha};

      GPU_matrix_push();
      GPU_matrix_translate_2f(rect.xmin, yofs);
      GPU_matrix_scale_2f(w, h);

      immUnbindProgram();
      waveform_draw_one(scopes->waveform_1, scopes->waveform_tot, col);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      GPU_matrix_pop();

      /* min max */
      immUniformColor3f(0.5f, 0.5f, 0.5f);
      min = yofs + scopes->minmax[0][0] * h;
      max = yofs + scopes->minmax[0][1] * h;
      CLAMP(min, rect.ymin, rect.ymax);
      CLAMP(max, rect.ymin, rect.ymax);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, rect.xmax - 3, min);
      immVertex2f(pos, rect.xmax - 3, max);
      immEnd();
    }
    /* RGB (3 channel) */
    else if (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB) {
      GPU_matrix_push();
      GPU_matrix_translate_2f(rect.xmin, yofs);
      GPU_matrix_scale_2f(w, h);
      immUnbindProgram();
      waveform_draw_one(scopes->waveform_1, scopes->waveform_tot, colors_alpha[0]);
      waveform_draw_one(scopes->waveform_2, scopes->waveform_tot, colors_alpha[1]);
      waveform_draw_one(scopes->waveform_3, scopes->waveform_tot, colors_alpha[2]);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      GPU_matrix_pop();
    }
    /* PARADE / YCC (3 channels) */
    else if (ELEM(scopes->wavefrm_mode,
                  SCOPES_WAVEFRM_RGB_PARADE,
                  SCOPES_WAVEFRM_YCC_601,
                  SCOPES_WAVEFRM_YCC_709,
                  SCOPES_WAVEFRM_YCC_JPEG))
    {
      const int rgb = (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB_PARADE);
      immUnbindProgram();
      GPU_matrix_push();
      GPU_matrix_translate_2f(rect.xmin, yofs);
      GPU_matrix_scale_2f(w3, h);

      waveform_draw_one(
          scopes->waveform_1, scopes->waveform_tot, (rgb) ? colors_alpha[0] : colorsycc_alpha[0]);

      GPU_matrix_translate_2f(1.0f, 0.0f);
      waveform_draw_one(
          scopes->waveform_2, scopes->waveform_tot, (rgb) ? colors_alpha[1] : colorsycc_alpha[1]);

      GPU_matrix_translate_2f(1.0f, 0.0f);
      waveform_draw_one(
          scopes->waveform_3, scopes->waveform_tot, (rgb) ? colors_alpha[2] : colorsycc_alpha[2]);

      GPU_matrix_pop();
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    }

    /* min max */
    if (scopes->wavefrm_mode != SCOPES_WAVEFRM_LUMA) {
      for (int c = 0; c < 3; c++) {
        if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_RGB_PARADE, SCOPES_WAVEFRM_RGB)) {
          immUniformColor3f(colors[c][0] * 0.75f, colors[c][1] * 0.75f, colors[c][2] * 0.75f);
        }
        else {
          immUniformColor3f(
              colorsycc[c][0] * 0.75f, colorsycc[c][1] * 0.75f, colorsycc[c][2] * 0.75f);
        }
        min = yofs + scopes->minmax[c][0] * h;
        max = yofs + scopes->minmax[c][1] * h;
        CLAMP(min, rect.ymin, rect.ymax);
        CLAMP(max, rect.ymin, rect.ymax);

        immBegin(GPU_PRIM_LINES, 2);
        immVertex2f(pos, rect.xmin + w + 2 + c * 2, min);
        immVertex2f(pos, rect.xmin + w + 2 + c * 2, max);
        immEnd();
      }
    }
  }

  immUnbindProgram();

  /* Restore scissor test. */
  GPU_scissor(UNPACK4(scissor));

  /* outline */
  draw_scope_end(&rect);

  GPU_blend(GPU_BLEND_NONE);
}

static float polar_to_x(float center, float diam, float ampli, float angle)
{
  return center + diam * ampli * cosf(angle);
}

static float polar_to_y(float center, float diam, float ampli, float angle)
{
  return center + diam * ampli * sinf(angle);
}

static void vectorscope_draw_target(
    uint pos, float centerx, float centery, float diam, const float colf[3], char label)
{
  float y, u, v;
  float tangle = 0.0f, tampli;
  float dangle, dampli;
  const char labelstr[2] = {label, '\0'};

  rgb_to_yuv(colf[0], colf[1], colf[2], &y, &u, &v, BLI_YUV_ITU_BT709);
  u *= SCOPES_VEC_U_SCALE;
  v *= SCOPES_VEC_V_SCALE;

  if (u > 0 && v >= 0) {
    tangle = atanf(v / u);
  }
  else if (u > 0 && v < 0) {
    tangle = atanf(v / u) + 2.0f * float(M_PI);
  }
  else if (u < 0) {
    tangle = atanf(v / u) + float(M_PI);
  }
  else if (u == 0 && v > 0.0f) {
    tangle = M_PI_2;
  }
  else if (u == 0 && v < 0.0f) {
    tangle = -M_PI_2;
  }
  tampli = sqrtf(u * u + v * v);

  /* small target vary by 2.5 degree and 2.5 IRE unit */
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.12f);
  dangle = DEG2RADF(2.5f);
  dampli = 2.5f / 200.0f;
  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle + dangle),
              polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle + dangle),
              polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle - dangle),
              polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle - dangle),
              polar_to_y(centery, diam, tampli + dampli, tangle - dangle));

  /* draw color letter as text */
  BLF_color4f(BLF_default(), 1.0f, 1.0f, 1.0f, 0.3f);
  BLF_draw_default(polar_to_x(centerx, diam, tampli, tangle) + 5,
                   polar_to_y(centery, diam, tampli, tangle),
                   0,
                   labelstr,
                   strlen(labelstr));

  immEnd();
}

void ui_draw_but_VECTORSCOPE(ARegion *region,
                             uiBut *but,
                             const uiWidgetColors * /*wcol*/,
                             const rcti *recti)
{
  const float skin_rad = DEG2RADF(123.0f); /* angle in radians of the skin tone line */
  const Scopes *scopes = (const Scopes *)but->poin;

  const float colors[6][3] = {
      {0.75, 0.0, 0.0},  /* Red */
      {0.75, 0.75, 0.0}, /* Yellow */
      {0.0, 0.75, 0.0},  /* Green */
      {0.0, 0.75, 0.75}, /* Cyan */
      {0.0, 0.0, 0.75},  /* Blue */
      {0.75, 0.0, 0.75}, /* Magenta */
  };

  const char color_names[] = {'R', 'Y', 'G', 'C', 'B', 'M'};

  rctf rect{};
  rect.xmin = float(recti->xmin + 1);
  rect.xmax = float(recti->xmax - 1);
  rect.ymin = float(recti->ymin + 1);
  rect.ymax = float(recti->ymax - 1);

  const float w = BLI_rctf_size_x(&rect);
  const float h = BLI_rctf_size_y(&rect);
  const float centerx = rect.xmin + w * 0.5f;
  const float centery = rect.ymin + h * 0.5f;
  const float diam = ((w < h) ? w : h) * 0.9f;

  const float alpha = scopes->vecscope_alpha;

  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);

  float color[4];
  UI_GetThemeColor4fv(TH_PREVIEW_BACK, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  rctf back_rect{};
  back_rect.xmin = rect.xmin - 1;
  back_rect.xmax = rect.xmax + 1;
  back_rect.ymin = rect.ymin - 1;
  back_rect.ymax = rect.ymax + 1;
  UI_draw_roundbox_4fv(&back_rect, true, 3.0f, color);

  /* need scissor test, vectorscope can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get(scissor);
  rcti scissor_new{};
  scissor_new.xmin = rect.xmin;
  scissor_new.ymin = rect.ymin;
  scissor_new.xmax = rect.xmax;
  scissor_new.ymax = rect.ymax;
  const rcti scissor_region = {0, region->winx, 0, region->winy};
  BLI_rcti_isect(&scissor_new, &scissor_region, &scissor_new);
  GPU_scissor(scissor_new.xmin,
              scissor_new.ymin,
              BLI_rcti_size_x(&scissor_new),
              BLI_rcti_size_y(&scissor_new));

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  const int increment = 6;
  const int tot_points = int(360 / increment);
  const float r = 0.5f;
  float step = 360.0f / (tot_points - 1);

  float circle_fill_points[(tot_points * 2) + 2];
  float circle_fill_vertex_colors[(tot_points * 4) + 4];

  /* draw filled RGB circle for background, only for LUMA mode */
  if (scopes->vecscope_mode == SCOPES_VECSCOPE_LUMA) {
    /* Initialize center point and color */
    circle_fill_points[0] = centerx;
    circle_fill_points[1] = centery;
    circle_fill_vertex_colors[0] = 0.2f;
    circle_fill_vertex_colors[1] = 0.2f;
    circle_fill_vertex_colors[2] = 0.2f;
    circle_fill_vertex_colors[3] = 0.8f;

    for (int i = 0; i < tot_points; i++) {
      float angle = step * i;
      const float a = DEG2RADF(angle);

      const float x = polar_to_x(centerx, diam, r, a);
      const float y = polar_to_y(centery, diam, r, a);

      const float u = (x - centerx) / diam / SCOPES_VEC_U_SCALE;
      const float v = (y - centery) / diam / SCOPES_VEC_V_SCALE;

      circle_fill_points[(i + 1) * 2] = x;
      circle_fill_points[(i + 1) * 2 + 1] = y;

      float r, g, b;
      yuv_to_rgb(0.5f, u, v, &r, &g, &b, BLI_YUV_ITU_BT709);

      circle_fill_vertex_colors[(i + 1) * 4] = r * 0.2f;
      circle_fill_vertex_colors[(i + 1) * 4 + 1] = g * 0.2f;
      circle_fill_vertex_colors[(i + 1) * 4 + 2] = b * 0.2f;
      circle_fill_vertex_colors[(i + 1) * 4 + 3] = 0.8f;
    }

    GPU_blend(GPU_BLEND_ALPHA);
    circle_draw_rgb(
        circle_fill_points, tot_points + 1, circle_fill_vertex_colors, GPU_PRIM_TRI_FAN);
  }
  /* draw filled Gray circle for background, only for RGB mode */
  else if (scopes->vecscope_mode == SCOPES_VECSCOPE_RGB) {
    GPU_blend(GPU_BLEND_NONE);
    immBegin(GPU_PRIM_TRI_FAN, tot_points + 2);
    immUniformColor3f(0.16f, 0.16f, 0.16f);
    immVertex2f(pos, centerx, centery);

    for (int i = 0; i <= 360; i += increment) {
      const float a = DEG2RADF(float(i));
      immVertex2f(pos, polar_to_x(centerx, diam, r, a), polar_to_y(centery, diam, r, a));
    }
    immEnd();
  }

  /* draw RGB ring */
  float circle_points[(tot_points * 2) + 3] = {};
  float circle_vertex_colors[(tot_points * 4) + 5] = {};

  for (int i = 0; i < tot_points; i++) {
    float angle = step * i;
    const float a = DEG2RADF(angle);

    const float x = polar_to_x(centerx, diam, 0.5f, a);
    const float y = polar_to_y(centery, diam, 0.5f, a);
    circle_points[i * 2] = x;
    circle_points[i * 2 + 1] = y;

    const float u = (x - centerx) / diam / SCOPES_VEC_U_SCALE;
    const float v = (y - centery) / diam / SCOPES_VEC_V_SCALE;
    float r, g, b;
    yuv_to_rgb(0.5f, u, v, &r, &g, &b, BLI_YUV_ITU_BT709);

    circle_vertex_colors[i * 4] = r;
    circle_vertex_colors[i * 4 + 1] = g;
    circle_vertex_colors[i * 4 + 2] = b;
    circle_vertex_colors[i * 4 + 3] = 0.8f;
  }

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_width(2.5f);
  circle_draw_rgb(circle_points, tot_points, circle_vertex_colors, GPU_PRIM_LINE_LOOP);
  GPU_line_width(1.5f);

  /* inner circles */
  GPU_blend(GPU_BLEND_ADDITIVE);
  for (int j = 0; j < 4; j++) {
    float inner_circle_points[(tot_points * 2) + 3] = {};
    float inner_circle_colors[(tot_points * 4) + 5] = {};
    const float r = (j + 1) * 0.1f;

    for (int i = 0; i < tot_points; i++) {
      float angle = step * i;
      const float a = DEG2RADF(angle);

      inner_circle_points[i * 2] = polar_to_x(centerx, diam, r, a);
      inner_circle_points[i * 2 + 1] = polar_to_y(centery, diam, r, a);

      inner_circle_colors[i * 4] = 0.1f;
      inner_circle_colors[i * 4 + 1] = 0.1f;
      inner_circle_colors[i * 4 + 2] = 0.1f;
      inner_circle_colors[i * 4 + 3] = 0.8f;
    }
    circle_draw_rgb(inner_circle_points, tot_points, inner_circle_colors, GPU_PRIM_LINE_LOOP);
  }

  /* draw grid elements */
  /* cross */
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.1f);
  immBegin(GPU_PRIM_LINES, 4);

  immVertex2f(pos, centerx - (diam * 0.5f) - 5, centery);
  immVertex2f(pos, centerx + (diam * 0.5f) + 5, centery);

  immVertex2f(pos, centerx, centery - (diam * 0.5f) - 5);
  immVertex2f(pos, centerx, centery + (diam * 0.5f) + 5);

  immEnd();

  /* skin tone line */
  GPU_blend(GPU_BLEND_ADDITIVE);
  immUniformColor3f(0.25f, 0.25f, 0.25f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(
      pos, polar_to_x(centerx, diam, 0.5f, skin_rad), polar_to_y(centery, diam, 0.5f, skin_rad));
  immVertex2f(
      pos, polar_to_x(centerx, diam, 0.1f, skin_rad), polar_to_y(centery, diam, 0.1f, skin_rad));
  immEnd();

  /* saturation points */
  for (int i = 0; i < 6; i++) {
    vectorscope_draw_target(pos, centerx, centery, diam, colors[i], color_names[i]);
  }

  if (scopes->ok && scopes->vecscope != nullptr) {
    /* pixel point cloud */
    GPU_point_size(1.0);

    GPU_matrix_push();
    GPU_matrix_translate_2f(centerx, centery);
    GPU_matrix_scale_1f(diam);

    const float col[3] = {alpha, alpha, alpha};
    if (scopes->vecscope_mode == SCOPES_VECSCOPE_RGB) {
      GPU_blend(GPU_BLEND_ALPHA);
      waveform_draw_rgb(scopes->vecscope, scopes->waveform_tot, scopes->vecscope_rgb, alpha);
    }
    else if (scopes->vecscope_mode == SCOPES_VECSCOPE_LUMA) {
      GPU_blend(GPU_BLEND_ADDITIVE);
      immUnbindProgram();
      waveform_draw_one(scopes->vecscope, scopes->waveform_tot, col);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    }

    GPU_matrix_pop();
  }

  immUnbindProgram();

  /* Restore scissor test. */
  GPU_scissor(UNPACK4(scissor));
  /* outline */
  draw_scope_end(&rect);

  GPU_blend(GPU_BLEND_NONE);
}

static void ui_draw_colorband_handle_tri(uint pos, float x1, float y1, float halfwidth)
{
  /* Half-width equals height for better AA with 45 degree slope. */
  immBegin(GPU_PRIM_TRIS, 3);
  immVertex2f(pos, x1 + halfwidth, y1);
  immVertex2f(pos, x1, y1 + halfwidth);
  immVertex2f(pos, x1 - halfwidth, y1);
  immEnd();
}

static void ui_draw_colorband_handle_box(uint pos, float x1, float y1, float x2, float y2)
{
  immBegin(GPU_PRIM_TRI_STRIP, 4);
  immVertex2f(pos, x2, y1);
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x1, y2);
  immEnd();
}

static void ui_draw_colorband_handle(uint shdr_pos,
                                     const rcti *rect,
                                     float x,
                                     const float rgb[3],
                                     const ColorManagedDisplay *display,
                                     bool active)
{
  const float sizey = BLI_rcti_size_y(rect);
  float colf[3] = {UNPACK3(rgb)};

  const float half_width = sizey / 3.5f;
  const float height = half_width * 1.4f;

  float y1 = rect->ymin;
  const float y2 = rect->ymax;

  /* align to pixels */
  x = floorf(x);

  GPU_blend(GPU_BLEND_ALPHA);

  /* Allow the lines to decrease as we get really small. */
  float line_width = std::max(std::min(U.pixelsize / 5.0f * fabs(half_width - 4.0f), U.pixelsize),
                              0.5f);

  /* Make things transparent as we get tiny. */
  uchar alpha = std::min(int(fabs(half_width - 2.0f) * 50.0f), 255);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);
  immUniform1i("colors_len", 2); /* "advanced" mode */
  if (active) {
    immUniform4f("color", 1.0f, 1.0f, 1.0f, alpha / 255.0f);
    immUniform4f("color2", 0.0f, 0.0f, 0.0f, alpha / 255.0f);
  }
  else {
    immUniform4f("color", 0.7f, 0.7f, 0.7f, alpha / 255.0f);
    immUniform4f("color2", 0.4f, 0.4f, 0.4f, alpha / 255.0f);
  }
  immUniform1f("dash_width", sizey / 6.0f / UI_SCALE_FAC);
  immUniform1f("udash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(shdr_pos, x, y1);
  immVertex2f(shdr_pos, x, y2);
  immEnd();

  immUnbindProgram();

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* shift handle down */
  y1 -= half_width / 2.0f;

  /* Black outline around the lower box. */
  immUniformColor4ub(0, 0, 0, alpha);

  ui_draw_colorband_handle_box(shdr_pos,
                               x - half_width - line_width,
                               y1 - line_width,
                               x + half_width + line_width,
                               y1 + height);

  /* Grey box, inset by line width. */
  immUniformColor4ub(128, 128, 128, alpha);
  ui_draw_colorband_handle_box(shdr_pos, x - half_width, y1, x + half_width, y1 + height);

  if (display) {
    IMB_colormanagement_scene_linear_to_display_v3(colf, display);
  }

  /* Color value, inset by another line width. */
  immUniformColor3fvAlpha(colf, alpha);
  ui_draw_colorband_handle_box(shdr_pos,
                               x - (half_width - line_width),
                               y1 + line_width,
                               x + (half_width - line_width),
                               y1 + height - line_width);

  /* Black outline around the top triangle. */
  immUniformColor4ub(0, 0, 0, alpha);
  ui_draw_colorband_handle_tri(shdr_pos, x, y1 + height, half_width + line_width);

  GPU_polygon_smooth(true);

  /* Triangle main color. */
  if (active) {
    immUniformColor4ub(220, 220, 220, alpha);
  }
  else {
    immUniformColor4ub(96, 96, 96, alpha);
  }
  ui_draw_colorband_handle_tri(shdr_pos, x, y1 + height, half_width - (0.5f * line_width));

  immUnbindProgram();

  GPU_polygon_smooth(false);
  GPU_blend(GPU_BLEND_NONE);
}

void ui_draw_but_COLORBAND(uiBut *but, const uiWidgetColors *wcol, const rcti *rect)
{
  const ColorManagedDisplay *display = ui_block_cm_display_get(but->block);
  uint pos_id, col_id;

  uiButColorBand *but_coba = (uiButColorBand *)but;
  ColorBand *coba = (but_coba->edit_coba == nullptr) ? (ColorBand *)but->poin :
                                                       but_coba->edit_coba;

  if (coba == nullptr) {
    return;
  }

  const float x1 = rect->xmin + U.pixelsize;
  const float sizex = rect->xmax - x1 - U.pixelsize;
  const float y1 = rect->ymin + U.pixelsize;
  const float sizey = rect->ymax - y1 - U.pixelsize;
  const float sizey_solid = sizey * 0.25f;

  /* exit early if too narrow */
  if (sizex <= 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);

  /* Line width outline. */
  GPUVertFormat *format = immVertexFormat();
  pos_id = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4ubv(wcol->outline);
  immBegin(GPU_PRIM_TRI_STRIP, 4);
  immVertex2f(pos_id, rect->xmin, rect->ymin);
  immVertex2f(pos_id, rect->xmax, rect->ymin);
  immVertex2f(pos_id, rect->xmin, rect->ymax);
  immVertex2f(pos_id, rect->xmax, rect->ymax);
  immEnd();
  immUnbindProgram();

  /* Drawing the checkerboard. */
  immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);
  const float checker_dark = UI_ALPHA_CHECKER_DARK / 255.0f;
  const float checker_light = UI_ALPHA_CHECKER_LIGHT / 255.0f;
  immUniform4f("color1", checker_dark, checker_dark, checker_dark, 1.0f);
  immUniform4f("color2", checker_light, checker_light, checker_light, 1.0f);
  immUniform1i("size", 8);
  immRectf(pos_id, x1, y1, x1 + sizex, y1 + sizey);
  immUnbindProgram();

  /* New format */
  format = immVertexFormat();
  pos_id = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  col_id = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

  CBData *cbd = coba->data;

  float v1[2], v2[2];
  float colf[4] = {0, 0, 0, 0}; /* initialize in case the colorband isn't valid */

  v1[1] = y1 + sizey_solid;
  v2[1] = y1 + sizey;

  immBegin(GPU_PRIM_TRI_STRIP, (sizex + 1) * 2);
  for (int a = 0; a <= sizex; a++) {
    const float pos = float(a) / sizex;
    BKE_colorband_evaluate(coba, pos, colf);
    if (display) {
      IMB_colormanagement_scene_linear_to_display_v3(colf, display);
    }

    v1[0] = v2[0] = x1 + a;

    immAttr4fv(col_id, colf);
    immVertex2fv(pos_id, v1);
    immVertex2fv(pos_id, v2);
  }
  immEnd();

  /* layer: color ramp without alpha for reference when manipulating ramp properties */
  v1[1] = y1;
  v2[1] = y1 + sizey_solid;

  immBegin(GPU_PRIM_TRI_STRIP, (sizex + 1) * 2);
  for (int a = 0; a <= sizex; a++) {
    const float pos = float(a) / sizex;
    BKE_colorband_evaluate(coba, pos, colf);
    if (display) {
      IMB_colormanagement_scene_linear_to_display_v3(colf, display);
    }

    v1[0] = v2[0] = x1 + a;

    immAttr4f(col_id, colf[0], colf[1], colf[2], 1.0f);
    immVertex2fv(pos_id, v1);
    immVertex2fv(pos_id, v2);
  }
  immEnd();

  immUnbindProgram();

  /* New format */
  format = immVertexFormat();
  pos_id = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  /* layer: draw handles */
  for (int a = 0; a < coba->tot; a++, cbd++) {
    if (a != coba->cur) {
      const float pos = x1 + cbd->pos * (sizex - 1) + 1;
      ui_draw_colorband_handle(pos_id, rect, pos, &cbd->r, display, false);
    }
  }

  /* layer: active handle */
  if (coba->tot != 0) {
    cbd = &coba->data[coba->cur];
    const float pos = x1 + cbd->pos * (sizex - 1) + 1;
    ui_draw_colorband_handle(pos_id, rect, pos, &cbd->r, display, true);
  }
}

void ui_draw_but_UNITVEC(uiBut *but,
                         const uiWidgetColors *wcol,
                         const rcti *rect,
                         const float radius)
{
  /* sphere color */
  const float diffuse[3] = {1.0f, 1.0f, 1.0f};
  float light[3];
  const float size = 0.5f * min_ff(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect));

  /* backdrop */
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  rctf box_rect{};
  box_rect.xmin = rect->xmin;
  box_rect.xmax = rect->xmax;
  box_rect.ymin = rect->ymin;
  box_rect.ymax = rect->ymax;
  UI_draw_roundbox_3ub_alpha(&box_rect, true, radius, wcol->inner, 255);

  GPU_face_culling(GPU_CULL_BACK);

  /* setup lights */
  ui_but_v3_get(but, light);

  /* transform to button */
  GPU_matrix_push();

  const bool use_project_matrix = (size >= -GPU_MATRIX_ORTHO_CLIP_NEAR_DEFAULT);
  if (use_project_matrix) {
    GPU_matrix_push_projection();
    GPU_matrix_ortho_set_z(-size, size);
  }

  GPU_matrix_translate_2f(rect->xmin + 0.5f * BLI_rcti_size_x(rect),
                          rect->ymin + 0.5f * BLI_rcti_size_y(rect));
  GPU_matrix_scale_1f(size);

  blender::gpu::Batch *sphere = GPU_batch_preset_sphere(2);
  SimpleLightingData simple_lighting_data;
  copy_v4_fl4(simple_lighting_data.l_color, diffuse[0], diffuse[1], diffuse[2], 1.0f);
  copy_v3_v3(simple_lighting_data.light, light);
  blender::gpu::UniformBuf *ubo = GPU_uniformbuf_create_ex(
      sizeof(SimpleLightingData), &simple_lighting_data, __func__);

  GPU_batch_program_set_builtin(sphere, GPU_SHADER_SIMPLE_LIGHTING);
  GPU_batch_uniformbuf_bind(sphere, "simple_lighting_data", ubo);
  GPU_batch_draw(sphere);
  GPU_uniformbuf_free(ubo);

  /* Restore. */
  GPU_face_culling(GPU_CULL_NONE);

  /* AA circle */
  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor3ubv(wcol->inner);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, 1.0f, 32);
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);

  if (use_project_matrix) {
    GPU_matrix_pop_projection();
  }

  /* matrix after circle */
  GPU_matrix_pop();

  immUnbindProgram();
}

static void ui_draw_but_curve_grid(const uint pos,
                                   const rcti *rect,
                                   const float zoom_x,
                                   const float zoom_y,
                                   const float offset_x,
                                   const float offset_y,
                                   const float step)
{
  const float start_x = (ceilf(offset_x / step) * step - offset_x) * zoom_x + rect->xmin;
  const float start_y = (ceilf(offset_y / step) * step - offset_y) * zoom_y + rect->ymin;

  const int line_count_x = ceilf((rect->xmax - start_x) / (step * zoom_x));
  const int line_count_y = ceilf((rect->ymax - start_y) / (step * zoom_y));

  if (line_count_x + line_count_y == 0) {
    return;
  }

  immBegin(GPU_PRIM_LINES, (line_count_x + line_count_y) * 2);
  for (int i = 0; i < line_count_x; i++) {
    const float x = start_x + i * step * zoom_x;
    immVertex2f(pos, x, rect->ymin);
    immVertex2f(pos, x, rect->ymax);
  }
  for (int i = 0; i < line_count_y; i++) {
    const float y = start_y + i * step * zoom_y;
    immVertex2f(pos, rect->xmin, y);
    immVertex2f(pos, rect->xmax, y);
  }
  immEnd();
}

void ui_draw_but_CURVE(ARegion *region, uiBut *but, const uiWidgetColors *wcol, const rcti *rect)
{
  uiButCurveMapping *but_cumap = (uiButCurveMapping *)but;
  CurveMapping *cumap = (but_cumap->edit_cumap == nullptr) ? (CurveMapping *)but->poin :
                                                             but_cumap->edit_cumap;

  const bool inactive = but->flag & UI_BUT_INACTIVE;
  const float fade_factor_float = inactive ? 0.33f : 1.0f;
  const uchar fade_factor_uchar = inactive ? 3 : 1;

  const float clip_size_x = BLI_rctf_size_x(&cumap->curr);
  const float clip_size_y = BLI_rctf_size_y(&cumap->curr);

  /* zero-sized curve */
  if (clip_size_x == 0.0f || clip_size_y == 0.0f) {
    return;
  }

  /* calculate offset and zoom */
  const float zoomx = (BLI_rcti_size_x(rect) - 1.0f) / clip_size_x;
  const float zoomy = (BLI_rcti_size_y(rect) - 1.0f) / clip_size_y;
  const float offsx = cumap->curr.xmin - (1.0f / zoomx);
  const float offsy = cumap->curr.ymin - (1.0f / zoomy);

  /* exit early if too narrow */
  if (zoomx <= 0.0f) {
    return;
  }

  CurveMap *cuma = &cumap->cm[cumap->cur];

  /* need scissor test, curve can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get(scissor);
  rcti scissor_new{};
  scissor_new.xmin = rect->xmin;
  scissor_new.ymin = rect->ymin;
  scissor_new.xmax = rect->xmax;
  scissor_new.ymax = rect->ymax;
  const rcti scissor_region = {0, region->winx, 0, region->winy};
  BLI_rcti_isect(&scissor_new, &scissor_region, &scissor_new);
  GPU_scissor(scissor_new.xmin,
              scissor_new.ymin,
              BLI_rcti_size_x(&scissor_new),
              BLI_rcti_size_y(&scissor_new));

  /* Do this first to not mess imm context */
  if (but_cumap->gradient_type == UI_GRAD_H) {
    /* magic trigger for curve backgrounds */
    const float col[3] = {0.0f, 0.0f, 0.0f}; /* dummy arg */

    rcti grid{};
    grid.xmin = rect->xmin + zoomx * (-offsx);
    grid.xmax = grid.xmin + zoomx;
    grid.ymin = rect->ymin + zoomy * (-offsy);
    grid.ymax = grid.ymin + zoomy;

    const ColorManagedDisplay *display = ui_block_cm_display_get(but->block);
    ui_draw_gradient(&grid, col, UI_GRAD_H, 1.0f, display);
  }

  GPU_line_width(1.0f);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* backdrop */
  float color_backdrop[4] = {0, 0, 0, 1};

  GPU_blend(GPU_BLEND_ALPHA);
  if (but_cumap->gradient_type == UI_GRAD_H) {
    /* grid, hsv uses different grid */
    ARRAY_SET_ITEMS(color_backdrop, 0, 0, 0, 48.0 / 255.0);
    immUniformColor4fv(color_backdrop);
    ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 0.1666666f);
  }
  else {
    /* Draw backdrop. */
    immUniformColor3ubvAlpha(wcol->inner, (wcol->inner[3] / fade_factor_uchar));
    immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

    /* Draw an outline around the clipped limits. */
    if (cumap->flag & CUMA_DO_CLIP) {
      format = immVertexFormat();
      pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

      immUniformColor3ubvAlpha(wcol->item, (wcol->item[3] / fade_factor_uchar));
      GPU_line_width(2.0f);
      imm_draw_box_wire_2d(pos,
                           rect->xmin + zoomx * (cumap->clipr.xmin - offsx),
                           rect->ymin + zoomy * (cumap->clipr.ymin - offsy),
                           rect->xmin + zoomx * (cumap->clipr.xmax - offsx),
                           rect->ymin + zoomy * (cumap->clipr.ymax - offsy));
      GPU_line_width(1.0f);
    }

    /* grid, every 0.25 step */
    immUniformColor3ubvAlpha(wcol->outline_sel, 64 / fade_factor_uchar);
    ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 0.25f);
    /* grid, every 1.0 step */
    immUniformColor3ubvAlpha(wcol->outline_sel, 92 / fade_factor_uchar);
    ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 1.0f);
    /* axes */
    uchar col_axis_x[3], col_axis_y[3];
    UI_GetThemeColor3ubv(TH_AXIS_X, col_axis_x);
    UI_GetThemeColor3ubv(TH_AXIS_Y, col_axis_y);

    immBegin(GPU_PRIM_LINES, 2);
    immUniformColor3ubvAlpha(col_axis_x, 128 / fade_factor_uchar);
    immVertex2f(pos, rect->xmin, rect->ymin + zoomy * (-offsy));
    immVertex2f(pos, rect->xmax, rect->ymin + zoomy * (-offsy));
    immEnd();

    immBegin(GPU_PRIM_LINES, 2);
    immUniformColor3ubvAlpha(col_axis_y, 128 / fade_factor_uchar);
    immVertex2f(pos, rect->xmin + zoomx * (-offsx), rect->ymin);
    immVertex2f(pos, rect->xmin + zoomx * (-offsx), rect->ymax);
    immEnd();
  }
  GPU_blend(GPU_BLEND_NONE);

  /* cfra option */
  /* XXX 2.48 */
#if 0
  if (cumap->flag & CUMA_DRAW_CFRA) {
    immUniformColor3ub(0x60, 0xc0, 0x40);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymin);
    immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymax);
    immEnd();
  }
#endif
  /* sample option */

  if (cumap->flag & CUMA_DRAW_SAMPLE) {
    immBegin(GPU_PRIM_LINES, 2); /* will draw one of the following 3 lines */
    if (but_cumap->gradient_type == UI_GRAD_H) {
      float tsample[3];
      float hsv[3];
      linearrgb_to_srgb_v3_v3(tsample, cumap->sample);
      rgb_to_hsv_v(tsample, hsv);
      immUniformColor3ub(240, 240, 240);

      immVertex2f(pos, rect->xmin + zoomx * (hsv[0] - offsx), rect->ymin);
      immVertex2f(pos, rect->xmin + zoomx * (hsv[0] - offsx), rect->ymax);
    }
    else if (cumap->cur == 3) {
      const float lum = IMB_colormanagement_get_luminance(cumap->sample);
      immUniformColor3ub(240, 240, 240);

      immVertex2f(pos, rect->xmin + zoomx * (lum - offsx), rect->ymin);
      immVertex2f(pos, rect->xmin + zoomx * (lum - offsx), rect->ymax);
    }
    else {
      if (cumap->cur == 0) {
        immUniformColor3ub(240, 100, 100);
      }
      else if (cumap->cur == 1) {
        immUniformColor3ub(100, 240, 100);
      }
      else {
        immUniformColor3ub(100, 100, 240);
      }

      immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymin);
      immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymax);
    }
    immEnd();
  }
  immUnbindProgram();

  if (cuma->table == nullptr) {
    BKE_curvemapping_changed(cumap, false);
  }

  CurveMapPoint *cmp = cuma->table;
  rctf line_range;

  /* First curve point. */
  if ((cumap->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
    line_range.xmin = rect->xmin;
    line_range.ymin = rect->ymin + zoomy * (cmp[0].y - offsy);
  }
  else {
    line_range.xmin = rect->xmin + zoomx * (cmp[0].x - offsx + cuma->ext_in[0]);
    line_range.ymin = rect->ymin + zoomy * (cmp[0].y - offsy + cuma->ext_in[1]);
  }
  /* Last curve point. */
  if ((cumap->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
    line_range.xmax = rect->xmax;
    line_range.ymax = rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy);
  }
  else {
    line_range.xmax = rect->xmin + zoomx * (cmp[CM_TABLE].x - offsx - cuma->ext_out[0]);
    line_range.ymax = rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy - cuma->ext_out[1]);
  }

  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Curve filled. */
  immUniformColor3ubvAlpha(wcol->item, wcol->item[3] / fade_factor_uchar);
  immBegin(GPU_PRIM_TRI_STRIP, (CM_TABLE * 2 + 2) + 4);
  immVertex2f(pos, line_range.xmin, rect->ymin);
  immVertex2f(pos, line_range.xmin, line_range.ymin);
  for (int a = 0; a <= CM_TABLE; a++) {
    const float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
    const float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
    immVertex2f(pos, fx, rect->ymin);
    immVertex2f(pos, fx, fy);
  }
  immVertex2f(pos, line_range.xmax, rect->ymin);
  immVertex2f(pos, line_range.xmax, line_range.ymax);
  immEnd();

  /* Curve line. */
  GPU_line_width(1.0f);
  immUniformColor3ubvAlpha(wcol->inner_sel, wcol->inner_sel[3] / fade_factor_uchar);
  GPU_line_smooth(true);
  immBegin(GPU_PRIM_LINE_STRIP, (CM_TABLE + 1) + 2);
  immVertex2f(pos, line_range.xmin, line_range.ymin);
  for (int a = 0; a <= CM_TABLE; a++) {
    const float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
    const float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
    immVertex2f(pos, fx, fy);
  }
  immVertex2f(pos, line_range.xmax, line_range.ymax);
  immEnd();

  /* Reset state for fill & line. */
  GPU_line_smooth(false);
  GPU_blend(GPU_BLEND_ALPHA);
  immUnbindProgram();

  /* The points, use aspect to make them visible on edges. */
  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

  GPU_program_point_size(true);

  float color_point[4], color_point_select[4], color_point_outline[4];
  rgba_uchar_to_float(color_point, wcol->text);
  rgba_uchar_to_float(color_point_select, wcol->text_sel);
  rgba_uchar_to_float(color_point_outline, wcol->inner_sel);
  color_point[3] = fade_factor_float;
  color_point_select[3] = fade_factor_float;
  color_point_outline[3] *= fade_factor_float;

  cmp = cuma->curve;
  const float point_size = max_ff(U.pixelsize * 2.0f,
                                  min_ff(UI_SCALE_FAC / but->block->aspect * 6.0f, 20.0f));

  int selected = 0;
  /* Find the total number of selected points. */
  for (int i = 0; i < cuma->totpoint; i++) {
    if (cmp[i].flag & CUMA_SELECT) {
      selected++;
    }
  }

  /* Curve widgets using a gradient background (such as Hue Correct), draw
   * an additional point in the back, forming an outline so they stand out. */
  if (but_cumap->gradient_type == UI_GRAD_H) {
    if ((cuma->totpoint - selected) > 0) {
      /* Background (outline) for unselected points. */
      immUniform4fv("color", color_point_outline);
      immUniform1f("size", point_size * 1.4f);
      immBegin(GPU_PRIM_POINTS, cuma->totpoint - selected);
      for (int a = 0; a < cuma->totpoint; a++) {
        if (cmp[a].flag & CUMA_SELECT) {
          continue;
        }
        const float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
        const float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
        immVertex2f(pos, fx, fy);
      }
      immEnd();
    }
    if (selected > 0) {
      /* Background (outline) for selected points. */
      immUniform1f("size", point_size * 1.8f);
      immBegin(GPU_PRIM_POINTS, selected);
      for (int a = 0; a < cuma->totpoint; a++) {
        if (!(cmp[a].flag & CUMA_SELECT)) {
          continue;
        }
        const float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
        const float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
        immVertex2f(pos, fx, fy);
      }
      immEnd();
    }
  }

  if ((cuma->totpoint - selected) > 0) {
    /* Unselected points. */
    immUniform1f("size", point_size);
    immUniform4fv("color", color_point);
    immBegin(GPU_PRIM_POINTS, cuma->totpoint - selected);
    for (int a = 0; a < cuma->totpoint; a++) {
      if (cmp[a].flag & CUMA_SELECT) {
        continue;
      }
      const float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
      const float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
      immVertex2f(pos, fx, fy);
    }
    immEnd();
  }

  if (selected > 0) {
    /* Selected points. */
    immUniform1f("size", point_size * 1.2f);
    immUniform4fv("color", color_point_select);
    immBegin(GPU_PRIM_POINTS, selected);
    for (int a = 0; a < cuma->totpoint; a++) {
      if (!(cmp[a].flag & CUMA_SELECT)) {
        continue;
      }
      const float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
      const float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
      immVertex2f(pos, fx, fy);
    }
    immEnd();
  }

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  /* Restore scissor-test. */
  GPU_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);

  /* outline */
  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor3ubv(wcol->outline);
  imm_draw_box_wire_2d(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  immUnbindProgram();
}

/**
 * Helper for #ui_draw_but_CURVEPROFILE. Used to tell whether to draw a control point's handles.
 */
static bool point_draw_handles(CurveProfilePoint *point)
{
  return (point->flag & PROF_SELECT &&
          (ELEM(point->h1, HD_FREE, HD_ALIGN) || ELEM(point->h2, HD_FREE, HD_ALIGN))) ||
         ELEM(point->flag, PROF_H1_SELECT, PROF_H2_SELECT);
}

void ui_draw_but_CURVEPROFILE(ARegion *region,
                              uiBut *but,
                              const uiWidgetColors *wcol,
                              const rcti *rect)
{
  float fx, fy;

  uiButCurveProfile *but_profile = (uiButCurveProfile *)but;
  CurveProfile *profile = (but_profile->edit_profile == nullptr) ? (CurveProfile *)but->poin :
                                                                   but_profile->edit_profile;

  /* Calculate offset and zoom. */
  const float zoomx = (BLI_rcti_size_x(rect) - 1.0f) / BLI_rctf_size_x(&profile->view_rect);
  const float zoomy = (BLI_rcti_size_y(rect) - 1.0f) / BLI_rctf_size_y(&profile->view_rect);
  const float offsx = profile->view_rect.xmin - (1.0f / zoomx);
  const float offsy = profile->view_rect.ymin - (1.0f / zoomy);

  /* Exit early if too narrow. */
  if (zoomx <= 0.0f) {
    return;
  }

  /* Test needed because path can draw outside of boundary. */
  int scissor[4];
  GPU_scissor_get(scissor);
  rcti scissor_new{};
  scissor_new.xmin = rect->xmin;
  scissor_new.ymin = rect->ymin;
  scissor_new.xmax = rect->xmax;
  scissor_new.ymax = rect->ymax;

  const rcti scissor_region = {0, region->winx, 0, region->winy};
  BLI_rcti_isect(&scissor_new, &scissor_region, &scissor_new);
  GPU_scissor(scissor_new.xmin,
              scissor_new.ymin,
              BLI_rcti_size_x(&scissor_new),
              BLI_rcti_size_y(&scissor_new));

  GPU_line_width(1.0f);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Draw the backdrop. */
  GPU_blend(GPU_BLEND_ALPHA);
  /* Main backdrop. */
  immUniformColor4ubv(wcol->inner);
  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  if (!(profile->flag & PROF_USE_CLIP)) {
    immUniformColor3ubvAlpha(wcol->outline_sel, 16);
    immRectf(pos,
             rect->xmin + zoomx * (profile->clip_rect.xmin - offsx),
             rect->ymin + zoomy * (profile->clip_rect.ymin - offsy),
             rect->xmin + zoomx * (profile->clip_rect.xmax - offsx),
             rect->ymin + zoomy * (profile->clip_rect.ymax - offsy));
  }

  /* 0.25 step grid. */
  immUniformColor3ubvAlpha(wcol->outline_sel, 64);
  ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 0.25f);
  /* 1.0 step grid. */
  immUniformColor3ubvAlpha(wcol->outline_sel, 92);
  ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 1.0f);
  GPU_blend(GPU_BLEND_NONE);

  /* Draw the path's fill. */
  if (profile->table == nullptr) {
    BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
  }
  CurveProfilePoint *pts = profile->table;
  /* Also add the last points on the right and bottom edges to close off the fill polygon. */
  const bool add_left_tri = profile->view_rect.xmin < 0.0f;
  const bool add_bottom_tri = profile->view_rect.ymin < 0.0f;
  int tot_points = BKE_curveprofile_table_size(profile) + 1 + add_left_tri + add_bottom_tri;
  const uint tot_triangles = tot_points - 2;

  /* Create array of the positions of the table's points. */
  float (*table_coords)[2] = static_cast<float (*)[2]>(
      MEM_mallocN(sizeof(*table_coords) * tot_points, __func__));
  for (uint i = 0; i < uint(BKE_curveprofile_table_size(profile)); i++) {
    /* Only add the points from the table here. */
    table_coords[i][0] = pts[i].x;
    table_coords[i][1] = pts[i].y;
  }
  /* Using some extra margin (-1.0f) for the coordinates used to complete the polygon
   * avoids the profile line crossing itself in some common situations, which can lead to
   * incorrect triangulation. See #841183. */
  if (add_left_tri && add_bottom_tri) {
    /* Add left side, bottom left corner, and bottom side points. */
    table_coords[tot_points - 3][0] = profile->view_rect.xmin - 1.0f;
    table_coords[tot_points - 3][1] = 1.0f;
    table_coords[tot_points - 2][0] = profile->view_rect.xmin - 1.0f;
    table_coords[tot_points - 2][1] = profile->view_rect.ymin - 1.0f;
    table_coords[tot_points - 1][0] = 1.0f;
    table_coords[tot_points - 1][1] = profile->view_rect.ymin - 1.0f;
  }
  else if (add_left_tri) {
    /* Add the left side and bottom left corner points. */
    table_coords[tot_points - 2][0] = profile->view_rect.xmin - 1.0f;
    table_coords[tot_points - 2][1] = 1.0f;
    table_coords[tot_points - 1][0] = profile->view_rect.xmin - 1.0f;
    table_coords[tot_points - 1][1] = -1.0f;
  }
  else if (add_bottom_tri) {
    /* Add the bottom side and bottom left corner points. */
    table_coords[tot_points - 2][0] = -1.0f;
    table_coords[tot_points - 2][1] = profile->view_rect.ymin - 1.0f;
    table_coords[tot_points - 1][0] = 1.0f;
    table_coords[tot_points - 1][1] = profile->view_rect.ymin - 1.0f;
  }
  else {
    /* Just add the bottom corner point. Side points would be redundant anyway. */
    table_coords[tot_points - 1][0] = -1.0f;
    table_coords[tot_points - 1][1] = -1.0f;
  }

  /* Calculate the table point indices of the triangles for the profile's fill. */
  if (tot_triangles > 0) {
    uint(*tri_indices)[3] = static_cast<uint(*)[3]>(
        MEM_mallocN(sizeof(*tri_indices) * tot_triangles, __func__));
    BLI_polyfill_calc(table_coords, tot_points, -1, tri_indices);

    /* Draw the triangles for the profile fill. */
    immUniformColor4ubv(wcol->item);
    GPU_blend(GPU_BLEND_ALPHA);
    GPU_polygon_smooth(false);
    immBegin(GPU_PRIM_TRIS, 3 * tot_triangles);
    for (uint i = 0; i < tot_triangles; i++) {
      const uint *tri = tri_indices[i];
      for (uint j = 0; j < 3; j++) {
        fx = rect->xmin + zoomx * (table_coords[tri[j]][0] - offsx);
        fy = rect->ymin + zoomy * (table_coords[tri[j]][1] - offsy);
        immVertex2f(pos, fx, fy);
      }
    }
    immEnd();
    MEM_freeN(tri_indices);
  }

  /* Draw the profile's path so the edge stands out a bit. */
  tot_points -= (add_left_tri + add_left_tri);
  const int edges_len = tot_points - 1;
  if (edges_len > 0) {
    GPU_line_width(1.0f);
    immUniformColor4ubv(wcol->inner_sel);
    GPU_line_smooth(true);
    immBegin(GPU_PRIM_LINE_STRIP, tot_points);
    for (int i = 0; i < tot_points; i++) {
      fx = rect->xmin + zoomx * (table_coords[i][0] - offsx);
      fy = rect->ymin + zoomy * (table_coords[i][1] - offsy);
      immVertex2f(pos, fx, fy);
    }
    immEnd();
  }

  MEM_SAFE_FREE(table_coords);

  /* Draw the handles for the selected control points. */
  pts = profile->path;
  const int path_len = tot_points = uint(profile->path_len);
  int selected_free_points = 0;
  for (int i = 0; i < path_len; i++) {
    if (point_draw_handles(&pts[i])) {
      selected_free_points++;
    }
  }
  /* Draw the lines to the handles from the points. */
  if (selected_free_points > 0) {
    GPU_line_width(1.0f);
    immUniformColor4ubv(wcol->inner_sel);
    GPU_line_smooth(true);
    immBegin(GPU_PRIM_LINES, selected_free_points * 4);
    float ptx, pty;
    for (int i = 0; i < path_len; i++) {
      if (point_draw_handles(&pts[i])) {
        ptx = rect->xmin + zoomx * (pts[i].x - offsx);
        pty = rect->ymin + zoomy * (pts[i].y - offsy);

        fx = rect->xmin + zoomx * (pts[i].h1_loc[0] - offsx);
        fy = rect->ymin + zoomy * (pts[i].h1_loc[1] - offsy);
        immVertex2f(pos, ptx, pty);
        immVertex2f(pos, fx, fy);

        fx = rect->xmin + zoomx * (pts[i].h2_loc[0] - offsx);
        fy = rect->ymin + zoomy * (pts[i].h2_loc[1] - offsy);
        immVertex2f(pos, ptx, pty);
        immVertex2f(pos, fx, fy);
      }
    }
    immEnd();
  }
  immUnbindProgram();

  /* New GPU instructions for control points and sampled points. */
  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

  GPU_program_point_size(true);

  float color_point[4], color_point_select[4], color_sample[4];
  rgba_uchar_to_float(color_point, wcol->text);
  color_point[3] = 1.0f;
  rgba_uchar_to_float(color_point_select, wcol->text_sel);
  color_point_select[3] = 1.0f;
  color_sample[0] = float(wcol->item[0]) / 255.0f;
  color_sample[1] = float(wcol->item[1]) / 255.0f;
  color_sample[2] = float(wcol->item[2]) / 255.0f;
  color_sample[3] = float(wcol->item[3]) / 255.0f;

  int selected = 0;
  for (int i = 0; i < path_len; i++) {
    if (pts[i].flag & PROF_SELECT) {
      selected++;
    }
  }

  /* Draw the control points. */
  GPU_line_smooth(false);
  GPU_blend(GPU_BLEND_ALPHA);
  const float point_size = max_ff(U.pixelsize * 2.0f,
                                  min_ff(UI_SCALE_FAC / but->block->aspect * 6.0f, 20.0f));

  if ((path_len - selected) > 0) {
    /* Unselected control points. */
    immUniform4fv("color", color_point);
    immUniform1f("size", point_size);
    immBegin(GPU_PRIM_POINTS, path_len - selected);
    for (int i = 0; i < path_len; i++) {
      if (pts[i].flag & PROF_SELECT) {
        continue;
      }
      fx = rect->xmin + zoomx * (pts[i].x - offsx);
      fy = rect->ymin + zoomy * (pts[i].y - offsy);
      immVertex2f(pos, fx, fy);
    }
    immEnd();
  }
  if (selected > 0) {
    /* Selected control points. */
    immUniform4fv("color", color_point_select);
    immUniform1f("size", point_size * 1.2f);
    immBegin(GPU_PRIM_POINTS, selected);
    for (int i = 0; i < path_len; i++) {
      if (!(pts[i].flag & PROF_SELECT)) {
        continue;
      }
      fx = rect->xmin + zoomx * (pts[i].x - offsx);
      fy = rect->ymin + zoomy * (pts[i].y - offsy);
      immVertex2f(pos, fx, fy);
    }
    immEnd();
  }

  /* Find the total number of selected handles. */
  selected = 0;
  for (int i = 0; i < path_len; i++) {
    if (point_draw_handles(&pts[i])) {
      if (pts[i].flag & PROF_H1_SELECT) {
        selected++;
      }
      if (pts[i].flag & PROF_H2_SELECT) {
        selected++;
      }
    }
  }

  if (((selected_free_points * 2) - selected) > 0) {
    /* Unselected handles. */
    immUniform4fv("color", color_point);
    immUniform1f("size", point_size);
    immBegin(GPU_PRIM_POINTS, (selected_free_points * 2) - selected);
    for (int i = 0; i < path_len; i++) {
      if (point_draw_handles(&pts[i])) {
        if (!(pts[i].flag & PROF_H1_SELECT)) {
          fx = rect->xmin + zoomx * (pts[i].h1_loc[0] - offsx);
          fy = rect->ymin + zoomy * (pts[i].h1_loc[1] - offsy);
          immVertex2f(pos, fx, fy);
        }
        if (!(pts[i].flag & PROF_H2_SELECT)) {
          fx = rect->xmin + zoomx * (pts[i].h2_loc[0] - offsx);
          fy = rect->ymin + zoomy * (pts[i].h2_loc[1] - offsy);
          immVertex2f(pos, fx, fy);
        }
      }
    }
    immEnd();
  }

  if (selected > 0) {
    /* Selected Handles. */
    immUniform4fv("color", color_point_select);
    immUniform1f("size", point_size * 1.2f);
    immBegin(GPU_PRIM_POINTS, selected);
    for (int i = 0; i < path_len; i++) {
      if (point_draw_handles(&pts[i])) {
        if (pts[i].flag & PROF_H1_SELECT) {
          fx = rect->xmin + zoomx * (pts[i].h1_loc[0] - offsx);
          fy = rect->ymin + zoomy * (pts[i].h1_loc[1] - offsy);
          immVertex2f(pos, fx, fy);
        }
        if (pts[i].flag & PROF_H2_SELECT) {
          fx = rect->xmin + zoomx * (pts[i].h2_loc[0] - offsx);
          fy = rect->ymin + zoomy * (pts[i].h2_loc[1] - offsy);
          immVertex2f(pos, fx, fy);
        }
      }
    }
    immEnd();
  }

  /* Draw the sampled points in addition to the control points if they have been created */
  pts = profile->segments;
  const int segments_len = uint(profile->segments_len);
  if (segments_len > 0 && pts) {
    immUniform4fv("color", color_sample);
    immUniform1f("size", point_size);
    immBegin(GPU_PRIM_POINTS, segments_len);
    for (int i = 0; i < segments_len; i++) {
      fx = rect->xmin + zoomx * (pts[i].x - offsx);
      fy = rect->ymin + zoomy * (pts[i].y - offsy);
      immVertex2f(pos, fx, fy);
    }
    immEnd();
  }
  immUnbindProgram();

  /* Restore scissor-test. */
  GPU_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);

  /* Outline */
  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor3ubv((const uchar *)wcol->outline);
  imm_draw_box_wire_2d(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

void ui_draw_but_TRACKPREVIEW(ARegion *region,
                              uiBut *but,
                              const uiWidgetColors * /*wcol*/,
                              const rcti *recti)
{
  bool ok = false;
  MovieClipScopes *scopes = (MovieClipScopes *)but->poin;

  rctf rect{};
  rect.xmin = float(recti->xmin + 1);
  rect.xmax = float(recti->xmax - 1);
  rect.ymin = float(recti->ymin + 1);
  rect.ymax = float(recti->ymax - 1);

  const int width = BLI_rctf_size_x(&rect) + 1;
  const int height = BLI_rctf_size_y(&rect);

  GPU_blend(GPU_BLEND_ALPHA);

  /* need scissor test, preview image can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get(scissor);
  rcti scissor_new{};
  scissor_new.xmin = rect.xmin;
  scissor_new.ymin = rect.ymin;
  scissor_new.xmax = rect.xmax;
  scissor_new.ymax = rect.ymax;
  const rcti scissor_region = {0, region->winx, 0, region->winy};
  BLI_rcti_isect(&scissor_new, &scissor_region, &scissor_new);
  GPU_scissor(scissor_new.xmin,
              scissor_new.ymin,
              BLI_rcti_size_x(&scissor_new),
              BLI_rcti_size_y(&scissor_new));

  if (scopes->track_disabled) {
    const float color[4] = {0.7f, 0.3f, 0.3f, 0.3f};
    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    rctf disabled_rect{};
    disabled_rect.xmin = rect.xmin - 1;
    disabled_rect.xmax = rect.xmax + 1;
    disabled_rect.ymin = rect.ymin;
    disabled_rect.ymax = rect.ymax + 1;
    UI_draw_roundbox_4fv(&disabled_rect, true, 3.0f, color);

    ok = true;
  }
  else if ((scopes->track_search) &&
           ((!scopes->track_preview) ||
            (scopes->track_preview->x != width || scopes->track_preview->y != height)))
  {
    if (scopes->track_preview) {
      IMB_freeImBuf(scopes->track_preview);
    }

    ImBuf *tmpibuf = BKE_tracking_sample_pattern(scopes->frame_width,
                                                 scopes->frame_height,
                                                 scopes->track_search,
                                                 scopes->track,
                                                 &scopes->undist_marker,
                                                 true,
                                                 scopes->use_track_mask,
                                                 width,
                                                 height,
                                                 scopes->track_pos);
    if (tmpibuf) {
      if (tmpibuf->float_buffer.data) {
        IMB_byte_from_float(tmpibuf);
      }

      if (tmpibuf->byte_buffer.data) {
        scopes->track_preview = tmpibuf;
      }
      else {
        IMB_freeImBuf(tmpibuf);
      }
    }
  }

  if (!ok && scopes->track_preview) {
    GPU_matrix_push();

    /* draw content of pattern area */
    GPU_scissor(rect.xmin, rect.ymin, scissor[2], scissor[3]);

    if (width > 0 && height > 0) {
      const ImBuf *drawibuf = scopes->track_preview;
      float col_sel[4], col_outline[4];

      if (scopes->use_track_mask) {
        const float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};
        UI_draw_roundbox_corner_set(UI_CNR_ALL);
        rctf mask_rect{};
        mask_rect.xmin = rect.xmin - 1;
        mask_rect.xmax = rect.xmax + 1;
        mask_rect.ymin = rect.ymin;
        mask_rect.ymax = rect.ymax + 1;
        UI_draw_roundbox_4fv(&mask_rect, true, 3.0f, color);
      }

      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
      immDrawPixelsTexTiled(&state,
                            rect.xmin,
                            rect.ymin + 1,
                            drawibuf->x,
                            drawibuf->y,
                            blender::gpu::TextureFormat::UNORM_8_8_8_8,
                            true,
                            drawibuf->byte_buffer.data,
                            1.0f,
                            1.0f,
                            nullptr);

      /* draw cross for pixel position */
      GPU_matrix_translate_2f(rect.xmin + scopes->track_pos[0], rect.ymin + scopes->track_pos[1]);
      GPU_scissor(rect.xmin, rect.ymin, BLI_rctf_size_x(&rect), BLI_rctf_size_y(&rect));

      GPUVertFormat *format = immVertexFormat();
      const uint pos = GPU_vertformat_attr_add(
          format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
      const uint col = GPU_vertformat_attr_add(
          format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);
      immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

      UI_GetThemeColor4fv(TH_SEL_MARKER, col_sel);
      UI_GetThemeColor4fv(TH_MARKER_OUTLINE, col_outline);

      /* Do stipple cross with geometry */
      immBegin(GPU_PRIM_LINES, 7 * 2 * 2);
      const float pos_sel[8] = {-10.0f, -7.0f, -4.0f, -1.0f, 2.0f, 5.0f, 8.0f, 11.0f};
      for (int axe = 0; axe < 2; axe++) {
        for (int i = 0; i < 7; i++) {
          const float x1 = pos_sel[i] * (1 - axe);
          const float y1 = pos_sel[i] * axe;
          const float x2 = pos_sel[i + 1] * (1 - axe);
          const float y2 = pos_sel[i + 1] * axe;

          if (i % 2 == 1) {
            immAttr4fv(col, col_sel);
          }
          else {
            immAttr4fv(col, col_outline);
          }

          immVertex2f(pos, x1, y1);
          immVertex2f(pos, x2, y2);
        }
      }
      immEnd();

      immUnbindProgram();
    }

    GPU_matrix_pop();

    ok = true;
  }

  if (!ok) {
    const float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};
    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    rctf box_rect{};
    box_rect.xmin = rect.xmin - 1;
    box_rect.xmax = rect.xmax + 1;
    box_rect.ymin = rect.ymin;
    box_rect.ymax = rect.ymax + 1;
    UI_draw_roundbox_4fv(&box_rect, true, 3.0f, color);
  }

  /* Restore scissor test. */
  GPU_scissor(UNPACK4(scissor));
  /* outline */
  draw_scope_end(&rect);

  GPU_blend(GPU_BLEND_NONE);
}

/* ****************************************************** */

void ui_draw_dropshadow(
    const rctf *rct, const float radius, const float width, const float aspect, const float alpha)
{
  if (width == 0.0f) {
    return;
  }

  /* This undoes the scale of the view for higher zoom factors to clamp the shadow size. */
  const float clamped_aspect = smoothminf(aspect, 1.0f, 0.5f);
  const float shadow_width = width * clamped_aspect;
  const float shadow_offset = min_ff(shadow_width, BLI_rctf_size_y(rct) - 2.0f * radius);

  const float inner_radius = max_ff(radius - U.pixelsize, 0.0);
  const float shadow_radius = radius + shadow_width - U.pixelsize;

  GPU_blend(GPU_BLEND_ALPHA);

  uiWidgetBaseParameters widget_params{};
  widget_params.recti.xmin = rct->xmin;
  widget_params.recti.ymin = rct->ymin;
  widget_params.recti.xmax = rct->xmax;
  widget_params.recti.ymax = rct->ymax;
  widget_params.rect.xmin = rct->xmin - shadow_width;
  widget_params.rect.ymin = rct->ymin - shadow_width;
  widget_params.rect.xmax = rct->xmax + shadow_width;
  widget_params.rect.ymax = rct->ymax + shadow_width - shadow_offset;
  widget_params.radi = inner_radius;
  widget_params.rad = shadow_radius;
  widget_params.round_corners[0] = (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 1.0f : 0.0f;
  widget_params.round_corners[1] = (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 1.0f : 0.0f;
  widget_params.round_corners[2] = (roundboxtype & UI_CNR_TOP_RIGHT) ? 1.0f : 0.0f;
  widget_params.round_corners[3] = (roundboxtype & UI_CNR_TOP_LEFT) ? 1.0f : 0.0f;
  widget_params.alpha_discard = 1.0f;

  blender::gpu::Batch *batch = ui_batch_roundbox_shadow_get();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_SHADOW);
  GPU_batch_uniform_4fv_array(batch, "parameters", 4, (const float (*)[4]) & widget_params);
  GPU_batch_uniform_1f(batch, "alpha", alpha);
  GPU_batch_draw(batch);

  GPU_blend(GPU_BLEND_NONE);
}
