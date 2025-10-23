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
               const GHOST_ContextParams &context_params,
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
   * virtual GHOST_TSuccess swapBufferAcquire() = 0;
   * virtual GHOST_TSuccess swapBufferRelease() = 0;
   * virtual GHOST_TSuccess setSwapInterval() = 0;
   * virtual GHOST_TSuccess getSwapInterval(int& interval_out) = 0;
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
    return context_ != nullptr;
  }

  /** \copydoc #GHOST_IWindow::getOSWindow */
  void *getOSWindow() const override;

  /** \copydoc #GHOST_IWindow::setPath */
  void setPath(const char * /*filepath*/) override
  {
    /* Pass. */
  }

  /** \copydoc #GHOST_IWindow::getWindowDecorationStyleFlags */
  virtual GHOST_TWindowDecorationStyleFlags getWindowDecorationStyleFlags() override;

  /** \copydoc #GHOST_IWindow::setWindowDecorationStyleFlags */
  virtual void setWindowDecorationStyleFlags(
      GHOST_TWindowDecorationStyleFlags style_flags) override;

  /** \copydoc #GHOST_IWindow::setWindowDecorationStyleSettings */
  virtual void setWindowDecorationStyleSettings(
      GHOST_WindowDecorationStyleSettings decoration_settings) override;

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
  GHOST_TSuccess setCursorShape(GHOST_TStandardCursor cursor_shape) override;

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
  GHOST_TSuccess getSwapInterval(int &interval_out) override;

  /** \copydoc #GHOST_IWindow::setAcceptDragOperation */
  void setAcceptDragOperation(bool can_accept) override;

  /** \copydoc #GHOST_IWindow::canAcceptDragOperation */
  bool canAcceptDragOperation() const override;

  /** \copydoc #GHOST_IWindow::setModifiedState */
  GHOST_TSuccess setModifiedState(bool is_unsaved_changes) override;

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

  /** \copydoc #GHOST_IWindow::swapBufferAcquire */
  GHOST_TSuccess swapBufferAcquire() override;
  /** \copydoc #GHOST_IWindow::swapBuffers */
  GHOST_TSuccess swapBufferRelease() override;

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
    return user_data_;
  }

  /** \copydoc #GHOST_IWindow::setUserData */
  void setUserData(const GHOST_TUserDataPtr user_data) override
  {
    user_data_ = user_data;
  }

  /** \copydoc #GHOST_IWindow::getNativePixelSize */
  float getNativePixelSize() override
  {
    if (native_pixel_size_ > 0.0f) {
      return native_pixel_size_;
    }
    return 1.0f;
  }

  /** \copydoc #GHOST_IWindow::getDPIHint */
  uint16_t getDPIHint() override
  {
    return 96;
  }

  /** \copydoc #GHOST_IWindow::getHDRInfo */
  GHOST_WindowHDRInfo getHDRInfo() override
  {
    return hdr_info_;
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
  GHOST_TDrawingContextType drawing_context_type_;

  /** The window user data */
  GHOST_TUserDataPtr user_data_;

  /** The current visibility of the cursor */
  bool cursor_visible_;

  /** The current grabbed state of the cursor */
  GHOST_TGrabCursorMode cursor_grab_;

  /** Grab cursor axis. */
  GHOST_TAxisFlag cursor_grab_axis_;

  /** Initial grab location. */
  int32_t cursor_grab_init_pos_[2];

  /** Accumulated offset from cursor_grab_init_pos_. */
  int32_t cursor_grab_accum_pos_[2];

  /** Wrap the cursor within this region. */
  GHOST_Rect cursor_grab_bounds_;

  /** The current shape of the cursor */
  GHOST_TStandardCursor cursor_shape_;

  /** The presence of progress indicator with the application icon */
  bool progress_bar_visible_;

  /** The acceptance of the "drop candidate" of the current drag & drop operation. */
  bool can_accept_drag_operation_;

  /** Modified state : are there unsaved changes */
  bool is_unsaved_changes_;

  /** Stores whether this is a full screen window. */
  bool full_screen_;

  /** Window Decoration Styles. */
  GHOST_TWindowDecorationStyleFlags window_decoration_style_flags_;
  GHOST_WindowDecorationStyleSettings window_decoration_style_settings_;

  /** The desired parameters to use when initializing the context for this window. */
  GHOST_ContextParams want_context_params_;

  /** Full-screen width */
  uint32_t full_screen_width_;
  /** Full-screen height */
  uint32_t full_screen_height_;

  /* OSX only, retina screens */
  float native_pixel_size_;

  GHOST_WindowHDRInfo hdr_info_ = GHOST_WINDOW_HDR_INFO_NONE;

 private:
  GHOST_Context *context_;
};

inline GHOST_TDrawingContextType GHOST_Window::getDrawingContextType()
{
  return drawing_context_type_;
}

inline bool GHOST_Window::getCursorVisibility() const
{
  return cursor_visible_;
}

inline GHOST_TGrabCursorMode GHOST_Window::getCursorGrabMode() const
{
  return cursor_grab_;
}

inline bool GHOST_Window::getCursorGrabModeIsWarp() const
{
  return (cursor_grab_ == GHOST_kGrabWrap) || (cursor_grab_ == GHOST_kGrabHide);
}

inline GHOST_TAxisFlag GHOST_Window::getCursorGrabAxis() const
{
  return cursor_grab_axis_;
}

inline void GHOST_Window::getCursorGrabInitPos(int32_t &x, int32_t &y) const
{
  x = cursor_grab_init_pos_[0];
  y = cursor_grab_init_pos_[1];
}

inline void GHOST_Window::getCursorGrabAccum(int32_t &x, int32_t &y) const
{
  x = cursor_grab_accum_pos_[0];
  y = cursor_grab_accum_pos_[1];
}

inline void GHOST_Window::setCursorGrabAccum(int32_t x, int32_t y)
{
  cursor_grab_accum_pos_[0] = x;
  cursor_grab_accum_pos_[1] = y;
}

inline GHOST_TStandardCursor GHOST_Window::getCursorShape() const
{
  return cursor_shape_;
}
