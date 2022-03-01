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
 */

#include "GHOST_System.h"

#include <chrono>
#include <stdio.h> /* just for printf */

#include "GHOST_DisplayManager.h"
#include "GHOST_EventManager.h"
#include "GHOST_TimerManager.h"
#include "GHOST_TimerTask.h"
#include "GHOST_WindowManager.h"

#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManager.h"
#endif

GHOST_System::GHOST_System()
    : m_nativePixel(false),
      m_windowFocus(true),
      m_displayManager(NULL),
      m_timerManager(NULL),
      m_windowManager(NULL),
      m_eventManager(NULL),
#ifdef WITH_INPUT_NDOF
      m_ndofManager(0),
#endif
      m_tabletAPI(GHOST_kTabletAutomatic),
      m_is_debug_enabled(false)
{
}

GHOST_System::~GHOST_System()
{
  exit();
}

uint64_t GHOST_System::getMilliSeconds() const
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

GHOST_ITimerTask *GHOST_System::installTimer(uint64_t delay,
                                             uint64_t interval,
                                             GHOST_TimerProcPtr timerProc,
                                             GHOST_TUserDataPtr userData)
{
  uint64_t millis = getMilliSeconds();
  GHOST_TimerTask *timer = new GHOST_TimerTask(millis + delay, interval, timerProc, userData);
  if (timer) {
    if (m_timerManager->addTimer(timer) == GHOST_kSuccess) {
      /* Check to see whether we need to fire the timer right away. */
      m_timerManager->fireTimers(millis);
    }
    else {
      delete timer;
      timer = NULL;
    }
  }
  return timer;
}

GHOST_TSuccess GHOST_System::removeTimer(GHOST_ITimerTask *timerTask)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (timerTask) {
    success = m_timerManager->removeTimer((GHOST_TimerTask *)timerTask);
  }
  return success;
}

GHOST_TSuccess GHOST_System::disposeWindow(GHOST_IWindow *window)
{
  GHOST_TSuccess success;

  /*
   * Remove all pending events for the window.
   */
  if (m_windowManager->getWindowFound(window)) {
    m_eventManager->removeWindowEvents(window);
  }
  if (window == m_windowManager->getFullScreenWindow()) {
    success = endFullScreen();
  }
  else {
    if (m_windowManager->getWindowFound(window)) {
      success = m_windowManager->removeWindow(window);
      if (success) {
        delete window;
      }
    }
    else {
      success = GHOST_kFailure;
    }
  }
  return success;
}

bool GHOST_System::validWindow(GHOST_IWindow *window)
{
  return m_windowManager->getWindowFound(window);
}

GHOST_TSuccess GHOST_System::beginFullScreen(const GHOST_DisplaySetting &setting,
                                             GHOST_IWindow **window,
                                             const bool stereoVisual,
                                             const bool alphaBackground)
{
  GHOST_TSuccess success = GHOST_kFailure;
  GHOST_ASSERT(m_windowManager, "GHOST_System::beginFullScreen(): invalid window manager");
  if (m_displayManager) {
    if (!m_windowManager->getFullScreen()) {
      m_displayManager->getCurrentDisplaySetting(GHOST_DisplayManager::kMainDisplay,
                                                 m_preFullScreenSetting);

      // GHOST_PRINT("GHOST_System::beginFullScreen(): activating new display settings\n");
      success = m_displayManager->setCurrentDisplaySetting(GHOST_DisplayManager::kMainDisplay,
                                                           setting);
      if (success == GHOST_kSuccess) {
        // GHOST_PRINT("GHOST_System::beginFullScreen(): creating full-screen window\n");
        success = createFullScreenWindow(
            (GHOST_Window **)window, setting, stereoVisual, alphaBackground);
        if (success == GHOST_kSuccess) {
          m_windowManager->beginFullScreen(*window, stereoVisual);
        }
        else {
          m_displayManager->setCurrentDisplaySetting(GHOST_DisplayManager::kMainDisplay,
                                                     m_preFullScreenSetting);
        }
      }
    }
  }
  if (success == GHOST_kFailure) {
    GHOST_PRINT("GHOST_System::beginFullScreen(): could not enter full-screen mode\n");
  }
  return success;
}

GHOST_TSuccess GHOST_System::updateFullScreen(const GHOST_DisplaySetting &setting,
                                              GHOST_IWindow ** /*window*/)
{
  GHOST_TSuccess success = GHOST_kFailure;
  GHOST_ASSERT(m_windowManager, "GHOST_System::updateFullScreen(): invalid window manager");
  if (m_displayManager) {
    if (m_windowManager->getFullScreen()) {
      success = m_displayManager->setCurrentDisplaySetting(GHOST_DisplayManager::kMainDisplay,
                                                           setting);
    }
  }

  return success;
}

