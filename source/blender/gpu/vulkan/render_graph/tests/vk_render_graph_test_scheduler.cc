/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_render_graph_test_types.hh"

namespace blender::gpu::render_graph {

/** Copy buffer should be done after the end rendering. */
TEST(vk_render_graph, begin_rendering_copy_buffer_end_rendering)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_image(image, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, ResourceOwner::SWAP_CHAIN);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph.add_node(begin_rendering);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph.add_node(copy_buffer);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph.add_node(end_rendering);
  }

  render_graph.submit_for_present(image);
  EXPECT_EQ(6, log.size());

  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, "
          "new_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);

  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() +
                "  image_view=0x2, image_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[1]);
  EXPECT_EQ("end_rendering()", log[2]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "buffer=0x3, offset=0, size=18446744073709551615)" +
          endl() + ")",
      log[3]);
  EXPECT_EQ("copy_buffer(src_buffer=0x3, dst_buffer=0x4" + endl() +
                " - region(src_offset=0, dst_offset=0, size=0)" + endl() + ")",
            log[4]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_MEMORY_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[5]);
}

TEST(vk_render_graph, begin_clear_attachments_copy_buffer_end)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_image(image, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, ResourceOwner::SWAP_CHAIN);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph.add_node(begin_rendering);
  }

  {
    VKClearAttachmentsNode::CreateInfo clear_attachments = {};
    clear_attachments.attachment_count = 1;
    clear_attachments.attachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachments.attachments[0].clearValue.color.float32[0] = 0.2;
    clear_attachments.attachments[0].clearValue.color.float32[1] = 0.4;
    clear_attachments.attachments[0].clearValue.color.float32[2] = 0.6;
    clear_attachments.attachments[0].clearValue.color.float32[3] = 1.0;
    clear_attachments.attachments[0].colorAttachment = 0;
    clear_attachments.vk_clear_rect.baseArrayLayer = 0;
    clear_attachments.vk_clear_rect.layerCount = 1;
    clear_attachments.vk_clear_rect.rect.extent.width = 1920;
    clear_attachments.vk_clear_rect.rect.extent.height = 1080;
    render_graph.add_node(clear_attachments);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph.add_node(copy_buffer);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph.add_node(end_rendering);
  }

  render_graph.submit_for_present(image);
  EXPECT_EQ(7, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, "
          "new_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() +
                "  image_view=0x2, image_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[1]);
  EXPECT_EQ(
      "clear_attachments( - attachment(aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, "
      "color_attachment=0)" +
          endl() + " - rect(rect=" + endl() + "    offset=" + endl() +
          "      x=0, y=0    , extent=" + endl() +
          "      width=1920, height=1080      , base_array_layer=0, layer_count=1)" + endl() + ")",
      log[2]);
  EXPECT_EQ("end_rendering()", log[3]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "buffer=0x3, offset=0, size=18446744073709551615)" +
          endl() + ")",
      log[4]);
  EXPECT_EQ("copy_buffer(src_buffer=0x3, dst_buffer=0x4" + endl() +
                " - region(src_offset=0, dst_offset=0, size=0)" + endl() + ")",
            log[5]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_MEMORY_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[6]);
}

