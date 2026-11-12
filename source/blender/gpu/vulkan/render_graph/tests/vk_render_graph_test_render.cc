/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_render_graph_test_types.hh"

namespace blender::gpu::render_graph {

class VKRenderGraphTestRender : public VKRenderGraphTest_P {};

/**
 * Expected log entry of a pipeline barrier that is recorded inside a rendering scope. Only
 * framebuffer-space stages are used, so it can be used for sub pass transitions.
 */
static std::string framebuffer_pipeline_barrier(const std::string &src_access_mask,
                                                const std::string &dst_access_mask,
                                                const std::string &old_layout,
                                                const std::string &new_layout)
{
  return "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, "
         "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, "
         "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, "
         "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, "
         "dst_stage_mask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, "
         "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, "
         "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, "
         "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT" +
         endl() + " - image_barrier(src_access_mask=" + src_access_mask +
         ", dst_access_mask=" + dst_access_mask + ", old_layout=" + old_layout +
         ", new_layout=" + new_layout + ", image=0x1, subresource_range=" + endl() +
         "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
         "base_array_layer=0, layer_count=4294967295  )" +
         endl() + ")";
}

static int count_log_entries(const Vector<std::string> &log, const std::string &prefix)
{
  int count = 0;
  for (const std::string &entry : log) {
    if (entry.rfind(prefix, 0) == 0) {
      count++;
    }
  }
  return count;
}

TEST_P(VKRenderGraphTestRender, begin_clear_attachments_end_read_back)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VKTrackedHandle<VkBuffer> buffer(3u);

  resources.add_image(image, false);
  buffer.set_resource_handle(resources.add_buffer(buffer));

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

  render_graph->add_node(VKSynchronizeReadbackNode::CreateInfo{buffer});
  submit(render_graph, command_buffer);

  ASSERT_EQ(7, log.size());
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
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TRANSFER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_HOST_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_HOST_READ_BIT, buffer=0x3, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[6]);
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
    VKNodeData<VKDrawNode> node = render_graph->alloc_node<VKDrawNode>();
    node.data.first_instance = 0;
    node.data.first_vertex = 0;
    node.data.instance_count = 1;
    node.data.vertex_count = 4;
    node.data.graphics.pipeline_data.push_constants_range = IndexRange(0);
    node.data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    node.data.graphics.pipeline_data.vk_pipeline = pipeline;
    node.data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    node.data.graphics.viewport.viewports.append(VkViewport{});
    node.data.graphics.viewport.scissors.append(VkRect2D{});

    VKDrawNode::CreateInfo draw_create_info(access_info);
    node.finalize(*render_graph, draw_create_info);
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
    VKNodeData<VKDrawNode> node = render_graph->alloc_node<VKDrawNode>();
    node.data.first_instance = 0;
    node.data.first_vertex = 0;
    node.data.instance_count = 1;
    node.data.vertex_count = 4;
    node.data.graphics.pipeline_data.push_constants_range = IndexRange(0);
    node.data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    node.data.graphics.pipeline_data.vk_pipeline = pipeline;
    node.data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    node.data.graphics.viewport.viewports.append(VkViewport{});
    node.data.graphics.viewport.scissors.append(VkRect2D{});

    VKDrawNode::CreateInfo draw_create_info(access_info);
    node.finalize(*render_graph, draw_create_info);
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
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "VK_ACCESS_TRANSFER_WRITE_BIT, "
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

/**
 * Tests that a synchronization barrier is generated for input attachment reads on layered images
 * after a color attachment write. Regression test for #158501.
 *
 * With local read the attachment is read back within the same rendering scope, without it the
 * scope is ended and the attachment is sampled instead.
 */
