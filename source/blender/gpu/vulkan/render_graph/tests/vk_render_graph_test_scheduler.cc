/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_render_graph_test_types.hh"

namespace blender::gpu::render_graph {

class VKRenderGraphTestScheduler : public VKRenderGraphTest_P {};

/** Copy buffer should be done after the end rendering. */
TEST_P(VKRenderGraphTestScheduler, begin_rendering_copy_buffer_end_rendering)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  resources.add_image(image, false);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, {}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph->add_node(copy_buffer);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  {
    render_graph::VKSynchronizationNode::CreateInfo synchronization = {};
    synchronization.vk_image = image;
    synchronization.vk_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    synchronization.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    render_graph->add_node(synchronization);
  }

  submit(render_graph, command_buffer);
  EXPECT_EQ(6, log.size());

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
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x2, image_layout=" + color_attachment_layout_str() +
                ", "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[3]);
  EXPECT_EQ("end_rendering()", log[4]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[5]);
}

TEST_P(VKRenderGraphTestScheduler, begin_clear_attachments_copy_buffer_end)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  resources.add_image(image, false);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, {}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
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
    render_graph->add_node(clear_attachments);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph->add_node(copy_buffer);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  {
    render_graph::VKSynchronizationNode::CreateInfo synchronization = {};
    synchronization.vk_image = image;
    synchronization.vk_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    synchronization.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    render_graph->add_node(synchronization);
  }

  submit(render_graph, command_buffer);
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
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x2, image_layout=" + color_attachment_layout_str() +
                ", "
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
          "dst_access_mask=, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[6]);
}

TEST_P(VKRenderGraphTestScheduler, begin_copy_buffer_clear_attachments_end)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  resources.add_image(image, false);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, {}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph->add_node(copy_buffer);
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
    render_graph->add_node(clear_attachments);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  {
    render_graph::VKSynchronizationNode::CreateInfo synchronization = {};
    synchronization.vk_image = image;
    synchronization.vk_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    synchronization.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    render_graph->add_node(synchronization);
  }

  submit(render_graph, command_buffer);
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
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x2, image_layout=" + color_attachment_layout_str() +
                ", "
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
          "dst_access_mask=, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[6]);
}

TEST_P(VKRenderGraphTestScheduler, begin_clear_attachments_copy_buffer_clear_attachments_end)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer_src(3u);
  VkHandle<VkBuffer> buffer_dst(4u);

  resources.add_image(image, false);
  resources.add_buffer(buffer_src);
  resources.add_buffer(buffer_dst);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, {}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
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
    render_graph->add_node(clear_attachments);
  }

  {
    VKCopyBufferNode::CreateInfo copy_buffer = {};
    copy_buffer.src_buffer = buffer_src;
    copy_buffer.dst_buffer = buffer_dst;
    render_graph->add_node(copy_buffer);
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
    render_graph->add_node(clear_attachments);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  {
    render_graph::VKSynchronizationNode::CreateInfo synchronization = {};
    synchronization.vk_image = image;
    synchronization.vk_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    synchronization.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    render_graph->add_node(synchronization);
  }

  submit(render_graph, command_buffer);
  ASSERT_EQ(8, log.size());

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
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[2]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x2, image_layout=" + color_attachment_layout_str() +
                ", "
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
          "dst_access_mask=, old_layout=" +
          color_attachment_layout_str() +
          ", "
          "new_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[7]);
}

/**
 * When copying the frame buffer content between two draw calls we should not move the command.
 *
 * This happens in EEVEE where the feedback radiance is copied before the world background is added
 * to the combined texture.
 */
