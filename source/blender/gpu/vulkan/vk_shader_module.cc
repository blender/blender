/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_shader_module.hh"
#include "vk_backend.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"

namespace blender::gpu {
VKShaderModule::~VKShaderModule()
{
  VKDevice &device = VKBackend::get().device;
  VKDiscardPool &discard_pool = device.discard_pool_for_current_thread();
  if (vk_shader_module != VK_NULL_HANDLE) {
    discard_pool.discard_shader_module(vk_shader_module);
    vk_shader_module = VK_NULL_HANDLE;
  }
}

void VKShaderModule::finalize(StringRefNull name)
{
  BLI_assert(vk_shader_module == VK_NULL_HANDLE);
  if (compilation_result.GetCompilationStatus() != shaderc_compilation_status_success) {
    return;
  }

  VK_ALLOCATION_CALLBACKS;

  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = (compilation_result.end() - compilation_result.begin()) *
                         sizeof(uint32_t);
  create_info.pCode = compilation_result.begin();

  const VKDevice &device = VKBackend::get().device;
  vkCreateShaderModule(
      device.vk_handle(), &create_info, vk_allocation_callbacks, &vk_shader_module);
  debug::object_label(vk_shader_module, name.c_str());
}

}  // namespace blender::gpu
