/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_query.hh"
#include "vk_backend.hh"
#include "vk_context.hh"

#include "GPU_debug.hh"

namespace blender::gpu {

VKQueryPool::~VKQueryPool()
{
  VKBackend &backend = VKBackend::get();
  const VKDevice &device = backend.device;

  while (!vk_query_pools_.is_empty()) {
    VkQueryPool vk_query_pool = vk_query_pools_.pop_last();
    vkDestroyQueryPool(device.vk_handle(), vk_query_pool, nullptr);
  }
}

uint32_t VKQueryPool::query_index_in_pool() const
{
  return queries_issued_ - (vk_query_pools_.size() - 1) * query_chunk_len_;
}

void VKQueryPool::init(GPUQueryType type)
{
  BLI_assert(vk_query_pools_.is_empty());
  queries_issued_ = 0;
  vk_query_type_ = to_vk_query_type(type);
}

void VKQueryPool::begin_query()
{
  VKBackend &backend = VKBackend::get();
  const VKDevice &device = backend.device;

  uint32_t pool_index = queries_issued_ / query_chunk_len_;
  bool is_new_pool = (queries_issued_ % query_chunk_len_) == 0;

  if (pool_index == vk_query_pools_.size()) {
    BLI_assert(is_new_pool);

    VkQueryPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    create_info.queryType = vk_query_type_;
    create_info.queryCount = query_chunk_len_;

    VkQueryPool vk_query_pool = VK_NULL_HANDLE;
    vkCreateQueryPool(device.vk_handle(), &create_info, nullptr, &vk_query_pool);
    vk_query_pools_.append(vk_query_pool);
  }
  BLI_assert(pool_index < vk_query_pools_.size());

  /* When using a new query pool make sure to reset it before first usage. */
  VKContext &context = *VKContext::get();
  VkQueryPool vk_query_pool = vk_query_pools_[pool_index];
  if (is_new_pool) {
    render_graph::VKResetQueryPoolNode::Data reset_query_pool = {};
    reset_query_pool.vk_query_pool = vk_query_pool;
    reset_query_pool.first_query = 0;
    reset_query_pool.query_count = query_chunk_len_;
    context.render_graph().add_node(reset_query_pool);
  }

  render_graph::VKBeginQueryNode::Data begin_query = {};
  begin_query.vk_query_pool = vk_query_pool;
  begin_query.query_index = query_index_in_pool();
  context.render_graph().add_node(begin_query);
}

void VKQueryPool::end_query()
{
  VKContext &context = *VKContext::get();
  render_graph::VKEndQueryNode::Data end_query = {};
  end_query.vk_query_pool = vk_query_pools_.last();
  end_query.query_index = query_index_in_pool();
  context.render_graph().add_node(end_query);
  queries_issued_ += 1;
}

void VKQueryPool::get_occlusion_result(MutableSpan<uint32_t> r_values)
{
  VKContext &context = *VKContext::get();
  /* During selection the frame buffer is still rendering. It needs to finish the render scope to
   * ensure the END_RENDERING node */
  context.rendering_end();
  context.flush_render_graph(RenderGraphFlushFlags::SUBMIT |
                             RenderGraphFlushFlags::WAIT_FOR_COMPLETION |
                             RenderGraphFlushFlags::RENEW_RENDER_GRAPH);

  int queries_left = queries_issued_;
  int pool_index = 0;
  VKBackend &backend = VKBackend::get();
  const VKDevice &device = backend.device;
  while (queries_left) {
    VkQueryPool vk_query_pool = vk_query_pools_[pool_index];
    uint32_t *r_values_chunk = &r_values[pool_index * query_chunk_len_];
    uint32_t values_chunk_size = min_ii(queries_left, query_chunk_len_);
    vkGetQueryPoolResults(device.vk_handle(),
                          vk_query_pool,
                          0,
                          values_chunk_size,
                          values_chunk_size * sizeof(uint32_t),
                          r_values_chunk,
                          sizeof(uint32_t),
                          VK_QUERY_RESULT_WAIT_BIT);

    queries_left = max_ii(queries_left - query_chunk_len_, 0);
    pool_index += 1;
  }
}

}  // namespace blender::gpu
