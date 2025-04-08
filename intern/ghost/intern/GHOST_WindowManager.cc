/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_WindowManager.hh"
#include "GHOST_Debug.hh"
#include "GHOST_Window.hh"
#include <algorithm>

GHOST_WindowManager::GHOST_WindowManager() : m_activeWindow(nullptr) {}

/* m_windows is freed by GHOST_System::disposeWindow */
GHOST_WindowManager::~GHOST_WindowManager() = default;

GHOST_TSuccess GHOST_WindowManager::addWindow(GHOST_IWindow *window)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (window) {
    if (!getWindowFound(window)) {
      /* Store the pointer to the window. */
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
    std::vector<GHOST_IWindow *>::iterator result = find(
        m_windows.begin(), m_windows.end(), window);
    if (result != m_windows.end()) {
      setWindowInactive(window);
      m_windows.erase(result);
      success = GHOST_kSuccess;
    }
  }
  return success;
}

bool GHOST_WindowManager::getWindowFound(const GHOST_IWindow *window) const
{
  bool found = false;
  if (window) {
    std::vector<GHOST_IWindow *>::const_iterator result = find(
        m_windows.begin(), m_windows.end(), window);
    if (result != m_windows.end()) {
      found = true;
    }
  }
  return found;
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

GHOST_IWindow *GHOST_WindowManager::getActiveWindow() const
{
  return m_activeWindow;
}

void GHOST_WindowManager::setWindowInactive(const GHOST_IWindow *window)
{
  if (window == m_activeWindow) {
    m_activeWindow = nullptr;
  }
}

const std::vector<GHOST_IWindow *> &GHOST_WindowManager::getWindows() const
{
  return m_windows;
}

GHOST_IWindow *GHOST_WindowManager::getWindowAssociatedWithOSWindow(const void *osWindow)
{
  std::vector<GHOST_IWindow *>::iterator iter;

  for (iter = m_windows.begin(); iter != m_windows.end(); ++iter) {
    if ((*iter)->getOSWindow() == osWindow) {
      return *iter;
    }
  }
  return nullptr;
}