TEST_P(VKRenderGraphTestRender, begin_draw_begin_draw_end__layered_input_attachment)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(3u);
  const bool local_read = use_dynamic_rendering_local_read;

  resources.add_image(image, true);

  /* Adds a draw node that accesses the image with the given access flags. */
  auto add_draw = [&](const VkAccessFlags access_flags) {
    VKResourceAccessInfo access_info = {};
    if (access_flags != 0) {
      access_info.images.append({image,
                                 access_flags,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
    }
    VKNodeData<VKDrawNode> node = render_graph->alloc_node<VKDrawNode>();
    node.data.first_instance = 0;
    node.data.first_vertex = 0;
    node.data.instance_count = 1;
    node.data.vertex_count = 4;
    node.data.graphics.pipeline_data.push_constants_range = IndexRange(0);
    node.data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    node.data.graphics.pipeline_data.vk_pipeline = pipeline;
    node.data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    node.data.graphics.viewport.viewports.append(VkViewport{});
    node.data.graphics.viewport.scissors.append(VkRect2D{});

    VKDrawNode::CreateInfo draw_create_info(access_info);
    node.finalize(*render_graph, draw_create_info);
  };

  /* First sub pass: write to image as color attachment. */
  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image,
         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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

  add_draw(0);

  if (local_read) {
    /* Second sub pass: the image is read back as an input attachment within the same rendering
     * scope. */
    add_draw(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
  }
  else {
    /* Fallback: restart the rendering scope without the image as an attachment. */
    {
      VKResourceAccessInfo access_info = {};
      access_info.images.append(
          {image,
           VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
           VK_IMAGE_ASPECT_COLOR_BIT,
           {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
      VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
      begin_rendering.node_data.color_attachments[0].sType =
          VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
      begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
      begin_rendering.node_data.color_attachments[0].imageView = VK_NULL_HANDLE;
      begin_rendering.node_data.color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
      begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
      begin_rendering.node_data.vk_rendering_info.layerCount = 1;
      begin_rendering.node_data.vk_rendering_info.pColorAttachments =
          begin_rendering.node_data.color_attachments;

      render_graph->add_node(begin_rendering);
    }

    /* The image is bound as a sampled texture. */
    add_draw(VK_ACCESS_SHADER_READ_BIT);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  submit(render_graph, command_buffer);

  /* The read barrier is missing without the fix for #158501: the image tracker skipped access
   * synchronization for tracked layered attachments. */
  const char *expected_access = local_read ? "VK_ACCESS_INPUT_ATTACHMENT_READ_BIT" :
                                             "VK_ACCESS_SHADER_READ_BIT";
  bool has_sync_barrier = false;
  for (const std::string &entry : log) {
    if (entry.find(expected_access) != std::string::npos) {
      has_sync_barrier = true;
      break;
    }
  }
  EXPECT_TRUE(has_sync_barrier)
      << "Missing synchronization barrier for input attachment read after color attachment write. "
         "See #158501.";

  if (local_read) {
    /* The rendering scope is kept open across the sub pass transition. The sub pass transition is
     * synchronized with a barrier that stays within the rendering scope and doesn't alter the
     * image layout. */
    EXPECT_EQ(1, count_log_entries(log, "begin_rendering("));
    EXPECT_EQ(1, count_log_entries(log, "end_rendering()"));
    EXPECT_EQ(9, log.size());
    EXPECT_EQ(framebuffer_pipeline_barrier("VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, "
                                           "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT",
                                           "VK_ACCESS_INPUT_ATTACHMENT_READ_BIT",
                                           color_attachment_layout_str(),
                                           color_attachment_layout_str()),
              log[6]);
    EXPECT_EQ("end_rendering()", log[8]);
  }
  else {
    /* Without local read the rendering scope is ended and a new rendering scope that doesn't
     * include the attachment is started: two begin/end pairs and two draws, locked to the full
     * log shape. The sampled-read transition itself is content-checked by the has_sync_barrier
     * loop above. */
    EXPECT_EQ(2, count_log_entries(log, "begin_rendering("));
    EXPECT_EQ(2, count_log_entries(log, "end_rendering()"));
    EXPECT_EQ(2, count_log_entries(log, "draw("));
    EXPECT_EQ(12, log.size());
  }
}

/*
 * Regression test for VUID-VkImageMemoryBarrier-oldLayout-01197: a layered image is a color
 * attachment while a shader writes another layer of it in the same rendering scope.
 *
 * Only layer 0 is attached and stays in #VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR. The image
 * tracker switches layer 1 to #VK_IMAGE_LAYOUT_GENERAL and back at the end of the scope.
 * Access-only barriers have to be limited to their subresources, otherwise they claim a layout for
 * layer 1 and the revert barrier becomes invalid.
 */
TEST_P(VKRenderGraphTestRender, begin_draw_draw_draw_end__layered_attachment_layer_write)
{
  if (!use_dynamic_rendering_local_read) {
    GTEST_SKIP() << "Per layer image layouts require dynamic rendering local read.";
  }

  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(3u);

  resources.add_image(image, true);

  /* Adds a draw node that accesses the image with the given access flags and subresource range. */
  auto add_draw = [&](const VkAccessFlags access_flags, const VKSubImageRange &subimage) {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image, access_flags, VK_IMAGE_ASPECT_COLOR_BIT, subimage});
    VKNodeData<VKDrawNode> node = render_graph->alloc_node<VKDrawNode>();
    node.data.first_instance = 0;
    node.data.first_vertex = 0;
    node.data.instance_count = 1;
    node.data.vertex_count = 4;
    node.data.graphics.pipeline_data.push_constants_range = IndexRange(0);
    node.data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    node.data.graphics.pipeline_data.vk_pipeline = pipeline;
    node.data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    node.data.graphics.viewport.viewports.append(VkViewport{});
    node.data.graphics.viewport.scissors.append(VkRect2D{});

    VKDrawNode::CreateInfo draw_create_info(access_info);
    node.finalize(*render_graph, draw_create_info);
  };

  /* The rendering scope attaches layer 0 of the image. */
  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image,
         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_IMAGE_ASPECT_COLOR_BIT,
         {0, 1, 0, 1}});
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

  /* Layer 0 is written as color attachment. */
  add_draw(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
           {0, 1, 0, 1});
  /* Layer 1 is written by a shader, requiring #VK_IMAGE_LAYOUT_GENERAL. */
  add_draw(VK_ACCESS_SHADER_WRITE_BIT, {0, 1, 1, 1});
  /* Layer 0 is read back as an input attachment within the same rendering scope. */
  add_draw(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, {0, 1, 0, 1});

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  submit(render_graph, command_buffer);

  /* The image tracker keeps layer 1 in GENERAL while the scope is open. Layer 1 isn't attached to
   * the scope, so the transition starts from the layout the image had before the scope. */
  EXPECT_EQ(12, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, "
          "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, "
          "VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
          "VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=VK_IMAGE_LAYOUT_GENERAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=1, "
          "base_array_layer=1, layer_count=1  )" +
          endl() + ")",
      log[1]);

  /* The input attachment read is synchronized from within the scope and only covers layer 0.
   * Covering the whole image would claim RENDERING_LOCAL_READ for layer 1 and break the layout
   * revert of layer 1. */
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, "
      "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, "
      "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, "
      "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, "
      "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, "
      "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, "
      "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_SHADER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, "
          "new_layout=VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=1, "
          "base_array_layer=0, layer_count=1  )" +
          endl() + ")",
      log[8]);
  EXPECT_EQ("end_rendering()", log[10]);

  /* Ending the rendering scope reverts the layout override of layer 1. */
  const std::string all_access =
      "VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, "
      "VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "
      "VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT";
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" +
          endl() + " - image_barrier(src_access_mask=" + all_access +
          ", dst_access_mask=" + all_access +
          ", old_layout=VK_IMAGE_LAYOUT_GENERAL, "
          "new_layout=VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=1, "
          "base_array_layer=1, layer_count=1  )" +
          endl() + ")",
      log[11]);
}

