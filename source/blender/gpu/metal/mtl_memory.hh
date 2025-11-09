/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"

#include "mtl_common.hh"

#include <atomic>
#include <ctime>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLRenderPipelineState;

/* Metal Memory Manager Overview. */
/*
 * The Metal Backend Memory manager is designed to provide an interface
 * for all other MTL_* modules where memory allocation is required.
 *
 * Different allocation strategies and data-structures are used depending
 * on how the data is used by the backend. These aim to optimally handle
 * system memory and abstract away any complexity from the MTL_* modules
 * themselves.
 *
 * There are two primary allocation modes which can be used:
 *
 * ** MTLScratchBufferManager **
 *
 *    Each MTLContext owns a ScratchBufferManager which is implemented
 *    as a pool of circular buffers, designed to handle temporary
 *    memory allocations which occur on a per-frame basis. The scratch
 *    buffers allow flushing of host memory to the GPU to be batched.
 *
 *    Each frame, the next scratch buffer is reset, then later flushed upon
 *    command buffer submission.
 *
 *    NOTE: This is allocated per-context due to allocations being tied
 *    to workload submissions and context-specific submissions.
 *
 *    Examples of scratch buffer usage are:
 *      - Immediate-mode temporary vertex buffers.
 *      - Shader uniform data updates
 *      - Staging of data for resource copies, or, data reads/writes.
 *
 *  Usage:
 *
 *    MTLContext::get_scratch_buffer_manager() - to fetch active manager.
 *
 *    MTLTemporaryBuffer scratch_buffer_allocate_range(size)
 *    MTLTemporaryBuffer scratch_buffer_allocate_range_aligned(size, align)
 *
 * ---------------------------------------------------------------------------------
 *  ** MTLBufferPool **
 *
 *    For static and longer-lasting memory allocations, such as those for UBOs,
 *    Vertex buffers, index buffers, etc; We want an optimal abstraction for
 *    fetching a MTLBuffer of the desired size and resource options.
 *
 *    Memory allocations can be expensive so the MTLBufferPool provides
 *    functionality to track usage of these buffers and once a buffer
 *    is no longer in use, it is returned to the buffer pool for use
 *    by another backend resource.
 *
 *    The MTLBufferPool provides functionality for safe tracking of resources,
 *    as buffers freed on the host side must have their usage by the GPU tracked,
 *    to ensure they are not prematurely re-used before they have finished being
 *    used by the GPU.
 *
 *    NOTE: The MTLBufferPool is a global construct which can be fetched from anywhere.
 *
 *  Usage:
 *    MTLContext::get_global_memory_manager();  - static routine to fetch global memory manager.
 *
 *    gpu::MTLBuffer *allocate(size, is_cpu_visibile)
 *    gpu::MTLBuffer *allocate_aligned(size, alignment, is_cpu_visibile)
 *    gpu::MTLBuffer *allocate_with_data(size, is_cpu_visibile, data_ptr)
 *    gpu::MTLBuffer *allocate_aligned_with_data(size, alignment, is_cpu_visibile, data_ptr)
 */

/* Debug memory statistics: Disabled by Macro rather than guarded for
 * performance considerations. */
#define MTL_DEBUG_MEMORY_STATISTICS 0

namespace blender::gpu {

/* Forward Declarations. */
class MTLContext;
class MTLCommandBufferManager;
class MTLUniformBuf;
class MTLStorageBuf;

/* -------------------------------------------------------------------- */
/** \name Memory Management.
 * \{ */

/* MTLBuffer allocation wrapper. */
class MTLBuffer {

 public:
  /* NOTE: ListBase API is not used due to custom destructor operation required to release
   * Metal objective C buffer resource. */
  gpu::MTLBuffer *next, *prev;

 private:
  /* Metal resource. */
  id<MTLBuffer> metal_buffer_;

