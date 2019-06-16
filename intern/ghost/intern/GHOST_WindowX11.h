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
 * Declaration of GHOST_WindowX11 class.
 */

#ifndef __GHOST_WINDOWX11_H__
#define __GHOST_WINDOWX11_H__

#include "GHOST_Window.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
// For tablets
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput.h>
#endif

#include "GHOST_TaskbarX11.h"

#include <map>

class STR_String;
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
   * \param title     The text shown in the title bar of the window.
   * \param left      The coordinate of the left edge of the window.
   * \param top       The coordinate of the top edge of the window.
   * \param width     The width the window.
   * \param height    The height the window.
   * \param state     The state the window is initially opened with.
   * \param parentWindow  Parent (embedder) window
   * \param type      The type of drawing context installed in this window.
   * \param stereoVisual  Stereo visual for quad buffered stereo.
   * \param alphaBackground Enable alpha blending of window with display background
   */
  GHOST_WindowX11(GHOST_SystemX11 *system,
                  Display *display,
                  const STR_String &title,
                  GHOST_TInt32 left,
                  GHOST_TInt32 top,
                  GHOST_TUns32 width,
                  GHOST_TUns32 height,
                  GHOST_TWindowState state,
                  const GHOST_TEmbedderWindowID parentWindow,
                  GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
                  const bool stereoVisual = false,
                  const bool exclusive = false,
                  const bool alphaBackground = false,
                  const bool is_debug = false);

  bool getValid() const;

  void setTitle(const STR_String &title);

  void getTitle(STR_String &title) const;

  void getWindowBounds(GHOST_Rect &bounds) const;

  void getClientBounds(GHOST_Rect &bounds) const;

  GHOST_TSuccess setClientWidth(GHOST_TUns32 width);

  GHOST_TSuccess setClientHeight(GHOST_TUns32 height);

  GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height);

  void screenToClient(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const;

  void clientToScreen(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const;

  GHOST_TWindowState getState() const;

  GHOST_TSuccess setState(GHOST_TWindowState state);

  GHOST_TSuccess setOrder(GHOST_TWindowOrder order);

  GHOST_TSuccess invalidate();

  GHOST_TSuccess setProgressBar(float progress);
  GHOST_TSuccess endProgressBar();

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  ~GHOST_WindowX11();

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
#ifdef WITH_X11_XINPUT
  GHOST_TabletData *GetTabletData()
  {
    return &m_tabletData;
  }
#else   // WITH_X11_XINPUT
  const GHOST_TabletData *GetTabletData()
  {
    return NULL;
  }
#endif  // WITH_X11_XINPUT

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
   * Check GHOST_WindowX11.cpp
   */
  bool m_post_init;
  GHOST_TWindowState m_post_state;

  GHOST_TSuccess beginFullScreen() const;

  GHOST_TSuccess endFullScreen() const;

  GHOST_TUns16 getDPIHint();

 protected:
  /**
   * \param type  The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type);

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

  GHOST_TGrabCursorMode getWindowCursorGrab() const;

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape);

  /**
   * Sets the cursor shape on the window using
   * native window system calls (Arbitrary size/color).
   */
  GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                            GHOST_TUns8 *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor);

 private:
  /// Force use of public constructor.

  GHOST_WindowX11();

  GHOST_WindowX11(const GHOST_WindowX11 &);

  Cursor getStandardCursor(GHOST_TStandardCursor g_cursor);

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
  std::map<unsigned int, Cursor> m_standard_cursors;

  GHOST_TaskBarX11 m_taskbar;

#ifdef WITH_XDND
  GHOST_DropTargetX11 *m_dropTarget;
#endif

#ifdef WITH_X11_XINPUT
  GHOST_TabletData m_tabletData;
#endif

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

#endif  // __GHOST_WINDOWX11_H__
