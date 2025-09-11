/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Developers Note:
 *
 * This test currently only creates windows and draws a 'dot' under the cursor on LMB,
 * quits when Q is pressed.
 *
 * More work is needed for logging drawing to work properly.
 *
 * - Use GPU_matrix API.
 * - Replace old OpenGL calls to `glColor`, etc with `imm` API.
 * - Investigate BLF font flushing (`UI_widgetbase_draw_cache_flush`) which is currently disabled.
 */

#ifdef _MSC_VER
#  pragma warning(disable : 4244 4305)
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GL.h"
#include <GL/glew.h>

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "BLF_api.hh"

#include "Basic.h"
#include "EventToBuf.h"
#include "ScrollBar.h"
#include "Util.h"

#include "WindowData.h"

/* GPU API. */
#include "GPU_context.hh"
#include "GPU_immediate.hh"
#include "GPU_init_exit.hh"

extern int datatoc_bfont_ttf_size;
extern char const datatoc_bfont_ttf[];

typedef struct _LoggerWindow LoggerWindow;
typedef struct _MultiTestApp MultiTestApp;

void loggerwindow_log(LoggerWindow *lw, char *line);

void multitestapp_toggle_extra_window(MultiTestApp *app);
void multitestapp_free_extrawindow(MultiTestApp *app);
LoggerWindow *multitestapp_get_logger(MultiTestApp *app);
GHOST_SystemHandle multitestapp_get_system(MultiTestApp *app);
void multitestapp_exit(MultiTestApp *app);

/**/

void rect_bevel_side(int rect[2][2], int side, float *lt, float *dk, const float col[3], int width)
{
  int ltidx = (side / 2) % 4;
  int dkidx = (ltidx + 1 + (side & 1)) % 4;
  int i, corner;

  glBegin(GL_LINES);
  for (i = 0; i < width; i++) {
    float ltf = pow(lt[i], 1.0 / 2.2), dkf = pow(dk[i], 1.0 / 2.2);
    float stf = (dkidx > ltidx) ? dkf : ltf;
    int lx = rect[1][0] - i - 1;
    int ly = rect[0][1] + i;

    glColor3f(col[0] * stf, col[1] * stf, col[2] * stf);
    for (corner = 0; corner < 4; corner++) {
      int x = (corner == 0 || corner == 1) ? (rect[0][0] + i) : (rect[1][0] - i - 1);
      int y = (corner == 0 || corner == 3) ? (rect[0][1] + i) : (rect[1][1] - i - 1);

      if (ltidx == corner) {
        glColor3f(col[0] * ltf, col[1] * ltf, col[2] * ltf);
      }
      if (dkidx == corner) {
        glColor3f(col[0] * dkf, col[1] * dkf, col[2] * dkf);
      }

      glVertex2i(lx, ly);
      glVertex2i(lx = x, ly = y);
    }
  }
  glEnd();

  glColor3fv(col);
  glRecti(rect[0][0] + width, rect[0][1] + width, rect[1][0] - width, rect[1][1] - width);
}

void rect_bevel_smooth(int rect[2][2], int width)
{
  float *lt = malloc(sizeof(*lt) * width);
  float *dk = malloc(sizeof(*dk) * width);
  float col[4];
  int i;

  for (i = 0; i < width; i++) {
    float v = width - 1 ? ((float)i / (width - 1)) : 0;
    lt[i] = 1.2 + (1.0 - 1.2) * v;
    dk[i] = 0.2 + (1.0 - 0.2) * v;
  }

  glGetFloatv(GL_CURRENT_COLOR, col);

  rect_bevel_side(rect, 3, lt, dk, col, width);

  free(lt);
  free(dk);
}

/*
 * MainWindow
 */

typedef struct {
  MultiTestApp *app;

  GHOST_WindowHandle win;
  GPUContext *gpu_context;

  int size[2];

  int lmouse[2], lmbut[3];

  int tmouse[2];
} MainWindow;

static void mainwindow_log(MainWindow *mw, char *str)
{
  loggerwindow_log(multitestapp_get_logger(mw->app), str);
}

