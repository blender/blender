/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemHeadless class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_System.hh"
#include "GHOST_WindowNULL.hh"

#if defined(WITH_OPENGL_BACKEND) && defined(__linux__)
#  include "GHOST_ContextEGL.hh"
#endif
#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif
#include "GHOST_ContextNone.hh"

class GHOST_WindowNULL;

class GHOST_SystemHeadless : public GHOST_System {
 public:
  GHOST_SystemHeadless() : GHOST_System()
  { /* nop */
  }
  ~GHOST_SystemHeadless() override = default;

  bool processEvents(bool /*waitForEvent*/) override
  {
    return false;
  }
  bool setConsoleWindowState(GHOST_TConsoleWindowState /*action*/) override
  {
    return false;
  }
  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys & /*keys*/) const override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess getButtons(GHOST_Buttons & /*buttons*/) const override
  {
    return GHOST_kSuccess;
  }
  GHOST_TCapabilityFlag getCapabilities() const override
  {
    return GHOST_TCapabilityFlag(
        GHOST_CAPABILITY_FLAG_ALL &
        /* No windowing functionality supported.
         * In most cases this value doesn't matter for the headless backend.
         *
         * Nevertheless, don't advertise support.
         *
         * NOTE: order the following flags as they they're declared in the source. */
        ~(
            /* Wrap. */
            GHOST_kCapabilityWindowPosition |
            /* Wrap. */
            GHOST_kCapabilityCursorWarp |
            /* Wrap. */
            GHOST_kCapabilityClipboardPrimary |
            /* Wrap. */
            GHOST_kCapabilityClipboardImage |
            /* Wrap. */
            GHOST_kCapabilityDesktopSample |
            /* Wrap. */
            GHOST_kCapabilityInputIME |
            /* Wrap. */
            GHOST_kCapabilityWindowDecorationStyles |
            /* Wrap. */
            GHOST_kCapabilityKeyboardHyperKey |
            /* Wrap. */
            GHOST_kCapabilityCursorRGBA |
            /* Wrap. */
            GHOST_kCapabilityCursorGenerator |
            /* Wrap. */
            GHOST_kCapabilityMultiMonitorPlacement |
            /* Wrap. */
            GHOST_kCapabilityWindowPath)

    );
  }
  char *getClipboard(bool /*selection*/) const override
  {
    return nullptr;
  }
  void putClipboard(const char * /*buffer*/, bool /*selection*/) const override
  { /* nop */
  }
  uint64_t getMilliSeconds() const override
  {
    return 0;
  }
  uint8_t getNumDisplays() const override
  {
    return uint8_t(1);
  }
  GHOST_TSuccess getCursorPosition(int32_t & /*x*/, int32_t & /*y*/) const override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setCursorPosition(int32_t /*x*/, int32_t /*y*/) override
  {
    return GHOST_kFailure;
  }
  void getMainDisplayDimensions(uint32_t & /*width*/, uint32_t & /*height*/) const override
  { /* nop */
  }
  void getAllDisplayDimensions(uint32_t & /*width*/, uint32_t & /*height*/) const override
  { /* nop */
  }
  GHOST_IContext *createOffscreenContext(GHOST_GPUSettings gpu_settings) override
  {
    const GHOST_ContextParams context_params_offscreen =
        GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS_OFFSCREEN(gpu_settings);
    /* This may not be used depending on the build configuration. */
    (void)context_params_offscreen;

    switch (gpu_settings.context_type) {
#ifdef WITH_VULKAN_BACKEND
      case GHOST_kDrawingContextTypeVulkan: {
#  ifdef _WIN32
        GHOST_Context *context = new GHOST_ContextVK(
            context_params_offscreen, (HWND)0, 1, 2, gpu_settings.preferred_device);
#  elif defined(__APPLE__)
        GHOST_Context *context = new GHOST_ContextVK(
            context_params_offscreen, nullptr, 1, 2, gpu_settings.preferred_device);
#  else
        GHOST_Context *context = new GHOST_ContextVK(context_params_offscreen,
                                                     GHOST_kVulkanPlatformHeadless,
                                                     0,
                                                     0,
                                                     nullptr,
                                                     nullptr,
                                                     nullptr,
                                                     1,
                                                     2,
                                                     gpu_settings.preferred_device);
#  endif
        if (context->initializeDrawingContext()) {
          return context;
        }

        delete context;
        return nullptr;
      }
#endif

#if defined(WITH_OPENGL_BACKEND) && defined(__linux__)
      case GHOST_kDrawingContextTypeOpenGL: {
        GHOST_Context *context;
        for (int minor = 6; minor >= 3; --minor) {
          context = new GHOST_ContextEGL((GHOST_System *)this,
                                         context_params_offscreen,
                                         EGLNativeWindowType(0),
                                         EGLNativeDisplayType(EGL_DEFAULT_DISPLAY),
                                         EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                         4,
                                         minor,
                                         GHOST_OPENGL_EGL_CONTEXT_FLAGS,
                                         GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
                                         EGL_OPENGL_API);

          if (context->initializeDrawingContext()) {
            return context;
          }
          delete context;
          context = nullptr;
        }

        return context;
      }
#endif

      default:
        /* Unsupported backend. */
        return nullptr;
    }

    return nullptr;
  }
  GHOST_TSuccess disposeContext(GHOST_IContext *context) override
  {
    delete context;

    return GHOST_kSuccess;
  }

  GHOST_TSuccess init() override
  {
    GHOST_TSuccess success = GHOST_System::init();

    if (success) {
      return GHOST_kSuccess;
    }

    return GHOST_kFailure;
  }

  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_GPUSettings gpu_settings,
                              const bool /*exclusive*/,
                              const bool /*is_dialog*/,
                              const GHOST_IWindow *parent_window) override
  {
    const GHOST_ContextParams context_params = GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS(
        gpu_settings);
    return new GHOST_WindowNULL(title,
                                left,
                                top,
                                width,
                                height,
                                state,
                                parent_window,
                                gpu_settings.context_type,
                                context_params);
  }

  GHOST_IWindow *getWindowUnderCursor(int32_t /*x*/, int32_t /*y*/) override
  {
    return nullptr;
  }
};
