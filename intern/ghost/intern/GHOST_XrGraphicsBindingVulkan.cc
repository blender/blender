/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <algorithm>
#include <cstring>
#include <sstream>

#include "GHOST_ContextVK.hh"
#include "GHOST_XrException.hh"
#include "GHOST_XrGraphicsBindingVulkan.hh"
#include "GHOST_Xr_intern.hh"

/** OpenXR/Vulkan specific function pointers. */
PFN_xrGetVulkanGraphicsRequirements2KHR
    GHOST_XrGraphicsBindingVulkan::s_xrGetVulkanGraphicsRequirements2KHR_fn = nullptr;
PFN_xrGetVulkanGraphicsDevice2KHR
    GHOST_XrGraphicsBindingVulkan::s_xrGetVulkanGraphicsDevice2KHR_fn = nullptr;
PFN_xrCreateVulkanInstanceKHR GHOST_XrGraphicsBindingVulkan::s_xrCreateVulkanInstanceKHR_fn =
    nullptr;
PFN_xrCreateVulkanDeviceKHR GHOST_XrGraphicsBindingVulkan::s_xrCreateVulkanDeviceKHR_fn = nullptr;

/* -------------------------------------------------------------------- */
/** \name Destroying resources.
 * \{ */

GHOST_XrGraphicsBindingVulkan::~GHOST_XrGraphicsBindingVulkan()
{
  m_ghost_ctx = nullptr;

  /* Destroy buffer */
  if (m_vk_buffer != VK_NULL_HANDLE) {
    vmaUnmapMemory(m_vma_allocator, m_vk_buffer_allocation);
    vmaDestroyBuffer(m_vma_allocator, m_vk_buffer, m_vk_buffer_allocation);
    m_vk_buffer = VK_NULL_HANDLE;
    m_vk_buffer_allocation = VK_NULL_HANDLE;
  }

  /* Destroy VMA */
  if (m_vma_allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(m_vma_allocator);
    m_vma_allocator = VK_NULL_HANDLE;
  }

  /* Destroy command buffer */
  if (m_vk_command_buffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(m_vk_device, m_vk_command_pool, 1, &m_vk_command_buffer);
    m_vk_command_buffer = VK_NULL_HANDLE;
  }

  /* Destroy command pool */
  if (m_vk_command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_vk_device, m_vk_command_pool, nullptr);
    m_vk_command_pool = VK_NULL_HANDLE;
  }

  m_vk_queue = VK_NULL_HANDLE;

  /* Destroy device */
  if (m_vk_device != VK_NULL_HANDLE) {
    vkDestroyDevice(m_vk_device, nullptr);
    m_vk_device = VK_NULL_HANDLE;
  }

  /* Destroy instance */
  if (m_vk_instance != VK_NULL_HANDLE) {
    vkDestroyInstance(m_vk_instance, nullptr);
    m_vk_instance = VK_NULL_HANDLE;
  }

  s_xrGetVulkanGraphicsRequirements2KHR_fn = nullptr;
  s_xrGetVulkanGraphicsDevice2KHR_fn = nullptr;
  s_xrCreateVulkanInstanceKHR_fn = nullptr;
  s_xrCreateVulkanDeviceKHR_fn = nullptr;
}

/* \} */

