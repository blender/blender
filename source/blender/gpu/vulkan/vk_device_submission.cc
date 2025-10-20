/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <chrono>
#include <condition_variable>
#include <thread>

#include "BLI_mutex.hh"
#include "BLI_task.h"

#include "vk_device.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Render graph
 * \{ */

struct VKRenderGraphWait {
  blender::Mutex is_submitted_mutex;
  std::condition_variable_any is_submitted_condition;
  bool is_submitted;
};

struct VKRenderGraphSubmitTask {
  render_graph::VKRenderGraph *render_graph;
  uint64_t timeline;
  bool submit_to_device;
  VkPipelineStageFlags wait_dst_stage_mask;
  VkSemaphore wait_semaphore;
  VkSemaphore signal_semaphore;
  VkFence signal_fence;
  VKRenderGraphWait *wait_for_submission;
};

TimelineValue VKDevice::render_graph_submit(render_graph::VKRenderGraph *render_graph,
                                            VKDiscardPool &context_discard_pool,
                                            bool submit_to_device,
                                            bool wait_for_completion,
                                            VkPipelineStageFlags wait_dst_stage_mask,
                                            VkSemaphore wait_semaphore,
                                            VkSemaphore signal_semaphore,
                                            VkFence signal_fence)
{
  if (render_graph->is_empty()) {
    render_graph->reset();
    BLI_thread_queue_push(
        unused_render_graphs_, render_graph, BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL);
    return timeline_value_;
  }

  VKRenderGraphSubmitTask *submit_task = MEM_new<VKRenderGraphSubmitTask>(__func__);
  submit_task->render_graph = render_graph;
  submit_task->submit_to_device = submit_to_device;
  submit_task->wait_dst_stage_mask = wait_dst_stage_mask;
  submit_task->wait_semaphore = wait_semaphore;
  submit_task->signal_semaphore = signal_semaphore;
  submit_task->signal_fence = signal_fence;
  submit_task->wait_for_submission = nullptr;

  /* We need to wait for submission as otherwise the signal semaphore can still not be in an
   * initial state. */
  const bool wait_for_submission = signal_semaphore != VK_NULL_HANDLE && !wait_for_completion;
  VKRenderGraphWait wait_condition{};
  if (wait_for_submission) {
    submit_task->wait_for_submission = &wait_condition;
  }
  TimelineValue timeline = 0;
  {
    std::scoped_lock lock(orphaned_data.mutex_get());
    timeline = submit_task->timeline = submit_to_device ? ++timeline_value_ : timeline_value_ + 1;
    orphaned_data.timeline_ = timeline;
    orphaned_data.move_data(context_discard_pool, timeline);
    BLI_thread_queue_push(
        submitted_render_graphs_, submit_task, BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL);
  }
  submit_task = nullptr;

  if (wait_for_submission) {
    std::unique_lock<blender::Mutex> lock(wait_condition.is_submitted_mutex);
    wait_condition.is_submitted_condition.wait(lock, [&] { return wait_condition.is_submitted; });
  }

  if (wait_for_completion) {
    wait_for_timeline(timeline);
  }
  return timeline;
}

void VKDevice::wait_for_timeline(TimelineValue timeline)
{
  if (timeline == 0) {
    return;
  }
  VkSemaphoreWaitInfo vk_semaphore_wait_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, nullptr, 0, 1, &vk_timeline_semaphore_, &timeline};
  vkWaitSemaphores(vk_device_, &vk_semaphore_wait_info, UINT64_MAX);
}

void VKDevice::wait_queue_idle()
{
  std::scoped_lock lock(*queue_mutex_);
  vkQueueWaitIdle(vk_queue_);
}

render_graph::VKRenderGraph *VKDevice::render_graph_new()
{
  render_graph::VKRenderGraph *render_graph = static_cast<render_graph::VKRenderGraph *>(
      BLI_thread_queue_pop_timeout(unused_render_graphs_, 0));
  if (render_graph) {
    return render_graph;
  }

  std::scoped_lock lock(resources.mutex);
  render_graph = MEM_new<render_graph::VKRenderGraph>(__func__, resources);
  render_graphs_.append(render_graph);
  return render_graph;
}

