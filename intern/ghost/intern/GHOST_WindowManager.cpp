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

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_WindowManager.h"
#include <algorithm>
#include "GHOST_Debug.h"
#include "GHOST_Window.h"

GHOST_WindowManager::GHOST_WindowManager()
    : m_fullScreenWindow(0), m_activeWindow(0), m_activeWindowBeforeFullScreen(0)
{
}

GHOST_WindowManager::~GHOST_WindowManager()
{
  /* m_windows is freed by GHOST_System::disposeWindow */
}

GHOST_TSuccess GHOST_WindowManager::addWindow(GHOST_IWindow *window)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (window) {
    if (!getWindowFound(window)) {
      // Store the pointer to the window
      m_windows.push_back(window);
      success = GHOST_kSuccess;
    }
  }
  return success;
}

GHOST_TSuccess GHOST_WindowManager::removeWindow(const GHOST_IWindow *window)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (window) {
    if (window == m_fullScreenWindow) {
      endFullScreen();
    }
    else {
      std::vector<GHOST_IWindow *>::iterator result = find(
          m_windows.begin(), m_windows.end(), window);
      if (result != m_windows.end()) {
        setWindowInactive(window);
        m_windows.erase(result);
        success = GHOST_kSuccess;
      }
    }
  }
  return success;
}

bool GHOST_WindowManager::getWindowFound(const GHOST_IWindow *window) const
{
  bool found = false;
  if (window) {
    if (getFullScreen() && (window == m_fullScreenWindow)) {
      found = true;
    }
    else {
      std::vector<GHOST_IWindow *>::const_iterator result = find(
          m_windows.begin(), m_windows.end(), window);
      if (result != m_windows.end()) {
        found = true;
      }
    }
  }
  return found;
}

bool GHOST_WindowManager::getFullScreen(void) const
{
  return m_fullScreenWindow != NULL;
}

GHOST_IWindow *GHOST_WindowManager::getFullScreenWindow(void) const
{
  return m_fullScreenWindow;
}

GHOST_TSuccess GHOST_WindowManager::beginFullScreen(GHOST_IWindow *window, bool /*stereoVisual*/)
{
  GHOST_TSuccess success = GHOST_kFailure;
  GHOST_ASSERT(window, "GHOST_WindowManager::beginFullScreen(): invalid window");
  GHOST_ASSERT(window->getValid(), "GHOST_WindowManager::beginFullScreen(): invalid window");
  if (!getFullScreen()) {
    m_fullScreenWindow = window;
    m_activeWindowBeforeFullScreen = getActiveWindow();
    setActiveWindow(m_fullScreenWindow);
    m_fullScreenWindow->beginFullScreen();
    success = GHOST_kSuccess;
  }
  return success;
}

GHOST_TSuccess GHOST_WindowManager::endFullScreen(void)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (getFullScreen()) {
    if (m_fullScreenWindow != NULL) {
      // GHOST_PRINT("GHOST_WindowManager::endFullScreen(): deleting full-screen window\n");
      setWindowInactive(m_fullScreenWindow);
      m_fullScreenWindow->endFullScreen();
      delete m_fullScreenWindow;
      // GHOST_PRINT("GHOST_WindowManager::endFullScreen(): done\n");
      m_fullScreenWindow = NULL;
      if (m_activeWindowBeforeFullScreen) {
        setActiveWindow(m_activeWindowBeforeFullScreen);
      }
    }
    success = GHOST_kSuccess;
  }
  return success;
}

GHOST_TSuccess GHOST_WindowManager::setActiveWindow(GHOST_IWindow *window)
{
  GHOST_TSuccess success = GHOST_kSuccess;
  if (window != m_activeWindow) {
    if (getWindowFound(window)) {
      m_activeWindow = window;
    }
    else {
      success = GHOST_kFailure;
    }
  }
  return success;
}

GHOST_IWindow *GHOST_WindowManager::getActiveWindow(void) const
{
  return m_activeWindow;
}

void GHOST_WindowManager::setWindowInactive(const GHOST_IWindow *window)
{
  if (window == m_activeWindow) {
    m_activeWindow = NULL;
  }
}

std::vector<GHOST_IWindow *> &GHOST_WindowManager::getWindows()
{
  return m_windows;
}

GHOST_IWindow *GHOST_WindowManager::getWindowAssociatedWithOSWindow(void *osWindow)
{
  std::vector<GHOST_IWindow *>::iterator iter;

  for (iter = m_windows.begin(); iter != m_windows.end(); ++iter) {
    if ((*iter)->getOSWindow() == osWindow)
      return *iter;
  }

  return NULL;
}

bool GHOST_WindowManager::getAnyModifiedState()
{
  bool isAnyModified = false;
  std::vector<GHOST_IWindow *>::iterator iter;

  for (iter = m_windows.begin(); iter != m_windows.end(); ++iter) {
    if ((*iter)->getModifiedState())
      isAnyModified = true;
  }

  return isAnyModified;
}
