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
  ~GHOST_Window() override;

  /** \copydoc #GHOST_IWindow::getValid */
  bool getValid() const override
  {
    return m_context != nullptr;
  }

  /** \copydoc #GHOST_IWindow::getOSWindow */
  void *getOSWindow() const override;

  /** \copydoc #GHOST_IWindow::setPath */
  GHOST_TSuccess setPath(const char * /*filepath*/) override
  {
    return GHOST_kFailure;
  }

  /** \copydoc #GHOST_IWindow::getWindowDecorationStyleFlags */
  virtual GHOST_TWindowDecorationStyleFlags getWindowDecorationStyleFlags() override;

  /** \copydoc #GHOST_IWindow::setWindowDecorationStyleFlags */
  virtual void setWindowDecorationStyleFlags(
      GHOST_TWindowDecorationStyleFlags styleFlags) override;

  /** \copydoc #GHOST_IWindow::setWindowDecorationStyleSettings */
  virtual void setWindowDecorationStyleSettings(
      GHOST_WindowDecorationStyleSettings decorationSettings) override;

  /** \copydoc #GHOST_IWindow::applyWindowDecorationStyle */
  virtual GHOST_TSuccess applyWindowDecorationStyle() override
  {
    return GHOST_kSuccess;
  }

  /** \copydoc #GHOST_IWindow::getCursorShape */
  inline GHOST_TStandardCursor getCursorShape() const override;

  bool isDialog() const override
  {
    return false;
  }

  /** \copydoc #GHOST_IWindow::setCursorShape */
  GHOST_TSuccess setCursorShape(GHOST_TStandardCursor cursorShape) override;

  /** \copydoc #GHOST_IWindow::setCustomCursorShape */
  GHOST_TSuccess setCustomCursorShape(const uint8_t *bitmap,
                                      const uint8_t *mask,
                                      const int size[2],
                                      const int hot_spot[2],
                                      bool can_invert_color) override;

  /** \copydoc #GHOST_IWindow::setCustomCursorGenerator */
  GHOST_TSuccess setCustomCursorGenerator(GHOST_CursorGenerator *cursor_generator) override;

  GHOST_TSuccess getCursorBitmap(GHOST_CursorBitmapRef *bitmap) override;

  /** \copydoc #GHOST_IWindow::getCursorVisibility */
  inline bool getCursorVisibility() const override;
  inline GHOST_TGrabCursorMode getCursorGrabMode() const;
  inline bool getCursorGrabModeIsWarp() const;
  inline GHOST_TAxisFlag getCursorGrabAxis() const;
  inline void getCursorGrabInitPos(int32_t &x, int32_t &y) const;
  inline void getCursorGrabAccum(int32_t &x, int32_t &y) const;
  inline void setCursorGrabAccum(int32_t x, int32_t y);

  /** \copydoc #GHOST_IWindow::setCursorVisibility */
  GHOST_TSuccess setCursorVisibility(bool visible) override;

  /** \copydoc #GHOST_IWindow::setCursorGrab */
  GHOST_TSuccess setCursorGrab(GHOST_TGrabCursorMode mode,
                               GHOST_TAxisFlag wrap_axis,
                               GHOST_Rect *bounds,
                               int32_t mouse_ungrab_xy[2]) override;

  /** \copydoc #GHOST_IWindow::getCursorGrabBounds */
  GHOST_TSuccess getCursorGrabBounds(GHOST_Rect &bounds) const override;

  void getCursorGrabState(GHOST_TGrabCursorMode &mode,
                          GHOST_TAxisFlag &wrap_axis,
                          GHOST_Rect &bounds,
                          bool &use_software_cursor) override;
  /** \copydoc #GHOST_IWindow::getCursorGrabUseSoftwareDisplay */
  bool getCursorGrabUseSoftwareDisplay() override;

  /** \copydoc #GHOST_IWindow::setProgressBar */
  GHOST_TSuccess setProgressBar(float /*progress*/) override
  {
    return GHOST_kFailure;
  }

  /** \copydoc #GHOST_IWindow::endProgressBar */
  GHOST_TSuccess endProgressBar() override
  {
    return GHOST_kFailure;
  }

  /** \copydoc #GHOST_IWindow::setSwapInterval */
  GHOST_TSuccess setSwapInterval(int interval) override;
  /** \copydoc #GHOST_IWindow::getSwapInterval */
  GHOST_TSuccess getSwapInterval(int &intervalOut) override;

  /** \copydoc #GHOST_IWindow::setAcceptDragOperation */
  void setAcceptDragOperation(bool canAccept) override;

  /** \copydoc #GHOST_IWindow::canAcceptDragOperation */
  bool canAcceptDragOperation() const override;

  /** \copydoc #GHOST_IWindow::setModifiedState */
  GHOST_TSuccess setModifiedState(bool isUnsavedChanges) override;

  /** \copydoc #GHOST_IWindow::getModifiedState */
  bool getModifiedState() override;

  /** \copydoc #GHOST_IWindow::getDrawingContextType */
  inline GHOST_TDrawingContextType getDrawingContextType() override;

  /**
   * \copydoc #GHOST_IWindow::setDrawingContextType
   *
   * \note Child classes do not need to overload this method,
   * They should overload #newDrawingContext instead.
   */
  GHOST_TSuccess setDrawingContextType(GHOST_TDrawingContextType type) override;

  /** \copydoc #GHOST_IWindow::getDrawingContext */
  GHOST_IContext *getDrawingContext() override;

  /** \copydoc #GHOST_IWindow::swapBuffers */
  GHOST_TSuccess swapBuffers() override;

  /** \copydoc #GHOST_IWindow::activateDrawingContext */
  GHOST_TSuccess activateDrawingContext() override;

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

  /** \copydoc #GHOST_IWindow::getDefaultFramebuffer */
  unsigned int getDefaultFramebuffer() override;

