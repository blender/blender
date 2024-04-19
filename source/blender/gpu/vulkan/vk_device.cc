/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <sstream>

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_device.hh"
#include "vk_memory.hh"
#include "vk_state_manager.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

#include "GPU_capabilities.hh"

#include "BLI_math_matrix_types.hh"

#include "GHOST_C-api.h"

extern "C" char datatoc_glsl_shader_defines_glsl[];

namespace blender::gpu {

void VKDevice::reinit()
{
  samplers_.free();
  samplers_.init();
}

void VKDevice::deinit()
{
  VK_ALLOCATION_CALLBACKS
  if (!is_initialized()) {
    return;
  }

  timeline_semaphore_.free(*this);
  dummy_buffer_.free();
  if (dummy_color_attachment_.has_value()) {
    delete &(*dummy_color_attachment_).get();
    dummy_color_attachment_.reset();
  }
  samplers_.free();
  destroy_discarded_resources();
  vkDestroyPipelineCache(vk_device_, vk_pipeline_cache_, vk_allocation_callbacks);
  descriptor_set_layouts_.deinit();
  vmaDestroyAllocator(mem_allocator_);
  mem_allocator_ = VK_NULL_HANDLE;

  debugging_tools_.deinit(vk_instance_);

  vk_instance_ = VK_NULL_HANDLE;
  vk_physical_device_ = VK_NULL_HANDLE;
  vk_device_ = VK_NULL_HANDLE;
  vk_queue_family_ = 0;
  vk_queue_ = VK_NULL_HANDLE;
  vk_physical_device_properties_ = {};
  glsl_patch_.clear();
}

bool VKDevice::is_initialized() const
{
  return vk_device_ != VK_NULL_HANDLE;
}

void VKDevice::init(void *ghost_context)
{
  BLI_assert(!is_initialized());
  GHOST_GetVulkanHandles((GHOST_ContextHandle)ghost_context,
                         &vk_instance_,
                         &vk_physical_device_,
                         &vk_device_,
                         &vk_queue_family_,
                         &vk_queue_);

  init_physical_device_properties();
  init_physical_device_memory_properties();
  init_physical_device_features();
  VKBackend::platform_init(*this);
  VKBackend::capabilities_init(*this);
  init_debug_callbacks();
  init_memory_allocator();
  init_pipeline_cache();

  samplers_.init();
  timeline_semaphore_.init(*this);

  debug::object_label(device_get(), "LogicalDevice");
  debug::object_label(queue_get(), "GenericQueue");
  init_glsl_patch();
}

void VKDevice::init_debug_callbacks()
{
  debugging_tools_.init(vk_instance_);
}

void VKDevice::init_physical_device_properties()
{
  BLI_assert(vk_physical_device_ != VK_NULL_HANDLE);
  vkGetPhysicalDeviceProperties(vk_physical_device_, &vk_physical_device_properties_);
}

void VKDevice::init_physical_device_memory_properties()
{
  BLI_assert(vk_physical_device_ != VK_NULL_HANDLE);
  vkGetPhysicalDeviceMemoryProperties(vk_physical_device_, &vk_physical_device_memory_properties_);
}

void VKDevice::init_physical_device_features()
{
  BLI_assert(vk_physical_device_ != VK_NULL_HANDLE);

  VkPhysicalDeviceFeatures2 features = {};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  vk_physical_device_vulkan_11_features_.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vk_physical_device_vulkan_12_features_.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

  features.pNext = &vk_physical_device_vulkan_11_features_;
  vk_physical_device_vulkan_11_features_.pNext = &vk_physical_device_vulkan_12_features_;

  vkGetPhysicalDeviceFeatures2(vk_physical_device_, &features);
  vk_physical_device_features_ = features.features;
}

void VKDevice::init_memory_allocator()
{
  VK_ALLOCATION_CALLBACKS;
  VmaAllocatorCreateInfo info = {};
  info.vulkanApiVersion = VK_API_VERSION_1_2;
  info.physicalDevice = vk_physical_device_;
  info.device = vk_device_;
  info.instance = vk_instance_;
  info.pAllocationCallbacks = vk_allocation_callbacks;
  vmaCreateAllocator(&info, &mem_allocator_);
}

void VKDevice::init_pipeline_cache()
{
  VK_ALLOCATION_CALLBACKS;
  VkPipelineCacheCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  vkCreatePipelineCache(vk_device_, &create_info, vk_allocation_callbacks, &vk_pipeline_cache_);
}

void VKDevice::init_dummy_buffer(VKContext &context)
{
  if (dummy_buffer_.is_allocated()) {
    return;
  }

  dummy_buffer_.create(sizeof(float4x4),
                       GPU_USAGE_DEVICE_ONLY,
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  dummy_buffer_.clear(context, 0);
}

void VKDevice::init_dummy_color_attachment()
{
  if (dummy_color_attachment_.has_value()) {
    return;
  }

  GPUTexture *texture = GPU_texture_create_2d(
      "dummy_attachment", 1, 1, 1, GPU_R32F, GPU_TEXTURE_USAGE_ATTACHMENT, nullptr);
  BLI_assert(texture);
  VKTexture &vk_texture = *unwrap(unwrap(texture));
  dummy_color_attachment_ = std::make_optional(std::reference_wrapper(vk_texture));
}

void VKDevice::init_glsl_patch()
{
  std::stringstream ss;

  ss << "#version 450\n";
  if (GPU_shader_draw_parameters_support()) {
    ss << "#extension GL_ARB_shader_draw_parameters : enable\n";
    ss << "#define GPU_ARB_shader_draw_parameters\n";
    ss << "#define gpu_BaseInstance (gl_BaseInstanceARB)\n";
  }

  ss << "#define gl_VertexID gl_VertexIndex\n";
  ss << "#define gpu_InstanceIndex (gl_InstanceIndex)\n";
  ss << "#define gl_InstanceID (gpu_InstanceIndex - gpu_BaseInstance)\n";

  /* TODO(fclem): This creates a validation error and should be already part of Vulkan 1.2. */
  ss << "#extension GL_ARB_shader_viewport_layer_array: enable\n";
  if (!workarounds_.shader_output_layer) {
    ss << "#define gpu_Layer gl_Layer\n";
  }
  if (!workarounds_.shader_output_viewport_index) {
    ss << "#define gpu_ViewportIndex gl_ViewportIndex\n";
  }

  ss << "#define DFDX_SIGN 1.0\n";
  ss << "#define DFDY_SIGN 1.0\n";

  /* GLSL Backend Lib. */
  ss << datatoc_glsl_shader_defines_glsl;
  glsl_patch_ = ss.str();
}

const char *VKDevice::glsl_patch_get() const
{
  BLI_assert(!glsl_patch_.empty());
  return glsl_patch_.c_str();
}

/* -------------------------------------------------------------------- */
/** \name Platform/driver/device information
 * \{ */

constexpr int32_t PCI_ID_NVIDIA = 0x10de;
constexpr int32_t PCI_ID_INTEL = 0x8086;
constexpr int32_t PCI_ID_AMD = 0x1002;
constexpr int32_t PCI_ID_ATI = 0x1022;
constexpr int32_t PCI_ID_APPLE = 0x106b;

eGPUDeviceType VKDevice::device_type() const
{
  /* According to the vulkan specifications:
   *
   * If the vendor has a PCI vendor ID, the low 16 bits of vendorID must contain that PCI vendor
   * ID, and the remaining bits must be set to zero. Otherwise, the value returned must be a valid
   * Khronos vendor ID.
   */
  switch (vk_physical_device_properties_.vendorID) {
    case PCI_ID_NVIDIA:
      return GPU_DEVICE_NVIDIA;
    case PCI_ID_INTEL:
      return GPU_DEVICE_INTEL;
    case PCI_ID_AMD:
    case PCI_ID_ATI:
      return GPU_DEVICE_ATI;
    case PCI_ID_APPLE:
      return GPU_DEVICE_APPLE;
    default:
      break;
  }

  return GPU_DEVICE_UNKNOWN;
}

eGPUDriverType VKDevice::driver_type() const
{
  /* It is unclear how to determine the driver type, but it is required to extract the correct
   * driver version. */
  return GPU_DRIVER_ANY;
}

std::string VKDevice::vendor_name() const
{
  /* Below 0x10000 are the PCI vendor IDs (https://pcisig.com/membership/member-companies) */
  if (vk_physical_device_properties_.vendorID < 0x10000) {
    switch (vk_physical_device_properties_.vendorID) {
      case PCI_ID_AMD:
        return "Advanced Micro Devices";
      case PCI_ID_NVIDIA:
        return "NVIDIA Corporation";
      case PCI_ID_INTEL:
        return "Intel Corporation";
      case PCI_ID_APPLE:
        return "Apple";
      default:
        return std::to_string(vk_physical_device_properties_.vendorID);
    }
  }
  else {
    /* above 0x10000 should be vkVendorIDs
     * NOTE: When debug_messaging landed we can use something similar to
     * vk::to_string(vk::VendorId(properties.vendorID));
     */
    return std::to_string(vk_physical_device_properties_.vendorID);
  }
}

std::string VKDevice::driver_version() const
{
  /*
   * NOTE: this depends on the driver type and is currently incorrect. Idea is to use a default per
   * OS.
   */
  const uint32_t driver_version = vk_physical_device_properties_.driverVersion;
  switch (vk_physical_device_properties_.vendorID) {
    case PCI_ID_NVIDIA:
      return std::to_string((driver_version >> 22) & 0x3FF) + "." +
             std::to_string((driver_version >> 14) & 0xFF) + "." +
             std::to_string((driver_version >> 6) & 0xFF) + "." +
             std::to_string(driver_version & 0x3F);
    case PCI_ID_INTEL: {
      const uint32_t major = VK_VERSION_MAJOR(driver_version);
      /* When using Mesa driver we should use VK_VERSION_*. */
      if (major > 30) {
        return std::to_string((driver_version >> 14) & 0x3FFFF) + "." +
               std::to_string((driver_version & 0x3FFF));
      }
      break;
    }
    default:
      break;
  }

  return std::to_string(VK_VERSION_MAJOR(driver_version)) + "." +
         std::to_string(VK_VERSION_MINOR(driver_version)) + "." +
         std::to_string(VK_VERSION_PATCH(driver_version));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resource management
 * \{ */

void VKDevice::context_register(VKContext &context)
{
  contexts_.append(std::reference_wrapper(context));
}

void VKDevice::context_unregister(VKContext &context)
{
  contexts_.remove(contexts_.first_index_of(std::reference_wrapper(context)));
}
Span<std::reference_wrapper<VKContext>> VKDevice::contexts_get() const
{
  return contexts_;
};

void VKDevice::discard_image(VkImage vk_image, VmaAllocation vma_allocation)
{
  discarded_images_.append(std::pair(vk_image, vma_allocation));
}

void VKDevice::discard_image_view(VkImageView vk_image_view)
{
  discarded_image_views_.append(vk_image_view);
}

void VKDevice::discard_buffer(VkBuffer vk_buffer, VmaAllocation vma_allocation)
{
  discarded_buffers_.append(std::pair(vk_buffer, vma_allocation));
}

void VKDevice::discard_render_pass(VkRenderPass vk_render_pass)
{
  discarded_render_passes_.append(vk_render_pass);
}
void VKDevice::discard_frame_buffer(VkFramebuffer vk_frame_buffer)
{
  discarded_frame_buffers_.append(vk_frame_buffer);
}

void VKDevice::destroy_discarded_resources()
{
  VK_ALLOCATION_CALLBACKS

  while (!discarded_image_views_.is_empty()) {
    VkImageView vk_image_view = discarded_image_views_.pop_last();
    vkDestroyImageView(vk_device_, vk_image_view, vk_allocation_callbacks);
  }

  while (!discarded_images_.is_empty()) {
    std::pair<VkImage, VmaAllocation> image_allocation = discarded_images_.pop_last();
    if (use_render_graph) {
      resources.remove_image(image_allocation.first);
    }
    vmaDestroyImage(mem_allocator_get(), image_allocation.first, image_allocation.second);
  }

  while (!discarded_buffers_.is_empty()) {
    std::pair<VkBuffer, VmaAllocation> buffer_allocation = discarded_buffers_.pop_last();
    if (use_render_graph) {
      resources.remove_buffer(buffer_allocation.first);
    }
    vmaDestroyBuffer(mem_allocator_get(), buffer_allocation.first, buffer_allocation.second);
  }

  while (!discarded_render_passes_.is_empty()) {
    VkRenderPass vk_render_pass = discarded_render_passes_.pop_last();
    vkDestroyRenderPass(vk_device_, vk_render_pass, vk_allocation_callbacks);
  }

  while (!discarded_frame_buffers_.is_empty()) {
    VkFramebuffer vk_frame_buffer = discarded_frame_buffers_.pop_last();
    vkDestroyFramebuffer(vk_device_, vk_frame_buffer, vk_allocation_callbacks);
  }
}

void VKDevice::memory_statistics_get(int *r_total_mem_kb, int *r_free_mem_kb) const
{
  VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
  vmaGetHeapBudgets(mem_allocator_get(), budgets);
  VkDeviceSize total_mem = 0;
  VkDeviceSize used_mem = 0;

  for (int memory_heap_index : IndexRange(vk_physical_device_memory_properties_.memoryHeapCount)) {
    const VkMemoryHeap &memory_heap =
        vk_physical_device_memory_properties_.memoryHeaps[memory_heap_index];
    const VmaBudget &budget = budgets[memory_heap_index];

    /* Skip host memory-heaps. */
    if (!bool(memory_heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) {
      continue;
    }

    total_mem += memory_heap.size;
    used_mem += budget.usage;
  }

  *r_total_mem_kb = int(total_mem / 1024);
  *r_free_mem_kb = int((total_mem - used_mem) / 1024);
}

/** \} */

}  // namespace blender::gpu
