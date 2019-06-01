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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IWindow interface class.
 */

#ifndef __GHOST_IWINDOW_H__
#define __GHOST_IWINDOW_H__

#include "STR_String.h"
#include "GHOST_Rect.h"
#include "GHOST_Types.h"

/**
 * Interface for GHOST windows.
 *
 * You can create a window with the system's GHOST_ISystem::createWindow
 * method.
 * \see GHOST_ISystem#createWindow
 *
 * There are two coordinate systems:
 *
 * - The screen coordinate system. The origin of the screen is located in the
 *   upper left corner of the screen.</li>
 * - The client rectangle coordinate system. The client rectangle of a window
 *   is the area that is drawable by the application (excluding title bars etc.).
 */
class GHOST_IWindow {
 public:
  /**
   * Destructor.
   */
  virtual ~GHOST_IWindow()
  {
  }

  /**
   * Returns indication as to whether the window is valid.
   * \return The validity of the window.
   */
  virtual bool getValid() const = 0;

  /**
   * Returns the associated OS object/handle
   * \return The associated OS object/handle
   */
  virtual void *getOSWindow() const = 0;

  /**
   * Returns the type of drawing context used in this window.
   * \return The current type of drawing context.
   */
  virtual GHOST_TDrawingContextType getDrawingContextType() = 0;

  /**
   * Tries to install a rendering context in this window.
   * \param type  The type of rendering context installed.
   * \return Indication as to whether installation has succeeded.
   */
  virtual GHOST_TSuccess setDrawingContextType(GHOST_TDrawingContextType type) = 0;

  /**
   * Sets the title displayed in the title bar.
   * \param title The title to display in the title bar.
   */
  virtual void setTitle(const STR_String &title) = 0;

  /**
   * Returns the title displayed in the title bar.
   * \param title The title displayed in the title bar.
   */
  virtual void getTitle(STR_String &title) const = 0;

  /**
   * Returns the window rectangle dimensions.
   * These are screen coordinates.
   * \param bounds The bounding rectangle of the window.
   */
  virtual void getWindowBounds(GHOST_Rect &bounds) const = 0;

  /**
   * Returns the client rectangle dimensions.
   * The left and top members of the rectangle are always zero.
   * \param bounds The bounding rectangle of the client area of the window.
   */
  virtual void getClientBounds(GHOST_Rect &bounds) const = 0;

  /**
   * Resizes client rectangle width.
   * \param width The new width of the client area of the window.
   */
  virtual GHOST_TSuccess setClientWidth(GHOST_TUns32 width) = 0;

  /**
   * Resizes client rectangle height.
   * \param height The new height of the client area of the window.
   */
  virtual GHOST_TSuccess setClientHeight(GHOST_TUns32 height) = 0;

