/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_System class.
 */

#pragma once

#include "GHOST_ISystem.h"

#include "GHOST_Buttons.h"
#include "GHOST_Debug.h"
#include "GHOST_EventManager.h"
#include "GHOST_ModifierKeys.h"
#ifdef WITH_GHOST_DEBUG
#  include "GHOST_EventPrinter.h"
#endif  // WITH_GHOST_DEBUG

class GHOST_DisplayManager;
class GHOST_Event;
class GHOST_TimerManager;
class GHOST_Window;
class GHOST_WindowManager;
#ifdef WITH_INPUT_NDOF
class GHOST_NDOFManager;
#endif

/**
 * Implementation of platform independent functionality of the GHOST_ISystem
 * interface.
 * GHOST_System is an abstract class because not all methods of GHOST_ISystem
 * are implemented.
 * \see GHOST_ISystem.
 */
class GHOST_System : public GHOST_ISystem {
 protected:
  /**
   * Constructor.
   * Protected default constructor to force use of static createSystem member.
   */
  GHOST_System();

  /**
   * Destructor.
   * Protected default constructor to force use of static dispose member.
   */
  virtual ~GHOST_System();

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
  virtual uint64_t getMilliSeconds() const;

  /**
   * Installs a timer.
   *
   * \note On most operating systems, messages need to be processed in order
   * for the timer callbacks to be invoked.
   *
   * \param delay: The time to wait for the first call to the #timerProc (in milliseconds).
   * \param interval: The interval between calls to the #timerProc.
   * \param timerProc: The callback invoked when the interval expires.
   * \param userData: Placeholder for user data.
   * \return A timer task (0 if timer task installation failed).
   */
  GHOST_ITimerTask *installTimer(uint64_t delay,
                                 uint64_t interval,
                                 GHOST_TimerProcPtr timerProc,
                                 GHOST_TUserDataPtr userData = nullptr);

  /**
   * Removes a timer.
   * \param timerTask: Timer task to be removed.
   * \return Indication of success.
   */
  GHOST_TSuccess removeTimer(GHOST_ITimerTask *timerTask);

  /***************************************************************************************
   * Display/window management functionality
   ***************************************************************************************/

  /**
   * Dispose a window.
   * \param window: Pointer to the window to be disposed.
   * \return Indication of success.
   */
  GHOST_TSuccess disposeWindow(GHOST_IWindow *window);

  /**
   * Create a new off-screen context.
   * Never explicitly delete the context, use #disposeContext() instead.
   * \return The new context (or 0 if creation failed).
   */
  virtual GHOST_IContext *createOffscreenContext(GHOST_GLSettings glSettings) = 0;

  /**
   * Returns whether a window is valid.
   * \param window: Pointer to the window to be checked.
   * \return Indication of validity.
   */
  bool validWindow(GHOST_IWindow *window);

  /**
   * Begins full screen mode.
   * \param setting: The new setting of the display.
   * \param window: Window displayed in full screen.
   * \param stereoVisual: Stereo visual for quad buffered stereo.
   * This window is invalid after full screen has been ended.
   * \return Indication of success.
   */
  GHOST_TSuccess beginFullScreen(const GHOST_DisplaySetting &setting,
                                 GHOST_IWindow **window,
                                 const bool stereoVisual);

  /**
   * Updates the resolution while in full-screen mode.
   * \param setting: The new setting of the display.
   * \param window: Window displayed in full screen.
   *
   * \return Indication of success.
   */
  GHOST_TSuccess updateFullScreen(const GHOST_DisplaySetting &setting, GHOST_IWindow **window);

  /**
   * Ends full screen mode.
   * \return Indication of success.
   */
  GHOST_TSuccess endFullScreen(void);

  /**
   * Returns current full screen mode status.
   * \return The current status.
   */
  bool getFullScreen(void);

  /**
   * Native pixel size support (MacBook 'retina').
   * \return The pixel size in float.
   */
  bool useNativePixel(void);
  bool m_nativePixel;

  bool supportsCursorWarp(void);
  bool supportsWindowPosition(void);

  /**
   * Focus window after opening, or put them in the background.
   */
  void useWindowFocus(const bool use_focus);

  bool m_windowFocus;

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
   * Inherited from GHOST_ISystem but left pure virtual
   *
   * virtual bool processEvents(bool waitForEvent) = 0;
   */

  /**
   * Dispatches all the events on the stack.
   * The event stack will be empty afterwards.
   */
  void dispatchEvents();

  /**
   * Adds the given event consumer to our list.
   * \param consumer: The event consumer to add.
   * \return Indication of success.
   */
  GHOST_TSuccess addEventConsumer(GHOST_IEventConsumer *consumer);

  /**
   * Remove the given event consumer to our list.
   * \param consumer: The event consumer to remove.
   * \return Indication of success.
   */
  GHOST_TSuccess removeEventConsumer(GHOST_IEventConsumer *consumer);

  /***************************************************************************************
   * Cursor management functionality
   ***************************************************************************************/

  /* Client relative functions use a default implementation
   * that converts from screen-coordinates to client coordinates.
   * Implementations may override. */

  GHOST_TSuccess getCursorPositionClientRelative(const GHOST_IWindow *window,
                                                 int32_t &x,
                                                 int32_t &y) const;
  GHOST_TSuccess setCursorPositionClientRelative(GHOST_IWindow *window, int32_t x, int32_t y);

