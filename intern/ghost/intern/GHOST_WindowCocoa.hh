/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WindowCocoa class.
 */

#pragma once

#ifndef __APPLE__
#  error Apple OSX only!
#endif  // __APPLE__

#include "GHOST_Window.hh"
#ifdef WITH_INPUT_IME
#  include "GHOST_Event.hh"
#endif

@class CAMetalLayer;
@class CocoaMetalView;
@class CocoaOpenGLView;
@class BlenderWindow;
@class NSCursor;
@class NSScreen;

class GHOST_SystemCocoa;

class GHOST_WindowCocoa : public GHOST_Window {
 public:
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
   * \param context_params: Parameters to use when initializing the context.
   * \param preferred_device: Preferred device to use when new device will be created.
   */
  GHOST_WindowCocoa(GHOST_SystemCocoa *systemCocoa,
                    const char *title,
                    int32_t left,
                    int32_t bottom,
                    uint32_t width,
                    uint32_t height,
                    GHOST_TWindowState state,
                    GHOST_TDrawingContextType type,
                    const GHOST_ContextParams &context_params,
                    bool dialog,
                    GHOST_WindowCocoa *parent_window,
                    const GHOST_GPUDevice &preferred_device);

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  ~GHOST_WindowCocoa() override;

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
   * Sets the file name represented by this window.
   * \param filepath: The file directory.
   */
  void setPath(const char *filepath) override;

  /**
   * Apply the window decoration style using the current flags and settings.
   */
  GHOST_TSuccess applyWindowDecorationStyle() override;

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
   * \param is_unsaved_changes: Unsaved changes or not.
   * \return Indication of success.
   */
  GHOST_TSuccess setModifiedState(bool is_unsaved_changes) override;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX: The x-coordinate on the screen.
   * \param inY: The y-coordinate on the screen.
   * \param outX: The x-coordinate in the client rectangle.
   * \param outY: The y-coordinate in the client rectangle.
   */
  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  /**
   * Converts a point in client rectangle coordinates to screen coordinates.
   * \param inX: The x-coordinate in the client rectangle.
   * \param inY: The y-coordinate in the client rectangle.
   * \param outX: The x-coordinate on the screen.
   * \param outY: The y-coordinate on the screen.
   */
  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  /**
   * Converts a point in client rectangle coordinates to screen coordinates.
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
   * \param inX: The x-coordinate on the screen.
   * \param inY: The y-coordinate on the screen.
   * \param outX: The x-coordinate in the client rectangle.
   * \param outY: The y-coordinate in the client rectangle.
   */
  void screenToClientIntern(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const;

  /**
   * Return the screen the window is displayed in.
   * \return The current screen NSScreen object
   */
  NSScreen *getScreen() const;

  /**
   * Return the primary screen, the screen defined as "Main Display" in macOS Settings, source of
   * all screen coordinates.
   * \note This function is placed in WindowCocoa since SystemCocoa cannot include Obj-C types.
   * \return The primary screen NSScreen object
   */
  static NSScreen *getPrimaryScreen();

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

  NSCursor *getStandardCursor(GHOST_TStandardCursor cursor) const;
  void loadCursor(bool visible, GHOST_TStandardCursor cursor) const;

  bool isDialog() const override;

  GHOST_TabletData &GetCocoaTabletData()
  {
    return tablet_;
  }

  /**
   * Sets the progress bar value displayed in the window/application icon
   * \param progress: The progress percentage (0.0 to 1.0).
   */
  GHOST_TSuccess setProgressBar(float progress) override;

  /**
   * Hides the progress bar icon
   */
  GHOST_TSuccess endProgressBar() override;

  void setNativePixelSize();

  /** public function to get the window containing the view */
  BlenderWindow *getViewWindow() const
  {
    return window_;
  };

  /* Internal value to ensure proper redraws during animations */
  void setImmediateDraw(bool value)
  {
    immediate_draw_ = value;
  }
  bool getImmediateDraw() const
  {
    return immediate_draw_;
  }

#ifdef WITH_INPUT_IME
  void beginIME(int32_t x, int32_t y, int32_t w, int32_t h, bool completed) override;
  void endIME() override;
#endif /* WITH_INPUT_IME */

 protected:
  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) override;

  /**
   * Invalidates the contents of this window.
   * \return Indication of success.
   */
  GHOST_TSuccess invalidate() override;

  /**
   * Sets the cursor visibility on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorVisibility(bool visible) override;

  /**
   * Sets the cursor grab on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode) override;

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) override;
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor shape) override;

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCustomCursorShape(const uint8_t *bitmap,
                                            const uint8_t *mask,
                                            const int size[2],
                                            const int hot_spot[2],
                                            bool can_invert_color) override;

  /** The window containing the view */
  BlenderWindow *window_;

  /** The view, either Metal or OpenGL */
  CocoaOpenGLView *opengl_view_;
  CocoaMetalView *metal_view_;
  CAMetalLayer *metal_layer_;

  /** The mother SystemCocoa class to send events */
  GHOST_SystemCocoa *system_cocoa_;

  NSCursor *custom_cursor_;

  GHOST_TabletData tablet_;

  bool immediate_draw_;
  bool is_dialog_;
  GHOST_GPUDevice preferred_device_;
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
  GHOST_EventIME(uint64_t msec,
                 GHOST_TEventType type,
                 GHOST_IWindow *window,
                 const void *customdata)
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
