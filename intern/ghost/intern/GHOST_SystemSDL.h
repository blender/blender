/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemSDL class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_DisplayManagerSDL.h"
#include "GHOST_Event.h"
#include "GHOST_System.h"
#include "GHOST_TimerManager.h"
#include "GHOST_WindowSDL.h"

extern "C" {
#include "SDL.h"
}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#  error "SDL 2.0 or newer is needed to build with Ghost"
#endif

class GHOST_WindowSDL;

class GHOST_SystemSDL : public GHOST_System {
 public:
  void addDirtyWindow(GHOST_WindowSDL *bad_wind);

  GHOST_SystemSDL();
  ~GHOST_SystemSDL();

  bool processEvents(bool waitForEvent);

  int setConsoleWindowState(GHOST_TConsoleWindowState /*action*/)
  {
    return 0;
  }

  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const;

  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const;

  char *getClipboard(bool selection) const;

  void putClipboard(const char *buffer, bool selection) const;

  uint64_t getMilliSeconds();

  uint8_t getNumDisplays() const;

  GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const;

  GHOST_TSuccess setCursorPosition(int32_t x, int32_t y);

  void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const;

  void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const;

  GHOST_IContext *createOffscreenContext(GHOST_GLSettings glSettings);

  GHOST_TSuccess disposeContext(GHOST_IContext *context);

 private:
  GHOST_TSuccess init();

  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_TDrawingContextType type,
                              GHOST_GLSettings glSettings,
                              const bool exclusive = false,
                              const bool is_dialog = false,
                              const GHOST_IWindow *parentWindow = NULL);

  /* SDL specific */
  GHOST_WindowSDL *findGhostWindow(SDL_Window *sdl_win);

  bool generateWindowExposeEvents();

  void processEvent(SDL_Event *sdl_event);

  /** The vector of windows that need to be updated. */
  std::vector<GHOST_WindowSDL *> m_dirty_windows;
};
