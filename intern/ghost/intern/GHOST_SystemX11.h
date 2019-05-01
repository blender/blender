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
 * Declaration of GHOST_SystemX11 class.
 */

#ifndef __GHOST_SYSTEMX11_H__
#define __GHOST_SYSTEMX11_H__

#include <X11/Xlib.h>
#include <X11/XKBlib.h> /* allow detectable autorepeate */

#include "GHOST_System.h"
#include "../GHOST_Types.h"

// For tablets
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput.h>

/* Disable xinput warp, currently not implemented by Xorg for multi-head display.
 * (see comment in xserver "Xi/xiwarppointer.c" -> "FIXME: panoramix stuff is missing" ~ v1.13.4)
 * If this is supported we can add back xinput for warping (fixing T48901).
 * For now disable (see T50383). */
// #  define USE_X11_XINPUT_WARP
#endif

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
#  define GHOST_X11_RES_NAME "Blender"  /* res_name */
#  define GHOST_X11_RES_CLASS "Blender" /* res_class */
#endif

/* generic error handlers */
int GHOST_X11_ApplicationErrorHandler(Display *display, XErrorEvent *theEvent);
int GHOST_X11_ApplicationIOErrorHandler(Display *display);

#define GHOST_X11_ERROR_HANDLERS_OVERRIDE(var) \
  struct { \
    XErrorHandler handler; \
    XIOErrorHandler handler_io; \
  } var = { \
      XSetErrorHandler(GHOST_X11_ApplicationErrorHandler), \
      XSetIOErrorHandler(GHOST_X11_ApplicationIOErrorHandler), \
  }

#define GHOST_X11_ERROR_HANDLERS_RESTORE(var) \
  { \
    (void)XSetErrorHandler(var.handler); \
    (void)XSetIOErrorHandler(var.handler_io); \
  } \
  ((void)0)

class GHOST_WindowX11;

/**
 * X11 Implementation of GHOST_System class.
 * \see GHOST_System.
 */

class GHOST_SystemX11 : public GHOST_System {
 public:
  /**
   * Constructor
   * this class should only be instanciated by GHOST_ISystem.
   */

  GHOST_SystemX11();

  /**
   * Destructor.
   */
  ~GHOST_SystemX11();

  GHOST_TSuccess init();

  /**
   * Informs if the system provides native dialogs (eg. confirm quit)
   */
  virtual bool supportsNativeDialogs(void);

  /**
   * \section Interface Inherited from GHOST_ISystem
   */

  /**
   * Returns the system time.
   * Returns the number of milliseconds since the start of the system process.
   * \return The number of milliseconds.
   */
  GHOST_TUns64 getMilliSeconds() const;

  /**
   * Returns the number of displays on this system.
   * \return The number of displays.
   */
  GHOST_TUns8 getNumDisplays() const;

  /**
   * Returns the dimensions of the main display on this system.
   * \return The dimension of the main display.
   */
  void getMainDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const;

  /**
   * Returns the dimensions of all displays on this system.
   * \return The dimension of the main display.
   */
  void getAllDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const;

  /**
   * Create a new window.
   * The new window is added to the list of windows managed.
   * Never explicitly delete the window, use disposeWindow() instead.
   * \param   title   The name of the window
   * (displayed in the title bar of the window if the OS supports it).
   * \param   left        The coordinate of the left edge of the window.
   * \param   top     The coordinate of the top edge of the window.
   * \param   width       The width the window.
   * \param   height      The height the window.
   * \param   state       The state of the window when opened.
   * \param   type        The type of drawing context installed in this window.
   * \param   stereoVisual    Create a stereo visual for quad buffered stereo.
   * \param   exclusive   Use to show the window ontop and ignore others
   *                      (used fullscreen).
   * \param   parentWindow    Parent (embedder) window
   * \return  The new window (or 0 if creation failed).
   */
  GHOST_IWindow *createWindow(const STR_String &title,
                              GHOST_TInt32 left,
                              GHOST_TInt32 top,
                              GHOST_TUns32 width,
                              GHOST_TUns32 height,
                              GHOST_TWindowState state,
                              GHOST_TDrawingContextType type,
                              GHOST_GLSettings glSettings,
                              const bool exclusive = false,
                              const GHOST_TEmbedderWindowID parentWindow = 0);

  /**
   * Create a new offscreen context.
   * Never explicitly delete the context, use disposeContext() instead.
   * \return  The new context (or 0 if creation failed).
   */
  GHOST_IContext *createOffscreenContext();

  /**
   * Dispose of a context.
   * \param   context Pointer to the context to be disposed.
   * \return  Indication of success.
   */
  GHOST_TSuccess disposeContext(GHOST_IContext *context);

  /**
   * Retrieves events from the system and stores them in the queue.
   * \param waitForEvent Flag to wait for an event (or return immediately).
   * \return Indication of the presence of events.
   */
  bool processEvents(bool waitForEvent);

  GHOST_TSuccess getCursorPosition(GHOST_TInt32 &x, GHOST_TInt32 &y) const;

