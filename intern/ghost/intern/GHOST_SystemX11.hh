/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemX11 class.
 */

#pragma once

#include <X11/XKBlib.h> /* Allow detectable auto-repeat. */
#include <X11/Xlib.h>

#include "../GHOST_Types.h"
#include "GHOST_System.hh"

/* For tablets. */
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput.h>

/* Disable XINPUT warp, currently not implemented by Xorg for multi-head display.
 * (see comment in XSERVER `Xi/xiwarppointer.c` -> `FIXME: panoramix stuff is missing` ~ v1.13.4)
 * If this is supported we can add back XINPUT for warping (fixing #48901).
 * For now disable (see #50383). */
// #  define USE_X11_XINPUT_WARP
#endif

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
#  define GHOST_X11_RES_NAME "Blender"  /* res_name */
#  define GHOST_X11_RES_CLASS "Blender" /* res_class */
#endif

/* generic error handlers */
int GHOST_X11_ApplicationErrorHandler(Display *display, XErrorEvent *event);
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
   * this class should only be instantiated by GHOST_ISystem.
   */

  GHOST_SystemX11();

  /**
   * Destructor.
   */
  ~GHOST_SystemX11() override;

  GHOST_TSuccess init() override;

  /**
   * \section Interface Inherited from GHOST_ISystem
   */

  /**
   * Returns the system time.
   * Returns the number of milliseconds since the start of the system process.
   * \return The number of milliseconds.
   */
  uint64_t getMilliSeconds() const override;

  /**
   * Returns the number of displays on this system.
   * \return The number of displays.
   */
  uint8_t getNumDisplays() const override;

  /**
   * Returns the dimensions of the main display on this system.
   * \return The dimension of the main display.
   */
  void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const override;

  /**
   * Returns the dimensions of all displays on this system.
   * \return The dimension of the main display.
   */
  void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const override;

  /**
   * Create a new window.
   * The new window is added to the list of windows managed.
   * Never explicitly delete the window, use #disposeWindow() instead.
   * \param title: The name of the window.
   * (displayed in the title bar of the window if the OS supports it).
   * \param left: The coordinate of the left edge of the window.
   * \param top: The coordinate of the top edge of the window.
   * \param width: The width the window.
   * \param height: The height the window.
   * \param state: The state of the window when opened.
   * \param stereoVisual: Create a stereo visual for quad buffered stereo.
   * \param exclusive: Use to show the window on top and ignore others (used full-screen).
   * \param parentWindow: Parent (embedder) window.
   * \return The new window (or 0 if creation failed).
   */
  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_GPUSettings gpuSettings,
                              const bool exclusive = false,
                              const bool is_dialog = false,
                              const GHOST_IWindow *parentWindow = nullptr) override;

  /**
   * Create a new off-screen context.
   * Never explicitly delete the context, use #disposeContext() instead.
   * \return The new context (or 0 if creation failed).
   */
  GHOST_IContext *createOffscreenContext(GHOST_GPUSettings gpuSettings) override;

  /**
   * Dispose of a context.
   * \param context: Pointer to the context to be disposed.
   * \return Indication of success.
   */
  GHOST_TSuccess disposeContext(GHOST_IContext *context) override;

  /**
   * Retrieves events from the system and stores them in the queue.
   * \param waitForEvent: Flag to wait for an event (or return immediately).
   * \return Indication of the presence of events.
   */
  bool processEvents(bool waitForEvent) override;

  GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const override;

  GHOST_TSuccess setCursorPosition(int32_t x, int32_t y) override;

  /**
   * Returns the state of all modifier keys.
   * \param keys: The state of all modifier keys (true == pressed).
   * \return Indication of success.
   */
  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const override;

  /**
   * Returns the state of the mouse buttons (outside the message queue).
   * \param buttons: The state of the buttons.
   * \return Indication of success.
   */
  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const override;

  GHOST_TCapabilityFlag getCapabilities() const override;

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

  /** Helped function for get data from the clipboard. */
  void getClipboard_xcout(const XEvent *evt,
                          Atom sel,
                          Atom target,
                          unsigned char **txt,
                          unsigned long *len,
                          unsigned int *context) const;

  /**
   * Returns unsigned char from CUT_BUFFER0
   * \param selection: Get selection, X11 only feature.
   * \return Returns the Clipboard indicated by Flag.
   */
  char *getClipboard(bool selection) const override;

  /**
   * Puts buffer to system clipboard
   * \param buffer: The buffer to copy to the clipboard.
   * \param selection: Set the selection into the clipboard, X11 only feature.
   */
  void putClipboard(const char *buffer, bool selection) const override;

  /**
   * Show a system message box
   * \param title: The title of the message box.
   * \param message: The message to display.
   * \param help_label: Help button label.
   * \param continue_label: Continue button label.
   * \param link: An optional hyperlink.
   * \param dialog_options: Options  how to display the message.
   */
  GHOST_TSuccess showMessageBox(const char *title,
                                const char *message,
                                const char *help_label,
                                const char *continue_label,
                                const char *link,
                                GHOST_DialogOptions dialog_options) const override;
#ifdef WITH_XDND
  /**
   * Creates a drag'n'drop event and pushes it immediately onto the event queue.
   * Called by GHOST_DropTargetX11 class.
   * \param eventType: The type of drag'n'drop event.
   * \param draggedObjectType: The type object concerned.
   * (currently array of file names, string, ?bitmap)
   * \param mouseX: x mouse coordinate (in window coordinates).
   * \param mouseY: y mouse coordinate.
   * \param window: The window on which the event occurred.
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
  bool setConsoleWindowState(GHOST_TConsoleWindowState /*action*/) override
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

  /** Use for scan-code look-ups. */
  XkbDescRec *m_xkb_descr;

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  XIM m_xim;
#endif

#ifdef WITH_X11_XINPUT
  /* Tablet devices */
  std::vector<GHOST_TabletX11> m_xtablets;
#endif

  /** The vector of windows that need to be updated. */
  std::vector<GHOST_WindowX11 *> m_dirty_windows;

  /** Start time at initialization. */
  uint64_t m_start_time;

  /** A vector of keyboard key masks. */
  char m_keyboard_vector[32];

  /**
   * To prevent multiple warp, we store the time of the last warp event
   * and stop accumulating all events generated before that.
   */
  Time m_last_warp_x;
  Time m_last_warp_y;

  /* Detect auto-repeat glitch. */
  unsigned int m_last_release_keycode;
  Time m_last_release_time;

  uint m_keycode_last_repeat_key;

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
