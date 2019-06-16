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
 * Declaration of GHOST_WindowNULL class.
 */

#ifndef __GHOST_WINDOWNULL_H__
#define __GHOST_WINDOWNULL_H__

#include "GHOST_Window.h"

#include <map>

class STR_String;
class GHOST_SystemNULL;

class GHOST_WindowNULL : public GHOST_Window {
 public:
  const GHOST_TabletData *GetTabletData()
  {
    return NULL;
  }

  GHOST_WindowNULL(GHOST_SystemNULL *system,
                   const STR_String &title,
                   GHOST_TInt32 left,
                   GHOST_TInt32 top,
                   GHOST_TUns32 width,
                   GHOST_TUns32 height,
                   GHOST_TWindowState state,
                   const GHOST_TEmbedderWindowID parentWindow,
                   GHOST_TDrawingContextType type,
                   const bool stereoVisual)
      : GHOST_Window(width, height, state, stereoVisual, false), m_system(system)
  {
    setTitle(title);
  }

 protected:
  GHOST_TSuccess installDrawingContext(GHOST_TDrawingContextType type)
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess removeDrawingContext()
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode)
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape)
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                            GHOST_TUns8 *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor)
  {
    return GHOST_kSuccess;
  }

  bool getValid() const
  {
    return true;
  }
  void setTitle(const STR_String &title)
  { /* nothing */
  }
  void getTitle(STR_String &title) const
  {
    title = "untitled";
  }
  void getWindowBounds(GHOST_Rect &bounds) const
  {
    getClientBounds(bounds);
  }
  void getClientBounds(GHOST_Rect &bounds) const
  { /* nothing */
  }
  GHOST_TSuccess setClientWidth(GHOST_TUns32 width)
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setClientHeight(GHOST_TUns32 height)
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
  {
    return GHOST_kFailure;
  }
  void screenToClient(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const
  {
    outX = inX;
    outY = inY;
  }
  void clientToScreen(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const
  {
    outX = inX;
    outY = inY;
  }
  GHOST_TSuccess swapBuffers()
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess activateDrawingContext()
  {
    return GHOST_kFailure;
  }
  ~GHOST_WindowNULL()
  { /* nothing */
  }
  GHOST_TSuccess setWindowCursorVisibility(bool visible)
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setState(GHOST_TWindowState state)
  {
    return GHOST_kSuccess;
  }
  GHOST_TWindowState getState() const
  {
    return GHOST_kWindowStateNormal;
  }
  GHOST_TSuccess invalidate()
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setOrder(GHOST_TWindowOrder order)
  {
    return GHOST_kSuccess;
  }

  GHOST_TSuccess beginFullScreen() const
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess endFullScreen() const
  {
    return GHOST_kSuccess;
  }

 private:
  GHOST_SystemNULL *m_system;

  /**
   * \param type  The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type)
  {
    return NULL;
  }
};

#endif  // __GHOST_WINDOWNULL_H__