  /* Host-visible mapped-memory pointer. Behavior depends on buffer type:
   * - Shared buffers: pointer represents base address of #MTLBuffer whose data
   *                   access has shared access by both the CPU and GPU on
   *                   Unified Memory Architectures (UMA).
   * - Managed buffer: Host-side mapped buffer region for CPU (Host) access. Managed buffers
   *                   must be manually flushed to transfer data to GPU-resident buffer.
   * - Private buffer: Host access is invalid, `data` will be nullptr. */
  void *data_;

  /* Whether buffer is allocated from an external source. */
  bool is_external_ = false;

  /* Allocation info. */
  MTLResourceOptions options_;
  id<MTLDevice> device_;
  uint64_t alignment_;
  uint64_t size_;

  /* Allocated size may be larger than actual size. */
  uint64_t usage_size_;

  /* Lifetime info - whether the current buffer is actively in use. A buffer
   * should be in use after it has been allocated. De-allocating the buffer, and
   * returning it to the free buffer pool will set in_use to false. Using a buffer
   * while it is not in-use should not be allowed and result in an error. */
  std::atomic<bool> in_use_;

 public:
  MTLBuffer(id<MTLDevice> device, uint64_t size, MTLResourceOptions options, uint alignment = 1);
  MTLBuffer(id<MTLBuffer> external_buffer);
  ~MTLBuffer();

  /* Fetch information about backing MTLBuffer. */
  id<MTLBuffer> get_metal_buffer() const;
  void *get_host_ptr() const;
  uint64_t get_size_used() const;
  uint64_t get_size() const;

  /* Flush data to GPU. */
  void flush();
  void flush_range(uint64_t offset, uint64_t length);
  bool requires_flush();

  /* Buffer usage tracking. */
  void flag_in_use(bool used);
  bool get_in_use();
  void set_usage_size(uint64_t size_used);

  /* Debug. */
  void set_label(NSString *str);

  /* Read properties. */
  MTLResourceOptions get_resource_options();
  uint64_t get_alignment();

  /* Resource-local free: For buffers allocated via memory manager,
   * this will call the context `free_buffer` method to return the buffer to the context memory
   * pool.
   *
   * Otherwise, free will release the associated metal resource.
   * As a note, calling the destructor will also destroy the buffer and associated metal
   * resource. */
  void free();

  /* Safety check to ensure buffers are not used after free. */
  void debug_ensure_used();

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLBuffer");
};

/* View into part of an MTLBuffer. */
struct MTLBufferRange {
  id<MTLBuffer> metal_buffer;
  void *data;
  uint64_t buffer_offset;
  uint64_t size;
  MTLResourceOptions options;

  void flush();
  bool requires_flush();
};

/* Circular scratch buffer allocations should be seen as temporary and only used within the
 * lifetime of the frame. */
using MTLTemporaryBuffer = MTLBufferRange;

/* Round-Robin Circular-buffer. */
class MTLCircularBuffer {
  friend class MTLScratchBufferManager;

 private:
  MTLContext &own_context_;

  /* Wrapped MTLBuffer allocation handled. */
  gpu::MTLBuffer *cbuffer_;
  /* Allocated SSBO that serves as source for cbuffer. */
  MTLStorageBuf *ssbo_source_ = nullptr;

  /* Current offset where next allocation will begin. */
  uint64_t current_offset_;

  /* Whether the Circular Buffer can grow during re-allocation if
   * the size is exceeded. */
  bool can_resize_;

  /* Usage information. */
  uint64_t used_frame_index_;
  uint64_t last_flush_base_offset_;

 public:
  MTLCircularBuffer(MTLContext &ctx, uint64_t initial_size, bool allow_grow);
  ~MTLCircularBuffer();
  MTLTemporaryBuffer allocate_range(uint64_t alloc_size);
  MTLTemporaryBuffer allocate_range_aligned(uint64_t alloc_size, uint alignment);
  void flush();

  /* Reset pointer back to start of circular buffer. */
  void reset();
};

/* Wrapper struct used by Memory Manager to sort and compare gpu::MTLBuffer resources inside the
 * memory pools. */
struct MTLBufferHandle {
  gpu::MTLBuffer *buffer;
  uint64_t buffer_size;
  time_t insert_time;

