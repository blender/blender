/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_global.h"

#include "DNA_userdef_types.h"

#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_memory.hh"

using namespace blender;
using namespace blender::gpu;

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Memory Management - MTLBufferPool and MTLSafeFreeList implementations. */

void MTLBufferPool::init(id<MTLDevice> mtl_device)
{
  if (!ensure_initialised_) {
    BLI_assert(mtl_device);
    ensure_initialised_ = true;
    device_ = mtl_device;

#if MTL_DEBUG_MEMORY_STATISTICS == 1
    /* Debug statistics. */
    total_allocation_bytes_ = 0;
    per_frame_allocation_count_ = 0;
    allocations_in_pool_ = 0;
    buffers_in_pool_ = 0;
#endif

    /* Free pools -- Create initial safe free pool */
    BLI_assert(current_free_list_ == nullptr);
    this->begin_new_safe_list();
  }
}

MTLBufferPool::~MTLBufferPool()
{
  this->free();
}

void MTLBufferPool::free()
{
  buffer_pool_lock_.lock();
  for (auto buffer : allocations_) {
    BLI_assert(buffer);
    delete buffer;
  }
  allocations_.clear();

  for (std::multiset<blender::gpu::MTLBufferHandle, blender::gpu::CompareMTLBuffer> *buffer_pool :
       buffer_pools_.values()) {
    delete buffer_pool;
  }
  buffer_pools_.clear();
  buffer_pool_lock_.unlock();
}

gpu::MTLBuffer *MTLBufferPool::allocate(uint64_t size, bool cpu_visible)
{
  /* Allocate buffer with default HW-compatible alignment of 256 bytes.
   * See https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf for more. */
  return this->allocate_aligned(size, 256, cpu_visible);
}

gpu::MTLBuffer *MTLBufferPool::allocate_with_data(uint64_t size,
                                                  bool cpu_visible,
                                                  const void *data)
{
  /* Allocate buffer with default HW-compatible alignment of 256 bytes.
   * See https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf for more. */
  return this->allocate_aligned_with_data(size, 256, cpu_visible, data);
}

gpu::MTLBuffer *MTLBufferPool::allocate_aligned(uint64_t size,
                                                uint32_t alignment,
                                                bool cpu_visible)
{
  /* Check not required. Main GPU module usage considered thread-safe. */
  // BLI_assert(BLI_thread_is_main());

  /* Calculate aligned size */
  BLI_assert(alignment > 0);
  uint64_t aligned_alloc_size = ceil_to_multiple_ul(size, alignment);

  /* Allocate new MTL Buffer */
  MTLResourceOptions options;
  if (cpu_visible) {
    options = ([device_ hasUnifiedMemory]) ? MTLResourceStorageModeShared :
                                             MTLResourceStorageModeManaged;
  }
  else {
    options = MTLResourceStorageModePrivate;
  }

  /* Check if we have a suitable buffer */
  gpu::MTLBuffer *new_buffer = nullptr;
  buffer_pool_lock_.lock();

  std::multiset<MTLBufferHandle, CompareMTLBuffer> **pool_search = buffer_pools_.lookup_ptr(
      (uint64_t)options);

  if (pool_search != nullptr) {
    std::multiset<MTLBufferHandle, CompareMTLBuffer> *pool = *pool_search;
    MTLBufferHandle size_compare(aligned_alloc_size);
    auto result = pool->lower_bound(size_compare);
    if (result != pool->end()) {
      /* Potential buffer found, check if within size threshold requirements. */
      gpu::MTLBuffer *found_buffer = result->buffer;
      BLI_assert(found_buffer);
      BLI_assert(found_buffer->get_metal_buffer());

      uint64_t found_size = found_buffer->get_size();

      if (found_size >= aligned_alloc_size &&
          found_size <= (aligned_alloc_size * mtl_buffer_size_threshold_factor_)) {
        MTL_LOG_INFO(
            "[MemoryAllocator] Suitable Buffer of size %lld found, for requested size: %lld\n",
            found_size,
            aligned_alloc_size);

        new_buffer = found_buffer;
        BLI_assert(!new_buffer->get_in_use());

        /* Remove buffer from free set. */
        pool->erase(result);
      }
      else {
        MTL_LOG_INFO(
            "[MemoryAllocator] Buffer of size %lld found, but was incompatible with requested "
            "size: "
            "%lld\n",
            found_size,
            aligned_alloc_size);
        new_buffer = nullptr;
      }
    }
  }

  /* Allocate new buffer. */
  if (new_buffer == nullptr) {
    new_buffer = new gpu::MTLBuffer(device_, size, options, alignment);

    /* Track allocation in context. */
    allocations_.append(new_buffer);
#if MTL_DEBUG_MEMORY_STATISTICS == 1
    total_allocation_bytes_ += aligned_alloc_size;
#endif
  }
  else {
    /* Re-use suitable buffer. */
    new_buffer->set_usage_size(aligned_alloc_size);

#if MTL_DEBUG_MEMORY_STATISTICS == 1
    /* Debug. */
    allocations_in_pool_ -= new_buffer->get_size();
    buffers_in_pool_--;
    BLI_assert(allocations_in_pool_ >= 0);
#endif

    /* Ensure buffer memory is correctly backed. */
    BLI_assert(new_buffer->get_metal_buffer());
  }
  /* Flag buffer as actively in-use. */
  new_buffer->flag_in_use(true);

#if MTL_DEBUG_MEMORY_STATISTICS == 1
  per_frame_allocation_count_++;
#endif

  /* Release lock. */
  buffer_pool_lock_.unlock();

  return new_buffer;
}

