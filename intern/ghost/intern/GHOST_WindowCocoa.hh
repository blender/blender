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
@class CocoaWindow;
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
   * \param stereoVisual: Stereo visual for quad buffered stereo.
   */
  GHOST_WindowCocoa(GHOST_SystemCocoa *systemCocoa,
                    const char *title,
                    int32_t left,
                    int32_t bottom,
                    uint32_t width,
                    uint32_t height,
                    GHOST_TWindowState state,
                    GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
                    const bool stereoVisual = false,
                    bool is_debug = false,
                    bool dialog = false,
                    GHOST_WindowCocoa *parentWindow = 0);

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  ~GHOST_WindowCocoa();

  /**
   * Returns indication as to whether the window is valid.
   * \return The validity of the window.
   */
  bool getValid() const;

  /**
   * Returns the associated NSWindow object
   * \return The associated NSWindow object
   */
  void *getOSWindow() const;

  /**
   * Sets the title displayed in the title bar.
   * \param title: The title to display in the title bar.
   */
  void setTitle(const char *title);
  /**
   * Returns the title displayed in the title bar.
   * \param title: The title displayed in the title bar.
   */
  std::string getTitle() const;

  /**
   * Returns the window rectangle dimensions.
   * The dimensions are given in screen coordinates that are
   * relative to the upper-left corner of the screen.
   * \param bounds: The bounding rectangle of the window.
   */
  void getWindowBounds(GHOST_Rect &bounds) const;

  /**
   * Returns the client rectangle dimensions.
   * The left and top members of the rectangle are always zero.
   * \param bounds: The bounding rectangle of the client area of the window.
   */
  void getClientBounds(GHOST_Rect &bounds) const;

  /**
   * Resizes client rectangle width.
   * \param width: The new width of the client area of the window.
   */
  GHOST_TSuccess setClientWidth(uint32_t width);

  /**
   * Resizes client rectangle height.
   * \param height: The new height of the client area of the window.
   */
  GHOST_TSuccess setClientHeight(uint32_t height);

  /**
   * Resizes client rectangle.
   * \param width: The new width of the client area of the window.
   * \param height: The new height of the client area of the window.
   */
  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height);

  /**
   * Returns the state of the window (normal, minimized, maximized).
   * \return The state of the window.
   */
  GHOST_TWindowState getState() const;

  /**
   * Sets the window "modified" status, indicating unsaved changes
   * \param isUnsavedChanges: Unsaved changes or not.
   * \return Indication of success.
   */
  GHOST_TSuccess setModifiedState(bool isUnsavedChanges);

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX: The x-coordinate on the screen.
   * \param inY: The y-coordinate on the screen.
   * \param outX: The x-coordinate in the client rectangle.
   * \param outY: The y-coordinate in the client rectangle.
   */
  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const;

  /**
   * Converts a point in client rectangle coordinates to screen coordinates.
   * \param inX: The x-coordinate in the client rectangle.
   * \param inY: The y-coordinate in the client rectangle.
   * \param outX: The x-coordinate on the screen.
   * \param outY: The y-coordinate on the screen.
   */
  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const;

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
   * Gets the screen the window is displayed in
   * \return The NSScreen object
   */
  NSScreen *getScreen();

  /**
   * Sets the state of the window (normal, minimized, maximized).
   * \param state: The state of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setState(GHOST_TWindowState state);

  /**
   * Sets the order of the window (bottom, top).
   * \param order: The order of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setOrder(GHOST_TWindowOrder order);

  NSCursor *getStandardCursor(GHOST_TStandardCursor cursor) const;
  void loadCursor(bool visible, GHOST_TStandardCursor cursor) const;

  bool isDialog() const;

  GHOST_TabletData &GetCocoaTabletData()
  {
    return m_tablet;
  }

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
  CocoaWindow *getCocoaWindow() const
  {
    return m_window;
  };

  /* Internal value to ensure proper redraws during animations */
  void setImmediateDraw(bool value)
  {
    m_immediateDraw = value;
  }
  bool getImmediateDraw(void) const
  {
    return m_immediateDraw;
  }

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
  GHOST_TSuccess setWindowCustomCursorShape(uint8_t *bitmap,
                                            uint8_t *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor);

  /** The window containing the view */
  CocoaWindow *m_window;

  /** The view, either Metal or OpenGL */
  CocoaOpenGLView *m_openGLView;
  CocoaMetalView *m_metalView;
  CAMetalLayer *m_metalLayer;

  /** The mother SystemCocoa class to send events */
  GHOST_SystemCocoa *m_systemCocoa;

  NSCursor *m_customCursor;

  GHOST_TabletData m_tablet;

  bool m_immediateDraw;
  bool m_debug_context;  // for debug messages during context setup
  bool m_is_dialog;
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
    this->m_data = customdata;
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