void VKDevice::submission_runner(TaskPool *__restrict pool, void *task_data)
{
  CLOG_TRACE(&LOG, "Submission runner has started");
  UNUSED_VARS(task_data);

  VKDevice *device = static_cast<VKDevice *>(BLI_task_pool_user_data(pool));
  VkCommandPool vk_command_pool = VK_NULL_HANDLE;
  VkCommandPoolCreateInfo vk_command_pool_create_info = {
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      nullptr,
      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      device->vk_queue_family_};
  vkCreateCommandPool(device->vk_device_, &vk_command_pool_create_info, nullptr, &vk_command_pool);

  render_graph::VKScheduler scheduler;
  render_graph::VKCommandBuilder command_builder;
  Vector<VkCommandBuffer> command_buffers_unused;
  TimelineResources<VkCommandBuffer> command_buffers_in_use;
  VkCommandBuffer vk_command_buffer = VK_NULL_HANDLE;
  Vector<VkCommandBuffer> unsubmitted_command_buffers;
  Vector<VkSubmitInfo> submit_infos;
  submit_infos.reserve(2);
  std::optional<render_graph::VKCommandBufferWrapper> command_buffer;
  uint64_t previous_gc_timeline = 0;

  CLOG_TRACE(&LOG, "Submission runner initialized");
  while (!BLI_task_pool_current_canceled(pool)) {
    VKRenderGraphSubmitTask *submit_task = static_cast<VKRenderGraphSubmitTask *>(
        BLI_thread_queue_pop_timeout(device->submitted_render_graphs_, 1));
    if (submit_task == nullptr) {
      continue;
    }
    uint64_t current_timeline = device->submission_finished_timeline_get();
    if (assign_if_different(previous_gc_timeline, current_timeline)) {
      device->orphaned_data.destroy_discarded_resources(*device, current_timeline);
    }

    /* End current command buffer when we need to wait for a semaphore. In this case all previous
     * recorded commands can run before the wait semaphores. The commands that must be guarded by
     * the semaphores are part of the new submitted render graph. */
    if (submit_task->wait_semaphore != VK_NULL_HANDLE && command_buffer.has_value()) {
      command_buffer->end_recording();
      unsubmitted_command_buffers.append(vk_command_buffer);
      command_buffer.reset();
    }

    if (!command_buffer.has_value()) {
      /* Check for completed command buffers that can be reused. */
      if (command_buffers_unused.is_empty()) {
        command_buffers_in_use.remove_old(current_timeline,
                                          [&](VkCommandBuffer vk_command_buffer) {
                                            command_buffers_unused.append(vk_command_buffer);
                                          });
      }

      /* Create new command buffers when there are no left to be reused. */
      if (command_buffers_unused.is_empty()) {
        command_buffers_unused.resize(10, VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo vk_command_buffer_allocate_info = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            nullptr,
            vk_command_pool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            10};
        vkAllocateCommandBuffers(
            device->vk_device_, &vk_command_buffer_allocate_info, command_buffers_unused.data());
      };

      vk_command_buffer = command_buffers_unused.pop_last();
      command_buffer = std::make_optional<render_graph::VKCommandBufferWrapper>(
          vk_command_buffer, device->extensions_);
      command_buffer->begin_recording();
    }

    BLI_assert(vk_command_buffer != VK_NULL_HANDLE);

    render_graph::VKRenderGraph &render_graph = *submit_task->render_graph;
    Span<render_graph::NodeHandle> node_handles = scheduler.select_nodes(render_graph);
    {
      std::scoped_lock lock_resources(device->resources.mutex);
      command_builder.build_nodes(render_graph, *command_buffer, node_handles);
    }
    command_builder.record_commands(render_graph, *command_buffer, node_handles);

    if (submit_task->submit_to_device) {
      /* Create submit infos for previous command buffers. */
      submit_infos.clear();
      if (!unsubmitted_command_buffers.is_empty()) {
        VkSubmitInfo vk_submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                       nullptr,
                                       0,
                                       nullptr,
                                       nullptr,
                                       uint32_t(unsubmitted_command_buffers.size()),
                                       unsubmitted_command_buffers.data(),
                                       0,
                                       nullptr};
        submit_infos.append(vk_submit_info);
      }

      /* Finalize current command buffer. */
      command_buffer->end_recording();
      unsubmitted_command_buffers.append(vk_command_buffer);

      uint32_t wait_semaphore_len = submit_task->wait_semaphore == VK_NULL_HANDLE ? 0 : 1;
      uint32_t signal_semaphore_len = submit_task->signal_semaphore == VK_NULL_HANDLE ? 1 : 2;
      VkSemaphore signal_semaphores[2] = {device->vk_timeline_semaphore_,
                                          submit_task->signal_semaphore};
      uint64_t signal_semaphore_values[2] = {submit_task->timeline, 0};

      VkTimelineSemaphoreSubmitInfo vk_timeline_semaphore_submit_info = {
          VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
          nullptr,
          0,
          nullptr,
          signal_semaphore_len,
          signal_semaphore_values};
      VkSubmitInfo vk_submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                     &vk_timeline_semaphore_submit_info,
                                     wait_semaphore_len,
                                     &submit_task->wait_semaphore,
                                     &submit_task->wait_dst_stage_mask,
                                     1,
                                     &unsubmitted_command_buffers.last(),
                                     signal_semaphore_len,
                                     signal_semaphores};
      submit_infos.append(vk_submit_info);

      {
        std::scoped_lock lock_queue(*device->queue_mutex_);
        vkQueueSubmit(device->vk_queue_,
                      submit_infos.size(),
                      submit_infos.data(),
                      submit_task->signal_fence);
      }
      if (submit_task->wait_for_submission != nullptr) {
        std::unique_lock<blender::Mutex> lock(
            submit_task->wait_for_submission->is_submitted_mutex);
        submit_task->wait_for_submission->is_submitted = true;
        submit_task->wait_for_submission->is_submitted_condition.notify_one();
      }
      vk_command_buffer = VK_NULL_HANDLE;
      for (VkCommandBuffer vk_command_buffer : unsubmitted_command_buffers) {
        command_buffers_in_use.append_timeline(submit_task->timeline, vk_command_buffer);
      }
      unsubmitted_command_buffers.clear();
      command_buffer.reset();
    }

    render_graph.reset();
    BLI_thread_queue_push(device->unused_render_graphs_,
                          std::move(submit_task->render_graph),
                          BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL);
    MEM_delete<VKRenderGraphSubmitTask>(submit_task);
  }
  CLOG_TRACE(&LOG, "Submission runner is being canceled");

  /* Clear command buffers and pool */
  {
    std::scoped_lock lock(*device->queue_mutex_);
    vkDeviceWaitIdle(device->vk_device_);
  }
  command_buffers_in_use.remove_old(UINT64_MAX, [&](VkCommandBuffer vk_command_buffer) {
    command_buffers_unused.append(vk_command_buffer);
  });
  vkFreeCommandBuffers(device->vk_device_,
                       vk_command_pool,
                       command_buffers_unused.size(),
                       command_buffers_unused.data());
  vkDestroyCommandPool(device->vk_device_, vk_command_pool, nullptr);
  CLOG_TRACE(&LOG, "Submission runner finished");
}

