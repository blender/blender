/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_render_graph_test_types.hh"

namespace blender::gpu::render_graph {

class VKRenderGraphTestRender : public VKRenderGraphTest_P {};

TEST_P(VKRenderGraphTestRender, begin_clear_attachments_end_read_back)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkBuffer> buffer(3u);

  resources.add_image(image, false);
  resources.add_buffer(buffer);

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
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  {
    VKCopyImageToBufferNode::CreateInfo copy_image_to_buffer = {};
    copy_image_to_buffer.node_data.src_image = image;
    copy_image_to_buffer.node_data.dst_buffer = buffer;
    copy_image_to_buffer.node_data.region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_image_to_buffer.vk_image_aspects = VK_IMAGE_ASPECT_COLOR_BIT;
    render_graph->add_node(copy_image_to_buffer);
  }

  submit(render_graph, command_buffer);

  EXPECT_EQ(6, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() +
          ", image=0x1, "
          "subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
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
          endl() + ")",
      log[4]);
  EXPECT_EQ(
      "copy_image_to_buffer(src_image=0x1, src_image_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "
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

TEST_P(VKRenderGraphTestRender, begin_draw_end)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(3u);

  resources.add_image(image, false);

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
  EXPECT_EQ(7, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, "
          "level_count=4294967295, base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, "
                "color_attachment_count=1, p_color_attachments=" +
                endl() + "  image_view=0x2, image_layout=" + color_attachment_layout_str() +
                ", "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[1]);
  EXPECT_EQ("set_viewport(num_viewports=1)", log[2]);
  EXPECT_EQ("set_scissor(num_scissors=1)", log[3]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline=0x3)",
            log[4]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[5]);
  EXPECT_EQ("end_rendering()", log[6]);
}

TEST_P(VKRenderGraphTestRender, begin_draw_end__layered)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(3u);

  resources.add_image(image, true);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
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
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               VK_ACCESS_SHADER_READ_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 1, VK_REMAINING_ARRAY_LAYERS}});
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
  EXPECT_EQ(9, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, "
          "level_count=4294967295, base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, "
          "VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", "
          "new_layout=VK_IMAGE_LAYOUT_GENERAL, "
          "image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=1, layer_count=4294967295  )" +
          endl() + ")",
      log[1]);
  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, "
                "color_attachment_count=1, p_color_attachments=" +
                endl() + "  image_view=0x2, image_layout=" + color_attachment_layout_str() +
                ", "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[2]);
  EXPECT_EQ("set_viewport(num_viewports=1)", log[3]);
  EXPECT_EQ("set_scissor(num_scissors=1)", log[4]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline=0x3)",
            log[5]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[6]);
  EXPECT_EQ("end_rendering()", log[7]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_SHADER_READ_BIT, "
          "VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, "
          "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, "
          "VK_ACCESS_TRANSFER_WRITE_BIT, dst_access_mask=VK_ACCESS_SHADER_READ_BIT, "
          "VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, "
          "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, "
          "VK_ACCESS_TRANSFER_WRITE_BIT, old_layout=VK_IMAGE_LAYOUT_GENERAL, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=1, layer_count=4294967295  )" +
          endl() + ")",
      log[8]);
}

INSTANTIATE_TEST_SUITE_P(, VKRenderGraphTestRender, ::testing::Values(true, false));

}  // namespace blender::gpu::render_graph