gpu::MTLBuffer *MTLBufferPool::allocate_aligned_with_data(uint64_t size,
                                                          uint32_t alignment,
                                                          bool cpu_visible,
                                                          const void *data)
{
  gpu::MTLBuffer *buf = this->allocate_aligned(size, 256, cpu_visible);

  /* Upload initial data. */
  BLI_assert(data != nullptr);
  BLI_assert(!(buf->get_resource_options() & MTLResourceStorageModePrivate));
  BLI_assert(size <= buf->get_size());
  BLI_assert(size <= [buf->get_metal_buffer() length]);
  memcpy(buf->get_host_ptr(), data, size);
  buf->flush_range(0, size);
  return buf;
}

bool MTLBufferPool::free_buffer(gpu::MTLBuffer *buffer)
{
  /* Ensure buffer is flagged as in-use. I.e. has not already been returned to memory pools. */
  bool buffer_in_use = buffer->get_in_use();
  BLI_assert(buffer_in_use);
  if (buffer_in_use) {

    /* Fetch active safe pool from atomic ptr. */
    MTLSafeFreeList *current_pool = this->get_current_safe_list();

    /* Place buffer in safe_free_pool before returning to MemoryManager buffer pools. */
    BLI_assert(current_pool);
    current_pool->insert_buffer(buffer);
    buffer->flag_in_use(false);

    return true;
  }
  return false;
}

