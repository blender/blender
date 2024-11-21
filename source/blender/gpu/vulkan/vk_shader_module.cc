/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_shader_module.hh"
#include "vk_backend.hh"
#include "vk_shader.hh"

#include <iomanip>
#include <sstream>

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
  if (compilation_result.GetCompilationStatus() != shaderc_compilation_status_success &&
      spirv_binary.is_empty())
  {
    return;
  }

  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  if (!spirv_binary.is_empty()) {
    create_info.codeSize = spirv_binary.size() * sizeof(uint32_t);
    create_info.pCode = spirv_binary.data();
  }
  else {
    create_info.codeSize = (compilation_result.end() - compilation_result.begin()) *
                           sizeof(uint32_t);
    create_info.pCode = compilation_result.begin();
  }

  const VKDevice &device = VKBackend::get().device;
  vkCreateShaderModule(device.vk_handle(), &create_info, nullptr, &vk_shader_module);
  debug::object_label(vk_shader_module, name.c_str());
}

void VKShaderModule::build_sources_hash()
{
  DefaultHash<std::string> hasher;
  BLI_assert(!combined_sources.empty());
  uint64_t hash = hasher(combined_sources);
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex << hash;
  sources_hash = ss.str();
  BLI_assert(!sources_hash.empty());
}

}  // namespace blender::gpu
