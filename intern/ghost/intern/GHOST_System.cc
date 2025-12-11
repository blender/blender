/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_System.hh"

#include "GHOST_EventManager.hh"
#include "GHOST_TimerManager.hh"
#include "GHOST_TimerTask.hh"
#include "GHOST_WindowManager.hh"

#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManager.hh"
#endif

GHOST_System::GHOST_System()
    : native_pixel_(false),
      window_focus_(true),
      auto_focus_(true),
      window_csd_params_({nullptr}),
      window_csd_layout_({0}),
      timer_manager_(nullptr),
      window_manager_(nullptr),
      event_manager_(nullptr),
#ifdef WITH_INPUT_NDOF
      ndof_manager_(nullptr),
#endif
      multitouch_gestures_(true),
      tablet_api_(GHOST_kTabletAutomatic),
      is_debug_enabled_(false)
{
}

GHOST_System::~GHOST_System()
{
  exit();
}

GHOST_TSuccess GHOST_System::hasClipboardImage() const
{
  return GHOST_kFailure;
}

uint *GHOST_System::getClipboardImage(int * /*r_width*/, int * /*r_height*/) const
{
  return nullptr;
}

GHOST_TSuccess GHOST_System::putClipboardImage(uint * /*rgba*/,
                                               int /*width*/,
                                               int /*height*/) const
{
  return GHOST_kFailure;
}

void GHOST_System::setWindowCSD(const GHOST_CSD_Params &params)
{
  window_csd_params_ = params;
}

const GHOST_CSD_Params &GHOST_System::getWindowCSD() const
{
  return window_csd_params_;
}

const GHOST_CSD_Layout &GHOST_System::getWindowCSD_Layout() const
{
  return window_csd_layout_;
}
void GHOST_System::setWindowCSD_Layout(const GHOST_CSD_Layout &layout)
{
  window_csd_layout_ = layout;
}

GHOST_ITimerTask *GHOST_System::installTimer(uint64_t delay,
                                             uint64_t interval,
                                             GHOST_TimerProcPtr timer_proc,
                                             GHOST_TUserDataPtr user_data)
{
  uint64_t millis = getMilliSeconds();
  GHOST_TimerTask *timer = new GHOST_TimerTask(millis + delay, interval, timer_proc, user_data);
  if (timer) {
    if (timer_manager_->addTimer(timer) == GHOST_kSuccess) {
      /* Check to see whether we need to fire the timer right away. */
      timer_manager_->fireTimers(millis);
    }
    else {
      delete timer;
      timer = nullptr;
    }
  }
  return timer;
}

GHOST_TSuccess GHOST_System::removeTimer(GHOST_ITimerTask *timerTask)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (timerTask) {
    success = timer_manager_->removeTimer((GHOST_TimerTask *)timerTask);
  }
  return success;
}

