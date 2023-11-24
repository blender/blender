/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_timeline_semaphore.hh"
#include "vk_backend.hh"
#include "vk_device.hh"
#include "vk_memory.hh"

namespace blender::gpu {

VKTimelineSemaphore::~VKTimelineSemaphore()
{
  const VKDevice &device = VKBackend::get().device_get();
  free(device);
}

void VKTimelineSemaphore::init(const VKDevice &device)
{
  if (vk_semaphore_ != VK_NULL_HANDLE) {
    return;
  }

  VK_ALLOCATION_CALLBACKS;
  VkSemaphoreTypeCreateInfo semaphore_type_create_info = {};
  semaphore_type_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  semaphore_type_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  semaphore_type_create_info.initialValue = 0;

  VkSemaphoreCreateInfo semaphore_create_info{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = &semaphore_type_create_info;
  vkCreateSemaphore(
      device.device_get(), &semaphore_create_info, vk_allocation_callbacks, &vk_semaphore_);
  debug::object_label(vk_semaphore_, "TimelineSemaphore");

  value_.reset();
}

void VKTimelineSemaphore::free(const VKDevice &device)
{
  if (vk_semaphore_ == VK_NULL_HANDLE) {
    return;
  }

  VK_ALLOCATION_CALLBACKS;
  vkDestroySemaphore(device.device_get(), vk_semaphore_, vk_allocation_callbacks);
  vk_semaphore_ = VK_NULL_HANDLE;

  value_.reset();
}

void VKTimelineSemaphore::wait(const VKDevice &device, const Value &wait_value)
{
  BLI_assert(vk_semaphore_ != VK_NULL_HANDLE);

  VkSemaphoreWaitInfo wait_info = {};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1;
  wait_info.pSemaphores = &vk_semaphore_;
  wait_info.pValues = wait_value;
  vkWaitSemaphores(device.device_get(), &wait_info, UINT64_MAX);
  last_completed_ = wait_value;
}

VKTimelineSemaphore::Value VKTimelineSemaphore::value_increase()
{
  value_.increase();
  return value_;
}

VKTimelineSemaphore::Value VKTimelineSemaphore::value_get() const
{
  return value_;
}

VKTimelineSemaphore::Value VKTimelineSemaphore::last_completed_value_get() const
{
  return last_completed_;
}

}  // namespace blender::gpu