/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 *
 * Simple test file for the GHOST library.
 * The OpenGL gear code is taken from the Qt sample code which,
 * in turn, is probably taken from somewhere as well.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FALSE 0

#include "GHOST_C-api.h"

#if defined(WIN32) || defined(__APPLE__)
#  ifdef WIN32
#    include <GL/gl.h>
#    include <windows.h>
#  else /* WIN32 */
/* __APPLE__ is defined */
#    include <AGL/gl.h>
#  endif /* WIN32 */
#else    /* defined(WIN32) || defined(__APPLE__) */
#  include <GL/gl.h>
#endif /* defined(WIN32) || defined(__APPLE__) */

static void gearsTimerProc(GHOST_TimerTaskHandle task, uint64_t time);
bool processEvent(GHOST_EventHandle hEvent, GHOST_TUserDataPtr user_data);

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLfloat fAngle = 0.0;
static int sExitRequested = 0;
static GHOST_SystemHandle shSystem = NULL;
static GHOST_WindowHandle sMainWindow = NULL;
static GHOST_WindowHandle sSecondaryWindow = NULL;
static GHOST_TStandardCursor sCursor = GHOST_kStandardCursorFirstCursor;
static GHOST_TimerTaskHandle sTestTimer;
static GHOST_TimerTaskHandle sGearsTimer;

static void testTimerProc(GHOST_TimerTaskHandle task, uint64_t time)
{
  printf("timer1, time=%d\n", (int)time);
}

