/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_ISystem.h"

#if defined(WITH_HEADLESS)
#  include "GHOST_SystemNULL.h"
#elif defined(WITH_GHOST_X11) && defined(WITH_GHOST_WAYLAND)
#  include "GHOST_SystemWayland.h"
#  include "GHOST_SystemX11.h"
#  include <stdexcept>
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

GHOST_TSuccess GHOST_ISystem::createSystem()
{
  GHOST_TSuccess success;
  if (!m_system) {
#if defined(WITH_HEADLESS)
    m_system = new GHOST_SystemNULL();
#elif defined(WITH_GHOST_X11) && defined(WITH_GHOST_WAYLAND)
    /* Special case, try Wayland, fall back to X11. */
    try {
      m_system = new GHOST_SystemWayland();
    }
    catch (const std::runtime_error &) {
      /* fallback to X11. */
      delete m_system;
      m_system = nullptr;
    }
    if (!m_system) {
      m_system = new GHOST_SystemX11();
    }
#elif defined(WITH_GHOST_X11)
    m_system = new GHOST_SystemX11();
#elif defined(WITH_GHOST_WAYLAND)
    m_system = new GHOST_SystemWayland();
#elif defined(WITH_GHOST_SDL)
    m_system = new GHOST_SystemSDL();
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