TEST_P(VKRenderGraphTestScheduler, begin_draw_copy_framebuffer_draw_end)
{
  VkHandle<VkImage> image_attachment(1u);
  VkHandle<VkImage> image_feedback(2u);
  VkHandle<VkImageView> image_view_attachment(3u);
  VkHandle<VkPipelineLayout> pipeline_layout_combine(4u);
  VkHandle<VkPipeline> pipeline_combine(5u);
  VkHandle<VkPipelineLayout> pipeline_layout_background(6u);
  VkHandle<VkPipeline> pipeline_background(7u);

  resources.add_image(image_attachment, false);
  resources.add_image(image_feedback, false);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image_attachment, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, {}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = image_view_attachment;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
  }

  {
    VKResourceAccessInfo access_info = {};
    VKDrawNode::CreateInfo draw(access_info);
    draw.node_data.first_instance = 0;
    draw.node_data.first_vertex = 0;
    draw.node_data.instance_count = 1;
    draw.node_data.vertex_count = 4;
    draw.node_data.graphics.pipeline_data.push_constants_data = nullptr;
    draw.node_data.graphics.pipeline_data.push_constants_size = 0;
    draw.node_data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    draw.node_data.graphics.pipeline_data.vk_pipeline = pipeline_combine;
    draw.node_data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout_combine;
    draw.node_data.graphics.viewport.viewports.append(VkViewport{});
    draw.node_data.graphics.viewport.scissors.append(VkRect2D{});
    render_graph->add_node(draw);
  }

  {
    VKCopyImageNode::CreateInfo copy_image = {};
    copy_image.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_image.node_data.src_image = image_attachment;
    copy_image.node_data.dst_image = image_feedback;
    copy_image.node_data.region.extent.width = 1920;
    copy_image.node_data.region.extent.height = 1080;
    copy_image.node_data.region.extent.depth = 1;
    copy_image.node_data.region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_image.node_data.region.srcSubresource.baseArrayLayer = 0;
    copy_image.node_data.region.srcSubresource.layerCount = 1;
    copy_image.node_data.region.srcSubresource.mipLevel = 0;
    render_graph->add_node(copy_image);
  }

  {
    VKResourceAccessInfo access_info = {};
    VKDrawNode::CreateInfo draw(access_info);
    draw.node_data.first_instance = 0;
    draw.node_data.first_vertex = 0;
    draw.node_data.instance_count = 1;
    draw.node_data.vertex_count = 4;
    draw.node_data.graphics.pipeline_data.push_constants_data = nullptr;
    draw.node_data.graphics.pipeline_data.push_constants_size = 0;
    draw.node_data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    draw.node_data.graphics.pipeline_data.vk_pipeline = pipeline_background;
    draw.node_data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout_background;
    draw.node_data.graphics.viewport.viewports.append(VkViewport{});
    draw.node_data.graphics.viewport.scissors.append(VkRect2D{});
    render_graph->add_node(draw);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  submit(render_graph, command_buffer);
  ASSERT_EQ(14, log.size());

  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x3, image_layout=" + color_attachment_layout_str() +
                ", resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[1]);
  EXPECT_EQ("set_viewport(num_viewports=1)", log[2]);
  EXPECT_EQ("set_scissor(num_scissors=1)", log[3]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline=0x5)",
            log[4]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[5]);
  EXPECT_EQ("end_rendering()", log[6]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", "
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
      log[7]);
  EXPECT_EQ(
      "copy_image(src_image=0x1, src_image_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
      "dst_image=0x2, dst_image_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL" +
          endl() + " - region(src_subresource=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, mip_level=0, base_array_layer=0, "
          "layer_count=1  , src_offset=" +
          endl() + "    x=0, y=0, z=0  , dst_subresource=" + endl() +
          "    aspect_mask=, mip_level=0, base_array_layer=0, layer_count=0  , dst_offset=" +
          endl() + "    x=0, y=0, z=0  , extent=" + endl() +
          "    width=1920, height=1080, depth=1  )" + endl() + ")",
      log[8]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[9]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x3, image_layout=" + color_attachment_layout_str() +
                ", resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_LOAD, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[10]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline=0x7)",
            log[11]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[12]);
  EXPECT_EQ("end_rendering()", log[13]);
}

/**
 * Update buffers can be moved to before the rendering scope as when the destination buffer isn't
 * used.
 */
