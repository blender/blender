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
 * Declaration of GHOST_SystemWayland class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_System.h"
#include "GHOST_WindowWayland.h"

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <string>

class GHOST_WindowWayland;

struct display_t;

class GHOST_SystemWayland : public GHOST_System {
 public:
  GHOST_SystemWayland();

  ~GHOST_SystemWayland() override;

  bool processEvents(bool waitForEvent) override;

  int toggleConsole(int action) override;

  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const override;

  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const override;

  GHOST_TUns8 *getClipboard(bool selection) const override;

  void putClipboard(GHOST_TInt8 *buffer, bool selection) const override;

  GHOST_TUns8 getNumDisplays() const override;

  GHOST_TSuccess getCursorPosition(GHOST_TInt32 &x, GHOST_TInt32 &y) const override;

  GHOST_TSuccess setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y) override;

  void getMainDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const override;

  void getAllDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const override;

  GHOST_IContext *createOffscreenContext() override;

  GHOST_TSuccess disposeContext(GHOST_IContext *context) override;

  GHOST_IWindow *createWindow(const char *title,
                              GHOST_TInt32 left,
                              GHOST_TInt32 top,
                              GHOST_TUns32 width,
                              GHOST_TUns32 height,
                              GHOST_TWindowState state,
                              GHOST_TDrawingContextType type,
                              GHOST_GLSettings glSettings,
                              const bool exclusive,
                              const bool is_dialog,
                              const GHOST_IWindow *parentWindow) override;

  wl_display *display();

  wl_compositor *compositor();

  xdg_wm_base *shell();

  void setSelection(const std::string &selection);

  GHOST_TSuccess setCursorShape(GHOST_TStandardCursor shape);

  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor cursorShape);

  GHOST_TSuccess setCustomCursorShape(GHOST_TUns8 *bitmap,
                                      GHOST_TUns8 *mask,
                                      int sizex,
                                      int sizey,
                                      int hotX,
                                      int hotY,
                                      bool canInvertColor);

  GHOST_TSuccess setCursorVisibility(bool visible);

  GHOST_TSuccess setCursorGrab(const GHOST_TGrabCursorMode mode, wl_surface *surface);

 private:
  struct display_t *d;
  std::string selection;
};
