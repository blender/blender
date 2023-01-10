/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * %Main interface file for C++ Api with declaration of GHOST_ISystem interface
 * class.
 * Contains the doxygen documentation main page.
 */

#pragma once

#include <stdlib.h>

#include "GHOST_IContext.h"
#include "GHOST_ITimerTask.h"
#include "GHOST_IWindow.h"
#include "GHOST_Types.h"

class GHOST_IEventConsumer;

/**
 * \page GHOSTPage GHOST
 *
 * \section intro Introduction
 *
 * GHOST is yet another acronym. It stands for "Generic Handy Operating System
 * Toolkit". It has been created to replace the OpenGL utility tool kit
 * <a href="http://www.opengl.org/resources/libraries/glut/">GLUT</a>.
 * GLUT was used in <a href="http://www.blender3d.com">Blender</a> until the
 * point that Blender needed to be ported to Apple's Mac OSX. Blender needed a
 * number of modifications in GLUT to work but the GLUT sources for OSX were
 * unavailable at the time. The decision was made to build our own replacement
 * for GLUT. In those days, NaN Technologies BV was the company that developed
 * Blender.
 * <br><br>
 * Enough history. What does GHOST have to offer?<br>
 * In short: everything that Blender needed from GLUT to run on all its supported
 * operating systems and some extra's.
 * This includes :
 *
 * - Time(r) management.
 * - Display/window management (windows are only created on the main display).
 * - Event management.
 * - Cursor shape management (no custom cursors for now).
 * - Access to the state of the mouse buttons and the keyboard.
 * - Menus for windows with events generated when they are accessed (this is
 *   work in progress).
 * - Video mode switching.
 * - Copy/Paste buffers.
 * - System paths.
 *
 * Font management has been moved to a separate library.
 *
 * \section platforms Platforms
 *
 * GHOST supports the following platforms:
 *
 * - OSX Cocoa.
 * - Windows.
 * - X11.
 * - SDL2 (experimental).
 * - NULL (headless mode).
 *
 * \section Building GHOST
 *
 * GHOST is not build standalone however there are tests in intern/ghost/test
 *
 * \section interface Interface
 * GHOST has two programming interfaces:
 *
 * - The C-API. For programs written in C.
 * - The C++-API. For programs written in C++.
 *
 * GHOST itself is written in C++ and the C-API is a wrapper around the C++
 * API.
 *
 * \subsection cplusplus_api The C++ API consists of the following files:
 *
 * - GHOST_IEvent.h
 * - GHOST_IEventConsumer.h
 * - GHOST_ISystem.h
 * - GHOST_ITimerTask.h
 * - GHOST_IWindow.h
 * - GHOST_Rect.h
 * - GHOST_Types.h
 *
 * For an example of using the C++-API, have a look at the GHOST_C-Test.cpp
 * program in the ?/ghost/test/gears/ directory.
 *
 * \subsection c_api The C-API
 * To use GHOST in programs written in C, include the file GHOST_C-API.h in
 * your program. This file includes the GHOST_Types.h file for all GHOST types
 * and defines functions that give you access to the same functionality present
 * in the C++ API.<br>
 * For an example of using the C-API, have a look at the GHOST_C-Test.c program
 * in the ?/ghost/test/gears/ directory.
 *
 * \section work Work in progress
 * \todo write WIP section
 */

/** \interface GHOST_ISystem
 * Interface for classes that provide access to the operating system.
 * There should be only one system class in an application.
 * Therefore, the routines to create and dispose the system are static.
 * Provides:
 *  -# Time(r) management.
 *  -# Display/window management (windows are only created on the main display).
 *  -# Event management.
 *  -# Cursor shape management (no custom cursors for now).
 *  -# Access to the state of the mouse buttons and the keyboard.
 *  -# Menus for windows with events generated when they are accessed (this is
 *     work in progress).
 */
class GHOST_ISystem {
 public:
  /**
   * Creates the one and only system.
   * \param verbose: report back-ends that were attempted no back-end could be loaded.
   * \param background: loading the system for background rendering (no visible windows).
   * \return An indication of success.
   */

  static GHOST_TSuccess createSystem(bool verbose, bool background);
  static GHOST_TSuccess createSystemBackground();

  /**
   * Disposes the one and only system.
   * \return An indication of success.
   */
  static GHOST_TSuccess disposeSystem();

  /**
   * Returns a pointer to the one and only system (nil if it hasn't been created).
   * \return A pointer to the system.
   */
  static GHOST_ISystem *getSystem();
  /**
   * Return an identifier for the one and only system.
   * \warning while it may be tempting this should never be used to check for supported features,
   * in that case, the GHOST API should be extended to query capabilities.
   * This is needed for X11/WAYLAND on Unix, without this - there is no convenient way for users to
   * check if WAYLAND or XWAYLAND are in use since they are dynamically selected at startup.
   * When dynamically switching between X11/WAYLAND is removed, this function can go too.
   */
  static const char *getSystemBackend();

  static GHOST_TBacktraceFn getBacktraceFn();
  static void setBacktraceFn(GHOST_TBacktraceFn backtrace_fn);