static void mainwindow_do_draw(MainWindow *mw)
{
  GHOST_ActivateWindowDrawingContext(mw->win);
  GHOST_SwapWindowBufferAcquire(mw->win);
  GPU_context_active_set(mw->gpu_context);

  if (mw->lmbut[0]) {
    glClearColor(0.5, 0.5, 0.5, 1);
  }
  else {
    glClearColor(1, 1, 1, 1);
  }
  glClear(GL_COLOR_BUFFER_BIT);

  glColor3f(0.5, 0.6, 0.8);
  glRecti(mw->tmouse[0] - 5, mw->tmouse[1] - 5, mw->tmouse[0] + 5, mw->tmouse[1] + 5);

  GHOST_SwapWindowBufferRelease(mw->win);
}

static void mainwindow_do_reshape(MainWindow *mw)
{
  GHOST_RectangleHandle bounds = GHOST_GetClientBounds(mw->win);

  GHOST_ActivateWindowDrawingContext(mw->win);
  GPU_context_active_set(mw->gpu_context);

  mw->size[0] = GHOST_GetWidthRectangle(bounds);
  mw->size[1] = GHOST_GetHeightRectangle(bounds);

  glViewport(0, 0, mw->size[0], mw->size[1]);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, mw->size[0], 0, mw->size[1], -1, 1);
  glTranslatef(0.375, 0.375, 0.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void mainwindow_do_key(MainWindow *mw, GHOST_TKey key, int press)
{
  switch (key) {
    case GHOST_kKeyC:
      if (press) {
        GHOST_SetCursorShape(mw->win,
                             (GHOST_TStandardCursor)(rand() % (GHOST_kStandardCursorNumCursors)));
      }
      break;
    case GHOST_kKeyLeftBracket:
      if (press) {
        GHOST_SetCursorVisibility(mw->win, false);
      }
      break;
    case GHOST_kKeyRightBracket:
      if (press) {
        GHOST_SetCursorVisibility(mw->win, true);
      }
      break;
    case GHOST_kKeyE:
      if (press) {
        multitestapp_toggle_extra_window(mw->app);
      }
      break;
    case GHOST_kKeyQ:
      if (press) {
        multitestapp_exit(mw->app);
      }
      break;
    case GHOST_kKeyT:
      if (press) {
        mainwindow_log(mw, "TextTest~|`hello`\"world\",<>/");
      }
      break;
    case GHOST_kKeyR:
      if (press) {
        int i;

        mainwindow_log(mw, "Invalidating window 10 times");
        for (i = 0; i < 10; i++) {
          GHOST_InvalidateWindow(mw->win);
        }
      }
      break;
    case GHOST_kKeyF11:
      if (press) {
        GHOST_SetWindowOrder(mw->win, GHOST_kWindowOrderBottom);
      }
      break;
  }
}

static void mainwindow_do_move(MainWindow *mw, int x, int y)
{
  mw->lmouse[0] = x, mw->lmouse[1] = y;

  if (mw->lmbut[0]) {
    mw->tmouse[0] = x, mw->tmouse[1] = y;
    GHOST_InvalidateWindow(mw->win);
  }
}

static void mainwindow_do_button(MainWindow *mw, int which, int press)
{
  if (which == GHOST_kButtonMaskLeft) {
    mw->lmbut[0] = press;
    mw->tmouse[0] = mw->lmouse[0], mw->tmouse[1] = mw->lmouse[1];
    GHOST_InvalidateWindow(mw->win);
  }
  else if (which == GHOST_kButtonMaskLeft) {
    mw->lmbut[1] = press;
  }
  else if (which == GHOST_kButtonMaskLeft) {
    mw->lmbut[2] = press;
  }
}

static void mainwindow_handle(void *priv, GHOST_EventHandle evt)
{
  MainWindow *mw = priv;
  GHOST_TEventType type = GHOST_GetEventType(evt);
  char buf[256];

  event_to_buf(evt, buf);
  mainwindow_log(mw, buf);

  switch (type) {
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cd = GHOST_GetEventData(evt);
      int x, y;
      GHOST_ScreenToClient(mw->win, cd->x, cd->y, &x, &y);
      mainwindow_do_move(mw, x, mw->size[1] - y - 1);
      break;
    }
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = GHOST_GetEventData(evt);
      mainwindow_do_button(mw, bd->button, (type == GHOST_kEventButtonDown));
      break;
    }
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *kd = GHOST_GetEventData(evt);
      mainwindow_do_key(mw, kd->key, (type == GHOST_kEventKeyDown));
      break;
    }

    case GHOST_kEventWindowUpdate:
      mainwindow_do_draw(mw);
      break;
    case GHOST_kEventWindowSize:
      mainwindow_do_reshape(mw);
      break;
  }
}

