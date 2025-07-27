/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** -- Metal backend implementation options. -- */
/* Whether we are building for an ARM or x86 platform. */
#ifdef __aarch64__
#  define METAL_PLATFORM_ARM 1
#else
#  define METAL_PLATFORM_ARM 0
#endif

/* Whether we are building for a portable platform. */
#ifdef WITH_APPLE_CROSSPLATFORM
#  define MTL_BACKEND_DESKTOP 0
#else
#  define MTL_BACKEND_DESKTOP 1
#endif

/* Flag to skip backend compatibility checks. */
#define MTL_BACKEND_ALWAYS_SUPPORTED (METAL_PLATFORM_ARM)
/* Whether we can optionally enable the low-powered iGPU on x86 platforms. */
#define MTL_BACKEND_LOW_POWER_GPU_SUPPORT (!METAL_PLATFORM_ARM)
/* Whether platform should compile managed buffer support. */
#define MTL_BACKEND_SUPPORTS_MANAGED_BUFFERS (!METAL_PLATFORM_ARM)
/* Whether platform should compile render target barrier support. */
#define MTL_BACKEND_SUPPORTS_RENDER_TARGET_BARRIER (MTL_BACKEND_DESKTOP)
/* Whether platform should allow usage of D24_S8 symbols in code. */
#define MTL_BACKEND_SUPPORTS_D24_S8_SYMBOLS (MTL_BACKEND_DESKTOP)
/* Whether border colour is supported on the given platform. */
#define MTL_BACKEND_SUPPORTS_BORDER_COLOR (MTL_BACKEND_DESKTOP)

#ifdef WITH_APPLE_CROSSPLATFORM
#  define GHOST_ContextMetal GHOST_ContextIOS
#else
#  define GHOST_ContextMetal GHOST_ContextMTL
#endif

/** -- Renderer Options -- */
/* Number of frames over which rolling averages are taken. */
#define MTL_FRAME_AVERAGE_COUNT 15
#define MTL_MAX_DRAWABLES 3
#define MTL_MAX_SET_BYTES_SIZE 4096
#define MTL_FORCE_WAIT_IDLE 0

/* Number of frames for which we retain in-flight resources such as scratch buffers.
 * Set as number of GPU frames in flight, plus an additional value for extra possible CPU frame. */
#define MTL_NUM_SAFE_FRAMES (MTL_MAX_DRAWABLES + 1)

/* Display debug information about missing attributes and incorrect vertex formats. */
#define MTL_DEBUG_SHADER_ATTRIBUTES 0