 protected:
  /**
   * Constructor.
   * Protected default constructor to force use of static createSystem member.
   */
  GHOST_ISystem()
  {
  }

  /**
   * Destructor.
   * Protected default constructor to force use of static dispose member.
   */
  virtual ~GHOST_ISystem()
  {
  }

 public:
  /***************************************************************************************
   * Time(r) functionality
   ***************************************************************************************/

  /**
   * Returns the system time.
   * Returns the number of milliseconds since the start of the system process.
   * Based on ANSI clock() routine.
   * \return The number of milliseconds.
   */
  virtual uint64_t getMilliSeconds() const = 0;

  /**
   * Installs a timer.
   * Note that, on most operating systems, messages need to be processed in order
   * for the timer callbacks to be invoked.
   * \param delay: The time to wait for the first call to the timerProc (in milliseconds).
   * \param interval: The interval between calls to the timerProc (in milliseconds).
   * \param timerProc: The callback invoked when the interval expires.
   * \param userData: Placeholder for user data.
   * \return A timer task (0 if timer task installation failed).
   */
  virtual GHOST_ITimerTask *installTimer(uint64_t delay,
                                         uint64_t interval,
                                         GHOST_TimerProcPtr timerProc,
                                         GHOST_TUserDataPtr userData = NULL) = 0;

  /**
   * Removes a timer.
   * \param timerTask: Timer task to be removed.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess removeTimer(GHOST_ITimerTask *timerTask) = 0;

  /***************************************************************************************
   * Display/window management functionality
   ***************************************************************************************/

  /**
   * Returns the number of displays on this system.
   * \return The number of displays.
   */
  virtual uint8_t getNumDisplays() const = 0;

  /**
   * Returns the dimensions of the main display on this system.
   * \return The dimension of the main display.
   */
  virtual void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const = 0;

  /**
   * Returns the combine dimensions of all monitors.
   * \return The dimension of the workspace.
   */
  virtual void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const = 0;

  /**
   * Create a new window.
   * The new window is added to the list of windows managed.
   * Never explicitly delete the window, use disposeWindow() instead.
   * \param title: The name of the window
   * (displayed in the title bar of the window if the OS supports it).
   * \param left: The coordinate of the left edge of the window.
   * \param top: The coordinate of the top edge of the window.
   * \param width: The width the window.
   * \param height: The height the window.
   * \param state: The state of the window when opened.
   * \param glSettings: Misc OpenGL settings.
   * \param exclusive: Use to show the window on top and ignore others (used full-screen).
   * \param is_dialog: Stay on top of parent window, no icon in taskbar, can't be minimized.
   * \param parentWindow: Parent (embedder) window
   * \return The new window (or 0 if creation failed).
   */
  virtual GHOST_IWindow *createWindow(const char *title,
                                      int32_t left,
                                      int32_t top,
                                      uint32_t width,
                                      uint32_t height,
                                      GHOST_TWindowState state,
                                      GHOST_GLSettings glSettings,
                                      const bool exclusive = false,
                                      const bool is_dialog = false,
                                      const GHOST_IWindow *parentWindow = NULL) = 0;

  /**
   * Dispose a window.
   * \param window: Pointer to the window to be disposed.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess disposeWindow(GHOST_IWindow *window) = 0;

  /**
   * Create a new off-screen context.
   * Never explicitly delete the context, use #disposeContext() instead.
   * \return The new context (or 0 if creation failed).
   */
  virtual GHOST_IContext *createOffscreenContext(GHOST_GLSettings glSettings) = 0;

  /**
   * Dispose of a context.
   * \param context: Pointer to the context to be disposed.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess disposeContext(GHOST_IContext *context) = 0;

  /**
   * Returns whether a window is valid.
   * \param window: Pointer to the window to be checked.
   * \return Indication of validity.
   */
  virtual bool validWindow(GHOST_IWindow *window) = 0;

  /**
   * Begins full screen mode.
   * \param setting: The new setting of the display.
   * \param window: Window displayed in full screen.
   *                  This window is invalid after full screen has been ended.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess beginFullScreen(const GHOST_DisplaySetting &setting,
                                         GHOST_IWindow **window,
                                         const bool stereoVisual) = 0;

  /**
   * Updates the resolution while in full-screen mode.
   * \param setting: The new setting of the display.
   * \param window: Window displayed in full screen.
   *
   * \return Indication of success.
   */
  virtual GHOST_TSuccess updateFullScreen(const GHOST_DisplaySetting &setting,
                                          GHOST_IWindow **window) = 0;

  /**
   * Ends full screen mode.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess endFullScreen(void) = 0;

  /**
   * Returns current full screen mode status.
   * \return The current status.
   */
  virtual bool getFullScreen(void) = 0;

  /**
   * Native pixel size support (MacBook 'retina').
   */
  virtual bool useNativePixel(void) = 0;

  /**
   * Return true when warping the cursor is supported.
   */
  virtual bool supportsCursorWarp() = 0;

  /**
   * Return true getting/setting the window position is supported.
   */
  virtual bool supportsWindowPosition() = 0;

