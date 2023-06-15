/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WindowX11 class.
 */

#pragma once

#include "GHOST_Window.hh"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
/* For tablets. */
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput.h>
#endif

#include "GHOST_TaskbarX11.hh"

#include <map>

class GHOST_SystemX11;

#ifdef WITH_XDND
class GHOST_DropTargetX11;
#endif

/**
 * X11 implementation of GHOST_IWindow.
 * Dimensions are given in screen coordinates that are
 * relative to the upper-left corner of the screen.
 */

class GHOST_WindowX11 : public GHOST_Window {
 public:
  /**
   * Constructor.
   * Creates a new window and opens it.
   * To check if the window was created properly, use the getValid() method.
   * \param title: The text shown in the title bar of the window.
   * \param left: The coordinate of the left edge of the window.
   * \param top: The coordinate of the top edge of the window.
   * \param width: The width the window.
   * \param height: The height the window.
   * \param state: The state the window is initially opened with.
   * \param parentWindow: Parent (embedder) window.
   * \param type: The type of drawing context installed in this window.
   * \param stereoVisual: Stereo visual for quad buffered stereo.
   */
  GHOST_WindowX11(GHOST_SystemX11 *system,
                  Display *display,
                  const char *title,
                  int32_t left,
                  int32_t top,
                  uint32_t width,
                  uint32_t height,
                  GHOST_TWindowState state,
                  GHOST_WindowX11 *parentWindow,
                  GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
                  const bool is_dialog = false,
                  const bool stereoVisual = false,
                  const bool exclusive = false,
                  const bool is_debug = false);

  bool getValid() const override;

  void setTitle(const char *title) override;

  std::string getTitle() const override;

  void getWindowBounds(GHOST_Rect &bounds) const override;

  void getClientBounds(GHOST_Rect &bounds) const override;

  bool isDialog() const override;

  GHOST_TSuccess setClientWidth(uint32_t width) override;

  GHOST_TSuccess setClientHeight(uint32_t height) override;

  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height) override;

  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  GHOST_TWindowState getState() const override;

  GHOST_TSuccess setState(GHOST_TWindowState state) override;

  GHOST_TSuccess setOrder(GHOST_TWindowOrder order) override;

  GHOST_TSuccess invalidate() override;

  GHOST_TSuccess setProgressBar(float progress) override;
  GHOST_TSuccess endProgressBar() override;

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  ~GHOST_WindowX11() override;

  /**
   * \section x11specific X11 system specific calls
   */

  /**
   * The reverse of invalidate! Tells this window
   * that all events for it have been pushed into
   * the GHOST event queue.
   */

  void validate();

  /**
   * Return a handle to the x11 window type.
   */
  Window getXWindow();

  GHOST_TabletData &GetTabletData()
  {
    return m_tabletData;
  }

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  XIC getX11_XIC()
  {
    return m_xic;
  }

  bool createX11_XIC();
#endif

#ifdef WITH_X11_XINPUT
  void refreshXInputDevices();
#endif

#ifdef WITH_XDND
  GHOST_DropTargetX11 *getDropTarget()
  {
    return m_dropTarget;
  }
#endif

  /*
   * Need this in case that we want start the window
   * in FullScree or Maximized state.
   * Check GHOST_WindowX11.cc
   */
  bool m_post_init;
  GHOST_TWindowState m_post_state;

  GHOST_TSuccess beginFullScreen() const override;

  GHOST_TSuccess endFullScreen() const override;

  GHOST_TSuccess setDialogHints(GHOST_WindowX11 *parentWindow);

  uint16_t getDPIHint() override;

 protected:
  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) override;

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

  GHOST_TGrabCursorMode getWindowCursorGrab() const;

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) override;
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor shape) override;

  /**
   * Sets the cursor shape on the window using
   * native window system calls (Arbitrary size/color).
   */
  GHOST_TSuccess setWindowCustomCursorShape(uint8_t *bitmap,
                                            uint8_t *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor) override;

 private:
  /* Force use of public constructor. */

  GHOST_WindowX11();

  GHOST_WindowX11(const GHOST_WindowX11 &);

  GHOST_TSuccess getStandardCursor(GHOST_TStandardCursor g_cursor, Cursor &xcursor);

  Cursor getEmptyCursor();

  Window m_window;
  Display *m_display;
  XVisualInfo *m_visualInfo;
  void *m_fbconfig;

  GHOST_TWindowState m_normal_state;

  /** A pointer to the typed system class. */
  GHOST_SystemX11 *m_system;

  /** Used to concatenate calls to invalidate() on this window. */
  bool m_invalid_window;

  /** XCursor structure of an empty (blank) cursor */
  Cursor m_empty_cursor;

  /** XCursor structure of the custom cursor */
  Cursor m_custom_cursor;

  /** XCursor to show when cursor is visible */
  Cursor m_visible_cursor;

  /** Cache of XC_* ID's to XCursor structures */
  std::map<uint, Cursor> m_standard_cursors;

  GHOST_TaskBarX11 m_taskbar;

#ifdef WITH_XDND
  GHOST_DropTargetX11 *m_dropTarget;
#endif

  GHOST_TabletData m_tabletData;

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  XIC m_xic;
#endif

  bool m_valid_setup;
  bool m_is_debug_context;

  void icccmSetState(int state);
  int icccmGetState() const;

  void netwmMaximized(bool set);
  bool netwmIsMaximized() const;

  void netwmFullScreen(bool set);
  bool netwmIsFullScreen() const;

  void motifFullScreen(bool set);
  bool motifIsFullScreen() const;
};
