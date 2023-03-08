/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_pipeline.hh"
#include "vk_context.hh"
#include "vk_memory.hh"

namespace blender::gpu {

VKPipeline::VKPipeline(VkPipeline vk_pipeline,
                       VKDescriptorSet &&descriptor_set,
                       VKPushConstants &&push_constants)
    : vk_pipeline_(vk_pipeline),
      descriptor_set_(std::move(descriptor_set)),
      push_constants_(std::move(push_constants))
{
}

VKPipeline::~VKPipeline()
{
  VK_ALLOCATION_CALLBACKS
  VkDevice vk_device = VKContext::get()->device_get();
  if (vk_pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(vk_device, vk_pipeline_, vk_allocation_callbacks);
  }
}

VKPipeline VKPipeline::create_compute_pipeline(
    VKContext &context,
    VkShaderModule compute_module,
    VkDescriptorSetLayout &descriptor_set_layout,
    VkPipelineLayout &pipeline_layout,
    const VKPushConstants::Layout &push_constants_layout)
{
  VK_ALLOCATION_CALLBACKS
  VkDevice vk_device = context.device_get();
  VkComputePipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.flags = 0;
  pipeline_info.stage = {};
  pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_info.stage.flags = 0;
  pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipeline_info.stage.module = compute_module;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.stage.pName = "main";

  VkPipeline vk_pipeline;
  if (vkCreateComputePipelines(
          vk_device, nullptr, 1, &pipeline_info, vk_allocation_callbacks, &vk_pipeline) !=
      VK_SUCCESS) {
    return VKPipeline();
  }

  VKDescriptorSet descriptor_set = context.descriptor_pools_get().allocate(descriptor_set_layout);
  VKPushConstants push_constants(&push_constants_layout);
  return VKPipeline(vk_pipeline, std::move(descriptor_set), std::move(push_constants));
}

VkPipeline VKPipeline::vk_handle() const
{
  return vk_pipeline_;
}

bool VKPipeline::is_valid() const
{
  return vk_pipeline_ != VK_NULL_HANDLE;
}

}  // namespace blender::gpu
