/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_render_graph_test_types.hh"

namespace blender::gpu::render_graph {

TEST(vk_render_graph, dispatch_read_back)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkPipeline> pipeline(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(3u);
  VkHandle<VkDescriptorSet> descriptor_set(4u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_buffer(buffer);
  VKResourceAccessInfo access_info = {};
  access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
  VKDispatchNode::CreateInfo dispatch_info(access_info);
  dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline;
  dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
  dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set;
  dispatch_info.dispatch_node.group_count_x = 1;
  dispatch_info.dispatch_node.group_count_y = 1;
  dispatch_info.dispatch_node.group_count_z = 1;
  render_graph.add_node(dispatch_info);
  render_graph.submit_buffer_for_read(buffer);
  EXPECT_EQ(3, log.size());
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x2)",
            log[0]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x3, "
      "p_descriptor_sets=0x4)",
      log[1]);
  EXPECT_EQ("dispatch(group_count_x=1, group_count_y=1, group_count_z=1)", log[2]);
}

/**
 * Test that the descriptor sets are updated once when chaining dispatching.
 */
TEST(vk_render_graph, dispatch_dispatch_read_back)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkPipeline> pipeline(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(3u);
  VkHandle<VkDescriptorSet> descriptor_set(4u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_buffer(buffer);

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set;
    dispatch_info.dispatch_node.group_count_x = 1;
    dispatch_info.dispatch_node.group_count_y = 1;
    dispatch_info.dispatch_node.group_count_z = 1;
    render_graph.add_node(dispatch_info);
  }
  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set;
    dispatch_info.dispatch_node.group_count_x = 2;
    dispatch_info.dispatch_node.group_count_y = 2;
    dispatch_info.dispatch_node.group_count_z = 2;
    render_graph.add_node(dispatch_info);
  }
  render_graph.submit_buffer_for_read(buffer);
  EXPECT_EQ(5, log.size());
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x2)",
            log[0]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x3, "
      "p_descriptor_sets=0x4)",
      log[1]);
  EXPECT_EQ("dispatch(group_count_x=1, group_count_y=1, group_count_z=1)", log[2]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_SHADER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_WRITE_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[3]);
  EXPECT_EQ("dispatch(group_count_x=2, group_count_y=2, group_count_z=2)", log[4]);
}

/**
 * Test that the descriptor sets are updated when chaining dispatching with different descriptor
 * sets.
 */
TEST(vk_render_graph, dispatch_dispatch_read_back_with_changing_descriptor_sets)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkPipeline> pipeline(2u);
  VkHandle<VkPipelineLayout> pipeline_layout(3u);
  VkHandle<VkDescriptorSet> descriptor_set_a(4u);
  VkHandle<VkDescriptorSet> descriptor_set_b(5u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_buffer(buffer);

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set_a;
    dispatch_info.dispatch_node.group_count_x = 1;
    dispatch_info.dispatch_node.group_count_y = 1;
    dispatch_info.dispatch_node.group_count_z = 1;
    render_graph.add_node(dispatch_info);
  }
  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set_b;
    dispatch_info.dispatch_node.group_count_x = 2;
    dispatch_info.dispatch_node.group_count_y = 2;
    dispatch_info.dispatch_node.group_count_z = 2;
    render_graph.add_node(dispatch_info);
  }
  render_graph.submit_buffer_for_read(buffer);
  EXPECT_EQ(6, log.size());
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x2)",
            log[0]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x3, "
      "p_descriptor_sets=0x4)",
      log[1]);
  EXPECT_EQ("dispatch(group_count_x=1, group_count_y=1, group_count_z=1)", log[2]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_SHADER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_WRITE_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[3]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x3, "
      "p_descriptor_sets=0x5)",
      log[4]);
  EXPECT_EQ("dispatch(group_count_x=2, group_count_y=2, group_count_z=2)", log[5]);
}

/**
 * Test that the descriptor sets are updated when chaining dispatching with different pipelines.
 */
TEST(vk_render_graph, dispatch_dispatch_read_back_with_changing_pipelines)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkPipeline> pipeline_a(2u);
  VkHandle<VkPipeline> pipeline_b(3u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkDescriptorSet> descriptor_set(5u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_buffer(buffer);

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline_a;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set;
    dispatch_info.dispatch_node.group_count_x = 1;
    dispatch_info.dispatch_node.group_count_y = 1;
    dispatch_info.dispatch_node.group_count_z = 1;
    render_graph.add_node(dispatch_info);
  }
  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline_b;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set;
    dispatch_info.dispatch_node.group_count_x = 2;
    dispatch_info.dispatch_node.group_count_y = 2;
    dispatch_info.dispatch_node.group_count_z = 2;
    render_graph.add_node(dispatch_info);
  }
  render_graph.submit_buffer_for_read(buffer);
  EXPECT_EQ(6, log.size());
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x2)",
            log[0]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x4, "
      "p_descriptor_sets=0x5)",
      log[1]);
  EXPECT_EQ("dispatch(group_count_x=1, group_count_y=1, group_count_z=1)", log[2]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_SHADER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_WRITE_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[3]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x3)",
            log[4]);
  EXPECT_EQ("dispatch(group_count_x=2, group_count_y=2, group_count_z=2)", log[5]);
}

/**
 * Test that the descriptor sets are updated when chaining dispatching with different pipelines and
 * descriptor sets.
 */
