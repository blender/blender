/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include <stdexcept>

#include "GHOST_ISystem.hh"
#include "GHOST_SystemHeadless.hh"

#if defined(WITH_GHOST_X11) && defined(WITH_GHOST_WAYLAND)
#  include "GHOST_SystemWayland.hh"
#  include "GHOST_SystemX11.hh"
#elif defined(WITH_GHOST_X11)
#  include "GHOST_SystemX11.hh"
#elif defined(WITH_GHOST_WAYLAND)
#  include "GHOST_SystemWayland.hh"
#elif defined(WITH_GHOST_SDL)
#  include "GHOST_SystemSDL.hh"
#elif defined(WIN32)
#  include "GHOST_SystemWin32.hh"
#elif defined(__APPLE__)
#  include "GHOST_SystemCocoa.hh"
#endif

GHOST_ISystem *GHOST_ISystem::m_system = nullptr;
const char *GHOST_ISystem::m_system_backend_id = nullptr;

GHOST_TBacktraceFn GHOST_ISystem::m_backtrace_fn = nullptr;

GHOST_TSuccess GHOST_ISystem::createSystem(bool verbose, [[maybe_unused]] bool background)
{
  /* When GHOST fails to start, report the back-ends that were attempted.
   * A Verbose argument could be supported in printing isn't always desired. */
  const char *backends_attempted[8] = {nullptr};
  int backends_attempted_num = 0;

  GHOST_TSuccess success;
  if (!m_system) {

#if defined(WITH_HEADLESS)
    /* Pass. */
#elif defined(WITH_GHOST_WAYLAND)
#  if defined(WITH_GHOST_WAYLAND_DYNLOAD)
    const bool has_wayland_libraries = ghost_wl_dynload_libraries_init();
#  else
    const bool has_wayland_libraries = true;
#  endif
#endif

#if defined(WITH_HEADLESS)
    /* Pass. */
#elif defined(WITH_GHOST_X11) && defined(WITH_GHOST_WAYLAND)
    /* Special case, try Wayland, fall back to X11. */
    if (has_wayland_libraries) {
      backends_attempted[backends_attempted_num++] = "WAYLAND";
      try {
        m_system = new GHOST_SystemWayland(background);
      }
      catch (const std::runtime_error &) {
        delete m_system;
        m_system = nullptr;
#  ifdef WITH_GHOST_WAYLAND_DYNLOAD
        ghost_wl_dynload_libraries_exit();
#  endif
      }
    }
    else {
      m_system = nullptr;
    }

    if (!m_system) {
      /* Try to fallback to X11. */
      backends_attempted[backends_attempted_num++] = "X11";
      try {
        m_system = new GHOST_SystemX11();
      }
      catch (const std::runtime_error &) {
        delete m_system;
        m_system = nullptr;
      }
    }
#elif defined(WITH_GHOST_X11)
    backends_attempted[backends_attempted_num++] = "X11";
    try {
      m_system = new GHOST_SystemX11();
    }
    catch (const std::runtime_error &) {
      delete m_system;
      m_system = nullptr;
    }
#elif defined(WITH_GHOST_WAYLAND)
    if (has_wayland_libraries) {
      backends_attempted[backends_attempted_num++] = "WAYLAND";
      try {
        m_system = new GHOST_SystemWayland(background);
      }
      catch (const std::runtime_error &) {
        delete m_system;
        m_system = nullptr;
#  ifdef WITH_GHOST_WAYLAND_DYNLOAD
        ghost_wl_dynload_libraries_exit();
#  endif
      }
    }
    else {
      m_system = nullptr;
    }
#elif defined(WITH_GHOST_SDL)
    backends_attempted[backends_attempted_num++] = "SDL";
    try {
      m_system = new GHOST_SystemSDL();
    }
    catch (const std::runtime_error &) {
      delete m_system;
      m_system = nullptr;
    }
#elif defined(WIN32)
    backends_attempted[backends_attempted_num++] = "WIN32";
    m_system = new GHOST_SystemWin32();
#elif defined(__APPLE__)
    backends_attempted[backends_attempted_num++] = "COCOA";
    m_system = new GHOST_SystemCocoa();
#endif

    if (m_system) {
      m_system_backend_id = backends_attempted[backends_attempted_num - 1];
    }
    else if (verbose) {
      fprintf(stderr, "GHOST: failed to initialize display for back-end(s): [");
      for (int i = 0; i < backends_attempted_num; i++) {
        if (i != 0) {
          fprintf(stderr, ", ");
        }
        fprintf(stderr, "'%s'", backends_attempted[i]);
      }
      fprintf(stderr, "]\n");
    }

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
    success = createSystem(false, true);
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

const char *GHOST_ISystem::getSystemBackend()
{
  return m_system_backend_id;
}

GHOST_TBacktraceFn GHOST_ISystem::getBacktraceFn()
{
  return GHOST_ISystem::m_backtrace_fn;
}

void GHOST_ISystem::setBacktraceFn(GHOST_TBacktraceFn backtrace_fn)
{
  GHOST_ISystem::m_backtrace_fn = backtrace_fn;
}