/**/

static void mainwindow_timer_proc(GHOST_TimerTaskHandle task, uint64_t time)
{
  MainWindow *mw = GHOST_GetTimerTaskUserData(task);
  char buf[64];

  sprintf(buf, "timer: %6.2f", (double)((int64_t)time) / 1000);
  mainwindow_log(mw, buf);
}

MainWindow *mainwindow_new(MultiTestApp *app)
{
  GHOST_SystemHandle sys = multitestapp_get_system(app);
  GHOST_WindowHandle win;
  GHOST_GPUSettings gpu_settings = {0};

  win = GHOST_CreateWindow(sys,
                           NULL,
                           "MultiTest:Main",
                           40,
                           40,
                           400,
                           400,
                           GHOST_kWindowStateNormal,
                           false,
                           GHOST_kDrawingContextTypeOpenGL,
                           gpu_settings);

  if (win) {
    MainWindow *mw = MEM_callocN(sizeof(*mw), "mainwindow_new");

    mw->gpu_context = GPU_context_create(win, NULL);
    GPU_init();

    mw->app = app;
    mw->win = win;

    GHOST_SetWindowUserData(mw->win, windowdata_new(mw, mainwindow_handle));

    GHOST_InstallTimer(sys, 1000, 10000, mainwindow_timer_proc, mw);

    return mw;
  }
  return NULL;
}

void mainwindow_free(MainWindow *mw)
{
  GHOST_SystemHandle sys = multitestapp_get_system(mw->app);

  windowdata_free(GHOST_GetWindowUserData(mw->win));
  GHOST_DisposeWindow(sys, mw->win);
  MEM_freeN(mw);
}

/*
 * LoggerWindow
 */

struct _LoggerWindow {
  MultiTestApp *app;

  GHOST_WindowHandle win;
  GPUContext *gpu_context;

  int font;
  int fontheight;

  int size[2];

  int ndisplines;
  int textarea[2][2];
  ScrollBar *scroll;

  char **loglines;
  int nloglines, logsize;

  int lmbut[3];
  int lmouse[2];
};

#define SCROLLBAR_PAD 2
#define SCROLLBAR_WIDTH 14
#define TEXTAREA_PAD 2
static void loggerwindow_recalc_regions(LoggerWindow *lw)
{
  int nscroll[2][2];

  nscroll[0][0] = SCROLLBAR_PAD;
  nscroll[0][1] = SCROLLBAR_PAD;
  nscroll[1][0] = nscroll[0][0] + SCROLLBAR_WIDTH;
  nscroll[1][1] = lw->size[1] - SCROLLBAR_PAD - 1;

  lw->textarea[0][0] = nscroll[1][0] + TEXTAREA_PAD;
  lw->textarea[0][1] = TEXTAREA_PAD;
  lw->textarea[1][0] = lw->size[0] - TEXTAREA_PAD - 1;
  lw->textarea[1][1] = lw->size[1] - TEXTAREA_PAD - 1;

  lw->ndisplines = (lw->textarea[1][1] - lw->textarea[0][1]) / lw->fontheight;

  scrollbar_set_thumbpct(lw->scroll, (float)lw->ndisplines / lw->nloglines);
  scrollbar_set_rect(lw->scroll, nscroll);
}

