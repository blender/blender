/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemNULL class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_DisplayManagerNULL.h"
#include "GHOST_System.h"
#include "GHOST_WindowNULL.h"

class GHOST_WindowNULL;

class GHOST_SystemNULL : public GHOST_System {
 public:
  GHOST_SystemNULL() : GHOST_System()
  { /* nop */
  }
  ~GHOST_SystemNULL()
  { /* nop */
  }
  bool processEvents(bool waitForEvent)
  {
    return false;
  }
  int setConsoleWindowState(GHOST_TConsoleWindowState action)
  {
    return 0;
  }
  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const
  {
    return GHOST_kSuccess;
  }
  char *getClipboard(bool selection) const
  {
    return nullptr;
  }
  void putClipboard(const char *buffer, bool selection) const
  { /* nop */
  }
  uint64_t getMilliSeconds() const
  {
    return 0;
  }
  uint8_t getNumDisplays() const
  {
    return uint8_t(1);
  }
  GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setCursorPosition(int32_t x, int32_t y)
  {
    return GHOST_kFailure;
  }
  void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const
  { /* nop */
  }
  void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const
  { /* nop */
  }
  GHOST_IContext *createOffscreenContext(GHOST_GLSettings glSettings)
  {
    return nullptr;
  }
  GHOST_TSuccess disposeContext(GHOST_IContext *context)
  {
    return GHOST_kFailure;
  }

  GHOST_TSuccess init()
  {
    GHOST_TSuccess success = GHOST_System::init();

    if (success) {
      m_displayManager = new GHOST_DisplayManagerNULL(this);

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
                              GHOST_TDrawingContextType type,
                              GHOST_GLSettings glSettings,
                              const bool exclusive,
                              const bool is_dialog,
                              const GHOST_IWindow *parentWindow)
  {
    return new GHOST_WindowNULL(this,
                                title,
                                left,
                                top,
                                width,
                                height,
                                state,
                                parentWindow,
                                type,
                                ((glSettings.flags & GHOST_glStereoVisual) != 0));
  }

  GHOST_IWindow *getWindowUnderCursor(int32_t x, int32_t y)
  {
    return nullptr;
  }
};
