/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_math_vector.hh"

#include "vk_node_info.hh"

namespace blender::gpu::render_graph {

/**
 * Information stored inside the render graph node. See `VKRenderGraphNode`.
 */
struct VKUpdateMipmapsData {
  VkImage vk_image;
  VkImageAspectFlags vk_image_aspect;
  int mipmaps;
  int layer_count;
  int3 l0_size;
};

/**
 * Update mipmaps Node
 */
class VKUpdateMipmapsNode : public VKNodeInfo<VKNodeType::UPDATE_MIPMAPS,
                                              VKUpdateMipmapsData,
                                              VKUpdateMipmapsData,
                                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                                              VKResourceType::IMAGE> {
 public:
  /**
   * Update the node data with the data inside create_info.
   *
   * Has been implemented as a template to ensure all node specific data
   * (`VK*Data`/`VK*CreateInfo`) types can be included in the same header file as the logic. The
   * actual node data (`VKRenderGraphNode` includes all header files.)
   */
  template<typename Node, typename Storage>
  void set_node_data(Node &node, Storage & /*storage*/, const CreateInfo &create_info)
  {
    node.update_mipmaps = create_info;
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    ResourceWithStamp resource = resources.get_image_and_increase_stamp(create_info.vk_image);
    node_links.outputs.append({resource,
                               VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               create_info.vk_image_aspect});
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = nullptr;
    image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = data.vk_image;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barrier.subresourceRange.aspectMask = data.vk_image_aspect;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = data.layer_count;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = 1;

    VkImageBlit image_blit = {};
    image_blit.srcSubresource.aspectMask = data.vk_image_aspect;
    image_blit.srcSubresource.layerCount = data.layer_count;
    image_blit.srcSubresource.mipLevel = 1;
    image_blit.dstSubresource.aspectMask = data.vk_image_aspect;
    image_blit.dstSubresource.layerCount = data.layer_count;
    image_blit.dstSubresource.mipLevel = 1;

    int3 dst_size = data.l0_size;
    for (int src_mipmap : IndexRange(data.mipmaps - 1)) {
      int dst_mipmap = src_mipmap + 1;
      int3 src_size = dst_size;
      dst_size = math::max(src_size / 2, int3(1));

      /* Update the source mipmap level to be in src optimal layout. */
      image_memory_barrier.subresourceRange.baseMipLevel = src_mipmap;
      command_buffer.pipeline_barrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_DEPENDENCY_BY_REGION_BIT,
                                      0,
                                      nullptr,
                                      0,
                                      nullptr,
                                      1,
                                      &image_memory_barrier);

      /* Blit the source mipmap level into the destination mipmap level. */
      image_blit.srcSubresource.mipLevel = src_mipmap;
      image_blit.srcOffsets[1] = {src_size.x, src_size.y, src_size.z};
      image_blit.dstSubresource.mipLevel = dst_mipmap;
      image_blit.dstOffsets[1] = {dst_size.x, dst_size.y, dst_size.z};
      command_buffer.blit_image(data.vk_image,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                data.vk_image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                1,
                                &image_blit,
                                VK_FILTER_LINEAR);
    }

    /* Ensure that all mipmap levels are in the VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL layout.
     * This is the last known layout that the render graph knows about. */
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = data.mipmaps - 1;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    command_buffer.pipeline_barrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_DEPENDENCY_BY_REGION_BIT,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &image_memory_barrier);
  }
};

}  // namespace blender::gpu::render_graph
