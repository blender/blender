/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_Window class.
 */

#pragma once

#include "GHOST_IWindow.hh"

class GHOST_Context;

/**
 * Platform independent implementation of GHOST_IWindow.
 * Dimensions are given in screen coordinates that are relative to the
 * upper-left corner of the screen.
 * Implements part of the GHOST_IWindow interface and adds some methods to
 * be implemented by sub-classes of this class.
 */
class GHOST_Window : public GHOST_IWindow {
 public:
  /**
   * Constructor.
   * Creates a new window and opens it.
   * To check if the window was created properly, use the getValid() method.
   * \param width: The width of the window.
   * \param height: The height of the window.
   * \param state: The state the window is initially opened with.
   * \param wantStereoVisual: Stereo visual for quad buffered stereo.
   * \param exclusive: Use to show the window on top and ignore others (used full-screen).
   */
  GHOST_Window(uint32_t width,
               uint32_t height,
               GHOST_TWindowState state,
               const bool wantStereoVisual = false,
               const bool exclusive = false);

  /**
   * \section Interface inherited from GHOST_IWindow left for derived class
   * implementation.
   * virtual  bool getValid() const = 0;
   * virtual void setTitle(const char * title) = 0;
   * virtual std::string getTitle() const = 0;
   * virtual  void getWindowBounds(GHOST_Rect& bounds) const = 0;
   * virtual  void getClientBounds(GHOST_Rect& bounds) const = 0;
   * virtual  GHOST_TSuccess setClientWidth(uint32_t width) = 0;
   * virtual  GHOST_TSuccess setClientHeight(uint32_t height) = 0;
   * virtual  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height) = 0;
   * virtual void screenToClient(
   *     int32_t inX, int32_t inY, int32_t& outX, int32_t& outY) const = 0;
   * virtual void clientToScreen(
   *     int32_t inX, int32_t inY, int32_t& outX, int32_t& outY) const = 0;
   * virtual GHOST_TWindowState getState() const = 0;
   * virtual GHOST_TSuccess setState(GHOST_TWindowState state) = 0;
   * virtual GHOST_TSuccess setOrder(GHOST_TWindowOrder order) = 0;
   * virtual GHOST_TSuccess swapBuffers() = 0;
   * virtual GHOST_TSuccess setSwapInterval() = 0;
   * virtual GHOST_TSuccess getSwapInterval(int& intervalOut) = 0;
   * virtual GHOST_TSuccess activateDrawingContext() = 0;
   * virtual GHOST_TSuccess invalidate() = 0;
   */

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  virtual ~GHOST_Window();

  /**
   * Returns indication as to whether the window is valid.
   * \return The validity of the window.
   */
  virtual bool getValid() const override
  {
    return m_context != nullptr;
  }

  /**
   * Returns the associated OS object/handle
   * \return The associated OS object/handle
   */
  virtual void *getOSWindow() const override;

  /**
   * Returns the current cursor shape.
   * \return The current cursor shape.
   */
  inline GHOST_TStandardCursor getCursorShape() const override;

  inline bool isDialog() const override
  {
    return false;
  }

  /**
   * Set the shape of the cursor.
   * \param cursorShape: The new cursor shape type id.
   * \return Indication of success.
   */
  GHOST_TSuccess setCursorShape(GHOST_TStandardCursor cursorShape) override;

  /**
   * Set the shape of the cursor to a custom cursor.
   * \param bitmap: The bitmap data for the cursor.
   * \param mask: The mask data for the cursor.
   * \param hotX: The X coordinate of the cursor hot-spot.
   * \param hotY: The Y coordinate of the cursor hot-spot.
   * \return Indication of success.
   */
  GHOST_TSuccess setCustomCursorShape(uint8_t *bitmap,
                                      uint8_t *mask,
                                      int sizex,
                                      int sizey,
                                      int hotX,
                                      int hotY,
                                      bool canInvertColor) override;

  GHOST_TSuccess getCursorBitmap(GHOST_CursorBitmapRef *bitmap) override;

  /**
   * Returns the visibility state of the cursor.
   * \return The visibility state of the cursor.
   */
  inline bool getCursorVisibility() const override;
  inline GHOST_TGrabCursorMode getCursorGrabMode() const;
  inline bool getCursorGrabModeIsWarp() const;
  inline GHOST_TAxisFlag getCursorGrabAxis() const;
  inline void getCursorGrabInitPos(int32_t &x, int32_t &y) const;
  inline void getCursorGrabAccum(int32_t &x, int32_t &y) const;
  inline void setCursorGrabAccum(int32_t x, int32_t y);