static void loggerwindow_setup_window_gl(LoggerWindow *lw)
{
  glViewport(0, 0, lw->size[0], lw->size[1]);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, lw->size[0], 0, lw->size[1], -1, 1);
  glTranslatef(0.375, 0.375, 0.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void loggerwindow_do_reshape(LoggerWindow *lw)
{
  GHOST_RectangleHandle bounds = GHOST_GetClientBounds(lw->win);

  GHOST_ActivateWindowDrawingContext(lw->win);
  GPU_context_active_set(lw->gpu_context);

  lw->size[0] = GHOST_GetWidthRectangle(bounds);
  lw->size[1] = GHOST_GetHeightRectangle(bounds);

  loggerwindow_recalc_regions(lw);
  loggerwindow_setup_window_gl(lw);
}

static void loggerwindow_do_draw(LoggerWindow *lw)
{
  int i, ndisplines, startline;
  int sb_rect[2][2], sb_thumb[2][2];

  GHOST_ActivateWindowDrawingContext(lw->win);
  GHOST_SwapWindowBufferAcquire(lw->win);
  GPU_context_active_set(lw->gpu_context);

  glClearColor(1, 1, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  glColor3f(0.8, 0.8, 0.8);
  rect_bevel_smooth(lw->textarea, 4);

  scrollbar_get_rect(lw->scroll, sb_rect);
  scrollbar_get_thumb(lw->scroll, sb_thumb);

  glColor3f(0.6, 0.6, 0.6);
  rect_bevel_smooth(sb_rect, 1);

  if (scrollbar_is_scrolling(lw->scroll)) {
    glColor3f(0.6, 0.7, 0.5);
  }
  else {
    glColor3f(0.9, 0.9, 0.92);
  }
  rect_bevel_smooth(sb_thumb, 1);

  startline = scrollbar_get_thumbpos(lw->scroll) * (lw->nloglines - 1);
  ndisplines = min_i(lw->ndisplines, lw->nloglines - startline);

  glColor3f(0, 0, 0);
  for (i = 0; i < ndisplines; i++) {
    /* stored in reverse order */
    char *line = lw->loglines[(lw->nloglines - 1) - (i + startline)];
    int x_pos = lw->textarea[0][0] + 4;
    int y_pos = lw->textarea[0][1] + 4 + i * lw->fontheight;

    BLF_position(lw->font, x_pos, y_pos, 0.0);
    BLF_draw(lw->font, line, 256);  // XXX
  }

  GHOST_SwapWindowBufferRelease(lw->win);

  immDeactivate();
}

static void loggerwindow_do_move(LoggerWindow *lw, int x, int y)
{
  lw->lmouse[0] = x, lw->lmouse[1] = y;

  if (scrollbar_is_scrolling(lw->scroll)) {
    scrollbar_keep_scrolling(lw->scroll, y);
    GHOST_InvalidateWindow(lw->win);
  }
}

static void loggerwindow_do_button(LoggerWindow *lw, int which, int press)
{
  if (which == GHOST_kButtonMaskLeft) {
    lw->lmbut[0] = press;

    if (press) {
      if (scrollbar_contains_pt(lw->scroll, lw->lmouse)) {
        scrollbar_start_scrolling(lw->scroll, lw->lmouse[1]);
        GHOST_SetCursorShape(lw->win, GHOST_kStandardCursorUpDown);
        GHOST_InvalidateWindow(lw->win);
      }
    }
    else {
      if (scrollbar_is_scrolling(lw->scroll)) {
        scrollbar_stop_scrolling(lw->scroll);
        GHOST_SetCursorShape(lw->win, GHOST_kStandardCursorDefault);
        GHOST_InvalidateWindow(lw->win);
      }
    }
  }
  else if (which == GHOST_kButtonMaskMiddle) {
    lw->lmbut[1] = press;
  }
  else if (which == GHOST_kButtonMaskRight) {
    lw->lmbut[2] = press;
  }
}

static void loggerwindow_do_key(LoggerWindow *lw, GHOST_TKey key, int press)
{
  switch (key) {
    case GHOST_kKeyQ:
      if (press) {
        multitestapp_exit(lw->app);
      }
      break;
  }
}

static void loggerwindow_handle(void *priv, GHOST_EventHandle evt)
{
  LoggerWindow *lw = priv;
  GHOST_TEventType type = GHOST_GetEventType(evt);

  switch (type) {
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cd = GHOST_GetEventData(evt);
      int x, y;
      GHOST_ScreenToClient(lw->win, cd->x, cd->y, &x, &y);
      loggerwindow_do_move(lw, x, lw->size[1] - y - 1);
      break;
    }
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = GHOST_GetEventData(evt);
      loggerwindow_do_button(lw, bd->button, (type == GHOST_kEventButtonDown));
      break;
    }
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *kd = GHOST_GetEventData(evt);
      loggerwindow_do_key(lw, kd->key, (type == GHOST_kEventKeyDown));
      break;
    }

    case GHOST_kEventWindowUpdate:
      loggerwindow_do_draw(lw);
      break;
    case GHOST_kEventWindowSize:
      loggerwindow_do_reshape(lw);
      break;
  }
}

