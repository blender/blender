/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_device.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_memory.hh"
#include "vk_state_manager.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_math_matrix_types.hh"

#include "GHOST_C-api.h"

namespace blender::gpu {

void VKDevice::deinit()
{
  dummy_buffer_.free();
  sampler_.free();
  vmaDestroyAllocator(mem_allocator_);
  mem_allocator_ = VK_NULL_HANDLE;
  debugging_tools_.deinit(vk_instance_);

  vk_instance_ = VK_NULL_HANDLE;
  vk_physical_device_ = VK_NULL_HANDLE;
  vk_device_ = VK_NULL_HANDLE;
  vk_queue_family_ = 0;
  vk_queue_ = VK_NULL_HANDLE;
  vk_physical_device_properties_ = {};
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
  VKBackend::platform_init(*this);
  VKBackend::capabilities_init(*this);
  init_debug_callbacks();
  init_memory_allocator();
  init_descriptor_pools();

  sampler_.create();

  debug::object_label(device_get(), "LogicalDevice");
  debug::object_label(queue_get(), "GenericQueue");
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

void VKDevice::init_descriptor_pools()
{
  descriptor_pools_.init(vk_device_);
}

void VKDevice::init_dummy_buffer(VKContext &context)
{
  if (dummy_buffer_.is_allocated()) {
    return;
  }

  dummy_buffer_.create(sizeof(float4x4), GPU_USAGE_DEVICE_ONLY, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  dummy_buffer_.clear(context, 0);
}

/* -------------------------------------------------------------------- */
/** \name Platform/driver/device information
 * \{ */

constexpr int32_t PCI_ID_NVIDIA = 0x10de;
constexpr int32_t PCI_ID_INTEL = 0x8086;
constexpr int32_t PCI_ID_AMD = 0x1002;
constexpr int32_t PCI_ID_ATI = 0x1022;

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
      case 0x1022:
        return "Advanced Micro Devices";
      case 0x10DE:
        return "NVIDIA Corporation";
      case 0x8086:
        return "Intel Corporation";
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
const Vector<std::reference_wrapper<VKContext>> &VKDevice::contexts_get() const
{
  return contexts_;
};

/** \} */

}  // namespace blender::gpu
