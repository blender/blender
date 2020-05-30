/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 *
 * Declaration of GHOST_WindowWayland class.
 */

#ifndef __GHOST_WINDOWWAYLAND_H__
#define __GHOST_WINDOWWAYLAND_H__

#include "GHOST_Window.h"

class GHOST_SystemWayland;

struct window_t;

class GHOST_WindowWayland : public GHOST_Window {
 public:
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor cursorShape) override;

  GHOST_WindowWayland(GHOST_SystemWayland *system,
                      const char *title,
                      GHOST_TInt32 left,
                      GHOST_TInt32 top,
                      GHOST_TUns32 width,
                      GHOST_TUns32 height,
                      GHOST_TWindowState state,
                      const GHOST_IWindow *parentWindow,
                      GHOST_TDrawingContextType type,
                      const bool is_dialog,
                      const bool stereoVisual,
                      const bool exclusive);

  ~GHOST_WindowWayland() override;

  GHOST_TSuccess close();

  GHOST_TSuccess activate();

  GHOST_TSuccess deactivate();

  GHOST_TSuccess notify_size();

 protected:
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode) override;

  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) override;

  GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                            GHOST_TUns8 *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor) override;

  void setTitle(const char *title) override;

  std::string getTitle() const override;

  void getWindowBounds(GHOST_Rect &bounds) const override;

  void getClientBounds(GHOST_Rect &bounds) const override;

  GHOST_TSuccess setClientWidth(GHOST_TUns32 width) override;

  GHOST_TSuccess setClientHeight(GHOST_TUns32 height) override;

  GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height) override;

  void screenToClient(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const override;

  void clientToScreen(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const override;

  GHOST_TSuccess setWindowCursorVisibility(bool visible) override;

  GHOST_TSuccess setState(GHOST_TWindowState state) override;

  GHOST_TWindowState getState() const override;

  GHOST_TSuccess invalidate() override;

  GHOST_TSuccess setOrder(GHOST_TWindowOrder order) override;

  GHOST_TSuccess beginFullScreen() const override;

  GHOST_TSuccess endFullScreen() const override;

  bool isDialog() const override;

 private:
  GHOST_SystemWayland *m_system;
  struct window_t *w;
  std::string title;

  /**
   * \param type  The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) override;
};

#endif  // __GHOST_WINDOWWAYLAND_H__
