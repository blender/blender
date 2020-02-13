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

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextWGL class.
 */

#include "GHOST_ContextWGL.h"

#include <tchar.h>

#include <cstdio>
#include <cassert>
#include <vector>

HGLRC GHOST_ContextWGL::s_sharedHGLRC = NULL;
int GHOST_ContextWGL::s_sharedCount = 0;

/* Some third-generation Intel video-cards are constantly bring problems */
static bool is_crappy_intel_card()
{
  return strstr((const char *)glGetString(GL_VENDOR), "Intel") != NULL;
}

GHOST_ContextWGL::GHOST_ContextWGL(bool stereoVisual,
                                   bool alphaBackground,
                                   HWND hWnd,
                                   HDC hDC,
                                   int contextProfileMask,
                                   int contextMajorVersion,
                                   int contextMinorVersion,
                                   int contextFlags,
                                   int contextResetNotificationStrategy)
    : GHOST_Context(stereoVisual),
      m_hWnd(hWnd),
      m_hDC(hDC),
      m_contextProfileMask(contextProfileMask),
      m_contextMajorVersion(contextMajorVersion),
      m_contextMinorVersion(contextMinorVersion),
      m_contextFlags(contextFlags),
      m_alphaBackground(alphaBackground),
      m_contextResetNotificationStrategy(contextResetNotificationStrategy),
      m_hGLRC(NULL)
#ifndef NDEBUG
      ,
      m_dummyVendor(NULL),
      m_dummyRenderer(NULL),
      m_dummyVersion(NULL)
#endif
{
  assert(m_hDC != NULL);
}

GHOST_ContextWGL::~GHOST_ContextWGL()
{
  if (m_hGLRC != NULL) {
    if (m_hGLRC == ::wglGetCurrentContext())
      WIN32_CHK(::wglMakeCurrent(NULL, NULL));

    if (m_hGLRC != s_sharedHGLRC || s_sharedCount == 1) {
      assert(s_sharedCount > 0);

      s_sharedCount--;

      if (s_sharedCount == 0)
        s_sharedHGLRC = NULL;

      WIN32_CHK(::wglDeleteContext(m_hGLRC));
    }
  }

#ifndef NDEBUG
  if (m_dummyRenderer) {
    free((void *)m_dummyRenderer);
    free((void *)m_dummyVendor);
    free((void *)m_dummyVersion);
  }
#endif
}

