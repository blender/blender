/* SPDX-FileCopyrightText: 2011-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WindowNULL class.
 */

#pragma once

#include "GHOST_Window.hh"

#include <map>

class GHOST_SystemHeadless;

class GHOST_WindowNULL : public GHOST_Window {
 public:
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor /*cursor_shape*/) override
  {
    return GHOST_kSuccess;
  }

  GHOST_WindowNULL(const char * /*title*/,
                   int32_t /*left*/,
                   int32_t /*top*/,
                   uint32_t width,
                   uint32_t height,
                   GHOST_TWindowState state,
                   const GHOST_IWindow * /*parent_window*/,
                   GHOST_TDrawingContextType /*type*/,
                   const GHOST_ContextParams &context_params)
      : GHOST_Window(width, height, state, context_params, false)
  {
  }

 protected:
  GHOST_TSuccess installDrawingContext(GHOST_TDrawingContextType /*type*/)
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess removeDrawingContext()
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode /*mode*/) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor /*shape*/) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCustomCursorShape(const uint8_t * /*bitmap*/,
                                            const uint8_t * /*mask*/,
                                            const int /*size*/[2],
                                            const int /*hot_spot*/[2],
                                            bool /*can_invert_color*/) override
  {
    return GHOST_kSuccess;
  }

  bool getValid() const override
  {
    return true;
  }
  void setTitle(const char * /*title*/) override
  { /* nothing */
  }
  std::string getTitle() const override
  {
    return "untitled";
  }
  void setPath(const char * /*filepath*/) override
  {
    /* Pass. */
  }
  void getWindowBounds(GHOST_Rect &bounds) const override
  {
    getClientBounds(bounds);
  }
  void getClientBounds(GHOST_Rect & /*bounds*/) const override
  { /* nothing */
  }
  GHOST_TSuccess setClientWidth(uint32_t /*width*/) override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setClientHeight(uint32_t /*height*/) override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setClientSize(uint32_t /*width*/, uint32_t /*height*/) override
  {
    return GHOST_kFailure;
  }
  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override
  {
    outX = inX;
    outY = inY;
  }
  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override
  {
    outX = inX;
    outY = inY;
  }
  GHOST_TSuccess swapBufferRelease() override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess activateDrawingContext() override
  {
    return GHOST_kFailure;
  }
  ~GHOST_WindowNULL() override = default;

  GHOST_TSuccess setWindowCursorVisibility(bool /*visible*/) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setState(GHOST_TWindowState /*state*/) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TWindowState getState() const override
  {
    return GHOST_kWindowStateNormal;
  }
  GHOST_TSuccess invalidate() override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setOrder(GHOST_TWindowOrder /*order*/) override
  {
    return GHOST_kSuccess;
  }

 private:
  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType /*type*/) override
  {
    return nullptr;
  }
};
