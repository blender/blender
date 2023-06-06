/* SPDX-FileCopyrightText: 2023 Blender Foundation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#pragma once

#include "BKE_global.h"
#include "BLI_set.hh"
#include "BLI_string.h"

#include "vk_common.hh"

#include <mutex>
#include <typeindex>

namespace blender::gpu {
class VKContext;
class VKDevice;

namespace debug {
class VKDebuggingTools {
 public:
  bool enabled = false;
  VkDebugUtilsMessageSeverityFlagsEXT message_severity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
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
  VKDebuggingTools() = default;
  ~VKDebuggingTools();
  void init(VkInstance vk_instance);
  void deinit(VkInstance vk_instance);
  bool is_ignore(int32_t id_number);
  VkResult init_messenger(VkInstance vk_instance);
  void destroy_messenger(VkInstance vk_instance);
  void print_labels(const VkDebugUtilsMessengerCallbackDataEXT *callback_data);

 private:
  VkDebugUtilsMessengerEXT vk_debug_utils_messenger = nullptr;
  Set<int32_t> vk_message_id_number_ignored;
  std::mutex ignore_mutex;
  void add_group(int32_t id_number);
  void remove_group(int32_t id_number);
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
/* how to use : debug::raise_message(0xB41ca2,VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,"This
 * is a raise message. %llx", (uintptr_t)vk_object); */
void raise_message(int32_t id_number,
                   VkDebugUtilsMessageSeverityFlagBitsEXT vk_severity_flag_bits,
                   const char *fmt,
                   ...);
}  // namespace debug
}  // namespace blender::gpu
