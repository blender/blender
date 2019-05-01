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
 * Declaration of GHOST_SystemCocoa class.
 */

#ifndef __GHOST_SYSTEMCOCOA_H__
#define __GHOST_SYSTEMCOCOA_H__

#ifndef __APPLE__
#  error Apple OSX only!
#endif  // __APPLE__

//#define __CARBONSOUND__

#include "GHOST_System.h"

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
  GHOST_TUns64 getMilliSeconds() const;

  /***************************************************************************************
   * Display/window management functionality
   ***************************************************************************************/

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

  /** Returns the combine dimensions of all monitors.
   * \return The dimension of the workspace.
   */
  void getAllDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const;

  /**
   * Create a new window.
   * The new window is added to the list of windows managed.
   * Never explicitly delete the window, use disposeWindow() instead.
   * \param   title           The name of the window
   * (displayed in the title bar of the window if the OS supports it).
   * \param   left            The coordinate of the left edge of the window.
   * \param   top             The coordinate of the top edge of the window.
   * \param   width           The width the window.
   * \param   height          The height the window.
   * \param   state           The state of the window when opened.
   * \param   type            The type of drawing context installed in this window.
   * \param glSettings: Misc OpenGL settings.
   * \param exclusive: Use to show the window ontop and ignore others (used fullscreen).
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

  /***************************************************************************************
   * Event management functionality
   ***************************************************************************************/

  /**
   * Gets events from the system and stores them in the queue.
   * \param waitForEvent Flag to wait for an event (or return immediately).
   * \return Indication of the presence of events.
   */
  bool processEvents(bool waitForEvent);

  /**
   * Handle User request to quit, from Menu bar Quit, and Cmd+Q
   * Display alert panel if changes performed since last save
   */
  GHOST_TUns8 handleQuitRequest();

  /**
   * Handle Cocoa openFile event
   * Display confirmation request panel if changes performed since last save
   */
  bool handleOpenDocumentRequest(void *filepathStr);

  /**
   * Handles a drag'n'drop destination event. Called by GHOST_WindowCocoa window subclass
   * \param eventType The type of drag'n'drop event
   * \param draggedObjectType The type object concerned
   * (currently array of file names, string, TIFF image)
   * \param mouseX x mouse coordinate (in cocoa base window coordinates)
   * \param mouseY y mouse coordinate
   * \param window The window on which the event occurred
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
   * \param x         The x-coordinate of the cursor.
   * \param y         The y-coordinate of the cursor.
   * \return          Indication of success.
   */
  GHOST_TSuccess getCursorPosition(GHOST_TInt32 &x, GHOST_TInt32 &y) const;

  /**
   * Updates the location of the cursor (location in screen coordinates).
   * \param x         The x-coordinate of the cursor.
   * \param y         The y-coordinate of the cursor.
   * \return          Indication of success.
   */
  GHOST_TSuccess setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y);

  /***************************************************************************************
   * Access to mouse button and keyboard states.
   ***************************************************************************************/

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
   * Returns Clipboard data
   * \param selection     Indicate which buffer to return
   * \return              Returns the selected buffer
   */
  GHOST_TUns8 *getClipboard(bool selection) const;

  /**
   * Puts buffer to system clipboard
   * \param buffer    The buffer to be copied
   * \param selection Indicates which buffer to copy too, only used on X11
   */
  void putClipboard(GHOST_TInt8 *buffer, bool selection) const;

  /**
   * Handles a window event. Called by GHOST_WindowCocoa window delegate
   * \param eventType The type of window event
   * \param window The window on which the event occurred
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleWindowEvent(GHOST_TEventType eventType, GHOST_WindowCocoa *window);

  /**
   * Handles the Cocoa event telling the application has become active (again)
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleApplicationBecomeActiveEvent();

  /**
   * External objects should call this when they send an event outside processEvents.
   */
  void notifyExternalEventProcessed();

  /**
   * \see GHOST_ISystem
   */
  int toggleConsole(int action)
  {
    return 0;
  }

  /**
   * Handles a tablet event.
   * \param eventPtr  An NSEvent pointer (casted to void* to enable compilation in standard C++)
   * \param eventType The type of the event.
   * It needs to be passed separately as it can be either directly in the event type,
   * or as a subtype if combined with a mouse button event.
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleTabletEvent(void *eventPtr, short eventType);
  bool handleTabletEvent(void *eventPtr);

  /**
   * Handles a mouse event.
   * \param eventPtr  An NSEvent pointer (casted to void* to enable compilation in standard C++)
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleMouseEvent(void *eventPtr);

  /**
   * Handles a key event.
   * \param eventPtr  An NSEvent pointer (casted to void* to enable compilation in standard C++)
   * \return Indication whether the event was handled.
   */
  GHOST_TSuccess handleKeyEvent(void *eventPtr);

  /**
   * Informs if the system provides native dialogs (eg. confirm quit)
   */
  virtual bool supportsNativeDialogs(void);

 protected:
  /**
   * Initializes the system.
   * For now, it just registers the window class (WNDCLASS).
   * \return A success value.
   */
  GHOST_TSuccess init();

  /**
   * Performs the actual cursor position update (location in screen coordinates).
   * \param x         The x-coordinate of the cursor.
   * \param y         The y-coordinate of the cursor.
   * \return          Indication of success.
   */
  GHOST_TSuccess setMouseCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y);

  /** Start time at initialization. */
  GHOST_TUns64 m_start_time;

  /** Event has been processed directly by Cocoa (or NDOF manager)
   * and has sent a ghost event to be dispatched */
  bool m_outsideLoopEventProcessed;

  /** Raised window is not yet known by the window manager,
   * so delay application become active event handling */
  bool m_needDelayedApplicationBecomeActiveEventProcessing;

  /** State of the modifiers. */
  GHOST_TUns32 m_modifierMask;

  /** Ignores window size messages (when window is dragged). */
  bool m_ignoreWindowSizedMessages;

  /** Temporarily ignore momentum scroll events */
  bool m_ignoreMomentumScroll;
  /** Is the scroll wheel event generated by a multitouch trackpad or mouse? */
  bool m_multiTouchScroll;
};

#endif  // __GHOST_SYSTEMCOCOA_H__