  GHOST_TSuccess setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y);

  /**
   * Returns the state of all modifier keys.
   * \param keys  The state of all modifier keys (true == pressed).
   * \return      Indication of success.
   */
  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const;

  /**
   * Returns the state of the mouse buttons (ouside the message queue).
   * \param buttons   The state of the buttons.
   * \return          Indication of success.
   */
  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const;

  /**
   * Flag a window as dirty. This will
   * generate a GHOST window update event on a call to processEvents()
   */

  void addDirtyWindow(GHOST_WindowX11 *bad_wind);

  /**
   * return a pointer to the X11 display structure
   */

  Display *getXDisplay()
  {
    return m_display;
  }

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  XIM getX11_XIM()
  {
    return m_xim;
  }
#endif

  /* Helped function for get data from the clipboard. */
  void getClipboard_xcout(const XEvent *evt,
                          Atom sel,
                          Atom target,
                          unsigned char **txt,
                          unsigned long *len,
                          unsigned int *context) const;

  /**
   * Returns unsigned char from CUT_BUFFER0
   * \param selection     Get selection, X11 only feature
   * \return              Returns the Clipboard indicated by Flag
   */
  GHOST_TUns8 *getClipboard(bool selection) const;

  /**
   * Puts buffer to system clipboard
   * \param buffer    The buffer to copy to the clipboard
   * \param selection Set the selection into the clipboard, X11 only feature
   */
  void putClipboard(GHOST_TInt8 *buffer, bool selection) const;

#ifdef WITH_XDND
  /**
   * Creates a drag'n'drop event and pushes it immediately onto the event queue.
   * Called by GHOST_DropTargetX11 class.
   * \param eventType The type of drag'n'drop event
   * \param draggedObjectType The type object concerned
   * (currently array of file names, string, ?bitmap)
   * \param mouseX x mouse coordinate (in window coordinates)
   * \param mouseY y mouse coordinate
   * \param window The window on which the event occurred
   * \return Indication whether the event was handled.
   */
  static GHOST_TSuccess pushDragDropEvent(GHOST_TEventType eventType,
                                          GHOST_TDragnDropTypes draggedObjectType,
                                          GHOST_IWindow *window,
                                          int mouseX,
                                          int mouseY,
                                          void *data);
#endif

  /**
   * \see GHOST_ISystem
   */
  int toggleConsole(int /*action*/)
  {
    return 0;
  }

#ifdef WITH_X11_XINPUT
  typedef struct GHOST_TabletX11 {
    GHOST_TTabletMode mode;
    XDevice *Device;
    XID ID;

    int MotionEvent;
    int ProxInEvent;
    int ProxOutEvent;
    int PressEvent;

    int PressureLevels;
    int XtiltLevels, YtiltLevels;
  } GHOST_TabletX11;

  std::vector<GHOST_TabletX11> &GetXTablets()
  {
    return m_xtablets;
  }
#endif  // WITH_X11_XINPUT

  struct {
    /**
     * Atom used for ICCCM, WM-spec and Motif.
     * We only need get this atom at the start, it's relative
     * to the display not the window and are public for every
     * window that need it.
     */
    Atom WM_STATE;
    Atom WM_CHANGE_STATE;
    Atom _NET_WM_STATE;
    Atom _NET_WM_STATE_MAXIMIZED_HORZ;
    Atom _NET_WM_STATE_MAXIMIZED_VERT;
    Atom _NET_WM_STATE_FULLSCREEN;
    Atom _MOTIF_WM_HINTS;
    Atom WM_TAKE_FOCUS;
    Atom WM_PROTOCOLS;
    Atom WM_DELETE_WINDOW;

    /* Atoms for Selection, copy & paste. */
    Atom TARGETS;
    Atom STRING;
    Atom COMPOUND_TEXT;
    Atom TEXT;
    Atom CLIPBOARD;
    Atom PRIMARY;
    Atom XCLIP_OUT;
    Atom INCR;
    Atom UTF8_STRING;
#ifdef WITH_X11_XINPUT
    Atom TABLET;
#endif
  } m_atom;

#ifdef WITH_X11_XINPUT
  XExtensionVersion m_xinput_version;
#endif

 private:
  Display *m_display;

  /* Use for scancode lookups. */
  XkbDescRec *m_xkb_descr;

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  XIM m_xim;
#endif

#ifdef WITH_X11_XINPUT
  /* Tablet devices */
  std::vector<GHOST_TabletX11> m_xtablets;
#endif

  /// The vector of windows that need to be updated.
  std::vector<GHOST_WindowX11 *> m_dirty_windows;

  /// Start time at initialization.
  GHOST_TUns64 m_start_time;

  /// A vector of keyboard key masks
  char m_keyboard_vector[32];

  /* to prevent multiple warp, we store the time of the last warp event
   * and stop accumulating all events generated before that */
  Time m_last_warp;

  /* detect autorepeat glitch */
  unsigned int m_last_release_keycode;
  Time m_last_release_time;

  /**
   * Return the ghost window associated with the
   * X11 window xwind
   */

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  bool openX11_IM();
#endif

#ifdef WITH_X11_XINPUT
  void clearXInputDevices();
  void refreshXInputDevices();
#endif

  GHOST_WindowX11 *findGhostWindow(Window xwind) const;

  void processEvent(XEvent *xe);

  Time lastEventTime(Time default_time);

  bool generateWindowExposeEvents();
};

#endif
