/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "MEM_guardedalloc.h"
#include "mallocn_intern.h"

#include "../../source/blender/blenlib/BLI_strict_flags.h"

namespace {

struct Local;
struct Global;

/**
 * This is stored per thread. Align to cache line size to avoid false sharing.
 */
struct alignas(128) Local {
  /**
   * Retain shared ownership of #Global to make sure that it is not destructed.
   */
  std::shared_ptr<Global> global;

  /** Helps to find bugs during program shutdown. */
  bool destructed = false;
  /**
   * This is the first created #Local and on the main thread. When the main local data is
   * destructed, we know that Blender is quitting and that we can't rely on thread locals being
   * available still.
   */
  bool is_main = false;
  /**
   * Number of bytes. This can be negative when e.g. one thread allocates a lot of memory, and
   * another frees it. It has to be an atomic, because it may be accessed by other threads when the
   * total memory usage is counted.
   */
  std::atomic<int64_t> mem_in_use = 0;
  /**
   * Number of allocated blocks. Can be negative and is atomic for the same reason as above.
   */
  std::atomic<int64_t> blocks_num = 0;
  /**
   * Amount of memory used when the peak was last updated. This is used so that we don't have to
   * update the peak memory usage after every memory allocation. Instead it's only updated when "a
   * lot" of new memory has been allocated. This makes the peak memory usage a little bit less
   * accurate, but it's still good enough for practical purposes.
   */
  std::atomic<int64_t> mem_in_use_during_peak_update = 0;

  Local();
  ~Local();
};

/**
 * This is a singleton that stores global data. It's owned by a `std::shared_ptr` which is owned by
 * the static variable in #get_global_ptr and all #Local objects.
 */
struct Global {
  /**
   * Mutex that protects the vector below.
   */
  std::mutex locals_mutex;
  /**
   * All currently constructed #Local. This must only be accessed when the mutex above is
   * locked. Individual threads insert and remove themselves here.
   */
  std::vector<Local *> locals;
  /**
   * Number of bytes that are not tracked by #Local. This is necessary because when a thread exits,
   * its #Local data is freed. The memory counts stored there would be lost. The memory counts may
   * be non-zero during thread destruction, if the thread did an unequal amount of allocations and
   * frees (which is perfectly valid behavior as long as other threads have the responsibility to
   * free any memory that the thread allocated).
   *
   * To solve this, the memory counts are added to these global counters when the thread
   * exists. The global counters are also used when the entire process starts to exit, because the
   * #Local data of the main thread is already destructed when the leak detection happens (during
   * destruction of static variables which happens after destruction of thread-locals).
   */
  std::atomic<int64_t> mem_in_use_outside_locals = 0;
  /**
   * Number of blocks that are not tracked by #Local, for the same reason as above.
   */
  std::atomic<int64_t> blocks_num_outside_locals = 0;
  /**
   * Peak memory usage since the last reset.
   */
  std::atomic<size_t> peak = 0;
};

}  // namespace

/**
 * This is true for most of the lifetime of the program. Only when it starts exiting this becomes
 * false indicating that global counters should be used for correctness.
 */
static std::atomic<bool> use_local_counters = true;
/**
 * When a thread allocated this amount of memory, the peak memory usage is updated. An alternative
 * would be to update the global peak memory after every allocation, but that would cause much more
 * overhead with little benefit.
 */
static constexpr int64_t peak_update_threshold = 1024 * 1024;

static std::shared_ptr<Global> &get_global_ptr()
{
  static std::shared_ptr<Global> global = std::make_shared<Global>();
  return global;
}

static Global &get_global()
{
  return *get_global_ptr();
}

static Local &get_local_data()
{
  static thread_local Local local;
  assert(!local.destructed);
  return local;
}

Local::Local()
{
  this->global = get_global_ptr();

  std::lock_guard lock{this->global->locals_mutex};
  if (this->global->locals.empty()) {
    /* This is the first thread creating #Local, it is therefore the main thread because it's
     * created through #memory_usage_init. */
    this->is_main = true;
  }
  /* Register self in the global list. */
  this->global->locals.push_back(this);
}

