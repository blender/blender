/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Gestures (cursor motions) creating, evaluating and drawing, shared between operators.
 */

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"
#include "BLI_lasso_2d.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_state.hh"

#include "BIF_glutil.hh"

using blender::Array;
using blender::int2;

wmGesture *WM_gesture_new(wmWindow *window, const ARegion *region, const wmEvent *event, int type)
{
  wmGesture *gesture = MEM_callocN<wmGesture>("new gesture");

  BLI_addtail(&window->gesture, gesture);

  gesture->type = type;
  gesture->event_type = event->type;
  gesture->event_modifier = event->modifier;
  gesture->event_keymodifier = event->keymodifier;
  gesture->winrct = region->winrct;
  gesture->user_data.use_free = true; /* Free if user-data is set. */
  gesture->modal_state = GESTURE_MODAL_NOP;
  gesture->move = false;

  int xy[2];
  WM_event_drag_start_xy(event, xy);

  if (ELEM(type,
           WM_GESTURE_RECT,
           WM_GESTURE_CROSS_RECT,
           WM_GESTURE_CIRCLE,
           WM_GESTURE_STRAIGHTLINE))
  {
    rcti *rect = MEM_callocN<rcti>("gesture rect new");

    gesture->customdata = rect;
    rect->xmin = xy[0] - gesture->winrct.xmin;
    rect->ymin = xy[1] - gesture->winrct.ymin;
    if (type == WM_GESTURE_CIRCLE) {
      /* Caller is responsible for initializing 'xmax' to radius. */
    }
    else {
      rect->xmax = xy[0] - gesture->winrct.xmin;
      rect->ymax = xy[1] - gesture->winrct.ymin;
    }
  }
  else if (ELEM(type, WM_GESTURE_LINES, WM_GESTURE_LASSO)) {
    float *lasso;
    gesture->points_alloc = 1024;
    gesture->customdata = lasso = MEM_malloc_arrayN<float>(size_t(2 * gesture->points_alloc),
                                                           "lasso points");
    lasso[0] = xy[0] - gesture->winrct.xmin;
    lasso[1] = xy[1] - gesture->winrct.ymin;
    gesture->points = 1;
  }
  else if (ELEM(type, WM_GESTURE_POLYLINE)) {
    gesture->points_alloc = 64;
    short *border = MEM_malloc_arrayN<short>(size_t(2 * gesture->points_alloc), "polyline points");
    gesture->customdata = border;
    border[0] = xy[0] - gesture->winrct.xmin;
    border[1] = xy[1] - gesture->winrct.ymin;
    gesture->mval.x = border[0];
    gesture->mval.y = border[1];
    gesture->points = 1;
  }

  return gesture;
}

void WM_gesture_end(wmWindow *win, wmGesture *gesture)
{
  BLI_remlink(&win->gesture, gesture);
  MEM_freeN(gesture->customdata);
  WM_generic_user_data_free(&gesture->user_data);
  MEM_freeN(gesture);
}

void WM_gestures_free_all(wmWindow *win)
{
  while (win->gesture.first) {
    WM_gesture_end(win, static_cast<wmGesture *>(win->gesture.first));
  }
}

void WM_gestures_remove(wmWindow *win)
{
  while (win->gesture.first) {
    WM_gesture_end(win, static_cast<wmGesture *>(win->gesture.first));
  }
}

bool WM_gesture_is_modal_first(const wmGesture *gesture)
{
  if (gesture == nullptr) {
    return true;
  }
  return (gesture->is_active_prev == false);
}

/* ******************* gesture draw ******************* */

