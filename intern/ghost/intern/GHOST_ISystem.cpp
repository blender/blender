/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include <stdexcept>

#include "GHOST_ISystem.h"
#include "GHOST_SystemHeadless.h"

#if defined(WITH_GHOST_X11) && defined(WITH_GHOST_WAYLAND)
#  include "GHOST_SystemWayland.h"
#  include "GHOST_SystemX11.h"
#elif defined(WITH_GHOST_X11)
#  include "GHOST_SystemX11.h"
#elif defined(WITH_GHOST_WAYLAND)
#  include "GHOST_SystemWayland.h"
#elif defined(WITH_GHOST_SDL)
#  include "GHOST_SystemSDL.h"
#elif defined(WIN32)
#  include "GHOST_SystemWin32.h"
#elif defined(__APPLE__)
#  include "GHOST_SystemCocoa.h"
#endif

GHOST_ISystem *GHOST_ISystem::m_system = nullptr;

GHOST_TBacktraceFn GHOST_ISystem::m_backtrace_fn = nullptr;

GHOST_TSuccess GHOST_ISystem::createSystem()
{
  GHOST_TSuccess success;
  if (!m_system) {

#if defined(WITH_HEADLESS)
    /* Pass. */
#elif defined(WITH_GHOST_WAYLAND)
#  if defined(WITH_GHOST_WAYLAND_DYNLOAD)
    const bool has_wayland_libraries = ghost_wl_dynload_libraries();
#  else
    const bool has_wayland_libraries = true;
#  endif
#endif

#if defined(WITH_HEADLESS)
    /* Pass. */
#elif defined(WITH_GHOST_X11) && defined(WITH_GHOST_WAYLAND)
    /* Special case, try Wayland, fall back to X11. */
    try {
      m_system = has_wayland_libraries ? new GHOST_SystemWayland() : nullptr;
    }
    catch (const std::runtime_error &) {
      delete m_system;
      m_system = nullptr;
    }
    if (!m_system) {
      /* Try to fallback to X11. */
      try {
        m_system = new GHOST_SystemX11();
      }
      catch (const std::runtime_error &) {
        delete m_system;
        m_system = nullptr;
      }
    }
#elif defined(WITH_GHOST_X11)
    try {
      m_system = new GHOST_SystemX11();
    }
    catch (const std::runtime_error &) {
      delete m_system;
      m_system = nullptr;
    }
#elif defined(WITH_GHOST_WAYLAND)
    try {
      m_system = has_wayland_libraries ? new GHOST_SystemWayland() : nullptr;
    }
    catch (const std::runtime_error &) {
      delete m_system;
      m_system = nullptr;
    }
#elif defined(WITH_GHOST_SDL)
    try {
      m_system = new GHOST_SystemSDL();
    }
    catch (const std::runtime_error &) {
      delete m_system;
      m_system = nullptr;
    }
#elif defined(WIN32)
    m_system = new GHOST_SystemWin32();
#elif defined(__APPLE__)
    m_system = new GHOST_SystemCocoa();
#endif
    success = m_system != nullptr ? GHOST_kSuccess : GHOST_kFailure;
  }
  else {
    success = GHOST_kFailure;
  }
  if (success) {
    success = m_system->init();
  }
  return success;
}

GHOST_TSuccess GHOST_ISystem::createSystemBackground()
{
  GHOST_TSuccess success;
  if (!m_system) {
#if !defined(WITH_HEADLESS)
    /* Try to create a off-screen render surface with the graphical systems. */
    success = createSystem();
    if (success) {
      return success;
    }
    /* Try to fallback to headless mode if all else fails. */
#endif
    m_system = new GHOST_SystemHeadless();
    success = m_system != nullptr ? GHOST_kSuccess : GHOST_kFailure;
  }
  else {
    success = GHOST_kFailure;
  }
  if (success) {
    success = m_system->init();
  }
  return success;
}

GHOST_TSuccess GHOST_ISystem::disposeSystem()
{
  GHOST_TSuccess success = GHOST_kSuccess;
  if (m_system) {
    delete m_system;
    m_system = nullptr;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_ISystem *GHOST_ISystem::getSystem()
{
  return m_system;
}

GHOST_TBacktraceFn GHOST_ISystem::getBacktraceFn()
{
  return GHOST_ISystem::m_backtrace_fn;
}

void GHOST_ISystem::setBacktraceFn(GHOST_TBacktraceFn backtrace_fn)
{
  GHOST_ISystem::m_backtrace_fn = backtrace_fn;
}