  /**
   * Resizes client rectangle.
   * \param width     The new width of the client area of the window.
   * \param height    The new height of the client area of the window.
   */
  virtual GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height) = 0;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX   The x-coordinate on the screen.
   * \param inY   The y-coordinate on the screen.
   * \param outX  The x-coordinate in the client rectangle.
   * \param outY  The y-coordinate in the client rectangle.
   */
  virtual void screenToClient(GHOST_TInt32 inX,
                              GHOST_TInt32 inY,
                              GHOST_TInt32 &outX,
                              GHOST_TInt32 &outY) const = 0;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX   The x-coordinate in the client rectangle.
   * \param inY   The y-coordinate in the client rectangle.
   * \param outX  The x-coordinate on the screen.
   * \param outY  The y-coordinate on the screen.
   */
  virtual void clientToScreen(GHOST_TInt32 inX,
                              GHOST_TInt32 inY,
                              GHOST_TInt32 &outX,
                              GHOST_TInt32 &outY) const = 0;

  /**
   * Tells if the ongoing drag'n'drop object can be accepted upon mouse drop
   */
  virtual void setAcceptDragOperation(bool canAccept) = 0;

  /**
   * Returns acceptance of the dropped object
   * Usually called by the "object dropped" event handling function
   */
  virtual bool canAcceptDragOperation() const = 0;

  /**
   * Returns the state of the window (normal, minimized, maximized).
   * \return The state of the window.
   */
  virtual GHOST_TWindowState getState() const = 0;

  /**
   * Sets the state of the window (normal, minimized, maximized).
   * \param state The state of the window.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setState(GHOST_TWindowState state) = 0;

  /**
   * Sets the window "modified" status, indicating unsaved changes
   * \param isUnsavedChanges Unsaved changes or not
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setModifiedState(bool isUnsavedChanges) = 0;

  /**
   * Gets the window "modified" status, indicating unsaved changes
   * \return True if there are unsaved changes
   */
  virtual bool getModifiedState() = 0;

  /**
   * Sets the order of the window (bottom, top).
   * \param order The order of the window.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setOrder(GHOST_TWindowOrder order) = 0;

  /**
   * Swaps front and back buffers of a window.
   * \return  A boolean success indicator.
   */
  virtual GHOST_TSuccess swapBuffers() = 0;

  /**
   * Sets the swap interval for swapBuffers.
   * \param interval The swap interval to use.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess setSwapInterval(int interval) = 0;

  /**
   * Gets the current swap interval for swapBuffers.
   * \param intervalOut pointer to location to return swap interval
   * (left untouched if there is an error)
   * \return A boolean success indicator of if swap interval was successfully read.
   */
  virtual GHOST_TSuccess getSwapInterval(int &intervalOut) = 0;

  /**
   * Activates the drawing context of this window.
   * \return  A boolean success indicator.
   */
  virtual GHOST_TSuccess activateDrawingContext() = 0;

  /**
   * Gets the OpenGL framebuffer associated with the window's contents.
   * \return The name of an OpenGL framebuffer object.
   */
  virtual unsigned int getDefaultFramebuffer() = 0;

  /**
   * Invalidates the contents of this window.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess invalidate() = 0;

  /**
   * Returns the window user data.
   * \return The window user data.
   */
  virtual GHOST_TUserDataPtr getUserData() const = 0;

  /**
   * Changes the window user data.
   * \param userData The window user data.
   */
  virtual void setUserData(const GHOST_TUserDataPtr userData) = 0;

  /**
   * Returns the tablet data (pressure etc).
   * \return The tablet data (pressure etc).
   */
  virtual const GHOST_TabletData *GetTabletData() = 0;

  /***************************************************************************************
   * Progress bar functionality
   ***************************************************************************************/

  /**
   * Sets the progress bar value displayed in the window/application icon
   * \param progress The progress %
   */
  virtual GHOST_TSuccess setProgressBar(float progress) = 0;

  /**
   * Hides the progress bar in the icon
   */
  virtual GHOST_TSuccess endProgressBar() = 0;

  /***************************************************************************************
   * Cursor management functionality
   ***************************************************************************************/

  /**
   * Returns the current cursor shape.
   * \return  The current cursor shape.
   */
  virtual GHOST_TStandardCursor getCursorShape() const = 0;

  /**
   * Set the shape of the cursor.
   * \param cursorShape:  The new cursor shape type id.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCursorShape(GHOST_TStandardCursor cursorShape) = 0;

  /**
   * Set the shape of the cursor to a custom cursor.
   * \param   bitmap  The bitmap data for the cursor.
   * \param   mask    The mask data for the cursor.
   * \param   hotX    The X coordinate of the cursor hotspot.
   * \param   hotY    The Y coordinate of the cursor hotspot.
   * \return  Indication of success.
   */
  virtual GHOST_TSuccess setCustomCursorShape(GHOST_TUns8 bitmap[16][2],
                                              GHOST_TUns8 mask[16][2],
                                              int hotX,
                                              int hotY) = 0;

  virtual GHOST_TSuccess setCustomCursorShape(GHOST_TUns8 *bitmap,
                                              GHOST_TUns8 *mask,
                                              int sizex,
                                              int sizey,
                                              int hotX,
                                              int hotY,
                                              int fg_color,
                                              int bg_color) = 0;

  /**
   * Returns the visibility state of the cursor.
   * \return  The visibility state of the cursor.
   */
  virtual bool getCursorVisibility() const = 0;

  /**
   * Shows or hides the cursor.
   * \param   visible The new visibility state of the cursor.
   * \return  Indication of success.
   */
  virtual GHOST_TSuccess setCursorVisibility(bool visible) = 0;

  /**
   * Grabs the cursor for a modal operation.
   * \param   grab The new grab state of the cursor.
   * \return  Indication of success.
   */
  virtual GHOST_TSuccess setCursorGrab(GHOST_TGrabCursorMode /*mode*/,
                                       GHOST_TAxisFlag /*wrap_axis*/,
                                       GHOST_Rect * /*bounds*/,
                                       GHOST_TInt32 /*mouse_ungrab_xy*/[2])
  {
    return GHOST_kSuccess;
  }

  /** */
  virtual GHOST_TSuccess beginFullScreen() const = 0;
  virtual GHOST_TSuccess endFullScreen() const = 0;

  virtual float getNativePixelSize(void) = 0;

  /**
   * Returns the recommended DPI for this window.
   * \return The recommended DPI for this window.
   */
  virtual GHOST_TUns16 getDPIHint() = 0;

#ifdef WITH_INPUT_IME
  /**
   * Enable IME attached to the given window, i.e. allows user-input
   * events to be dispatched to the IME.
   * \param x Requested x-coordinate of the rectangle
   * \param y Requested y-coordinate of the rectangle
   * \param w Requested width of the rectangle
   * \param h Requested height of the rectangle
   * \param complete Whether or not to complete the ongoing composition
   * true:  Start a new composition
   * false: Move the IME windows to the given position without finishing it.
   */
  virtual void beginIME(
      GHOST_TInt32 x, GHOST_TInt32 y, GHOST_TInt32 w, GHOST_TInt32 h, int completed) = 0;

  /**
   * Disable the IME attached to the given window, i.e. prohibits any user-input
   * events from being dispatched to the IME.
   */
  virtual void endIME() = 0;
#endif /* WITH_INPUT_IME */

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IWindow")
#endif
};

#endif  // __GHOST_IWINDOW_H__
