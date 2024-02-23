/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "CLG_log.h"

#include "BLI_math_color.h"

#include "GPU_context.h"
#include "GPU_debug.h"
#include "GPU_init_exit.h"
#include "gpu_testing.hh"

#include "GHOST_C-api.h"

namespace blender::gpu {

void GPUTest::SetUp()
{
  prev_g_debug_ = G.debug;
  G.debug |= g_debug_flags_;

  CLG_init();
  GPU_backend_type_selection_set(gpu_backend_type);
  GHOST_GPUSettings gpuSettings = {};
  gpuSettings.context_type = draw_context_type;
  gpuSettings.flags = GHOST_gpuDebugContext;
  ghost_system = GHOST_CreateSystem();
  ghost_context = GHOST_CreateGPUContext(ghost_system, gpuSettings);
  GHOST_ActivateGPUContext(ghost_context);
  context = GPU_context_create(nullptr, ghost_context);
  GPU_init();

  BLI_init_srgb_conversion();

  GPU_render_begin();
  GPU_context_begin_frame(context);
  GPU_debug_capture_begin(nullptr);
}

void GPUTest::TearDown()
{
  GPU_debug_capture_end();
  GPU_context_end_frame(context);
  GPU_render_end();

  GPU_exit();
  GPU_context_discard(context);
  GHOST_DisposeGPUContext(ghost_system, ghost_context);
  GHOST_DisposeSystem(ghost_system);
  CLG_exit();

  G.debug = prev_g_debug_;
}

}  // namespace blender::gpu