static void wm_gesture_draw_line_active_side(const rcti *rect, const bool flip)
{
  GPUVertFormat *format = immVertexFormat();
  uint shdr_pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  uint shdr_col = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);

  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

  const float gradient_length = 150.0f * UI_SCALE_FAC;
  float line_dir[2];
  float gradient_dir[2];
  float gradient_point[2][2];

  const float line_start[2] = {float(rect->xmin), float(rect->ymin)};
  const float line_end[2] = {float(rect->xmax), float(rect->ymax)};
  const float color_line_gradient_start[4] = {0.2f, 0.2f, 0.2f, 0.4f};
  const float color_line_gradient_end[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  sub_v2_v2v2(line_dir, line_end, line_start);
  normalize_v2(line_dir);
  ortho_v2_v2(gradient_dir, line_dir);
  if (!flip) {
    mul_v2_fl(gradient_dir, -1.0f);
  }
  mul_v2_fl(gradient_dir, gradient_length);
  add_v2_v2v2(gradient_point[0], line_start, gradient_dir);
  add_v2_v2v2(gradient_point[1], line_end, gradient_dir);

  immBegin(GPU_PRIM_TRIS, 6);
  immAttr4f(shdr_col, UNPACK4(color_line_gradient_start));
  immVertex2f(shdr_pos, line_start[0], line_start[1]);
  immAttr4f(shdr_col, UNPACK4(color_line_gradient_start));
  immVertex2f(shdr_pos, line_end[0], line_end[1]);
  immAttr4f(shdr_col, UNPACK4(color_line_gradient_end));
  immVertex2f(shdr_pos, gradient_point[1][0], gradient_point[1][1]);

  immAttr4f(shdr_col, UNPACK4(color_line_gradient_start));
  immVertex2f(shdr_pos, line_start[0], line_start[1]);
  immAttr4f(shdr_col, UNPACK4(color_line_gradient_end));
  immVertex2f(shdr_pos, gradient_point[1][0], gradient_point[1][1]);
  immAttr4f(shdr_col, UNPACK4(color_line_gradient_end));
  immVertex2f(shdr_pos, gradient_point[0][0], gradient_point[0][1]);
  immEnd();

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

static void wm_gesture_draw_line(wmGesture *gt)
{
  const rcti *rect = (rcti *)gt->customdata;

  if (gt->draw_active_side) {
    wm_gesture_draw_line_active_side(rect, gt->use_flip);
  }

  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode. */
  immUniform4f("color", 0.4f, 0.4f, 0.4f, 1.0f);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("udash_factor", 0.5f);

  float xmin = float(rect->xmin);
  float ymin = float(rect->ymin);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(shdr_pos, xmin, ymin);
  immVertex2f(shdr_pos, float(rect->xmax), float(rect->ymax));
  immEnd();

  immUnbindProgram();
}

static void wm_gesture_draw_rect(wmGesture *gt)
{
  const rcti *rect = static_cast<const rcti *>(gt->customdata);

  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  GPU_blend(GPU_BLEND_ALPHA);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.05f);

  immRectf(shdr_pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);

  shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode. */
  immUniform4f("color", 0.4f, 0.4f, 0.4f, 1.0f);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("udash_factor", 0.5f);

  imm_draw_box_wire_2d(
      shdr_pos, float(rect->xmin), float(rect->ymin), float(rect->xmax), float(rect->ymax));

  immUnbindProgram();

  /* Draws a diagonal line in the lined box to test #wm_gesture_draw_line. */
  // wm_gesture_draw_line(gt);
}

static void wm_gesture_draw_circle(wmGesture *gt)
{
  const rcti *rect = static_cast<const rcti *>(gt->customdata);

  GPU_blend(GPU_BLEND_ALPHA);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.05f);
  imm_draw_circle_fill_2d(shdr_pos, float(rect->xmin), float(rect->ymin), float(rect->xmax), 40);

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode. */
  immUniform4f("color", 0.4f, 0.4f, 0.4f, 1.0f);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform1f("dash_width", 4.0f);
  immUniform1f("udash_factor", 0.5f);

  imm_draw_circle_wire_2d(shdr_pos, float(rect->xmin), float(rect->ymin), float(rect->xmax), 40);

  immUnbindProgram();
}

struct LassoFillData {
  uchar *px;
  int width;
};

static void draw_filled_lasso_px_cb(int x, int x_end, int y, void *user_data)
{
  LassoFillData *data = static_cast<LassoFillData *>(user_data);
  uchar *col = &(data->px[(y * data->width) + x]);
  memset(col, 0x10, x_end - x);
}

