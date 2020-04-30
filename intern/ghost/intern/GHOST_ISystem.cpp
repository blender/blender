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

#include "GHOST_ISystem.h"

#if defined(WITH_X11) || defined(WITH_GHOST_WAYLAND)
#  ifdef WITH_X11
#    include "GHOST_SystemX11.h"
#  endif
#  ifdef WITH_GHOST_WAYLAND
#    include "GHOST_SystemWayland.h"
#  endif
#else
#  ifdef WITH_HEADLESS
#    include "GHOST_SystemNULL.h"
#  elif defined(WITH_GHOST_SDL)
#    include "GHOST_SystemSDL.h"
#  elif defined(WIN32)
#    include "GHOST_SystemWin32.h"
#  else
#    ifdef __APPLE__
#      include "GHOST_SystemCocoa.h"
#    endif
#  endif
#endif

GHOST_ISystem *GHOST_ISystem::m_system = NULL;

GHOST_TSuccess GHOST_ISystem::createSystem()
{
  GHOST_TSuccess success;
  if (!m_system) {
#if defined(WITH_X11) || defined(WITH_GHOST_WAYLAND)
#  ifdef WITH_GHOST_WAYLAND
    try {
      m_system = new GHOST_SystemWayland();
    }
    catch (const std::exception &) {
    }
#  endif
#  ifdef WITH_X11
    if (!m_system) {
      m_system = new GHOST_SystemX11();
    }
#  endif
#else
#  ifdef WITH_HEADLESS
    m_system = new GHOST_SystemNULL();
#  elif defined(WITH_GHOST_SDL)
    m_system = new GHOST_SystemSDL();
#  elif defined(WIN32)
    m_system = new GHOST_SystemWin32();
#  else
#    ifdef __APPLE__
    m_system = new GHOST_SystemCocoa();
#    endif
#  endif
#endif
    success = m_system != NULL ? GHOST_kSuccess : GHOST_kFailure;
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
    m_system = NULL;
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
