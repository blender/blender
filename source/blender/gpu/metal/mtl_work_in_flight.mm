/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_time.hh"

#include "mtl_work_in_flight.hh"

namespace blender::gpu {

MTLWorkInFlight::MTLWorkInFlight(unsigned int max_in_flight)
{
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  gpu_fence_ = [ctx->device newSharedEvent];
  async_timeline_values_.resize(max_in_flight);
  async_timeline_values_.fill(0);
}

MTLWorkInFlight::~MTLWorkInFlight()
{
  if (gpu_fence_ != nil) {
    [gpu_fence_ release];
    gpu_fence_ = nil;
  }
}

void MTLWorkInFlight::reset()
{
  async_timeline_values_.fill(0);
  work_index_ = 0;
}

void MTLWorkInFlight::begin_work()
{
  uint64_t async_timeline_prev = async_timeline_values_[work_index_];
  if (async_timeline_prev > 0) {
    if (gpu_fence_ != nil) {
      while (gpu_fence_.signaledValue < async_timeline_prev) {
        BLI_time_sleep_ms(1);
      }
    }
  }
}

void MTLWorkInFlight::end_work()
{
  /* Perform render step between samples to allow flushing of freed GPUBackend resources. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  ctx->main_command_buffer.encode_signal_event(gpu_fence_, ++current_signal_value_);
  ctx->flush();
  async_timeline_values_[work_index_] = current_signal_value_;
  work_index_ = (work_index_ + 1) % async_timeline_values_.size();
}

}  // namespace blender::gpu