bool GHOST_XrGraphicsBindingVulkan::checkVersionRequirements(GHOST_Context &ghost_ctx,
                                                             XrInstance instance,
                                                             XrSystemId system_id,
                                                             std::string *r_requirement_info) const
{
#define LOAD_PFN(var, name) \
  if (var == nullptr && \
      XR_FAILED(xrGetInstanceProcAddr(instance, #name, (PFN_xrVoidFunction *)&var))) \
  { \
    var = nullptr; \
    *r_requirement_info = std::string("Unable to retrieve " #name " instance function"); \
    return false; \
  }
  /* Get the function pointers for OpenXR/Vulkan. If any fails we expect that we cannot use the
   * given context. */
  LOAD_PFN(s_xrGetVulkanGraphicsRequirements2KHR_fn, xrGetVulkanGraphicsRequirements2KHR);
  LOAD_PFN(s_xrGetVulkanGraphicsDevice2KHR_fn, xrGetVulkanGraphicsDevice2KHR);
  LOAD_PFN(s_xrCreateVulkanInstanceKHR_fn, xrCreateVulkanInstanceKHR);
  LOAD_PFN(s_xrCreateVulkanDeviceKHR_fn, xrCreateVulkanDeviceKHR);
#undef LOAD_PFN

  XrGraphicsRequirementsVulkanKHR xr_graphics_requirements{
      /*type*/ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
  };
  if (XR_FAILED(s_xrGetVulkanGraphicsRequirements2KHR_fn(
          instance, system_id, &xr_graphics_requirements)))
  {
    *r_requirement_info = std::string("Unable to retrieve Xr version requirements for Vulkan");
    return false;
  }

  /* Check if the Vulkan API instance version is supported. */
  GHOST_ContextVK &context_vk = static_cast<GHOST_ContextVK &>(ghost_ctx);
  const XrVersion vk_version = XR_MAKE_VERSION(
      context_vk.m_context_major_version, context_vk.m_context_minor_version, 0);
  if (vk_version < xr_graphics_requirements.minApiVersionSupported ||
      vk_version > xr_graphics_requirements.maxApiVersionSupported)
  {
    std::ostringstream strstream;
    strstream << "Min Vulkan version "
              << XR_VERSION_MAJOR(xr_graphics_requirements.minApiVersionSupported) << "."
              << XR_VERSION_MINOR(xr_graphics_requirements.minApiVersionSupported) << std::endl;
    strstream << "Max Vulkan version "
              << XR_VERSION_MAJOR(xr_graphics_requirements.maxApiVersionSupported) << "."
              << XR_VERSION_MINOR(xr_graphics_requirements.maxApiVersionSupported) << std::endl;

    *r_requirement_info = strstream.str();
    return false;
  }

  return true;
}

void GHOST_XrGraphicsBindingVulkan::initFromGhostContext(GHOST_Context &ghost_ctx,
                                                         XrInstance instance,
                                                         XrSystemId system_id)
{
  m_ghost_ctx = static_cast<GHOST_ContextVK *>(&ghost_ctx);
  /* Create a new VkInstance that is compatible with OpenXR */
  VkApplicationInfo vk_application_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                           nullptr,
                                           "Blender",
                                           VK_MAKE_VERSION(1, 0, 0),
                                           "BlenderXR",
                                           VK_MAKE_VERSION(1, 0, 0),
                                           VK_MAKE_VERSION(1, 2, 0)};
  VkInstanceCreateInfo vk_instance_create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                  nullptr,
                                                  0,
                                                  &vk_application_info,
                                                  0,
                                                  nullptr,
                                                  0,
                                                  nullptr};
  XrVulkanInstanceCreateInfoKHR xr_instance_create_info = {XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
                                                           nullptr,
                                                           system_id,
                                                           0,
                                                           vkGetInstanceProcAddr,
                                                           &vk_instance_create_info,
                                                           nullptr};
  VkResult vk_result;
  CHECK_XR(s_xrCreateVulkanInstanceKHR_fn(
               instance, &xr_instance_create_info, &m_vk_instance, &vk_result),
           "Unable to create an OpenXR compatible Vulkan instance.");

  /* Physical device selection */
  XrVulkanGraphicsDeviceGetInfoKHR xr_device_get_info = {
      XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR, nullptr, system_id, m_vk_instance};
  s_xrGetVulkanGraphicsDevice2KHR_fn(instance, &xr_device_get_info, &m_vk_physical_device);

  /* Queue family */
  uint32_t vk_queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_vk_physical_device, &vk_queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> vk_queue_families(vk_queue_family_count);
  m_graphics_queue_family = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(
      m_vk_physical_device, &vk_queue_family_count, vk_queue_families.data());
  for (uint32_t i = 0; i < vk_queue_family_count; i++) {
    if (vk_queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
        vk_queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
    {
      m_graphics_queue_family = i;
      break;
    }
  }

  /* Graphic device creation */
  const float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo vk_queue_create_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                  nullptr,
                                                  0,
                                                  m_graphics_queue_family,
                                                  1,
                                                  &queue_priority};
  VkDeviceCreateInfo vk_device_create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                              nullptr,
                                              0,
                                              1,
                                              &vk_queue_create_info,
                                              0,
                                              nullptr,
                                              0,
                                              nullptr};
  XrVulkanDeviceCreateInfoKHR xr_device_create_info = {XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
                                                       nullptr,
                                                       system_id,
                                                       0,
                                                       vkGetInstanceProcAddr,
                                                       m_vk_physical_device,
                                                       &vk_device_create_info,
                                                       nullptr};
  CHECK_XR(
      s_xrCreateVulkanDeviceKHR_fn(instance, &xr_device_create_info, &m_vk_device, &vk_result),
      "Unable to create an OpenXR compatible Vulkan device.");

  vkGetDeviceQueue(m_vk_device, m_graphics_queue_family, 0, &m_vk_queue);

  /* Command buffer pool */
  VkCommandPoolCreateInfo vk_command_pool_create_info = {
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      nullptr,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      m_graphics_queue_family};
  vkCreateCommandPool(m_vk_device, &vk_command_pool_create_info, nullptr, &m_vk_command_pool);

  /* Command buffer */
  VkCommandBufferAllocateInfo vk_command_buffer_allocate_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      nullptr,
      m_vk_command_pool,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      1};
  vkAllocateCommandBuffers(m_vk_device, &vk_command_buffer_allocate_info, &m_vk_command_buffer);

  /* Select the best data transfer mode based on the OpenXR device and ContextVK. */
  m_data_transfer_mode = choseDataTransferMode();

  if (m_data_transfer_mode == GHOST_kVulkanXRModeCPU) {
    /* VMA */
    VmaAllocatorCreateInfo allocator_create_info = {};
    allocator_create_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_2;
    allocator_create_info.physicalDevice = m_vk_physical_device;
    allocator_create_info.device = m_vk_device;
    allocator_create_info.instance = m_vk_instance;
    vmaCreateAllocator(&allocator_create_info, &m_vma_allocator);
  }

  /* Update the binding struct */
  oxr_binding.vk.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
  oxr_binding.vk.next = nullptr;
  oxr_binding.vk.instance = m_vk_instance;
  oxr_binding.vk.physicalDevice = m_vk_physical_device;
  oxr_binding.vk.device = m_vk_device;
  oxr_binding.vk.queueFamilyIndex = m_graphics_queue_family;
  oxr_binding.vk.queueIndex = 0;
}