  /**
   * Focus window after opening, or put them in the background.
   */
  virtual void useWindowFocus(const bool use_focus) = 0;

  /**
   * Get the Window under the cursor.
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return The window under the cursor or nullptr if none.
   */
  virtual GHOST_IWindow *getWindowUnderCursor(int32_t x, int32_t y) = 0;

  /***************************************************************************************
   * Event management functionality
   ***************************************************************************************/

  /**
   * Retrieves events from the system and stores them in the queue.
   * \param waitForEvent: Flag to wait for an event (or return immediately).
   * \return Indication of the presence of events.
   */
  virtual bool processEvents(bool waitForEvent) = 0;

  /**
   * Retrieves events from the queue and send them to the event consumers.
   */
  virtual void dispatchEvents() = 0;

  /**
   * Adds the given event consumer to our list.
   * \param consumer: The event consumer to add.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess addEventConsumer(GHOST_IEventConsumer *consumer) = 0;

  /**
   * Removes the given event consumer to our list.
   * \param consumer: The event consumer to remove.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess removeEventConsumer(GHOST_IEventConsumer *consumer) = 0;

  /***************************************************************************************
   * Cursor management functionality
   ***************************************************************************************/

  /**
   * Returns the current location of the cursor (location in window coordinates)
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getCursorPositionClientRelative(const GHOST_IWindow *window,
                                                         int32_t &x,
                                                         int32_t &y) const = 0;
  /**
   * Updates the location of the cursor (location in window coordinates).
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCursorPositionClientRelative(GHOST_IWindow *window,
                                                         int32_t x,
                                                         int32_t y) = 0;

  /**
   * Returns the current location of the cursor (location in screen coordinates)
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const = 0;

  /**
   * Updates the location of the cursor (location in screen coordinates).
   * Not all operating systems allow the cursor to be moved (without the input device being moved).
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCursorPosition(int32_t x, int32_t y) = 0;

  /***************************************************************************************
   * Access to mouse button and keyboard states.
   ***************************************************************************************/

  /**
   * Returns the state of a modifier key (outside the message queue).
   * \param mask: The modifier key state to retrieve.
   * \param isDown: The state of a modifier key (true == pressed).
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getModifierKeyState(GHOST_TModifierKey mask, bool &isDown) const = 0;

  /**
   * Returns the state of a mouse button (outside the message queue).
   * \param mask: The button state to retrieve.
   * \param isDown: Button state.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getButtonState(GHOST_TButton mask, bool &isDown) const = 0;

  /**
   * Enable multi-touch gestures if supported.
   * \param use: Enable or disable.
   */
  virtual void setMultitouchGestures(const bool use) = 0;

  /**
   * Set which tablet API to use. Only affects Windows, other platforms have a single API.
   * \param api: Enum indicating which API to use.
   */
  virtual void setTabletAPI(GHOST_TTabletAPI api) = 0;

#ifdef WITH_INPUT_NDOF
  /**
   * Sets 3D mouse deadzone
   * \param deadzone: Dead-zone of the 3D mouse (both for rotation and pan) relative to full range
   */
  virtual void setNDOFDeadZone(float deadzone) = 0;
#endif

  /**
   * Set the Console State
   * \param action: console state
   * \return current status (true: visible, 0: hidden)
   */
  virtual bool setConsoleWindowState(GHOST_TConsoleWindowState action) = 0;

  /***************************************************************************************
   * Access to clipboard.
   ***************************************************************************************/

  /**
   * Returns the selection buffer
   * \return "unsigned char" from X11 XA_CUT_BUFFER0 buffer
   *
   */
  virtual char *getClipboard(bool selection) const = 0;

  /**
   * Put data to the Clipboard
   */
  virtual void putClipboard(const char *buffer, bool selection) const = 0;

  /***************************************************************************************
   * System Message Box.
   ***************************************************************************************/

  /**
   * Show a system message box
   *
   * \param title: The title of the message box.
   * \param message: The message to display.
   * \param help_label: Help button label.
   * \param continue_label: Continue button label.
   * \param link: An optional hyperlink.
   * \param dialog_options: Options  how to display the message.
   */
  virtual GHOST_TSuccess showMessageBox(const char * /*title*/,
                                        const char * /*message*/,
                                        const char * /*help_label*/,
                                        const char * /*continue_label*/,
                                        const char * /*link*/,
                                        GHOST_DialogOptions /*dialog_options*/) const = 0;

  /***************************************************************************************
   * Debugging
   ***************************************************************************************/

  /**
   * Specify whether debug messages are to be shown.
   * \param debug: Flag for systems to debug.
   */
  virtual void initDebug(GHOST_Debug debug) = 0;

  /**
   * Check whether debug messages are to be shown.
   */
  virtual bool isDebugEnabled() = 0;

 protected:
  /**
   * Initialize the system.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess init() = 0;

  /**
   * Shut the system down.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess exit() = 0;

  /** The one and only system */
  static GHOST_ISystem *m_system;
  static const char *m_system_backend_id;

  /** Function to call that sets the back-trace. */
  static GHOST_TBacktraceFn m_backtrace_fn;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_ISystem")
#endif
};
