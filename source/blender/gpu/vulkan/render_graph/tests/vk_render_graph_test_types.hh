/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <locale>
#include <sstream>

#include "render_graph/vk_render_graph.hh"
#include "vk_common.hh"
#include "vk_to_string.hh"

namespace blender::gpu::render_graph {

BLI_INLINE std::string &endl()
{
  static std::string endl;
  if (endl.empty()) {
    std::stringstream ss;
    ss << std::endl;
    endl = ss.str();
  }
  return endl;
}

class CommandBufferLog : public VKCommandBufferInterface {
  Vector<std::string> &log_;
  bool is_recording_ = false;

 public:
  CommandBufferLog(Vector<std::string> &log, bool use_dynamic_rendering_local_read_ = true)
      : log_(log)
  {
    use_dynamic_rendering_local_read = use_dynamic_rendering_local_read_;
  }
  virtual ~CommandBufferLog() {}

  void begin_recording() override
  {
    EXPECT_FALSE(is_recording_);
    is_recording_ = true;
  }

  void end_recording() override
  {
    EXPECT_TRUE(is_recording_);
    is_recording_ = false;
  }

  void bind_pipeline(VkPipelineBindPoint pipeline_bind_point, VkPipeline pipeline) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "bind_pipeline(";
    ss << "pipeline_bind_point=" << to_string(pipeline_bind_point);
    ss << ", pipeline=" << to_string(pipeline);
    ss << ")";
    log_.append(ss.str());
  }

  void bind_descriptor_sets(VkPipelineBindPoint pipeline_bind_point,
                            VkPipelineLayout layout,
                            uint32_t first_set,
                            uint32_t descriptor_set_count,
                            const VkDescriptorSet *p_descriptor_sets,
                            uint32_t dynamic_offset_count,
                            const uint32_t *p_dynamic_offsets) override
  {
    UNUSED_VARS(pipeline_bind_point,
                layout,
                first_set,
                descriptor_set_count,
                p_descriptor_sets,
                dynamic_offset_count,
                p_dynamic_offsets);
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "bind_descriptor_sets(";
    ss << "pipeline_bind_point=" << to_string(pipeline_bind_point);
    ss << ", layout=" << to_string(layout);
    ss << ", p_descriptor_sets=" << to_string(p_descriptor_sets[0]);
    ss << ")";
    log_.append(ss.str());
  }