#ifdef WITH_VULKAN_BACKEND
  /** \copydoc #GHOST_GetVulkanSwapChainFormat */
  virtual GHOST_TSuccess getVulkanSwapChainFormat(
      GHOST_VulkanSwapChainData *r_swap_chain_data) override;
#endif

  /** \copydoc #GHOST_IWindow::getUserData */
  GHOST_TUserDataPtr getUserData() const override
  {
    return m_userData;
  }

  /** \copydoc #GHOST_IWindow::setUserData */
  void setUserData(const GHOST_TUserDataPtr userData) override
  {
    m_userData = userData;
  }

  /** \copydoc #GHOST_IWindow::getNativePixelSize */
  float getNativePixelSize() override
  {
    if (m_nativePixelSize > 0.0f) {
      return m_nativePixelSize;
    }
    return 1.0f;
  }

  /** \copydoc #GHOST_IWindow::getDPIHint */
  uint16_t getDPIHint() override
  {
    return 96;
  }

#ifdef WITH_INPUT_IME
  void beginIME(
      int32_t /*x*/, int32_t /*y*/, int32_t /*w*/, int32_t /*h*/, bool /*completed*/) override
  {
    /* do nothing temporarily if not in windows */
  }

  void endIME() override
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

  /** \copydoc #GHOST_IWindow::setWindowCursorShape */
  virtual GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) = 0;

  /** \copydoc #GHOST_IWindow::setWindowCustomCursorShape */
  virtual GHOST_TSuccess setWindowCustomCursorShape(const uint8_t *bitmap,
                                                    const uint8_t *mask,
                                                    const int size[2],
                                                    const int hot_size[2],
                                                    bool can_invert_color) = 0;
  /** \copydoc #GHOST_IWindow::setWindowCustomCursorGenerator */
  virtual GHOST_TSuccess setWindowCustomCursorGenerator(GHOST_CursorGenerator *cursor_generator)
  {
    cursor_generator->free_fn(cursor_generator);
    return GHOST_kFailure;
  };

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

  /** The acceptance of the "drop candidate" of the current drag & drop operation. */
  bool m_canAcceptDragOperation;

  /** Modified state : are there unsaved changes */
  bool m_isUnsavedChanges;

  /** Stores whether this is a full screen window. */
  bool m_fullScreen;

  /** Window Decoration Styles. */
  GHOST_TWindowDecorationStyleFlags m_windowDecorationStyleFlags;
  GHOST_WindowDecorationStyleSettings m_windowDecorationStyleSettings;

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
