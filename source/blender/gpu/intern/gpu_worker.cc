/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_worker.hh"

namespace blender::gpu {

GPUWorker::GPUWorker(uint32_t threads_count, ContextType context_type, WorkCallback callback)
    : callback_(callback)
{
  work_queue_ = BLI_thread_queue_init();

  for (int i : IndexRange(threads_count)) {
    UNUSED_VARS(i);
    std::shared_ptr<GPUSecondaryContext> thread_context =
        context_type == ContextType::PerThread ? std::make_shared<GPUSecondaryContext>() : nullptr;
    threads_.append(std::make_unique<std::thread>([=]() { this->run(thread_context); }));
  }
}

GPUWorker::~GPUWorker()
{
  /* Any work left should have been cancelled at this point. */
  BLI_assert(BLI_thread_queue_is_empty(work_queue_));
  /* Signal background threads to stop waiting for new tasks if none are left. */
  BLI_thread_queue_nowait(work_queue_);
  /* But we still wait, in case the above assert fails. */
  BLI_thread_queue_wait_finish(work_queue_);
  for (std::unique_ptr<std::thread> &thread : threads_) {
    thread->join();
  }
  BLI_thread_queue_free(work_queue_);
}

WorkID GPUWorker::push_work(void *work, ThreadQueueWorkPriority priority)
{
  return BLI_thread_queue_push(work_queue_, work, priority);
}

bool GPUWorker::cancel_work(WorkID id)
{
  return BLI_thread_queue_cancel_work(work_queue_, id);
}

bool GPUWorker::is_empty()
{
  return BLI_thread_queue_is_empty(work_queue_);
}

void GPUWorker::run(std::shared_ptr<GPUSecondaryContext> context)
{
  if (context) {
    context->activate();
  }

  /* Loop until the queue is cancelled. */
  while (void *work = BLI_thread_queue_pop(work_queue_)) {
    callback_(work);
  }
}

}  // namespace blender::gpu