GHOST_TSuccess GHOST_System::endFullScreen(void)
{
  GHOST_TSuccess success = GHOST_kFailure;
  GHOST_ASSERT(m_windowManager, "GHOST_System::endFullScreen(): invalid window manager");
  if (m_windowManager->getFullScreen()) {
    // GHOST_IWindow* window = m_windowManager->getFullScreenWindow();
    // GHOST_PRINT("GHOST_System::endFullScreen(): leaving window manager full-screen mode\n");
    success = m_windowManager->endFullScreen();
    GHOST_ASSERT(m_displayManager, "GHOST_System::endFullScreen(): invalid display manager");
    // GHOST_PRINT("GHOST_System::endFullScreen(): leaving full-screen mode\n");
    success = m_displayManager->setCurrentDisplaySetting(GHOST_DisplayManager::kMainDisplay,
                                                         m_preFullScreenSetting);
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

bool GHOST_System::getFullScreen(void)
{
  bool fullScreen;
  if (m_windowManager) {
    fullScreen = m_windowManager->getFullScreen();
  }
  else {
    fullScreen = false;
  }
  return fullScreen;
}

GHOST_IWindow *GHOST_System::getWindowUnderCursor(int32_t x, int32_t y)
{
  /* TODO: This solution should follow the order of the activated windows (Z-order).
   * It is imperfect but usable in most cases. */
  for (GHOST_IWindow *iwindow : m_windowManager->getWindows()) {
    if (iwindow->getState() == GHOST_kWindowStateMinimized) {
      continue;
    }

    GHOST_Rect bounds;
    iwindow->getClientBounds(bounds);
    if (bounds.isInside(x, y)) {
      return iwindow;
    }
  }

  return NULL;
}

void GHOST_System::dispatchEvents()
{
#ifdef WITH_INPUT_NDOF
  /* NDOF Motion event is sent only once per dispatch, so do it now: */
  if (m_ndofManager) {
    m_ndofManager->sendMotionEvent();
  }
#endif

  if (m_eventManager) {
    m_eventManager->dispatchEvents();
  }

  m_timerManager->fireTimers(getMilliSeconds());
}

GHOST_TSuccess GHOST_System::addEventConsumer(GHOST_IEventConsumer *consumer)
{
  GHOST_TSuccess success;
  if (m_eventManager) {
    success = m_eventManager->addConsumer(consumer);
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_System::removeEventConsumer(GHOST_IEventConsumer *consumer)
{
  GHOST_TSuccess success;
  if (m_eventManager) {
    success = m_eventManager->removeConsumer(consumer);
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_System::pushEvent(GHOST_IEvent *event)
{
  GHOST_TSuccess success;
  if (m_eventManager) {
    success = m_eventManager->pushEvent(event);
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_System::getModifierKeyState(GHOST_TModifierKeyMask mask, bool &isDown) const
{
  GHOST_ModifierKeys keys;
  /* Get the state of all modifier keys. */
  GHOST_TSuccess success = getModifierKeys(keys);
  if (success) {
    /* Isolate the state of the key requested. */
    isDown = keys.get(mask);
  }
  return success;
}

GHOST_TSuccess GHOST_System::getButtonState(GHOST_TButtonMask mask, bool &isDown) const
{
  GHOST_Buttons buttons;
  /* Get the state of all mouse buttons. */
  GHOST_TSuccess success = getButtons(buttons);
  if (success) {
    /* Isolate the state of the mouse button requested. */
    isDown = buttons.get(mask);
  }
  return success;
}

void GHOST_System::setTabletAPI(GHOST_TTabletAPI api)
{
  m_tabletAPI = api;
}

GHOST_TTabletAPI GHOST_System::getTabletAPI(void)
{
  return m_tabletAPI;
}

#ifdef WITH_INPUT_NDOF
void GHOST_System::setNDOFDeadZone(float deadzone)
{
  if (this->m_ndofManager) {
    this->m_ndofManager->setDeadZone(deadzone);
  }
}
#endif

GHOST_TSuccess GHOST_System::init()
{
  m_timerManager = new GHOST_TimerManager();
  m_windowManager = new GHOST_WindowManager();
  m_eventManager = new GHOST_EventManager();

#ifdef WITH_GHOST_DEBUG
  if (m_eventManager) {
    m_eventPrinter = new GHOST_EventPrinter();
    m_eventManager->addConsumer(m_eventPrinter);
  }
#endif /* WITH_GHOST_DEBUG */

  if (m_timerManager && m_windowManager && m_eventManager) {
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_System::exit()
{
  if (getFullScreen()) {
    endFullScreen();
  }

  delete m_displayManager;
  m_displayManager = NULL;

  delete m_windowManager;
  m_windowManager = NULL;

  delete m_timerManager;
  m_timerManager = NULL;

  delete m_eventManager;
  m_eventManager = NULL;

#ifdef WITH_INPUT_NDOF
  delete m_ndofManager;
  m_ndofManager = NULL;
#endif

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_System::createFullScreenWindow(GHOST_Window **window,
                                                    const GHOST_DisplaySetting &settings,
                                                    const bool stereoVisual,
                                                    const bool alphaBackground)
{
  GHOST_GLSettings glSettings = {0};

  if (stereoVisual)
    glSettings.flags |= GHOST_glStereoVisual;
  if (alphaBackground)
    glSettings.flags |= GHOST_glAlphaBackground;

  /* NOTE: don't use #getCurrentDisplaySetting() because on X11 we may
   * be zoomed in and the desktop may be bigger than the viewport. */
  GHOST_ASSERT(m_displayManager,
               "GHOST_System::createFullScreenWindow(): invalid display manager");
  // GHOST_PRINT("GHOST_System::createFullScreenWindow(): creating full-screen window\n");
  *window = (GHOST_Window *)createWindow("",
                                         0,
                                         0,
                                         settings.xPixels,
                                         settings.yPixels,
                                         GHOST_kWindowStateNormal,
                                         GHOST_kDrawingContextTypeOpenGL,
                                         glSettings,
                                         true /* exclusive */);
  return (*window == NULL) ? GHOST_kFailure : GHOST_kSuccess;
}

bool GHOST_System::useNativePixel(void)
{
  m_nativePixel = true;
  return 1;
}

void GHOST_System::useWindowFocus(const bool use_focus)
{
  m_windowFocus = use_focus;
}

void GHOST_System::initDebug(bool is_debug_enabled)
{
  m_is_debug_enabled = is_debug_enabled;
}

bool GHOST_System::isDebugEnabled()
{
  return m_is_debug_enabled;
}
