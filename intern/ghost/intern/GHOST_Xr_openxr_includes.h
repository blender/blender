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
 */

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
#  include <GL/glxew.h>
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
