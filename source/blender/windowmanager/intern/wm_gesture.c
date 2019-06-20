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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * Gestures (cursor motions) creating, evaluating and drawing, shared between operators.
 */

#include "DNA_screen_types.h"
#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_lasso_2d.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_draw.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_state.h"

#include "BIF_glutil.h"

/* context checked on having screen, window and area */
wmGesture *WM_gesture_new(bContext *C, const wmEvent *event, int type)
{
  wmGesture *gesture = MEM_callocN(sizeof(wmGesture), "new gesture");
  wmWindow *window = CTX_wm_window(C);
  ARegion *ar = CTX_wm_region(C);

  BLI_addtail(&window->gesture, gesture);

  gesture->type = type;
  gesture->event_type = event->type;
  gesture->winrct = ar->winrct;
  gesture->user_data.use_free = true; /* Free if userdata is set. */
  gesture->modal_state = GESTURE_MODAL_NOP;

  if (ELEM(type,
           WM_GESTURE_RECT,
           WM_GESTURE_CROSS_RECT,
           WM_GESTURE_TWEAK,
           WM_GESTURE_CIRCLE,
           WM_GESTURE_STRAIGHTLINE)) {
    rcti *rect = MEM_callocN(sizeof(rcti), "gesture rect new");

    gesture->customdata = rect;
    rect->xmin = event->x - gesture->winrct.xmin;
    rect->ymin = event->y - gesture->winrct.ymin;
    if (type == WM_GESTURE_CIRCLE) {
      /* caller is responsible for initializing 'xmax' to radius. */
    }
    else {
      rect->xmax = event->x - gesture->winrct.xmin;
      rect->ymax = event->y - gesture->winrct.ymin;
    }
  }
  else if (ELEM(type, WM_GESTURE_LINES, WM_GESTURE_LASSO)) {
    short *lasso;
    gesture->points_alloc = 1024;
    gesture->customdata = lasso = MEM_mallocN(sizeof(short[2]) * gesture->points_alloc,
                                              "lasso points");
    lasso[0] = event->x - gesture->winrct.xmin;
    lasso[1] = event->y - gesture->winrct.ymin;
    gesture->points = 1;
  }

  return gesture;
}

void WM_gesture_end(bContext *C, wmGesture *gesture)
{
  wmWindow *win = CTX_wm_window(C);

  if (win->tweak == gesture) {
    win->tweak = NULL;
  }
  BLI_remlink(&win->gesture, gesture);
  MEM_freeN(gesture->customdata);
  WM_generic_user_data_free(&gesture->user_data);
  MEM_freeN(gesture);
}

void WM_gestures_remove(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);

  while (win->gesture.first) {
    WM_gesture_end(C, win->gesture.first);
  }
}

bool WM_gesture_is_modal_first(const wmGesture *gesture)
{
  if (gesture == NULL) {
    return true;
  }
  return (gesture->is_active_prev == false);
}

/* tweak and line gestures */
int wm_gesture_evaluate(wmGesture *gesture, const wmEvent *event)
{
  if (gesture->type == WM_GESTURE_TWEAK) {
    rcti *rect = gesture->customdata;
    const int delta[2] = {
        BLI_rcti_size_x(rect),
        BLI_rcti_size_y(rect),
    };

    if (WM_event_drag_test_with_delta(event, delta)) {
      int theta = round_fl_to_int(4.0f * atan2f((float)delta[1], (float)delta[0]) / (float)M_PI);
      int val = EVT_GESTURE_W;

      if (theta == 0) {
        val = EVT_GESTURE_E;
      }
      else if (theta == 1) {
        val = EVT_GESTURE_NE;
      }
      else if (theta == 2) {
        val = EVT_GESTURE_N;
      }
      else if (theta == 3) {
        val = EVT_GESTURE_NW;
      }
      else if (theta == -1) {
        val = EVT_GESTURE_SE;
      }
      else if (theta == -2) {
        val = EVT_GESTURE_S;
      }
      else if (theta == -3) {
        val = EVT_GESTURE_SW;
      }

#if 0
      /* debug */
      if (val == 1) {
        printf("tweak north\n");
      }
      if (val == 2) {
        printf("tweak north-east\n");
      }
      if (val == 3) {
        printf("tweak east\n");
      }
      if (val == 4) {
        printf("tweak south-east\n");
      }
      if (val == 5) {
        printf("tweak south\n");
      }
      if (val == 6) {
        printf("tweak south-west\n");
      }
      if (val == 7) {
        printf("tweak west\n");
      }
      if (val == 8) {
        printf("tweak north-west\n");
      }
#endif
      return val;
    }
  }
  return 0;
}