void VKDevice::init_submission_pool()
{
  CLOG_TRACE(&LOG, "Create submission pool");
  submission_pool_ = BLI_task_pool_create_background_serial(this, TASK_PRIORITY_HIGH);
  submitted_render_graphs_ = BLI_thread_queue_init();
  unused_render_graphs_ = BLI_thread_queue_init();

  VkSemaphoreTypeCreateInfo vk_semaphore_type_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr, VK_SEMAPHORE_TYPE_TIMELINE, 0};
  VkSemaphoreCreateInfo vk_semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &vk_semaphore_type_create_info, 0};
  vkCreateSemaphore(vk_device_, &vk_semaphore_create_info, nullptr, &vk_timeline_semaphore_);

  BLI_task_pool_push(submission_pool_, VKDevice::submission_runner, nullptr, false, nullptr);
}

void VKDevice::deinit_submission_pool()
{
  CLOG_TRACE(&LOG, "Cancelling submission pool");
  BLI_task_pool_cancel(submission_pool_);
  CLOG_TRACE(&LOG, "Waiting for completion");
  BLI_task_pool_work_and_wait(submission_pool_);
  CLOG_TRACE(&LOG, "Freeing submission pool");
  BLI_task_pool_free(submission_pool_);
  submission_pool_ = nullptr;

  while (!BLI_thread_queue_is_empty(submitted_render_graphs_)) {
    VKRenderGraphSubmitTask *submit_task = static_cast<VKRenderGraphSubmitTask *>(
        BLI_thread_queue_pop(submitted_render_graphs_));
    MEM_delete<VKRenderGraphSubmitTask>(submit_task);
  }
  BLI_thread_queue_free(submitted_render_graphs_);
  submitted_render_graphs_ = nullptr;
  BLI_thread_queue_free(unused_render_graphs_);
  unused_render_graphs_ = nullptr;

  vkDestroySemaphore(vk_device_, vk_timeline_semaphore_, nullptr);
  vk_timeline_semaphore_ = VK_NULL_HANDLE;
}

/** \} */

}  // namespace blender::gpu
