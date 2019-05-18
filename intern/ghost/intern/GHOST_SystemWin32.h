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
 * Declaration of GHOST_SystemWin32 class.
 */

#ifndef __GHOST_SYSTEMWIN32_H__
#define __GHOST_SYSTEMWIN32_H__

#ifndef WIN32
#  error WIN32 only!
#endif  // WIN32

/* require Windows XP or newer */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x501

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>  // for drag-n-drop

#include "GHOST_System.h"

class GHOST_EventButton;
class GHOST_EventCursor;
class GHOST_EventKey;
class GHOST_EventWheel;
class GHOST_EventWindow;
class GHOST_EventDragnDrop;

class GHOST_WindowWin32;

/**
 * WIN32 Implementation of GHOST_System class.
 * \see GHOST_System.
 */
class GHOST_SystemWin32 : public GHOST_System {
 public:
  /**
   * Constructor.
   */
  GHOST_SystemWin32();

  /**
   * Destructor.
   */
  ~GHOST_SystemWin32();

  /***************************************************************************************
   ** Time(r) functionality
   ***************************************************************************************/

  /**
   * Returns the system time.
   * Returns the number of milliseconds since the start of the system process.
   * This overloaded method uses the high frequency timer if available.
   * \return The number of milliseconds.
   */
  GHOST_TUns64 getMilliSeconds() const;

  /***************************************************************************************
   ** Display/window management functionality
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
   * \param   left    The coordinate of the left edge of the window.
   * \param   top     The coordinate of the top edge of the window.
   * \param   width   The width the window.
   * \param   height  The height the window.
   * \param   state   The state of the window when opened.
   * \param   type    The type of drawing context installed in this window.
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
   * Never explicitly delete the window, use disposeContext() instead.
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
   ** Event management functionality
   ***************************************************************************************/

  /**
   * Gets events from the system and stores them in the queue.
   * \param waitForEvent Flag to wait for an event (or return immediately).
   * \return Indication of the presence of events.
   */
  bool processEvents(bool waitForEvent);

  /***************************************************************************************
   ** Cursor management functionality
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
   ** Access to mouse button and keyboard states.
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
   * Returns unsigned char from CUT_BUFFER0
   * \param selection     Used by X11 only
   * \return              Returns the Clipboard
   */
  GHOST_TUns8 *getClipboard(bool selection) const;

  /**
   * Puts buffer to system clipboard
   * \param selection     Used by X11 only
   * \return              No return
   */
  void putClipboard(GHOST_TInt8 *buffer, bool selection) const;

  /**
   * Creates a drag'n'drop event and pushes it immediately onto the event queue.
   * Called by GHOST_DropTargetWin32 class.
   * \param eventType: The type of drag'n'drop event
   * \param draggedObjectType: The type object concerned
   * (currently array of file names, string, ?bitmap)
   * \param mouseX: x mouse coordinate (in window coordinates)
   * \param mouseY: y mouse coordinate
   * \param window: The window on which the event occurred
   * \return Indication whether the event was handled.
   */
  static GHOST_TSuccess pushDragDropEvent(GHOST_TEventType eventType,
                                          GHOST_TDragnDropTypes draggedObjectType,
                                          GHOST_WindowWin32 *window,
                                          int mouseX,
                                          int mouseY,
                                          void *data);

 protected:
  /**
   * Initializes the system.
   * For now, it just registers the window class (WNDCLASS).
   * \return A success value.
   */
  GHOST_TSuccess init();

  /**
   * Closes the system down.
   * \return A success value.
   */
  GHOST_TSuccess exit();

  /**
   * Converts raw WIN32 key codes from the wndproc to GHOST keys.
   * \param vKey      The virtual key from hardKey
   * \param ScanCode  The ScanCode of pressed key (similar to PS/2 Set 1)
   * \param extend    Flag if key is not primly (left or right)
   * \return The GHOST key (GHOST_kKeyUnknown if no match).
   */
  GHOST_TKey convertKey(short vKey, short ScanCode, short extend) const;

  /**
   * Catches raw WIN32 key codes from WM_INPUT in the wndproc.
   * \param raw       RawInput structure with detailed info about the key event
   * \param keyDown   Pointer flag that specify if a key is down
   * \param vk        Pointer to virtual key
   * \return The GHOST key (GHOST_kKeyUnknown if no match).
   */
  GHOST_TKey hardKey(RAWINPUT const &raw, int *keyDown, char *vk);

  /**
   * Creates mouse button event.
   * \param type      The type of event to create.
   * \param window    The window receiving the event (the active window).
   * \param mask      The button mask of this event.
   * \return The event created.
   */
  static GHOST_EventButton *processButtonEvent(GHOST_TEventType type,
                                               GHOST_WindowWin32 *window,
                                               GHOST_TButtonMask mask);

  /**
   * Creates pointer event.
   * \param type      The type of event to create.
   * \param window    The window receiving the event (the active window).
   * \param wParam    The wParam from the wndproc
   * \param lParam    The lParam from the wndproc
   * \param eventhandled true if the method handled the event
   * \return The event created.
   */
  static GHOST_Event *processPointerEvent(GHOST_TEventType type,
                                          GHOST_WindowWin32 *window,
                                          WPARAM wParam,
                                          LPARAM lParam,
                                          bool &eventhandled);

