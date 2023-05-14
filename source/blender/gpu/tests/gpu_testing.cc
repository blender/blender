/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "CLG_log.h"

#include "GPU_context.h"
#include "GPU_debug.h"
#include "GPU_init_exit.h"
#include "gpu_testing.hh"

#include "GHOST_C-api.h"

#include "BKE_global.h"

namespace blender::gpu {

void GPUTest::SetUp()
{
  prev_g_debug_ = G.debug;
  G.debug |= G_DEBUG_GPU | G_DEBUG_GPU_RENDERDOC;

  CLG_init();
  GPU_backend_type_selection_set(gpu_backend_type);
  GHOST_GLSettings glSettings = {};
  glSettings.context_type = draw_context_type;
  glSettings.flags = GHOST_glDebugContext;
  ghost_system = GHOST_CreateSystem();
  ghost_context = GHOST_CreateOpenGLContext(ghost_system, glSettings);
  GHOST_ActivateOpenGLContext(ghost_context);
  context = GPU_context_create(nullptr, ghost_context);
  GPU_init();

  BLI_init_srgb_conversion();

  GPU_context_begin_frame(context);
  GPU_debug_capture_begin();
}

void GPUTest::TearDown()
{
  GPU_debug_capture_end();
  GPU_context_end_frame(context);

  GPU_exit();
  GPU_context_discard(context);
  GHOST_DisposeOpenGLContext(ghost_system, ghost_context);
  GHOST_DisposeSystem(ghost_system);
  CLG_exit();

  G.debug = prev_g_debug_;
}

}  // namespace blender::gpu
