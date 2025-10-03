/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Nodes inside the render graph are connected via links to the resources they use. These links are
 * determined when adding a node to the render graph.
 *
 * The inputs of the node link to the resources that the node reads from. The outputs of the node
 * link to the resources that the node modifies.
 *
 * All links inside the graph are stored inside `VKResourceDependencies`.
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"
#include "vk_resource_state_tracker.hh"

namespace blender::gpu::render_graph {

struct VKRenderGraphLink {
  /**
   * Which resource is being accessed.
   */
  ResourceWithStamp resource;

  /**
   * How is the resource being accessed.
   *
   * When generating pipeline barriers of a resource, the nodes access flags are evaluated to
   * create src/dst access masks.
   */
  VkAccessFlags vk_access_flags;

  /**
   * When resource is an image, which layout should the image be using.
   *
   * When generating the commands this attribute is compared with the actual image layout of the
   * the image. Additional pipeline barriers will be added to transit to the layout stored here.
   */
  VkImageLayout vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  /**
   * Which aspect of the image is being used.
   */
  VkImageAspectFlags vk_image_aspect = VK_IMAGE_ASPECT_NONE;

  /**
   * The layers and mipmap levels to bind.
   *
   * Used when layer_tracking will be enabled to transit the layout of these layers only.
   */
  VKSubImageRange subimage;

  void debug_print(std::ostream &ss, const VKResourceStateTracker &resources) const;

  /**
   * Check if this link points to a buffer resource. Implementation checks vk_image_aspect field as
   * that must be set to NONE for buffers.
   *
   * Saved additional lookups when reordering nodes.
   */
  bool is_link_to_buffer() const
  {
    return vk_image_aspect == VK_IMAGE_ASPECT_NONE;
  }
};

/**
 * All input and output links of a node in the render graph.
 */
struct VKRenderGraphNodeLinks {
  /** All links to resources that a node reads from. */
  Vector<VKRenderGraphLink> inputs;
  /** All links to resources that a node writes to. */
  Vector<VKRenderGraphLink> outputs;

  void debug_print(const VKResourceStateTracker &resources) const;
};

}  // namespace blender::gpu::render_graph
