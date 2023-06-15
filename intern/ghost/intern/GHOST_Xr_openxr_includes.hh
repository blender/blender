/* SPDX-FileCopyrightText: 2012-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * \note This is taken mostly from the OpenXR SDK, but with modified D3D versions (e.g. d3d11_4.h
 * -> d3d11.h). Take care for that when updating, we don't want to require newest Win SDKs to be
 * installed.
 */

#pragma once

/* Platform headers */
#ifdef XR_USE_PLATFORM_WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

/* Graphics headers */
#ifdef XR_USE_GRAPHICS_API_D3D10
#  include <d3d10_1.h>
#endif
#ifdef XR_USE_GRAPHICS_API_D3D11
#  include <d3d11.h>
#endif
#ifdef XR_USE_GRAPHICS_API_D3D12
#  include <d3d12.h>
#endif
#ifdef WITH_GHOST_X11
#  include <epoxy/egl.h>
#  include <epoxy/glx.h>
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
