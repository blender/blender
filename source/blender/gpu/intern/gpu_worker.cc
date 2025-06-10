/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_worker.hh"

namespace blender::gpu {

GPUWorker::GPUWorker(uint32_t threads_count,
                     ContextType context_type,
                     std::mutex &mutex,
                     std::function<void *()> pop_work,
                     std::function<void(void *)> do_work)
    : mutex_(mutex)
{
  for (int i : IndexRange(threads_count)) {
    UNUSED_VARS(i);
    std::shared_ptr<GPUSecondaryContext> thread_context =
        context_type == ContextType::PerThread ? std::make_shared<GPUSecondaryContext>() : nullptr;
    threads_.append(
        std::make_unique<std::thread>([=]() { this->run(thread_context, pop_work, do_work); }));
  }
}

GPUWorker::~GPUWorker()
{
  {
    std::unique_lock<std::mutex> lock(mutex_);
    terminate_ = true;
  }
  condition_var_.notify_all();
  for (std::unique_ptr<std::thread> &thread : threads_) {
    thread->join();
  }
}

void GPUWorker::run(std::shared_ptr<GPUSecondaryContext> context,
                    std::function<void *()> pop_work,
                    std::function<void(void *)> do_work)
{
  if (context) {
    context->activate();
  }

  std::unique_lock<std::mutex> lock(mutex_);

  /* Loop until we get the terminate signal. */
  while (!terminate_) {
    void *work = pop_work();
    if (!work) {
      condition_var_.wait(lock);
      if (terminate_) {
        break;
      }
      continue;
    }

    lock.unlock();
    do_work(work);
    lock.lock();
  }
}

}  // namespace blender::gpu
