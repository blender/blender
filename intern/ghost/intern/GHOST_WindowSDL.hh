/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WindowSDL class.
 */

#pragma once

#include "GHOST_SystemSDL.hh"
#include "GHOST_Window.hh"

#include <map>

extern "C" {
#include "SDL.h"
}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#  error "SDL 2.0 or newer is needed to build with Ghost"
#endif

class GHOST_SystemSDL;

class GHOST_WindowSDL : public GHOST_Window {
 private:
  GHOST_SystemSDL *m_system;
  bool m_valid_setup;
  bool m_invalid_window;

  SDL_Window *m_sdl_win;
  SDL_Cursor *m_sdl_custom_cursor;

 public:
  GHOST_WindowSDL(GHOST_SystemSDL *system,
                  const char *title,
                  int32_t left,
                  int32_t top,
                  uint32_t width,
                  uint32_t height,
                  GHOST_TWindowState state,
                  GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
                  const bool stereoVisual = false,
                  const bool exclusive = false,
                  const GHOST_IWindow *parentWindow = nullptr);

  ~GHOST_WindowSDL();

  /* SDL specific */
  SDL_Window *getSDLWindow()
  {
    return m_sdl_win;
  }

  GHOST_TSuccess invalidate() override;

  /**
   * called by the X11 system implementation when expose events
   * for the window have been pushed onto the GHOST queue
   */

  void validate()
  {
    m_invalid_window = false;
  }

  bool getValid() const override;

  void getWindowBounds(GHOST_Rect &bounds) const override;
  void getClientBounds(GHOST_Rect &bounds) const override;

 protected:
  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) override;

  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode) override;

  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) override;
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor shape) override;

  GHOST_TSuccess setWindowCustomCursorShape(uint8_t *bitmap,
                                            uint8_t *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor) override;

  GHOST_TSuccess setWindowCursorVisibility(bool visible) override;

  void setTitle(const char *title) override;

  std::string getTitle() const override;

  GHOST_TSuccess setClientWidth(uint32_t width) override;

  GHOST_TSuccess setClientHeight(uint32_t height) override;

  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height) override;

  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  GHOST_TSuccess setState(GHOST_TWindowState state) override;

  GHOST_TWindowState getState() const override;

  GHOST_TSuccess setOrder(GHOST_TWindowOrder /*order*/) override
  {
    // TODO
    return GHOST_kSuccess;
  }

  // TODO
  GHOST_TSuccess beginFullScreen() const override
  {
    return GHOST_kFailure;
  }

  GHOST_TSuccess endFullScreen() const override
  {
    return GHOST_kFailure;
  }

  uint16_t getDPIHint() override;
};
