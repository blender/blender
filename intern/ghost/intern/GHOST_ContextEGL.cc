/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

static void egl_print_error(const char *message, const EGLint error)
{
  const char *code = get_egl_error_enum_string(error);
  const char *msg = get_egl_error_message_string(error);

  fprintf(stderr,
          "%sEGL Error (0x%04X): %s: %s\n",
          message,
          uint(error),
          code ? code : "<Unknown>",
          msg ? msg : "<Unknown>");
}

static bool egl_chk(bool result,
                    const char *file = nullptr,
                    int line = 0,
                    const char *text = nullptr)
{
  if (!result) {
    const EGLint error = eglGetError();
#ifndef NDEBUG
    const char *code = get_egl_error_enum_string(error);
    const char *msg = get_egl_error_message_string(error);
    fprintf(stderr,
            "%s:%d: [%s] -> EGL Error (0x%04X): %s: %s\n",
            file,
            line,
            text,
            uint(error),
            code ? code : "<Unknown>",
            msg ? msg : "<Unknown>");
#else
    egl_print_error("", error);
    (void)(file);
    (void)(line);
    (void)(text);
#endif
  }

  return result;
}

#ifndef NDEBUG
#  define EGL_CHK(x) egl_chk((x), __FILE__, __LINE__, #x)
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
                                   const GHOST_ContextParams &context_params,
                                   EGLNativeWindowType nativeWindow,
                                   EGLNativeDisplayType nativeDisplay,
                                   EGLint contextProfileMask,
                                   EGLint contextMajorVersion,
                                   EGLint contextMinorVersion,
                                   EGLint contextFlags,
                                   EGLint contextResetNotificationStrategy,
                                   EGLenum api)
    : GHOST_Context(context_params),
      system_(system),
      native_display_(nativeDisplay),
      native_window_(nativeWindow),
      context_profile_mask_(contextProfileMask),
      context_major_version_(contextMajorVersion),
      context_minor_version_(contextMinorVersion),
      context_flags_(contextFlags),
      context_reset_notification_strategy_(contextResetNotificationStrategy),
      api_(api),
      context_(EGL_NO_CONTEXT),
      surface_(EGL_NO_SURFACE),
      display_(EGL_NO_DISPLAY),
      config_(EGL_NO_CONFIG_KHR),
      swap_interval_(1),
      shared_context_(
          choose_api(api, s_gl_sharedContext, s_gles_sharedContext, s_vg_sharedContext)),
      shared_count_(choose_api(api, s_gl_sharedCount, s_gles_sharedCount, s_vg_sharedCount)),
      surface_from_native_window_(false)
{
}

