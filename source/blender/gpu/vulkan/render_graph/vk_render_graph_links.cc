/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_render_graph_links.hh"

#include <sstream>

namespace blender::gpu::render_graph {

void VKRenderGraphLink::debug_print(std::stringstream &ss,
                                    const VKResourceStateTracker &resources) const
{
  const VKResourceStateTracker::Resource &tracked_resource = resources.resources_.lookup(
      resource.handle);
  ss << "handle=" << resource.handle;
  ss << ", type=";

  switch (tracked_resource.type) {
    case VKResourceType::BUFFER: {
      ss << "BUFFER";
      ss << ", vk_handle=" << (uint64_t)tracked_resource.buffer.vk_buffer;
      break;
    }
    case VKResourceType::IMAGE: {
      ss << "IMAGE";
      ss << ", vk_handle=" << (uint64_t)tracked_resource.image.vk_image;
      break;
    }
    case VKResourceType::NONE: {
      ss << "NONE";
      break;
    }
  }
}

void VKRenderGraphNodeLinks::debug_print(const VKResourceStateTracker &resources) const
{
  std::stringstream ss;
  for (const VKRenderGraphLink &link : inputs) {
    ss << "- input ";
    link.debug_print(ss, resources);
    ss << "\n";
  }
  for (const VKRenderGraphLink &link : outputs) {
    ss << "- output ";
    link.debug_print(ss, resources);
    ss << "\n";
  }

  std::cout << ss.str();
}

}  // namespace blender::gpu::render_graph