  /**
   * Creates cursor event.
   * \param type      The type of event to create.
   * \param window    The window receiving the event (the active window).
   * \return The event created.
   */
  static GHOST_EventCursor *processCursorEvent(GHOST_TEventType type, GHOST_WindowWin32 *window);

  /**
   * Handles a mouse wheel event.
   * \param window    The window receiving the event (the active window).
   * \param wParam    The wParam from the wndproc
   * \param lParam    The lParam from the wndproc
   */
  static void processWheelEvent(GHOST_WindowWin32 *window, WPARAM wParam, LPARAM lParam);

  /**
   * Creates a key event and updates the key data stored locally (m_modifierKeys).
   * In most cases this is a straightforward conversion of key codes.
   * For the modifier keys however, we want to distinguish left and right keys.
   * \param window    The window receiving the event (the active window).
   * \param raw       RawInput structure with detailed info about the key event
   */
  static GHOST_EventKey *processKeyEvent(GHOST_WindowWin32 *window, RAWINPUT const &raw);

  /**
   * Process special keys (VK_OEM_*), to see if current key layout
   * gives us anything special, like ! on french AZERTY.
   * \param vKey      The virtual key from hardKey
   * \param scanCode  The ScanCode of pressed key (simular to PS/2 Set 1)
   */
  GHOST_TKey processSpecialKey(short vKey, short scanCode) const;

  /**
   * Creates a window event.
   * \param type      The type of event to create.
   * \param window    The window receiving the event (the active window).
   * \return The event created.
   */
  static GHOST_Event *processWindowEvent(GHOST_TEventType type, GHOST_WindowWin32 *window);

#ifdef WITH_INPUT_IME
  /**
   * Creates a IME event.
   * \param type      The type of event to create.
   * \param window        The window receiving the event (the active window).
   * \param data      IME data.
   * \return The event created.
   */
  static GHOST_Event *processImeEvent(GHOST_TEventType type,
                                      GHOST_WindowWin32 *window,
                                      GHOST_TEventImeData *data);
#endif  // WITH_INPUT_IME

  /**
   * Handles minimum window size.
   * \param minmax    The MINMAXINFO structure.
   */
  static void processMinMaxInfo(MINMAXINFO *minmax);

#ifdef WITH_INPUT_NDOF
  /**
   * Handles Motion and Button events from a SpaceNavigator or related device.
   * Instead of returning an event object, this function communicates directly
   * with the GHOST_NDOFManager.
   * \param raw       RawInput structure with detailed info about the NDOF event
   * \return Whether an event was generated and sent.
   */
  bool processNDOF(RAWINPUT const &raw);
#endif

  /**
   * Returns the local state of the modifier keys (from the message queue).
   * \param keys The state of the keys.
   */
  inline void retrieveModifierKeys(GHOST_ModifierKeys &keys) const;

  /**
   * Stores the state of the modifier keys locally.
   * For internal use only!
   * param keys The new state of the modifier keys.
   */
  inline void storeModifierKeys(const GHOST_ModifierKeys &keys);

  /**
   * Check current key layout for AltGr
   */
  inline void handleKeyboardChange(void);

  /**
   * Windows call back routine for our window class.
   */
  static LRESULT WINAPI s_wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  /**
   * Toggles console
   * \param action
   * - 0 - Hides
   * - 1 - Shows
   * - 2 - Toggles
   * - 3 - Hides if it runs not from  command line
   * - * - Does nothing
   * \return current status (1 -visible, 0 - hidden)
   */
  int toggleConsole(int action);

  /** The current state of the modifier keys. */
  GHOST_ModifierKeys m_modifierKeys;
  /** State variable set at initialization. */
  bool m_hasPerformanceCounter;
  /** High frequency timer variable. */
  __int64 m_freq;
  /** High frequency timer variable. */
  __int64 m_start;
  /** AltGr on current keyboard layout. */
  bool m_hasAltGr;
  /** language identifier. */
  WORD m_langId;
  /** stores keyboard layout. */
  HKL m_keylayout;

  /** Console status */
  int m_consoleStatus;

  /** Wheel delta accumulator */
  int m_wheelDeltaAccum;
};

inline void GHOST_SystemWin32::retrieveModifierKeys(GHOST_ModifierKeys &keys) const
{
  keys = m_modifierKeys;
}

inline void GHOST_SystemWin32::storeModifierKeys(const GHOST_ModifierKeys &keys)
{
  m_modifierKeys = keys;
}

inline void GHOST_SystemWin32::handleKeyboardChange(void)
{
  m_keylayout = GetKeyboardLayout(0);  // get keylayout for current thread
  int i;
  SHORT s;

  // save the language identifier.
  m_langId = LOWORD(m_keylayout);

  for (m_hasAltGr = false, i = 32; i < 256; ++i) {
    s = VkKeyScanEx((char)i, m_keylayout);
    // s == -1 means no key that translates passed char code
    // high byte contains shift state. bit 2 ctrl pressed, bit 4 alt pressed
    // if both are pressed, we have AltGr keycombo on keylayout
    if (s != -1 && (s & 0x600) == 0x600) {
      m_hasAltGr = true;
      break;
    }
  }
}
#endif  // __GHOST_SYSTEMWIN32_H__