GHOST_ContextEGL::~GHOST_ContextEGL()
{
  if (display_ != EGL_NO_DISPLAY) {

    bindAPI(api_);

    if (context_ != EGL_NO_CONTEXT) {
      if (context_ == ::eglGetCurrentContext()) {
        EGL_CHK(::eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
      }
      if (context_ != shared_context_ || shared_count_ == 1) {
        assert(shared_count_ > 0);

        shared_count_--;

        if (shared_count_ == 0) {
          shared_context_ = EGL_NO_CONTEXT;
        }
        EGL_CHK(::eglDestroyContext(display_, context_));
      }
    }

    if (surface_ != EGL_NO_SURFACE) {
      EGL_CHK(::eglDestroySurface(display_, surface_));
    }
  }
}

GHOST_TSuccess GHOST_ContextEGL::swapBufferRelease()
{
  return EGL_CHK(::eglSwapBuffers(display_, surface_)) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::setSwapInterval(int interval)
{
  if (epoxy_egl_version(display_) >= 11) {
    if (EGL_CHK(::eglSwapInterval(display_, interval))) {
      swap_interval_ = interval;

      return GHOST_kSuccess;
    }
    return GHOST_kFailure;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::getSwapInterval(int &interval_out)
{
  /* This is a bit of a kludge because there does not seem to
   * be a way to query the swap interval with EGL. */
  interval_out = swap_interval_;

  return GHOST_kSuccess;
}

EGLDisplay GHOST_ContextEGL::getDisplay() const
{
  return display_;
}

EGLConfig GHOST_ContextEGL::getConfig() const
{
  return config_;
}

EGLContext GHOST_ContextEGL::getContext() const
{
  return context_;
}

GHOST_TSuccess GHOST_ContextEGL::activateDrawingContext()
{
  if (display_) {
    active_context_ = this;
    bindAPI(api_);
    return EGL_CHK(::eglMakeCurrent(display_, surface_, surface_, context_)) ? GHOST_kSuccess :
                                                                               GHOST_kFailure;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::releaseDrawingContext()
{
  if (display_) {
    active_context_ = nullptr;
    bindAPI(api_);

    return EGL_CHK(::eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) ?
               GHOST_kSuccess :
               GHOST_kFailure;
  }
  return GHOST_kFailure;
}

inline bool GHOST_ContextEGL::bindAPI(EGLenum api)
{
  if (epoxy_egl_version(display_) >= 12) {
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

  if (context_params_.is_stereo_visual) {
    fprintf(stderr, "Warning! Stereo OpenGL ES contexts are not supported.\n");
  }
  context_params_.is_stereo_visual = false; /* It doesn't matter what the Window wants. */

  EGLDisplay prev_display = eglGetCurrentDisplay();
  EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW);
  EGLSurface prev_read = eglGetCurrentSurface(EGL_READ);
  EGLContext prev_context = eglGetCurrentContext();

  EGLint egl_major = 0, egl_minor = 0;

  if (!EGL_CHK((display_ = ::eglGetDisplay(native_display_)) != EGL_NO_DISPLAY)) {
    goto error;
  }

  {
    const EGLBoolean init_display_result = ::eglInitialize(display_, &egl_major, &egl_minor);
    const EGLint init_display_error = (init_display_result) ? 0 : eglGetError();

    if (!init_display_result || (egl_major == 0 && egl_minor == 0)) {
      /* We failed to create a regular render window, retry and see if we can create a headless
       * render context. */
      ::eglTerminate(display_);

      const char *egl_extension_st = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
      assert(egl_extension_st != nullptr);
      assert(egl_extension_st == nullptr ||
             strstr(egl_extension_st, "EGL_MESA_platform_surfaceless") != nullptr);
      if (egl_extension_st == nullptr ||
          strstr(egl_extension_st, "EGL_MESA_platform_surfaceless") == nullptr)
      {
        egl_print_error("Failed to create display GPU context: ", init_display_error);
        fprintf(
            stderr,
            "Failed to create headless GPU context: No EGL_MESA_platform_surfaceless extension");
        goto error;
      }

      display_ = eglGetPlatformDisplayEXT(
          EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);

      const EGLBoolean headless_result = ::eglInitialize(display_, &egl_major, &egl_minor);
      const EGLint init_headless_error = (headless_result) ? 0 : eglGetError();

      if (!headless_result) {
        egl_print_error("Failed to create display GPU context: ", init_display_error);
        egl_print_error("Failed to create headless GPU context: ", init_headless_error);
        goto error;
      }
    }
  }

#ifdef WITH_GHOST_DEBUG
  fprintf(stderr, "EGL Version %d.%d\n", egl_major, egl_minor);
#endif

  if (!EGL_CHK(::eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))) {
    goto error;
  }
  if (!bindAPI(api_)) {
    goto error;
  }

  /* Build attribute list. */

  attrib_list.reserve(20);

  if (api_ == EGL_OPENGL_ES_API && epoxy_egl_version(display_) >= 12) {
    /* According to the spec it seems that you are required to set EGL_RENDERABLE_TYPE,
     * but some implementations (ANGLE) do not seem to care. */

    if (context_major_version_ == 1) {
      attrib_list.push_back(EGL_RENDERABLE_TYPE);
      attrib_list.push_back(EGL_OPENGL_ES_BIT);
    }
    else if (context_major_version_ == 2) {
      attrib_list.push_back(EGL_RENDERABLE_TYPE);
      attrib_list.push_back(EGL_OPENGL_ES2_BIT);
    }
    else if (context_major_version_ == 3) {
      attrib_list.push_back(EGL_RENDERABLE_TYPE);
      attrib_list.push_back(EGL_OPENGL_ES3_BIT_KHR);
    }
    else {
      fprintf(stderr,
              "Warning! Unable to request an ES context of version %d.%d\n",
              context_major_version_,
              context_minor_version_);
    }

    if (!((context_major_version_ == 1) ||
          (context_major_version_ == 2 && epoxy_egl_version(display_) >= 13) ||
          (context_major_version_ == 3 &&
           epoxy_has_egl_extension(display_, "KHR_create_context")) ||
          (context_major_version_ == 3 && epoxy_egl_version(display_) >= 15)))
    {
      fprintf(stderr,
              "Warning! May not be able to create a version %d.%d ES context with version %d.%d "
              "of EGL\n",
              context_major_version_,
              context_minor_version_,
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

  if (native_window_ == 0) {
    /* Off-screen surface. */
    attrib_list.push_back(EGL_SURFACE_TYPE);
    attrib_list.push_back(EGL_PBUFFER_BIT);
  }

  attrib_list.push_back(EGL_NONE);

  if (!EGL_CHK(::eglChooseConfig(display_, &(attrib_list[0]), &config_, 1, &num_config))) {
    goto error;
  }

  /* A common error is to assume that ChooseConfig worked because it returned EGL_TRUE. */
  if (num_config != 1) { /* `num_config` should be exactly 1. */
    goto error;
  }

  if (native_window_ != 0) {
    std::vector<EGLint> surface_attrib_list;
    surface_attrib_list.reserve(3);
#ifdef WITH_GHOST_WAYLAND
    /* Fix transparency issue on: `Wayland + Nouveau/Zink+NVK`. Due to unsupported texture formats
     * drivers can hit transparency code-paths resulting in showing the desktop in viewports.
     *
     * See #102994. */
    /* EGL_EXT_present_opaque isn't added to the latest release of epoxy, but is part of the latest
     * EGL https://github.com/KhronosGroup/EGL-Registry/blob/main/api/egl.xml */
    if (epoxy_has_egl_extension(display_, "EGL_EXT_present_opaque")) {
#  ifndef EGL_PRESENT_OPAQUE_EXT
#    define EGL_PRESENT_OPAQUE_EXT 0x31DF
#  endif
      surface_attrib_list.push_back(EGL_PRESENT_OPAQUE_EXT);
      surface_attrib_list.push_back(EGL_TRUE);
    }
#endif
    surface_attrib_list.push_back(EGL_NONE);

    surface_ = ::eglCreateWindowSurface(
        display_, config_, native_window_, surface_attrib_list.data());
    surface_from_native_window_ = true;
  }
  else {
    static const EGLint pb_attrib_list[] = {
        EGL_WIDTH,
        1,
        EGL_HEIGHT,
        1,
        EGL_NONE,
    };
    surface_ = ::eglCreatePbufferSurface(display_, config_, pb_attrib_list);
  }

  if (!EGL_CHK(surface_ != EGL_NO_SURFACE)) {
    goto error;
  }
  attrib_list.clear();

  if (epoxy_egl_version(display_) >= 15 || epoxy_has_egl_extension(display_, "KHR_create_context"))
  {
    if (api_ == EGL_OPENGL_API || api_ == EGL_OPENGL_ES_API) {
      if (context_major_version_ != 0) {
        attrib_list.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
        attrib_list.push_back(context_major_version_);
      }

      if (context_minor_version_ != 0) {
        attrib_list.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
        attrib_list.push_back(context_minor_version_);
      }

      if (context_flags_ != 0) {
        attrib_list.push_back(EGL_CONTEXT_FLAGS_KHR);
        attrib_list.push_back(context_flags_);
      }
    }
    else {
      if (context_major_version_ != 0 || context_minor_version_ != 0) {
        fprintf(stderr,
                "Warning! Cannot request specific versions of %s contexts.",
                api_string(api_).c_str());
      }

      if (context_flags_ != 0) {
        fprintf(stderr, "Warning! Flags cannot be set on %s contexts.", api_string(api_).c_str());
      }
    }

    if (api_ == EGL_OPENGL_API) {
      if (context_profile_mask_ != 0) {
        attrib_list.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
        attrib_list.push_back(context_profile_mask_);
      }
    }
    else {
      if (context_profile_mask_ != 0) {
        fprintf(
            stderr, "Warning! Cannot select profile for %s contexts.", api_string(api_).c_str());
      }
    }

    if (api_ == EGL_OPENGL_API || epoxy_egl_version(display_) >= 15) {
      if (context_reset_notification_strategy_ != 0) {
        attrib_list.push_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR);
        attrib_list.push_back(context_reset_notification_strategy_);
      }
    }
    else {
      if (context_reset_notification_strategy_ != 0) {
        fprintf(stderr,
                "Warning! EGL %d.%d cannot set the reset notification strategy on %s contexts.",
                egl_major,
                egl_minor,
                api_string(api_).c_str());
      }
    }
  }
  else {
    if (api_ == EGL_OPENGL_ES_API) {
      if (context_major_version_ != 0) {
        attrib_list.push_back(EGL_CONTEXT_CLIENT_VERSION);
        attrib_list.push_back(context_major_version_);
      }
    }
    else {
      if (context_major_version_ != 0 || context_minor_version_ != 0) {
        fprintf(stderr,
                "Warning! EGL %d.%d is unable to select between versions of %s.",
                egl_major,
                egl_minor,
                api_string(api_).c_str());
      }
    }

    if (context_flags_ != 0) {
      fprintf(stderr, "Warning! EGL %d.%d is unable to set context flags.", egl_major, egl_minor);
    }
    if (context_profile_mask_ != 0) {
      fprintf(stderr,
              "Warning! EGL %d.%d is unable to select between profiles.",
              egl_major,
              egl_minor);
    }
    if (context_reset_notification_strategy_ != 0) {
      fprintf(stderr,
              "Warning! EGL %d.%d is unable to set the reset notification strategies.",
              egl_major,
              egl_minor);
    }
  }

  attrib_list.push_back(EGL_NONE);

  context_ = ::eglCreateContext(display_, config_, shared_context_, &(attrib_list[0]));

  if (!EGL_CHK(context_ != EGL_NO_CONTEXT)) {
    goto error;
  }

  if (shared_context_ == EGL_NO_CONTEXT) {
    shared_context_ = context_;
  }

  shared_count_++;

  if (!EGL_CHK(::eglMakeCurrent(display_, surface_, surface_, context_))) {
    goto error;
  }

  {
    const GHOST_TVSyncModes vsync = getVSync();
    if (vsync != GHOST_kVSyncModeUnset) {
      setSwapInterval(int(vsync));
    }
  }

  if (native_window_ != 0) {
    initClearGL();
    ::eglSwapBuffers(display_, surface_);
  }

  active_context_ = this;
  return GHOST_kSuccess;

error:
  if (prev_display != EGL_NO_DISPLAY) {
    EGL_CHK(eglMakeCurrent(prev_display, prev_draw, prev_read, prev_context));
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextEGL::releaseNativeHandles()
{
  native_display_ = nullptr;

  native_window_ = 0;
  if (surface_from_native_window_) {
    surface_ = EGL_NO_SURFACE;
  }

  return GHOST_kSuccess;
}