static void draw_filled_lasso(wmGesture *gt, const blender::int2 *lasso_pt_extra)
{
  const int mcoords_len = gt->points + (lasso_pt_extra ? 1 : 0);
  Array<int2> mcoords(mcoords_len);
  int i;
  rcti rect;
  const float red[4] = {1.0f, 0.0f, 0.0f, 0.0f};

  if (gt->type == WM_GESTURE_POLYLINE) {
    const short *lasso = static_cast<const short *>(gt->customdata);
    for (i = 0; i < mcoords_len; i++, lasso += 2) {
      mcoords[i][0] = lasso[0];
      mcoords[i][1] = lasso[1];
    }
  }
  else {
    const float *lasso = static_cast<const float *>(gt->customdata);
    for (i = 0; i < mcoords_len; i++, lasso += 2) {
      mcoords[i][0] = lasso[0];
      mcoords[i][1] = lasso[1];
    }
  }

  if (lasso_pt_extra) {
    mcoords[mcoords_len - 1][0] = lasso_pt_extra->x;
    mcoords[mcoords_len - 1][1] = lasso_pt_extra->y;
  }

  BLI_lasso_boundbox(&rect, mcoords);

  BLI_rcti_translate(&rect, gt->winrct.xmin, gt->winrct.ymin);
  BLI_rcti_isect(&gt->winrct, &rect, &rect);
  BLI_rcti_translate(&rect, -gt->winrct.xmin, -gt->winrct.ymin);

  /* Highly unlikely this will fail, but could crash if (mcoords_len == 0). */
  if (BLI_rcti_is_empty(&rect) == false) {
    const int w = BLI_rcti_size_x(&rect);
    const int h = BLI_rcti_size_y(&rect);
    uchar *pixel_buf = MEM_calloc_arrayN<uchar>(size_t(w) * size_t(h), __func__);
    LassoFillData lasso_fill_data = {pixel_buf, w};

    BLI_bitmap_draw_2d_poly_v2i_n(rect.xmin,
                                  rect.ymin,
                                  rect.xmax,
                                  rect.ymax,
                                  mcoords,
                                  draw_filled_lasso_px_cb,
                                  &lasso_fill_data);

    GPU_blend(GPU_BLEND_ADDITIVE_PREMULT);

    IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
    GPU_shader_bind(state.shader);
    GPU_shader_uniform_float_ex(
        state.shader, GPU_shader_get_uniform(state.shader, "shuffle"), 4, 1, red);

    immDrawPixelsTexTiled(&state,
                          rect.xmin,
                          rect.ymin,
                          w,
                          h,
                          blender::gpu::TextureFormat::UNORM_8,
                          false,
                          pixel_buf,
                          1.0f,
                          1.0f,
                          nullptr);

    GPU_shader_unbind();

    MEM_freeN(pixel_buf);

    GPU_blend(GPU_BLEND_NONE);
  }
}

/* TODO: Extract this common functionality so it can be shared between Sculpt brushes, the annotate
 * tool, and this common logic. */
static void draw_lasso_smooth_stroke_indicator(wmGesture *gt, const uint shdr_pos)
{
  float (*lasso)[2] = static_cast<float (*)[2]>(gt->customdata);
  float last_x = lasso[gt->points - 1][0];
  float last_y = lasso[gt->points - 1][1];

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);

  GPU_line_width(1.25f);
  const float color[3] = {1.0f, 0.39f, 0.39f};

  const float radius = 4.0f;

  /* Draw Inner Ring */
  immUniformColor4f(color[0], color[1], color[2], 0.8f);
  imm_draw_circle_wire_2d(shdr_pos, gt->mval.x, gt->mval.y, radius, 40);

  /* Draw Outer Ring: Dark color for contrast on light backgrounds (e.g. gray on white) */
  float darkcolor[3];
  mul_v3_v3fl(darkcolor, color, 0.40f);
  immUniformColor4f(darkcolor[0], darkcolor[1], darkcolor[2], 0.8f);
  imm_draw_circle_wire_2d(shdr_pos, gt->mval.x, gt->mval.y, radius + 1, 40);

  /* Draw line from the last saved position to the current mouse position. */
  immUniformColor4f(color[0], color[1], color[2], 0.8f);
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(shdr_pos, gt->mval.x, gt->mval.y);
  immVertex2f(shdr_pos, last_x, last_y);
  immEnd();

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
  immUnbindProgram();
}

static void wm_gesture_draw_lasso(wmGesture *gt, bool filled)
{
  const float *lasso = (float *)gt->customdata;
  int i;

  if (filled) {
    draw_filled_lasso(gt, nullptr);
  }

  const int numverts = gt->points;

  /* Nothing to draw, do early output. */
  if (numverts < 2) {
    return;
  }

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode. */
  immUniform4f("color", 0.4f, 0.4f, 0.4f, 1.0f);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform1f("dash_width", 2.0f);
  immUniform1f("udash_factor", 0.5f);

  immBegin((gt->type == WM_GESTURE_LASSO) ? GPU_PRIM_LINE_LOOP : GPU_PRIM_LINE_STRIP, numverts);

  for (i = 0; i < gt->points; i++, lasso += 2) {
    immVertex2f(shdr_pos, lasso[0], lasso[1]);
  }

  immEnd();
  immUnbindProgram();

  if (gt->use_smooth) {
    draw_lasso_smooth_stroke_indicator(gt, shdr_pos);
  }
}