GHOST_TVulkanXRModes GHOST_XrGraphicsBindingVulkan::choseDataTransferMode()
{
  GHOST_VulkanHandles vulkan_handles;
  m_ghost_ctx->getVulkanHandles(vulkan_handles);

  /* Retrieve the Context physical device properties. */
  VkPhysicalDeviceVulkan11Properties vk_physical_device_vulkan11_properties = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
  VkPhysicalDeviceProperties2 vk_physical_device_properties = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &vk_physical_device_vulkan11_properties};
  vkGetPhysicalDeviceProperties2(vulkan_handles.physical_device, &vk_physical_device_properties);

  /* Retrieve OpenXR physical device properties. */
  VkPhysicalDeviceVulkan11Properties xr_physical_device_vulkan11_properties = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
  VkPhysicalDeviceProperties2 xr_physical_device_properties = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &xr_physical_device_vulkan11_properties};
  vkGetPhysicalDeviceProperties2(m_vk_physical_device, &xr_physical_device_properties);

  /* When the physical device properties match between the Vulkan device and the Xr devices we
   * assume that they are the same physical device in the machine and we can use shared memory.
   * If not we fall back to CPU based data transfer.*/
  const bool is_same_physical_device = memcmp(&vk_physical_device_vulkan11_properties,
                                              &xr_physical_device_vulkan11_properties,
                                              sizeof(VkPhysicalDeviceVulkan11Properties)) == 0;
  if (!is_same_physical_device) {
    return GHOST_kVulkanXRModeCPU;
  }

  /* Check for available extensions. We assume that the needed extensions are enabled when
   * available during construction. */
  uint32_t device_extension_count;
  vkEnumerateDeviceExtensionProperties(
      vulkan_handles.physical_device, nullptr, &device_extension_count, nullptr);
  std::vector<VkExtensionProperties> available_device_extensions(device_extension_count);
  vkEnumerateDeviceExtensionProperties(vulkan_handles.physical_device,
                                       nullptr,
                                       &device_extension_count,
                                       available_device_extensions.data());

  auto has_extension = [=](const char *extension_name) {
    for (const auto &extension : available_device_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        return true;
      }
    }
    return false;
  };