static void gearGL(
    GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth)
{
  GLint i;
  GLfloat r0, r1, r2;
  GLfloat angle, da;
  GLfloat u, v, len;
  const double pi = 3.14159264;

  r0 = inner_radius;
  r1 = (float)(outer_radius - tooth_depth / 2.0);
  r2 = (float)(outer_radius + tooth_depth / 2.0);

  da = (float)(2.0 * pi / teeth / 4.0);

  glShadeModel(GL_FLAT);
  glNormal3f(0.0, 0.0, 1.0);

  /* draw front face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = (float)(i * 2.0 * pi / teeth);
    glVertex3f((float)(r0 * cos(angle)), (float)(r0 * sin(angle)), (float)(width * 0.5));
    glVertex3f((float)(r1 * cos(angle)), (float)(r1 * sin(angle)), (float)(width * 0.5));
    glVertex3f((float)(r0 * cos(angle)), (float)(r0 * sin(angle)), (float)(width * 0.5));
    glVertex3f((float)(r1 * cos(angle + 3 * da)),
               (float)(r1 * sin(angle + 3 * da)),
               (float)(width * 0.5));
  }
  glEnd();

  /* draw front sides of teeth */
  glBegin(GL_QUADS);
  da = (float)(2.0 * pi / teeth / 4.0);
  for (i = 0; i < teeth; i++) {
    angle = (float)(i * 2.0 * pi / teeth);
    glVertex3f((float)(r1 * cos(angle)), (float)(r1 * sin(angle)), (float)(width * 0.5));
    glVertex3f((float)(r2 * cos(angle + da)), (float)(r2 * sin(angle + da)), (float)(width * 0.5));
    glVertex3f((float)(r2 * cos(angle + 2 * da)),
               (float)(r2 * sin(angle + 2 * da)),
               (float)(width * 0.5));
    glVertex3f((float)(r1 * cos(angle + 3 * da)),
               (float)(r1 * sin(angle + 3 * da)),
               (float)(width * 0.5));
  }
  glEnd();

  glNormal3f(0.0, 0.0, -1.0);

  /* draw back face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = (float)(i * 2.0 * pi / teeth);
    glVertex3f((float)(r1 * cos(angle)), (float)(r1 * sin(angle)), (float)(-width * 0.5));
    glVertex3f((float)(r0 * cos(angle)), (float)(r0 * sin(angle)), (float)(-width * 0.5));
    glVertex3f((float)(r1 * cos(angle + 3 * da)),
               (float)(r1 * sin(angle + 3 * da)),
               (float)(-width * 0.5));
    glVertex3f((float)(r0 * cos(angle)), (float)(r0 * sin(angle)), (float)(-width * 0.5));
  }
  glEnd();

  /* draw back sides of teeth */
  glBegin(GL_QUADS);
  da = (float)(2.0 * pi / teeth / 4.0);
  for (i = 0; i < teeth; i++) {
    angle = (float)(i * 2.0 * pi / teeth);
    glVertex3f((float)(r1 * cos(angle + 3 * da)),
               (float)(r1 * sin(angle + 3 * da)),
               (float)(-width * 0.5));
    glVertex3f((float)(r2 * cos(angle + 2 * da)),
               (float)(r2 * sin(angle + 2 * da)),
               (float)(-width * 0.5));
    glVertex3f(
        (float)(r2 * cos(angle + da)), (float)(r2 * sin(angle + da)), (float)(-width * 0.5));
    glVertex3f((float)(r1 * cos(angle)), (float)(r1 * sin(angle)), (float)(-width * 0.5));
  }
  glEnd();

  /* draw outward faces of teeth */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < teeth; i++) {
    angle = (float)(i * 2.0 * pi / teeth);
    glVertex3f((float)(r1 * cos(angle)), (float)(r1 * sin(angle)), (float)(width * 0.5));
    glVertex3f((float)(r1 * cos(angle)), (float)(r1 * sin(angle)), (float)(-width * 0.5));
    u = (float)(r2 * cos(angle + da) - r1 * cos(angle));
    v = (float)(r2 * sin(angle + da) - r1 * sin(angle));
    len = (float)(sqrt(u * u + v * v));
    u /= len;
    v /= len;
    glNormal3f(v, -u, 0.0);
    glVertex3f((float)(r2 * cos(angle + da)), (float)(r2 * sin(angle + da)), (float)(width * 0.5));
    glVertex3f(
        (float)(r2 * cos(angle + da)), (float)(r2 * sin(angle + da)), (float)(-width * 0.5));
    glNormal3f((float)(cos(angle)), (float)(sin(angle)), 0.0);
    glVertex3f((float)(r2 * cos(angle + 2 * da)),
               (float)(r2 * sin(angle + 2 * da)),
               (float)(width * 0.5));
    glVertex3f((float)(r2 * cos(angle + 2 * da)),
               (float)(r2 * sin(angle + 2 * da)),
               (float)(-width * 0.5));
    u = (float)(r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da));
    v = (float)(r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da));
    glNormal3f(v, -u, 0.0);
    glVertex3f((float)(r1 * cos(angle + 3 * da)),
               (float)(r1 * sin(angle + 3 * da)),
               (float)(width * 0.5));
    glVertex3f((float)(r1 * cos(angle + 3 * da)),
               (float)(r1 * sin(angle + 3 * da)),
               (float)(-width * 0.5));
    glNormal3f((float)(cos(angle)), (float)(sin(angle)), 0.0);
  }
  glVertex3f((float)(r1 * cos(0.0)), (float)(r1 * sin(0.0)), (float)(width * 0.5));
  glVertex3f((float)(r1 * cos(0.0)), (float)(r1 * sin(0.0)), (float)(-width * 0.5));
  glEnd();

  glShadeModel(GL_SMOOTH);

  /* draw inside radius cylinder */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = (float)(i * 2.0 * pi / teeth);
    glNormal3f((float)(-cos(angle)), (float)(-sin(angle)), 0.0);
    glVertex3f((float)(r0 * cos(angle)), (float)(r0 * sin(angle)), (float)(-width * 0.5));
    glVertex3f((float)(r0 * cos(angle)), (float)(r0 * sin(angle)), (float)(width * 0.5));
  }
  glEnd();
}

