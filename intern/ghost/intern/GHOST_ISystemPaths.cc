/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_ISystemPaths.hh"

#ifdef WIN32
#  include "GHOST_SystemPathsWin32.hh"
#else
#  ifdef __APPLE__
#    include "GHOST_SystemPathsCocoa.hh"
#  else
#    include "GHOST_SystemPathsUnix.hh"
#  endif
#endif

GHOST_ISystemPaths *GHOST_ISystemPaths::system_paths_ = nullptr;

GHOST_TSuccess GHOST_ISystemPaths::create()
{
  GHOST_TSuccess success;
  if (!system_paths_) {
#ifdef WIN32
    system_paths_ = new GHOST_SystemPathsWin32();
#else
#  ifdef __APPLE__
    system_paths_ = new GHOST_SystemPathsCocoa();
#  else
    system_paths_ = new GHOST_SystemPathsUnix();
#  endif
#endif
    success = system_paths_ != nullptr ? GHOST_kSuccess : GHOST_kFailure;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_ISystemPaths::dispose()
{
  GHOST_TSuccess success = GHOST_kSuccess;
  if (system_paths_) {
    delete system_paths_;
    system_paths_ = nullptr;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_ISystemPaths *GHOST_ISystemPaths::get()
{
  if (!system_paths_) {
    create();
  }
  return system_paths_;
}
