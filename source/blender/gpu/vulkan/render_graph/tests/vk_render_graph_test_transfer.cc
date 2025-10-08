/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_render_graph_test_types.hh"

namespace blender::gpu::render_graph {

class VKRenderGraphTestTransfer : public VKRenderGraphTest {};

/**
 * Fill a single buffer and read it back.
 */
TEST_F(VKRenderGraphTestTransfer, fill_and_read_back)
{
  VkHandle<VkBuffer> buffer(1u);

  resources.add_buffer(buffer);
  VKFillBufferNode::CreateInfo fill_buffer = {buffer, 1024, 42};
  render_graph->add_node(fill_buffer);
  submit(render_graph, command_buffer);

  EXPECT_EQ(1, log.size());
  EXPECT_EQ("fill_buffer(dst_buffer=0x1, dst_offset=0, size=1024, data=42)", log[0]);
}

/**
 * Fill a single buffer, copy it to a staging buffer and read the staging buffer back.
 */
TEST_F(VKRenderGraphTestTransfer, fill_transfer_and_read_back)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkBuffer> staging_buffer(2u);

  resources.add_buffer(buffer);
  VKFillBufferNode::CreateInfo fill_buffer = {buffer, 1024, 42};
  render_graph->add_node(fill_buffer);
  resources.add_buffer(staging_buffer);

  VKCopyBufferNode::CreateInfo copy_buffer = {};
  copy_buffer.src_buffer = buffer;
  copy_buffer.dst_buffer = staging_buffer;
  copy_buffer.region.srcOffset = 0;
  copy_buffer.region.dstOffset = 0;
  copy_buffer.region.size = 1024;
  render_graph->add_node(copy_buffer);

  submit(render_graph, command_buffer);

  EXPECT_EQ(3, log.size());
  EXPECT_EQ("fill_buffer(dst_buffer=0x1, dst_offset=0, size=1024, data=42)", log[0]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[1]);
  EXPECT_EQ("copy_buffer(src_buffer=0x1, dst_buffer=0x2" + endl() +
                " - region(src_offset=0, dst_offset=0, size=1024)" + endl() + ")",
            log[2]);
}

/**
 * Fill a buffer twice, before reading back.
 *
 * Between the two fills a write->write barrier should be created.
 */
TEST_F(VKRenderGraphTestTransfer, fill_fill_read_back)
{
  VkHandle<VkBuffer> buffer(1u);

  resources.add_buffer(buffer);
  VKFillBufferNode::CreateInfo fill_buffer_1 = {buffer, 1024, 0};
  render_graph->add_node(fill_buffer_1);
  VKFillBufferNode::CreateInfo fill_buffer_2 = {buffer, 1024, 42};
  render_graph->add_node(fill_buffer_2);
  submit(render_graph, command_buffer);

  EXPECT_EQ(3, log.size());
  EXPECT_EQ("fill_buffer(dst_buffer=0x1, dst_offset=0, size=1024, data=0)", log[0]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[1]);
  EXPECT_EQ("fill_buffer(dst_buffer=0x1, dst_offset=0, size=1024, data=42)", log[2]);
}

/**
 * Fill a single buffer, copy it to a staging buffer and read the staging buffer back.
 */
TEST_F(VKRenderGraphTestTransfer, clear_clear_copy_and_read_back)
{
  VkHandle<VkImage> src_image(1u);
  VkHandle<VkImage> dst_image(2u);
  VkHandle<VkBuffer> staging_buffer(3u);

  resources.add_image(src_image, false);
  resources.add_image(dst_image, false);
  resources.add_buffer(staging_buffer);
  VkClearColorValue color_white = {};
  color_white.float32[0] = 1.0f;
  color_white.float32[1] = 1.0f;
  color_white.float32[2] = 1.0f;
  color_white.float32[3] = 1.0f;
  VkClearColorValue color_black = {};
  color_black.float32[0] = 0.0f;
  color_black.float32[1] = 0.0f;
  color_black.float32[2] = 0.0f;
  color_black.float32[3] = 1.0f;

  VKClearColorImageNode::CreateInfo clear_color_image_src = {};
  clear_color_image_src.vk_image = src_image;
  clear_color_image_src.vk_clear_color_value = color_white;
  VKClearColorImageNode::CreateInfo clear_color_image_dst = {};
  clear_color_image_dst.vk_image = dst_image;
  clear_color_image_dst.vk_clear_color_value = color_black;

  VKCopyImageNode::CreateInfo copy_image = {};
  copy_image.node_data.src_image = src_image;
  copy_image.node_data.dst_image = dst_image;
  copy_image.node_data.region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_image.node_data.region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_image.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  VKCopyImageToBufferNode::CreateInfo copy_dst_image_to_buffer = {};
  copy_dst_image_to_buffer.node_data.src_image = dst_image;
  copy_dst_image_to_buffer.node_data.dst_buffer = staging_buffer;
  copy_dst_image_to_buffer.node_data.region.imageSubresource.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  copy_dst_image_to_buffer.vk_image_aspects = VK_IMAGE_ASPECT_COLOR_BIT;

  render_graph->add_node(clear_color_image_src);
  render_graph->add_node(clear_color_image_dst);
  render_graph->add_node(copy_image);
  render_graph->add_node(copy_dst_image_to_buffer);
  submit(render_graph, command_buffer);

  EXPECT_EQ(8, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, new_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("clear_color_image(image=0x1, image_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)",
            log[1]);

  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, new_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "image=0x2, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ("clear_color_image(image=0x2, image_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)",
            log[3]);

  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image=0x2, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[4]);
  EXPECT_EQ(
      "copy_image(src_image=0x1, src_image_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
      "dst_image=0x2, dst_image_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL" +
          endl() + " - region(src_subresource=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, mip_level=0, base_array_layer=0, "
          "layer_count=0  , src_offset=" +
          endl() + "    x=0, y=0, z=0  , dst_subresource=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, mip_level=0, base_array_layer=0, "
          "layer_count=0  , dst_offset=" +
          endl() + "    x=0, y=0, z=0  , extent=" + endl() + "    width=0, height=0, depth=0  )" +
          endl() + ")",
      log[5]);

  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image=0x2, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[6]);
  EXPECT_EQ(
      "copy_image_to_buffer(src_image=0x2, src_image_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
      "dst_buffer=0x3" +
          endl() +
          " - region(buffer_offset=0, buffer_row_length=0, buffer_image_height=0, "
          "image_subresource=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, mip_level=0, base_array_layer=0, "
          "layer_count=0  , image_offset=" +
          endl() + "    x=0, y=0, z=0  , image_extent=\n    width=0, height=0, depth=0  )" +
          endl() + ")",
      log[7]);
}

