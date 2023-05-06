/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextEGL class.
 */

#include "GHOST_ContextEGL.hh"

#include <set>
#include <sstream>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cstring>

#define CASE_CODE_RETURN_STR(code) \
  case code: \
    return #code;

static const char *get_egl_error_enum_string(EGLint error)
{
  switch (error) {
    CASE_CODE_RETURN_STR(EGL_SUCCESS)
    CASE_CODE_RETURN_STR(EGL_NOT_INITIALIZED)
    CASE_CODE_RETURN_STR(EGL_BAD_ACCESS)
    CASE_CODE_RETURN_STR(EGL_BAD_ALLOC)
    CASE_CODE_RETURN_STR(EGL_BAD_ATTRIBUTE)
    CASE_CODE_RETURN_STR(EGL_BAD_CONTEXT)
    CASE_CODE_RETURN_STR(EGL_BAD_CONFIG)
    CASE_CODE_RETURN_STR(EGL_BAD_CURRENT_SURFACE)
    CASE_CODE_RETURN_STR(EGL_BAD_DISPLAY)
    CASE_CODE_RETURN_STR(EGL_BAD_SURFACE)
    CASE_CODE_RETURN_STR(EGL_BAD_MATCH)
    CASE_CODE_RETURN_STR(EGL_BAD_PARAMETER)
    CASE_CODE_RETURN_STR(EGL_BAD_NATIVE_PIXMAP)
    CASE_CODE_RETURN_STR(EGL_BAD_NATIVE_WINDOW)
    CASE_CODE_RETURN_STR(EGL_CONTEXT_LOST)
    default:
      return nullptr;
  }
}

static const char *get_egl_error_message_string(EGLint error)
{
  switch (error) {
    case EGL_SUCCESS:
      return "The last function succeeded without error.";

    case EGL_NOT_INITIALIZED:
      return (
          "EGL is not initialized, or could not be initialized, "
          "for the specified EGL display connection.");

    case EGL_BAD_ACCESS:
      return (
          "EGL cannot access a requested resource "
          "(for example a context is bound in another thread).");

    case EGL_BAD_ALLOC:
      return "EGL failed to allocate resources for the requested operation.";

    case EGL_BAD_ATTRIBUTE:
      return "An unrecognized attribute or attribute value was passed in the attribute list.";

    case EGL_BAD_CONTEXT:
      return "An EGLContext argument does not name a valid EGL rendering context.";

    case EGL_BAD_CONFIG:
      return "An EGLConfig argument does not name a valid EGL frame buffer configuration.";

    case EGL_BAD_CURRENT_SURFACE:
      return (
          "The current surface of the calling thread is a window, "
          "pixel buffer or pixmap that is no longer valid.");

    case EGL_BAD_DISPLAY:
      return "An EGLDisplay argument does not name a valid EGL display connection.";

    case EGL_BAD_SURFACE:
      return (
          "An EGLSurface argument does not name a valid surface "
          "(window, pixel buffer or pixmap) configured for GL rendering.");

    case EGL_BAD_MATCH:
      return (
          "Arguments are inconsistent "
          "(for example, a valid context requires buffers not supplied by a valid surface).");

    case EGL_BAD_PARAMETER:
      return "One or more argument values are invalid.";

    case EGL_BAD_NATIVE_PIXMAP:
      return "A NativePixmapType argument does not refer to a valid native pixmap.";

    case EGL_BAD_NATIVE_WINDOW:
      return "A NativeWindowType argument does not refer to a valid native window.";

    case EGL_CONTEXT_LOST:
      return (
          "A power management event has occurred. "
          "The application must destroy all contexts and reinitialize OpenGL ES state "
          "and objects to continue rendering.");

    default:
      return nullptr;
  }
}

