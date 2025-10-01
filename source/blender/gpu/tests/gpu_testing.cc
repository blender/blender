/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "CLG_log.h"

#include "BLI_math_color.h"
#include "BLI_threads.h"

#include "GPU_context.hh"
#include "GPU_debug.hh"
#include "GPU_init_exit.hh"

#include "gpu_testing.hh"

#include "GHOST_C-api.h"
#include "GHOST_Path-api.hh"

namespace blender::gpu {

GHOST_SystemHandle GPUTest::ghost_system_;
GHOST_ContextHandle GPUTest::ghost_context_;
GPUContext *GPUTest::context_;

int32_t GPUTest::prev_g_debug_;

void GPUTest::SetUpTestSuite(GHOST_TDrawingContextType draw_context_type,
                             GPUBackendType gpu_backend_type,
                             int32_t g_debug_flags)
{
  prev_g_debug_ = G.debug;
  G.debug |= g_debug_flags;

  CLG_init();
  BLI_threadapi_init();
  GPU_backend_type_selection_set(gpu_backend_type);
  if (!GPU_backend_supported()) {
    GTEST_SKIP() << "GPU backend not supported";
  }
  GHOST_GPUSettings gpu_settings = {};
  gpu_settings.context_type = draw_context_type;
  gpu_settings.flags = GHOST_gpuDebugContext;
  ghost_system_ = GHOST_CreateSystemBackground();
  GPU_backend_ghost_system_set(ghost_system_);
  ghost_context_ = GHOST_CreateGPUContext(ghost_system_, gpu_settings);
  GHOST_ActivateGPUContext(ghost_context_);
  context_ = GPU_context_create(nullptr, ghost_context_);
  GPU_init();

  BLI_init_srgb_conversion();

  GPU_render_begin();
  GPU_context_begin_frame(context_);
  GPU_debug_capture_begin(nullptr);
}

void GPUTest::TearDownTestSuite()
{
  GPU_debug_capture_end();
  GPU_context_end_frame(context_);
  GPU_render_end();

  GPU_exit();
  GPU_context_discard(context_);
  GHOST_DisposeGPUContext(ghost_system_, ghost_context_);
  GHOST_DisposeSystem(ghost_system_);
  GHOST_DisposeSystemPaths();
  CLG_exit();

  G.debug = prev_g_debug_;
}

void GPUTest::SetUp()
{
  const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
  std::stringstream ss;
  ss << info->test_suite_name() << "." << info->name();
  debug_group_name_ = ss.str();
  GPU_debug_group_begin(debug_group_name_.c_str());
}

void GPUTest::TearDown()
{
  GPU_debug_group_end();
}

}  // namespace blender::gpu