static void drawGearGL(int id)
{
  static GLfloat pos[4] = {5.0f, 5.0f, 10.0f, 1.0f};
  static GLfloat ared[4] = {0.8f, 0.1f, 0.0f, 1.0f};
  static GLfloat agreen[4] = {0.0f, 0.8f, 0.2f, 1.0f};
  static GLfloat ablue[4] = {0.2f, 0.2f, 1.0f, 1.0f};

  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  switch (id) {
    case 1:
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, ared);
      gearGL(1.0f, 4.0f, 1.0f, 20, 0.7f);
      break;
    case 2:
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, agreen);
      gearGL(0.5f, 2.0f, 2.0f, 10, 0.7f);
      break;
    case 3:
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, ablue);
      gearGL(1.3f, 2.0f, 0.5f, 10, 0.7f);
      break;
    default:
      break;
  }
  glEnable(GL_NORMALIZE);
}

static void drawGL(void)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();

  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);
  glRotatef(view_rotz, 0.0, 0.0, 1.0);

  glPushMatrix();
  glTranslatef(-3.0, -2.0, 0.0);
  glRotatef(fAngle, 0.0, 0.0, 1.0);
  drawGearGL(1);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(3.1f, -2.0f, 0.0f);
  glRotatef((float)(-2.0 * fAngle - 9.0), 0.0, 0.0, 1.0);
  drawGearGL(2);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-3.1f, 2.2f, -1.8f);
  glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
  glRotatef((float)(2.0 * fAngle - 2.0), 0.0, 0.0, 1.0);
  drawGearGL(3);
  glPopMatrix();

  glPopMatrix();
}

static void setViewPortGL(GHOST_WindowHandle hWindow)
{
  GHOST_RectangleHandle hRect = NULL;
  GLfloat w, h;

  GHOST_ActivateWindowDrawingContext(hWindow);
  hRect = GHOST_GetClientBounds(hWindow);

  w = (float)GHOST_GetWidthRectangle(hRect) / (float)GHOST_GetHeightRectangle(hRect);
  h = 1.0;

  glViewport(0, 0, GHOST_GetWidthRectangle(hRect), GHOST_GetHeightRectangle(hRect));

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-w, w, -h, h, 5.0, 60.0);
  // glOrtho(0, bnds.getWidth(), 0, bnds.getHeight(), -10, 10);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);

  glClearColor(.2f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  GHOST_DisposeRectangle(hRect);
}

bool processEvent(GHOST_EventHandle hEvent, GHOST_TUserDataPtr user_data)
{
  bool handled = true;
  int cursor;
  int visibility;
  GHOST_TEventKeyData *keyData = NULL;
  GHOST_TEventWheelData *wheelData = NULL;
  GHOST_WindowHandle window = GHOST_GetEventWindow(hEvent);

  switch (GHOST_GetEventType(hEvent)) {
#if 0
    case GHOST_kEventUnknown:
      break;
    case GHOST_kEventCursorButton:
      break;
    case GHOST_kEventCursorMove:
      break;
#endif
    case GHOST_kEventWheel: {
      wheelData = (GHOST_TEventWheelData *)GHOST_GetEventData(hEvent);
      if (wheelData->value > 0) {
        view_rotz += 5.f;
      }
      else {
        view_rotz -= 5.f;
      }
      break;
    }

    case GHOST_kEventKeyUp:
      break;

    case GHOST_kEventKeyDown: {
      keyData = (GHOST_TEventKeyData *)GHOST_GetEventData(hEvent);
      switch (keyData->key) {
        case GHOST_kKeyC: {
          cursor = sCursor;
          cursor++;
          if (cursor >= GHOST_kStandardCursorNumCursors) {
            cursor = GHOST_kStandardCursorFirstCursor;
          }
          sCursor = (GHOST_TStandardCursor)cursor;
          GHOST_SetCursorShape(window, sCursor);
          break;
        }
        case GHOST_kKeyH: {
          visibility = GHOST_GetCursorVisibility(window);
          GHOST_SetCursorVisibility(window, !visibility);
          break;
        }
        case GHOST_kKeyQ:
          sExitRequested = 1;
        case GHOST_kKeyT:
          if (!sTestTimer) {
            sTestTimer = GHOST_InstallTimer(shSystem, 0, 1000, testTimerProc, NULL);
          }
          else {
            GHOST_RemoveTimer(shSystem, sTestTimer);
            sTestTimer = 0;
          }
          break;
        case GHOST_kKeyW: {
          if (sMainWindow) {
            char *title = GHOST_GetTitle(sMainWindow);
            char *ntitle = malloc(strlen(title) + 2);

            sprintf(ntitle, "%s-", title);
            GHOST_SetTitle(sMainWindow, ntitle);

            free(ntitle);
            free(title);
          }
          break;
        }
        default:
          break;
      }
      break;
    }

    case GHOST_kEventWindowClose: {
      GHOST_WindowHandle window2 = GHOST_GetEventWindow(hEvent);
      if (window2 == sMainWindow) {
        sExitRequested = 1;
      }
      else {
        if (sGearsTimer) {
          GHOST_RemoveTimer(shSystem, sGearsTimer);
          sGearsTimer = 0;
        }
        GHOST_DisposeWindow(shSystem, window2);
      }
      break;
    }

    case GHOST_kEventWindowActivate:
      handled = false;
      break;
    case GHOST_kEventWindowDeactivate:
      handled = false;
      break;
    case GHOST_kEventWindowUpdate: {
      GHOST_WindowHandle window2 = GHOST_GetEventWindow(hEvent);
      if (!GHOST_ValidWindow(shSystem, window2)) {
        break;
      }
      setViewPortGL(window2);
      GHOST_SwapWindowBufferAcquire(window2);
      drawGL();
      GHOST_SwapWindowBufferRelease(window2);
      break;
    }
    default:
      handled = false;
      break;
  }
  return handled;
}

