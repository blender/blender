/* SPDX-FileCopyrightText: 2023 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"
#include "BLI_dynstr.h"
#include "CLG_log.h"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_debug.hh"

static CLG_LogRef LOG = {"gpu.debug.vulkan"};

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
  vk_debug_utils_messenger = nullptr;
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
    init_messenger(vk_instance);
  }
}

void VKDebuggingTools::deinit(VkInstance vk_instance)
{
  if (enabled) {
    destroy_messenger(vk_instance);
  }
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

namespace blender::gpu::debug {
VKDebuggingTools::~VKDebuggingTools()
{
  BLI_assert(vk_debug_utils_messenger == nullptr);
};

void VKDebuggingTools::print_labels(const VkDebugUtilsMessengerCallbackDataEXT *callback_data)
{
  std::stringstream ss;
  for (uint32_t object = 0; object < callback_data->objectCount; ++object) {
    ss << " - ObjectType[" << to_string(callback_data->pObjects[object].objectType) << "],";
    ss << "Handle[0x" << std::hex
       << static_cast<uintptr_t>(callback_data->pObjects[object].objectHandle) << "]";
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
VKAPI_ATTR VkBool32 VKAPI_CALL
messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                   VkDebugUtilsMessageTypeFlagsEXT /* message_type*/,
                   const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                   void *user_data);
VKAPI_ATTR VkBool32 VKAPI_CALL
messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                   VkDebugUtilsMessageTypeFlagsEXT /* message_type*/,
                   const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                   void *user_data)
{
  VKDebuggingTools &debugging_tools = *reinterpret_cast<VKDebuggingTools *>(user_data);
  if (debugging_tools.is_ignore(callback_data->messageIdNumber)) {
    return VK_FALSE;
  }
  bool use_color = CLG_color_support_get(&LOG);
  UNUSED_VARS(use_color);
  bool enabled = false;
  if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ||
      (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT))
  {
    if ((LOG.type->flag & CLG_FLAG_USE) && (LOG.type->level >= CLG_SEVERITY_INFO)) {
      const char *format = "{0x%x}% s\n %s ";
      CLG_logf(LOG.type,
               CLG_SEVERITY_INFO,
               "",
               "",
               format,
               callback_data->messageIdNumber,
               callback_data->pMessageIdName,
               callback_data->pMessage);
      enabled = true;
    }
  }
  else {
    CLG_Severity clog_severity;
    switch (message_severity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        clog_severity = CLG_SEVERITY_WARN;
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        clog_severity = CLG_SEVERITY_ERROR;
        break;
      default:
        BLI_assert_unreachable();
    }
    enabled = true;
    if (clog_severity == CLG_SEVERITY_ERROR) {
      const char *format = " %s {0x%x}\n %s ";
      CLG_logf(LOG.type,
               clog_severity,
               "",
               "",
               format,
               callback_data->pMessageIdName,
               callback_data->messageIdNumber,
               callback_data->pMessage);
    }
    else if (LOG.type->level >= CLG_SEVERITY_WARN) {
      const char *format = " %s {0x%x}\n %s ";
      CLG_logf(LOG.type,
               clog_severity,
               "",
               "",
               format,
               callback_data->pMessageIdName,
               callback_data->messageIdNumber,
               callback_data->pMessage);
    }
  }
  if ((enabled) && ((callback_data->objectCount > 0) || (callback_data->cmdBufLabelCount > 0) ||
                    (callback_data->queueLabelCount > 0)))
  {
    debugging_tools.print_labels(callback_data);
  }
  return VK_TRUE;
};

VkResult VKDebuggingTools::init_messenger(VkInstance vk_instance)
{
  vk_message_id_number_ignored.clear();
  BLI_assert(enabled);

  VkDebugUtilsMessengerCreateInfoEXT create_info;
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.pNext = nullptr;
  create_info.flags = 0;
  create_info.messageSeverity = message_severity;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = messenger_callback;
  create_info.pUserData = this;
  VkResult res = vkCreateDebugUtilsMessengerEXT_r(
      vk_instance, &create_info, nullptr, &vk_debug_utils_messenger);
  BLI_assert(res == VK_SUCCESS);
  return res;
}

void VKDebuggingTools::destroy_messenger(VkInstance vk_instance)
{
  if (vk_debug_utils_messenger == nullptr) {
    return;
  }
  BLI_assert(enabled);
  vkDestroyDebugUtilsMessengerEXT_r(vk_instance, vk_debug_utils_messenger, nullptr);

  vk_message_id_number_ignored.clear();
  vk_debug_utils_messenger = nullptr;
  return;
}

bool VKDebuggingTools::is_ignore(int32_t id_number)
{
  bool found = false;
  {
    std::scoped_lock lock(ignore_mutex);
    found = vk_message_id_number_ignored.contains(id_number);
  }
  return found;
}

void VKDebuggingTools::add_group(int32_t id_number)
{
  std::scoped_lock lock(ignore_mutex);
  vk_message_id_number_ignored.add(id_number);
};

void VKDebuggingTools::remove_group(int32_t id_number)
{
  std::scoped_lock lock(ignore_mutex);
  vk_message_id_number_ignored.remove(id_number);
};

void raise_message(int32_t id_number,
                   VkDebugUtilsMessageSeverityFlagBitsEXT vk_severity_flag_bits,
                   const char *format,
                   ...)
{
  const VKDevice &device = VKBackend::get().device_get();
  const VKDebuggingTools &debugging_tools = device.debugging_tools_get();
  if (debugging_tools.enabled) {
    DynStr *ds = nullptr;
    va_list arg;
    char *info = nullptr;

    va_start(arg, format);

    ds = BLI_dynstr_new();
    BLI_dynstr_vappendf(ds, format, arg);
    info = BLI_dynstr_get_cstring(ds);
    BLI_dynstr_free(ds);

    va_end(arg);

    static VkDebugUtilsMessengerCallbackDataEXT vk_call_back_data;
    vk_call_back_data.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    vk_call_back_data.pNext = VK_NULL_HANDLE;
    vk_call_back_data.messageIdNumber = id_number;
    vk_call_back_data.pMessageIdName = "VulkanMessenger";
    vk_call_back_data.objectCount = 0;
    vk_call_back_data.flags = 0;
    vk_call_back_data.pObjects = VK_NULL_HANDLE;
    vk_call_back_data.pMessage = info;
    debugging_tools.vkSubmitDebugUtilsMessageEXT_r(device.instance_get(),
                                                   vk_severity_flag_bits,
                                                   VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                                   &vk_call_back_data);
    MEM_freeN((void *)info);
  }
}

};  // namespace blender::gpu::debug
