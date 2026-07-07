/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_threads.h"
#include "BLI_vector.hh"
#include "GPU_context.hh"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace blender::gpu {

using WorkCallback = void (*)(void *);
using WorkID = uint64_t;

/**
 * Abstracts the creation and management of secondary threads with GPU contexts.
 * Must be created from the main thread.
 * Threads and their context remain alive until destruction.
 */
class GPUWorker {
 private:
  Vector<std::unique_ptr<std::thread>> threads_;
  ThreadQueue *work_queue_;
  WorkCallback callback_;

 public:
  enum class ContextType {
    /** Use the main GPU context on the worker threads. */
    Main,
    /** Use a different secondary GPU context for each worker thread. */
    PerThread,
  };

  /**
   * \param threads_count: Number of threads to span.
   * \param context_type: The type of context each thread uses.
   * \param callback: The callback function that will be called for each acquired work
   *                 (passed as a void pointer).
   */
  GPUWorker(uint32_t threads_count, ContextType context_type, WorkCallback callback);
  ~GPUWorker();

  WorkID push_work(void *work, ThreadQueueWorkPriority priority);
  bool cancel_work(WorkID id);
  bool is_empty();

 private:
  void run(std::shared_ptr<GPUSecondaryContext> context);
};

}  // namespace blender::gpu
