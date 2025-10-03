/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <typeinfo>

#ifdef _WIN32
#  include "BLI_winstuff.h"
#endif

#include <vulkan/vulkan.h>
#ifdef _WIN32
#  include <vulkan/vulkan_win32.h>
#endif

#if !defined(_WIN32) or defined(_M_ARM64)
/* Silence compilation warning on non-windows x64 systems. */
#  define VMA_EXTERNAL_MEMORY_WIN32 0
#endif
#include "vk_mem_alloc.h"

#include "GPU_index_buffer.hh"
#include "GPU_state.hh"
#include "gpu_query.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_texture_private.hh"

namespace blender::gpu {

using TimelineValue = uint64_t;

/**
 * Based on the usage of an Image View a different image view type should be created.
 *
 * When using a GPU_TEXTURE_CUBE as an frame buffer attachment it will be used as a
 * GPU_TEXTURE_2D_ARRAY. eg only a single side of the cube map will be attached. But when bound as
 * a shader resource the cube-map will be used.
 */
enum class eImageViewUsage {
  /** Image View will be used as a bindable shader resource. */
  ShaderBinding,
  /** Image View will be used as an frame-buffer attachment. */
  Attachment,
};

enum class VKImageViewArrayed {
  DONT_CARE,
  NOT_ARRAYED,
  ARRAYED,
};

struct VKSubImageRange {
  uint32_t mipmap_level = 0;
  uint32_t mipmap_count = VK_REMAINING_MIP_LEVELS;
  uint32_t layer_base = 0;
  uint32_t layer_count = VK_REMAINING_ARRAY_LAYERS;
};

VkImageAspectFlags to_vk_image_aspect_flag_bits(const TextureFormat format);
VkImageAspectFlags to_vk_image_aspect_flag_bits(const GPUFrameBufferBits buffers);
VkFormat to_vk_format(const TextureFormat format);
TextureFormat to_gpu_format(const VkFormat format);
VkFormat to_vk_format(const GPUVertCompType type,
                      const uint32_t size,
                      const GPUVertFetchMode fetch_mode);
VkFormat to_vk_format(const shader::Type type);
VkQueryType to_vk_query_type(const GPUQueryType query_type);

VkComponentSwizzle to_vk_component_swizzle(const char swizzle);
VkImageViewType to_vk_image_view_type(const GPUTextureType type,
                                      eImageViewUsage view_type,
                                      VKImageViewArrayed arrayed);
VkImageType to_vk_image_type(const GPUTextureType type);
VkClearColorValue to_vk_clear_color_value(const eGPUDataFormat format, const void *data);
VkIndexType to_vk_index_type(const GPUIndexBufType index_type);
VkPrimitiveTopology to_vk_primitive_topology(const GPUPrimType prim_type);
VkCullModeFlags to_vk_cull_mode_flags(const GPUFaceCullTest cull_test);
VkSamplerAddressMode to_vk_sampler_address_mode(const GPUSamplerExtendMode extend_mode);
VkDescriptorType to_vk_descriptor_type(const shader::ShaderCreateInfo::Resource &resource);

template<typename T> VkObjectType to_vk_object_type(T /*vk_obj*/)
{
  const std::type_info &tid = typeid(T);
#define VK_EQ_TYPEID(name, name2) \
  if (tid == typeid(name)) { \
    return VK_OBJECT_TYPE_##name2; \
  }

  VK_EQ_TYPEID(VkInstance, INSTANCE);
  VK_EQ_TYPEID(VkPhysicalDevice, PHYSICAL_DEVICE);
  VK_EQ_TYPEID(VkDevice, DEVICE);
  VK_EQ_TYPEID(VkQueue, QUEUE);
  VK_EQ_TYPEID(VkSemaphore, SEMAPHORE);
  VK_EQ_TYPEID(VkCommandBuffer, COMMAND_BUFFER);
  VK_EQ_TYPEID(VkFence, FENCE);
  VK_EQ_TYPEID(VkDeviceMemory, DEVICE_MEMORY);
  VK_EQ_TYPEID(VkBuffer, BUFFER);
  VK_EQ_TYPEID(VkImage, IMAGE);
  VK_EQ_TYPEID(VkEvent, EVENT);
  VK_EQ_TYPEID(VkQueryPool, QUERY_POOL);
  VK_EQ_TYPEID(VkBufferView, BUFFER_VIEW);
  VK_EQ_TYPEID(VkImageView, IMAGE_VIEW);
  VK_EQ_TYPEID(VkShaderModule, SHADER_MODULE);
  VK_EQ_TYPEID(VkPipelineCache, PIPELINE_CACHE);
  VK_EQ_TYPEID(VkPipelineLayout, PIPELINE_LAYOUT);
  VK_EQ_TYPEID(VkRenderPass, RENDER_PASS);
  VK_EQ_TYPEID(VkPipeline, PIPELINE);
  VK_EQ_TYPEID(VkDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT);
  VK_EQ_TYPEID(VkSampler, SAMPLER);
  VK_EQ_TYPEID(VkDescriptorPool, DESCRIPTOR_POOL);
  VK_EQ_TYPEID(VkDescriptorSet, DESCRIPTOR_SET);
  VK_EQ_TYPEID(VkFramebuffer, FRAMEBUFFER);
  VK_EQ_TYPEID(VkCommandPool, COMMAND_POOL);
  VK_EQ_TYPEID(VkSamplerYcbcrConversion, SAMPLER_YCBCR_CONVERSION);
  VK_EQ_TYPEID(VkDescriptorUpdateTemplate, DESCRIPTOR_UPDATE_TEMPLATE);
  VK_EQ_TYPEID(VkSurfaceKHR, SURFACE_KHR);
  VK_EQ_TYPEID(VkSwapchainKHR, SWAPCHAIN_KHR);
  VK_EQ_TYPEID(VkDisplayKHR, DISPLAY_KHR);
  VK_EQ_TYPEID(VkDisplayModeKHR, DISPLAY_MODE_KHR);
  VK_EQ_TYPEID(VkDebugReportCallbackEXT, DEBUG_REPORT_CALLBACK_EXT);
#ifdef VK_ENABLE_BETA_EXTENSIONS
  VK_EQ_TYPEID(VkVideoSessionKHR, VIDEO_SESSION_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
  VK_EQ_TYPEID(VkVideoSessionParametersKHR, VIDEO_SESSION_PARAMETERS_KHR);
#endif
  VK_EQ_TYPEID(VkCuModuleNVX, CU_MODULE_NVX);
  VK_EQ_TYPEID(VkCuFunctionNVX, CU_FUNCTION_NVX);
  VK_EQ_TYPEID(VkDebugUtilsMessengerEXT, DEBUG_UTILS_MESSENGER_EXT);
  VK_EQ_TYPEID(VkAccelerationStructureKHR, ACCELERATION_STRUCTURE_KHR);
  VK_EQ_TYPEID(VkValidationCacheEXT, VALIDATION_CACHE_EXT);
  VK_EQ_TYPEID(VkAccelerationStructureNV, ACCELERATION_STRUCTURE_NV);
  VK_EQ_TYPEID(VkPerformanceConfigurationINTEL, PERFORMANCE_CONFIGURATION_INTEL);
  VK_EQ_TYPEID(VkDeferredOperationKHR, DEFERRED_OPERATION_KHR);
  VK_EQ_TYPEID(VkIndirectCommandsLayoutNV, INDIRECT_COMMANDS_LAYOUT_NV);
  VK_EQ_TYPEID(VkPrivateDataSlotEXT, PRIVATE_DATA_SLOT_EXT);

  BLI_assert_unreachable();
#undef VK_EQ_TYPEID
  return VK_OBJECT_TYPE_UNKNOWN;
}

#define NOT_YET_IMPLEMENTED \
  printf("%s:%d `%s` not implemented yet\n", __FILE__, __LINE__, __func__);

}  // namespace blender::gpu
