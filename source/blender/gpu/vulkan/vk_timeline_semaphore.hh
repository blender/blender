/* SPDX-FileCopyrightText: 2023 Blender Authorss
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

namespace blender::gpu {
class VKDevice;

/**
 * A timeline semaphore is a special semaphore type used to syncronize between commands and
 * resource usage in a time aware fasion.
 *
 * Synchronization is a core part of Vulkan and the Timeline Semaphore is a utility that
 * facilitates its implementation in Blender.
 *
 * There are resources that needs to be tracked in time in order to know when to submit, free or
 * reuse these resource. Some usecases are:
 *
 * - Command buffers can only be reset or freed when they are executed on the device. When the
 *   command buffers are still pending for execution they may not be reused or freed.
 * - Buffers are only allowed to be reuploaded when they are not used at this moment by the device.
 *   This CPU/GPU synchronization can be guarded by a timeline semaphore. In this case barriers
 *   may not be used as they don't cover CPU synchronization for host allocated buffers.
 *
 * Usage:
 *
 * For each device queue a timeline semaphore should be constructed. Every time when a command
 * buffer is submitted the submission will wait for the current timeline value to be completed.
 * Locally the command buffer can keep track of the timeline value when submitting commands so
 * `gpuFinish` could be implemented is a context aware fasion.
 *
 * #VKTimelineSemaphore::Value can be stored locally. By calling the wait function you can ensure
 * that at least the given value has been finished.
 */
class VKTimelineSemaphore {
 public:
  /**
   * VKTimelineSemaphore::Value is used to track the timeline semaphore value.
   */
  class Value {
    uint64_t value_ = 0;

   public:
    operator const uint64_t *() const
    {
      return &value_;
    }

    bool operator<(const Value &other) const
    {
      return this->value_ < other.value_;
    }

    bool operator==(const Value &other) const
    {
      return this->value_ == other.value_;
    }

   private:
    void reset()
    {
      value_ = 0;
    }

    void increase()
    {
      value_++;
    }

    friend class VKTimelineSemaphore;
  };

 private:
  VkSemaphore vk_semaphore_ = VK_NULL_HANDLE;
  Value value_;
  Value last_completed_;

 public:
  ~VKTimelineSemaphore();

  void init(const VKDevice &device);
  void free(const VKDevice &device);

  /**
   * Wait for semaphore completion.
   *
   * Ensuring all commands queues before and including the given value have been finished.
   */
  void wait(const VKDevice &device, const Value &value);

  Value value_increase();
  Value value_get() const;
  Value last_completed_value_get() const;

  VkSemaphore vk_handle() const
  {
    BLI_assert(vk_semaphore_ != VK_NULL_HANDLE);
    return vk_semaphore_;
  }
};

}  // namespace blender::gpu