void MTLBufferPool::update_memory_pools()
{
  /* Ensure thread-safe access to `completed_safelist_queue_`, which contains
   * the list of MTLSafeFreeList's whose buffers are ready to be
   * re-inserted into the Memory Manager pools.
   * we also need to lock access to general buffer pools, to ensure allocations
   * are not simultaneously happening on background threads. */
  safelist_lock_.lock();
  buffer_pool_lock_.lock();

#if MTL_DEBUG_MEMORY_STATISTICS == 1
  int num_buffers_added = 0;
#endif

  /* Always free oldest MTLSafeFreeList first. */
  for (int safe_pool_free_index = 0; safe_pool_free_index < completed_safelist_queue_.size();
       safe_pool_free_index++) {
    MTLSafeFreeList *current_pool = completed_safelist_queue_[safe_pool_free_index];

    /* Iterate through all MTLSafeFreeList linked-chunks. */
    while (current_pool != nullptr) {
      current_pool->lock_.lock();
      BLI_assert(current_pool);
      BLI_assert(current_pool->in_free_queue_);
      int counter = 0;
      int size = min_ii(current_pool->current_list_index_, MTLSafeFreeList::MAX_NUM_BUFFERS_);

      /* Re-add all buffers within frame index to MemoryManager pools. */
      while (counter < size) {

        gpu::MTLBuffer *buf = current_pool->safe_free_pool_[counter];

        /* Insert buffer back into open pools. */
        BLI_assert(buf->get_in_use() == false);
        this->insert_buffer_into_pool(buf->get_resource_options(), buf);
        counter++;

#if MTL_DEBUG_MEMORY_STATISTICS == 1
        num_buffers_added++;
#endif
      }

      /* Fetch next MTLSafeFreeList chunk, if any. */
      MTLSafeFreeList *next_list = nullptr;
      if (current_pool->has_next_pool_ > 0) {
        next_list = current_pool->next_.load();
      }

      /* Delete current MTLSafeFreeList */
      current_pool->lock_.unlock();
      delete current_pool;
      current_pool = nullptr;

      /* Move onto next chunk. */
      if (next_list != nullptr) {
        current_pool = next_list;
      }
    }
  }

#if MTL_DEBUG_MEMORY_STATISTICS == 1
  printf("--- Allocation Stats ---\n");
  printf("  Num buffers processed in pool (this frame): %u\n", num_buffers_added);

  uint framealloc = (uint)per_frame_allocation_count_;
  printf("  Allocations in frame: %u\n", framealloc);
  printf("  Total Buffers allocated: %u\n", (uint)allocations_.size());
  printf("  Total Memory allocated: %u MB\n", (uint)total_allocation_bytes_ / (1024 * 1024));

  uint allocs = (uint)(allocations_in_pool_) / 1024 / 2024;
  printf("  Free memory in pools: %u MB\n", allocs);

  uint buffs = (uint)buffers_in_pool_;
  printf("  Buffers in pools: %u\n", buffs);

  printf("  Pools %u:\n", (uint)buffer_pools_.size());
  auto key_iterator = buffer_pools_.keys().begin();
  auto value_iterator = buffer_pools_.values().begin();
  while (key_iterator != buffer_pools_.keys().end()) {
    uint64_t mem_in_pool = 0;
    uint64_t iters = 0;
    for (auto it = (*value_iterator)->begin(); it != (*value_iterator)->end(); it++) {
      mem_in_pool += it->buffer_size;
      iters++;
    }

    printf("    Buffers in pool (%u)(%llu): %u (%u MB)\n",
           (uint)*key_iterator,
           iters,
           (uint)((*value_iterator)->size()),
           (uint)mem_in_pool / 1024 / 1024);
    ++key_iterator;
    ++value_iterator;
  }

  per_frame_allocation_count_ = 0;
#endif

  /* Clear safe pools list */
  completed_safelist_queue_.clear();
  buffer_pool_lock_.unlock();
  safelist_lock_.unlock();
}

void MTLBufferPool::push_completed_safe_list(MTLSafeFreeList *safe_list)
{
  /* When an MTLSafeFreeList has been released by the GPU, and buffers are ready to
   * be re-inserted into the MemoryManager pools for future use, add the MTLSafeFreeList
   * to the `completed_safelist_queue_` for flushing at a controlled point in time. */
  safe_list->lock_.lock();
  BLI_assert(safe_list);
  BLI_assert(safe_list->reference_count_ == 0 &&
             "Pool must be fully dereferenced by all in-use cmd buffers before returning.\n");
  BLI_assert(safe_list->in_free_queue_ == false && "Pool must not already be in queue");

  /* Flag MTLSafeFreeList as having been added, and insert into SafeFreePool queue. */
  safe_list->flag_in_queue();
  safelist_lock_.lock();
  completed_safelist_queue_.append(safe_list);
  safelist_lock_.unlock();
  safe_list->lock_.unlock();
}

MTLSafeFreeList *MTLBufferPool::get_current_safe_list()
{
  /* Thread-safe access via atomic ptr. */
  return current_free_list_;
}

void MTLBufferPool::begin_new_safe_list()
{
  safelist_lock_.lock();
  current_free_list_ = new MTLSafeFreeList();
  safelist_lock_.unlock();
}