GHOST_TSuccess GHOST_System::disposeWindow(GHOST_IWindow *window)
{
  GHOST_TSuccess success;

  /*
   * Remove all pending events for the window.
   */
  if (window_manager_->getWindowFound(window)) {
    event_manager_->removeWindowEvents(window);
    success = window_manager_->removeWindow(window);
    if (success) {
      delete window;
    }
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

bool GHOST_System::validWindow(GHOST_IWindow *window)
{
  return window_manager_->getWindowFound(window);
}

GHOST_IWindow *GHOST_System::getWindowUnderCursor(int32_t x, int32_t y)
{
  /* TODO: This solution should follow the order of the activated windows (Z-order).
   * It is imperfect but usable in most cases. Ideally each platform should provide
   * a custom version of this function that properly considers z-order. */

  std::vector<GHOST_IWindow *> windows = window_manager_->getWindows();
  std::vector<GHOST_IWindow *>::reverse_iterator iwindow_iter;

  /* Search through the windows in reverse order because in most cases
   * the window that is on top was created after those that are below it. */

  for (iwindow_iter = windows.rbegin(); iwindow_iter != windows.rend(); ++iwindow_iter) {

    GHOST_IWindow *win = *iwindow_iter;

    if (win->getState() == GHOST_kWindowStateMinimized) {
      continue;
    }

    GHOST_Rect bounds;
    win->getClientBounds(bounds);
    if (bounds.isInside(x, y)) {
      return win;
    }
  }

  return nullptr;
}

void GHOST_System::dispatchEvents()
{
#ifdef WITH_INPUT_NDOF
  /* NDOF Motion event is sent only once per dispatch, so do it now: */
  if (ndof_manager_) {
    ndof_manager_->sendMotionEvent();
  }
#endif

  if (event_manager_) {
    event_manager_->dispatchEvents();
  }

  timer_manager_->fireTimers(getMilliSeconds());
}

GHOST_TSuccess GHOST_System::addEventConsumer(GHOST_IEventConsumer *consumer)
{
  GHOST_TSuccess success;
  if (event_manager_) {
    success = event_manager_->addConsumer(consumer);
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_System::removeEventConsumer(GHOST_IEventConsumer *consumer)
{
  GHOST_TSuccess success;
  if (event_manager_) {
    success = event_manager_->removeConsumer(consumer);
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_System::pushEvent(std::unique_ptr<const GHOST_IEvent> event)
{
  GHOST_TSuccess success;
  if (event_manager_) {
    success = event_manager_->pushEvent(std::move(event));
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_System::getCursorPositionClientRelative(const GHOST_IWindow *window,
                                                             int32_t &x,
                                                             int32_t &y) const
{
  /* Sub-classes that can implement this directly should do so. */
  int32_t screen_x, screen_y;
  GHOST_TSuccess success = getCursorPosition(screen_x, screen_y);
  if (success == GHOST_kSuccess) {
    window->screenToClient(screen_x, screen_y, x, y);
  }
  return success;
}

GHOST_TSuccess GHOST_System::setCursorPositionClientRelative(GHOST_IWindow *window,
                                                             int32_t x,
                                                             int32_t y)
{
  /* Sub-classes that can implement this directly should do so. */
  int32_t screen_x, screen_y;
  window->clientToScreen(x, y, screen_x, screen_y);
  return setCursorPosition(screen_x, screen_y);
}

uint32_t GHOST_System::getCursorPreferredLogicalSize() const
{
  return uint32_t(24);
}

GHOST_TSuccess GHOST_System::getModifierKeyState(GHOST_TModifierKey mask, bool &is_down) const
{
  GHOST_ModifierKeys keys;
  /* Get the state of all modifier keys. */
  GHOST_TSuccess success = getModifierKeys(keys);
  if (success) {
    /* Isolate the state of the key requested. */
    is_down = keys.get(mask);
  }
  return success;
}

GHOST_TSuccess GHOST_System::getButtonState(GHOST_TButton mask, bool &is_down) const
{
  GHOST_Buttons buttons;
  /* Get the state of all mouse buttons. */
  GHOST_TSuccess success = getButtons(buttons);
  if (success) {
    /* Isolate the state of the mouse button requested. */
    is_down = buttons.get(mask);
  }
  return success;
}

void GHOST_System::setMultitouchGestures(const bool use)
{
  multitouch_gestures_ = use;
}

void GHOST_System::setTabletAPI(GHOST_TTabletAPI api)
{
  tablet_api_ = api;
}

GHOST_TTabletAPI GHOST_System::getTabletAPI()
{
  return tablet_api_;
}

GHOST_TSuccess GHOST_System::getPixelAtCursor(float /*r_color*/[3]) const
{
  return GHOST_kFailure;
}

#ifdef WITH_INPUT_NDOF
void GHOST_System::setNDOFDeadZone(float deadzone)
{
  if (this->ndof_manager_) {
    this->ndof_manager_->setDeadZone(deadzone);
  }
}
#endif

GHOST_TSuccess GHOST_System::init()
{
  timer_manager_ = new GHOST_TimerManager();
  window_manager_ = new GHOST_WindowManager();
  event_manager_ = new GHOST_EventManager();

#ifdef WITH_GHOST_DEBUG
  if (event_manager_) {
    event_printer_ = new GHOST_EventPrinter();
    event_manager_->addConsumer(event_printer_);
  }
#endif /* WITH_GHOST_DEBUG */

  if (timer_manager_ && window_manager_ && event_manager_) {
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_System::exit()
{
  /** WARNING: exit() may run more than once, since it may need to be called from a derived class
   * destructor. Take it into account when modifying this function. */

  delete window_manager_;
  window_manager_ = nullptr;

  delete timer_manager_;
  timer_manager_ = nullptr;

  delete event_manager_;
  event_manager_ = nullptr;

#ifdef WITH_INPUT_NDOF
  delete ndof_manager_;
  ndof_manager_ = nullptr;
#endif

  return GHOST_kSuccess;
}

bool GHOST_System::useNativePixel()
{
  native_pixel_ = true;
  return true;
}

void GHOST_System::useWindowFocus(const bool use_focus)
{
  window_focus_ = use_focus;
}

void GHOST_System::setAutoFocus(const bool auto_focus)
{
  auto_focus_ = auto_focus;
}

void GHOST_System::initDebug(GHOST_Debug debug)
{
  is_debug_enabled_ = debug.flags & GHOST_kDebugDefault;
}

bool GHOST_System::isDebugEnabled()
{
  return is_debug_enabled_;
}