TEST_P(VKRenderGraphTestRender, begin_draw_end__layered_depth)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(3u);

  resources.add_image(image, true);

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                               VK_IMAGE_ASPECT_DEPTH_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.depth_attachment.sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.depth_attachment.imageLayout =
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    begin_rendering.node_data.depth_attachment.imageView = image_view;
    begin_rendering.node_data.depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    begin_rendering.node_data.depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 0;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pDepthAttachment =
        &begin_rendering.node_data.depth_attachment;

    render_graph->add_node(begin_rendering);
  }

  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               VK_ACCESS_SHADER_READ_BIT,
                               VK_IMAGE_ASPECT_DEPTH_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 1, VK_REMAINING_ARRAY_LAYERS}});
    VKNodeData<VKDrawNode> node = render_graph->alloc_node<VKDrawNode>();
    node.data.first_instance = 0;
    node.data.first_vertex = 0;
    node.data.instance_count = 1;
    node.data.vertex_count = 4;
    node.data.graphics.pipeline_data.push_constants_range = IndexRange(0);
    node.data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    node.data.graphics.pipeline_data.vk_pipeline = pipeline;
    node.data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    node.data.graphics.viewport.viewports.append(VkViewport{});
    node.data.graphics.viewport.scissors.append(VkRect2D{});

    VKDrawNode::CreateInfo draw_create_info(access_info);
    node.finalize(*render_graph, draw_create_info);
  }

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  submit(render_graph, command_buffer);
  EXPECT_EQ(9, log.size());

  /* Pre-barrier: UNDEFINED -> DEPTH_ATTACHMENT_OPTIMAL */
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_DEPTH_BIT, base_mip_level=0, "
          "level_count=4294967295, base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);

  /* Layer transition: DEPTH_ATTACHMENT_OPTIMAL -> GENERAL (shader read of layer 1) */
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, "
          "VK_ACCESS_TRANSFER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, "
          "VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, "
          "VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, "
          "VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, "
          "old_layout=VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, "
          "new_layout=VK_IMAGE_LAYOUT_GENERAL, "
          "image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_DEPTH_BIT, base_mip_level=0, "
          "level_count=4294967295, "
          "base_array_layer=1, layer_count=4294967295  )" +
          endl() + ")",
      log[1]);

  EXPECT_EQ("begin_rendering(p_rendering_info=flags=, render_area=" + endl() +
                "  offset=" + endl() + "    x=0, y=0  , extent=" + endl() +
                "    width=0, height=0  , layer_count=1, view_mask=0, "
                "color_attachment_count=0, p_color_attachments=" +
                endl() + ", p_depth_attachment=" + endl() +
                "  image_view=0x2, "
                "image_layout=VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, "
                "resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
                "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
                "load_op=VK_ATTACHMENT_LOAD_OP_DONT_CARE, "
                "store_op=VK_ATTACHMENT_STORE_OP_STORE" +
                endl() + ")",
            log[2]);

  EXPECT_EQ("set_viewport(num_viewports=1)", log[3]);
  EXPECT_EQ("set_scissor(num_scissors=1)", log[4]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline=0x3)",
            log[5]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[6]);
  EXPECT_EQ("end_rendering()", log[7]);

  /* Post-barrier: revert GENERAL -> DEPTH_ATTACHMENT_OPTIMAL */
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
          "new_layout=VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, image=0x1, subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_DEPTH_BIT, base_mip_level=0, "
          "level_count=4294967295, "
          "base_array_layer=1, layer_count=4294967295  )" +
          endl() + ")",
      log[8]);
}