/**/

LoggerWindow *loggerwindow_new(MultiTestApp *app)
{
  GHOST_GPUSettings gpu_settings = {0};
  GHOST_SystemHandle sys = multitestapp_get_system(app);
  uint32_t screensize[2];
  GHOST_WindowHandle win;

  int posx = 40;
  int posy = 0;
  if (GHOST_GetMainDisplayDimensions(sys, &screensize[0], &screensize[1]) == GHOST_kSuccess) {
    posy = screensize[1] - 432;
  }

  win = GHOST_CreateWindow(sys,
                           NULL,
                           "MultiTest:Logger",
                           posx,
                           posy,
                           800,
                           300,
                           GHOST_kWindowStateNormal,
                           false,
                           GHOST_kDrawingContextTypeOpenGL,
                           gpu_settings);

  if (win) {
    LoggerWindow *lw = MEM_callocN(sizeof(*lw), "loggerwindow_new");

    lw->gpu_context = GPU_context_create(win, NULL);
    GPU_init();

    int bbox[2][2];
    lw->app = app;
    lw->win = win;

    lw->font = BLF_load_default(false);
    BLF_size(lw->font, 11, 72);
    lw->fontheight = BLF_height(lw->font, "A_", 2);

    lw->nloglines = lw->logsize = 0;
    lw->loglines = MEM_mallocN(sizeof(*lw->loglines) * lw->nloglines, "loglines");

    lw->scroll = scrollbar_new(2, 40);

    GHOST_SetWindowUserData(lw->win, windowdata_new(lw, loggerwindow_handle));

    loggerwindow_do_reshape(lw);

    return lw;
  }
  else {
    return NULL;
  }
}

void loggerwindow_log(LoggerWindow *lw, char *line)
{
  if (lw->nloglines == lw->logsize) {
    lw->loglines = memdbl(lw->loglines, &lw->logsize, sizeof(*lw->loglines));
  }

  lw->loglines[lw->nloglines++] = string_dup(line);
  scrollbar_set_thumbpct(lw->scroll, (float)lw->ndisplines / lw->nloglines);

  GHOST_InvalidateWindow(lw->win);
}

void loggerwindow_free(LoggerWindow *lw)
{
  GHOST_SystemHandle sys = multitestapp_get_system(lw->app);
  int i;

  for (i = 0; i < lw->nloglines; i++) {
    MEM_freeN(lw->loglines[i]);
  }
  MEM_freeN(lw->loglines);

  windowdata_free(GHOST_GetWindowUserData(lw->win));
  GHOST_DisposeWindow(sys, lw->win);
  MEM_freeN(lw);
}

/*
 * ExtraWindow
 */

typedef struct {
  MultiTestApp *app;

  GHOST_WindowHandle win;
  GPUContext *gpu_context;

  int size[2];
} ExtraWindow;

