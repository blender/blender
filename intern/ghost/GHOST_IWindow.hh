/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IWindow interface class.
 */

#pragma once

#include "GHOST_Rect.hh"
#include "GHOST_Types.h"

#include <stdlib.h>
#include <string>

class GHOST_IContext;

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
 *   upper left corner of the screen.
 * - The client rectangle coordinate system. The client rectangle of a window
 *   is the area that is drawable by the application (excluding title bars etc.).
 */
class GHOST_IWindow {
 public:
  /**
   * Destructor.
   */
  virtual ~GHOST_IWindow() {}

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
   * \param type: The type of rendering context installed.
   * \return Indication as to whether installation has succeeded.
   */
  virtual GHOST_TSuccess setDrawingContextType(GHOST_TDrawingContextType type) = 0;

  /**
   * Returns the drawing context used in this window.
   * \return The current drawing context.
   */
  virtual GHOST_IContext *getDrawingContext() = 0;

  /**
   * Sets the title displayed in the title bar.
   * \param title: The title to display in the title bar.
   */
  virtual void setTitle(const char *title) = 0;

  /**
   * Returns the title displayed in the title bar.
   * \param title: The title displayed in the title bar.
   */
  virtual std::string getTitle() const = 0;

  /**
   * Returns the window rectangle dimensions.
   * These are screen coordinates.
   * \param bounds: The bounding rectangle of the window.
   */
  virtual void getWindowBounds(GHOST_Rect &bounds) const = 0;

  /**
   * Returns the client rectangle dimensions.
   * The left and top members of the rectangle are always zero.
   * \param bounds: The bounding rectangle of the client area of the window.
   */
  virtual void getClientBounds(GHOST_Rect &bounds) const = 0;

  /**
   * Resizes client rectangle width.
   * \param width: The new width of the client area of the window.
   */
  virtual GHOST_TSuccess setClientWidth(uint32_t width) = 0;

  /**
   * Resizes client rectangle height.
   * \param height: The new height of the client area of the window.
   */
  virtual GHOST_TSuccess setClientHeight(uint32_t height) = 0;

  /**
   * Resizes client rectangle.
   * \param width: The new width of the client area of the window.
   * \param height: The new height of the client area of the window.
   */
  virtual GHOST_TSuccess setClientSize(uint32_t width, uint32_t height) = 0;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX: The x-coordinate on the screen.
   * \param inY: The y-coordinate on the screen.
   * \param outX: The x-coordinate in the client rectangle.
   * \param outY: The y-coordinate in the client rectangle.
   */
  virtual void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const = 0;

  /**
   * Converts a point in client rectangle coordinates to screen coordinates.
   * \param inX: The x-coordinate in the client rectangle.
   * \param inY: The y-coordinate in the client rectangle.
   * \param outX: The x-coordinate on the screen.
   * \param outY: The y-coordinate on the screen.
   */
  virtual void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const = 0;

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
   * \param state: The state of the window.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setState(GHOST_TWindowState state) = 0;

  /**
   * Sets the window "modified" status, indicating unsaved changes
   * \param isUnsavedChanges: Unsaved changes or not.
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
   * \param order: The order of the window.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setOrder(GHOST_TWindowOrder order) = 0;

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess swapBuffers() = 0;

  /**
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess setSwapInterval(int interval) = 0;

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param intervalOut: pointer to location to return swap interval.
   * (left untouched if there is an error)
   * \return A boolean success indicator of if swap interval was successfully read.
   */
  virtual GHOST_TSuccess getSwapInterval(int &intervalOut) = 0;

  /**
   * Activates the drawing context of this window.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess activateDrawingContext() = 0;

  /**
   * Gets the OpenGL frame-buffer associated with the window's contents.
   * \return The name of an OpenGL frame-buffer object.
   */
  virtual unsigned int getDefaultFramebuffer() = 0;

  /**
   * Gets the Vulkan framebuffer related resource handles associated with the Vulkan context.
   * Needs to be called after each swap events as the framebuffer will change.
   * \return  A boolean success indicator.
   */
  virtual GHOST_TSuccess getVulkanBackbuffer(
      void *image, void *framebuffer, void *render_pass, void *extent, uint32_t *fb_id) = 0;

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
   * \param userData: The window user data.
   */
  virtual void setUserData(const GHOST_TUserDataPtr userData) = 0;

  virtual bool isDialog() const = 0;

  /***************************************************************************************
   * Progress bar functionality
   ***************************************************************************************/

  /**
   * Sets the progress bar value displayed in the window/application icon
   * \param progress: The progress percentage (0.0 to 1.0).
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
   * \return The current cursor shape.
   */
  virtual GHOST_TStandardCursor getCursorShape() const = 0;

  /**
   * Set the shape of the cursor.
   * \param cursorShape: The new cursor shape type id.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCursorShape(GHOST_TStandardCursor cursorShape) = 0;

  virtual GHOST_TSuccess getCursorGrabBounds(GHOST_Rect &bounds) const = 0;

  virtual void getCursorGrabState(GHOST_TGrabCursorMode &mode,
                                  GHOST_TAxisFlag &axis_flag,
                                  GHOST_Rect &bounds,
                                  bool &use_software_cursor) = 0;

  virtual bool getCursorGrabUseSoftwareDisplay() = 0;

  /**
   * Test if the standard cursor shape is supported by current platform.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor cursorShape) = 0;

  /**
   * Set the shape of the cursor to a custom cursor.
   * \param bitmap: The bitmap data for the cursor.
   * \param mask: The mask data for the cursor.
   * \param hotX: The X coordinate of the cursor hot-spot.
   * \param hotY: The Y coordinate of the cursor hot-spot.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCustomCursorShape(uint8_t *bitmap,
                                              uint8_t *mask,
                                              int sizex,
                                              int sizey,
                                              int hotX,
                                              int hotY,
                                              bool canInvertColor) = 0;

  virtual GHOST_TSuccess getCursorBitmap(GHOST_CursorBitmapRef *bitmap) = 0;

  /**
   * Returns the visibility state of the cursor.
   * \return The visibility state of the cursor.
   */
  virtual bool getCursorVisibility() const = 0;

  /**
   * Shows or hides the cursor.
   * \param visible: The new visibility state of the cursor.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCursorVisibility(bool visible) = 0;

  /**
   * Grabs the cursor for a modal operation.
   * \param grab: The new grab state of the cursor.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCursorGrab(GHOST_TGrabCursorMode /*mode*/,
                                       GHOST_TAxisFlag /*wrap_axis*/,
                                       GHOST_Rect * /*bounds*/,
                                       int32_t /*mouse_ungrab_xy*/[2])
  {
    return GHOST_kSuccess;
  }

  /** */
  virtual GHOST_TSuccess beginFullScreen() const = 0;
  virtual GHOST_TSuccess endFullScreen() const = 0;

  virtual float getNativePixelSize() = 0;

  /**
   * Returns the recommended DPI for this window.
   * \return The recommended DPI for this window.
   */
  virtual uint16_t getDPIHint() = 0;

#ifdef WITH_INPUT_IME
  /**
   * Enable IME attached to the given window, i.e. allows user-input
   * events to be dispatched to the IME.
   * \param x: Requested x-coordinate of the rectangle.
   * \param y: Requested y-coordinate of the rectangle.
   * \param w: Requested width of the rectangle.
   * \param h: Requested height of the rectangle.
   * \param complete: Whether or not to complete the ongoing composition.
   * - true:  Start a new composition
   * - false: Move the IME windows to the given position without finishing it.
   */
  virtual void beginIME(int32_t x, int32_t y, int32_t w, int32_t h, bool completed) = 0;

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