void MTLBufferPool::ensure_buffer_pool(MTLResourceOptions options)
{
  std::multiset<MTLBufferHandle, CompareMTLBuffer> **pool_search = buffer_pools_.lookup_ptr(
      (uint64_t)options);
  if (pool_search == nullptr) {
    std::multiset<MTLBufferHandle, CompareMTLBuffer> *pool =
        new std::multiset<MTLBufferHandle, CompareMTLBuffer>();
    buffer_pools_.add_new((uint64_t)options, pool);
  }
}

void MTLBufferPool::insert_buffer_into_pool(MTLResourceOptions options, gpu::MTLBuffer *buffer)
{
  /* Ensure `safelist_lock_` is locked in calling code before modifying. */
  BLI_assert(buffer);

  /* Reset usage size to actual size of allocation. */
  buffer->set_usage_size(buffer->get_size());

  /* Ensure pool exists. */
  this->ensure_buffer_pool(options);

  /* TODO(Metal): Support purgeability - Allow buffer in pool to have its memory taken back by the
   * OS if needed. As we keep allocations around, they may not actually be in use, but we can
   * ensure they do not block other apps from using memory. Upon a buffer being needed again, we
   * can reset this state.
   *  TODO(Metal): Purgeability state does not update instantly, so this requires a deferral. */
  BLI_assert(buffer->get_metal_buffer());
  /* buffer->metal_buffer); [buffer->metal_buffer setPurgeableState:MTLPurgeableStateVolatile]; */

  std::multiset<MTLBufferHandle, CompareMTLBuffer> *pool = buffer_pools_.lookup(options);
  pool->insert(MTLBufferHandle(buffer));

#if MTL_DEBUG_MEMORY_STATISTICS == 1
  /* Debug statistics. */
  allocations_in_pool_ += buffer->get_size();
  buffers_in_pool_++;
#endif
}

MTLSafeFreeList::MTLSafeFreeList()
{
  reference_count_ = 1;
  in_free_queue_ = false;
  current_list_index_ = 0;
  next_ = nullptr;
  has_next_pool_ = 0;
}

void MTLSafeFreeList::insert_buffer(gpu::MTLBuffer *buffer)
{
  BLI_assert(in_free_queue_ == false);

  /* Lockless list insert. */
  uint insert_index = current_list_index_++;

  /* If the current MTLSafeFreeList size is exceeded, we ripple down the linked-list chain and
   * insert the buffer into the next available chunk. */
  if (insert_index >= MTLSafeFreeList::MAX_NUM_BUFFERS_) {

    /* Check if first caller to generate next pool. */
    int has_next = has_next_pool_++;
    if (has_next == 0) {
      next_ = new MTLSafeFreeList();
    }
    MTLSafeFreeList *next_list = next_.load();
    BLI_assert(next_list);
    next_list->insert_buffer(buffer);

    /* Clamp index to chunk limit if overflowing. */
    current_list_index_ = MTLSafeFreeList::MAX_NUM_BUFFERS_;
    return;
  }

  safe_free_pool_[insert_index] = buffer;
}

/* Increments from active GPUContext thread. */
void MTLSafeFreeList::increment_reference()
{
  lock_.lock();
  BLI_assert(in_free_queue_ == false);
  reference_count_++;
  referenced_by_workload_ = true;
  lock_.unlock();
}

/* Reference decrements and addition to completed list queue can occur from MTLCommandBuffer
 * completion callback thread. */
void MTLSafeFreeList::decrement_reference()
{
  lock_.lock();
  BLI_assert(in_free_queue_ == false);
  int ref_count = --reference_count_;

  if (ref_count == 0) {
    MTLContext::get_global_memory_manager()->push_completed_safe_list(this);
  }
  lock_.unlock();
}