Local::~Local()
{
  std::lock_guard lock{this->global->locals_mutex};

  /* Unregister self from the global list. */
  this->global->locals.erase(
      std::find(this->global->locals.begin(), this->global->locals.end(), this));
  /* Don't forget the memory counts stored locally. */
  this->global->blocks_num_outside_locals.fetch_add(this->blocks_num, std::memory_order_relaxed);
  this->global->mem_in_use_outside_locals.fetch_add(this->mem_in_use, std::memory_order_relaxed);

  if (this->is_main) {
    /* The main thread started shutting down. Use global counters from now on to avoid accessing
     * thread-locals after they have been destructed. */
    use_local_counters.store(false, std::memory_order_relaxed);
  }
  /* Helps to detect when thread locals are accidentally accessed after destruction. */
  this->destructed = true;
}

/** Check if the current memory usage is higher than the peak and update it if yes. */
static void update_global_peak()
{
  Global &global = get_global();
  /* Update peak. */
  global.peak = std::max<size_t>(global.peak, memory_usage_current());

  std::lock_guard lock{global.locals_mutex};

  for (Local *local : global.locals) {
    assert(!local->destructed);
    /* Updating this makes sure that the peak is not updated too often, which would degrade
     * performance. */
    local->mem_in_use_during_peak_update = local->mem_in_use.load(std::memory_order_relaxed);
  }
}

void memory_usage_init()
{
  /* Makes sure that the static and thread-local variables on the main thread are initialized. */
  get_local_data();
}

void memory_usage_block_alloc(const size_t size)
{
  if (LIKELY(use_local_counters.load(std::memory_order_relaxed))) {
    Local &local = get_local_data();
    /* Increase local memory counts. This does not cause thread synchronization in the majority of
     * cases, because each thread has these counters on a separate cache line. It may only cause
     * synchronization if another thread is computing the total current memory usage at the same
     * time, which is very rare compared to doing allocations. */
    local.blocks_num.fetch_add(1, std::memory_order_relaxed);
    local.mem_in_use.fetch_add(int64_t(size), std::memory_order_relaxed);

    /* If a certain amount of new memory has been allocated, update the peak. */
    if (local.mem_in_use - local.mem_in_use_during_peak_update > peak_update_threshold) {
      update_global_peak();
    }
  }
  else {
    Global &global = get_global();
    /* Increase global memory counts. */
    global.blocks_num_outside_locals.fetch_add(1, std::memory_order_relaxed);
    global.mem_in_use_outside_locals.fetch_add(int64_t(size), std::memory_order_relaxed);
  }
}

void memory_usage_block_free(const size_t size)
{
  if (LIKELY(use_local_counters)) {
    /* Decrease local memory counts. See comment in #memory_usage_block_alloc for details regarding
     * thread synchronization. */
    Local &local = get_local_data();
    local.mem_in_use.fetch_sub(int64_t(size), std::memory_order_relaxed);
    local.blocks_num.fetch_sub(1, std::memory_order_relaxed);
  }
  else {
    Global &global = get_global();
    /* Decrease global memory counts. */
    global.blocks_num_outside_locals.fetch_sub(1, std::memory_order_relaxed);
    global.mem_in_use_outside_locals.fetch_sub(int64_t(size), std::memory_order_relaxed);
  }
}

size_t memory_usage_block_num()
{
  Global &global = get_global();
  std::lock_guard lock{global.locals_mutex};

  /* Count the number of active blocks. */
  int64_t blocks_num = global.blocks_num_outside_locals;
  for (Local *local : global.locals) {
    blocks_num += local->blocks_num;
  }
  return size_t(blocks_num);
}

size_t memory_usage_current()
{
  Global &global = get_global();
  std::lock_guard lock{global.locals_mutex};

  /* Count the memory that's currently in use. */
  int64_t mem_in_use = global.mem_in_use_outside_locals;
  for (Local *local : global.locals) {
    mem_in_use += local->mem_in_use;
  }
  return size_t(mem_in_use);
}

/**
 * Get the approximate peak memory usage since the last call to #memory_usage_peak_reset.
 * This is approximate, because the peak usage is not updated after every allocation (see
 * #peak_update_threshold).
 *
 * In the worst case, the peak memory usage is underestimated by
 * `peak_update_threshold * #threads`. After large allocations (larger than the threshold), the
 * peak usage is always updated so those allocations will always be taken into account.
 */
size_t memory_usage_peak()
{
  update_global_peak();
  Global &global = get_global();
  return global.peak;
}

void memory_usage_peak_reset()
{
  Global &global = get_global();
  global.peak = memory_usage_current();
}
