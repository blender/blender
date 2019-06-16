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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#define FALSE 0

#ifdef _MSC_VER
#  pragma warning(disable : 4244 4305)
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "GL.h"

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#ifdef USE_BMF
#  include "BMF_Api.h"
#else
#  include "BLF_api.h"
extern int datatoc_bfont_ttf_size;
extern char datatoc_bfont_ttf[];

/* cheat */
char U[1024] = {0};
#endif

#include "Util.h"
#include "Basic.h"
#include "ScrollBar.h"
#include "EventToBuf.h"

#include "WindowData.h"

/***/

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

      if (ltidx == corner)
        glColor3f(col[0] * ltf, col[1] * ltf, col[2] * ltf);
      if (dkidx == corner)
        glColor3f(col[0] * dkf, col[1] * dkf, col[2] * dkf);

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

  if (mw->lmbut[0]) {
    glClearColor(0.5, 0.5, 0.5, 1);
  }
  else {
    glClearColor(1, 1, 1, 1);
  }
  glClear(GL_COLOR_BUFFER_BIT);

  glColor3f(0.5, 0.6, 0.8);
  glRecti(mw->tmouse[0] - 5, mw->tmouse[1] - 5, mw->tmouse[0] + 5, mw->tmouse[1] + 5);

  GHOST_SwapWindowBuffers(mw->win);
}

