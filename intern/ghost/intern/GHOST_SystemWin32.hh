/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemWin32 class.
 */

#pragma once

#ifndef WIN32
#  error WIN32 only!
#endif /* WIN32 */

#define WIN32_LEAN_AND_MEAN
#include <ole2.h> /* For drag-n-drop. */
#include <windows.h>

#include "GHOST_System.hh"

class GHOST_EventButton;
class GHOST_EventCursor;
class GHOST_EventKey;
class GHOST_EventWheel;
class GHOST_EventWindow;
class GHOST_EventDragnDrop;

class GHOST_ContextD3D;
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
   * This method converts performance counter measurements into milliseconds since the start of the
   * system process.
   * \return The number of milliseconds since the start of the system process.
   */
  uint64_t performanceCounterToMillis(__int64 perf_ticks) const;

  /**
   * This method converts system ticks into milliseconds since the start of the
   * system process.
   * \return The number of milliseconds since the start of the system process.
   */
  uint64_t tickCountToMillis(__int64 ticks) const;

  /**
   * Returns the system time.
   * Returns the number of milliseconds since the start of the system process.
   * This overloaded method uses the high frequency timer if available.
   * \return The number of milliseconds.
   */
  uint64_t getMilliSeconds() const;

  /***************************************************************************************
   ** Display/window management functionality
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

  /**
   * Returns the dimensions of all displays on this system.
   * \return The dimension of the main display.
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
   * \param parentWindow: Parent window.
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
                              const GHOST_IWindow *parentWindow = 0);

  /**
   * Create a new off-screen context.
   * Never explicitly delete the window, use #disposeContext() instead.
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
   * Create a new off-screen DirectX context.
   * Never explicitly delete the context, use #disposeContext() instead.
   * This is for GHOST internal, Win32 specific use, so it can be called statically.
   *
   * \return The new context (or 0 if creation failed).
   */
  static GHOST_ContextD3D *createOffscreenContextD3D();

  /**
   * Dispose of a DirectX context.
   * This is for GHOST internal, Win32 specific use, so it can be called statically.
   * \param context: Pointer to the context to be disposed.
   * \return Indication of success.
   */
  static GHOST_TSuccess disposeContextD3D(GHOST_ContextD3D *context);

  /***************************************************************************************
   ** Event management functionality
   ***************************************************************************************/

  /**
   * Gets events from the system and stores them in the queue.
   * \param waitForEvent: Flag to wait for an event (or return immediately).
   * \return Indication of the presence of events.
   */
  bool processEvents(bool waitForEvent);

  /***************************************************************************************
   ** Cursor management functionality
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
   ** Access to mouse button and keyboard states.
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
   * Returns unsigned char from CUT_BUFFER0
   * \param selection: Used by X11 only.
   * \return Returns the Clipboard.
   */
  char *getClipboard(bool selection) const;

  /**
   * Puts buffer to system clipboard.
   * \param selection: Used by X11 only.
   * \return No return.
   */
  void putClipboard(const char *buffer, bool selection) const;

  /**
   * Returns GHOST_kSuccess if the clipboard contains an image.
   */
  GHOST_TSuccess hasClipboardImage(void) const;

  /**
   * Get image data from the Clipboard
   * \param r_width: the returned image width in pixels.
   * \param r_height: the returned image height in pixels.
   * \return pointer uint array in RGBA byte order. Caller must free.
   */
  uint *getClipboardImage(int *r_width, int *r_height) const;

  /**
   * Put image data to the Clipboard
   * \param rgba: uint array in RGBA byte order.
   * \param width: the image width in pixels.
   * \param height: the image height in pixels.
   */
  GHOST_TSuccess putClipboardImage(uint *rgba, int width, int height) const;

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
                                GHOST_DialogOptions dialog_options) const;

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

  /***************************************************************************************
   ** Modify tablet API
   ***************************************************************************************/

  /**
   * Set which tablet API to use.
   * \param api: Enum indicating which API to use.
   */
  void setTabletAPI(GHOST_TTabletAPI api) override;

  /***************************************************************************************
   ** Debug Info
   ***************************************************************************************/

  /**
   * Specify which debug messages are to be shown.
   * \param debug: Flag for systems to debug.
   */
  void initDebug(GHOST_Debug debug) override;

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
   * Converts raw WIN32 key codes from the `wndproc` to GHOST keys.
   * \param vKey: The virtual key from #hardKey.
   * \param ScanCode: The ScanCode of pressed key (similar to PS/2 Set 1).
   * \param extend: Flag if key is not primly (left or right).
   * \return The GHOST key (GHOST_kKeyUnknown if no match).
   */
  GHOST_TKey convertKey(short vKey, short ScanCode, short extend) const;

  /**
   * Catches raw WIN32 key codes from WM_INPUT in the `wndproc`.
   * \param raw: RawInput structure with detailed info about the key event.
   * \param r_key_down: Set true when the key is pressed, otherwise false.
   * \return The GHOST key (GHOST_kKeyUnknown if no match).
   */
  GHOST_TKey hardKey(RAWINPUT const &raw, bool *r_key_down);

  /**
   * Creates mouse button event.
   * \param type: The type of event to create.
   * \param window: The window receiving the event (the active window).
   * \param mask: The button mask of this event.
   * \return The event created.
   */
  static GHOST_EventButton *processButtonEvent(GHOST_TEventType type,
                                               GHOST_WindowWin32 *window,
                                               GHOST_TButton mask);

  /**
   * Creates tablet events from Wintab events.
   * \param window: The window receiving the event (the active window).
   */
  static void processWintabEvent(GHOST_WindowWin32 *window);

  /**
   * Creates tablet events from pointer events.
   * \param type: The type of pointer event.
   * \param window: The window receiving the event (the active window).
   * \param wParam: The wParam from the `wndproc`.
   * \param lParam: The lParam from the `wndproc`.
   * \param eventhandled: True if the method handled the event.
   */
  static void processPointerEvent(
      UINT type, GHOST_WindowWin32 *window, WPARAM wParam, LPARAM lParam, bool &eventhandled);

  /**
   * Creates cursor event.
   * \param window: The window receiving the event (the active window).
   * \return The event created.
   */
  static GHOST_EventCursor *processCursorEvent(GHOST_WindowWin32 *window,
                                               const int32_t screen_co[2]);

  /**
   * Handles a mouse wheel event.
   * \param window: The window receiving the event (the active window).
   * \param wParam: The wParam from the `wndproc`.
   * \param lParam: The lParam from the `wndproc`.
   */
  static void processWheelEvent(GHOST_WindowWin32 *window, WPARAM wParam, LPARAM lParam);

  /**
   * Creates a key event and updates the key data stored locally (m_modifierKeys).
   * In most cases this is a straightforward conversion of key codes.
   * For the modifier keys however, we want to distinguish left and right keys.
   * \param window: The window receiving the event (the active window).
   * \param raw: RawInput structure with detailed info about the key event.
   */
  static GHOST_EventKey *processKeyEvent(GHOST_WindowWin32 *window, RAWINPUT const &raw);

  /**
   * Process special keys `VK_OEM_*`, to see if current key layout
   * gives us anything special, like `!` on French AZERTY.
   * \param vKey: The virtual key from #hardKey.
   * \param scanCode: The ScanCode of pressed key (similar to PS/2 Set 1).
   */
  GHOST_TKey processSpecialKey(short vKey, short scanCode) const;

  /**
   * Creates a window size event.
   * \param window: The window receiving the event (the active window).
   * \return The event created.
   */
  static GHOST_Event *processWindowSizeEvent(GHOST_WindowWin32 *window);

  /**
   * Creates a window event.
   * \param type: The type of event to create.
   * \param window: The window receiving the event (the active window).
   * \return The event created.
   */
  static GHOST_Event *processWindowEvent(GHOST_TEventType type, GHOST_WindowWin32 *window);

