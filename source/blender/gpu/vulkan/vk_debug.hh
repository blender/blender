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
namespace debug {
typedef struct VKDebuggingTools {
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

} VKDebuggingTools;

bool init_callbacks(VKContext *context, PFN_vkGetInstanceProcAddr instance_proc_addr);
void destroy_callbacks(VKContext *context);
void object_label(VKContext *context, VkObjectType vk_object_type, uint64_t object_handle, const char *name);

template<typename T> void object_label(VKContext *context, T vk_object_type, const char *name)
{
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }
  const size_t label_size = 64;
  char label[label_size];
  memset(label, 0, label_size);
  static int stats = 0;
  SNPRINTF(label, "%s_%d", name, stats++);
  object_label(context, to_vk_object_type(vk_object_type), (uint64_t)vk_object_type, (const char *)label);
};

void push_marker(VKContext *context, VkCommandBuffer vk_command_buffer, const char *name);
void set_marker(VKContext *context, VkCommandBuffer vk_command_buffer, const char *name);
void pop_marker(VKContext *context, VkCommandBuffer vk_command_buffer);
void push_marker(VKContext *context, VkQueue vk_queue, const char *name);
void set_marker(VKContext *context, VkQueue vk_queue, const char *name);
void pop_marker(VKContext *context, VkQueue vk_queue);
}  // namespace debug
}  // namespace blender::gpu