#ifdef _WIN32
#elif defined(__APPLE__)
#else /* UNIX/Linux */
  bool has_vk_khr_external_memory_fd_extension = has_extension(
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
  if (has_vk_khr_external_memory_fd_extension) {
    return GHOST_kVulkanXRModeFD;
  }
#endif

  return GHOST_kVulkanXRModeCPU;
}

static std::optional<int64_t> choose_swapchain_format_from_candidates(
    const std::vector<int64_t> &gpu_binding_formats, const std::vector<int64_t> &runtime_formats)
{
  if (gpu_binding_formats.empty()) {
    return std::nullopt;
  }

  auto res = std::find_first_of(gpu_binding_formats.begin(),
                                gpu_binding_formats.end(),
                                runtime_formats.begin(),
                                runtime_formats.end());
  if (res == gpu_binding_formats.end()) {
    return std::nullopt;
  }

  return *res;
}

std::optional<int64_t> GHOST_XrGraphicsBindingVulkan::chooseSwapchainFormat(
    const std::vector<int64_t> &runtime_formats,
    GHOST_TXrSwapchainFormat &r_format,
    bool &r_is_srgb_format) const
{
  std::vector<int64_t> gpu_binding_formats = {
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_SRGB,
  };

  r_format = GHOST_kXrSwapchainFormatRGBA8;
  r_is_srgb_format = false;
  std::optional result = choose_swapchain_format_from_candidates(gpu_binding_formats,
                                                                 runtime_formats);
  if (result) {
    switch (*result) {
      case VK_FORMAT_R16G16B16A16_SFLOAT:
        r_format = GHOST_kXrSwapchainFormatRGBA16F;
        break;
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_R8G8B8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_SRGB:
        r_format = GHOST_kXrSwapchainFormatRGBA8;
        break;
    }

    switch (*result) {
      case VK_FORMAT_R16G16B16A16_SFLOAT:
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_UNORM:
        r_is_srgb_format = false;
        break;
      case VK_FORMAT_R8G8B8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_SRGB:
        r_is_srgb_format = true;
        break;
    }
  }
  return result;
}

std::vector<XrSwapchainImageBaseHeader *> GHOST_XrGraphicsBindingVulkan::createSwapchainImages(
    uint32_t image_count)
{
  std::vector<XrSwapchainImageBaseHeader *> base_images;
  std::vector<XrSwapchainImageVulkan2KHR> vulkan_images(
      image_count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR, nullptr, VK_NULL_HANDLE});
  for (XrSwapchainImageVulkan2KHR &image : vulkan_images) {
    base_images.push_back(reinterpret_cast<XrSwapchainImageBaseHeader *>(&image));
  }
  m_image_cache.push_back(std::move(vulkan_images));

  return base_images;
}

void GHOST_XrGraphicsBindingVulkan::submitToSwapchainImage(
    XrSwapchainImageBaseHeader &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  XrSwapchainImageVulkan2KHR &vulkan_image = *reinterpret_cast<XrSwapchainImageVulkan2KHR *>(
      &swapchain_image);

  switch (m_data_transfer_mode) {
    case GHOST_kVulkanXRModeFD:
      submitToSwapchainImageFd(vulkan_image, draw_info);
      break;

    case GHOST_kVulkanXRModeCPU:
      submitToSwapchainImageCpu(vulkan_image, draw_info);
      break;

    default:
      // assert(false);
      break;
  }
}

/* -------------------------------------------------------------------- */
/** \name Data transfer CPU
 * \{ */

