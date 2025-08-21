/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WindowIOS class.
 */

#pragma once

#ifndef __APPLE__
#  error Apple OSX only!
#endif  // __APPLE__

#include "GHOST_Window.hh"
#ifdef WITH_INPUT_IME
#  include "GHOST_Event.hh"
#endif

@class UIView;
@class MTKView;
@class UIViewController;
@class UIWindow;
@class UITextField;

struct CGSize;
struct CGPoint;

class GHOST_SystemIOS;

class GHOST_WindowIOS : public GHOST_Window {
 public:
  UIWindow *rootWindow;
  /**
   * Constructor.
   * Creates a new window and opens it.
   * To check if the window was created properly, use the #getValid() method.
   * \param systemCocoa: The associated system class to forward events to.
   * \param title: The text shown in the title bar of the window.
   * \param left: The coordinate of the left edge of the window.
   * \param bottom: The coordinate of the bottom edge of the window.
   * \param width: The width the window.
   * \param height: The height the window.
   * \param state: The state the window is initially opened with.
   * \param type: The type of drawing context installed in this window.
   * \param stereoVisual: Stereo visual for quad buffered stereo.
   */
  GHOST_WindowIOS(GHOST_SystemIOS *system_ios,
                  const char *title,
                  int32_t left,
                  int32_t bottom,
                  uint32_t width,
                  uint32_t height,
                  GHOST_TWindowState state,
                  GHOST_TDrawingContextType type,
                  const GHOST_ContextParams &context_params,
                  bool /*is_dialog*/,
                  GHOST_WindowIOS *parent_window);

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  ~GHOST_WindowIOS() override;

  /**
   * Returns indication as to whether the window is valid.
   * \return The validity of the window.
   */
  bool getValid() const override;

  /**
   * Returns the associated NSWindow object
   * \return The associated NSWindow object
   */
  void *getOSWindow() const override;

  /**
   * Swaps the current framebuffer to the screen
   * \return Success or failure
   */
  GHOST_TSuccess swapBuffers() override;

  /**
   * Sets the title displayed in the title bar.
   * \param title: The title to display in the title bar.
   */
  void setTitle(const char *title) override;
  /**
   * Returns the title displayed in the title bar.
   * \param title: The title displayed in the title bar.
   */
  std::string getTitle() const override;

  /**
   * Makes sure we get another draw request.
   */
  void needsDisplayUpdate();

  /**
   * Returns the window rectangle dimensions.
   * The dimensions are given in screen coordinates that are
   * relative to the upper-left corner of the screen.
   * \param bounds: The bounding rectangle of the window.
   */
  void getWindowBounds(GHOST_Rect &bounds) const override;

  /**
   * Returns the client rectangle dimensions.
   * The left and top members of the rectangle are always zero.
   * \param bounds: The bounding rectangle of the client area of the window.
   */
  void getClientBounds(GHOST_Rect &bounds) const override;

  /**
   * Resizes client rectangle width.
   * \param width: The new width of the client area of the window.
   */
  GHOST_TSuccess setClientWidth(uint32_t width) override;

  /**
   * Resizes client rectangle height.
   * \param height: The new height of the client area of the window.
   */
  GHOST_TSuccess setClientHeight(uint32_t height) override;