bool MTLSafeFreeList::should_flush()
{
  /* We should only consider refreshing a list if it has been referenced by active workloads, and
   * contains a sufficient buffer count to avoid overheads associated with flushing the list. If
   * the reference count is only equal to 1, buffers may have been added, but no command
   * submissions will have been issued, hence buffers could be returned to the pool prematurely if
   * associated workload submission occurs later. */
  return ((reference_count_ > 1 || referenced_by_workload_) &&
          current_list_index_ > MIN_BUFFER_FLUSH_COUNT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MTLBuffer wrapper class implementation.
 * \{ */

/* Construct a gpu::MTLBuffer wrapper around a newly created metal::MTLBuffer. */
MTLBuffer::MTLBuffer(id<MTLDevice> mtl_device,
                     uint64_t size,
                     MTLResourceOptions options,
                     uint alignment)
{
  /* Calculate aligned allocation size. */
  BLI_assert(alignment > 0);
  uint64_t aligned_alloc_size = ceil_to_multiple_ul(size, alignment);

  alignment_ = alignment;
  device_ = mtl_device;
  is_external_ = false;

  options_ = options;
  this->flag_in_use(false);

  metal_buffer_ = [device_ newBufferWithLength:aligned_alloc_size options:options];
  BLI_assert(metal_buffer_);

  size_ = aligned_alloc_size;
  this->set_usage_size(size_);
  if (!(options_ & MTLResourceStorageModePrivate)) {
    data_ = [metal_buffer_ contents];
  }
  else {
    data_ = nullptr;
  }
}

MTLBuffer::MTLBuffer(id<MTLBuffer> external_buffer)
{
  BLI_assert(external_buffer != nil);

  /* Ensure external_buffer remains referenced while in-use. */
  metal_buffer_ = external_buffer;
  [metal_buffer_ retain];

  /* Extract properties. */
  is_external_ = true;
  device_ = nil;
  alignment_ = 1;
  options_ = [metal_buffer_ resourceOptions];
  size_ = [metal_buffer_ allocatedSize];
  this->set_usage_size(size_);
  data_ = [metal_buffer_ contents];
  in_use_ = true;
}

gpu::MTLBuffer::~MTLBuffer()
{
  if (metal_buffer_ != nil) {
    [metal_buffer_ release];
    metal_buffer_ = nil;
  }
}

void gpu::MTLBuffer::free()
{
  if (!is_external_) {
    MTLContext::get_global_memory_manager()->free_buffer(this);
  }
  else {
    if (metal_buffer_ != nil) {
      [metal_buffer_ release];
      metal_buffer_ = nil;
    }
  }
}

id<MTLBuffer> gpu::MTLBuffer::get_metal_buffer() const
{
  return metal_buffer_;
}

void *gpu::MTLBuffer::get_host_ptr() const
{
  BLI_assert(!(options_ & MTLResourceStorageModePrivate));
  BLI_assert(data_);
  return data_;
}

uint64_t gpu::MTLBuffer::get_size() const
{
  return size_;
}

uint64_t gpu::MTLBuffer::get_size_used() const
{
  return usage_size_;
}

bool gpu::MTLBuffer::requires_flush()
{
  /* We do not need to flush shared memory, as addressable buffer is shared. */
  return options_ & MTLResourceStorageModeManaged;
}

void gpu::MTLBuffer::set_label(NSString *str)
{
  metal_buffer_.label = str;
}

void gpu::MTLBuffer::debug_ensure_used()
{
  /* Debug: If buffer is not flagged as in-use, this is a problem. */
  BLI_assert_msg(
      in_use_,
      "Buffer should be marked as 'in-use' if being actively used by an instance. Buffer "
      "has likely already been freed.");
}

void gpu::MTLBuffer::flush()
{
  this->debug_ensure_used();
  if (this->requires_flush()) {
    [metal_buffer_ didModifyRange:NSMakeRange(0, size_)];
  }
}

void gpu::MTLBuffer::flush_range(uint64_t offset, uint64_t length)
{
  this->debug_ensure_used();
  if (this->requires_flush()) {
    BLI_assert((offset + length) <= size_);
    [metal_buffer_ didModifyRange:NSMakeRange(offset, length)];
  }
}

void gpu::MTLBuffer::flag_in_use(bool used)
{
  in_use_ = used;
}

bool gpu::MTLBuffer::get_in_use()
{
  return in_use_;
}

void gpu::MTLBuffer::set_usage_size(uint64_t size_used)
{
  BLI_assert(size_used > 0 && size_used <= size_);
  usage_size_ = size_used;
}

MTLResourceOptions gpu::MTLBuffer::get_resource_options()
{
  return options_;
}

uint64_t gpu::MTLBuffer::get_alignment()
{
  return alignment_;
}

bool MTLBufferRange::requires_flush()
{
  /* We do not need to flush shared memory. */
  return this->options & MTLResourceStorageModeManaged;
}

void MTLBufferRange::flush()
{
  if (this->requires_flush()) {
    BLI_assert(this->metal_buffer);
    BLI_assert((this->buffer_offset + this->size) <= [this->metal_buffer length]);
    BLI_assert(this->buffer_offset >= 0);
    [this->metal_buffer
        didModifyRange:NSMakeRange(this->buffer_offset, this->size - this->buffer_offset)];
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MTLScratchBufferManager and MTLCircularBuffer implementation.
 * \{ */

MTLScratchBufferManager::~MTLScratchBufferManager()
{
  this->free();
}

void MTLScratchBufferManager::init()
{

  if (!this->initialised_) {
    BLI_assert(context_.device);

    /* Initialize Scratch buffers. */
    for (int sb = 0; sb < mtl_max_scratch_buffers_; sb++) {
      scratch_buffers_[sb] = new MTLCircularBuffer(
          context_, mtl_scratch_buffer_initial_size_, true);
      BLI_assert(scratch_buffers_[sb]);
      BLI_assert(&(scratch_buffers_[sb]->own_context_) == &context_);
    }
    current_scratch_buffer_ = 0;
    initialised_ = true;
  }
}

void MTLScratchBufferManager::free()
{
  initialised_ = false;

  /* Release Scratch buffers */
  for (int sb = 0; sb < mtl_max_scratch_buffers_; sb++) {
    delete scratch_buffers_[sb];
    scratch_buffers_[sb] = nullptr;
  }
  current_scratch_buffer_ = 0;
}

MTLTemporaryBuffer MTLScratchBufferManager::scratch_buffer_allocate_range(uint64_t alloc_size)
{
  return this->scratch_buffer_allocate_range_aligned(alloc_size, 1);
}

MTLTemporaryBuffer MTLScratchBufferManager::scratch_buffer_allocate_range_aligned(
    uint64_t alloc_size, uint alignment)
{
  /* Ensure scratch buffer allocation alignment adheres to offset alignment requirements. */
  alignment = max_uu(alignment, 256);

  BLI_assert_msg(current_scratch_buffer_ >= 0, "Scratch Buffer index not set");
  MTLCircularBuffer *current_scratch_buff = this->scratch_buffers_[current_scratch_buffer_];
  BLI_assert_msg(current_scratch_buff != nullptr, "Scratch Buffer does not exist");
  MTLTemporaryBuffer allocated_range = current_scratch_buff->allocate_range_aligned(alloc_size,
                                                                                    alignment);
  BLI_assert(allocated_range.size >= alloc_size && allocated_range.size <= alloc_size + alignment);
  BLI_assert(allocated_range.metal_buffer != nil);
  return allocated_range;
}

void MTLScratchBufferManager::ensure_increment_scratch_buffer()
{
  /* Fetch active scratch buffer. */
  MTLCircularBuffer *active_scratch_buf = scratch_buffers_[current_scratch_buffer_];
  BLI_assert(&active_scratch_buf->own_context_ == &context_);

  /* Ensure existing scratch buffer is no longer in use. MTL_MAX_SCRATCH_BUFFERS specifies
   * the number of allocated scratch buffers. This value should be equal to the number of
   * simultaneous frames in-flight. I.e. the maximal number of scratch buffers which are
   * simultaneously in-use. */
  if (active_scratch_buf->used_frame_index_ < context_.get_current_frame_index()) {
    current_scratch_buffer_ = (current_scratch_buffer_ + 1) % mtl_max_scratch_buffers_;
    active_scratch_buf = scratch_buffers_[current_scratch_buffer_];
    active_scratch_buf->reset();
    BLI_assert(&active_scratch_buf->own_context_ == &context_);
    MTL_LOG_INFO("Scratch buffer %d reset - (ctx %p)(Frame index: %d)\n",
                 current_scratch_buffer_,
                 &context_,
                 context_.get_current_frame_index());
  }
}

void MTLScratchBufferManager::flush_active_scratch_buffer()
{
  /* Fetch active scratch buffer and verify context. */
  MTLCircularBuffer *active_scratch_buf = scratch_buffers_[current_scratch_buffer_];
  BLI_assert(&active_scratch_buf->own_context_ == &context_);
  active_scratch_buf->flush();
}

/* MTLCircularBuffer implementation. */
MTLCircularBuffer::MTLCircularBuffer(MTLContext &ctx, uint64_t initial_size, bool allow_grow)
    : own_context_(ctx)
{
  BLI_assert(this);
  MTLResourceOptions options = ([own_context_.device hasUnifiedMemory]) ?
                                   MTLResourceStorageModeShared :
                                   MTLResourceStorageModeManaged;
  cbuffer_ = new gpu::MTLBuffer(own_context_.device, initial_size, options, 256);
  current_offset_ = 0;
  can_resize_ = allow_grow;
  cbuffer_->flag_in_use(true);

  used_frame_index_ = ctx.get_current_frame_index();
  last_flush_base_offset_ = 0;

  /* Debug label. */
  if (G.debug & G_DEBUG_GPU) {
    cbuffer_->set_label(@"Circular Scratch Buffer");
  }
}

MTLCircularBuffer::~MTLCircularBuffer()
{
  delete cbuffer_;
}

MTLTemporaryBuffer MTLCircularBuffer::allocate_range(uint64_t alloc_size)
{
  return this->allocate_range_aligned(alloc_size, 1);
}

MTLTemporaryBuffer MTLCircularBuffer::allocate_range_aligned(uint64_t alloc_size, uint alignment)
{
  BLI_assert(this);

  /* Ensure alignment of an allocation is aligned to compatible offset boundaries. */
  BLI_assert(alignment > 0);
  alignment = max_ulul(alignment, 256);

  /* Align current offset and allocation size to desired alignment */
  uint64_t aligned_current_offset = ceil_to_multiple_ul(current_offset_, alignment);
  uint64_t aligned_alloc_size = ceil_to_multiple_ul(alloc_size, alignment);
  bool can_allocate = (aligned_current_offset + aligned_alloc_size) < cbuffer_->get_size();

  BLI_assert(aligned_current_offset >= current_offset_);
  BLI_assert(aligned_alloc_size >= alloc_size);

  BLI_assert(aligned_current_offset % alignment == 0);
  BLI_assert(aligned_alloc_size % alignment == 0);

  /* Recreate Buffer */
  if (!can_allocate) {
    uint64_t new_size = cbuffer_->get_size();
    if (can_resize_) {
      /* Resize to the maximum of basic resize heuristic OR the size of the current offset +
       * requested allocation -- we want the buffer to grow to a large enough size such that it
       * does not need to resize mid-frame. */
      new_size = max_ulul(
          min_ulul(MTLScratchBufferManager::mtl_scratch_buffer_max_size_, new_size * 1.2),
          aligned_current_offset + aligned_alloc_size);

#if MTL_SCRATCH_BUFFER_ALLOW_TEMPORARY_EXPANSION == 1
      /* IF a requested allocation EXCEEDS the maximum supported size, temporarily allocate up to
       * this, but shrink down ASAP. */
      if (new_size > MTLScratchBufferManager::mtl_scratch_buffer_max_size_) {

        /* If new requested allocation is bigger than maximum allowed size, temporarily resize to
         * maximum allocation size -- Otherwise, clamp the buffer size back down to the defined
         * maximum */
        if (aligned_alloc_size > MTLScratchBufferManager::mtl_scratch_buffer_max_size_) {
          new_size = aligned_alloc_size;
          MTL_LOG_INFO("Temporarily growing Scratch buffer to %d MB\n",
                       (int)new_size / 1024 / 1024);
        }
        else {
          new_size = MTLScratchBufferManager::mtl_scratch_buffer_max_size_;
          MTL_LOG_INFO("Shrinking Scratch buffer back to %d MB\n", (int)new_size / 1024 / 1024);
        }
      }
      BLI_assert(aligned_alloc_size <= new_size);
#else
      new_size = min_ulul(MTLScratchBufferManager::mtl_scratch_buffer_max_size_, new_size);

      if (aligned_alloc_size > new_size) {
        BLI_assert(false);

        /* Cannot allocate */
        MTLTemporaryBuffer alloc_range;
        alloc_range.metal_buffer = nil;
        alloc_range.data = nullptr;
        alloc_range.buffer_offset = 0;
        alloc_range.size = 0;
        alloc_range.options = cbuffer_->options;
      }
#endif
    }
    else {
      MTL_LOG_WARNING(
          "Performance Warning: Reached the end of circular buffer of size: %llu, but cannot "
          "resize. Starting new buffer\n",
          cbuffer_->get_size());
      BLI_assert(aligned_alloc_size <= new_size);

      /* Cannot allocate. */
      MTLTemporaryBuffer alloc_range;
      alloc_range.metal_buffer = nil;
      alloc_range.data = nullptr;
      alloc_range.buffer_offset = 0;
      alloc_range.size = 0;
      alloc_range.options = cbuffer_->get_resource_options();
    }

    /* Flush current buffer to ensure changes are visible on the GPU. */
    this->flush();

    /* Discard old buffer and create a new one - Relying on Metal reference counting to track
     * in-use buffers */
    MTLResourceOptions prev_options = cbuffer_->get_resource_options();
    uint prev_alignment = cbuffer_->get_alignment();
    delete cbuffer_;
    cbuffer_ = new gpu::MTLBuffer(own_context_.device, new_size, prev_options, prev_alignment);
    cbuffer_->flag_in_use(true);
    current_offset_ = 0;
    last_flush_base_offset_ = 0;

    /* Debug label. */
    if (G.debug & G_DEBUG_GPU) {
      cbuffer_->set_label(@"Circular Scratch Buffer");
    }
    MTL_LOG_INFO("Resized Metal circular buffer to %llu bytes\n", new_size);

    /* Reset allocation Status. */
    aligned_current_offset = 0;
    BLI_assert((aligned_current_offset + aligned_alloc_size) <= cbuffer_->get_size());
  }

  /* Allocate chunk. */
  MTLTemporaryBuffer alloc_range;
  alloc_range.metal_buffer = cbuffer_->get_metal_buffer();
  alloc_range.data = (void *)((uint8_t *)([alloc_range.metal_buffer contents]) +
                              aligned_current_offset);
  alloc_range.buffer_offset = aligned_current_offset;
  alloc_range.size = aligned_alloc_size;
  alloc_range.options = cbuffer_->get_resource_options();
  BLI_assert(alloc_range.data);

  /* Shift offset to match alignment. */
  current_offset_ = aligned_current_offset + aligned_alloc_size;
  BLI_assert(current_offset_ <= cbuffer_->get_size());
  return alloc_range;
}

void MTLCircularBuffer::flush()
{
  BLI_assert(this);

  uint64_t len = current_offset_ - last_flush_base_offset_;
  if (len > 0) {
    cbuffer_->flush_range(last_flush_base_offset_, len);
    last_flush_base_offset_ = current_offset_;
  }
}

void MTLCircularBuffer::reset()
{
  BLI_assert(this);

  /* If circular buffer has data written to it, offset will be greater than zero. */
  if (current_offset_ > 0) {

    /* Ensure the circular buffer is no longer being used by an in-flight frame. */
    BLI_assert((own_context_.get_current_frame_index() >=
                (used_frame_index_ + MTL_NUM_SAFE_FRAMES - 1)) &&
               "Trying to reset Circular scratch buffer's while its data is still being used by "
               "an in-flight frame");

    current_offset_ = 0;
    last_flush_base_offset_ = 0;
  }

  /* Update used frame index to current. */
  used_frame_index_ = own_context_.get_current_frame_index();
}

/** \} */

}  // blender::gpu