/* ******************* gesture draw ******************* */

static void wm_gesture_draw_line(wmGesture *gt)
{
  rcti *rect = (rcti *)gt->customdata;

  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  glGetFloatv(GL_VIEWPORT, viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniformArray4fv(
      "colors", (float *)(float[][4]){{0.4f, 0.4f, 0.4f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("dash_factor", 0.5f);

  float xmin = (float)rect->xmin;
  float ymin = (float)rect->ymin;

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(shdr_pos, xmin, ymin);
  immVertex2f(shdr_pos, (float)rect->xmax, (float)rect->ymax);
  immEnd();

  immUnbindProgram();
}

static void wm_gesture_draw_rect(wmGesture *gt)
{
  rcti *rect = (rcti *)gt->customdata;

  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

  GPU_blend(true);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.05f);

  immRecti(shdr_pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  immUnbindProgram();

  GPU_blend(false);

  shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  glGetFloatv(GL_VIEWPORT, viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniformArray4fv(
      "colors", (float *)(float[][4]){{0.4f, 0.4f, 0.4f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("dash_factor", 0.5f);

  imm_draw_box_wire_2d(
      shdr_pos, (float)rect->xmin, (float)rect->ymin, (float)rect->xmax, (float)rect->ymax);

  immUnbindProgram();

  /* draws a diagonal line in the lined box to test wm_gesture_draw_line */
  // wm_gesture_draw_line(gt);
}

static void wm_gesture_draw_circle(wmGesture *gt)
{
  rcti *rect = (rcti *)gt->customdata;

  GPU_blend(true);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.05f);
  imm_draw_circle_fill_2d(shdr_pos, (float)rect->xmin, (float)rect->ymin, (float)rect->xmax, 40);

  immUnbindProgram();

  GPU_blend(false);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  glGetFloatv(GL_VIEWPORT, viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniformArray4fv(
      "colors", (float *)(float[][4]){{0.4f, 0.4f, 0.4f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 4.0f);
  immUniform1f("dash_factor", 0.5f);

  imm_draw_circle_wire_2d(shdr_pos, (float)rect->xmin, (float)rect->ymin, (float)rect->xmax, 40);

  immUnbindProgram();
}

struct LassoFillData {
  unsigned char *px;
  int width;
};

static void draw_filled_lasso_px_cb(int x, int x_end, int y, void *user_data)
{
  struct LassoFillData *data = user_data;
  unsigned char *col = &(data->px[(y * data->width) + x]);
  memset(col, 0x10, x_end - x);
}

static void draw_filled_lasso(wmGesture *gt)
{
  const short *lasso = (short *)gt->customdata;
  const int tot = gt->points;
  int(*moves)[2] = MEM_mallocN(sizeof(*moves) * (tot + 1), __func__);
  int i;
  rcti rect;
  float red[4] = {1.0f, 0.0f, 0.0f, 0.0f};

  for (i = 0; i < tot; i++, lasso += 2) {
    moves[i][0] = lasso[0];
    moves[i][1] = lasso[1];
  }

  BLI_lasso_boundbox(&rect, (const int(*)[2])moves, tot);

  BLI_rcti_translate(&rect, gt->winrct.xmin, gt->winrct.ymin);
  BLI_rcti_isect(&gt->winrct, &rect, &rect);
  BLI_rcti_translate(&rect, -gt->winrct.xmin, -gt->winrct.ymin);

  /* highly unlikely this will fail, but could crash if (tot == 0) */
  if (BLI_rcti_is_empty(&rect) == false) {
    const int w = BLI_rcti_size_x(&rect);
    const int h = BLI_rcti_size_y(&rect);
    unsigned char *pixel_buf = MEM_callocN(sizeof(*pixel_buf) * w * h, __func__);
    struct LassoFillData lasso_fill_data = {pixel_buf, w};

    BLI_bitmap_draw_2d_poly_v2i_n(rect.xmin,
                                  rect.ymin,
                                  rect.xmax,
                                  rect.ymax,
                                  (const int(*)[2])moves,
                                  tot,
                                  draw_filled_lasso_px_cb,
                                  &lasso_fill_data);

    /* Additive Blending */
    GPU_blend(true);
    glBlendFunc(GL_ONE, GL_ONE);

    GLint unpack_alignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
    GPU_shader_bind(state.shader);
    GPU_shader_uniform_vector(
        state.shader, GPU_shader_get_uniform_ensure(state.shader, "shuffle"), 4, 1, red);

    immDrawPixelsTex(&state,
                     rect.xmin,
                     rect.ymin,
                     w,
                     h,
                     GL_RED,
                     GL_UNSIGNED_BYTE,
                     GL_NEAREST,
                     pixel_buf,
                     1.0f,
                     1.0f,
                     NULL);

    GPU_shader_unbind();

    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);

    MEM_freeN(pixel_buf);

    GPU_blend(false);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  MEM_freeN(moves);
}

static void wm_gesture_draw_lasso(wmGesture *gt, bool filled)
{
  const short *lasso = (short *)gt->customdata;
  int i;

  if (filled) {
    draw_filled_lasso(gt);
  }

  const int numverts = gt->points;

  /* Nothing to draw, do early output. */
  if (numverts < 2) {
    return;
  }

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  glGetFloatv(GL_VIEWPORT, viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniformArray4fv(
      "colors", (float *)(float[][4]){{0.4f, 0.4f, 0.4f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 2.0f);
  immUniform1f("dash_factor", 0.5f);

  immBegin((gt->type == WM_GESTURE_LASSO) ? GPU_PRIM_LINE_LOOP : GPU_PRIM_LINE_STRIP, numverts);

  for (i = 0; i < gt->points; i++, lasso += 2) {
    immVertex2f(shdr_pos, (float)lasso[0], (float)lasso[1]);
  }

  immEnd();

  immUnbindProgram();
}

static void wm_gesture_draw_cross(wmWindow *win, wmGesture *gt)
{
  rcti *rect = (rcti *)gt->customdata;
  const int winsize_x = WM_window_pixels_x(win);
  const int winsize_y = WM_window_pixels_y(win);

  float x1, x2, y1, y2;

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  glGetFloatv(GL_VIEWPORT, viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniformArray4fv(
      "colors", (float *)(float[][4]){{0.4f, 0.4f, 0.4f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("dash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 4);

  x1 = (float)(rect->xmin - winsize_x);
  y1 = (float)rect->ymin;
  x2 = (float)(rect->xmin + winsize_x);
  y2 = y1;

  immVertex2f(shdr_pos, x1, y1);
  immVertex2f(shdr_pos, x2, y2);

  x1 = (float)rect->xmin;
  y1 = (float)(rect->ymin - winsize_y);
  x2 = x1;
  y2 = (float)(rect->ymin + winsize_y);

  immVertex2f(shdr_pos, x1, y1);
  immVertex2f(shdr_pos, x2, y2);

  immEnd();

  immUnbindProgram();
}

/* called in wm_draw.c */
void wm_gesture_draw(wmWindow *win)
{
  wmGesture *gt = (wmGesture *)win->gesture.first;

  GPU_line_width(1.0f);
  for (; gt; gt = gt->next) {
    /* all in subwindow space */
    wmViewport(&gt->winrct);

    if (gt->type == WM_GESTURE_RECT) {
      wm_gesture_draw_rect(gt);
    }
#if 0
    else if (gt->type == WM_GESTURE_TWEAK) {
      wm_gesture_draw_line(gt);
    }
#endif
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
  }
}

void wm_gesture_tag_redraw(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);

  if (screen) {
    screen->do_draw_gesture = true;
  }
}
