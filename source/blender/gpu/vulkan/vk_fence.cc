/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_fence.hh"
#include "vk_backend.hh"
#include "vk_common.hh"
#include "vk_context.hh"

namespace blender::gpu {

void VKFence::signal()
{
  VKContext &context = *VKContext::get();
  timeline_value_ = context.flush_render_graph(RenderGraphFlushFlags::SUBMIT |
                                               RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
}

void VKFence::wait()
{
  VKDevice &device = VKBackend::get().device;
  device.wait_for_timeline(timeline_value_);
  timeline_value_ = 0;
}

}  // namespace blender::gpu
