/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief COM helper functions for windows
 */

#ifndef _WIN32
#  error "This include is for Windows only!"
#endif

#include "BLI_sys_types.h"

#define WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#  define NOMINMAX
#  include <windows.h>
#  undef NOMINMAX
#else
#  include <windows.h>
#endif

namespace blender {
class CoInitializeWrapper {
  HRESULT _hr;

 public:
  CoInitializeWrapper(DWORD flags)
  {
    _hr = CoInitializeEx(nullptr, flags);
  }
  ~CoInitializeWrapper()
  {
    if (SUCCEEDED(_hr)) {
      CoUninitialize();
    }
  }
  operator HRESULT()
  {
    return _hr;
  }
};
}  // namespace blender
