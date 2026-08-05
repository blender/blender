/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_work_in_flight.hh"
#include "vk_backend.hh"
#include "vk_context.hh"

namespace blender::gpu {

VKWorkInFlight::VKWorkInFlight(unsigned int max_in_flight)
{
  async_timeline_values_.resize(max_in_flight);
  async_timeline_values_.fill(0);
}

void VKWorkInFlight::reset()
{
  async_timeline_values_.fill(0);
  work_index_ = 0;
}

void VKWorkInFlight::begin_work()
{
  TimelineValue async_timeline_prev = async_timeline_values_[work_index_];
  if (async_timeline_prev > 0) {
    VKDevice &device = VKBackend::get().device;
    device.wait_for_timeline(async_timeline_prev);
  }
}

void VKWorkInFlight::end_work()
{
  /* Perform render step between samples to avoid allocation of a high amount of command buffer
   * memory that can eventually result in out-of-memory errors or a TDR when submitted as one large
   * command buffer. */
  VKContext &context = *VKContext::get();
  TimelineValue async_timeline = context.flush_render_graph(
      RenderGraphFlushFlags::SUBMIT | RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
  async_timeline_values_[work_index_] = async_timeline;
  work_index_ = (work_index_ + 1) % async_timeline_values_.size();
}

}  // namespace blender::gpu
