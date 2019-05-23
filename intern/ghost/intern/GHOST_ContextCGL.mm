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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file ghost/intern/GHOST_ContextCGL.mm
 *  \ingroup GHOST
 *
 * Definition of GHOST_ContextCGL class.
 */

#include "GHOST_ContextCGL.h"

#include <Cocoa/Cocoa.h>

#include <vector>
#include <cassert>

NSOpenGLContext *GHOST_ContextCGL::s_sharedOpenGLContext = nil;
int GHOST_ContextCGL::s_sharedCount = 0;

GHOST_ContextCGL::GHOST_ContextCGL(bool stereoVisual, NSOpenGLView *openGLView)
    : GHOST_Context(stereoVisual), m_openGLView(openGLView), m_openGLContext(nil), m_debug(false)
{
#if defined(WITH_GL_PROFILE_CORE)
  m_coreProfile = true;
#else
  m_coreProfile = false;
#endif
}

GHOST_ContextCGL::~GHOST_ContextCGL()
{
  if (m_openGLContext != nil) {
    if (m_openGLContext == [NSOpenGLContext currentContext]) {
      [NSOpenGLContext clearCurrentContext];

      if (m_openGLView) {
        [m_openGLView clearGLContext];
      }
    }

    if (m_openGLContext != s_sharedOpenGLContext || s_sharedCount == 1) {
      assert(s_sharedCount > 0);

      s_sharedCount--;

      if (s_sharedCount == 0)
        s_sharedOpenGLContext = nil;

      [m_openGLContext release];
    }
  }
}

GHOST_TSuccess GHOST_ContextCGL::swapBuffers()
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [m_openGLContext flushBuffer];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::setSwapInterval(int interval)
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [m_openGLContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::getSwapInterval(int &intervalOut)
{
  if (m_openGLContext != nil) {
    GLint interval;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [m_openGLContext getValues:&interval forParameter:NSOpenGLCPSwapInterval];

    [pool drain];

    intervalOut = static_cast<int>(interval);

    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::activateDrawingContext()
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [m_openGLContext makeCurrentContext];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::releaseDrawingContext()
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSOpenGLContext clearCurrentContext];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextCGL::updateDrawingContext()
{
  if (m_openGLContext != nil) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [m_openGLContext update];
    [pool drain];
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

static void makeAttribList(std::vector<NSOpenGLPixelFormatAttribute> &attribs,
                           bool coreProfile,
                           bool stereoVisual,
                           bool needAlpha,
                           bool softwareGL)
{
  attribs.clear();

  attribs.push_back(NSOpenGLPFAOpenGLProfile);
  attribs.push_back(coreProfile ? NSOpenGLProfileVersion3_2Core : NSOpenGLProfileVersionLegacy);

  // Pixel Format Attributes for the windowed NSOpenGLContext
  attribs.push_back(NSOpenGLPFADoubleBuffer);

  if (softwareGL) {
    attribs.push_back(NSOpenGLPFARendererID);
    attribs.push_back(kCGLRendererGenericFloatID);
  }
  else {
    attribs.push_back(NSOpenGLPFAAccelerated);
    attribs.push_back(NSOpenGLPFANoRecovery);
  }

  attribs.push_back(NSOpenGLPFAAllowOfflineRenderers);  // for automatic GPU switching

  if (stereoVisual)
    attribs.push_back(NSOpenGLPFAStereo);

  if (needAlpha) {
    attribs.push_back(NSOpenGLPFAAlphaSize);
    attribs.push_back((NSOpenGLPixelFormatAttribute)8);
  }

  attribs.push_back((NSOpenGLPixelFormatAttribute)0);
}

// TODO(merwin): make this available to all platforms
static void getVersion(int *major, int *minor)
{
#if 1  // legacy GL
  sscanf((const char *)glGetString(GL_VERSION), "%d.%d", major, minor);
#else  // 3.0+
  glGetIntegerv(GL_MAJOR_VERSION, major);
  glGetIntegerv(GL_MINOR_VERSION, minor);
#endif
}

GHOST_TSuccess GHOST_ContextCGL::initializeDrawingContext()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  std::vector<NSOpenGLPixelFormatAttribute> attribs;
  attribs.reserve(40);

#ifdef GHOST_OPENGL_ALPHA
  static const bool needAlpha = true;
#else
  static const bool needAlpha = false;
#endif

  static bool softwareGL = getenv("BLENDER_SOFTWAREGL");  // command-line argument would be better
  GLint major = 0, minor = 0;
  NSOpenGLPixelFormat *pixelFormat;
  // TODO: keep pixel format for subsequent windows/contexts instead of recreating each time

  makeAttribList(attribs, m_coreProfile, m_stereoVisual, needAlpha, softwareGL);

  pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:&attribs[0]];

  if (pixelFormat == nil)
    goto error;

  m_openGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                               shareContext:s_sharedOpenGLContext];
  [pixelFormat release];

  [m_openGLContext makeCurrentContext];

  getVersion(&major, &minor);
  if (m_debug) {
    fprintf(stderr, "OpenGL version %d.%d%s\n", major, minor, softwareGL ? " (software)" : "");
    fprintf(stderr, "Renderer: %s\n", glGetString(GL_RENDERER));
  }

#ifdef GHOST_WAIT_FOR_VSYNC
  {
    GLint swapInt = 1;
    /* wait for vsync, to avoid tearing artifacts */
    [m_openGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
  }
#endif

  initContextGLEW();

  if (m_openGLView) {
    [m_openGLView setOpenGLContext:m_openGLContext];
    [m_openGLContext setView:m_openGLView];
    initClearGL();
  }

  [m_openGLContext flushBuffer];

  if (s_sharedCount == 0)
    s_sharedOpenGLContext = m_openGLContext;

  s_sharedCount++;

  [pool drain];

  return GHOST_kSuccess;

error:

  [pixelFormat release];

  [pool drain];

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextCGL::releaseNativeHandles()
{
  m_openGLContext = NULL;
  m_openGLView = NULL;

  return GHOST_kSuccess;
}