  inline MTLBufferHandle(gpu::MTLBuffer *buf)
  {
    this->buffer = buf;
    this->buffer_size = this->buffer->get_size();
    this->insert_time = std::time(nullptr);
  }

  inline MTLBufferHandle(uint64_t compare_size)
  {
    this->buffer = nullptr;
    this->buffer_size = compare_size;
    this->insert_time = 0;
  }
};

struct CompareMTLBuffer {
  bool operator()(const MTLBufferHandle &lhs, const MTLBufferHandle &rhs) const
  {
    return lhs.buffer_size < rhs.buffer_size;
  }
};

/**
 * An #MTLSafeFreeList is a temporary list of #gpu::MTLBuffers which have
 * been freed by the high level backend, but are pending GPU work execution before
 * the #gpu::MTLBuffers can be returned to the Memory manager pools.
 * This list is implemented as a chunked linked-list.
 *
 * Only a single #MTLSafeFreeList is active at one time and is associated with current command
 * buffer submissions. If an #MTLBuffer is freed during the lifetime of a command buffer, it could
 * still possibly be in-use and as such, the #MTLSafeFreeList will increment its reference count
 * for each command buffer submitted while the current pool is active.
 *
 * - Reference count is incremented upon #MTLCommandBuffer commit.
 * - Reference count is decremented in the #MTLCommandBuffer completion callback handler.
 *
 * A new #MTLSafeFreeList will begin each render step (frame). This pooling of buffers, rather than
 * individual buffer resource tracking reduces performance overhead.
 *
 * - The reference count starts at 1 to ensure that the reference count cannot prematurely reach
 *   zero until any command buffers have been submitted. This additional decrement happens
 *   when the next #MTLSafeFreeList is created, to allow the existing pool to be released once
 *   the reference count hits zero after submitted command buffers complete.
 *
 * NOTE: the Metal API independently tracks resources used by command buffers for the purpose of
 * keeping resources alive while in-use by the driver and CPU, however, this differs from the
 * #MTLSafeFreeList mechanism in the Metal backend, which exists for the purpose of allowing
 * previously allocated #MTLBuffer resources to be re-used. This allows us to save on the expensive
 * cost of memory allocation.
 */
class MTLSafeFreeList {
  friend class MTLBufferPool;

 private:
  std::atomic<int> reference_count_;
  std::atomic<bool> in_free_queue_;
  std::atomic<bool> referenced_by_workload_;
  std::recursive_mutex lock_;
  /* Linked list of next MTLSafeFreeList chunk if current chunk is full. */
  std::atomic<MTLSafeFreeList *> next_;

  /* Lockless list. MAX_NUM_BUFFERS_ within a chunk based on considerations
   * for performance and memory. Higher chunk counts are preferable for efficiently
   * performing block operations such as copying several objects simultaneously.
   *
   * MIN_BUFFER_FLUSH_COUNT refers to the minimum count of buffers in the MTLSafeFreeList
   * before buffers are returned to global memory pool. This is set at a point to reduce
   * overhead of small pool flushes, while ensuring floating memory overhead is not excessive. */
  static const int MAX_NUM_BUFFERS_ = 8192;
  static const int MIN_BUFFER_FLUSH_COUNT = 120;
  std::atomic<int> current_list_index_;
  gpu::MTLBuffer *safe_free_pool_[MAX_NUM_BUFFERS_];

 public:
  MTLSafeFreeList();

  /* Can be used from multiple threads. Performs insertion into Safe Free List with the least
   * amount of threading synchronization. */
  void insert_buffer(gpu::MTLBuffer *buffer);

  /* Whether we need to start a new safe free list, or can carry on using the existing one. */
  bool should_flush();

  /* Increments command buffer reference count. */
  void increment_reference();

  /* Decrement and return of buffers to pool occur on MTLCommandBuffer completion callback. */
  void decrement_reference();

