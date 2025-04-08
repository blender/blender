/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WindowManager class.
 */

#pragma once

#include <vector>

#include "GHOST_IWindow.hh"

/**
 * Manages system windows (platform independent implementation).
 */
class GHOST_WindowManager {
 public:
  /**
   * Constructor.
   */
  GHOST_WindowManager();

  /**
   * Destructor.
   */
  ~GHOST_WindowManager();

  /**
   * Add a window to our list.
   * It is only added if it is not already in the list.
   * \param window: Pointer to the window to be added.
   * \return Indication of success.
   */
  GHOST_TSuccess addWindow(GHOST_IWindow *window);

  /**
   * Remove a window from our list.
   * \param window: Pointer to the window to be removed.
   * \return Indication of success.
   */
  GHOST_TSuccess removeWindow(const GHOST_IWindow *window);

  /**
   * Returns whether the window is in our list.
   * \param window: Pointer to the window to query.
   * \return A boolean indicator.
   */
  bool getWindowFound(const GHOST_IWindow *window) const;

  /**
   * Sets new window as active window (the window receiving events).
   * There can be only one window active which should be in the current window list.
   * \param window: The new active window.
   */
  GHOST_TSuccess setActiveWindow(GHOST_IWindow *window);

  /**
   * Returns the active window (the window receiving events).
   * There can be only one window active which should be in the current window list.
   * \return window The active window (or nullptr if there is none).
   */
  GHOST_IWindow *getActiveWindow() const;

  /**
   * Set this window to be inactive (not receiving events).
   * \param window: The window to deactivate.
   */
  void setWindowInactive(const GHOST_IWindow *window);

  /**
   * Return a vector of the windows currently managed by this
   * class.
   * \return Constant reference to the vector of windows managed
   */
  const std::vector<GHOST_IWindow *> &getWindows() const;

  /**
   * Finds the window associated with an OS window object/handle.
   * \param osWindow: The OS window object/handle.
   * \return The associated window, null if none corresponds.
   */
  GHOST_IWindow *getWindowAssociatedWithOSWindow(const void *osWindow);

 protected:
  /** The list of windows managed */
  std::vector<GHOST_IWindow *> m_windows;

  /** The active window. */
  GHOST_IWindow *m_activeWindow;

  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_WindowManager")
};