  /**
   * Shows or hides the cursor.
   * \param visible: The new visibility state of the cursor.
   * \return Indication of success.
   */
  GHOST_TSuccess setCursorVisibility(bool visible) override;

  /**
   * Sets the cursor grab.
   * \param mode: The new grab state of the cursor.
   * \return Indication of success.
   */
  GHOST_TSuccess setCursorGrab(GHOST_TGrabCursorMode mode,
                               GHOST_TAxisFlag wrap_axis,
                               GHOST_Rect *bounds,
                               int32_t mouse_ungrab_xy[2]) override;

  /**
   * Gets the cursor grab region, if unset the window is used.
   * reset when grab is disabled.
   */
  GHOST_TSuccess getCursorGrabBounds(GHOST_Rect &bounds) const override;

  void getCursorGrabState(GHOST_TGrabCursorMode &mode,
                          GHOST_TAxisFlag &wrap_axis,
                          GHOST_Rect &bounds,
                          bool &use_software_cursor) override;
  /**
   * Return true when a software cursor should be used.
   */
  bool getCursorGrabUseSoftwareDisplay() override;

  /**
   * Sets the progress bar value displayed in the window/application icon
   * \param progress: The progress percentage (0.0 to 1.0).
   */
  virtual GHOST_TSuccess setProgressBar(float /*progress*/) override
  {
    return GHOST_kFailure;
  }

  /**
   * Hides the progress bar in the icon
   */
  virtual GHOST_TSuccess endProgressBar() override
  {
    return GHOST_kFailure;
  }

  /**
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int interval) override;

  /**
   * Gets the current swap interval for #swapBuffers.
   * \return An integer.
   */
  GHOST_TSuccess getSwapInterval(int &intervalOut) override;

  /**
   * Tells if the ongoing drag'n'drop object can be accepted upon mouse drop
   */
  void setAcceptDragOperation(bool canAccept) override;

  /**
   * Returns acceptance of the dropped object
   * Usually called by the "object dropped" event handling function
   */
  bool canAcceptDragOperation() const override;

  /**
   * Sets the window "modified" status, indicating unsaved changes
   * \param isUnsavedChanges: Unsaved changes or not.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setModifiedState(bool isUnsavedChanges) override;

  /**
   * Gets the window "modified" status, indicating unsaved changes
   * \return True if there are unsaved changes
   */
  virtual bool getModifiedState() override;

  /**
   * Returns the type of drawing context used in this window.
   * \return The current type of drawing context.
   */
  inline GHOST_TDrawingContextType getDrawingContextType() override;

  /**
   * Tries to install a rendering context in this window.
   * Child classes do not need to overload this method,
   * They should overload #newDrawingContext instead.
   * \param type: The type of rendering context installed.
   * \return Indication as to whether installation has succeeded.
   */
  GHOST_TSuccess setDrawingContextType(GHOST_TDrawingContextType type) override;

  /**
   * Returns the drawing context used in this window.
   * \return The current drawing context.
   */
  virtual GHOST_IContext *getDrawingContext() override;

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess swapBuffers() override;

  /**
   * Activates the drawing context of this window.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess activateDrawingContext() override;

  /**
   * Updates the drawing context of this window. Needed
   * whenever the window is changed.
   * \return Indication of success.
   */
  GHOST_TSuccess updateDrawingContext();

  /**
   * Get the drawing context associated with this window.
   *\return Pointer to the context object.
   */
  GHOST_Context *getContext();

  /**
   * Gets the OpenGL frame-buffer associated with the window's contents.
   * \return The ID of an OpenGL frame-buffer object.
   */
  virtual unsigned int getDefaultFramebuffer() override;

  /**
   * Gets the Vulkan framebuffer related resource handles associated with the Vulkan context.
   * Needs to be called after each swap events as the framebuffer will change.
   * \return  A boolean success indicator.
   */
  virtual GHOST_TSuccess getVulkanBackbuffer(
      void *image, void *framebuffer, void *render_pass, void *extent, uint32_t *fb_id) override;

  /**
   * Returns the window user data.
   * \return The window user data.
   */
  inline GHOST_TUserDataPtr getUserData() const override
  {
    return m_userData;
  }

  /**
   * Changes the window user data.
   * \param userData: The window user data.
   */
  void setUserData(const GHOST_TUserDataPtr userData) override
  {
    m_userData = userData;
  }

  float getNativePixelSize() override
  {
    if (m_nativePixelSize > 0.0f) {
      return m_nativePixelSize;
    }
    return 1.0f;
  }