void GHOST_XrGraphicsBindingVulkan::submitToSwapchainImageCpu(
    XrSwapchainImageVulkan2KHR &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  /* Acquire frame buffer image. */
  GHOST_VulkanOpenXRData openxr_data = {GHOST_kVulkanXRModeCPU};
  m_ghost_ctx->openxr_acquire_framebuffer_image_callback_(&openxr_data);

  /* Import render result. */
  VkDeviceSize component_size = 4 * sizeof(uint8_t);
  if (draw_info.swapchain_format == GHOST_kXrSwapchainFormatRGBA16F ||
      draw_info.swapchain_format == GHOST_kXrSwapchainFormatRGBA16)
  {
    component_size = 4 * sizeof(uint16_t);
  }
  VkDeviceSize image_data_size = openxr_data.extent.width * openxr_data.extent.height *
                                 component_size;

  if (m_vk_buffer != VK_NULL_HANDLE && m_vk_buffer_allocation_info.size < image_data_size) {
    vmaUnmapMemory(m_vma_allocator, m_vk_buffer_allocation);
    vmaDestroyBuffer(m_vma_allocator, m_vk_buffer, m_vk_buffer_allocation);
    m_vk_buffer = VK_NULL_HANDLE;
    m_vk_buffer_allocation = VK_NULL_HANDLE;
  }

  if (m_vk_buffer == VK_NULL_HANDLE) {
    VkBufferCreateInfo vk_buffer_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                nullptr,
                                                0,
                                                image_data_size,
                                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                VK_SHARING_MODE_EXCLUSIVE,
                                                0,
                                                nullptr};
    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocation_create_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    vmaCreateBuffer(m_vma_allocator,
                    &vk_buffer_create_info,
                    &allocation_create_info,
                    &m_vk_buffer,
                    &m_vk_buffer_allocation,
                    &m_vk_buffer_allocation_info);
    vmaMapMemory(
        m_vma_allocator, m_vk_buffer_allocation, &m_vk_buffer_allocation_info.pMappedData);
  }
  std::memcpy(
      m_vk_buffer_allocation_info.pMappedData, openxr_data.cpu.image_data, image_data_size);

  /* Copy frame buffer image to swapchain image. */
  VkCommandBuffer vk_command_buffer = m_vk_command_buffer;

  /* - Begin command recording */
  VkCommandBufferBeginInfo vk_command_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr};
  vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);

  /* Transfer imported render result & swap chain image (UNDEFINED -> GENERAL) */
  VkImageMemoryBarrier vk_image_memory_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                  nullptr,
                                                  0,
                                                  VK_ACCESS_TRANSFER_WRITE_BIT,
                                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                                  VK_IMAGE_LAYOUT_GENERAL,
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  swapchain_image.image,
                                                  {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(vk_command_buffer,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &vk_image_memory_barrier);

  /* Copy buffer to image */
  VkBufferImageCopy vk_buffer_image_copy = {
      0,
      0,
      0,
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      {draw_info.ofsx, draw_info.ofsy, 0},
      {openxr_data.extent.width, openxr_data.extent.height, 1}};
  vkCmdCopyBufferToImage(vk_command_buffer,
                         m_vk_buffer,
                         swapchain_image.image,
                         VK_IMAGE_LAYOUT_GENERAL,
                         1,
                         &vk_buffer_image_copy);

  /* - End command recording */
  vkEndCommandBuffer(vk_command_buffer);
  /* - Submit command buffer to queue. */
  VkSubmitInfo vk_submit_info = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &vk_command_buffer};
  vkQueueSubmit(m_vk_queue, 1, &vk_submit_info, VK_NULL_HANDLE);

  /* - Wait until device is idle. */
  vkQueueWaitIdle(m_vk_queue);

  /* - Reset command buffer for next eye/frame */
  vkResetCommandBuffer(vk_command_buffer, 0);

  /* Release frame buffer image. */
  m_ghost_ctx->openxr_release_framebuffer_image_callback_(&openxr_data);
}

/* \} */

/* -------------------------------------------------------------------- */
/** \name Data transfer FD
 * \{ */