static void draw_start_vertex_circle(const wmGesture &gt, const uint shdr_pos)
{
  const int numverts = gt.points;

  /* Draw the circle around the starting vertex. */
  const short (*border)[2] = static_cast<short int (*)[2]>(gt.customdata);

  const float start_pos[2] = {float(border[0][0]), float(border[0][1])};
  const float current_pos[2] = {float(gt.mval.x), float(gt.mval.y)};

  const float dist = len_v2v2(start_pos, current_pos);
  const float limit = pow2f(blender::wm::gesture::POLYLINE_CLICK_RADIUS * UI_SCALE_FAC);

  if (dist < limit && numverts > 2) {
    const float u = smoothstep(0.0f, limit, dist);
    const float radius = interpf(
        1.0f * UI_SCALE_FAC, blender::wm::gesture::POLYLINE_CLICK_RADIUS * UI_SCALE_FAC, u);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    const blender::float3 color = {1.0f, 1.0f, 1.0f};
    immUniformColor4f(color.x, color.y, color.z, 0.8f);
    imm_draw_circle_wire_2d(shdr_pos, start_pos[0], start_pos[1], radius, 15.0f);

    const blender::float3 darker_color = color * 0.4f;
    immUniformColor4f(darker_color.x, darker_color.y, darker_color.z, 0.8f);
    imm_draw_circle_wire_2d(shdr_pos, start_pos[0], start_pos[1], radius + 1, 15.0f);

    immUnbindProgram();
  }
}

static void wm_gesture_draw_polyline(wmGesture *gt)
{
  draw_filled_lasso(gt, &gt->mval);

  const int numverts = gt->points + 1;
  if (numverts < 2) {
    return;
  }

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniform4f("color", 0.4f, 0.4f, 0.4f, 1.0f);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform1f("dash_width", 2.0f);
  immUniform1f("udash_factor", 0.5f);

  immBegin(GPU_PRIM_LINE_LOOP, numverts);

  const short *border = (short *)gt->customdata;
  for (int i = 0; i < gt->points; i++, border += 2) {
    immVertex2f(shdr_pos, float(border[0]), float(border[1]));
  }
  immVertex2f(shdr_pos, float(gt->mval.x), float(gt->mval.y));

  immEnd();

  immUnbindProgram();

  draw_start_vertex_circle(*gt, shdr_pos);
}

static void wm_gesture_draw_cross(const wmWindow *win, const wmGesture *gt)
{
  const rcti *rect = static_cast<const rcti *>(gt->customdata);
  const blender::int2 win_size = WM_window_native_pixel_size(win);

  float x1, x2, y1, y2;

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode. */
  immUniform4f("color", 0.4f, 0.4f, 0.4f, 1.0f);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("udash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 4);

  x1 = float(rect->xmin - win_size[0]);
  y1 = float(rect->ymin);
  x2 = float(rect->xmin + win_size[0]);
  y2 = y1;

  immVertex2f(shdr_pos, x1, y1);
  immVertex2f(shdr_pos, x2, y2);

  x1 = float(rect->xmin);
  y1 = float(rect->ymin - win_size[1]);
  x2 = x1;
  y2 = float(rect->ymin + win_size[1]);

  immVertex2f(shdr_pos, x1, y1);
  immVertex2f(shdr_pos, x2, y2);

  immEnd();

  immUnbindProgram();
}

void wm_gesture_draw(wmWindow *win)
{
  wmGesture *gt = (wmGesture *)win->gesture.first;

  GPU_line_width(1.0f);
  for (; gt; gt = gt->next) {
    /* All in sub-window space. */
    wmViewport(&gt->winrct);

    if (gt->type == WM_GESTURE_RECT) {
      wm_gesture_draw_rect(gt);
    }
    else if (gt->type == WM_GESTURE_CIRCLE) {
      wm_gesture_draw_circle(gt);
    }
    else if (gt->type == WM_GESTURE_CROSS_RECT) {
      if (gt->is_active) {
        wm_gesture_draw_rect(gt);
      }
      else {
        wm_gesture_draw_cross(win, gt);
      }
    }
    else if (gt->type == WM_GESTURE_LINES) {
      wm_gesture_draw_lasso(gt, false);
    }
    else if (gt->type == WM_GESTURE_LASSO) {
      wm_gesture_draw_lasso(gt, true);
    }
    else if (gt->type == WM_GESTURE_STRAIGHTLINE) {
      wm_gesture_draw_line(gt);
    }
    else if (gt->type == WM_GESTURE_POLYLINE) {
      wm_gesture_draw_polyline(gt);
    }
  }
}

void wm_gesture_tag_redraw(wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);

  if (screen) {
    screen->do_draw_gesture = true;
  }
}
