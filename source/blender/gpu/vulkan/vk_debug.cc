/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <sstream>

#include "BKE_global.hh"
#include "CLG_log.h"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_debug.hh"
#include "vk_to_string.hh"

static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {
void VKContext::debug_group_begin(const char *name, int)
{
  render_graph.debug_group_begin(name);
}

void VKContext::debug_group_end()
{
  render_graph.debug_group_end();
}

bool VKContext::debug_capture_begin(const char *title)
{
  flush_render_graph();
  return VKBackend::get().debug_capture_begin(title);
}

bool VKBackend::debug_capture_begin(const char *title)
{
#ifdef WITH_RENDERDOC
  bool result = renderdoc_api_.start_frame_capture(device.instance_get(), nullptr);
  if (result && title) {
    renderdoc_api_.set_frame_capture_title(title);
  }
  return result;
#else
  UNUSED_VARS(title);
  return false;
#endif
}

void VKContext::debug_capture_end()
{
  flush_render_graph();
  VKBackend::get().debug_capture_end();
}

void VKBackend::debug_capture_end()
{
#ifdef WITH_RENDERDOC
  renderdoc_api_.end_frame_capture(device.instance_get(), nullptr);
#endif
}

void *VKContext::debug_capture_scope_create(const char *name)
{
  return (void *)name;
}

bool VKContext::debug_capture_scope_begin(void *scope)
{
#ifdef WITH_RENDERDOC
  const char *title = (const char *)scope;
  if (StringRefNull(title) != StringRefNull(G.gpu_debug_scope_name)) {
    return false;
  }
  VKBackend::get().debug_capture_begin(title);
#else
  UNUSED_VARS(scope);
#endif
  return false;
}

void VKContext::debug_capture_scope_end(void *scope)
{
#ifdef WITH_RENDERDOC
  const char *title = (const char *)scope;
  if (StringRefNull(title) == StringRefNull(G.gpu_debug_scope_name)) {
    VKBackend::get().debug_capture_end();
  }
#else
  UNUSED_VARS(scope);
#endif
}

}  // namespace blender::gpu

namespace blender::gpu::debug {

void VKDebuggingTools::init(VkInstance vk_instance)
{
  CLG_logref_init(&LOG);
  init_messenger(vk_instance);
}

void VKDebuggingTools::deinit(VkInstance vk_instance)
{
  destroy_messenger(vk_instance);
}

void object_label(VkObjectType vk_object_type, uint64_t object_handle, const char *name)
{
  const VKDevice &device = VKBackend::get().device;
  if (G.debug & G_DEBUG_GPU && device.functions.vkSetDebugUtilsObjectName) {
    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = vk_object_type;
    info.objectHandle = object_handle;
    info.pObjectName = name;
    device.functions.vkSetDebugUtilsObjectName(device.vk_handle(), &info);
  }
}

}  // namespace blender::gpu::debug

namespace blender::gpu::debug {

void VKDebuggingTools::print_labels(const VkDebugUtilsMessengerCallbackDataEXT *callback_data)
{
  std::stringstream ss;
  for (uint32_t object = 0; object < callback_data->objectCount; ++object) {
    ss << " - ObjectType[" << to_string(callback_data->pObjects[object].objectType) << "],";
    ss << "Handle[0x" << std::hex << uintptr_t(callback_data->pObjects[object].objectHandle)
       << "]";
    if (callback_data->pObjects[object].pObjectName) {
      ss << ",Name[" << callback_data->pObjects[object].pObjectName << "]";
    }
    ss << std::endl;
  }
  for (uint32_t label = 0; label < callback_data->cmdBufLabelCount; ++label) {
    if (callback_data->pCmdBufLabels[label].pLabelName) {
      ss << " - CommandBuffer : " << callback_data->pCmdBufLabels[label].pLabelName << std::endl;
    }
  }
  for (uint32_t label = 0; label < callback_data->queueLabelCount; ++label) {
    if (callback_data->pQueueLabels[label].pLabelName) {
      ss << " - Queue : " << callback_data->pQueueLabels[label].pLabelName << std::endl;
    }
  }
  ss << std::endl;
  printf("%s", ss.str().c_str());
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                   VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
                   const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                   void *user_data)
{

  CLG_Severity severity = CLG_SEVERITY_INFO;
  if (message_severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT))
  {
    severity = CLG_SEVERITY_INFO;
  }
  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    severity = CLG_SEVERITY_WARN;
  }
  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    severity = CLG_SEVERITY_ERROR;
  }

  if ((LOG.type->flag & CLG_FLAG_USE) && (LOG.type->level <= severity)) {
    const char *format = "{0x%x}% s\n %s ";
    CLG_logf(LOG.type,
             severity,
             "",
             "",
             format,
             callback_data->messageIdNumber,
             callback_data->pMessageIdName,
             callback_data->pMessage);
  }

  const bool do_labels = (callback_data->objectCount + callback_data->cmdBufLabelCount +
                          callback_data->queueLabelCount) > 0;
  if (do_labels) {
    VKDebuggingTools &debugging_tools = *reinterpret_cast<VKDebuggingTools *>(user_data);
    debugging_tools.print_labels(callback_data);
  }

  return VK_FALSE;
};

void VKDebuggingTools::init_messenger(VkInstance vk_instance)
{
  if (vk_debug_utils_messenger) {
    return;
  }

  VKDevice &device = VKBackend::get().device;
  if (!device.functions.vkCreateDebugUtilsMessenger) {
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT create_info;
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.pNext = nullptr;
  create_info.flags = 0;
  create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = messenger_callback;
  create_info.pUserData = this;
  device.functions.vkCreateDebugUtilsMessenger(
      vk_instance, &create_info, nullptr, &vk_debug_utils_messenger);
  return;
}

void VKDebuggingTools::destroy_messenger(VkInstance vk_instance)
{
  if (vk_debug_utils_messenger == nullptr) {
    return;
  }

  VKDevice &device = VKBackend::get().device;
  device.functions.vkDestroyDebugUtilsMessenger(vk_instance, vk_debug_utils_messenger, nullptr);
  vk_debug_utils_messenger = nullptr;
  return;
}

};  // namespace blender::gpu::debug