/**
 * Clear an image, blit it to another image, copy to a staging buffer and read back.
 */
TEST_F(VKRenderGraphTestTransfer, clear_blit_copy_and_read_back)
{
  VkHandle<VkImage> src_image(1u);
  VkHandle<VkImage> dst_image(2u);
  VkHandle<VkBuffer> staging_buffer(3u);

  resources.add_image(src_image, false);
  resources.add_image(dst_image, false);
  resources.add_buffer(staging_buffer);
  VkClearColorValue color_black = {};
  color_black.float32[0] = 0.0f;
  color_black.float32[1] = 0.0f;
  color_black.float32[2] = 0.0f;
  color_black.float32[3] = 1.0f;
  VkImageBlit vk_image_blit = {};
  VKClearColorImageNode::CreateInfo clear_color_image_src = {};
  clear_color_image_src.vk_image = src_image;
  clear_color_image_src.vk_clear_color_value = color_black;
  VKCopyImageToBufferNode::CreateInfo copy_dst_image_to_buffer = {};
  copy_dst_image_to_buffer.node_data.src_image = dst_image;
  copy_dst_image_to_buffer.node_data.dst_buffer = staging_buffer;
  copy_dst_image_to_buffer.node_data.region.imageSubresource.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  copy_dst_image_to_buffer.vk_image_aspects = VK_IMAGE_ASPECT_COLOR_BIT;

  render_graph->add_node(clear_color_image_src);
  VKBlitImageNode::CreateInfo blit_image = {src_image, dst_image, vk_image_blit, VK_FILTER_LINEAR};
  render_graph->add_node(blit_image);
  render_graph->add_node(copy_dst_image_to_buffer);
  submit(render_graph, command_buffer);

  EXPECT_EQ(6, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
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
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() +
          " - image_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, new_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "image=0x2, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ(
      "blit_image(src_image=0x1, src_image_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
      "dst_image=0x2, dst_image_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
      "filter=VK_FILTER_LINEAR" +
          endl() + " - region(src_subresource=" + endl() +
          "    aspect_mask=, mip_level=0, base_array_layer=0, layer_count=0  , dst_subresource=" +
          endl() + "    aspect_mask=, mip_level=0, base_array_layer=0, layer_count=0  )" + endl() +
          ")",
      log[3]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image=0x2, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[4]);
  EXPECT_EQ(
      "copy_image_to_buffer(src_image=0x2, src_image_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
      "dst_buffer=0x3" +
          endl() +
          " - region(buffer_offset=0, buffer_row_length=0, buffer_image_height=0, "
          "image_subresource=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, mip_level=0, base_array_layer=0, "
          "layer_count=0  , image_offset=" +
          endl() + "    x=0, y=0, z=0  , image_extent=" + endl() +
          "    width=0, height=0, depth=0  )" + endl() + ")",
      log[5]);
}

/**
 * Modify a previous added copy buffer command.
 */
TEST_F(VKRenderGraphTestTransfer, copy_buffer_modify_data)
{
  VkHandle<VkBuffer> buffer_src(1u);
  VkHandle<VkBuffer> buffer_dst(2u);

  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);
  VKCopyBufferNode::CreateInfo copy_buffer = {buffer_src, buffer_dst, {0, 0, 32}};
  NodeHandle copy_buffer_handle = render_graph->add_node(copy_buffer);
  VKCopyBufferNode::Data &copy_buffer_data = render_graph->get_node_data(copy_buffer_handle);
  copy_buffer_data.region.size = 64;
  submit(render_graph, command_buffer);

  EXPECT_EQ(2, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "buffer=0x1, offset=0, size=18446744073709551615)" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("copy_buffer(src_buffer=0x1, dst_buffer=0x2" + endl() +
                " - region(src_offset=0, dst_offset=0, size=64)" + endl() + ")",
            log[1]);
}

}  // namespace blender::gpu::render_graph