TEST_P(VKRenderGraphTestScheduler, begin_update_draw_update_draw_update_draw_end)
{
  VkHandle<VkBuffer> buffer_a(1u);
  VkHandle<VkBuffer> buffer_b(2u);
  VkHandle<VkImage> image(3u);
  VkHandle<VkImageView> image_view(4u);
  VkHandle<VkPipelineLayout> pipeline_layout(5u);
  VkHandle<VkPipeline> pipeline(6u);

  resources.add_image(image, false);
  resources.add_buffer(buffer_a);
  resources.add_buffer(buffer_b);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, {}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = image_view;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
  }

  {
    VKUpdateBufferNode::CreateInfo update_buffer = {};
    update_buffer.dst_buffer = buffer_a;
    update_buffer.data_size = 16;
    update_buffer.data = MEM_callocN(16, __func__);

    render_graph->add_node(update_buffer);
  }

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer_a, VK_ACCESS_UNIFORM_READ_BIT});
    VKDrawNode::CreateInfo draw(access_info);
    draw.node_data.first_instance = 0;
    draw.node_data.first_vertex = 0;
    draw.node_data.instance_count = 1;
    draw.node_data.vertex_count = 1;
    draw.node_data.graphics.pipeline_data.push_constants_data = nullptr;
    draw.node_data.graphics.pipeline_data.push_constants_size = 0;
    draw.node_data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    draw.node_data.graphics.pipeline_data.vk_pipeline = pipeline;
    draw.node_data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    draw.node_data.graphics.viewport.viewports.append(VkViewport{});
    draw.node_data.graphics.viewport.scissors.append(VkRect2D{});
    render_graph->add_node(draw);
  }

  {
    VKUpdateBufferNode::CreateInfo update_buffer = {};
    update_buffer.dst_buffer = buffer_b;
    update_buffer.data_size = 24;
    update_buffer.data = MEM_callocN(24, __func__);

    render_graph->add_node(update_buffer);
  }

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer_b, VK_ACCESS_UNIFORM_READ_BIT});
    VKDrawNode::CreateInfo draw(access_info);
    draw.node_data.first_instance = 0;
    draw.node_data.first_vertex = 0;
    draw.node_data.instance_count = 1;
    draw.node_data.vertex_count = 2;
    draw.node_data.graphics.pipeline_data.push_constants_data = nullptr;
    draw.node_data.graphics.pipeline_data.push_constants_size = 0;
    draw.node_data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    draw.node_data.graphics.pipeline_data.vk_pipeline = pipeline;
    draw.node_data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    draw.node_data.graphics.viewport.viewports.append(VkViewport{});
    draw.node_data.graphics.viewport.scissors.append(VkRect2D{});
    render_graph->add_node(draw);
  }

  {
    VKUpdateBufferNode::CreateInfo update_buffer = {};
    update_buffer.dst_buffer = buffer_a;
    update_buffer.data_size = 16;
    update_buffer.data = MEM_callocN(16, __func__);

    render_graph->add_node(update_buffer);
  }

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer_a, VK_ACCESS_UNIFORM_READ_BIT});
    VKDrawNode::CreateInfo draw(access_info);
    draw.node_data.first_instance = 0;
    draw.node_data.first_vertex = 0;
    draw.node_data.instance_count = 1;
    draw.node_data.vertex_count = 3;
    draw.node_data.graphics.pipeline_data.push_constants_data = nullptr;
    draw.node_data.graphics.pipeline_data.push_constants_size = 0;
    draw.node_data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    draw.node_data.graphics.pipeline_data.vk_pipeline = pipeline;
    draw.node_data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    draw.node_data.graphics.viewport.viewports.append(VkViewport{});
    draw.node_data.graphics.viewport.scissors.append(VkRect2D{});
    render_graph->add_node(draw);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  submit(render_graph, command_buffer);
  ASSERT_EQ(19, log.size());
  EXPECT_EQ("update_buffer(dst_buffer=0x1, dst_offset=0, data_size=16)", log[0]);
  EXPECT_EQ("update_buffer(dst_buffer=0x2, dst_offset=0, data_size=24)", log[1]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() +
          ", "
          "image=0x3, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, "
          "base_mip_level=0, level_count=4294967295, base_array_layer=0, layer_count=4294967295  "
          ")" +
          endl() + ")",
      log[2]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - "
          "buffer_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_UNIFORM_READ_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[3]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - "
          "buffer_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_UNIFORM_READ_BIT, buffer=0x2, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[4]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x4, image_layout=" + color_attachment_layout_str() +
                ", resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[5]);
  EXPECT_EQ("set_viewport(num_viewports=1)", log[6]);
  EXPECT_EQ("set_scissor(num_scissors=1)", log[7]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline=0x6)",
            log[8]);
  EXPECT_EQ("draw(vertex_count=1, instance_count=1, first_vertex=0, first_instance=0)", log[9]);
  EXPECT_EQ("draw(vertex_count=2, instance_count=1, first_vertex=0, first_instance=0)", log[10]);
  EXPECT_EQ("end_rendering()", log[11]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_UNIFORM_READ_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[12]);
  EXPECT_EQ("update_buffer(dst_buffer=0x1, dst_offset=0, data_size=16)", log[13]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_UNIFORM_READ_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[14]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x3, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[15]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x4, image_layout=" + color_attachment_layout_str() +
                ", resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_LOAD, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[16]);
  EXPECT_EQ("draw(vertex_count=3, instance_count=1, first_vertex=0, first_instance=0)", log[17]);
  EXPECT_EQ("end_rendering()", log[18]);
}

