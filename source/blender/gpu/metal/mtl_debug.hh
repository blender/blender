/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BKE_global.hh"
#include "BLI_system.h"
#include "CLG_log.h"

/** Options for organizing Metal GPU debug captures. */
/* Maximum nested debug group depth. Groups beyond this will still have the pass name pulled into
 * the RenderCommandEncoder, but will not display in the trace.
 * Use -1 for unlimited. */
#define METAL_DEBUG_CAPTURE_MAX_NESTED_GROUPS -1

/* Whether empty debug groups should be hidden. */
#define METAL_DEBUG_CAPTURE_HIDE_EMPTY 0

namespace blender {
namespace gpu {
namespace debug {

extern CLG_LogRef LOG;

/* Initialize debugging. */
void mtl_debug_init();

/* Using Macro's instead of variadic template due to non-string-literal
 * warning for CLG_logf when indirectly passing format string. */
#define EXPAND_ARGS(...) , ##__VA_ARGS__
#define MTL_LOG_ERROR(info, ...) \
  { \
    if (G.debug & G_DEBUG_GPU) { \
      CLOG_ERROR(&debug::LOG, info EXPAND_ARGS(__VA_ARGS__)); \
      BLI_system_backtrace(stderr); \
    } \
  }

#define MTL_LOG_WARNING(info, ...) \
  { \
    if (G.debug & G_DEBUG_GPU) { \
      CLOG_WARN(&debug::LOG, info EXPAND_ARGS(__VA_ARGS__)); \
    } \
  }

#define MTL_LOG_INFO(info, ...) \
  { \
    if (G.debug & G_DEBUG_GPU) { \
      CLOG_INFO(&debug::LOG, 2, info EXPAND_ARGS(__VA_ARGS__)); \
    } \
  }

}  // namespace debug
}  // namespace gpu
}  // namespace blender
