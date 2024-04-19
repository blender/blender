/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Draw and dispatch commands are shader based and resources needs to be bound. The bound resources
 * are stored inside the state manager. Structures and functions inside this file improve code
 * re-usage when resources are part of the state manager.
 *
 * VKResourceAccessInfo: is a structure that can store the access information of a draw/dispatch
 * node. This information should be added to the create info of the render graph node
 * (`VKNodeInfo::CreateInfo`). When the links of the node is build,
 * `VKResourceAccessInfo.build_links` can be called to build the render graph links for these
 * resources.
 */

#pragma once

#include "BLI_utility_mixins.hh"

#include "vk_common.hh"
#include "vk_render_graph_links.hh"

namespace blender::gpu::render_graph {
class VKResourceStateTracker;
class VKRenderGraphLinks;

/** Struct describing the access to an image. */
struct VKImageAccess {
  VkImage vk_image;
  VkAccessFlags vk_access_flags;
};

/** Struct describing the access to a buffer. */
struct VKBufferAccess {
  VkBuffer vk_buffer;
  VkAccessFlags vk_access_flags;
};

/** Struct describing all resource accesses a draw/dispatch node has. */
struct VKResourceAccessInfo : NonCopyable {
  Vector<VKBufferAccess> buffers;
  Vector<VKImageAccess> images;

  /**
   * Extract read/write resource dependencies and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources, VKRenderGraphNodeLinks &node_links) const;
};

}  // namespace blender::gpu::render_graph
