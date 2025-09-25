/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextWGL class.
 */

#include "GHOST_ContextWGL.hh"

#include <tchar.h>

#include <cassert>
#include <cstdio>
#include <vector>

HGLRC GHOST_ContextWGL::s_sharedHGLRC = nullptr;
int GHOST_ContextWGL::s_sharedCount = 0;

/* Some third-generation Intel video-cards are constantly bring problems */
static bool is_crappy_intel_card()
{
  return strstr((const char *)glGetString(GL_VENDOR), "Intel") != nullptr;
}

GHOST_ContextWGL::GHOST_ContextWGL(const GHOST_ContextParams &context_params,
                                   bool alphaBackground,
                                   HWND hWnd,
                                   HDC hDC,
                                   int contextProfileMask,
                                   int contextMajorVersion,
                                   int contextMinorVersion,
                                   int contextFlags,
                                   int contextResetNotificationStrategy)
    : GHOST_Context(context_params),
      h_wnd_(hWnd),
      h_DC_(hDC),
      context_profile_mask_(contextProfileMask),
      context_major_version_(contextMajorVersion),
      context_minor_version_(contextMinorVersion),
      context_flags_(contextFlags),
      alpha_background_(alphaBackground),
      context_reset_notification_strategy_(contextResetNotificationStrategy),
      h_GLRC_(nullptr)
#ifndef NDEBUG
      ,
      dummy_vendor_(nullptr),
      dummy_renderer_(nullptr),
      dummy_version_(nullptr)
#endif
{
  assert(h_DC_ != nullptr);
}

GHOST_ContextWGL::~GHOST_ContextWGL()
{
  if (h_GLRC_ != nullptr) {
    if (h_GLRC_ == ::wglGetCurrentContext()) {
      WIN32_CHK(::wglMakeCurrent(nullptr, nullptr));
    }

    if (h_GLRC_ != s_sharedHGLRC || s_sharedCount == 1) {
      assert(s_sharedCount > 0);

      s_sharedCount--;

      if (s_sharedCount == 0) {
        s_sharedHGLRC = nullptr;
      }

      WIN32_CHK(::wglDeleteContext(h_GLRC_));
    }
  }

#ifndef NDEBUG
  if (dummy_renderer_) {
    free((void *)dummy_renderer_);
    free((void *)dummy_vendor_);
    free((void *)dummy_version_);
  }
#endif
}