GHOST_TSuccess GHOST_ContextWGL::swapBuffers()
{
  return WIN32_CHK(::SwapBuffers(m_hDC)) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextWGL::setSwapInterval(int interval)
{
  if (WGLEW_EXT_swap_control)
    return WIN32_CHK(::wglSwapIntervalEXT(interval)) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
  else
    return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextWGL::getSwapInterval(int &intervalOut)
{
  if (WGLEW_EXT_swap_control) {
    intervalOut = ::wglGetSwapIntervalEXT();
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextWGL::activateDrawingContext()
{
  if (WIN32_CHK(::wglMakeCurrent(m_hDC, m_hGLRC))) {
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextWGL::releaseDrawingContext()
{
  if (WIN32_CHK(::wglMakeCurrent(NULL, NULL))) {
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

/* Ron Fosner's code for weighting pixel formats and forcing software.
 * See http://www.opengl.org/resources/faq/technical/weight.cpp
 */
static int weight_pixel_format(PIXELFORMATDESCRIPTOR &pfd, PIXELFORMATDESCRIPTOR &preferredPFD)
{
  int weight = 0;

  /* assume desktop color depth is 32 bits per pixel */

  /* cull unusable pixel formats */
  /* if no formats can be found, can we determine why it was rejected? */
  if (!(pfd.dwFlags & PFD_SUPPORT_OPENGL) || !(pfd.dwFlags & PFD_DRAW_TO_WINDOW) ||
      !(pfd.dwFlags & PFD_DOUBLEBUFFER) || /* Blender _needs_ this */
      !(pfd.iPixelType == PFD_TYPE_RGBA) ||
      (pfd.cColorBits > 32) ||            /* 64 bit formats disable aero */
      (pfd.dwFlags & PFD_GENERIC_FORMAT)) /* no software renderers */
  {
    return 0;
  }

  weight = 1; /* it's usable */

  weight += pfd.cColorBits - 8;

  if (preferredPFD.cAlphaBits > 0 && pfd.cAlphaBits > 0)
    weight++;
#ifdef WIN32_COMPOSITING
  if ((preferredPFD.dwFlags & PFD_SUPPORT_COMPOSITION) && (pfd.dwFlags & PFD_SUPPORT_COMPOSITION))
    weight++;
#endif

  return weight;
}

/*
 * A modification of Ron Fosner's replacement for ChoosePixelFormat
 * returns 0 on error, else returns the pixel format number to be used
 */
static int choose_pixel_format_legacy(HDC hDC, PIXELFORMATDESCRIPTOR &preferredPFD)
{
  int iPixelFormat = 0;
  int weight = 0;

  int iStereoPixelFormat = 0;
  int stereoWeight = 0;

  /* choose a pixel format using the useless Windows function in case we come up empty handed */
  int iLastResortPixelFormat = ::ChoosePixelFormat(hDC, &preferredPFD);

  WIN32_CHK(iLastResortPixelFormat != 0);

  int lastPFD = ::DescribePixelFormat(hDC, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);

  WIN32_CHK(lastPFD != 0);

  for (int i = 1; i <= lastPFD; i++) {
    PIXELFORMATDESCRIPTOR pfd;
    int check = ::DescribePixelFormat(hDC, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    WIN32_CHK(check == lastPFD);

    int w = weight_pixel_format(pfd, preferredPFD);

    if (w > weight) {
      weight = w;
      iPixelFormat = i;
    }

    if (w > stereoWeight && (preferredPFD.dwFlags & pfd.dwFlags & PFD_STEREO)) {
      stereoWeight = w;
      iStereoPixelFormat = i;
    }
  }

  /* choose any available stereo format over a non-stereo format */
  if (iStereoPixelFormat != 0)
    iPixelFormat = iStereoPixelFormat;

  if (iPixelFormat == 0) {
    fprintf(stderr, "Warning! Using result of ChoosePixelFormat.\n");
    iPixelFormat = iLastResortPixelFormat;
  }

  return iPixelFormat;
}

/**
 * Clone a window for the purpose of creating a temporary context to initialize WGL extensions.
 * There is no generic way to clone the lpParam parameter,
 * so the caller is responsible for cloning it themselves.
 */
static HWND clone_window(HWND hWnd, LPVOID lpParam)
{
  int count;

  SetLastError(NO_ERROR);

  DWORD dwExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
  WIN32_CHK(GetLastError() == NO_ERROR);

  WCHAR lpClassName[100] = L"";
  count = GetClassNameW(hWnd, lpClassName, sizeof(lpClassName));
  WIN32_CHK(count != 0);

  WCHAR lpWindowName[100] = L"";
  count = GetWindowTextW(hWnd, lpWindowName, sizeof(lpWindowName));
  WIN32_CHK(count != 0);

  DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
  WIN32_CHK(GetLastError() == NO_ERROR);

  RECT rect;
  GetWindowRect(hWnd, &rect);
  WIN32_CHK(GetLastError() == NO_ERROR);

  HWND hWndParent = (HWND)GetWindowLongPtr(hWnd, GWLP_HWNDPARENT);
  WIN32_CHK(GetLastError() == NO_ERROR);

  HMENU hMenu = GetMenu(hWnd);
  WIN32_CHK(GetLastError() == NO_ERROR);

  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
  WIN32_CHK(GetLastError() == NO_ERROR);

  HWND hwndCloned = CreateWindowExW(dwExStyle,
                                    lpClassName,
                                    lpWindowName,
                                    dwStyle,
                                    rect.left,
                                    rect.top,
                                    rect.right - rect.left,
                                    rect.bottom - rect.top,
                                    hWndParent,
                                    hMenu,
                                    hInstance,
                                    lpParam);

  WIN32_CHK(hwndCloned != NULL);

  return hwndCloned;
}

void GHOST_ContextWGL::initContextWGLEW(PIXELFORMATDESCRIPTOR &preferredPFD)
{
  HWND dummyHWND = NULL;

  HDC dummyHDC = NULL;
  HGLRC dummyHGLRC = NULL;

  HDC prevHDC;
  HGLRC prevHGLRC;

  int iPixelFormat;

  SetLastError(NO_ERROR);

  prevHDC = ::wglGetCurrentDC();
  WIN32_CHK(GetLastError() == NO_ERROR);

  prevHGLRC = ::wglGetCurrentContext();
  WIN32_CHK(GetLastError() == NO_ERROR);

  iPixelFormat = choose_pixel_format_legacy(m_hDC, preferredPFD);

  if (iPixelFormat == 0)
    goto finalize;

  PIXELFORMATDESCRIPTOR chosenPFD;
  if (!WIN32_CHK(
          ::DescribePixelFormat(m_hDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &chosenPFD)))
    goto finalize;

  if (m_hWnd) {
    dummyHWND = clone_window(m_hWnd, NULL);

    if (dummyHWND == NULL)
      goto finalize;

    dummyHDC = GetDC(dummyHWND);
  }

  if (!WIN32_CHK(dummyHDC != NULL))
    goto finalize;

  if (!WIN32_CHK(::SetPixelFormat(dummyHDC, iPixelFormat, &chosenPFD)))
    goto finalize;

  dummyHGLRC = ::wglCreateContext(dummyHDC);

  if (!WIN32_CHK(dummyHGLRC != NULL))
    goto finalize;

  if (!WIN32_CHK(::wglMakeCurrent(dummyHDC, dummyHGLRC)))
    goto finalize;

  if (GLEW_CHK(glewInit()) != GLEW_OK)
    fprintf(stderr, "Warning! Dummy GLEW/WGLEW failed to initialize properly.\n");

    // the following are not technially WGLEW, but they also require a context to work

#ifndef NDEBUG
  free((void *)m_dummyRenderer);
  free((void *)m_dummyVendor);
  free((void *)m_dummyVersion);

  m_dummyRenderer = _strdup(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
  m_dummyVendor = _strdup(reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
  m_dummyVersion = _strdup(reinterpret_cast<const char *>(glGetString(GL_VERSION)));
#endif

finalize:
  WIN32_CHK(::wglMakeCurrent(prevHDC, prevHGLRC));

  if (dummyHGLRC != NULL)
    WIN32_CHK(::wglDeleteContext(dummyHGLRC));

  if (dummyHWND != NULL) {
    if (dummyHDC != NULL)
      WIN32_CHK(::ReleaseDC(dummyHWND, dummyHDC));

    WIN32_CHK(::DestroyWindow(dummyHWND));
  }
}

static void makeAttribList(std::vector<int> &out, bool stereoVisual, bool needAlpha)
{
  out.clear();
  out.reserve(30);

  out.push_back(WGL_SUPPORT_OPENGL_ARB);
  out.push_back(GL_TRUE);

  out.push_back(WGL_DRAW_TO_WINDOW_ARB);
  out.push_back(GL_TRUE);

  out.push_back(WGL_DOUBLE_BUFFER_ARB);
  out.push_back(GL_TRUE);

  out.push_back(WGL_ACCELERATION_ARB);
  out.push_back(WGL_FULL_ACCELERATION_ARB);

  if (stereoVisual) {
    out.push_back(WGL_STEREO_ARB);
    out.push_back(GL_TRUE);
  }

  out.push_back(WGL_PIXEL_TYPE_ARB);
  out.push_back(WGL_TYPE_RGBA_ARB);

  out.push_back(WGL_COLOR_BITS_ARB);
  out.push_back(24);

  if (needAlpha) {
    out.push_back(WGL_ALPHA_BITS_ARB);
    out.push_back(8);
  }

  out.push_back(0);
}

int GHOST_ContextWGL::_choose_pixel_format_arb_1(bool stereoVisual, bool needAlpha)
{
  std::vector<int> iAttributes;

#define _MAX_PIXEL_FORMATS 32

  int iPixelFormat = 0;
  int iPixelFormats[_MAX_PIXEL_FORMATS];

  makeAttribList(iAttributes, stereoVisual, needAlpha);

  UINT nNumFormats;
  WIN32_CHK(wglChoosePixelFormatARB(
      m_hDC, &(iAttributes[0]), NULL, _MAX_PIXEL_FORMATS, iPixelFormats, &nNumFormats));

  if (nNumFormats > 0) {
    iPixelFormat = iPixelFormats[0];

#ifdef WIN32_COMPOSITING
    if (needAlpha) {
      // scan through all pixel format to make sure one supports compositing
      PIXELFORMATDESCRIPTOR pfd;
      int i;

      for (i = 0; i < nNumFormats; i++) {
        if (DescribePixelFormat(m_hDC, iPixelFormats[i], sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
          if (pfd.dwFlags & PFD_SUPPORT_COMPOSITION) {
            iPixelFormat = iPixelFormats[i];
            break;
          }
        }
      }
      if (i == nNumFormats) {
        fprintf(stderr, "Warning! Unable to find a pixel format with compositing capability.\n");
      }
    }
#endif
  }

  // check pixel format
  if (iPixelFormat != 0) {
    if (needAlpha) {
      int alphaBits, iQuery = WGL_ALPHA_BITS_ARB;
      wglGetPixelFormatAttribivARB(m_hDC, iPixelFormat, 0, 1, &iQuery, &alphaBits);
      if (alphaBits == 0) {
        fprintf(stderr, "Warning! Unable to find a frame buffer with alpha channel.\n");
      }
    }
  }
  return iPixelFormat;
}

int GHOST_ContextWGL::choose_pixel_format_arb(bool stereoVisual, bool needAlpha)
{
  int iPixelFormat;

  iPixelFormat = _choose_pixel_format_arb_1(stereoVisual, needAlpha);

  if (iPixelFormat == 0 && stereoVisual) {
    fprintf(stderr, "Warning! Unable to find a stereo pixel format.\n");

    iPixelFormat = _choose_pixel_format_arb_1(false, needAlpha);

    m_stereoVisual = false;  // set context property to actual value
  }

  return iPixelFormat;
}

int GHOST_ContextWGL::choose_pixel_format(bool stereoVisual, bool needAlpha)
{
  PIXELFORMATDESCRIPTOR preferredPFD = {
      sizeof(PIXELFORMATDESCRIPTOR), /* size */
      1,                             /* version */
      (DWORD)(
          PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW |
          PFD_DOUBLEBUFFER |                /* support double-buffering */
          (stereoVisual ? PFD_STEREO : 0) | /* support stereo */
          (
#ifdef WIN32_COMPOSITING
              needAlpha ?
                  PFD_SUPPORT_COMPOSITION : /* support composition for transparent background */
#endif
                  0)),
      PFD_TYPE_RGBA,               /* color type */
      (BYTE)(needAlpha ? 32 : 24), /* preferred color depth */
      0,
      0,
      0,
      0,
      0,
      0,                         /* color bits (ignored) */
      (BYTE)(needAlpha ? 8 : 0), /* alpha buffer */
      0,                         /* alpha shift (ignored) */
      0,                         /* no accumulation buffer */
      0,
      0,
      0,
      0,              /* accum bits (ignored) */
      0,              /* depth buffer */
      0,              /* stencil buffer */
      0,              /* no auxiliary buffers */
      PFD_MAIN_PLANE, /* main layer */
      0,              /* reserved */
      0,
      0,
      0 /* layer, visible, and damage masks (ignored) */
  };

  initContextWGLEW(preferredPFD);

  int iPixelFormat = 0;

  if (WGLEW_ARB_pixel_format)
    iPixelFormat = choose_pixel_format_arb(stereoVisual, needAlpha);

  if (iPixelFormat == 0)
    iPixelFormat = choose_pixel_format_legacy(m_hDC, preferredPFD);

  return iPixelFormat;
}

#ifndef NDEBUG
static void reportContextString(const char *name, const char *dummy, const char *context)
{
  fprintf(stderr, "%s: %s\n", name, context);

  if (dummy && strcmp(dummy, context) != 0)
    fprintf(stderr, "Warning! Dummy %s: %s\n", name, dummy);
}
#endif

GHOST_TSuccess GHOST_ContextWGL::initializeDrawingContext()
{
  SetLastError(NO_ERROR);

  HGLRC prevHGLRC = ::wglGetCurrentContext();
  WIN32_CHK(GetLastError() == NO_ERROR);

  HDC prevHDC = ::wglGetCurrentDC();
  WIN32_CHK(GetLastError() == NO_ERROR);

  if (!WGLEW_ARB_create_context || ::GetPixelFormat(m_hDC) == 0) {
    const bool needAlpha = m_alphaBackground;
    int iPixelFormat;
    int lastPFD;

    PIXELFORMATDESCRIPTOR chosenPFD;

    iPixelFormat = choose_pixel_format(m_stereoVisual, needAlpha);

    if (iPixelFormat == 0) {
      goto error;
    }

    lastPFD = ::DescribePixelFormat(
        m_hDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &chosenPFD);

    if (!WIN32_CHK(lastPFD != 0)) {
      goto error;
    }

    if (needAlpha && chosenPFD.cAlphaBits == 0)
      fprintf(stderr, "Warning! Unable to find a pixel format with an alpha channel.\n");

    if (!WIN32_CHK(::SetPixelFormat(m_hDC, iPixelFormat, &chosenPFD))) {
      goto error;
    }
  }

  if (WGLEW_ARB_create_context) {
    int profileBitCore = m_contextProfileMask & WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
    int profileBitCompat = m_contextProfileMask & WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

#ifdef WITH_GLEW_ES
    int profileBitES = m_contextProfileMask & WGL_CONTEXT_ES_PROFILE_BIT_EXT;
#endif

    if (!WGLEW_ARB_create_context_profile && profileBitCore)
      fprintf(stderr, "Warning! OpenGL core profile not available.\n");

    if (!WGLEW_ARB_create_context_profile && profileBitCompat)
      fprintf(stderr, "Warning! OpenGL compatibility profile not available.\n");

#ifdef WITH_GLEW_ES
    if (!WGLEW_EXT_create_context_es_profile && profileBitES && m_contextMajorVersion == 1)
      fprintf(stderr, "Warning! OpenGL ES profile not available.\n");

    if (!WGLEW_EXT_create_context_es2_profile && profileBitES && m_contextMajorVersion == 2)
      fprintf(stderr, "Warning! OpenGL ES2 profile not available.\n");
#endif

    int profileMask = 0;

    if (WGLEW_ARB_create_context_profile && profileBitCore)
      profileMask |= profileBitCore;

    if (WGLEW_ARB_create_context_profile && profileBitCompat)
      profileMask |= profileBitCompat;

#ifdef WITH_GLEW_ES
    if (WGLEW_EXT_create_context_es_profile && profileBitES)
      profileMask |= profileBitES;
#endif

    if (profileMask != m_contextProfileMask)
      fprintf(stderr, "Warning! Ignoring untested OpenGL context profile mask bits.");

    std::vector<int> iAttributes;

    if (profileMask) {
      iAttributes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
      iAttributes.push_back(profileMask);
    }

    if (m_contextMajorVersion != 0) {
      iAttributes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
      iAttributes.push_back(m_contextMajorVersion);
    }

    if (m_contextMinorVersion != 0) {
      iAttributes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
      iAttributes.push_back(m_contextMinorVersion);
    }

    if (m_contextFlags != 0) {
      iAttributes.push_back(WGL_CONTEXT_FLAGS_ARB);
      iAttributes.push_back(m_contextFlags);
    }

    if (m_contextResetNotificationStrategy != 0) {
      if (WGLEW_ARB_create_context_robustness) {
        iAttributes.push_back(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
        iAttributes.push_back(m_contextResetNotificationStrategy);
      }
      else {
        fprintf(stderr, "Warning! Cannot set the reset notification strategy.");
      }
    }

    iAttributes.push_back(0);

    m_hGLRC = ::wglCreateContextAttribsARB(m_hDC, NULL, &(iAttributes[0]));
  }

  /* Silence warnings interpreted as errors by users when trying to get
   * a context with version higher than 3.3 Core. */
  const bool silent = m_contextMajorVersion > 3;
  if (!WIN32_CHK_SILENT(m_hGLRC != NULL, silent)) {
    goto error;
  }

  s_sharedCount++;

  if (s_sharedHGLRC == NULL) {
    s_sharedHGLRC = m_hGLRC;
  }
  else if (!WIN32_CHK(::wglShareLists(s_sharedHGLRC, m_hGLRC))) {
    goto error;
  }

  if (!WIN32_CHK(::wglMakeCurrent(m_hDC, m_hGLRC))) {
    goto error;
  }

  initContextGLEW();

  if (is_crappy_intel_card()) {
    /* Some Intel cards with context 4.1 or 4.2
     * don't have the point sprite enabled by default.
     *
     * However GL_POINT_SPRITE was removed in 3.2 and is now permanently ON.
     * Then use brute force. */
    glEnable(GL_POINT_SPRITE);
  }

  initClearGL();
  ::SwapBuffers(m_hDC);

#ifndef NDEBUG
  const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
  const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));

  reportContextString("Vendor", m_dummyVendor, vendor);
  reportContextString("Renderer", m_dummyRenderer, renderer);
  reportContextString("Version", m_dummyVersion, version);

  fprintf(stderr, "Context Version: %d.%d\n", m_contextMajorVersion, m_contextMinorVersion);
#endif

  return GHOST_kSuccess;
error:
  ::wglMakeCurrent(prevHDC, prevHGLRC);
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextWGL::releaseNativeHandles()
{
  GHOST_TSuccess success = m_hGLRC != s_sharedHGLRC || s_sharedCount == 1 ? GHOST_kSuccess :
                                                                            GHOST_kFailure;

  m_hWnd = NULL;
  m_hDC = NULL;

  return success;
}