TEST(vk_render_graph, dispatch_dispatch_read_back_with_changing_pipelines_descriptor_sets)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkPipeline> pipeline_a(2u);
  VkHandle<VkPipeline> pipeline_b(3u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkDescriptorSet> descriptor_set_a(5u);
  VkHandle<VkDescriptorSet> descriptor_set_b(6u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_buffer(buffer);

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline_a;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set_a;
    dispatch_info.dispatch_node.group_count_x = 1;
    dispatch_info.dispatch_node.group_count_y = 1;
    dispatch_info.dispatch_node.group_count_z = 1;
    render_graph.add_node(dispatch_info);
  }
  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchNode::CreateInfo dispatch_info(access_info);
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline = pipeline_b;
    dispatch_info.dispatch_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
    dispatch_info.dispatch_node.pipeline_data.vk_descriptor_set = descriptor_set_b;
    dispatch_info.dispatch_node.group_count_x = 2;
    dispatch_info.dispatch_node.group_count_y = 2;
    dispatch_info.dispatch_node.group_count_z = 2;
    render_graph.add_node(dispatch_info);
  }
  render_graph.submit_buffer_for_read(buffer);
  EXPECT_EQ(7, log.size());
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x2)",
            log[0]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x4, "
      "p_descriptor_sets=0x5)",
      log[1]);
  EXPECT_EQ("dispatch(group_count_x=1, group_count_y=1, group_count_z=1)", log[2]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_SHADER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_WRITE_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[3]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x3)",
            log[4]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x4, "
      "p_descriptor_sets=0x6)",
      log[5]);
  EXPECT_EQ("dispatch(group_count_x=2, group_count_y=2, group_count_z=2)", log[6]);
}

/**
 * Test dispatch indirect
 */
TEST(vk_render_graph, dispatch_indirect_read_back)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkBuffer> command_buffer(2u);
  VkHandle<VkPipeline> pipeline(3u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkDescriptorSet> descriptor_set(5u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_buffer(buffer);
  resources.add_buffer(command_buffer);

  VKResourceAccessInfo access_info = {};
  access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
  VKDispatchIndirectNode::CreateInfo dispatch_indirect_info(access_info);
  dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_pipeline = pipeline;
  dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_pipeline_layout = pipeline_layout;
  dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_descriptor_set = descriptor_set;
  dispatch_indirect_info.dispatch_indirect_node.buffer = command_buffer;
  dispatch_indirect_info.dispatch_indirect_node.offset = 0;
  render_graph.add_node(dispatch_indirect_info);
  render_graph.submit_buffer_for_read(buffer);
  EXPECT_EQ(4, log.size());
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_INDIRECT_COMMAND_READ_BIT, buffer=0x2, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x3)",
            log[1]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x4, "
      "p_descriptor_sets=0x5)",
      log[2]);
  EXPECT_EQ("dispatch_indirect(buffer=0x2, offset=0)", log[3]);
}

TEST(vk_render_graph, dispatch_indirect_dispatch_indirect_read_back)
{
  VkHandle<VkBuffer> buffer(1u);
  VkHandle<VkBuffer> command_buffer(2u);
  VkHandle<VkPipeline> pipeline(3u);
  VkHandle<VkPipelineLayout> pipeline_layout(4u);
  VkHandle<VkDescriptorSet> descriptor_set(5u);

  Vector<std::string> log;
  VKResourceStateTracker resources;
  VKRenderGraph render_graph(std::make_unique<CommandBufferLog>(log), resources);
  resources.add_buffer(buffer);
  resources.add_buffer(command_buffer);

  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchIndirectNode::CreateInfo dispatch_indirect_info(access_info);
    dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_pipeline = pipeline;
    dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_pipeline_layout =
        pipeline_layout;
    dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_descriptor_set = descriptor_set;
    dispatch_indirect_info.dispatch_indirect_node.buffer = command_buffer;
    dispatch_indirect_info.dispatch_indirect_node.offset = 0;
    render_graph.add_node(dispatch_indirect_info);
  }
  {
    VKResourceAccessInfo access_info = {};
    access_info.buffers.append({buffer, VK_ACCESS_SHADER_WRITE_BIT});
    VKDispatchIndirectNode::CreateInfo dispatch_indirect_info(access_info);
    dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_pipeline = pipeline;
    dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_pipeline_layout =
        pipeline_layout;
    dispatch_indirect_info.dispatch_indirect_node.pipeline_data.vk_descriptor_set = descriptor_set;
    dispatch_indirect_info.dispatch_indirect_node.buffer = command_buffer;
    dispatch_indirect_info.dispatch_indirect_node.offset = 12;
    render_graph.add_node(dispatch_indirect_info);
  }
  render_graph.submit_buffer_for_read(buffer);
  EXPECT_EQ(6, log.size());

  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=, "
          "dst_access_mask=VK_ACCESS_INDIRECT_COMMAND_READ_BIT, buffer=0x2, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[0]);
  EXPECT_EQ("bind_pipeline(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, pipeline=0x3)",
            log[1]);
  EXPECT_EQ(
      "bind_descriptor_sets(pipeline_bind_point=VK_PIPELINE_BIND_POINT_COMPUTE, layout=0x4, "
      "p_descriptor_sets=0x5)",
      log[2]);
  EXPECT_EQ("dispatch_indirect(buffer=0x2, offset=0)", log[3]);
  EXPECT_EQ(
      "pipeline_barrier(src_stage_mask=VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, "
      "VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, "
      "dst_stage_mask=VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT" +
          endl() +
          " - buffer_barrier(src_access_mask=VK_ACCESS_SHADER_WRITE_BIT, "
          "dst_access_mask=VK_ACCESS_SHADER_WRITE_BIT, buffer=0x1, offset=0, "
          "size=18446744073709551615)" +
          endl() + ")",
      log[4]);
  EXPECT_EQ("dispatch_indirect(buffer=0x2, offset=12)", log[5]);
}
}  // namespace blender::gpu::render_graph
