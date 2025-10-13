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

/** Struct describing the access to an image. */
struct VKImageAccess {
  VkImage vk_image;
  VkAccessFlags vk_access_flags;
  VkImageAspectFlags vk_image_aspect;
  /* Used for sub-resource tracking within a rendering scope.
   *
   * By default all layers of images are tracked as a single resource. Only inside a render scope
   * we can temporary change a subset of layers, when the image is used as an attachment and a
   * image load/store.
   */
  VKSubImageRange subimage;

  /** Determine the image layout for the vk_access_flags. */
  VkImageLayout to_vk_image_layout(bool supports_local_read) const;
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

  /**
   * Reset the instance for reuse.
   */
  void reset();
};

}  // namespace blender::gpu::render_graph