int main(int argc, char **argv)
{
  GHOST_GPUSettings gpu_settings = {0};
  char *title1 = "gears - main window";
  char *title2 = "gears - secondary window";
  GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(processEvent, NULL);

  /* Create the system */
  shSystem = GHOST_CreateSystem();
  GHOST_AddEventConsumer(shSystem, consumer);

  if (shSystem) {
    /* Create the main window */
    sMainWindow = GHOST_CreateWindow(shSystem,
                                     NULL,
                                     title1,
                                     10,
                                     64,
                                     320,
                                     200,
                                     GHOST_kWindowStateNormal,
                                     false,
                                     GHOST_kDrawingContextTypeOpenGL,
                                     gpu_settings);
    if (!sMainWindow) {
      printf("could not create main window\n");
      exit(-1);
    }

    /* Create a secondary window */
    sSecondaryWindow = GHOST_CreateWindow(shSystem,
                                          NULL,
                                          title2,
                                          340,
                                          64,
                                          320,
                                          200,
                                          GHOST_kWindowStateNormal,
                                          false,
                                          GHOST_kDrawingContextTypeOpenGL,
                                          gpu_settings);
    if (!sSecondaryWindow) {
      printf("could not create secondary window\n");
      exit(-1);
    }

    /* Install a timer to have the gears running */
    sGearsTimer = GHOST_InstallTimer(shSystem, 0, 10, gearsTimerProc, sMainWindow);

    /* Enter main loop */
    while (!sExitRequested) {
      if (!GHOST_ProcessEvents(shSystem, false)) {
#ifdef WIN32
        /* If there were no events, be nice to other applications */
        Sleep(10);
#endif
      }
      GHOST_DispatchEvents(shSystem);
    }
  }

  /* Dispose windows */
  if (GHOST_ValidWindow(shSystem, sMainWindow)) {
    GHOST_DisposeWindow(shSystem, sMainWindow);
  }
  if (GHOST_ValidWindow(shSystem, sSecondaryWindow)) {
    GHOST_DisposeWindow(shSystem, sSecondaryWindow);
  }

  /* Dispose the system */
  GHOST_DisposeSystem(shSystem);

  return 0;
}

static void gearsTimerProc(GHOST_TimerTaskHandle hTask, uint64_t time)
{
  GHOST_WindowHandle hWindow = NULL;
  fAngle += 2.0;
  view_roty += 1.0;
  hWindow = (GHOST_WindowHandle)GHOST_GetTimerTaskUserData(hTask);
  if (GHOST_ValidWindow(shSystem, hWindow)) {
    GHOST_InvalidateWindow(hWindow);
  }
}
