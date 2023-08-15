/* SPDX-FileCopyrightText: 2022-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemHeadless class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_DisplayManagerNULL.hh"
#include "GHOST_System.hh"
#include "GHOST_WindowNULL.hh"

#ifdef __linux__
#  include "GHOST_ContextEGL.hh"
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
    return 0;
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
    return GHOST_TCapabilityFlag(GHOST_CAPABILITY_FLAG_ALL &
                                 /* No windowing functionality supported. */
                                 ~(GHOST_kCapabilityWindowPosition | GHOST_kCapabilityCursorWarp |
                                   GHOST_kCapabilityPrimaryClipboard |
                                   GHOST_kCapabilityClipboardImages));
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
  GHOST_IContext *createOffscreenContext(GHOST_GPUSettings /*gpuSettings*/) override
  {
#ifdef __linux__
    GHOST_Context *context;
    for (int minor = 6; minor >= 3; --minor) {
      context = new GHOST_ContextEGL((GHOST_System *)this,
                                     false,
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
#else
    return nullptr;
#endif
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
      m_displayManager = new GHOST_DisplayManagerNULL();

      if (m_displayManager) {
        return GHOST_kSuccess;
      }
    }

    return GHOST_kFailure;
  }

  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_GPUSettings gpuSettings,
                              const bool /*exclusive*/,
                              const bool /*is_dialog*/,
                              const GHOST_IWindow *parentWindow) override
  {
    return new GHOST_WindowNULL(title,
                                left,
                                top,
                                width,
                                height,
                                state,
                                parentWindow,
                                gpuSettings.context_type,
                                ((gpuSettings.flags & GHOST_gpuStereoVisual) != 0));
  }

  GHOST_IWindow *getWindowUnderCursor(int32_t /*x*/, int32_t /*y*/) override
  {
    return nullptr;
  }
};
