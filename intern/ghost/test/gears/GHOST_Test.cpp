/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 * Simple test file for the GHOST library.
 * The OpenGL gear code is taken from the Qt sample code which,
 * in turn, is probably taken from somewhere as well.
 * Stereo code by Raymond de Vries, januari 2002
 */

#include <iostream>
#include <math.h>
#include <string>

#if defined(WIN32) || defined(__APPLE__)
#  ifdef WIN32
#    include <atlbase.h>
#    include <windows.h>

#    include <GL/gl.h>
#  else  // WIN32 \
         // __APPLE__ is defined
#    include <AGL/gl.h>
#  endif  // WIN32
#else     // defined(WIN32) || defined(__APPLE__)
#  include <GL/gl.h>
#endif  // defined(WIN32) || defined(__APPLE__)

#include "GHOST_Rect.hh"

#include "GHOST_IEvent.hh"
#include "GHOST_IEventConsumer.hh"
#include "GHOST_ISystem.hh"

#define LEFT_EYE 0
#define RIGHT_EYE 1

static bool nVidiaWindows;  // very dirty but hey, it's for testing only

static void gearsTimerProc(GHOST_ITimerTask *task, uint64_t time);

static class Application *fApp;
static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLfloat fAngle = 0.0;
static GHOST_ISystem *fSystem = 0;

void StereoProjection(float left,
                      float right,
                      float bottom,
                      float top,
                      float nearplane,
                      float farplane,
                      float zero_plane,
                      float dist,
                      float eye);

static void testTimerProc(GHOST_ITimerTask * /*task*/, uint64_t time)
{
  std::cout << "timer1, time=" << int(time) << "\n";
}

static void gearGL(
    GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth)
{
  GLint i;
  GLfloat r0, r1, r2;
  GLfloat angle, da;
  GLfloat u, v, len;

  r0 = inner_radius;
  r1 = outer_radius - tooth_depth / 2.0;
  r2 = outer_radius + tooth_depth / 2.0;

  const double pi = 3.14159264;
  da = 2.0 * pi / teeth / 4.0;

  glShadeModel(GL_FLAT);
  glNormal3f(0.0, 0.0, 1.0);

  /* draw front face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * pi / teeth;
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
  }
  glEnd();

  /* draw front sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * pi / teeth / 4.0;
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * pi / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
  }
  glEnd();

  glNormal3f(0.0, 0.0, -1.0);

  /* draw back face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * pi / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
  }
  glEnd();

  /* draw back sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * pi / teeth / 4.0;
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * pi / teeth;
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
  }
  glEnd();

  /* draw outward faces of teeth */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * pi / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    u = r2 * cos(angle + da) - r1 * cos(angle);
    v = r2 * sin(angle + da) - r1 * sin(angle);
    len = sqrt(u * u + v * v);
    u /= len;
    v /= len;
    glNormal3f(v, -u, 0.0);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
    u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
    v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
    glNormal3f(v, -u, 0.0);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
  }
  glVertex3f(r1 * cos(0.0), r1 * sin(0.0), width * 0.5);
  glVertex3f(r1 * cos(0.0), r1 * sin(0.0), -width * 0.5);
  glEnd();

  glShadeModel(GL_SMOOTH);

  /* draw inside radius cylinder */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * pi / teeth;
    glNormal3f(-cos(angle), -sin(angle), 0.0);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
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

void RenderCamera()
{
  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);
  glRotatef(view_rotz, 0.0, 0.0, 1.0);
}

void RenderScene()
{
  glPushMatrix();
  glTranslatef(-3.0, -2.0, 0.0);
  glRotatef(fAngle, 0.0, 0.0, 1.0);
  drawGearGL(1);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(3.1f, -2.0f, 0.0f);
  glRotatef(-2.0 * fAngle - 9.0, 0.0, 0.0, 1.0);
  drawGearGL(2);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-3.1f, 2.2f, -1.8f);
  glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
  glRotatef(2.0 * fAngle - 2.0, 0.0, 0.0, 1.0);
  drawGearGL(3);
  glPopMatrix();
}