TEST(vk_render_graph, begin_copy_buffer_clear_attachments_end)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_image(image, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, ResourceOwner::SWAP_CHAIN);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph.add_node(begin_rendering);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph.add_node(copy_buffer);
  }

  {
    VKClearAttachmentsNode::CreateInfo clear_attachments = {};
    clear_attachments.attachment_count = 1;
    clear_attachments.attachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachments.attachments[0].clearValue.color.float32[0] = 0.2;
    clear_attachments.attachments[0].clearValue.color.float32[1] = 0.4;
    clear_attachments.attachments[0].clearValue.color.float32[2] = 0.6;
    clear_attachments.attachments[0].clearValue.color.float32[3] = 1.0;
    clear_attachments.attachments[0].colorAttachment = 0;
    clear_attachments.vk_clear_rect.baseArrayLayer = 0;
    clear_attachments.vk_clear_rect.layerCount = 1;
    clear_attachments.vk_clear_rect.rect.extent.width = 1920;
    clear_attachments.vk_clear_rect.rect.extent.height = 1080;
    render_graph.add_node(clear_attachments);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph.add_node(end_rendering);
  }

  render_graph.submit_for_present(image);
  EXPECT_EQ(7, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "buffer=0x3, offset=0, size=18446744073709551615)" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("copy_buffer(src_buffer=0x3, dst_buffer=0x4" + endl() +
                " - region(src_offset=0, dst_offset=0, size=0)" + endl() + ")",
            log[1]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, "
          "new_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() +
                "  image_view=0x2, image_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[3]);
  EXPECT_EQ(
      "clear_attachments( - attachment(aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, "
      "color_attachment=0)" +
          endl() + " - rect(rect=" + endl() + "    offset=" + endl() +
          "      x=0, y=0    , extent=" + endl() +
          "      width=1920, height=1080      , base_array_layer=0, layer_count=1)" + endl() + ")",
      log[4]);
  EXPECT_EQ("end_rendering()", log[5]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_MEMORY_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[6]);
}

TEST(vk_render_graph, begin_clear_attachments_copy_buffer_clear_attachments_end)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_image(image, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, ResourceOwner::SWAP_CHAIN);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph.add_node(begin_rendering);
  }

  {
    VKClearAttachmentsNode::CreateInfo clear_attachments = {};
    clear_attachments.attachment_count = 1;
    clear_attachments.attachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachments.attachments[0].clearValue.color.float32[0] = 0.2;
    clear_attachments.attachments[0].clearValue.color.float32[1] = 0.4;
    clear_attachments.attachments[0].clearValue.color.float32[2] = 0.6;
    clear_attachments.attachments[0].clearValue.color.float32[3] = 1.0;
    clear_attachments.attachments[0].colorAttachment = 0;
    clear_attachments.vk_clear_rect.baseArrayLayer = 0;
    clear_attachments.vk_clear_rect.layerCount = 1;
    clear_attachments.vk_clear_rect.rect.extent.width = 1920;
    clear_attachments.vk_clear_rect.rect.extent.height = 1080;
    render_graph.add_node(clear_attachments);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph.add_node(copy_buffer);
  }

  {
    VKClearAttachmentsNode::CreateInfo clear_attachments = {};
    clear_attachments.attachment_count = 1;
    clear_attachments.attachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachments.attachments[0].clearValue.color.float32[0] = 0.2;
    clear_attachments.attachments[0].clearValue.color.float32[1] = 0.4;
    clear_attachments.attachments[0].clearValue.color.float32[2] = 0.6;
    clear_attachments.attachments[0].clearValue.color.float32[3] = 1.0;
    clear_attachments.attachments[0].colorAttachment = 0;
    clear_attachments.vk_clear_rect.baseArrayLayer = 0;
    clear_attachments.vk_clear_rect.layerCount = 1;
    clear_attachments.vk_clear_rect.rect.extent.width = 1920;
    clear_attachments.vk_clear_rect.rect.extent.height = 1080;
    render_graph.add_node(clear_attachments);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph.add_node(end_rendering);
  }

  render_graph.submit_for_present(image);
  EXPECT_EQ(8, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "buffer=0x3, offset=0, size=18446744073709551615)" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("copy_buffer(src_buffer=0x3, dst_buffer=0x4" + endl() +
                " - region(src_offset=0, dst_offset=0, size=0)" + endl() + ")",
            log[1]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, "
          "new_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() +
                "  image_view=0x2, image_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[3]);
  EXPECT_EQ(
      "clear_attachments( - attachment(aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, "
      "color_attachment=0)" +
          endl() + " - rect(rect=" + endl() + "    offset=" + endl() +
          "      x=0, y=0    , extent=" + endl() +
          "      width=1920, height=1080      , base_array_layer=0, layer_count=1)" + endl() + ")",
      log[4]);
  EXPECT_EQ(
      "clear_attachments( - attachment(aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, "
      "color_attachment=0)" +
          endl() + " - rect(rect=" + endl() + "    offset=" + endl() +
          "      x=0, y=0    , extent=" + endl() +
          "      width=1920, height=1080      , base_array_layer=0, layer_count=1)" + endl() + ")",
      log[5]);
  EXPECT_EQ("end_rendering()", log[6]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_MEMORY_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[7]);
}

}  // namespace blender::gpu::render_graph
