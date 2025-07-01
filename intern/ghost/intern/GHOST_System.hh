/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_System class.
 */

#pragma once

#include "GHOST_ISystem.hh"

#include "GHOST_Buttons.hh"
#include "GHOST_Debug.hh"
#include "GHOST_EventManager.hh"
#include "GHOST_ModifierKeys.hh"
#ifdef WITH_GHOST_DEBUG
#  include "GHOST_EventPrinter.hh"
#endif  // WITH_GHOST_DEBUG

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
  ~GHOST_System() override;

 public:
  /***************************************************************************************
   * Time(r) functionality
   ***************************************************************************************/

  /** \copydoc #GHOST_ISystem::installTimer */
  GHOST_ITimerTask *installTimer(uint64_t delay,
                                 uint64_t interval,
                                 GHOST_TimerProcPtr timerProc,
                                 GHOST_TUserDataPtr userData = nullptr) override;
  /** \copydoc #GHOST_ISystem::removeTimer */
  GHOST_TSuccess removeTimer(GHOST_ITimerTask *timerTask) override;

  /***************************************************************************************
   * Display/window management functionality
   ***************************************************************************************/

  /** \copydoc #GHOST_ISystem::disposeWindow */
  GHOST_TSuccess disposeWindow(GHOST_IWindow *window) override;

  /** \copydoc #GHOST_ISystem::createOffscreenContext */
  GHOST_IContext *createOffscreenContext(GHOST_GPUSettings gpuSettings) override = 0;

  /** \copydoc #GHOST_ISystem::validWindow */
  bool validWindow(GHOST_IWindow *window) override;

  /** \copydoc #GHOST_ISystem::useNativePixel */
  bool useNativePixel() override;
  bool m_nativePixel;

  /** \copydoc #GHOST_ISystem::useWindowFocus */
  void useWindowFocus(const bool use_focus) override;

  bool m_windowFocus;

  /** \copydoc #GHOST_ISystem::setAutoFocus */
  void setAutoFocus(const bool auto_focus) override;
  bool m_autoFocus;

  /** \copydoc #GHOST_ISystem::getWindowUnderCursor */
  GHOST_IWindow *getWindowUnderCursor(int32_t x, int32_t y) override;

  /***************************************************************************************
   * Event management functionality
   ***************************************************************************************/

  /**
   * Inherited from GHOST_ISystem but left pure virtual
   *
   * <pre>
   * virtual bool processEvents(bool waitForEvent) = 0;
   * </pre>
   */

  /** \copydoc #GHOST_ISystem::dispatchEvents */
  void dispatchEvents() override;
  /** \copydoc #GHOST_ISystem::addEventConsumer */
  GHOST_TSuccess addEventConsumer(GHOST_IEventConsumer *consumer) override;
  /** \copydoc #GHOST_ISystem::removeEventConsumer */
  GHOST_TSuccess removeEventConsumer(GHOST_IEventConsumer *consumer) override;

  /***************************************************************************************
   * Cursor management functionality
   ***************************************************************************************/

  /* Client relative functions use a default implementation
   * that converts from screen-coordinates to client coordinates.
   * Implementations may override. */

  /** \copydoc #GHOST_ISystem::getCursorPositionClientRelative */
  GHOST_TSuccess getCursorPositionClientRelative(const GHOST_IWindow *window,
                                                 int32_t &x,
                                                 int32_t &y) const override;
  /** \copydoc #GHOST_ISystem::setCursorPositionClientRelative */
  GHOST_TSuccess setCursorPositionClientRelative(GHOST_IWindow *window,
                                                 int32_t x,
                                                 int32_t y) override;

  /** \copydoc #GHOST_ISystem::getCursorPreferredLogicalSize */
  uint32_t getCursorPreferredLogicalSize() const override;

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

  /** \copydoc #GHOST_ISystem::getModifierKeyState */
  GHOST_TSuccess getModifierKeyState(GHOST_TModifierKey mask, bool &isDown) const override;

  /** \copydoc #GHOST_ISystem::getButtonState */
  GHOST_TSuccess getButtonState(GHOST_TButton mask, bool &isDown) const override;

  /** \copydoc #GHOST_ISystem::setMultitouchGestures */
  void setMultitouchGestures(const bool use) override;

  /** \copydoc #GHOST_ISystem::setTabletAPI */
  void setTabletAPI(GHOST_TTabletAPI api) override;
  GHOST_TTabletAPI getTabletAPI();

  /** \copydoc #GHOST_ISystem::getPixelAtCursor */
  GHOST_TSuccess getPixelAtCursor(float r_color[3]) const override;

#ifdef WITH_INPUT_NDOF
  /***************************************************************************************
   * Access to 3D mouse.
   ***************************************************************************************/

  /** \copydoc #GHOST_ISystem::setNDOFDeadZone */
  void setNDOFDeadZone(float deadzone) override;
#endif

  /***************************************************************************************
   * Access to the Clipboard
   ***************************************************************************************/

  /** \copydoc #GHOST_ISystem::getClipboard */
  char *getClipboard(bool selection) const override = 0;
  /** \copydoc #GHOST_ISystem::putClipboard */
  void putClipboard(const char *buffer, bool selection) const override = 0;
  /** \copydoc #GHOST_ISystem::hasClipboardImage */
  GHOST_TSuccess hasClipboardImage() const override;
  /** \copydoc #GHOST_ISystem::getClipboardImage */
  uint *getClipboardImage(int *r_width, int *r_height) const override;
  /** \copydoc #GHOST_ISystem::putClipboardImage */
  GHOST_TSuccess putClipboardImage(uint *rgba, int width, int height) const override;

  /** \copydoc #GHOST_ISystem::showMessageBox */
  GHOST_TSuccess showMessageBox(const char * /*title*/,
                                const char * /*message*/,
                                const char * /*help_label*/,
                                const char * /*continue_label*/,
                                const char * /*link*/,
                                GHOST_DialogOptions /*dialog_options*/) const override
  {
    return GHOST_kFailure;
  };

  /***************************************************************************************
   * Other (internal) functionality.
   ***************************************************************************************/

  /**
   * Pushes an event on the stack.
   * To dispatch it, call dispatchEvent() or dispatchEvents().
   * Do not delete the event!
   * \param event: The event to push on the stack.
   */
  GHOST_TSuccess pushEvent(const GHOST_IEvent *event);

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

  /***************************************************************************************
   * Debugging
   ***************************************************************************************/

  /** \copydoc #GHOST_ISystem::initDebug */
  void initDebug(GHOST_Debug debug) override;

  /** \copydoc #GHOST_ISystem::isDebugEnabled */
  bool isDebugEnabled() override;

 protected:
  /** \copydoc #GHOST_ISystem::init */
  GHOST_TSuccess init() override;
  /** \copydoc #GHOST_ISystem::exit */
  GHOST_TSuccess exit() override;

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

#ifdef WITH_GHOST_DEBUG
  /** Prints all the events. */
  GHOST_EventPrinter *m_eventPrinter;
#endif  // WITH_GHOST_DEBUG

  /** Use multi-touch gestures. */
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
