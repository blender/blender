/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <sstream>

#include "vk_render_graph_links.hh"
#include "vk_to_string.hh"

namespace blender::gpu::render_graph {

void VKRenderGraphLink::debug_print(std::ostream &ss,
                                    const VKResourceStateTracker &resources) const
{
  const VKResourceStateTracker::Resource &tracked_resource = resources.resources_.lookup(
      resource.handle);
  ss << "handle=" << resource.handle;
  ss << ", type=";

  switch (tracked_resource.type) {
    case VKResourceType::BUFFER: {
      ss << "BUFFER";
      ss << ", vk_handle=" << uint64_t(tracked_resource.buffer.vk_buffer);
#ifndef NDEBUG
      if (tracked_resource.name) {
        ss << ", name=" << tracked_resource.name;
      }
#endif
      ss << ", vk_access=" << to_string_vk_access_flags(vk_access_flags);
      break;
    }
    case VKResourceType::IMAGE: {
      ss << "IMAGE";
      ss << ", vk_handle=" << uint64_t(tracked_resource.image.vk_image);
#ifndef NDEBUG
      if (tracked_resource.name) {
        ss << ", name=" << tracked_resource.name;
      }
#endif
      ss << ", vk_access=" << to_string_vk_access_flags(vk_access_flags);
      ss << ", vk_image_layout=" << to_string(vk_image_layout);
      ss << ", vk_image_aspect=" << to_string_vk_image_aspect_flags(vk_image_aspect);
      ss << ", layer_base=" << subimage.layer_base;
      ss << ", mipmap_level=" << subimage.mipmap_level;
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