  /**
   * Inherited from GHOST_ISystem but left pure virtual
   * <pre>
   * GHOST_TSuccess getCursorPosition(int32_t& x, int32_t& y) const = 0;
   * GHOST_TSuccess setCursorPosition(int32_t x, int32_t y)
   * </pre>
   */

  /***************************************************************************************
   * Access to mouse button and keyboard states.
   ***************************************************************************************/

  /**
   * Returns the state of a modifier key (outside the message queue).
   * \param mask: The modifier key state to retrieve.
   * \param isDown: The state of a modifier key (true == pressed).
   * \return Indication of success.
   */
  GHOST_TSuccess getModifierKeyState(GHOST_TModifierKey mask, bool &isDown) const;

  /**
   * Returns the state of a mouse button (outside the message queue).
   * \param mask: The button state to retrieve.
   * \param isDown: Button state.
   * \return Indication of success.
   */
  GHOST_TSuccess getButtonState(GHOST_TButton mask, bool &isDown) const;

  /**
   * Enable multi-touch gestures if supported.
   * \param use: Enable or disable.
   */
  void setMultitouchGestures(const bool use);

  /**
   * Set which tablet API to use. Only affects Windows, other platforms have a single API.
   * \param api: Enum indicating which API to use.
   */
  virtual void setTabletAPI(GHOST_TTabletAPI api);
  GHOST_TTabletAPI getTabletAPI(void);

#ifdef WITH_INPUT_NDOF
  /***************************************************************************************
   * Access to 3D mouse.
   ***************************************************************************************/

  /**
   * Sets 3D mouse dead-zone
   * \param deadzone: Dead-zone of the 3D mouse (both for rotation and pan) relative to full range.
   */
  void setNDOFDeadZone(float deadzone);
#endif

  /***************************************************************************************
   * Other (internal) functionality.
   ***************************************************************************************/

  /**
   * Pushes an event on the stack.
   * To dispatch it, call dispatchEvent() or dispatchEvents().
   * Do not delete the event!
   * \param event: The event to push on the stack.
   */
  GHOST_TSuccess pushEvent(GHOST_IEvent *event);

  /**
   * \return The timer manager.
   */
  inline GHOST_TimerManager *getTimerManager() const;

  /**
   * \return A pointer to our event manager.
   */
  inline GHOST_EventManager *getEventManager() const;

  /**
   * \return A pointer to our window manager.
   */
  inline GHOST_WindowManager *getWindowManager() const;

#ifdef WITH_INPUT_NDOF
  /**
   * \return A pointer to our n-degree of freedom manager.
   */
  inline GHOST_NDOFManager *getNDOFManager() const;
#endif

  /**
   * Returns the state of all modifier keys.
   * \param keys: The state of all modifier keys (true == pressed).
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const = 0;

  /**
   * Returns the state of the mouse buttons (outside the message queue).
   * \param buttons: The state of the buttons.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const = 0;

  /**
   * Returns the selection buffer
   * \param selection: Only used on X11.
   * \return Returns the clipboard data
   *
   */
  virtual char *getClipboard(bool selection) const = 0;

  /**
   * Put data to the Clipboard
   * \param buffer: The buffer to copy to the clipboard.
   * \param selection: The clipboard to copy too only used on X11.
   */
  virtual void putClipboard(const char *buffer, bool selection) const = 0;

  /**
   * Show a system message box
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
                                        GHOST_DialogOptions /*dialog_options*/) const
  {
    return GHOST_kFailure;
  };

  /***************************************************************************************
   * Debugging
   ***************************************************************************************/

  /**
   * Specify whether debug messages are to be shown.
   * \param debug: Flag for systems to debug.
   */
  virtual void initDebug(GHOST_Debug debug);

  /**
   * Check whether debug messages are to be shown.
   */
  virtual bool isDebugEnabled();

 protected:
  /**
   * Initialize the system.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess init();

  /**
   * Shut the system down.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess exit();

  /**
   * Creates a full-screen window.
   * \param window: The window created.
   * \return Indication of success.
   */
  GHOST_TSuccess createFullScreenWindow(GHOST_Window **window,
                                        const GHOST_DisplaySetting &settings,
                                        const bool stereoVisual);

  /** The display manager (platform dependent). */
  GHOST_DisplayManager *m_displayManager;

  /** The timer manager. */
  GHOST_TimerManager *m_timerManager;

  /** The window manager. */
  GHOST_WindowManager *m_windowManager;

  /** The event manager. */
  GHOST_EventManager *m_eventManager;

#ifdef WITH_INPUT_NDOF
  /** The N-degree of freedom device manager */
  GHOST_NDOFManager *m_ndofManager;
#endif

  /** Prints all the events. */
#ifdef WITH_GHOST_DEBUG
  GHOST_EventPrinter *m_eventPrinter;
#endif  // WITH_GHOST_DEBUG

  /** Settings of the display before the display went full-screen. */
  GHOST_DisplaySetting m_preFullScreenSetting;

  /* Use multi-touch gestures? */
  bool m_multitouchGestures;

  /** Which tablet API to use. */
  GHOST_TTabletAPI m_tabletAPI;

  bool m_is_debug_enabled;
};

inline GHOST_TimerManager *GHOST_System::getTimerManager() const
{
  return m_timerManager;
}

inline GHOST_EventManager *GHOST_System::getEventManager() const
{
  return m_eventManager;
}

inline GHOST_WindowManager *GHOST_System::getWindowManager() const
{
  return m_windowManager;
}

#ifdef WITH_INPUT_NDOF
inline GHOST_NDOFManager *GHOST_System::getNDOFManager() const
{
  return m_ndofManager;
}
#endif