  /**
   * Returns the recommended DPI for this window.
   * \return The recommended DPI for this window.
   */
  virtual inline uint16_t getDPIHint() override
  {
    return 96;
  }

#ifdef WITH_INPUT_IME
  virtual void beginIME(
      int32_t /*x*/, int32_t /*y*/, int32_t /*w*/, int32_t /*h*/, bool /*completed*/) override
  {
    /* do nothing temporarily if not in windows */
  }

  virtual void endIME() override
  {
    /* do nothing temporarily if not in windows */
  }
#endif /* WITH_INPUT_IME */

 protected:
  /**
   * Tries to install a rendering context in this window.
   * \param type: The type of rendering context installed.
   * \return Indication as to whether installation has succeeded.
   */
  virtual GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) = 0;

  /**
   * Sets the cursor visibility on the window using
   * native window system calls.
   */
  virtual GHOST_TSuccess setWindowCursorVisibility(bool visible) = 0;

  /**
   * Sets the cursor grab on the window using
   * native window system calls.
   */
  virtual GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode /*mode*/)
  {
    return GHOST_kSuccess;
  }

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  virtual GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) = 0;

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  virtual GHOST_TSuccess setWindowCustomCursorShape(uint8_t *bitmap,
                                                    uint8_t *mask,
                                                    int szx,
                                                    int szy,
                                                    int hotX,
                                                    int hotY,
                                                    bool canInvertColor) = 0;

  GHOST_TSuccess releaseNativeHandles();

  /** The drawing context installed in this window. */
  GHOST_TDrawingContextType m_drawingContextType;

  /** The window user data */
  GHOST_TUserDataPtr m_userData;

  /** The current visibility of the cursor */
  bool m_cursorVisible;

  /** The current grabbed state of the cursor */
  GHOST_TGrabCursorMode m_cursorGrab;

  /** Grab cursor axis. */
  GHOST_TAxisFlag m_cursorGrabAxis;

  /** Initial grab location. */
  int32_t m_cursorGrabInitPos[2];

  /** Accumulated offset from m_cursorGrabInitPos. */
  int32_t m_cursorGrabAccumPos[2];

  /** Wrap the cursor within this region. */
  GHOST_Rect m_cursorGrabBounds;

  /** The current shape of the cursor */
  GHOST_TStandardCursor m_cursorShape;

  /** The presence of progress indicator with the application icon */
  bool m_progressBarVisible;

  /** The acceptance of the "drop candidate" of the current drag'n'drop operation */
  bool m_canAcceptDragOperation;

  /** Modified state : are there unsaved changes */
  bool m_isUnsavedChanges;

  /** Stores whether this is a full screen window. */
  bool m_fullScreen;

  /** Whether to attempt to initialize a context with a stereo frame-buffer. */
  bool m_wantStereoVisual;

  /** Full-screen width */
  uint32_t m_fullScreenWidth;
  /** Full-screen height */
  uint32_t m_fullScreenHeight;

  /* OSX only, retina screens */
  float m_nativePixelSize;

 private:
  GHOST_Context *m_context;
};

inline GHOST_TDrawingContextType GHOST_Window::getDrawingContextType()
{
  return m_drawingContextType;
}

inline bool GHOST_Window::getCursorVisibility() const
{
  return m_cursorVisible;
}

inline GHOST_TGrabCursorMode GHOST_Window::getCursorGrabMode() const
{
  return m_cursorGrab;
}

inline bool GHOST_Window::getCursorGrabModeIsWarp() const
{
  return (m_cursorGrab == GHOST_kGrabWrap) || (m_cursorGrab == GHOST_kGrabHide);
}

inline GHOST_TAxisFlag GHOST_Window::getCursorGrabAxis() const
{
  return m_cursorGrabAxis;
}

inline void GHOST_Window::getCursorGrabInitPos(int32_t &x, int32_t &y) const
{
  x = m_cursorGrabInitPos[0];
  y = m_cursorGrabInitPos[1];
}

inline void GHOST_Window::getCursorGrabAccum(int32_t &x, int32_t &y) const
{
  x = m_cursorGrabAccumPos[0];
  y = m_cursorGrabAccumPos[1];
}

inline void GHOST_Window::setCursorGrabAccum(int32_t x, int32_t y)
{
  m_cursorGrabAccumPos[0] = x;
  m_cursorGrabAccumPos[1] = y;
}

inline GHOST_TStandardCursor GHOST_Window::getCursorShape() const
{
  return m_cursorShape;
}