static void View(GHOST_IWindow *window, bool stereo, int eye = 0)
{
  window->activateDrawingContext();
  GHOST_Rect bnds;
  int noOfScanlines = 0, lowerScanline = 0;
  /* hard coded for testing purposes, display device dependent */
  int verticalBlankingInterval = 32;
  float left, right, bottom, top;
  float nearplane, farplane, zeroPlane, distance;
  float eyeSeparation = 0.62f;
  window->getClientBounds(bnds);

  // viewport
  if (stereo) {
    if (nVidiaWindows) {
      // handled by nVidia driver so act as normal (explicitly put here since
      // it -is- stereo)
      glViewport(0, 0, bnds.getWidth(), bnds.getHeight());
    }
    else {  // generic cross platform above-below stereo
      noOfScanlines = (bnds.getHeight() - verticalBlankingInterval) / 2;
      switch (eye) {
        case LEFT_EYE:
          // upper half of window
          lowerScanline = bnds.getHeight() - noOfScanlines;
          break;
        case RIGHT_EYE:
          // lower half of window
          lowerScanline = 0;
          break;
      }
    }
  }
  else {
    noOfScanlines = bnds.getHeight();
    lowerScanline = 0;
  }

  glViewport(0, lowerScanline, bnds.getWidth(), noOfScanlines);

  // projection
  left = -6.0;
  right = 6.0;
  bottom = -4.8f;
  top = 4.8f;
  nearplane = 5.0;
  farplane = 60.0;

  if (stereo) {
    zeroPlane = 0.0;
    distance = 14.5;
    switch (eye) {
      case LEFT_EYE:
        StereoProjection(left,
                         right,
                         bottom,
                         top,
                         nearplane,
                         farplane,
                         zeroPlane,
                         distance,
                         -eyeSeparation / 2.0);
        break;
      case RIGHT_EYE:
        StereoProjection(left,
                         right,
                         bottom,
                         top,
                         nearplane,
                         farplane,
                         zeroPlane,
                         distance,
                         eyeSeparation / 2.0);
        break;
    }
  }
  else {
    //      left = -w;
    //      right = w;
    //      bottom = -h;
    //      top = h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(left, right, bottom, top, 5.0, 60.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -40.0);
  }

  glClearColor(.2f, 0.0f, 0.0f, 0.0f);
}

void StereoProjection(float left,
                      float right,
                      float bottom,
                      float top,
                      float nearplane,
                      float farplane,
                      float zero_plane,
                      float dist,
                      float eye)
/* Perform the perspective projection for one eye's sub-field.
 * The projection is in the direction of the negative z axis.
 *
 * -6.0, 6.0, -4.8, 4.8,
 * left, right, bottom, top = the coordinate range, in the plane of zero
 * parallax setting, which will be displayed on the screen.  The
 * ratio between (right-left) and (top-bottom) should equal the aspect
 * ratio of the display.
 *
 * 6.0, -6.0,
 * near, far = the z-coordinate values of the clipping planes.
 *
 * 0.0,
 * zero_plane = the z-coordinate of the plane of zero parallax setting.
 *
 * 14.5,
 * dist = the distance from the center of projection to the plane
 * of zero parallax.
 *
 * -0.31
 * eye = half the eye separation; positive for the right eye sub-field,
 * negative for the left eye sub-field.
 */
{
  float xmid, ymid, clip_near, clip_far, topw, bottomw, leftw, rightw, dx, dy, n_over_d;

  dx = right - left;
  dy = top - bottom;

  xmid = (right + left) / 2.0;
  ymid = (top + bottom) / 2.0;

  clip_near = dist + zero_plane - nearplane;
  clip_far = dist + zero_plane - farplane;

  n_over_d = clip_near / dist;

  topw = n_over_d * dy / 2.0;
  bottomw = -topw;
  rightw = n_over_d * (dx / 2.0 - eye);
  leftw = n_over_d * (-dx / 2.0 - eye);

  /* Need to be in projection mode for this. */
  glLoadIdentity();
  glFrustum(leftw, rightw, bottomw, topw, clip_near, clip_far);

  glTranslatef(-xmid - eye, -ymid, -zero_plane - dist);
}

class Application : public GHOST_IEventConsumer {
 public:
  Application(GHOST_ISystem *system);
  ~Application();
  virtual bool processEvent(GHOST_IEvent *event);

