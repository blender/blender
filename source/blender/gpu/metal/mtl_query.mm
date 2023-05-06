/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "mtl_query.hh"

namespace blender::gpu {

static const size_t VISIBILITY_COUNT_PER_BUFFER = 512;
/* Defined in the documentation but can't be queried programmatically:
 * https://developer.apple.com/documentation/metal/mtlvisibilityresultmode/mtlvisibilityresultmodeboolean?language=objc
 */
static const size_t VISIBILITY_RESULT_SIZE_IN_BYTES = 8;

MTLQueryPool::MTLQueryPool()
{
  allocate();
}
MTLQueryPool::~MTLQueryPool()
{
  for (gpu::MTLBuffer *buf : buffer_) {
    BLI_assert(buf);
    buf->free();
  }
}

void MTLQueryPool::allocate()
{
  /* Allocate Metal buffer for visibility results. */
  size_t buffer_size_in_bytes = VISIBILITY_COUNT_PER_BUFFER * VISIBILITY_RESULT_SIZE_IN_BYTES;
  gpu::MTLBuffer *buffer = MTLContext::get_global_memory_manager()->allocate(buffer_size_in_bytes,
                                                                             true);
  BLI_assert(buffer);

  /* We must zero-initialize the query buffer as visibility queries with no draws between
   * begin and end will not write any result to the buffer. */
  memset(buffer->get_host_ptr(), 0, buffer_size_in_bytes);
  buffer->flush();
  buffer_.append(buffer);
}

static inline MTLVisibilityResultMode to_mtl_type(GPUQueryType type)
{
  if (type == GPU_QUERY_OCCLUSION) {
    return MTLVisibilityResultModeBoolean;
  }
  BLI_assert(0);
  return MTLVisibilityResultModeBoolean;
}

void MTLQueryPool::init(GPUQueryType type)
{
  BLI_assert(initialized_ == false);
  initialized_ = true;
  type_ = type;
  mtl_type_ = to_mtl_type(type);
  query_issued_ = 0;
}

void MTLQueryPool::begin_query()
{
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));

  /* Ensure our allocated buffer pool has enough space for the current queries. */
  int query_id = query_issued_;
  int requested_buffer = query_id / VISIBILITY_COUNT_PER_BUFFER;
  if (requested_buffer >= buffer_.size()) {
    allocate();
  }

  BLI_assert(requested_buffer < buffer_.size());
  gpu::MTLBuffer *buffer = buffer_[requested_buffer];

  /* Ensure visibility buffer is set on the context. If visibility buffer changes,
   * we need to begin a new render pass with an updated reference in the
   * MTLRenderPassDescriptor. */
  ctx->set_visibility_buffer(buffer);

  ctx->ensure_begin_render_pass();
  id<MTLRenderCommandEncoder> rec = ctx->main_command_buffer.get_active_render_command_encoder();
  [rec setVisibilityResultMode:mtl_type_
                        offset:(query_id % VISIBILITY_COUNT_PER_BUFFER) *
                               VISIBILITY_RESULT_SIZE_IN_BYTES];
  query_issued_ += 1;
}

void MTLQueryPool::end_query()
{
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));

  id<MTLRenderCommandEncoder> rec = ctx->main_command_buffer.get_active_render_command_encoder();
  [rec setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
}

void MTLQueryPool::get_occlusion_result(MutableSpan<uint32_t> r_values)
{
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));

  /* Create a blit encoder to synchronize the query buffer results between
   * GPU and CPU when not using shared-memory. */
  if ([ctx->device hasUnifiedMemory] == false) {
    id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
    BLI_assert(blit_encoder);
    for (gpu::MTLBuffer *buf : buffer_) {
      [blit_encoder synchronizeResource:buf->get_metal_buffer()];
    }
    BLI_assert(ctx->get_inside_frame());
  }

  /* Wait for GPU operations to complete and for query buffer contents
   * to be synchronized back to host memory. */
  GPU_finish();

  /* Iterate through all possible visibility buffers and copy results into provided
   * container. */
  for (const int i : IndexRange(query_issued_)) {
    int requested_buffer = i / VISIBILITY_COUNT_PER_BUFFER;
    const uint64_t *queries = static_cast<const uint64_t *>(
        buffer_[requested_buffer]->get_host_ptr());
    r_values[i] = static_cast<uint32_t>(queries[i % VISIBILITY_COUNT_PER_BUFFER]);
  }
  ctx->set_visibility_buffer(nullptr);
}

}  // namespace blender::gpu
