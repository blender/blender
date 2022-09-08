/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Debug features of OpenGL.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_debug.h"
#include "GPU_platform.h"

#include "mtl_context.hh"
#include "mtl_debug.hh"

#include "CLG_log.h"

#include <utility>

namespace blender::gpu::debug {

CLG_LogRef LOG = {"gpu.debug.metal"};

void mtl_debug_init()
{
  CLOG_ENSURE(&LOG);
}

}  // namespace blender::gpu::debug

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Debug Groups
 *
 * Useful for debugging through XCode GPU Debugger. This ensures all the API calls grouped into
 * "passes".
 * \{ */

void MTLContext::debug_group_begin(const char *name, int index)
{
  if (G.debug & G_DEBUG_GPU) {
    this->main_command_buffer.push_debug_group(name, index);
  }
}

void MTLContext::debug_group_end()
{
  if (G.debug & G_DEBUG_GPU) {
    this->main_command_buffer.pop_debug_group();
  }
}

/** \} */

}  // namespace blender::gpu