/**
 * Tests a rendering scope with three sub passes: write, read back as an input attachment, write
 * again.
 *
 * With local read the scope stays open, so there is one begin_rendering/end_rendering pair and
 * the sub pass transitions are synchronized by an in-scope barrier without layout changes.
 */
TEST_P(VKRenderGraphTestRender, begin_draw_draw_draw_end__multi_subpass)
{
  const bool local_read = use_dynamic_rendering_local_read;

  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(3u);

  resources.add_image(image, true);

  /* Adds a draw node that accesses the image with the given access flags. */
  auto add_draw = [&](const VkAccessFlags access_flags) {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               access_flags,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
    VKNodeData<VKDrawNode> node = render_graph->alloc_node<VKDrawNode>();
    node.data.first_instance = 0;
    node.data.first_vertex = 0;
    node.data.instance_count = 1;
    node.data.vertex_count = 4;
    node.data.graphics.pipeline_data.push_constants_range = IndexRange(0);
    node.data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    node.data.graphics.pipeline_data.vk_pipeline = pipeline;
    node.data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    node.data.graphics.viewport.viewports.append(VkViewport{});
    node.data.graphics.viewport.scissors.append(VkRect2D{});

    VKDrawNode::CreateInfo draw_create_info(access_info);
    node.finalize(*render_graph, draw_create_info);
  };

  /* Adds a begin_rendering node that attaches the image as color attachment 0. When #attach is
   * false the image view is null and the image is read back as a sampled texture instead. */
  auto add_begin_rendering = [&](VkAttachmentLoadOp load, bool attach, VkAccessFlags access) {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               access,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
    VKBeginRenderingNode::CreateInfo begin_rendering(access_info);
    begin_rendering.node_data.color_attachments[0].sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    begin_rendering.node_data.color_attachments[0].imageLayout = color_attachment_layout();
    begin_rendering.node_data.color_attachments[0].imageView = attach ? static_cast<VkImageView>(
                                                                            image_view) :
                                                                        VK_NULL_HANDLE;
    begin_rendering.node_data.color_attachments[0].loadOp = load;
    begin_rendering.node_data.color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    begin_rendering.node_data.vk_rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    begin_rendering.node_data.vk_rendering_info.colorAttachmentCount = 1;
    begin_rendering.node_data.vk_rendering_info.layerCount = 1;
    begin_rendering.node_data.vk_rendering_info.pColorAttachments =
        begin_rendering.node_data.color_attachments;

    render_graph->add_node(begin_rendering);
  };

  auto add_end_rendering = [&]() {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  };

  if (local_read) {
    /* One scope: write, read back as an input attachment, write again. The scope is kept open
     * across the sub pass transitions, so there is one begin/end pair and the transitions are
     * synchronized by an in-scope barrier without a layout change. */
    add_begin_rendering(
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, true, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_draw(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_draw(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
    add_draw(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_end_rendering();

    submit(render_graph, command_buffer);

    EXPECT_EQ(1, count_log_entries(log, "begin_rendering("));
    EXPECT_EQ(1, count_log_entries(log, "end_rendering()"));
    EXPECT_EQ(11, log.size());

    /* Sub pass 0 -> 1: write becomes input attachment read. */
    EXPECT_EQ(framebuffer_pipeline_barrier("VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT",
                                           "VK_ACCESS_INPUT_ATTACHMENT_READ_BIT",
                                           color_attachment_layout_str(),
                                           color_attachment_layout_str()),
              log[6]);
    /* Sub pass 1 -> 2: input attachment read becomes write. */
    EXPECT_EQ(framebuffer_pipeline_barrier("VK_ACCESS_INPUT_ATTACHMENT_READ_BIT",
                                           "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT",
                                           color_attachment_layout_str(),
                                           color_attachment_layout_str()),
              log[8]);
  }
  else {
    /* Fallback: each sub pass is a separate rendering scope, so there are three begin/end pairs,
     * locked to the full log shape. */
    add_begin_rendering(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        true,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_draw(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_end_rendering();

    /* Read the written image back as a sampled texture: the scope does not include the image. */
    add_begin_rendering(VK_ATTACHMENT_LOAD_OP_LOAD,
                        false,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_draw(VK_ACCESS_SHADER_READ_BIT);
    add_end_rendering();

    add_begin_rendering(VK_ATTACHMENT_LOAD_OP_LOAD,
                        true,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_draw(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    add_end_rendering();

    submit(render_graph, command_buffer);

    EXPECT_EQ(3, count_log_entries(log, "begin_rendering("));
    EXPECT_EQ(3, count_log_entries(log, "end_rendering()"));
    EXPECT_EQ(19, log.size());
  }
}

/*
 * Tests that a compute write between two rendering scopes is synchronized outside the scopes.
 *
 * The first scope writes the image as a color attachment, a dispatch writes it as a shader image,
 * and the second scope reads it back. A barrier recorded inside a rendering scope may only use
 * framebuffer-space stages (VUID-vkCmdPipelineBarrier-srcStageMask-09556), so the COMPUTE stage
 * of the dispatch must not leak into any barrier between the second begin_rendering and its
 * end_rendering.
 */
TEST_P(VKRenderGraphTestRender, begin_draw_dispatch_draw_end__compute_access_between_scopes)
{
  VkHandle<VkImage> image(1u);
  VkHandle<VkImageView> image_view(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkPipeline> pipeline(3u);
  VkHandle<VkDescriptorSet> descriptor_set(5u);
  const bool local_read = use_dynamic_rendering_local_read;

  resources.add_image(image, false);

  /* Adds a draw node that accesses the image with the given access flags. */
  auto add_draw = [&](const VkAccessFlags access_flags) {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               access_flags,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
    VKNodeData<VKDrawNode> node = render_graph->alloc_node<VKDrawNode>();
    node.data.first_instance = 0;
    node.data.first_vertex = 0;
    node.data.instance_count = 1;
    node.data.vertex_count = 4;
    node.data.graphics.pipeline_data.push_constants_range = IndexRange(0);
    node.data.graphics.pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
    node.data.graphics.pipeline_data.vk_pipeline = pipeline;
    node.data.graphics.pipeline_data.vk_pipeline_layout = pipeline_layout;
    node.data.graphics.viewport.viewports.append(VkViewport{});
    node.data.graphics.viewport.scissors.append(VkRect2D{});

    VKDrawNode::CreateInfo draw_create_info(access_info);
    node.finalize(*render_graph, draw_create_info);
  };

  /* First scope: write to the image as color attachment. */
  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append(
        {image,
         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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

  add_draw(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

  /* The dispatch writes the image as a shader image, requiring #VK_IMAGE_LAYOUT_GENERAL. */
  {
    VKResourceAccessInfo access_info = {};
    access_info.images.append({image,
                               VK_ACCESS_SHADER_WRITE_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               {0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set;
    dispatch_info.dispatch_node.group_count_x = 1;
    dispatch_info.dispatch_node.group_count_y = 1;
    dispatch_info.dispatch_node.group_count_z = 1;
    render_graph->add_node(dispatch_info);
  }

  /* Second scope: read the image back as an input attachment (or as a sampled texture without
   * local read). */
  add_draw(local_read ? VK_ACCESS_INPUT_ATTACHMENT_READ_BIT : VK_ACCESS_SHADER_READ_BIT);

  {
    VKEndRenderingNode::CreateInfo end_rendering = {};
    render_graph->add_node(end_rendering);
  }

  submit(render_graph, command_buffer);

  const std::string attachment_layout = color_attachment_layout_str();
  const char *read_access = local_read ? "VK_ACCESS_INPUT_ATTACHMENT_READ_BIT" :
                                         "VK_ACCESS_SHADER_READ_BIT";
  const char *read_layout = local_read ? attachment_layout.c_str() : "VK_IMAGE_LAYOUT_GENERAL";

  /* Two scopes: the dispatch between them forces a scope end and a restart. */
  EXPECT_EQ(17, log.size());
  EXPECT_EQ(2, count_log_entries(log, "begin_rendering("));
  EXPECT_EQ(2, count_log_entries(log, "end_rendering()"));
  EXPECT_EQ(1, count_log_entries(log, "dispatch("));

  /* The attachments start in the default layout. */
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, "
          "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, old_layout=VK_IMAGE_LAYOUT_UNDEFINED, "
          "new_layout=" +
          color_attachment_layout_str() + ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[0]);

  /* The dispatch's write requires GENERAL; the transitions are synchronized outside the scopes.
   */
  int general_transitions = 0;
  for (const std::string &entry : log) {
    if (entry.find("new_layout=VK_IMAGE_LAYOUT_GENERAL") != std::string::npos) {
      general_transitions++;
    }
  }
  EXPECT_EQ(local_read ? 1 : 2, general_transitions);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, "
          "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, dst_access_mask=VK_ACCESS_SHADER_WRITE_BIT, "
          "old_layout=" +
          color_attachment_layout_str() +
          ", new_layout=VK_IMAGE_LAYOUT_GENERAL, image=0x1, "
          "subresource_range=" +
          endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[8]);
  EXPECT_EQ("dispatch(group_count_x=1, group_count_y=1, group_count_z=1)", log[11]);

  /* The compute write is waited on with a COMPUTE src stage, recorded before the scope restarts.
   */
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT" +
          endl() +
          " - image_barrier(src_access_mask=VK_ACCESS_SHADER_WRITE_BIT, dst_access_mask=" +
          read_access + ", old_layout=VK_IMAGE_LAYOUT_GENERAL, new_layout=" + read_layout +
          ", image=0x1, subresource_range=" + endl() +
          "    aspect_mask=VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level=0, level_count=4294967295, "
          "base_array_layer=0, layer_count=4294967295  )" +
          endl() + ")",
      log[12]);

  /* The scope restarts with a LOAD, and the second scope contains no barriers: a clipped stage
   * mask would show up as TOP_OF_PIPE or ALL_COMMANDS there. */
  EXPECT_EQ(
      "begin_rendering(p_rendering_info=flags=, render_area=" + endl() + "  offset=" + endl() +
          "    x=0, y=0  , extent=" + endl() +
          "    width=0, height=0  , layer_count=1, view_mask=0, color_attachment_count=1, "
          "p_color_attachments=" +
          endl() + "  image_view=0x2, image_layout=" + color_attachment_layout_str() +
          ", resolve_mode=VK_RESOLVE_MODE_NONE, resolve_image_view=0, "
          "resolve_image_layout=VK_IMAGE_LAYOUT_UNDEFINED, load_op=VK_ATTACHMENT_LOAD_OP_LOAD, "
          "store_op=VK_ATTACHMENT_STORE_OP_STORE" +
          endl() + ")",
      log[14]);
  EXPECT_EQ("draw(vertex_count=4, instance_count=1, first_vertex=0, first_instance=0)", log[15]);
  EXPECT_EQ("end_rendering()", log[16]);
}

INSTANTIATE_TEST_SUITE_P(, VKRenderGraphTestRender, ::testing::Values(true, false));

}  // namespace blender::gpu::render_graph