  void flag_in_queue()
  {
    in_free_queue_ = true;
    if (current_list_index_ >= MTLSafeFreeList::MAX_NUM_BUFFERS_) {
      MTLSafeFreeList *next_pool = next_.load();
      if (next_pool) {
        next_pool->flag_in_queue();
      }
    }
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLSafeFreeList");
};

/* MTLBuffer pools. */
/* Allocating Metal buffers is expensive, so we cache all allocated buffers,
 * and when requesting a new buffer, find one which fits the required dimensions
 * from an existing pool of buffers.
 *
 * When freeing MTLBuffers, we insert them into the current MTLSafeFreeList, which defers
 * release of the buffer until the associated command buffers have finished executing.
 * This prevents a buffer from being re-used while it is still in-use by the GPU.
 *
 * * Once command buffers complete, MTLSafeFreeList's associated with the current
 *   command buffer submission are added to the `completed_safelist_queue_`.
 *
 * * At a set point in time, all MTLSafeFreeList's in `completed_safelist_queue_` have their
 *   MTLBuffers re-inserted into the Memory Manager's pools. */
class MTLBufferPool {

 private:
#if MTL_DEBUG_MEMORY_STATISTICS == 1
  /* Memory statistics. */
  std::atomic<int64_t> total_allocation_bytes_;

  /* Debug statistics. */
  std::atomic<int> per_frame_allocation_count_;
  std::atomic<int64_t> buffers_in_pool_;
#endif

  /* Metal resources. */
  bool initialized_ = false;
  id<MTLDevice> device_ = nil;

  /* The buffer selection aims to pick a buffer which meets the minimum size requirements.
   * To do this, we keep an ordered set of all available buffers. If the buffer is larger than the
   * desired allocation size, we check it against `mtl_buffer_size_threshold_factor_`,
   * which defines what % larger than the original allocation the buffer can be.
   * - A higher value results in greater re-use of previously allocated buffers of similar sizes.
   * - A lower value may result in more dynamic allocations, but minimized memory usage for a given
   *   scenario.
   * The current value of 1.26 is calibrated for optimal performance and memory utilization. */
  static constexpr float mtl_buffer_size_threshold_factor_ = 1.26;

  /* Buffer pools using MTLResourceOptions as key for allocation type.
   * Aliased as 'uint64_t' for map type compatibility.
   * - A size-ordered list (MultiSet) of allocated buffers is kept per MTLResourceOptions
   *   permutation. This allows efficient lookup for buffers of a given requested size.
   * - MTLBufferHandle wraps a gpu::MTLBuffer pointer to achieve easy size-based sorting
   *   via CompareMTLBuffer.
   *
   * NOTE: buffer_pool_lock_ guards against concurrent access to the memory allocator. This
   * can occur during light baking or rendering operations. */
  using MTLBufferPoolOrderedList = std::multiset<MTLBufferHandle, CompareMTLBuffer>;
  using MTLBufferResourceOptions = uint64_t;

  std::mutex buffer_pool_lock_;
  blender::Map<MTLBufferResourceOptions, MTLBufferPoolOrderedList *> buffer_pools_;

  /* Linked list to track all existing allocations. Prioritizing fast insert/deletion. */
  gpu::MTLBuffer *allocations_list_base_;
  uint allocations_list_size_;

  /* Maintain a queue of all MTLSafeFreeList's that have been released
   * by the GPU and are ready to have their buffers re-inserted into the
   * MemoryManager pools.
   * Access to this queue is made thread-safe through safelist_lock_. */
  std::mutex safelist_lock_;
  blender::Vector<MTLSafeFreeList *> completed_safelist_queue_;

  /* Current free list, associated with active MTLCommandBuffer submission. */
  /* MTLBuffer::free() can be called from separate threads, due to usage within animation
   * system/worker threads. */
  std::atomic<MTLSafeFreeList *> current_free_list_;
  std::atomic<int64_t> allocations_in_pool_;

  /* Previous list, to be released after one full frame. */
  MTLSafeFreeList *prev_free_buffer_list_ = nullptr;

 public:
  void init(id<MTLDevice> device);
  ~MTLBufferPool();

