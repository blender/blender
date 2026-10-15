/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(_MSC_VER)
#  define API(returnType) extern "C" __declspec(dllexport) returnType __cdecl
#else
#  define API(returnType) extern "C" returnType
#endif

/* VkFormat values used here (from the Vulkan spec). Not including the Vulkan headers to avoid
 * the dependency. */
#define VK_FORMAT_R8G8B8_UNORM 23
#define VK_FORMAT_R8G8B8_SRGB 29
#define VK_FORMAT_R8G8B8A8_UNORM 37
#define VK_FORMAT_R8G8B8A8_SRGB 43