static bool egl_chk(bool result,
                    const char *file = nullptr,
                    int line = 0,
                    const char *text = nullptr)
{
  if (!result) {
    const EGLint error = eglGetError();

    const char *code = get_egl_error_enum_string(error);
    const char *msg = get_egl_error_message_string(error);

#ifndef NDEBUG
    fprintf(stderr,
            "%s:%d: [%s] -> EGL Error (0x%04X): %s: %s\n",
            file,
            line,
            text,
            uint(error),
            code ? code : "<Unknown>",
            msg ? msg : "<Unknown>");
#else
    fprintf(stderr,
            "EGL Error (0x%04X): %s: %s\n",
            uint(error),
            code ? code : "<Unknown>",
            msg ? msg : "<Unknown>");
    (void)(file);
    (void)(line);
    (void)(text);
#endif
  }

  return result;
}

#ifndef NDEBUG
#  define EGL_CHK(x) egl_chk((x), __FILE__, __LINE__, #  x)
#else
#  define EGL_CHK(x) egl_chk(x)
#endif

EGLContext GHOST_ContextEGL::s_gl_sharedContext = EGL_NO_CONTEXT;
EGLint GHOST_ContextEGL::s_gl_sharedCount = 0;

EGLContext GHOST_ContextEGL::s_gles_sharedContext = EGL_NO_CONTEXT;
EGLint GHOST_ContextEGL::s_gles_sharedCount = 0;

EGLContext GHOST_ContextEGL::s_vg_sharedContext = EGL_NO_CONTEXT;
EGLint GHOST_ContextEGL::s_vg_sharedCount = 0;

#ifdef _MSC_VER
#  pragma warning(disable : 4715)
#endif

template<typename T> T &choose_api(EGLenum api, T &a, T &b, T &c)
{
  switch (api) {
    case EGL_OPENGL_API:
      return a;
    case EGL_OPENGL_ES_API:
      return b;
    case EGL_OPENVG_API:
      return c;
    default:
      abort();
  }
}

GHOST_ContextEGL::GHOST_ContextEGL(const GHOST_System *const system,
                                   bool stereoVisual,
                                   EGLNativeWindowType nativeWindow,
                                   EGLNativeDisplayType nativeDisplay,
                                   EGLint contextProfileMask,
                                   EGLint contextMajorVersion,
                                   EGLint contextMinorVersion,
                                   EGLint contextFlags,
                                   EGLint contextResetNotificationStrategy,
                                   EGLenum api)
    : GHOST_Context(stereoVisual),
      m_system(system),
      m_nativeDisplay(nativeDisplay),
      m_nativeWindow(nativeWindow),
      m_contextProfileMask(contextProfileMask),
      m_contextMajorVersion(contextMajorVersion),
      m_contextMinorVersion(contextMinorVersion),
      m_contextFlags(contextFlags),
      m_contextResetNotificationStrategy(contextResetNotificationStrategy),
      m_api(api),
      m_context(EGL_NO_CONTEXT),
      m_surface(EGL_NO_SURFACE),
      m_display(EGL_NO_DISPLAY),
      m_swap_interval(1),
      m_sharedContext(
          choose_api(api, s_gl_sharedContext, s_gles_sharedContext, s_vg_sharedContext)),
      m_sharedCount(choose_api(api, s_gl_sharedCount, s_gles_sharedCount, s_vg_sharedCount)),
      m_surface_from_native_window(false)
{
}