  GHOST_ISystem *system_;
  GHOST_IWindow *main_window_;
  GHOST_IWindow *secondary_window_;
  GHOST_ITimerTask *gears_timer_, *test_timer_;
  GHOST_TStandardCursor cursor_;
  bool exit_requested_;

  bool stereo;
};

Application::Application(GHOST_ISystem *system)
    : system_(system),
      main_window_(0),
      secondary_window_(0),
      gears_timer_(0),
      test_timer_(0),
      cursor_(GHOST_kStandardCursorFirstCursor),
      exit_requested_(false),
      stereo(false)
{
  GHOST_GPUSettings gpu_settings = {0};
  gpu_settings.context_type = GHOST_kDrawingContextTypeOpenGL;
  fApp = this;

  // Create the main window
  main_window_ = system->createWindow(
      "gears - main window", 10, 64, 320, 200, GHOST_kWindowStateNormal, gpu_settings);

  if (!main_window_) {
    std::cout << "could not create main window\n";
    exit(-1);
  }

  // Create a secondary window
  secondary_window_ = system->createWindow(
      "gears - secondary window", 340, 64, 320, 200, GHOST_kWindowStateNormal, gpu_settings);
  if (!secondary_window_) {
    std::cout << "could not create secondary window\n";
    exit(-1);
  }

  // Install a timer to have the gears running
  gears_timer_ = system->installTimer(0 /*delay*/, 20 /*interval*/, gearsTimerProc, main_window_);
}

Application::~Application()
{
  // Dispose windows
  if (system_->validWindow(main_window_)) {
    system_->disposeWindow(main_window_);
  }
  if (system_->validWindow(secondary_window_)) {
    system_->disposeWindow(secondary_window_);
  }
}