/**
 * When drawing, copying and continue drawing to an attachment, the attachment layout should be
 * transitioned.
 *
 * This case happens when updating the swap-chain image with the result of editors.
 */
TEST_P(VKRenderGraphTestScheduler, begin_draw_copy_to_attachment_draw_end)
{
  VkHandle<VkImage> image_attachment(1u);
  VkHandle<VkImage> image_editor(2u);
  VkHandle<VkImageView> image_view_attachment(3u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(5u);

  resources.add_image(image_attachment, false);
  resources.add_image(image_editor, false);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image_attachment, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, {}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = image_view_attachment;
    begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
  }

  {
    VKResourceAccessInfo access_info = {};
    VKDrawNode::CreateInfo draw(access_info);
    draw.node_data.first_instance = 0;
    draw.node_data.first_vertex = 0;
    draw.node_data.instance_count = 1;
    draw.node_data.vertex_count = 4;
    draw.node_data.graphics.pipeline_data.push_constants_data = nullptr;
    draw.node_data.graphics.pipeline_data.push_constants_size = 0;
    draw.node_data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    draw.node_data.graphics.pipeline_data.vk_pipeline = pipeline;
    draw.node_data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    draw.node_data.graphics.viewport.viewports.append(VkViewport{});
    draw.node_data.graphics.viewport.scissors.append(VkRect2D{});
    render_graph->add_node(draw);
  }

  {
    VKCopyImageNode::CreateInfo copy_image = {};
    copy_image.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_image.node_data.src_image = image_editor;
    copy_image.node_data.dst_image = image_attachment;
    copy_image.node_data.region.extent.width = 1920;
    copy_image.node_data.region.extent.height = 1080;
    copy_image.node_data.region.extent.depth = 1;
    copy_image.node_data.region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_image.node_data.region.srcSubresource.baseArrayLayer = 0;
    copy_image.node_data.region.srcSubresource.layerCount = 1;
    copy_image.node_data.region.srcSubresource.mipLevel = 0;
    render_graph->add_node(copy_image);
  }

  {
    VKResourceAccessInfo access_info = {};
    VKDrawNode::CreateInfo draw(access_info);
    draw.node_data.first_instance = 0;
    draw.node_data.first_vertex = 0;
    draw.node_data.instance_count = 1;
    draw.node_data.vertex_count = 4;
    draw.node_data.graphics.pipeline_data.push_constants_data = nullptr;
    draw.node_data.graphics.pipeline_data.push_constants_size = 0;
    draw.node_data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    draw.node_data.graphics.pipeline_data.vk_pipeline = pipeline;
    draw.node_data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    draw.node_data.graphics.viewport.viewports.append(VkViewport{});
    draw.node_data.graphics.viewport.scissors.append(VkRect2D{});
    render_graph->add_node(draw);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  submit(render_graph, command_buffer);
  ASSERT_EQ(13, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x3, image_layout=" + color_attachment_layout_str() +
                ", resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[1]);
  EXPECT_EQ("set_viewport(num_viewports=1)", log[2]);
  EXPECT_EQ("set_scissor(num_scissors=1)", log[3]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline=0x5)",
            log[4]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[5]);
  EXPECT_EQ("end_rendering()", log[6]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, dst_access_mask=VK_ACCESS_TRANSFER_READ_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, new_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
          "image=0x2, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", "
          "new_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[7]);
  EXPECT_EQ(
      "copy_image(src_image=0x2, src_image_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
      "dst_image=0x1, dst_image_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL" +
          endl() + " - region(src_subresource=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, mip_level=0, base_array_layer=0, "
          "layer_count=1  , src_offset=" +
          endl() + "    x=0, y=0, z=0  , dst_subresource=" + endl() +
          "    aspect_mask=, mip_level=0, base_array_layer=0, layer_count=0  , dst_offset=" +
          endl() + "    x=0, y=0, z=0  , extent=" + endl() +
          "    width=1920, height=1080, depth=1  )" + endl() + ")",
      log[8]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[9]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
                "p_color_attachments=" +
                endl() + "  image_view=0x3, image_layout=" + color_attachment_layout_str() +
                ", resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_LOAD, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[10]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[11]);
  EXPECT_EQ("end_rendering()", log[12]);
}

INSTANTIATE_TEST_SUITE_P(, VKRenderGraphTestScheduler, ::testing::Values(true, false));

}  // namespace blender::gpu::render_graph
