/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_threads.h"
#include "COM_result.hh"
#include "DRW_engine.hh"

#include "compositor_cache.hh"

namespace blender::seq {

CompositorCache::~CompositorCache()
{
  bool use_main_context = false;
  if (this->last_evaluation_used_gpu) {
    /* Free resources with GPU context enabled. Cleanup may happen from the main thread, and we
     * must use the main context there. */
    BLI_assert(BLI_thread_is_main() || this->secondary_gpu_context.ghost_context != nullptr);
    use_main_context = BLI_thread_is_main();
    if (use_main_context) {
      DRW_gpu_context_enable();
    }
    else {
      gpu::GPU_activate_secondary_context(this->secondary_gpu_context);
    }
  }

  this->cache_manager.free();

  /* See comment above on context enabling. */
  if (this->last_evaluation_used_gpu) {
    if (use_main_context) {
      DRW_gpu_context_disable();
    }
    else {
      gpu::GPU_deactivate_secondary_context(this->secondary_gpu_context);
    }
  }
}

void CompositorCache::recreate_if_needed(bool gpu,
                                         compositor::ResultPrecision precision,
                                         const gpu::GPUSecondaryContextData &gpu_context)
{
  this->secondary_gpu_context = gpu ? gpu_context : gpu::GPUSecondaryContextData();

  if (this->last_evaluation_used_gpu == gpu && this->last_evaluation_precision == precision) {
    return;
  }
  this->cache_manager.free();
  this->last_evaluation_used_gpu = gpu;
  this->last_evaluation_precision = precision;
}

}  // namespace blender::seq