  /**
   * Resizes client rectangle.
   * \param width: The new width of the client area of the window.
   * \param height: The new height of the client area of the window.
   */
  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height) override;

  /**
   * Returns the state of the window (normal, minimized, maximized).
   * \return The state of the window.
   */
  GHOST_TWindowState getState() const override;

  /**
   * Sets the window "modified" status, indicating unsaved changes
   * \param isUnsavedChanges: Unsaved changes or not.
   * \return Indication of success.
   */
  GHOST_TSuccess setModifiedState(bool isUnsavedChanges) override;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX: The x-coordinate on the screen.
   * \param inY: The y-coordinate on the screen.
   * \param outX: The x-coordinate in the client rectangle.
   * \param outY: The y-coordinate in the client rectangle.
   */
  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX: The x-coordinate in the client rectangle.
   * \param inY: The y-coordinate in the client rectangle.
   * \param outX: The x-coordinate on the screen.
   * \param outY: The y-coordinate on the screen.
   */
  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * but without the y coordinate conversion needed for ghost compatibility.
   * \param inX: The x-coordinate in the client rectangle.
   * \param inY: The y-coordinate in the client rectangle.
   * \param outX: The x-coordinate on the screen.
   * \param outY: The y-coordinate on the screen.
   */
  void clientToScreenIntern(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates,
   * but without the y coordinate conversion needed for ghost compatibility.
   * \param inX: The x-coordinate in the client rectangle.
   * \param inY: The y-coordinate in the client rectangle.
   * \param outX: The x-coordinate on the screen.
   * \param outY: The y-coordinate on the screen.
   */
  void screenToClientIntern(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const;

  /**
   * Sets the state of the window (normal, minimized, maximized).
   * \param state: The state of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setState(GHOST_TWindowState state) override;

  /**
   * Sets the order of the window (bottom, top).
   * \param order: The order of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setOrder(GHOST_TWindowOrder order) override;

  void loadCursor(bool visible, GHOST_TStandardCursor cursor) const;

  bool isDialog() const override;

  /**
   * Sets the progress bar value displayed in the window/application icon
   * \param progress: The progress percentage (0.0 to 1.0).
   */
  GHOST_TSuccess setProgressBar(float progress);

  /**
   * Hides the progress bar icon
   */
  GHOST_TSuccess endProgressBar();

  void setNativePixelSize(void);

  GHOST_TSuccess beginFullScreen() const
  {
    return GHOST_kFailure;
  }

  GHOST_TSuccess endFullScreen() const
  {
    return GHOST_kFailure;
  }

  /** public function to get the window containing the OpenGL view */
  UIView *getUIView() const
  {
    return uiview_;
  };

  /* Internal value to ensure proper redraws during animations */
  void setImmediateDraw(bool value)
  {
    immediate_draw_ = value;
  }
  bool getImmediateDraw(void) const
  {
    return immediate_draw_;
  }

  /**
   * Ghost system to send events to.
   * \return the system used to create this window.
   */
  GHOST_SystemIOS *getSystem() const
  {
    return system_ios_;
  }

  /* Active window controls. We can only present on active windows.  */
  void requestToActivateWindow();
  void requestToDeactivateWindow();
  bool makeKeyWindow();
  void resignKeyWindow();

#ifdef WITH_INPUT_IME
  void beginIME(int32_t x, int32_t y, int32_t w, int32_t h, bool completed);
  void endIME();
#endif /* WITH_INPUT_IME */

 protected:
  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type);

  /**
   * Invalidates the contents of this window.
   * \return Indication of success.
   */
  GHOST_TSuccess invalidate();

  /**
   * Sets the cursor visibility on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorVisibility(bool visible);

  /**
   * Sets the cursor grab on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode);

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape);
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor shape);

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCustomCursorShape(const uint8_t *bitmap,
                                            const uint8_t *mask,
                                            const int size[2],
                                            const int hot_size[2],
                                            bool canInvertColor);

  /** \copydoc #GHOST_IWindow::getDPIHint */
  uint16_t getDPIHint();

  /** The mother SystemCocoa class to send events */
  GHOST_SystemIOS *system_ios_;

  /** The view, either Metal or OpenGL */
  UIViewController *uiview_controller_;
  UIView *uiview_;
  MTKView *metal_view_;

  bool immediate_draw_;
  bool debug_context_;  // for debug messages during context setup
  bool is_dialog_;
  bool is_active_window_;
  bool request_to_make_active_;

  GHOST_WindowIOS *parent_window_;

  char *window_title_;

 public:
  inline UIView *getView()
  {
    return uiview_;
  }
  CGPoint scalePointToWindow(CGPoint &point);

  void beginFrame();
  void endFrame();
  /* The current approach is to issue the swap/present from the main
   * draw loop *only* for the currently active window. This is because trying
   * to issue presents on anything but the MTKView supplied to drawInMTKView
   * can result in issues. We also defer the swap/present until the end of the
   * draw loop because Blender can sometime issue multiple requests and this can
   * make iOS sad if we try and issue more than one per draw-loop.
   * Because every on-screen window (viewport, file loading etc.) is always full
   * screen we only have one active window at a time. If we change to having
   * sub windows then we may need to revisit this.
   */
  void flushDeferredSwapBuffers();
  int deferred_swap_buffers_count;

  /* Keyboard handling */
  GHOST_TSuccess popupOnscreenKeyboard(const GHOST_KeyboardProperties &keyboard_properties);
  GHOST_TSuccess hideOnscreenKeyboard();
  const GHOST_TabletData getTabletData();
  UITextField *getUITextField();
  const char *getLastKeyboardString();
  /* This is the size of the window pre-scaled */
  CGSize getLogicalWindowSize();
  /* This is the size of the window post-scaled */
  CGSize getNativeWindowSize();
  float getWindowScaleFactor();
};

#ifdef WITH_INPUT_IME
class GHOST_EventIME : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param msec: The time this event was generated.
   * \param type: The type of key event.
   * \param key: The key code of the key.
   */
  GHOST_EventIME(uint64_t msec, GHOST_TEventType type, GHOST_IWindow *window, void *customdata)
      : GHOST_Event(msec, type, window)
  {
    this->data_ = customdata;
  }
};

typedef int GHOST_ImeStateFlagCocoa;
enum {
  GHOST_IME_INPUT_FOCUSED = (1 << 0),
  GHOST_IME_ENABLED = (1 << 1),
  GHOST_IME_COMPOSING = (1 << 2),
  GHOST_IME_KEY_CONTROL_CHAR = (1 << 3),
  GHOST_IME_COMPOSITION_EVENT = (1 << 4),  // For Korean input
  GHOST_IME_RESULT_EVENT = (1 << 5)        // For Korean input
};
#endif /* WITH_INPUT_IME */