bool Application::processEvent(const GHOST_IEvent *event)
{
  GHOST_IWindow *window = event->getWindow();
  bool handled = true;

  switch (event->getType()) {
#if 0
    case GHOST_kEventUnknown:
      break;
    case GHOST_kEventCursorButton:
      std::cout << "GHOST_kEventCursorButton";
      break;
    case GHOST_kEventCursorMove:
      std::cout << "GHOST_kEventCursorMove";
      break;
#endif
    case GHOST_kEventWheel: {
      GHOST_TEventWheelData *wheelData = (GHOST_TEventWheelData *)event->getData();
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
      GHOST_TEventKeyData *keyData = (GHOST_TEventKeyData *)event->getData();
      switch (keyData->key) {
        case GHOST_kKeyC: {
          int cursor = cursor_;
          cursor++;
          if (cursor >= GHOST_kStandardCursorNumCursors) {
            cursor = GHOST_kStandardCursorFirstCursor;
          }
          cursor_ = (GHOST_TStandardCursor)cursor;
          window->setCursorShape(cursor_);
          break;
        }

        case GHOST_kKeyE: {
          int x = 200, y = 200;
          system_->setCursorPosition(x, y);
          break;
        }

        case GHOST_kKeyH:
          window->setCursorVisibility(!window->getCursorVisibility());
          break;

        case GHOST_kKeyM: {
          bool down = false;
          system_->getModifierKeyState(GHOST_kModifierKeyLeftShift, down);
          if (down) {
            std::cout << "left shift down\n";
          }
          system_->getModifierKeyState(GHOST_kModifierKeyRightShift, down);
          if (down) {
            std::cout << "right shift down\n";
          }
          system_->getModifierKeyState(GHOST_kModifierKeyLeftAlt, down);
          if (down) {
            std::cout << "left Alt down\n";
          }
          system_->getModifierKeyState(GHOST_kModifierKeyRightAlt, down);
          if (down) {
            std::cout << "right Alt down\n";
          }
          system_->getModifierKeyState(GHOST_kModifierKeyLeftControl, down);
          if (down) {
            std::cout << "left control down\n";
          }
          system_->getModifierKeyState(GHOST_kModifierKeyRightControl, down);
          if (down) {
            std::cout << "right control down\n";
          }
          break;
        }

        case GHOST_kKeyQ:
          exit_requested_ = true;
          break;

        case GHOST_kKeyS:  // toggle mono and stereo
          if (stereo) {
            stereo = false;
          }
          else {
            stereo = true;
          }
          break;

        case GHOST_kKeyT:
          if (!test_timer_) {
            test_timer_ = system_->installTimer(0, 1000, testTimerProc);
          }

          else {
            system_->removeTimer(test_timer_);
            test_timer_ = 0;
          }

          break;

        case GHOST_kKeyW:
          if (main_window_) {
            std::string title = main_window_->getTitle();
            title += "-";
            main_window_->setTitle(title);
          }
          break;

        default:
          break;
      }
      break;
    }

    case GHOST_kEventWindowClose: {
      GHOST_IWindow *window2 = event->getWindow();
      if (window2 == main_window_) {
        exit_requested_ = true;
      }
      else {
        system_->disposeWindow(window2);
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
      GHOST_IWindow *window2 = event->getWindow();
      if (!system_->validWindow(window2)) {
        break;
      }

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      if (stereo) {
        View(window2, stereo, LEFT_EYE);
        glPushMatrix();
        RenderCamera();
        RenderScene();
        glPopMatrix();

        View(window2, stereo, RIGHT_EYE);
        glPushMatrix();
        RenderCamera();
        RenderScene();
        glPopMatrix();
      }
      else {
        View(window2, stereo);
        glPushMatrix();
        RenderCamera();
        RenderScene();
        glPopMatrix();
      }
      window2->swapBufferRelease();
      break;
    }

    default:
      handled = false;
      break;
  }
  return handled;
}

int main(int /*argc*/, char ** /*argv*/)
{
  nVidiaWindows = false;
  //  nVidiaWindows = true;

#ifdef WIN32
  /* Set a couple of settings in the registry for the nVidia detonator driver.
   * So this is very specific...
   */
  if (nVidiaWindows) {
    LONG lresult;
    HKEY hkey = 0;
    DWORD dwd = 0;
    // unsigned char buffer[128];

    CRegKey regkey;
    // DWORD keyValue;
    // lresult = regkey.Open(
    //     HKEY_LOCAL_MACHINE, "SOFTWARE\\NVIDIA Corporation\\Global\\Stereo3D\\StereoEnable");
    lresult = regkey.Open(HKEY_LOCAL_MACHINE,
                          "SOFTWARE\\NVIDIA Corporation\\Global\\Stereo3D\\StereoEnable",
                          KEY_ALL_ACCESS);

    if (lresult == ERROR_SUCCESS) {
      printf("Successfully opened key\n");
    }
#  if 0
    lresult = regkey.QueryValue(&keyValue, "StereoEnable");
    if (lresult == ERROR_SUCCESS) {
      printf("Successfully queried key\n");
    }
#  endif
    lresult = regkey.SetValue(
        HKEY_LOCAL_MACHINE, "SOFTWARE\\NVIDIA Corporation\\Global\\Stereo3D\\StereoEnable", "1");
    if (lresult == ERROR_SUCCESS) {
      printf("Successfully set value for key\n");
    }
    regkey.Close();
    if (lresult == ERROR_SUCCESS) {
      printf("Successfully closed key\n");
    }
    //      regkey.Write("2");
  }
#endif  // WIN32

  // Create the system
  GHOST_ISystem::createSystem();
  fSystem = GHOST_ISystem::getSystem();

  if (fSystem) {
    // Create an application object
    Application app(fSystem);

    // Add the application as event consumer
    fSystem->addEventConsumer(&app);

    // Enter main loop
    while (!app.exit_requested_) {
      // printf("main: loop\n");
      fSystem->processEvents(true);
      fSystem->dispatchEvents();
    }

    // Remove so ghost doesn't do a double free
    fSystem->removeEventConsumer(&app);
  }

  // Dispose the system
  GHOST_ISystem::disposeSystem();

  return 0;
}

static void gearsTimerProc(GHOST_ITimerTask *task, uint64_t /*time*/)
{
  fAngle += 2.0;
  view_roty += 1.0;
  GHOST_IWindow *window = (GHOST_IWindow *)task->getUserData();
  if (fSystem->validWindow(window)) {
    window->invalidate();
  }
}
