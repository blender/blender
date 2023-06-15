/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "renderdoc_api.hh"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#else
#  include <dlfcn.h>
#endif
#include <iostream>

namespace renderdoc::api {
bool Renderdoc::start_frame_capture(RENDERDOC_DevicePointer device_handle,
                                    RENDERDOC_WindowHandle window_handle)
{
  if (!check_loaded()) {
    return false;
  }
  renderdoc_api_->StartFrameCapture(device_handle, window_handle);
  return true;
}

void Renderdoc::end_frame_capture(RENDERDOC_DevicePointer device_handle,
                                  RENDERDOC_WindowHandle window_handle)

{
  if (!check_loaded()) {
    return;
  }
  renderdoc_api_->EndFrameCapture(device_handle, window_handle);
}

bool Renderdoc::check_loaded()
{
  switch (state_) {
    case State::UNINITIALIZED:
      load();
      return renderdoc_api_ != nullptr;
      break;
    case State::NOT_FOUND:
      return false;
    case State::LOADED:
      return true;
  }
  return false;
}

void Renderdoc::load()
{
#ifdef _WIN32
  if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod,
                                                                           "RENDERDOC_GetAPI");
    RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&renderdoc_api_);
  }
#else
  if (void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
    RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&renderdoc_api_);
  }
#endif
}

}  // namespace renderdoc::api