/* SPDX-FileCopyrightText: 2023 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_debug.hh"

namespace blender::gpu {
void VKContext::debug_group_begin(const char *name, int)
{
  const VKDevice &device = VKBackend::get().device_get();
  debug::push_marker(device, name);
}

void VKContext::debug_group_end()
{
  const VKDevice &device = VKBackend::get().device_get();
  debug::pop_marker(device);
}

bool VKContext::debug_capture_begin()
{
  return VKBackend::get().debug_capture_begin();
}

bool VKBackend::debug_capture_begin()
{
#ifdef WITH_RENDERDOC
  return renderdoc_api_.start_frame_capture(device_get().instance_get(), nullptr);
#else
  return false;
#endif
}

void VKContext::debug_capture_end()
{
  VKBackend::get().debug_capture_end();
}

void VKBackend::debug_capture_end()
{
#ifdef WITH_RENDERDOC
  renderdoc_api_.end_frame_capture(device_get().instance_get(), nullptr);
#endif
}

void *VKContext::debug_capture_scope_create(const char * /*name*/)
{
  return nullptr;
}

bool VKContext::debug_capture_scope_begin(void * /*scope*/)
{
  return false;
}

void VKContext::debug_capture_scope_end(void * /*scope*/) {}
}  // namespace blender::gpu

namespace blender::gpu::debug {

void VKDebuggingTools::init(VkInstance vk_instance)
{
  PFN_vkGetInstanceProcAddr instance_proc_addr = vkGetInstanceProcAddr;
  enabled = false;
  vkCmdBeginDebugUtilsLabelEXT_r = (PFN_vkCmdBeginDebugUtilsLabelEXT)instance_proc_addr(
      vk_instance, "vkCmdBeginDebugUtilsLabelEXT");
  vkCmdEndDebugUtilsLabelEXT_r = (PFN_vkCmdEndDebugUtilsLabelEXT)instance_proc_addr(
      vk_instance, "vkCmdEndDebugUtilsLabelEXT");
  vkCmdInsertDebugUtilsLabelEXT_r = (PFN_vkCmdInsertDebugUtilsLabelEXT)instance_proc_addr(
      vk_instance, "vkCmdInsertDebugUtilsLabelEXT");
  vkCreateDebugUtilsMessengerEXT_r = (PFN_vkCreateDebugUtilsMessengerEXT)instance_proc_addr(
      vk_instance, "vkCreateDebugUtilsMessengerEXT");
  vkDestroyDebugUtilsMessengerEXT_r = (PFN_vkDestroyDebugUtilsMessengerEXT)instance_proc_addr(
      vk_instance, "vkDestroyDebugUtilsMessengerEXT");
  vkQueueBeginDebugUtilsLabelEXT_r = (PFN_vkQueueBeginDebugUtilsLabelEXT)instance_proc_addr(
      vk_instance, "vkQueueBeginDebugUtilsLabelEXT");
  vkQueueEndDebugUtilsLabelEXT_r = (PFN_vkQueueEndDebugUtilsLabelEXT)instance_proc_addr(
      vk_instance, "vkQueueEndDebugUtilsLabelEXT");
  vkQueueInsertDebugUtilsLabelEXT_r = (PFN_vkQueueInsertDebugUtilsLabelEXT)instance_proc_addr(
      vk_instance, "vkQueueInsertDebugUtilsLabelEXT");
  vkSetDebugUtilsObjectNameEXT_r = (PFN_vkSetDebugUtilsObjectNameEXT)instance_proc_addr(
      vk_instance, "vkSetDebugUtilsObjectNameEXT");
  vkSetDebugUtilsObjectTagEXT_r = (PFN_vkSetDebugUtilsObjectTagEXT)instance_proc_addr(
      vk_instance, "vkSetDebugUtilsObjectTagEXT");
  vkSubmitDebugUtilsMessageEXT_r = (PFN_vkSubmitDebugUtilsMessageEXT)instance_proc_addr(
      vk_instance, "vkSubmitDebugUtilsMessageEXT");
  if (vkCmdBeginDebugUtilsLabelEXT_r) {
    enabled = true;
  }
}

void VKDebuggingTools::deinit()
{
  vkCmdBeginDebugUtilsLabelEXT_r = nullptr;
  vkCmdEndDebugUtilsLabelEXT_r = nullptr;
  vkCmdInsertDebugUtilsLabelEXT_r = nullptr;
  vkCreateDebugUtilsMessengerEXT_r = nullptr;
  vkDestroyDebugUtilsMessengerEXT_r = nullptr;
  vkQueueBeginDebugUtilsLabelEXT_r = nullptr;
  vkQueueEndDebugUtilsLabelEXT_r = nullptr;
  vkQueueInsertDebugUtilsLabelEXT_r = nullptr;
  vkSetDebugUtilsObjectNameEXT_r = nullptr;
  vkSetDebugUtilsObjectTagEXT_r = nullptr;
  vkSubmitDebugUtilsMessageEXT_r = nullptr;
  enabled = false;
}

void object_label(VkObjectType vk_object_type, uint64_t object_handle, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDevice &device = VKBackend::get().device_get();
    const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
    if (debugging_tools.enabled) {
      const VKDevice &device = VKBackend::get().device_get();
      VkDebugUtilsObjectNameInfoEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      info.objectType = vk_object_type;
      info.objectHandle = object_handle;
      info.pObjectName = name;
      debugging_tools.vkSetDebugUtilsObjectNameEXT_r(device.device_get(), &info);
    }
  }
}

void push_marker(VkCommandBuffer vk_command_buffer, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDevice &device = VKBackend::get().device_get();
    const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkCmdBeginDebugUtilsLabelEXT_r(vk_command_buffer, &info);
    }
  }
}

void set_marker(VkCommandBuffer vk_command_buffer, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDevice &device = VKBackend::get().device_get();
    const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkCmdInsertDebugUtilsLabelEXT_r(vk_command_buffer, &info);
    }
  }
}

void pop_marker(VkCommandBuffer vk_command_buffer)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDevice &device = VKBackend::get().device_get();
    const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
    if (debugging_tools.enabled) {
      debugging_tools.vkCmdEndDebugUtilsLabelEXT_r(vk_command_buffer);
    }
  }
}

void push_marker(const VKDevice &device, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkQueueBeginDebugUtilsLabelEXT_r(device.queue_get(), &info);
    }
  }
}

void set_marker(const VKDevice &device, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkQueueInsertDebugUtilsLabelEXT_r(device.queue_get(), &info);
    }
  }
}

void pop_marker(const VKDevice &device)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
    if (debugging_tools.enabled) {
      debugging_tools.vkQueueEndDebugUtilsLabelEXT_r(device.queue_get());
    }
  }
}

}  // namespace blender::gpu::debug
