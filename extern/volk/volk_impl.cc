/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define VOLK_IMPLEMENTATION
#ifdef _WIN32
#  define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#  define VK_USE_PLATFORM_METAL_EXT
#else
#  define VK_USE_PLATFORM_WAYLAND_KHR
#  define VK_USE_PLATFORM_XLIB_KHR
#endif

/* NOTE: Blender uses device specific symbol tables. To ease development we hide the
 * global device prototypes and move the instance symbols into the volk namespace. With
 * these changes most runtime null reference error becomes a compile time check.
 * 
 * Runtime errors can still happen when an extension isn't enabled or driver doesn't
 * support the extension. */
#define VOLK_NAMESPACE
#define VOLK_NO_GLOBAL_PROTOTYPES

#include "volk.h"
