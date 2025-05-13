/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"
#include "GPU_context.hh"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace blender::gpu {

/* Abstracts the creation and management of secondary threads with GPU contexts.
 * Must be created from the main thread.
 * Threads and their context remain alive until destruction. */
class GPUWorker {
 private:
  Vector<std::unique_ptr<std::thread>> threads_;
  std::condition_variable condition_var_;
  std::mutex mutex_;
  std::atomic<bool> terminate_ = false;

 public:
  enum class ContextType {
    /* Use the main GPU context on the worker threads. */
    Main,
    /* Use a different secondary GPU context for each worker thread. */
    PerThread,
  };

  /**
   * \param threads_count: Number of threads to span.
   * \param context_type: The type of context each thread uses.
   * \param run_cb: The callback function that will be called by a thread on `wake_up()`.
   */
  GPUWorker(uint32_t threads_count, ContextType context_type, std::function<void()> run_cb);
  ~GPUWorker();

  /* Wake up a single thread. */
  void wake_up()
  {
    condition_var_.notify_one();
  }

 private:
  void run(std::shared_ptr<GPUSecondaryContext> context, std::function<void()> run_cb);
};

}  // namespace blender::gpu
