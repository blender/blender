/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemSDL class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_DisplayManagerSDL.hh"
#include "GHOST_Event.hh"
#include "GHOST_System.hh"
#include "GHOST_TimerManager.hh"
#include "GHOST_WindowSDL.hh"

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

  bool processEvents(bool waitForEvent) override;

  bool setConsoleWindowState(GHOST_TConsoleWindowState /*action*/) override
  {
    return false;
  }

  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const override;

  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const override;

  GHOST_TCapabilityFlag getCapabilities() const override;

  char *getClipboard(bool selection) const override;

  void putClipboard(const char *buffer, bool selection) const override;

  uint64_t getMilliSeconds() const override;

  uint8_t getNumDisplays() const override;

  GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const override;

  GHOST_TSuccess setCursorPosition(int32_t x, int32_t y) override;

  void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const override;

  void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const override;

  GHOST_IContext *createOffscreenContext(GHOST_GPUSettings gpuSettings) override;

  GHOST_TSuccess disposeContext(GHOST_IContext *context) override;

 private:
  GHOST_TSuccess init() override;

  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_GPUSettings gpuSettings,
                              const bool exclusive = false,
                              const bool is_dialog = false,
                              const GHOST_IWindow *parentWindow = nullptr) override;

  /* SDL specific */
  GHOST_WindowSDL *findGhostWindow(SDL_Window *sdl_win);

  bool generateWindowExposeEvents();

  void processEvent(SDL_Event *sdl_event);

  /** The vector of windows that need to be updated. */
  std::vector<GHOST_WindowSDL *> m_dirty_windows;
};
