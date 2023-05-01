/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

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
  debug::push_marker(this, vk_queue_, name);
}

void VKContext::debug_group_end()
{
  debug::pop_marker(this, vk_queue_);
}

bool VKContext::debug_capture_begin()
{
  return VKBackend::get().debug_capture_begin(vk_instance_);
}

bool VKBackend::debug_capture_begin(VkInstance vk_instance)
{
#ifdef WITH_RENDERDOC
  return renderdoc_api_.start_frame_capture(vk_instance, nullptr);
#else
  UNUSED_VARS(vk_instance);
  return false;
#endif
}

void VKContext::debug_capture_end()
{
  VKBackend::get().debug_capture_end(vk_instance_);
}

void VKBackend::debug_capture_end(VkInstance vk_instance)
{
#ifdef WITH_RENDERDOC
  renderdoc_api_.end_frame_capture(vk_instance, nullptr);
#else
  UNUSED_VARS(vk_instance);
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

static void load_dynamic_functions(VKContext *context,
                                   PFN_vkGetInstanceProcAddr instance_proc_addr)
{
  VKDebuggingTools &debugging_tools = context->debugging_tools_get();
  VkInstance vk_instance = context->instance_get();

  if (instance_proc_addr) {

    debugging_tools.enabled = false;
    debugging_tools.vkCmdBeginDebugUtilsLabelEXT_r = (PFN_vkCmdBeginDebugUtilsLabelEXT)
        instance_proc_addr(vk_instance, "vkCmdBeginDebugUtilsLabelEXT");
    debugging_tools.vkCmdEndDebugUtilsLabelEXT_r = (PFN_vkCmdEndDebugUtilsLabelEXT)
        instance_proc_addr(vk_instance, "vkCmdEndDebugUtilsLabelEXT");
    debugging_tools.vkCmdInsertDebugUtilsLabelEXT_r = (PFN_vkCmdInsertDebugUtilsLabelEXT)
        instance_proc_addr(vk_instance, "vkCmdInsertDebugUtilsLabelEXT");
    debugging_tools.vkCreateDebugUtilsMessengerEXT_r = (PFN_vkCreateDebugUtilsMessengerEXT)
        instance_proc_addr(vk_instance, "vkCreateDebugUtilsMessengerEXT");
    debugging_tools.vkDestroyDebugUtilsMessengerEXT_r = (PFN_vkDestroyDebugUtilsMessengerEXT)
        instance_proc_addr(vk_instance, "vkDestroyDebugUtilsMessengerEXT");
    debugging_tools.vkQueueBeginDebugUtilsLabelEXT_r = (PFN_vkQueueBeginDebugUtilsLabelEXT)
        instance_proc_addr(vk_instance, "vkQueueBeginDebugUtilsLabelEXT");
    debugging_tools.vkQueueEndDebugUtilsLabelEXT_r = (PFN_vkQueueEndDebugUtilsLabelEXT)
        instance_proc_addr(vk_instance, "vkQueueEndDebugUtilsLabelEXT");
    debugging_tools.vkQueueInsertDebugUtilsLabelEXT_r = (PFN_vkQueueInsertDebugUtilsLabelEXT)
        instance_proc_addr(vk_instance, "vkQueueInsertDebugUtilsLabelEXT");
    debugging_tools.vkSetDebugUtilsObjectNameEXT_r = (PFN_vkSetDebugUtilsObjectNameEXT)
        instance_proc_addr(vk_instance, "vkSetDebugUtilsObjectNameEXT");
    debugging_tools.vkSetDebugUtilsObjectTagEXT_r = (PFN_vkSetDebugUtilsObjectTagEXT)
        instance_proc_addr(vk_instance, "vkSetDebugUtilsObjectTagEXT");
    debugging_tools.vkSubmitDebugUtilsMessageEXT_r = (PFN_vkSubmitDebugUtilsMessageEXT)
        instance_proc_addr(vk_instance, "vkSubmitDebugUtilsMessageEXT");
    if (debugging_tools.vkCmdBeginDebugUtilsLabelEXT_r) {
      debugging_tools.enabled = true;
    }
  }
  else {
    debugging_tools.vkCmdBeginDebugUtilsLabelEXT_r = nullptr;
    debugging_tools.vkCmdEndDebugUtilsLabelEXT_r = nullptr;
    debugging_tools.vkCmdInsertDebugUtilsLabelEXT_r = nullptr;
    debugging_tools.vkCreateDebugUtilsMessengerEXT_r = nullptr;
    debugging_tools.vkDestroyDebugUtilsMessengerEXT_r = nullptr;
    debugging_tools.vkQueueBeginDebugUtilsLabelEXT_r = nullptr;
    debugging_tools.vkQueueEndDebugUtilsLabelEXT_r = nullptr;
    debugging_tools.vkQueueInsertDebugUtilsLabelEXT_r = nullptr;
    debugging_tools.vkSetDebugUtilsObjectNameEXT_r = nullptr;
    debugging_tools.vkSetDebugUtilsObjectTagEXT_r = nullptr;
    debugging_tools.vkSubmitDebugUtilsMessageEXT_r = nullptr;
    debugging_tools.enabled = false;
  }
}

bool init_callbacks(VKContext *context, PFN_vkGetInstanceProcAddr instance_proc_addr)
{
  if (instance_proc_addr) {
    load_dynamic_functions(context, instance_proc_addr);
    return true;
  };
  return false;
}

void destroy_callbacks(VKContext *context)
{
  VKDebuggingTools &debugging_tools = context->debugging_tools_get();
  if (debugging_tools.enabled) {
    load_dynamic_functions(context, nullptr);
  }
}

void object_label(VKContext *context,
                  VkObjectType vk_object_type,
                  uint64_t object_handle,
                  const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = context->debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsObjectNameInfoEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      info.objectType = vk_object_type;
      info.objectHandle = object_handle;
      info.pObjectName = name;
      debugging_tools.vkSetDebugUtilsObjectNameEXT_r(context->device_get(), &info);
    }
  }
}

void push_marker(VKContext *context, VkCommandBuffer vk_command_buffer, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = context->debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkCmdBeginDebugUtilsLabelEXT_r(vk_command_buffer, &info);
    }
  }
}

void set_marker(VKContext *context, VkCommandBuffer vk_command_buffer, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = context->debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkCmdInsertDebugUtilsLabelEXT_r(vk_command_buffer, &info);
    }
  }
}

void pop_marker(VKContext *context, VkCommandBuffer vk_command_buffer)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = context->debugging_tools_get();
    if (debugging_tools.enabled) {
      debugging_tools.vkCmdEndDebugUtilsLabelEXT_r(vk_command_buffer);
    }
  }
}

void push_marker(VKContext *context, VkQueue vk_queue, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = context->debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkQueueBeginDebugUtilsLabelEXT_r(vk_queue, &info);
    }
  }
}

void set_marker(VKContext *context, VkQueue vk_queue, const char *name)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = context->debugging_tools_get();
    if (debugging_tools.enabled) {
      VkDebugUtilsLabelEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      info.pLabelName = name;
      debugging_tools.vkQueueInsertDebugUtilsLabelEXT_r(vk_queue, &info);
    }
  }
}

void pop_marker(VKContext *context, VkQueue vk_queue)
{
  if (G.debug & G_DEBUG_GPU) {
    const VKDebuggingTools &debugging_tools = context->debugging_tools_get();
    if (debugging_tools.enabled) {
      debugging_tools.vkQueueEndDebugUtilsLabelEXT_r(vk_queue);
    }
  }
}
}  // namespace blender::gpu::debug