  void bind_index_buffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType index_type) override
  {
    UNUSED_VARS(buffer, offset, index_type);
    EXPECT_TRUE(is_recording_);
    GTEST_FAIL() << __func__ << " not implemented!";
  }

  void bind_vertex_buffers(uint32_t first_binding,
                           uint32_t binding_count,
                           const VkBuffer *p_buffers,
                           const VkDeviceSize *p_offsets) override
  {
    UNUSED_VARS(first_binding, binding_count, p_buffers, p_offsets);
    EXPECT_TRUE(is_recording_);
    GTEST_FAIL() << __func__ << " not implemented!";
  }

  void draw(uint32_t vertex_count,
            uint32_t instance_count,
            uint32_t first_vertex,
            uint32_t first_instance) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "draw(";
    ss << "vertex_count=" << vertex_count;
    ss << ", instance_count=" << instance_count;
    ss << ", first_vertex=" << first_vertex;
    ss << ", first_instance=" << first_instance;
    ss << ")";
    log_.append(ss.str());
  }

  void draw_indexed(uint32_t index_count,
                    uint32_t instance_count,
                    uint32_t first_index,
                    int32_t vertex_offset,
                    uint32_t first_instance) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "draw_indexed(";
    ss << "index_count=" << index_count;
    ss << ", instance_count=" << instance_count;
    ss << ", first_index=" << first_index;
    ss << ", vertex_offset=" << vertex_offset;
    ss << ", first_instance=" << first_instance;
    ss << ")";
    log_.append(ss.str());
  }

  void draw_indirect(VkBuffer buffer,
                     VkDeviceSize offset,
                     uint32_t draw_count,
                     uint32_t stride) override
  {
    UNUSED_VARS(buffer, offset, draw_count, stride);
    EXPECT_TRUE(is_recording_);
    GTEST_FAIL() << __func__ << " not implemented!";
  }

  void draw_indexed_indirect(VkBuffer buffer,
                             VkDeviceSize offset,
                             uint32_t draw_count,
                             uint32_t stride) override
  {
    UNUSED_VARS(buffer, offset, draw_count, stride);
    EXPECT_TRUE(is_recording_);
    GTEST_FAIL() << __func__ << " not implemented!";
  }

  void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override
  {
    UNUSED_VARS(group_count_x, group_count_y, group_count_z);
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "dispatch(";
    ss << "group_count_x=" << group_count_x;
    ss << ", group_count_y=" << group_count_y;
    ss << ", group_count_z=" << group_count_z;
    ss << ")";
    log_.append(ss.str());
  }

  void dispatch_indirect(VkBuffer buffer, VkDeviceSize offset) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "dispatch_indirect(";
    ss << "buffer=" << to_string(buffer);
    ss << ", offset=" << offset;
    ss << ")";
    log_.append(ss.str());
  }

  void update_buffer(VkBuffer dst_buffer,
                     VkDeviceSize dst_offset,
                     VkDeviceSize data_size,
                     const void * /*p_data*/) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "update_buffer(";
    ss << "dst_buffer=" << to_string(dst_buffer);
    ss << ", dst_offset=" << dst_offset;
    ss << ", data_size=" << data_size;
    ss << ")";
    log_.append(ss.str());
  }
  void copy_buffer(VkBuffer src_buffer,
                   VkBuffer dst_buffer,
                   uint32_t region_count,
                   const VkBufferCopy *p_regions) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "copy_buffer(";
    ss << "src_buffer=" << to_string(src_buffer);
    ss << ", dst_buffer=" << to_string(dst_buffer);
    ss << std::endl;
    for (const VkBufferCopy &region : Span<const VkBufferCopy>(p_regions, region_count)) {
      ss << " - region(" << to_string(region, 1) << ")" << std::endl;
    }
    ss << ")";
    log_.append(ss.str());
  }

  void copy_image(VkImage src_image,
                  VkImageLayout src_image_layout,
                  VkImage dst_image,
                  VkImageLayout dst_image_layout,
                  uint32_t region_count,
                  const VkImageCopy *p_regions) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "copy_image(";
    ss << "src_image=" << to_string(src_image);
    ss << ", src_image_layout=" << to_string(src_image_layout);
    ss << ", dst_image=" << to_string(dst_image);
    ss << ", dst_image_layout=" << to_string(dst_image_layout);
    ss << std::endl;
    for (const VkImageCopy &region : Span<const VkImageCopy>(p_regions, region_count)) {
      ss << " - region(" << to_string(region, 1) << ")" << std::endl;
    }
    ss << ")";
    log_.append(ss.str());
  }

  void blit_image(VkImage src_image,
                  VkImageLayout src_image_layout,
                  VkImage dst_image,
                  VkImageLayout dst_image_layout,
                  uint32_t region_count,
                  const VkImageBlit *p_regions,
                  VkFilter filter) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "blit_image(";
    ss << "src_image=" << to_string(src_image);
    ss << ", src_image_layout=" << to_string(src_image_layout);
    ss << ", dst_image=" << to_string(dst_image);
    ss << ", dst_image_layout=" << to_string(dst_image_layout);
    ss << ", filter=" << to_string(filter);
    ss << std::endl;
    for (const VkImageBlit &region : Span<const VkImageBlit>(p_regions, region_count)) {
      ss << " - region(" << to_string(region, 1) << ")" << std::endl;
    }
    ss << ")";
    log_.append(ss.str());
  }

  void copy_buffer_to_image(VkBuffer src_buffer,
                            VkImage dst_image,
                            VkImageLayout dst_image_layout,
                            uint32_t region_count,
                            const VkBufferImageCopy *p_regions) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "copy_buffer_to_image(";
    ss << "src_buffer=" << to_string(src_buffer);
    ss << ", dst_image=" << to_string(dst_image);
    ss << ", src_image_layout=" << to_string(dst_image_layout);
    ss << std::endl;
    for (const VkBufferImageCopy &region : Span<const VkBufferImageCopy>(p_regions, region_count))
    {
      ss << " - region(" << to_string(region, 1) << ")" << std::endl;
    }
    ss << ")";
    log_.append(ss.str());
  }

  void copy_image_to_buffer(VkImage src_image,
                            VkImageLayout src_image_layout,
                            VkBuffer dst_buffer,
                            uint32_t region_count,
                            const VkBufferImageCopy *p_regions) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "copy_image_to_buffer(";
    ss << "src_image=" << to_string(src_image);
    ss << ", src_image_layout=" << to_string(src_image_layout);
    ss << ", dst_buffer=" << to_string(dst_buffer);
    ss << std::endl;
    for (const VkBufferImageCopy &region : Span<const VkBufferImageCopy>(p_regions, region_count))
    {
      ss << " - region(" << to_string(region, 1) << ")" << std::endl;
    }
    ss << ")";
    log_.append(ss.str());
  }

  void fill_buffer(VkBuffer dst_buffer,
                   VkDeviceSize dst_offset,
                   VkDeviceSize size,
                   uint32_t data) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "fill_buffer(";
    ss << "dst_buffer=" << to_string(dst_buffer);
    ss << ", dst_offset=" << dst_offset;
    ss << ", size=" << size;
    ss << ", data=" << data;
    ss << ")";
    log_.append(ss.str());
  }

  void clear_color_image(VkImage image,
                         VkImageLayout image_layout,
                         const VkClearColorValue *p_color,
                         uint32_t range_count,
                         const VkImageSubresourceRange *p_ranges) override
  {
    UNUSED_VARS(p_color, range_count, p_ranges);
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "clear_color_image(";
    ss << "image=" << to_string(image);
    ss << ", image_layout=" << to_string(image_layout);
    ss << ")";
    log_.append(ss.str());
  }

  void clear_depth_stencil_image(VkImage image,
                                 VkImageLayout image_layout,
                                 const VkClearDepthStencilValue *p_depth_stencil,
                                 uint32_t range_count,
                                 const VkImageSubresourceRange *p_ranges) override
  {
    UNUSED_VARS(image, image_layout, p_depth_stencil, range_count, p_ranges);
    EXPECT_TRUE(is_recording_);
    GTEST_FAIL() << __func__ << " not implemented!";
  }

  void clear_attachments(uint32_t attachment_count,
                         const VkClearAttachment *p_attachments,
                         uint32_t rect_count,
                         const VkClearRect *p_rects) override
  {
    UNUSED_VARS(attachment_count, p_attachments, rect_count, p_rects);
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "clear_attachments(";
    for (const VkClearAttachment &attachment :
         Span<VkClearAttachment>(p_attachments, attachment_count))
    {
      ss << " - attachment(" << to_string(attachment, 1) << ")" << std::endl;
    }
    for (const VkClearRect &rect : Span<VkClearRect>(p_rects, rect_count)) {
      ss << " - rect(" << to_string(rect, 1) << ")" << std::endl;
    }
    ss << ")";

    log_.append(ss.str());
  }

  void pipeline_barrier(VkPipelineStageFlags src_stage_mask,
                        VkPipelineStageFlags dst_stage_mask,
                        VkDependencyFlags dependency_flags,
                        uint32_t memory_barrier_count,
                        const VkMemoryBarrier *p_memory_barriers,
                        uint32_t buffer_memory_barrier_count,
                        const VkBufferMemoryBarrier *p_buffer_memory_barriers,
                        uint32_t image_memory_barrier_count,
                        const VkImageMemoryBarrier *p_image_memory_barriers) override
  {
    UNUSED_VARS(dependency_flags, memory_barrier_count, p_memory_barriers);
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "pipeline_barrier(";
    ss << "src_stage_mask=" << to_string_vk_pipeline_stage_flags(src_stage_mask);
    ss << ", dst_stage_mask=" << to_string_vk_pipeline_stage_flags(dst_stage_mask);
    ss << std::endl;
    for (VkImageMemoryBarrier image_barrier :
         Span<VkImageMemoryBarrier>(p_image_memory_barriers, image_memory_barrier_count))
    {
      ss << " - image_barrier(" << to_string(image_barrier, 1) << ")" << std::endl;
    }
    for (VkBufferMemoryBarrier buffer_barrier :
         Span<VkBufferMemoryBarrier>(p_buffer_memory_barriers, buffer_memory_barrier_count))
    {
      ss << " - buffer_barrier(" << to_string(buffer_barrier, 1) << ")" << std::endl;
    }
    ss << ")";

    log_.append(ss.str());
  }

  void push_constants(VkPipelineLayout layout,
                      VkShaderStageFlags stage_flags,
                      uint32_t offset,
                      uint32_t size,
                      const void *p_values) override
  {
    UNUSED_VARS(layout, stage_flags, offset, size, p_values);
    EXPECT_TRUE(is_recording_);
    GTEST_FAIL() << __func__ << " not implemented!";
  }

  void begin_rendering(const VkRenderingInfo *p_rendering_info) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "begin_rendering(";
    ss << "p_rendering_info=" << to_string(*p_rendering_info);
    ss << ")";
    log_.append(ss.str());
  }

  void end_rendering() override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "end_rendering()";
    log_.append(ss.str());
  }

  void begin_query(VkQueryPool /*vk_query_pool*/,
                   uint32_t /*query_index*/,
                   VkQueryControlFlags /*vk_query_control_flags*/) override
  {
  }
  void end_query(VkQueryPool /*vk_query_pool*/, uint32_t /*query_index*/) override {}
  void reset_query_pool(VkQueryPool /*vk_query_pool*/,
                        uint32_t /*first_query*/,
                        uint32_t /*query_count*/) override
  {
  }

  void set_viewport(const Vector<VkViewport> viewports) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "set_viewport(num_viewports=" << viewports.size() << ")";
    log_.append(ss.str());
  }

  void set_scissor(const Vector<VkRect2D> scissors) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "set_scissor(num_scissors=" << scissors.size() << ")";
    log_.append(ss.str());
  }

  void set_line_width(const float line_width) override
  {
    EXPECT_TRUE(is_recording_);
    std::stringstream ss;
    ss << "set_line_width(line_width=" << line_width << ")";
    log_.append(ss.str());
  }

  void begin_debug_utils_label(const VkDebugUtilsLabelEXT * /*vk_debug_utils_label*/) override {}
  void end_debug_utils_label() override {}
};