void GHOST_XrGraphicsBindingVulkan::submitToSwapchainImageFd(
    XrSwapchainImageVulkan2KHR &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  GHOST_VulkanOpenXRData openxr_data = {GHOST_kVulkanXRModeFD};
  m_ghost_ctx->openxr_acquire_framebuffer_image_callback_(&openxr_data);

  /* Create an image handle */
  VkExternalMemoryImageCreateInfo vk_external_memory_image_info = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      nullptr,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};

  VkImageCreateInfo vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                     &vk_external_memory_image_info,
                                     0,
                                     VK_IMAGE_TYPE_2D,
                                     openxr_data.gpu.image_format,
                                     {openxr_data.extent.width, openxr_data.extent.height, 1},
                                     1,
                                     1,
                                     VK_SAMPLE_COUNT_1_BIT,
                                     VK_IMAGE_TILING_OPTIMAL,
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                     VK_SHARING_MODE_EXCLUSIVE,
                                     0,
                                     nullptr,
                                     VK_IMAGE_LAYOUT_UNDEFINED};

  VkImage vk_image;
  vkCreateImage(m_vk_device, &vk_image_info, nullptr, &vk_image);

  /* Import the memory */
  VkMemoryDedicatedAllocateInfo vk_memory_dedicated_allocation_info = {
      VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr, vk_image, VK_NULL_HANDLE};
  VkImportMemoryFdInfoKHR import_memory_info = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                                &vk_memory_dedicated_allocation_info,
                                                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                                                int(openxr_data.gpu.image_handle)};
  VkMemoryAllocateInfo allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &import_memory_info, openxr_data.gpu.memory_size};
  VkDeviceMemory device_memory;
  vkAllocateMemory(m_vk_device, &allocate_info, nullptr, &device_memory);

  /* Bind the imported memory to the image. */
  vkBindImageMemory(m_vk_device, vk_image, device_memory, openxr_data.gpu.memory_offset);

  /* Copy frame buffer image to swapchain image. */
  VkCommandBuffer vk_command_buffer = m_vk_command_buffer;

  /* Begin command recording */
  VkCommandBufferBeginInfo vk_command_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr};
  vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);

  /* Transfer imported render result & swap chain image (UNDEFINED -> GENERAL) */
  VkImageMemoryBarrier vk_image_memory_barrier[] = {{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                     nullptr,
                                                     0,
                                                     VK_ACCESS_TRANSFER_READ_BIT,
                                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                                     VK_IMAGE_LAYOUT_GENERAL,
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     vk_image,
                                                     {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}},
                                                    {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                     nullptr,
                                                     0,
                                                     VK_ACCESS_TRANSFER_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                                     VK_IMAGE_LAYOUT_GENERAL,
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     swapchain_image.image,
                                                     {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}}};
  vkCmdPipelineBarrier(vk_command_buffer,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       2,
                       vk_image_memory_barrier);

  /* Copy image to swapchain */
  VkImageCopy vk_image_copy = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                               {0, 0, 0},
                               {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                               {draw_info.ofsx, draw_info.ofsy, 0},
                               {openxr_data.extent.width, openxr_data.extent.height, 1}};
  vkCmdCopyImage(vk_command_buffer,
                 vk_image,
                 VK_IMAGE_LAYOUT_GENERAL,
                 swapchain_image.image,
                 VK_IMAGE_LAYOUT_GENERAL,
                 1,
                 &vk_image_copy);

  /* End command recording. */
  vkEndCommandBuffer(vk_command_buffer);
  /* Submit command buffer to queue. */
  VkSubmitInfo vk_submit_info = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &vk_command_buffer};
  vkQueueSubmit(m_vk_queue, 1, &vk_submit_info, VK_NULL_HANDLE);

  /* Wait until device is idle. */
  vkQueueWaitIdle(m_vk_queue);

  /* Reset command buffer for next eye/frame. */
  vkResetCommandBuffer(vk_command_buffer, 0);

  vkDestroyImage(m_vk_device, vk_image, nullptr);
  vkFreeMemory(m_vk_device, device_memory, nullptr);
}

/* \} */

bool GHOST_XrGraphicsBindingVulkan::needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const
{
  return ghost_ctx.isUpsideDown();
}