static void extrawindow_do_draw(ExtraWindow *ew)
{
  GHOST_ActivateWindowDrawingContext(ew->win);
  GHOST_SwapWindowBufferAcquire(eq->win);
  GPU_context_active_set(ew->gpu_context);

  glClearColor(1, 1, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  glColor3f(0.8, 0.8, 0.8);
  glRecti(10, 10, ew->size[0] - 10, ew->size[1] - 10);

  GHOST_SwapWindowBufferRelease(ew->win);
}

static void extrawindow_do_reshape(ExtraWindow *ew)
{
  GHOST_RectangleHandle bounds = GHOST_GetClientBounds(ew->win);

  GHOST_ActivateWindowDrawingContext(ew->win);
  GPU_context_active_set(ew->gpu_context);

  ew->size[0] = GHOST_GetWidthRectangle(bounds);
  ew->size[1] = GHOST_GetHeightRectangle(bounds);

  glViewport(0, 0, ew->size[0], ew->size[1]);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, ew->size[0], 0, ew->size[1], -1, 1);
  glTranslatef(0.375, 0.375, 0.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void extrawindow_do_key(ExtraWindow *ew, GHOST_TKey key, int press)
{
  switch (key) {
    case GHOST_kKeyE:
      if (press) {
        multitestapp_toggle_extra_window(ew->app);
      }
      break;
  }
}

static void extrawindow_spin_cursor(ExtraWindow *ew, uint64_t time)
{
  uint8_t bitmap[16][2];
  uint8_t mask[16][2];
  double ftime = (double)((int64_t)time) / 1000;
  float angle = fmod(ftime, 1.0) * 3.1415 * 2;
  int i;

  memset(&bitmap, 0, sizeof(bitmap));
  memset(&mask, 0, sizeof(mask));

  bitmap[0][0] |= mask[0][0] |= 0xF;
  bitmap[1][0] |= mask[1][0] |= 0xF;
  bitmap[2][0] |= mask[2][0] |= 0xF;
  bitmap[3][0] |= mask[3][0] |= 0xF;

  for (i = 0; i < 7; i++) {
    int x = 7 + cos(angle) * i;
    int y = 7 + sin(angle) * i;

    mask[y][x / 8] |= (1 << (x % 8));
  }
  for (i = 0; i < 64; i++) {
    float v = (i / 63.0) * 3.1415 * 2;
    int x = 7 + cos(v) * 7;
    int y = 7 + sin(v) * 7;

    mask[y][x / 8] |= (1 << (x % 8));
  }

  const int size[2] = {16, 16};
  const int hot_spot[2] = {0, 0};

  GHOST_SetCustomCursorShape(ew->win, &bitmap[0][0], &mask[0][0], size, hot_spot, true);
}

static void extrawindow_handle(void *priv, GHOST_EventHandle evt)
{
  ExtraWindow *ew = priv;
  GHOST_TEventType type = GHOST_GetEventType(evt);
  char buf[256];

  event_to_buf(evt, buf);
  loggerwindow_log(multitestapp_get_logger(ew->app), buf);

  switch (type) {
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *kd = GHOST_GetEventData(evt);
      extrawindow_do_key(ew, kd->key, (type == GHOST_kEventKeyDown));
      break;
    }

    case GHOST_kEventCursorMove: {
      extrawindow_spin_cursor(ew, GHOST_GetEventTime(evt));
      break;
    }

    case GHOST_kEventWindowClose:
      multitestapp_free_extrawindow(ew->app);
      break;
    case GHOST_kEventWindowUpdate:
      extrawindow_do_draw(ew);
      break;
    case GHOST_kEventWindowSize:
      extrawindow_do_reshape(ew);
      break;
  }
}

/**/

ExtraWindow *extrawindow_new(MultiTestApp *app)
{
  GHOST_GPUSettings gpu_settings = {0};
  GHOST_SystemHandle sys = multitestapp_get_system(app);
  GHOST_WindowHandle win;

  win = GHOST_CreateWindow(sys,
                           NULL,
                           "MultiTest:Extra",
                           500,
                           40,
                           400,
                           400,
                           GHOST_kWindowStateNormal,
                           false,
                           GHOST_kDrawingContextTypeOpenGL,
                           gpu_settings);

  if (win) {
    ExtraWindow *ew = MEM_callocN(sizeof(*ew), "mainwindow_new");

    ew->gpu_context = GPU_context_create(win, NULL);
    GPU_init();

    ew->app = app;
    ew->win = win;

    GHOST_SetWindowUserData(ew->win, windowdata_new(ew, extrawindow_handle));

    return ew;
  }
  else {
    return NULL;
  }
}

void extrawindow_free(ExtraWindow *ew)
{
  GHOST_SystemHandle sys = multitestapp_get_system(ew->app);

  windowdata_free(GHOST_GetWindowUserData(ew->win));
  GHOST_DisposeWindow(sys, ew->win);
  MEM_freeN(ew);
}

/*
 * MultiTestApp
 */

struct _MultiTestApp {
  GHOST_SystemHandle sys;
  MainWindow *main;
  LoggerWindow *logger;
  ExtraWindow *extra;

  int exit;
};

static bool multitest_event_handler(GHOST_EventHandle evt, GHOST_TUserDataPtr data)
{
  MultiTestApp *app = data;
  GHOST_WindowHandle win;

  win = GHOST_GetEventWindow(evt);
  if (win && !GHOST_ValidWindow(app->sys, win)) {
    loggerwindow_log(app->logger, "WARNING: bad event, non-valid window\n");
    return true;
  }

  if (win) {
    WindowData *wb = GHOST_GetWindowUserData(win);

    windowdata_handle(wb, evt);
  }
  else {
    GHOST_TEventType type = GHOST_GetEventType(evt);

    /* GHOST_kEventQuit are the only 'system' events,
     * that is, events without a window.
     */
    switch (type) {
      case GHOST_kEventQuitRequest:
        app->exit = 1;
        break;

      default:
        fatal("Unhandled system event: %d (%s)\n", type, eventtype_to_string(type));
        break;
    }
  }

  return true;
}

/**/

MultiTestApp *multitestapp_new(void)
{
  MultiTestApp *app = MEM_mallocN(sizeof(*app), "multitestapp_new");
  GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(multitest_event_handler, app);

  app->sys = GHOST_CreateSystem();
  if (!app->sys) {
    fatal("Unable to create ghost system");
  }
  GPU_backend_ghost_system_set(app->sys);

  if (!GHOST_AddEventConsumer(app->sys, consumer)) {
    fatal("Unable to add multitest event consumer ");
  }

  app->main = mainwindow_new(app);
  if (!app->main) {
    fatal("Unable to create main window");
  }

  app->logger = loggerwindow_new(app);
  if (!app->logger) {
    fatal("Unable to create logger window");
  }

  app->extra = NULL;
  app->exit = 0;

  return app;
}

LoggerWindow *multitestapp_get_logger(MultiTestApp *app)
{
  return app->logger;
}

GHOST_SystemHandle multitestapp_get_system(MultiTestApp *app)
{
  return app->sys;
}

void multitestapp_free_extrawindow(MultiTestApp *app)
{
  extrawindow_free(app->extra);
  app->extra = NULL;
}

void multitestapp_toggle_extra_window(MultiTestApp *app)
{
  if (app->extra) {
    multitestapp_free_extrawindow(app);
  }
  else {
    app->extra = extrawindow_new(app);
  }
}

void multitestapp_exit(MultiTestApp *app)
{
  app->exit = 1;
}

void multitestapp_run(MultiTestApp *app)
{
  while (!app->exit) {
    GHOST_ProcessEvents(app->sys, true);
    GHOST_DispatchEvents(app->sys);
  }
}

void multitestapp_free(MultiTestApp *app)
{
  BLF_exit();
  GPU_exit();

  mainwindow_free(app->main);
  loggerwindow_free(app->logger);
  GHOST_DisposeSystem(app->sys);
  MEM_freeN(app);
}

/***/

int main(int argc, char **argv)
{
  MultiTestApp *app;

  BLF_init();

  app = multitestapp_new();

  multitestapp_run(app);
  multitestapp_free(app);

  return 0;
}