GHOST_ContextEGL::~GHOST_ContextEGL()
{
  if (m_display != EGL_NO_DISPLAY) {

    bindAPI(m_api);

    if (m_context != EGL_NO_CONTEXT) {
      if (m_context == ::eglGetCurrentContext()) {
        EGL_CHK(::eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
      }
      if (m_context != m_sharedContext || m_sharedCount == 1) {
        assert(m_sharedCount > 0);

        m_sharedCount--;

        if (m_sharedCount == 0) {
          m_sharedContext = EGL_NO_CONTEXT;
        }
        EGL_CHK(::eglDestroyContext(m_display, m_context));
      }
    }

    if (m_surface != EGL_NO_SURFACE) {
      EGL_CHK(::eglDestroySurface(m_display, m_surface));
    }
  }
}

GHOST_TSuccess GHOST_ContextEGL::swapBuffers()
{
  return EGL_CHK(::eglSwapBuffers(m_display, m_surface)) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::setSwapInterval(int interval)
{
  if (epoxy_egl_version(m_display) >= 11) {
    if (EGL_CHK(::eglSwapInterval(m_display, interval))) {
      m_swap_interval = interval;

      return GHOST_kSuccess;
    }
    return GHOST_kFailure;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::getSwapInterval(int &intervalOut)
{
  /* This is a bit of a kludge because there does not seem to
   * be a way to query the swap interval with EGL. */
  intervalOut = m_swap_interval;

  return GHOST_kSuccess;
}

EGLDisplay GHOST_ContextEGL::getDisplay() const
{
  return m_display;
}

EGLConfig GHOST_ContextEGL::getConfig() const
{
  return m_config;
}

EGLContext GHOST_ContextEGL::getContext() const
{
  return m_context;
}

GHOST_TSuccess GHOST_ContextEGL::activateDrawingContext()
{
  if (m_display) {
    bindAPI(m_api);
    return EGL_CHK(::eglMakeCurrent(m_display, m_surface, m_surface, m_context)) ? GHOST_kSuccess :
                                                                                   GHOST_kFailure;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::releaseDrawingContext()
{
  if (m_display) {
    bindAPI(m_api);

    return EGL_CHK(::eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) ?
               GHOST_kSuccess :
               GHOST_kFailure;
  }
  return GHOST_kFailure;
}

inline bool GHOST_ContextEGL::bindAPI(EGLenum api)
{
  if (epoxy_egl_version(m_display) >= 12) {
    return (EGL_CHK(eglBindAPI(api)) == EGL_TRUE);
  }

  return false;
}

static const std::string &api_string(EGLenum api)
{
  static const std::string a("OpenGL");
  static const std::string b("OpenGL ES");
  static const std::string c("OpenVG");

  return choose_api(api, a, b, c);
}

GHOST_TSuccess GHOST_ContextEGL::initializeDrawingContext()
{
  /* Objects have to be declared here due to the use of `goto`. */
  std::vector<EGLint> attrib_list;
  EGLint num_config = 0;

  if (m_stereoVisual) {
    fprintf(stderr, "Warning! Stereo OpenGL ES contexts are not supported.\n");
  }
  m_stereoVisual = false; /* It doesn't matter what the Window wants. */

  EGLDisplay prev_display = eglGetCurrentDisplay();
  EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW);
  EGLSurface prev_read = eglGetCurrentSurface(EGL_READ);
  EGLContext prev_context = eglGetCurrentContext();

  EGLint egl_major = 0, egl_minor = 0;

  if (!EGL_CHK((m_display = ::eglGetDisplay(m_nativeDisplay)) != EGL_NO_DISPLAY)) {
    goto error;
  }

  if (!EGL_CHK(::eglInitialize(m_display, &egl_major, &egl_minor)) ||
      (egl_major == 0 && egl_minor == 0))
  {
    /* We failed to create a regular render window, retry and see if we can create a headless
     * render context. */
    ::eglTerminate(m_display);

    const char *egl_extension_st = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    assert(egl_extension_st != nullptr);
    assert(strstr(egl_extension_st, "EGL_MESA_platform_surfaceless") != nullptr);
    if (egl_extension_st == nullptr ||
        strstr(egl_extension_st, "EGL_MESA_platform_surfaceless") == nullptr)
    {
      goto error;
    }

    m_display = eglGetPlatformDisplayEXT(
        EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);

    if (!EGL_CHK(::eglInitialize(m_display, &egl_major, &egl_minor))) {
      goto error;
    }
    /* Because the first eglInitialize will print an error to the terminal, print a "success"
     * message here to let the user know that we successfully recovered from the error. */
    fprintf(stderr, "\nManaged to successfully fallback to surfaceless EGL rendering!\n\n");
  }
#ifdef WITH_GHOST_DEBUG
  fprintf(stderr, "EGL Version %d.%d\n", egl_major, egl_minor);
#endif

  if (!EGL_CHK(::eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))) {
    goto error;
  }
  if (!bindAPI(m_api)) {
    goto error;
  }

  /* Build attribute list. */

  attrib_list.reserve(20);

  if (m_api == EGL_OPENGL_ES_API && epoxy_egl_version(m_display) >= 12) {
    /* According to the spec it seems that you are required to set EGL_RENDERABLE_TYPE,
     * but some implementations (ANGLE) do not seem to care. */

    if (m_contextMajorVersion == 1) {
      attrib_list.push_back(EGL_RENDERABLE_TYPE);
      attrib_list.push_back(EGL_OPENGL_ES_BIT);
    }
    else if (m_contextMajorVersion == 2) {
      attrib_list.push_back(EGL_RENDERABLE_TYPE);
      attrib_list.push_back(EGL_OPENGL_ES2_BIT);
    }
    else if (m_contextMajorVersion == 3) {
      attrib_list.push_back(EGL_RENDERABLE_TYPE);
      attrib_list.push_back(EGL_OPENGL_ES3_BIT_KHR);
    }
    else {
      fprintf(stderr,
              "Warning! Unable to request an ES context of version %d.%d\n",
              m_contextMajorVersion,
              m_contextMinorVersion);
    }

    if (!((m_contextMajorVersion == 1) ||
          (m_contextMajorVersion == 2 && epoxy_egl_version(m_display) >= 13) ||
          (m_contextMajorVersion == 3 &&
           epoxy_has_egl_extension(m_display, "KHR_create_context")) ||
          (m_contextMajorVersion == 3 && epoxy_egl_version(m_display) >= 15)))
    {
      fprintf(stderr,
              "Warning! May not be able to create a version %d.%d ES context with version %d.%d "
              "of EGL\n",
              m_contextMajorVersion,
              m_contextMinorVersion,
              egl_major,
              egl_minor);
    }
  }
  else {
    attrib_list.push_back(EGL_RENDERABLE_TYPE);
    attrib_list.push_back(EGL_OPENGL_BIT);
  }

  attrib_list.push_back(EGL_RED_SIZE);
  attrib_list.push_back(8);

  attrib_list.push_back(EGL_GREEN_SIZE);
  attrib_list.push_back(8);

  attrib_list.push_back(EGL_BLUE_SIZE);
  attrib_list.push_back(8);

#ifdef GHOST_OPENGL_ALPHA
  attrib_list.push_back(EGL_ALPHA_SIZE);
  attrib_list.push_back(8);
#endif

  if (m_nativeWindow == 0) {
    /* Off-screen surface. */
    attrib_list.push_back(EGL_SURFACE_TYPE);
    attrib_list.push_back(EGL_PBUFFER_BIT);
  }

  attrib_list.push_back(EGL_NONE);

  if (!EGL_CHK(::eglChooseConfig(m_display, &(attrib_list[0]), &m_config, 1, &num_config))) {
    goto error;
  }

  /* A common error is to assume that ChooseConfig worked because it returned EGL_TRUE. */
  if (num_config != 1) { /* `num_config` should be exactly 1. */
    goto error;
  }

  if (m_nativeWindow != 0) {
    m_surface = ::eglCreateWindowSurface(m_display, m_config, m_nativeWindow, nullptr);
    m_surface_from_native_window = true;
  }
  else {
    static const EGLint pb_attrib_list[] = {
        EGL_WIDTH,
        1,
        EGL_HEIGHT,
        1,
        EGL_NONE,
    };
    m_surface = ::eglCreatePbufferSurface(m_display, m_config, pb_attrib_list);
  }

  if (!EGL_CHK(m_surface != EGL_NO_SURFACE)) {
    goto error;
  }
  attrib_list.clear();

  if (epoxy_egl_version(m_display) >= 15 ||
      epoxy_has_egl_extension(m_display, "KHR_create_context")) {
    if (m_api == EGL_OPENGL_API || m_api == EGL_OPENGL_ES_API) {
      if (m_contextMajorVersion != 0) {
        attrib_list.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
        attrib_list.push_back(m_contextMajorVersion);
      }

      if (m_contextMinorVersion != 0) {
        attrib_list.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
        attrib_list.push_back(m_contextMinorVersion);
      }

      if (m_contextFlags != 0) {
        attrib_list.push_back(EGL_CONTEXT_FLAGS_KHR);
        attrib_list.push_back(m_contextFlags);
      }
    }
    else {
      if (m_contextMajorVersion != 0 || m_contextMinorVersion != 0) {
        fprintf(stderr,
                "Warning! Cannot request specific versions of %s contexts.",
                api_string(m_api).c_str());
      }

      if (m_contextFlags != 0) {
        fprintf(stderr, "Warning! Flags cannot be set on %s contexts.", api_string(m_api).c_str());
      }
    }

    if (m_api == EGL_OPENGL_API) {
      if (m_contextProfileMask != 0) {
        attrib_list.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
        attrib_list.push_back(m_contextProfileMask);
      }
    }
    else {
      if (m_contextProfileMask != 0) {
        fprintf(
            stderr, "Warning! Cannot select profile for %s contexts.", api_string(m_api).c_str());
      }
    }

    if (m_api == EGL_OPENGL_API || epoxy_egl_version(m_display) >= 15) {
      if (m_contextResetNotificationStrategy != 0) {
        attrib_list.push_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR);
        attrib_list.push_back(m_contextResetNotificationStrategy);
      }
    }
    else {
      if (m_contextResetNotificationStrategy != 0) {
        fprintf(stderr,
                "Warning! EGL %d.%d cannot set the reset notification strategy on %s contexts.",
                egl_major,
                egl_minor,
                api_string(m_api).c_str());
      }
    }
  }
  else {
    if (m_api == EGL_OPENGL_ES_API) {
      if (m_contextMajorVersion != 0) {
        attrib_list.push_back(EGL_CONTEXT_CLIENT_VERSION);
        attrib_list.push_back(m_contextMajorVersion);
      }
    }
    else {
      if (m_contextMajorVersion != 0 || m_contextMinorVersion != 0) {
        fprintf(stderr,
                "Warning! EGL %d.%d is unable to select between versions of %s.",
                egl_major,
                egl_minor,
                api_string(m_api).c_str());
      }
    }

    if (m_contextFlags != 0) {
      fprintf(stderr, "Warning! EGL %d.%d is unable to set context flags.", egl_major, egl_minor);
    }
    if (m_contextProfileMask != 0) {
      fprintf(stderr,
              "Warning! EGL %d.%d is unable to select between profiles.",
              egl_major,
              egl_minor);
    }
    if (m_contextResetNotificationStrategy != 0) {
      fprintf(stderr,
              "Warning! EGL %d.%d is unable to set the reset notification strategies.",
              egl_major,
              egl_minor);
    }
  }

  attrib_list.push_back(EGL_NONE);

  m_context = ::eglCreateContext(m_display, m_config, m_sharedContext, &(attrib_list[0]));

  if (!EGL_CHK(m_context != EGL_NO_CONTEXT)) {
    goto error;
  }

  if (m_sharedContext == EGL_NO_CONTEXT) {
    m_sharedContext = m_context;
  }

  m_sharedCount++;

  if (!EGL_CHK(::eglMakeCurrent(m_display, m_surface, m_surface, m_context))) {
    goto error;
  }

  if (m_nativeWindow != 0) {
    initClearGL();
    ::eglSwapBuffers(m_display, m_surface);
  }

  return GHOST_kSuccess;

error:
  if (prev_display != EGL_NO_DISPLAY) {
    EGL_CHK(eglMakeCurrent(prev_display, prev_draw, prev_read, prev_context));
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::releaseNativeHandles()
{
  m_nativeDisplay = nullptr;

  m_nativeWindow = 0;
  if (m_surface_from_native_window) {
    m_surface = EGL_NO_SURFACE;
  }

  return GHOST_kSuccess;
}