GHOST_TSuccess GHOST_ContextWGL::swapBufferRelease()
{
  return WIN32_CHK(::SwapBuffers(h_DC_)) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextWGL::setSwapInterval(int interval)
{
  if (epoxy_has_wgl_extension(h_DC_, "WGL_EXT_swap_control")) {
    return WIN32_CHK(::wglSwapIntervalEXT(interval)) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextWGL::getSwapInterval(int &interval_out)
{
  if (epoxy_has_wgl_extension(h_DC_, "WGL_EXT_swap_control")) {
    interval_out = ::wglGetSwapIntervalEXT();
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextWGL::activateDrawingContext()
{
  if (WIN32_CHK(::wglMakeCurrent(h_DC_, h_GLRC_))) {
    active_context_ = this;
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ContextWGL::releaseDrawingContext()
{
  /* Calling wglMakeCurrent(nullptr, nullptr) without an active context returns an error,
   * so we always pass the device context handle. */
  if (WIN32_CHK(::wglMakeCurrent(h_DC_, nullptr))) {
    active_context_ = nullptr;
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
      !(pfd.dwFlags & PFD_DOUBLEBUFFER) || /* Blender _needs_ this. */
      !(pfd.iPixelType == PFD_TYPE_RGBA) ||
      (pfd.cColorBits > 32) ||            /* 64 bit formats disable AERO. */
      (pfd.dwFlags & PFD_GENERIC_FORMAT)) /* No software renderers. */
  {
    return 0;
  }

  weight = 1; /* it's usable */

  weight += pfd.cColorBits - 8;

  if (preferredPFD.cAlphaBits > 0 && pfd.cAlphaBits > 0) {
    weight++;
  }

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

  int lastPFD = ::DescribePixelFormat(hDC, 1, sizeof(PIXELFORMATDESCRIPTOR), nullptr);

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
  if (iStereoPixelFormat != 0) {
    iPixelFormat = iStereoPixelFormat;
  }

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

  WIN32_CHK(hwndCloned != nullptr);

  return hwndCloned;
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

/* Temporary context used to create the actual context. We need ARB pixel format
 * and context extensions, which are only available within a context. */
struct DummyContextWGL {
  HWND dummyHWND = nullptr;

  HDC dummyHDC = nullptr;
  HGLRC dummyHGLRC = nullptr;

  HDC prevHDC = nullptr;
  HGLRC prevHGLRC = nullptr;

  int dummyPixelFormat = 0;

  PIXELFORMATDESCRIPTOR preferredPFD;

  bool has_WGL_ARB_pixel_format = false;
  bool has_WGL_ARB_create_context = false;
  bool has_WGL_ARB_create_context_profile = false;
  bool has_WGL_ARB_create_context_robustness = false;

  DummyContextWGL(HDC hDC, HWND hWnd, bool stereoVisual, bool needAlpha)
  {
    preferredPFD = {
        sizeof(PIXELFORMATDESCRIPTOR), /* size */
        1,                             /* version */
        (DWORD)(PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW |
                PFD_DOUBLEBUFFER |                /* support double-buffering */
                (stereoVisual ? PFD_STEREO : 0)), /* support stereo */
        PFD_TYPE_RGBA,                            /* color type */
        (BYTE)(needAlpha ? 32 : 24),              /* preferred color depth */
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
        0,              /* Accumulation bits (ignored). */
        0,              /* depth buffer */
        0,              /* stencil buffer */
        0,              /* no auxiliary buffers */
        PFD_MAIN_PLANE, /* main layer */
        0,              /* reserved */
        0,
        0,
        0 /* layer, visible, and damage masks (ignored) */
    };

    SetLastError(NO_ERROR);

    prevHDC = ::wglGetCurrentDC();
    WIN32_CHK(GetLastError() == NO_ERROR);

    prevHGLRC = ::wglGetCurrentContext();
    WIN32_CHK(GetLastError() == NO_ERROR);

    dummyPixelFormat = choose_pixel_format_legacy(hDC, preferredPFD);

    if (dummyPixelFormat == 0) {
      return;
    }

    PIXELFORMATDESCRIPTOR chosenPFD;
    if (!WIN32_CHK(::DescribePixelFormat(
            hDC, dummyPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &chosenPFD)))
    {
      return;
    }

    if (hWnd) {
      dummyHWND = clone_window(hWnd, nullptr);

      if (dummyHWND == nullptr) {
        return;
      }

      dummyHDC = GetDC(dummyHWND);
    }

    if (!WIN32_CHK(dummyHDC != nullptr)) {
      return;
    }

    if (!WIN32_CHK(::SetPixelFormat(dummyHDC, dummyPixelFormat, &chosenPFD))) {
      return;
    }

    dummyHGLRC = ::wglCreateContext(dummyHDC);

    if (!WIN32_CHK(dummyHGLRC != nullptr)) {
      return;
    }

    if (!WIN32_CHK(::wglMakeCurrent(dummyHDC, dummyHGLRC))) {
      return;
    }

    has_WGL_ARB_pixel_format = epoxy_has_wgl_extension(hDC, "WGL_ARB_pixel_format");
    has_WGL_ARB_create_context = epoxy_has_wgl_extension(hDC, "WGL_ARB_create_context");
    has_WGL_ARB_create_context_profile = epoxy_has_wgl_extension(hDC,
                                                                 "WGL_ARB_create_context_profile");
    has_WGL_ARB_create_context_robustness = epoxy_has_wgl_extension(
        hDC, "WGL_ARB_create_context_robustness");
  }

  ~DummyContextWGL()
  {
    WIN32_CHK(::wglMakeCurrent(prevHDC, prevHGLRC));

    if (dummyHGLRC != nullptr) {
      WIN32_CHK(::wglDeleteContext(dummyHGLRC));
    }

    if (dummyHWND != nullptr) {
      if (dummyHDC != nullptr) {
        WIN32_CHK(::ReleaseDC(dummyHWND, dummyHDC));
      }

      WIN32_CHK(::DestroyWindow(dummyHWND));
    }
  }
};

int GHOST_ContextWGL::_choose_pixel_format_arb_1(bool stereoVisual, bool needAlpha)
{
  std::vector<int> iAttributes;

#define _MAX_PIXEL_FORMATS 32

  int iPixelFormat = 0;
  int iPixelFormats[_MAX_PIXEL_FORMATS];

  makeAttribList(iAttributes, stereoVisual, needAlpha);

  uint nNumFormats;
  WIN32_CHK(wglChoosePixelFormatARB(
      h_DC_, &(iAttributes[0]), nullptr, _MAX_PIXEL_FORMATS, iPixelFormats, &nNumFormats));

  if (nNumFormats > 0) {
    iPixelFormat = iPixelFormats[0];
  }

  // check pixel format
  if (iPixelFormat != 0) {
    if (needAlpha) {
      int alphaBits, iQuery = WGL_ALPHA_BITS_ARB;
      wglGetPixelFormatAttribivARB(h_DC_, iPixelFormat, 0, 1, &iQuery, &alphaBits);
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

    context_params_.is_stereo_visual = false; /* Set context property to actual value. */
  }

  return iPixelFormat;
}

#ifndef NDEBUG
static void reportContextString(const char *name, const char *dummy, const char *context)
{
  fprintf(stderr, "%s: %s\n", name, context);

  if (dummy && strcmp(dummy, context) != 0) {
    fprintf(stderr, "Warning! Dummy %s: %s\n", name, dummy);
  }
}
#endif

GHOST_TSuccess GHOST_ContextWGL::initializeDrawingContext()
{
  SetLastError(NO_ERROR);

  HGLRC prevHGLRC = ::wglGetCurrentContext();
  WIN32_CHK(GetLastError() == NO_ERROR);

  HDC prevHDC = ::wglGetCurrentDC();
  WIN32_CHK(GetLastError() == NO_ERROR);

  {
    const bool needAlpha = alpha_background_;
    DummyContextWGL dummy(h_DC_, h_wnd_, context_params_.is_stereo_visual, needAlpha);

    if (!dummy.has_WGL_ARB_create_context || ::GetPixelFormat(h_DC_) == 0) {
      int iPixelFormat = 0;

      if (dummy.has_WGL_ARB_pixel_format) {
        iPixelFormat = choose_pixel_format_arb(context_params_.is_stereo_visual, needAlpha);
      }

      if (iPixelFormat == 0) {
        iPixelFormat = choose_pixel_format_legacy(h_DC_, dummy.preferredPFD);
      }

      if (iPixelFormat == 0) {
        goto error;
      }

      PIXELFORMATDESCRIPTOR chosenPFD;
      int lastPFD = ::DescribePixelFormat(
          h_DC_, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &chosenPFD);

      if (!WIN32_CHK(lastPFD != 0)) {
        goto error;
      }

      if (needAlpha && chosenPFD.cAlphaBits == 0) {
        fprintf(stderr, "Warning! Unable to find a pixel format with an alpha channel.\n");
      }

      if (!WIN32_CHK(::SetPixelFormat(h_DC_, iPixelFormat, &chosenPFD))) {
        goto error;
      }
    }

    if (dummy.has_WGL_ARB_create_context) {
      int profileBitCore = context_profile_mask_ & WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
      int profileBitCompat = context_profile_mask_ & WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

      if (!dummy.has_WGL_ARB_create_context_profile && profileBitCore) {
        fprintf(stderr, "Warning! OpenGL core profile not available.\n");
      }

      if (!dummy.has_WGL_ARB_create_context_profile && profileBitCompat) {
        fprintf(stderr, "Warning! OpenGL compatibility profile not available.\n");
      }

      int profileMask = 0;

      if (dummy.has_WGL_ARB_create_context_profile && profileBitCore) {
        profileMask |= profileBitCore;
      }

      if (dummy.has_WGL_ARB_create_context_profile && profileBitCompat) {
        profileMask |= profileBitCompat;
      }

      if (profileMask != context_profile_mask_) {
        fprintf(stderr, "Warning! Ignoring untested OpenGL context profile mask bits.");
      }

      std::vector<int> iAttributes;

      if (profileMask) {
        iAttributes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
        iAttributes.push_back(profileMask);
      }

      if (context_major_version_ != 0) {
        iAttributes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
        iAttributes.push_back(context_major_version_);
      }

      if (context_minor_version_ != 0) {
        iAttributes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
        iAttributes.push_back(context_minor_version_);
      }

      if (context_flags_ != 0) {
        iAttributes.push_back(WGL_CONTEXT_FLAGS_ARB);
        iAttributes.push_back(context_flags_);
      }

      if (context_reset_notification_strategy_ != 0) {
        if (dummy.has_WGL_ARB_create_context_robustness) {
          iAttributes.push_back(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
          iAttributes.push_back(context_reset_notification_strategy_);
        }
        else {
          fprintf(stderr, "Warning! Cannot set the reset notification strategy.");
        }
      }

      iAttributes.push_back(0);

      h_GLRC_ = ::wglCreateContextAttribsARB(h_DC_, s_sharedHGLRC, &(iAttributes[0]));
    }
  }

  /* Silence warnings interpreted as errors by users when trying to get
   * a context with version higher than 3.3 Core. */
  {
    const bool silent = context_major_version_ > 3;
    if (!WIN32_CHK_SILENT(h_GLRC_ != nullptr, silent)) {
      goto error;
    }
  }

  {
    const GHOST_TVSyncModes vsync = getVSync();
    if (vsync != GHOST_kVSyncModeUnset) {
      setSwapInterval(int(vsync));
    }
  }

  s_sharedCount++;

  if (s_sharedHGLRC == nullptr) {
    s_sharedHGLRC = h_GLRC_;
  }

  if (!WIN32_CHK(::wglMakeCurrent(h_DC_, h_GLRC_))) {
    goto error;
  }

  if (is_crappy_intel_card()) {
    /* Some Intel cards with context 4.1 or 4.2
     * don't have the point sprite enabled by default.
     *
     * However GL_POINT_SPRITE was removed in 3.2 and is now permanently ON.
     * Then use brute force. */
    glEnable(GL_POINT_SPRITE);
  }

  initClearGL();
  ::SwapBuffers(h_DC_);

#ifndef NDEBUG
  {
    const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));

    reportContextString("Vendor", dummy_vendor_, vendor);
    reportContextString("Renderer", dummy_renderer_, renderer);
    reportContextString("Version", dummy_version_, version);

    fprintf(stderr, "Context Version: %d.%d\n", context_major_version_, context_minor_version_);
  }
#endif

  active_context_ = this;
  return GHOST_kSuccess;
error:
  ::wglMakeCurrent(prevHDC, prevHGLRC);
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextWGL::releaseNativeHandles()
{
  GHOST_TSuccess success = h_GLRC_ != s_sharedHGLRC || s_sharedCount == 1 ? GHOST_kSuccess :
                                                                            GHOST_kFailure;

  h_wnd_ = nullptr;
  h_DC_ = nullptr;

  return success;
}
