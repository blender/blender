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

GHOST_WindowManager::GHOST_WindowManager() : active_window_(nullptr) {}

/* windows_ is freed by GHOST_System::disposeWindow */
GHOST_WindowManager::~GHOST_WindowManager() = default;

GHOST_TSuccess GHOST_WindowManager::addWindow(GHOST_IWindow *window)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (window) {
    if (!getWindowFound(window)) {
      /* Store the pointer to the window. */
      windows_.push_back(window);
      success = GHOST_kSuccess;
    }
  }
  return success;
}

GHOST_TSuccess GHOST_WindowManager::removeWindow(const GHOST_IWindow *window)
{
  GHOST_TSuccess success = GHOST_kFailure;
  if (window) {
    std::vector<GHOST_IWindow *>::iterator result = find(windows_.begin(), windows_.end(), window);
    if (result != windows_.end()) {
      setWindowInactive(window);
      windows_.erase(result);
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
        windows_.begin(), windows_.end(), window);
    if (result != windows_.end()) {
      found = true;
    }
  }
  return found;
}

GHOST_TSuccess GHOST_WindowManager::setActiveWindow(GHOST_IWindow *window)
{
  GHOST_TSuccess success = GHOST_kSuccess;
  if (window != active_window_) {
    if (getWindowFound(window)) {
      active_window_ = window;
    }
    else {
      success = GHOST_kFailure;
    }
  }
  return success;
}

GHOST_IWindow *GHOST_WindowManager::getActiveWindow() const
{
  return active_window_;
}

void GHOST_WindowManager::setWindowInactive(const GHOST_IWindow *window)
{
  if (window == active_window_) {
    active_window_ = nullptr;
  }
}

const std::vector<GHOST_IWindow *> &GHOST_WindowManager::getWindows() const
{
  return windows_;
}

GHOST_IWindow *GHOST_WindowManager::getWindowAssociatedWithOSWindow(const void *osWindow)
{
  std::vector<GHOST_IWindow *>::iterator iter;

  for (iter = windows_.begin(); iter != windows_.end(); ++iter) {
    GHOST_Window *window = static_cast<GHOST_Window *>(*iter);
    if (window->getOSWindow() == osWindow) {
      return *iter;
    }
  }
  return nullptr;
}