class VKRenderGraphTest : public ::testing::Test {
 public:
  VKRenderGraphTest()
  {
    resources.use_dynamic_rendering_local_read = use_dynamic_rendering_local_read;
    render_graph = std::make_unique<VKRenderGraph>(resources);
    command_buffer = std::make_unique<CommandBufferLog>(log, use_dynamic_rendering_local_read);
  }

 protected:
  Vector<std::string> log;
  VKResourceStateTracker resources;
  std::unique_ptr<VKRenderGraph> render_graph;
  std::unique_ptr<CommandBufferLog> command_buffer;
  bool use_dynamic_rendering_local_read = true;
};

class VKRenderGraphTest_P : public ::testing::TestWithParam<std::tuple<bool>> {
 public:
  VKRenderGraphTest_P()
  {
    use_dynamic_rendering_local_read = std::get<0>(GetParam());
    resources.use_dynamic_rendering_local_read = use_dynamic_rendering_local_read;
    render_graph = std::make_unique<VKRenderGraph>(resources);
    command_buffer = std::make_unique<CommandBufferLog>(log, use_dynamic_rendering_local_read);
  }

 protected:
  VkImageLayout color_attachment_layout() const
  {
    return use_dynamic_rendering_local_read ? VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR :
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  std::string color_attachment_layout_str() const
  {
    return use_dynamic_rendering_local_read ? "VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR" :
                                              "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL";
  }

  Vector<std::string> log;
  VKResourceStateTracker resources;
  std::unique_ptr<VKRenderGraph> render_graph;
  std::unique_ptr<CommandBufferLog> command_buffer;
  bool use_dynamic_rendering_local_read = true;
};

/**
 * Union to create a dummy vulkan handler.
 *
 * Due to platform differences the actual VKObjectType type can be different (`uint64_t` or
 * `VkObjectType_T*`).
 */
template<typename VKObjectType> union VkHandle {
  VKObjectType vk_handle;
  uint64_t handle;

  VkHandle(uint64_t handle) : handle(handle) {}

  operator VKObjectType() const
  {
    return vk_handle;
  }
};

static inline void submit(std::unique_ptr<VKRenderGraph> &render_graph,
                          std::unique_ptr<CommandBufferLog> &command_buffer)
{
  VKScheduler scheduler;
  VKCommandBuilder command_builder;
  Span<render_graph::NodeHandle> node_handles = scheduler.select_nodes(*render_graph);
  command_builder.build_nodes(*render_graph, *command_buffer, node_handles);

  command_buffer->begin_recording();
  command_builder.record_commands(*render_graph, *command_buffer, node_handles);
  command_buffer->end_recording();

  render_graph->reset();
}
}  // namespace blender::gpu::render_graph
