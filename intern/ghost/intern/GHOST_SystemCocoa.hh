/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemCocoa class.
 */

#pragma once

#ifndef __APPLE__
#  error Apple OSX only!
#endif  // __APPLE__

// #define __CARBONSOUND__

#include "GHOST_System.hh"

class GHOST_EventCursor;
class GHOST_EventKey;
class GHOST_EventWindow;
class GHOST_WindowCocoa;

class GHOST_SystemCocoa : public GHOST_System {
 public:
  /**
   * Constructor.
   */
  GHOST_SystemCocoa();

  /**
   * Destructor.
   */
  ~GHOST_SystemCocoa();

  /***************************************************************************************
   * Time(r) functionality
   ***************************************************************************************/

  /**
   * Returns the system time.
   * Returns the number of milliseconds since the start of the system process.
   * Based on ANSI clock() routine.
   * \return The number of milliseconds.
   */
  uint64_t getMilliSeconds() const;

  /***************************************************************************************
   * Display/window management functionality
   ***************************************************************************************/

  /**
   * Returns the number of displays on this system.
   * \return The number of displays.
   */
  uint8_t getNumDisplays() const;

  /**
   * Returns the dimensions of the main display on this system.
   * \return The dimension of the main display.
   */
  void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const;

  /** Returns the combine dimensions of all monitors.
   * \return The dimension of the workspace.
   */
  void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const;

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
   * \param gpuSettings: Misc GPU settings.
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
                              const GHOST_IWindow *parentWindow = nullptr);

  /**
   * Create a new off-screen context.
   * Never explicitly delete the context, use #disposeContext() instead.
   * \return The new context (or 0 if creation failed).
   */
  GHOST_IContext *createOffscreenContext(GHOST_GPUSettings gpuSettings);

  /**
   * Dispose of a context.
   * \param context: Pointer to the context to be disposed.
   * \return Indication of success.
   */
  GHOST_TSuccess disposeContext(GHOST_IContext *context);

  /**
   * Get the Window under the cursor.
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return The window under the cursor or nullptr if none.
   */
  GHOST_IWindow *getWindowUnderCursor(int32_t x, int32_t y);

  /***************************************************************************************
   * Event management functionality
   ***************************************************************************************/

  /**
   * Gets events from the system and stores them in the queue.
   * \param waitForEvent: Flag to wait for an event (or return immediately).
   * \return Indication of the presence of events.
   */
  bool processEvents(bool waitForEvent);

  /**
   * Handle User request to quit, from Menu bar Quit, and Command+Q
   * Display alert panel if changes performed since last save
   */
  void handleQuitRequest();

  /**
   * Handle Cocoa openFile event
   * Display confirmation request panel if changes performed since last save
   */
  bool handleOpenDocumentRequest(void *filepathStr);

  /**
   * Handles a drag & drop destination event. Called by GHOST_WindowCocoa window subclass.
   * \param eventType: The type of drag & drop event.
   * \param draggedObjectType: The type object concerned.
   * (currently array of file names, string, TIFF image).
   * \param mouseX: x mouse coordinate (in cocoa base window coordinates).
   * \param mouseY: y mouse coordinate.
   * \param window: The window on which the event occurred.
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleDraggingEvent(GHOST_TEventType eventType,
                                     GHOST_TDragnDropTypes draggedObjectType,
                                     GHOST_WindowCocoa *window,
                                     int mouseX,
                                     int mouseY,
                                     void *data);

  /***************************************************************************************
   * Cursor management functionality
   ***************************************************************************************/

  /**
   * Returns the current location of the cursor (location in screen coordinates)
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return Indication of success.
   */
  GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const;

  /**
   * Updates the location of the cursor (location in screen coordinates).
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return Indication of success.
   */
  GHOST_TSuccess setCursorPosition(int32_t x, int32_t y);

  /**
   * Get the color of the pixel at the current mouse cursor location
   * \param r_color: returned sRGB float colors
   * \return Success value (true == successful and supported by platform)
   */
  GHOST_TSuccess getPixelAtCursor(float r_color[3]) const;

  /***************************************************************************************
   * Access to mouse button and keyboard states.
   ***************************************************************************************/

  /**
   * Returns the state of all modifier keys.
   * \param keys: The state of all modifier keys (true == pressed).
   * \return Indication of success.
   */
  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const;

  /**
   * Returns the state of the mouse buttons (outside the message queue).
   * \param buttons: The state of the buttons.
   * \return Indication of success.
   */
  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const;

  GHOST_TCapabilityFlag getCapabilities() const;

  /**
   * Returns Clipboard data
   * \param selection: Indicate which buffer to return.
   * \return Returns the selected buffer
   */
  char *getClipboard(bool selection) const;

  /**
   * Puts buffer to system clipboard
   * \param buffer: The buffer to be copied.
   * \param selection: Indicates which buffer to copy too, only used on X11.
   */
  void putClipboard(const char *buffer, bool selection) const;

  /**
   * Handles a window event. Called by GHOST_WindowCocoa window delegate
   * \param eventType: The type of window event.
   * \param window: The window on which the event occurred.
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleWindowEvent(GHOST_TEventType eventType, GHOST_WindowCocoa *window);

  /**
   * Handles the Cocoa event telling the application has become active (again)
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleApplicationBecomeActiveEvent();

  /**
   * \return True if any dialog window is open.
   */
  bool hasDialogWindow();

  /**
   * External objects should call this when they send an event outside processEvents.
   */
  void notifyExternalEventProcessed();

  /**
   * \see GHOST_ISystem
   */
  bool setConsoleWindowState(GHOST_TConsoleWindowState /*action*/)
  {
    return false;
  }

  /**
   * Handles a tablet event.
   * \param eventPtr: An #NSEvent pointer (cast to void* to enable compilation in standard C++).
   * \param eventType: The type of the event.
   * It needs to be passed separately as it can be either directly in the event type,
   * or as a sub-type if combined with a mouse button event.
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleTabletEvent(void *eventPtr, short eventType);
  bool handleTabletEvent(void *eventPtr);

  /**
   * Handles a mouse event.
   * \param eventPtr: An #NSEvent pointer (cast to `void *` to enable compilation in standard C++).
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleMouseEvent(void *eventPtr);

  /**
   * Handles a key event.
   * \param eventPtr: An #NSEvent pointer (cast to `void *` to enable compilation in standard C++).
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleKeyEvent(void *eventPtr);

  /**
   * Show a system message box
   * \param title: The title of the message box.
   * \param message: The message to display.
   * \param help_label: Help button label.
   * \param continue_label: Continue button label.
   * \param link: An optional hyperlink.
   * \param dialog_options: Options  how to display the message.
   */
  virtual GHOST_TSuccess showMessageBox(const char *title,
                                        const char *message,
                                        const char *help_label,
                                        const char *continue_label,
                                        const char *link,
                                        GHOST_DialogOptions dialog_options) const;

 protected:
  /**
   * Initializes the system.
   * For now, it just registers the window class (WNDCLASS).
   * \return A success value.
   */
  GHOST_TSuccess init();

  /**
   * Performs the actual cursor position update (location in screen coordinates).
   * \param x: The x-coordinate of the cursor.
   * \param y: The y-coordinate of the cursor.
   * \return Indication of success.
   */
  GHOST_TSuccess setMouseCursorPosition(int32_t x, int32_t y);

  /** Event has been processed directly by Cocoa (or NDOF manager)
   * and has sent a ghost event to be dispatched */
  bool m_outsideLoopEventProcessed;

  /** Raised window is not yet known by the window manager,
   * so delay application become active event handling */
  bool m_needDelayedApplicationBecomeActiveEventProcessing;

  /** State of the modifiers. */
  uint32_t m_modifierMask;

  /** Ignores window size messages (when window is dragged). */
  bool m_ignoreWindowSizedMessages;

  /** Temporarily ignore momentum scroll events */
  bool m_ignoreMomentumScroll;
  /** Is the scroll wheel event generated by a multi-touch trackpad or mouse? */
  bool m_multiTouchScroll;
  /** To prevent multiple warp, we store the time of the last warp event
   * and ignore mouse moved events generated before that. */
  double m_last_warp_timestamp;
};
