/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_worker.hh"

namespace blender::gpu {

GPUWorker::GPUWorker(uint32_t threads_count, bool share_context, std::function<void()> run_cb)
{
  std::shared_ptr<GPUSecondaryContext> shared_context = nullptr;
  if (share_context) {
    shared_context = std::make_shared<GPUSecondaryContext>();
  }

  for (int i : IndexRange(threads_count)) {
    UNUSED_VARS(i);
    std::shared_ptr<GPUSecondaryContext> thread_context =
        share_context ? shared_context : std::make_shared<GPUSecondaryContext>();
    threads_.append(std::make_unique<std::thread>([=]() { this->run(thread_context, run_cb); }));
  }
}

GPUWorker::~GPUWorker()
{
  terminate_ = true;
  condition_var_.notify_all();
  for (std::unique_ptr<std::thread> &thread : threads_) {
    thread->join();
  }
}

void GPUWorker::run(std::shared_ptr<GPUSecondaryContext> context, std::function<void()> run_cb)
{
  context->activate();

  /* Loop until we get the terminate signal. */
  while (!terminate_) {
    {
      /* Wait until wake_up() */
      std::unique_lock<std::mutex> lock(mutex_);
      condition_var_.wait(lock);
    }
    if (terminate_) {
      continue;
    }

    run_cb();
  }
}

}  // namespace blender::gpu