#ifdef WITH_INPUT_IME
  /**
   * Creates a IME event.
   * \param type: The type of event to create.
   * \param window: The window receiving the event (the active window).
   * \param data: IME data.
   * \return The event created.
   */
  static GHOST_Event *processImeEvent(GHOST_TEventType type,
                                      GHOST_WindowWin32 *window,
                                      GHOST_TEventImeData *data);
#endif /* WITH_INPUT_IME */

  /**
   * Handles minimum window size.
   * \param minmax: The MINMAXINFO structure.
   */
  static void processMinMaxInfo(MINMAXINFO *minmax);

#ifdef WITH_INPUT_NDOF
  /**
   * Handles Motion and Button events from a SpaceNavigator or related device.
   * Instead of returning an event object, this function communicates directly
   * with the GHOST_NDOFManager.
   * \param raw: RawInput structure with detailed info about the NDOF event.
   * \return Whether an event was generated and sent.
   */
  bool processNDOF(RAWINPUT const &raw);
#endif

  /**
   * Drives Direct Manipulation update.
   */
  void driveTrackpad();

  /**
   * Creates trackpad events for the active window.
   */
  void processTrackpad();

  /**
   * Check current key layout for AltGr
   */
  inline void handleKeyboardChange(void);

  /**
   * Windows call back routine for our window class.
   */
  static LRESULT WINAPI s_wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  /**
   * Set the Console State
   * \param action: console state
   * \return current status (1 -visible, 0 - hidden)
   */
  bool setConsoleWindowState(GHOST_TConsoleWindowState action);

  /** State variable set at initialization. */
  bool m_hasPerformanceCounter;
  /** High frequency timer variable. */
  __int64 m_freq;
  /** High frequency timer variable. */
  __int64 m_start;
  /** Low frequency timer variable. */
  __int64 m_lfstart;
  /** AltGr on current keyboard layout. */
  bool m_hasAltGr;
  /** Language identifier. */
  WORD m_langId;
  /** Stores keyboard layout. */
  HKL m_keylayout;

  /** Console status. */
  bool m_consoleStatus;

  /** Wheel delta accumulator. */
  int m_wheelDeltaAccum;
};

inline void GHOST_SystemWin32::handleKeyboardChange(void)
{
  m_keylayout = GetKeyboardLayout(0); /* Get keylayout for current thread. */
  int i;
  SHORT s;

  /* Save the language identifier. */
  m_langId = LOWORD(m_keylayout);

  for (m_hasAltGr = false, i = 32; i < 256; ++i) {
    s = VkKeyScanEx((char)i, m_keylayout);
    /* `s == -1` means no key that translates passed char code high byte contains shift state.
     * bit 2 Control pressed, bit 4 `Alt` pressed if both are pressed,
     * we have `AltGr` key-combination on key-layout. */
    if (s != -1 && (s & 0x600) == 0x600) {
      m_hasAltGr = true;
      break;
    }
  }
}
