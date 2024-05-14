/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_render_graph_test_types.hh"

namespace blender::gpu::render_graph {

TEST(vk_render_graph, transfer_and_present)
{
  VkHandle<VkImage> back_buffer(1u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_image(
      back_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ResourceOwner::SWAP_CHAIN);

  render_graph.submit_for_present(back_buffer);

  EXPECT_EQ(1, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT\n - image_barrier(src_access_mask=, "
      "dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
      "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
      "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=\n    "
      "aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
      "base_array_layer=0, layer_count=4294967295  )\n)",
      log[0]);
}

TEST(vk_render_graph, clear_and_present)
{
  VkHandle<VkImage> back_buffer(1u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_image(back_buffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, ResourceOwner::SWAP_CHAIN);

  VKClearColorImageNode::CreateInfo clear_color_image = {};
  clear_color_image.vk_image = back_buffer;
  render_graph.add_node(clear_color_image);

  render_graph.submit_for_present(back_buffer);

  EXPECT_EQ(3, log.size());

  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, "
          "new_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("clear_color_image(image=0x1, image_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)",
            log[1]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
}

}  // namespace blender::gpu::render_graph