  gpu::MTLBuffer *allocate(uint64_t size, bool cpu_visible);
  gpu::MTLBuffer *allocate_aligned(uint64_t size, uint alignment, bool cpu_visible);
  gpu::MTLBuffer *allocate_with_data(uint64_t size, bool cpu_visible, const void *data = nullptr);
  gpu::MTLBuffer *allocate_aligned_with_data(uint64_t size,
                                             uint alignment,
                                             bool cpu_visible,
                                             const void *data = nullptr);
  bool free_buffer(gpu::MTLBuffer *buffer);

  /* Flush MTLSafeFreeList buffers, for completed lists in `completed_safelist_queue_`,
   * back to memory pools. */
  void update_memory_pools();

  /* Access and control over active MTLSafeFreeList. */
  MTLSafeFreeList *get_current_safe_list();
  void begin_new_safe_list();

  /* Add a completed MTLSafeFreeList to completed_safelist_queue_. */
  void push_completed_safe_list(MTLSafeFreeList *list);

 private:
  void ensure_buffer_pool(MTLResourceOptions options);
  void insert_buffer_into_pool(MTLResourceOptions options, gpu::MTLBuffer *buffer);
  void free();

  /* Allocations list. */
  void allocations_list_insert(gpu::MTLBuffer *buffer);
  void allocations_list_delete(gpu::MTLBuffer *buffer);
  void allocations_list_delete_all();
};

/* Scratch buffers are circular-buffers used for temporary data within the current frame.
 * In order to preserve integrity of contents when having multiple-frames-in-flight,
 * we cycle through a collection of scratch buffers which are reset upon next use.
 *
 * Below are a series of properties, declared to manage scratch buffers. If a scratch buffer
 * overflows, then the original buffer will be flushed and submitted, with retained references
 * by usage within the command buffer, and a new buffer will be created.
 * - The new buffer will grow in size to account for increased demand in temporary memory.
 */
class MTLScratchBufferManager {

 private:
  /* Maximum number of scratch buffers to allocate. This should be the maximum number of
   * simultaneous frames in flight. */
  static constexpr uint mtl_max_scratch_buffers_ = MTL_NUM_SAFE_FRAMES;

 public:
  /* Maximum size of single scratch buffer allocation. When re-sizing, this is the maximum size the
   * newly allocated buffers will grow to. Larger allocations are possible if
   * `MTL_SCRATCH_BUFFER_ALLOW_TEMPORARY_EXPANSION` is enabled, but these will instead allocate new
   * buffers from the memory pools on the fly. */
  static constexpr uint mtl_scratch_buffer_max_size_ = 128 * 1024 * 1024;

  /* Initial size of circular scratch buffers prior to growth. */
  static constexpr uint mtl_scratch_buffer_initial_size_ = 16 * 1024 * 1024;

 private:
  /* Parent MTLContext. */
  MTLContext &context_;
  bool initialised_ = false;

  /* Scratch buffer currently in-use. */
  uint current_scratch_buffer_ = 0;

  /* Scratch buffer pool. */
  MTLCircularBuffer *scratch_buffers_[mtl_max_scratch_buffers_];

 public:
  MTLScratchBufferManager(MTLContext &context) : context_(context) {};
  ~MTLScratchBufferManager();

  /* Explicit initialization and freeing of resources.
   * Initialization must occur after device creation. */
  void init();
  void free();

  /* Allocation functions for creating temporary allocations from active circular buffer. */
  MTLTemporaryBuffer scratch_buffer_allocate_range(uint64_t alloc_size);
  MTLTemporaryBuffer scratch_buffer_allocate_range_aligned(uint64_t alloc_size, uint alignment);

  /* Ensure a new scratch buffer is started if we move onto a new frame.
   * Called when a new command buffer begins. */
  void ensure_increment_scratch_buffer();

  /* Flush memory for active scratch buffer to GPU.
   * This call will perform a partial flush of the buffer starting from
   * the last offset the data was flushed from, to the current offset. */
  void flush_active_scratch_buffer();

  /* Bind the whole scratch buffer as a SSBO resource. */
  void bind_as_ssbo(int slot);
  void unbind_as_ssbo();

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLBufferPool");
};

/** \} */

}  // namespace blender::gpu
