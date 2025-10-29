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

/**
 * Abstracts the creation and management of secondary threads with GPU contexts.
 * Must be created from the main thread.
 * Threads and their context remain alive until destruction.
 */
class GPUWorker {
 private:
  Vector<std::unique_ptr<std::thread>> threads_;
  std::condition_variable condition_var_;
  std::mutex &mutex_;
  bool terminate_ = false;

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
   * \param mutex: Mutex used when trying to acquire the next work
   *               (and reused internally for termination).
   * \param pop_work: The callback function that will be called to acquire the next work,
   *                  should return a void pointer.
   *                  NOTE: The mutex is locked when this function is called.
   * \param do_work: The callback function that will be called for each acquired work
   *                 (passed as a void pointer).
   *                 NOTE: The mutex is unlocked when this function is called.
   */
  GPUWorker(uint32_t threads_count,
            ContextType context_type,
            std::mutex &mutex,
            std::function<void *()> pop_work,
            std::function<void(void *)> do_work);
  ~GPUWorker();

  /* Wake up a single thread. */
  void wake_up()
  {
    condition_var_.notify_one();
  }

 private:
  void run(std::shared_ptr<GPUSecondaryContext> context,
           std::function<void *()> pop_work,
           std::function<void(void *)> do_work);
};

}  // namespace blender::gpu
