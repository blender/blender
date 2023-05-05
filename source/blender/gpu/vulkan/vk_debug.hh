/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */
#pragma once

#include "BKE_global.h"
#include "BLI_string.h"

#include "vk_common.hh"

#include <typeindex>

namespace blender::gpu {
class VKContext;
class VKDevice;

namespace debug {
struct VKDebuggingTools {
  bool enabled = false;
  /* Function pointer definitions. */
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_r = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_r = nullptr;
  PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT_r = nullptr;
  PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_r = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_r = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT_r = nullptr;
  PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT_r = nullptr;
  PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT_r = nullptr;
  PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_r = nullptr;
  PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_r = nullptr;
  PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT_r = nullptr;

  void init(VkInstance vk_instance);
  void deinit();
};

void object_label(VkObjectType vk_object_type, uint64_t object_handle, const char *name);
template<typename T> void object_label(T vk_object_type, const char *name)
{
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }
  const size_t label_size = 64;
  char label[label_size];
  memset(label, 0, label_size);
  static int stats = 0;
  SNPRINTF(label, "%s_%d", name, stats++);
  object_label(to_vk_object_type(vk_object_type), (uint64_t)vk_object_type, (const char *)label);
};

void push_marker(VkCommandBuffer vk_command_buffer, const char *name);
void set_marker(VkCommandBuffer vk_command_buffer, const char *name);
void pop_marker(VkCommandBuffer vk_command_buffer);
void push_marker(const VKDevice &device, const char *name);
void set_marker(const VKDevice &device, const char *name);
void pop_marker(const VKDevice &device);
}  // namespace debug
}  // namespace blender::gpu
