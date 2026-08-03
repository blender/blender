/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_batch.hh"

#include "vk_context.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"
#include "vk_storage_buffer.hh"
#include "vk_vertex_attribute_object.hh"

namespace blender::gpu {

void VKBatch::ensure_data_uploaded() const
{
  for (int index : IndexRange(GPU_BATCH_VBO_MAX_LEN)) {
    VKVertexBuffer *vertex_buffer = vertex_buffer_get(index);
    if (vertex_buffer) {
      vertex_buffer->upload();
    }
  }
  VKIndexBuffer *index_buffer = index_buffer_get();
  if (index_buffer) {
    index_buffer->upload_data();
  }
}

void VKBatch::draw(int vertex_first, int vertex_count, int instance_first, int instance_count)
{
  ensure_data_uploaded();

  VKContext &context = *VKContext::get();
  render_graph::VKRenderGraph &graph = context.render_graph();
  render_graph::VKResourceAccessInfo &resource_access_info = context.reset_and_get_access_info();

  const VKVertexAttributeObject &vao = get_vertex_attribute_object(context);

  VKIndexBuffer *index_buffer = index_buffer_get();
  const bool draw_indexed = index_buffer != nullptr;

  VKFrameBuffer &framebuffer = *context.active_framebuffer_get();
  framebuffer.rendering_ensure(context);

  if (draw_indexed) {
    render_graph::VKNodeData<render_graph::VKDrawIndexedNode> node =
        graph.alloc_node<render_graph::VKDrawIndexedNode>();
    node.data.index_count = vertex_count;
    node.data.instance_count = instance_count;
    node.data.first_index = index_buffer->index_start_get() + vertex_first;
    node.data.vertex_offset = index_buffer->index_base_get();
    node.data.first_instance = instance_first;

    node.data.index_buffer.buffer = index_buffer->resource();
    node.data.index_buffer.index_type = index_buffer->vk_index_type();
    vao.bind(node.data.vertex_buffers);
    context.update_pipeline_data(framebuffer, prim_type, vao.vertex_input_key, node.data.graphics);

    render_graph::VKDrawIndexedNode::CreateInfo create_info(resource_access_info);
    node.finalize(graph, create_info);
  }
  else {
    render_graph::VKNodeData<render_graph::VKDrawNode> node =
        graph.alloc_node<render_graph::VKDrawNode>();
    node.data.vertex_count = vertex_count;
    node.data.instance_count = instance_count;
    node.data.first_vertex = vertex_first;
    node.data.first_instance = instance_first;

    vao.bind(node.data.vertex_buffers);
    context.update_pipeline_data(framebuffer, prim_type, vao.vertex_input_key, node.data.graphics);

    render_graph::VKDrawNode::CreateInfo create_info(resource_access_info);
    node.finalize(graph, create_info);
  }
}

void VKBatch::draw_indirect(StorageBuf *indirect_buf, intptr_t offset)
{
  multi_draw_indirect(indirect_buf, 1, offset, 0);
}

void VKBatch::multi_draw_indirect(StorageBuf *indirect_buf,
                                  const int count,
                                  const intptr_t offset,
                                  const intptr_t stride)
{
  VKStorageBuffer &indirect_buffer = *unwrap(unwrap(indirect_buf));
  multi_draw_indirect(indirect_buffer, count, offset, stride);
}

void VKBatch::multi_draw_indirect(const VKStorageBuffer &indirect_buffer,
                                  const int count,
                                  const intptr_t offset,
                                  const intptr_t stride)
{
  ensure_data_uploaded();

  VKContext &context = *VKContext::get();
  render_graph::VKRenderGraph &graph = context.render_graph();
  render_graph::VKResourceAccessInfo &resource_access_info = context.reset_and_get_access_info();

  const VKVertexAttributeObject &vao = get_vertex_attribute_object(context);

  VKIndexBuffer *index_buffer = index_buffer_get();
  const bool draw_indexed = index_buffer != nullptr;

  VKFrameBuffer &framebuffer = *context.active_framebuffer_get();
  framebuffer.rendering_ensure(context);

  if (draw_indexed) {
    render_graph::VKNodeData<render_graph::VKDrawIndexedIndirectNode> node =
        graph.alloc_node<render_graph::VKDrawIndexedIndirectNode>();
    node.data.indirect_buffer = indirect_buffer.resource();
    node.data.offset = offset;
    node.data.draw_count = count;
    node.data.stride = stride;

    node.data.index_buffer.buffer = index_buffer->resource();
    node.data.index_buffer.index_type = index_buffer->vk_index_type();
    vao.bind(node.data.vertex_buffers);
    context.update_pipeline_data(framebuffer, prim_type, vao.vertex_input_key, node.data.graphics);

    render_graph::VKDrawIndexedIndirectNode::CreateInfo create_info(resource_access_info);
    node.finalize(graph, create_info);
  }
  else {
    render_graph::VKNodeData<render_graph::VKDrawIndirectNode> node =
        graph.alloc_node<render_graph::VKDrawIndirectNode>();
    node.data.indirect_buffer = indirect_buffer.resource();
    node.data.offset = offset;
    node.data.draw_count = count;
    node.data.stride = stride;

    vao.bind(node.data.vertex_buffers);
    context.update_pipeline_data(framebuffer, prim_type, vao.vertex_input_key, node.data.graphics);

    render_graph::VKDrawIndirectNode::CreateInfo create_info(resource_access_info);
    node.finalize(graph, create_info);
  }
}

const VKVertexAttributeObject &VKBatch::get_vertex_attribute_object(VKContext &context)
{
  if (flag & GPU_BATCH_DIRTY) {
    flag &= ~GPU_BATCH_DIRTY;
    vao_cache_.clear();
  }
  VKDevice &device = VKBackend::get().device;
  const VKShaderInterface &interface = unwrap(context.shader)->interface_get();
  return vao_cache_.get_or_create(context, *this, interface.id, device.vertex_input_descriptions);
}

}  // namespace blender::gpu