static void mainwindow_do_reshape(MainWindow *mw)
{
  GHOST_RectangleHandle bounds = GHOST_GetClientBounds(mw->win);

  GHOST_ActivateWindowDrawingContext(mw->win);

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
      if (press)
        GHOST_SetCursorShape(mw->win,
                             (GHOST_TStandardCursor)(rand() % (GHOST_kStandardCursorNumCursors)));
      break;
    case GHOST_kKeyLeftBracket:
      if (press)
        GHOST_SetCursorVisibility(mw->win, 0);
      break;
    case GHOST_kKeyRightBracket:
      if (press)
        GHOST_SetCursorVisibility(mw->win, 1);
      break;
    case GHOST_kKeyE:
      if (press)
        multitestapp_toggle_extra_window(mw->app);
      break;
    case GHOST_kKeyQ:
      if (press)
        multitestapp_exit(mw->app);
      break;
    case GHOST_kKeyT:
      if (press)
        mainwindow_log(mw, "TextTest~|`hello`\"world\",<>/");
      break;
    case GHOST_kKeyR:
      if (press) {
        int i;

        mainwindow_log(mw, "Invalidating window 10 times");
        for (i = 0; i < 10; i++)
          GHOST_InvalidateWindow(mw->win);
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

static void mainwindow_timer_proc(GHOST_TimerTaskHandle task, GHOST_TUns64 time)
{
  MainWindow *mw = GHOST_GetTimerTaskUserData(task);
  char buf[64];

  sprintf(buf, "timer: %6.2f", (double)((GHOST_TInt64)time) / 1000);
  mainwindow_log(mw, buf);
}

MainWindow *mainwindow_new(MultiTestApp *app)
{
  GHOST_SystemHandle sys = multitestapp_get_system(app);
  GHOST_WindowHandle win;
  GHOST_GLSettings glSettings = {0};

  win = GHOST_CreateWindow(sys,
                           "MultiTest:Main",
                           40,
                           40,
                           400,
                           400,
                           GHOST_kWindowStateNormal,
                           GHOST_kDrawingContextTypeOpenGL,
                           glSettings);

  if (win) {
    MainWindow *mw = MEM_callocN(sizeof(*mw), "mainwindow_new");
    mw->app = app;
    mw->win = win;

    GHOST_SetWindowUserData(mw->win, windowdata_new(mw, mainwindow_handle));

    GHOST_InstallTimer(sys, 1000, 10000, mainwindow_timer_proc, mw);

    return mw;
  }
  else {
    return NULL;
  }
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

#ifdef USE_BMF
  BMF_Font *font;
#else
  int font;
#endif
  int fonttexid;
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

  if (lw->fonttexid != -1) {
    glBindTexture(GL_TEXTURE_2D, lw->fonttexid);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
  }
  glColor3f(0, 0, 0);
  for (i = 0; i < ndisplines; i++) {
    /* stored in reverse order */
    char *line = lw->loglines[(lw->nloglines - 1) - (i + startline)];
    int x_pos = lw->textarea[0][0] + 4;
    int y_pos = lw->textarea[0][1] + 4 + i * lw->fontheight;

#ifdef USE_BMF
    if (lw->fonttexid == -1) {
      glRasterPos2i(x_pos, y_pos);
      BMF_DrawString(lw->font, line);
    }
    else {
      BMF_DrawStringTexture(lw->font, line, x_pos, y_pos, 0.0);
    }
#else
    BLF_position(lw->font, x_pos, y_pos, 0.0);
    BLF_draw(lw->font, line, 256);  // XXX
#endif
  }

#ifdef USE_BMF
  if (lw->fonttexid != -1) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
  }
#endif

  GHOST_SwapWindowBuffers(lw->win);
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
      if (press)
        multitestapp_exit(lw->app);
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
  GHOST_GLSettings glSettings = {0};
  GHOST_SystemHandle sys = multitestapp_get_system(app);
  GHOST_TUns32 screensize[2];
  GHOST_WindowHandle win;

  GHOST_GetMainDisplayDimensions(sys, &screensize[0], &screensize[1]);
  win = GHOST_CreateWindow(sys,
                           "MultiTest:Logger",
                           40,
                           screensize[1] - 432,
                           800,
                           300,
                           GHOST_kWindowStateNormal,
                           GHOST_kDrawingContextTypeOpenGL,
                           glSettings);

  if (win) {
    LoggerWindow *lw = MEM_callocN(sizeof(*lw), "loggerwindow_new");
    int bbox[2][2];
    lw->app = app;
    lw->win = win;

#ifdef USE_BMF
    lw->font = BMF_GetFont(BMF_kScreen12);
    lw->fonttexid = BMF_GetFontTexture(lw->font);

    BMF_GetBoundingBox(lw->font, &bbox[0][0], &bbox[0][1], &bbox[1][0], &bbox[1][1]);
    lw->fontheight = rect_height(bbox);
#else
    lw->font = BLF_load_mem("default", (unsigned char *)datatoc_bfont_ttf, datatoc_bfont_ttf_size);
    BLF_size(lw->font, 11, 72);
    lw->fontheight = BLF_height(lw->font, "A_", 2);
#endif

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

  int size[2];
} ExtraWindow;

static void extrawindow_do_draw(ExtraWindow *ew)
{
  GHOST_ActivateWindowDrawingContext(ew->win);

  glClearColor(1, 1, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  glColor3f(0.8, 0.8, 0.8);
  glRecti(10, 10, ew->size[0] - 10, ew->size[1] - 10);

  GHOST_SwapWindowBuffers(ew->win);
}

static void extrawindow_do_reshape(ExtraWindow *ew)
{
  GHOST_RectangleHandle bounds = GHOST_GetClientBounds(ew->win);

  GHOST_ActivateWindowDrawingContext(ew->win);

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
      if (press)
        multitestapp_toggle_extra_window(ew->app);
      break;
  }
}

static void extrawindow_spin_cursor(ExtraWindow *ew, GHOST_TUns64 time)
{
  GHOST_TUns8 bitmap[16][2];
  GHOST_TUns8 mask[16][2];
  double ftime = (double)((GHOST_TInt64)time) / 1000;
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

  GHOST_SetCustomCursorShape(ew->win, bitmap, mask, 16, 16, 0, 0, true);
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
  GHOST_GLSettings glSettings = {0};
  GHOST_SystemHandle sys = multitestapp_get_system(app);
  GHOST_WindowHandle win;

  win = GHOST_CreateWindow(sys,
                           "MultiTest:Extra",
                           500,
                           40,
                           400,
                           400,
                           GHOST_kWindowStateNormal,
                           GHOST_kDrawingContextTypeOpenGL,
                           glSettings);

  if (win) {
    ExtraWindow *ew = MEM_callocN(sizeof(*ew), "mainwindow_new");
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

static int multitest_event_handler(GHOST_EventHandle evt, GHOST_TUserDataPtr data)
{
  MultiTestApp *app = data;
  GHOST_WindowHandle win;

  win = GHOST_GetEventWindow(evt);
  if (win && !GHOST_ValidWindow(app->sys, win)) {
    loggerwindow_log(app->logger, "WARNING: bad event, non-valid window\n");
    return 1;
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
      case GHOST_kEventQuit:
        app->exit = 1;
        break;

      default:
        fatal("Unhandled system event: %d (%s)\n", type, eventtype_to_string(type));
        break;
    }
  }

  return 1;
}

/**/

MultiTestApp *multitestapp_new(void)
{
  MultiTestApp *app = MEM_mallocN(sizeof(*app), "multitestapp_new");
  GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(multitest_event_handler, app);

  app->sys = GHOST_CreateSystem();
  if (!app->sys)
    fatal("Unable to create ghost system");

  if (!GHOST_AddEventConsumer(app->sys, consumer))
    fatal("Unable to add multitest event consumer ");

  app->main = mainwindow_new(app);
  if (!app->main)
    fatal("Unable to create main window");

  app->logger = loggerwindow_new(app);
  if (!app->logger)
    fatal("Unable to create logger window");

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
    GHOST_ProcessEvents(app->sys, 1);
    GHOST_DispatchEvents(app->sys);
  }
}

void multitestapp_free(MultiTestApp *app)
{
  mainwindow_free(app->main);
  loggerwindow_free(app->logger);
  GHOST_DisposeSystem(app->sys);
  MEM_freeN(app);
}

/***/

int main(int argc, char **argv)
{
  MultiTestApp *app;

#ifndef USE_BMF
  BLF_init();
#endif

  app = multitestapp_new();

  multitestapp_run(app);
  multitestapp_free(app);

  return 0;
}
