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

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"ghost.xr"};

#ifdef _WIN32
#  include <vulkan/vulkan_win32.h>
#endif

/* -------------------------------------------------------------------- */
/** \name Constructor
 * \{ */

GHOST_XrGraphicsBindingVulkan::GHOST_XrGraphicsBindingVulkan(GHOST_Context &ghost_ctx)
    : GHOST_IXrGraphicsBinding(), ghost_ctx_(static_cast<GHOST_ContextVK &>(ghost_ctx))
{
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Destroying resources.
 * \{ */

GHOST_XrGraphicsBindingVulkan::~GHOST_XrGraphicsBindingVulkan()
{
  /* Destroy buffer */
  if (vk_buffer_ != VK_NULL_HANDLE) {
    vmaUnmapMemory(vma_allocator_, vk_buffer_allocation_);
    vmaDestroyBuffer(vma_allocator_, vk_buffer_, vk_buffer_allocation_);
    vk_buffer_ = VK_NULL_HANDLE;
    vk_buffer_allocation_ = VK_NULL_HANDLE;
  }

  for (ImportedMemory &imported_memory : imported_memory_) {
    vkDestroyImage(vk_device_, imported_memory.vk_image_xr, nullptr);
    vkFreeMemory(vk_device_, imported_memory.vk_device_memory_xr, nullptr);
  }
  imported_memory_.clear();

  /* Destroy VMA */
  if (vma_allocator_ != VK_NULL_HANDLE) {
    vmaDestroyAllocator(vma_allocator_);
    vma_allocator_ = VK_NULL_HANDLE;
  }

  /* Destroy command buffer */
  if (vk_command_buffer_ != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(vk_device_, vk_command_pool_, 1, &vk_command_buffer_);
    vk_command_buffer_ = VK_NULL_HANDLE;
  }

  /* Destroy command pool */
  if (vk_command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(vk_device_, vk_command_pool_, nullptr);
    vk_command_pool_ = VK_NULL_HANDLE;
  }

  vk_queue_ = VK_NULL_HANDLE;

  /* Destroy device */
  if (vk_device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(vk_device_, nullptr);
    vk_device_ = VK_NULL_HANDLE;
  }

  /* Destroy instance */
  if (vk_instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(vk_instance_, nullptr);
    vk_instance_ = VK_NULL_HANDLE;
  }
}

/** \} */

bool GHOST_XrGraphicsBindingVulkan::loadExtensionFunctions(XrInstance instance)
{
#define LOAD_FUNCTION(fn_ptr, name) \
  XR_SUCCEEDED(xrGetInstanceProcAddr(instance, #name, (PFN_xrVoidFunction *)&fn_ptr))

  extensions_.vulkan_enable = LOAD_FUNCTION(functions_.xrGetVulkanInstanceExtensionsKHR,
                                            xrGetVulkanInstanceExtensionsKHR) &&
                              LOAD_FUNCTION(functions_.xrGetVulkanDeviceExtensionsKHR,
                                            xrGetVulkanDeviceExtensionsKHR) &&
                              LOAD_FUNCTION(functions_.xrGetVulkanGraphicsDeviceKHR,
                                            xrGetVulkanGraphicsDeviceKHR) &&
                              LOAD_FUNCTION(functions_.xrGetVulkanGraphicsRequirementsKHR,
                                            xrGetVulkanGraphicsRequirementsKHR);
  extensions_.vulkan_enable2 =
      LOAD_FUNCTION(functions_.xrGetVulkanGraphicsRequirements2KHR,
                    xrGetVulkanGraphicsRequirements2KHR) &&
      LOAD_FUNCTION(functions_.xrGetVulkanGraphicsDevice2KHR, xrGetVulkanGraphicsDevice2KHR) &&
      LOAD_FUNCTION(functions_.xrCreateVulkanInstanceKHR, xrCreateVulkanInstanceKHR) &&
      LOAD_FUNCTION(functions_.xrCreateVulkanDeviceKHR, xrCreateVulkanDeviceKHR);

#undef LOAD_FUNCTION

  CLOG_INFO(&LOG,
            "XR/Vulkan graphics extensions:\n"
            " - [%c] XR_KHR_vulkan_enable\n"
            " - [%c] XR_KHR_vulkan_enable2",
            extensions_.vulkan_enable ? 'X' : ' ',
            extensions_.vulkan_enable2 ? 'X' : ' ');
  return extensions_.vulkan_enable || extensions_.vulkan_enable2;
}

bool GHOST_XrGraphicsBindingVulkan::checkVersionRequirements(GHOST_Context &ghost_ctx,
                                                             XrInstance instance,
                                                             XrSystemId system_id,
                                                             std::string *r_requirement_info) const
{
  std::ostringstream strstream;

  GHOST_ContextVK &context_vk = static_cast<GHOST_ContextVK &>(ghost_ctx);
  const XrVersion vk_version = XR_MAKE_VERSION(
      context_vk.context_major_version_, context_vk.context_minor_version_, 0);

  if (extensions_.vulkan_enable) {
    XrGraphicsRequirementsVulkanKHR xr_graphics_requirements{
        /*type*/ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
    };

    if (XR_FAILED(functions_.xrGetVulkanGraphicsRequirementsKHR(
            instance, system_id, &xr_graphics_requirements)))
    {
      *r_requirement_info = std::string("Unable to retrieve Xr version requirements for Vulkan");
      return false;
    }

    /* Check if the Vulkan API instance version is supported. */
    if (vk_version < xr_graphics_requirements.minApiVersionSupported) {
      strstream.clear();
      strstream << "Min Vulkan version "
                << XR_VERSION_MAJOR(xr_graphics_requirements.minApiVersionSupported) << "."
                << XR_VERSION_MINOR(xr_graphics_requirements.minApiVersionSupported) << std::endl;
    }
    if (vk_version > xr_graphics_requirements.maxApiVersionSupported) {
      CLOG_INFO(&LOG,
                "OpenXR platform vulkan version requirements do not match with Blender. "
                "This is known to happen when using Occulus/Meta Quest. A workaround for this is "
                "already enabled by enabling extensions that are known to be in core vulkan. "
                "(minimum vulkan version=%d.%d, maximum vulkan version=%d.%d).",
                XR_VERSION_MAJOR(xr_graphics_requirements.minApiVersionSupported),
                XR_VERSION_MINOR(xr_graphics_requirements.minApiVersionSupported),
                XR_VERSION_MAJOR(xr_graphics_requirements.maxApiVersionSupported),
                XR_VERSION_MINOR(xr_graphics_requirements.maxApiVersionSupported));
    }
  }

  if (extensions_.vulkan_enable2) {
    XrGraphicsRequirementsVulkanKHR xr_graphics_requirements2{
        /*type*/ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
    };

    if (XR_FAILED(functions_.xrGetVulkanGraphicsRequirements2KHR(
            instance, system_id, &xr_graphics_requirements2)))
    {
      *r_requirement_info = std::string("Unable to retrieve Xr version requirements for Vulkan");
      return false;
    }

    if (vk_version < xr_graphics_requirements2.minApiVersionSupported) {
      strstream.clear();
      strstream << "Min Vulkan version "
                << XR_VERSION_MAJOR(xr_graphics_requirements2.minApiVersionSupported) << "."
                << XR_VERSION_MINOR(xr_graphics_requirements2.minApiVersionSupported) << std::endl;
    }
    if (vk_version > xr_graphics_requirements2.maxApiVersionSupported) {
      CLOG_INFO(&LOG,
                "OpenXR platform vulkan version requirements do not match with Blender. "
                "This is known to happen when using Occulus/Meta Quest. A workaround for this is "
                "already enabled by enabling extensions that are known to be in core vulkan. "
                "(minimum vulkan version=%d.%d, maximum vulkan version=%d.%d).",
                XR_VERSION_MAJOR(xr_graphics_requirements2.minApiVersionSupported),
                XR_VERSION_MINOR(xr_graphics_requirements2.minApiVersionSupported),
                XR_VERSION_MAJOR(xr_graphics_requirements2.maxApiVersionSupported),
                XR_VERSION_MINOR(xr_graphics_requirements2.maxApiVersionSupported));
    }
  }

  /* When one of the version doesn't match we will error out. We assume when both extensions are
   * supported that both will use the same requirements. */
  if (!strstream.str().empty()) {
    *r_requirement_info = strstream.str();
    return false;
  }

  return true;
}

void GHOST_XrGraphicsBindingVulkan::initFromGhostContext(GHOST_Context &ghost_ctx,
                                                         XrInstance instance,
                                                         XrSystemId system_id)
{
  if (tryReuseVulkanInstance(static_cast<GHOST_ContextVK &>(ghost_ctx), instance, system_id)) {
    return;
  }
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
  CHECK_XR(functions_.xrCreateVulkanInstanceKHR(
               instance, &xr_instance_create_info, &vk_instance_, &vk_result),
           "Unable to create an OpenXR compatible Vulkan instance.");

  /* Physical device selection */
  XrVulkanGraphicsDeviceGetInfoKHR xr_device_get_info = {
      XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR, nullptr, system_id, vk_instance_};
  CHECK_XR(functions_.xrGetVulkanGraphicsDevice2KHR(
               instance, &xr_device_get_info, &vk_physical_device_),
           "Unable to create an OpenXR compatible Vulkan physical device.");

  /* Queue family */
  uint32_t vk_queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device_, &vk_queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> vk_queue_families(vk_queue_family_count);
  graphics_queue_family_ = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(
      vk_physical_device_, &vk_queue_family_count, vk_queue_families.data());
  for (uint32_t i = 0; i < vk_queue_family_count; i++) {
    if (vk_queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
        vk_queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
    {
      graphics_queue_family_ = i;
      break;
    }
  }

  /* Graphic device creation */
  const float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo vk_queue_create_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                  nullptr,
                                                  0,
                                                  graphics_queue_family_,
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
                                                       vk_physical_device_,
                                                       &vk_device_create_info,
                                                       nullptr};
  CHECK_XR(functions_.xrCreateVulkanDeviceKHR(
               instance, &xr_device_create_info, &vk_device_, &vk_result),
           "Unable to create an OpenXR compatible Vulkan logical device.");

  vkGetDeviceQueue(vk_device_, graphics_queue_family_, 0, &vk_queue_);

  /* Command buffer pool */
  VkCommandPoolCreateInfo vk_command_pool_create_info = {
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      nullptr,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      graphics_queue_family_};
  vkCreateCommandPool(vk_device_, &vk_command_pool_create_info, nullptr, &vk_command_pool_);

  /* Command buffer */
  VkCommandBufferAllocateInfo vk_command_buffer_allocate_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      nullptr,
      vk_command_pool_,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      1};
  vkAllocateCommandBuffers(vk_device_, &vk_command_buffer_allocate_info, &vk_command_buffer_);

  /* Select the best data transfer mode based on the OpenXR device and ContextVK. */
  data_transfer_mode_ = choseDataTransferMode();

  if (data_transfer_mode_ == GHOST_kVulkanXRModeCPU) {
    /* VMA */
    VmaAllocatorCreateInfo allocator_create_info = {};
    allocator_create_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_2;
    allocator_create_info.physicalDevice = vk_physical_device_;
    allocator_create_info.device = vk_device_;
    allocator_create_info.instance = vk_instance_;
    vmaCreateAllocator(&allocator_create_info, &vma_allocator_);
  }

  /* Update the binding struct */
  oxr_binding.vk.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
  oxr_binding.vk.next = nullptr;
  oxr_binding.vk.instance = vk_instance_;
  oxr_binding.vk.physicalDevice = vk_physical_device_;
  oxr_binding.vk.device = vk_device_;
  oxr_binding.vk.queueFamilyIndex = graphics_queue_family_;
  oxr_binding.vk.queueIndex = 0;
}

bool GHOST_XrGraphicsBindingVulkan::tryReuseVulkanInstance(GHOST_ContextVK &ghost_ctx,
                                                           XrInstance instance,
                                                           XrSystemId system_id)
{
  if (!extensions_.vulkan_enable) {
    CLOG_INFO(&LOG, "Unable to reuse vulkan instance: XR_KHR_vulkan_enable isn't supported");
    return false;
  }

  GHOST_VulkanHandles context_handles;
  if (ghost_ctx.getVulkanHandles(context_handles) == GHOST_kFailure) {
    return false;
  }

  bool result = true;

  /* Perform all checks. When stacking the calls with `&&` only the first
   * failing message will be reported. */
  result &= areRequiredInstanceExtensionsEnabled(instance, system_id);
  result &= areRequiredDeviceExtensionsEnabled(instance, system_id);
  result &= isSamePhysicalDeviceSelected(instance, system_id, context_handles);

  if (!result) {
    return result;
  }

  CLOG_INFO(&LOG, "Reusing vulkan instance.");
  data_transfer_mode_ = GHOST_kVulkanXRModeRenderGraph;

  /* Initialize binding struct */
  oxr_binding.vk.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
  oxr_binding.vk.next = nullptr;
  oxr_binding.vk.instance = context_handles.instance;
  oxr_binding.vk.physicalDevice = context_handles.physical_device;
  oxr_binding.vk.device = context_handles.device;
  oxr_binding.vk.queueFamilyIndex = context_handles.graphic_queue_family;
  oxr_binding.vk.queueIndex = 0;
  return true;
}

static blender::Vector<std::string> split_extension_names(blender::StringRef extension_names)
{
  blender::Vector<std::string> result;
  std::stringstream ss(extension_names);
  std::string extension_name;

  while (std::getline(ss, extension_name, ' ')) {
    if (!extension_name.empty()) {
      result.append(extension_name);
    }
  }

  return result;
}

bool GHOST_XrGraphicsBindingVulkan::areRequiredInstanceExtensionsEnabled(
    XrInstance instance, XrSystemId system_id) const
{
  uint32_t buffer_count = 0;
  functions_.xrGetVulkanInstanceExtensionsKHR(instance, system_id, 0, &buffer_count, nullptr);
  std::string buffer(buffer_count, '\0');
  functions_.xrGetVulkanInstanceExtensionsKHR(
      instance, system_id, uint32_t(buffer.size()), &buffer_count, buffer.data());
  blender::Vector<std::string> instance_extension_names = split_extension_names(buffer);
  bool all_extensions_enabled = true;
  std::stringstream log_ss;
  log_ss << "Required vulkan instance extensions:";
  for (const std::string &extension_name : instance_extension_names) {
    bool is_extension_enabled = GHOST_ContextVK::is_instance_extension_enabled(
        extension_name.c_str());
    log_ss << "\n - [" << (is_extension_enabled ? 'X' : ' ') << "] " << extension_name;
    all_extensions_enabled &= is_extension_enabled;
  }
  CLOG_DEBUG(&LOG, "%s", log_ss.str().c_str());
  if (!all_extensions_enabled) {
    CLOG_INFO(&LOG,
              "Unable to reuse vulkan instance: not all required instance extensions are enabled");
    return false;
  }

  return true;
}

bool GHOST_XrGraphicsBindingVulkan::areRequiredDeviceExtensionsEnabled(XrInstance instance,
                                                                       XrSystemId system_id) const
{
  uint32_t buffer_count = 0;
  functions_.xrGetVulkanDeviceExtensionsKHR(instance, system_id, 0, &buffer_count, nullptr);
  std::string buffer(buffer_count, '\0');
  functions_.xrGetVulkanDeviceExtensionsKHR(
      instance, system_id, uint32_t(buffer.size()), &buffer_count, buffer.data());
  blender::Vector<std::string> instance_extension_names = split_extension_names(buffer);
  bool all_extensions_enabled = true;
  std::stringstream log_ss;
  log_ss << "Required vulkan device extensions:";
  for (const std::string &extension_name : instance_extension_names) {
    bool is_extension_enabled = GHOST_ContextVK::is_device_extension_enabled(
        extension_name.c_str());
    log_ss << "\n - [" << (is_extension_enabled ? 'X' : ' ') << "] " << extension_name;
    all_extensions_enabled &= is_extension_enabled;
  }
  CLOG_DEBUG(&LOG, "%s", log_ss.str().c_str());
  if (!all_extensions_enabled) {
    CLOG_INFO(&LOG,
              "Unable to reuse vulkan instance: not all required device extensions are enabled");
    return false;
  }

  return true;
}

bool GHOST_XrGraphicsBindingVulkan::isSamePhysicalDeviceSelected(
    XrInstance instance, XrSystemId system_id, const GHOST_VulkanHandles &context_handles) const
{
  VkPhysicalDevice openxr_physical_device = VK_NULL_HANDLE;
  functions_.xrGetVulkanGraphicsDeviceKHR(
      instance, system_id, context_handles.instance, &openxr_physical_device);
  if (context_handles.physical_device != openxr_physical_device) {
    VkPhysicalDeviceProperties openxr_physical_device_properties = {};
    vkGetPhysicalDeviceProperties(openxr_physical_device, &openxr_physical_device_properties);
    VkPhysicalDeviceProperties context_physical_device_properties = {};
    vkGetPhysicalDeviceProperties(context_handles.physical_device,
                                  &context_physical_device_properties);
    CLOG_INFO(&LOG,
              "Unable to reuse vulkan instance: OpenXR requires to use a different GPU [%s] than "
              "currently in used [%s].",
              openxr_physical_device_properties.deviceName,
              context_physical_device_properties.deviceName);
    return false;
  }
  return true;
}

GHOST_TVulkanXRModes GHOST_XrGraphicsBindingVulkan::choseDataTransferMode()
{
  GHOST_VulkanHandles vulkan_handles;
  ghost_ctx_.getVulkanHandles(vulkan_handles);

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
  vkGetPhysicalDeviceProperties2(vk_physical_device_, &xr_physical_device_properties);

  /* When the physical device properties match between the Vulkan device and the Xr devices we
   * assume that they are the same physical device in the machine and we can use shared memory.
   * If not we fall back to CPU based data transfer. */
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
  bool has_vk_khr_external_memory_win32_extension = has_extension(
      VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
  if (has_vk_khr_external_memory_win32_extension) {
    return GHOST_kVulkanXRModeWin32;
  }
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

    /* When using render graph, the render graph commands will ensure that the drawing is done in
     * scene reference space and blits to the swapchain with sRGB conversion. No need to render
     * into an sRGB framebuffer. */
    if (data_transfer_mode_ != GHOST_kVulkanXRModeRenderGraph) {
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
  image_cache_.push_back(std::move(vulkan_images));

  return base_images;
}

void GHOST_XrGraphicsBindingVulkan::submitToSwapchainBegin() {}
void GHOST_XrGraphicsBindingVulkan::submitToSwapchainImage(
    XrSwapchainImageBaseHeader &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  XrSwapchainImageVulkan2KHR &vulkan_image = *reinterpret_cast<XrSwapchainImageVulkan2KHR *>(
      &swapchain_image);

  switch (data_transfer_mode_) {
    case GHOST_kVulkanXRModeFD:
    case GHOST_kVulkanXRModeWin32:
      submitToSwapchainImageGpu(vulkan_image, draw_info);
      break;

    case GHOST_kVulkanXRModeCPU:
      submitToSwapchainImageCpu(vulkan_image, draw_info);
      break;

    case GHOST_kVulkanXRModeRenderGraph:
      submitToSwapchainImageRenderGraph(vulkan_image, draw_info);
      break;
  }
}
void GHOST_XrGraphicsBindingVulkan::submitToSwapchainEnd() {}

/* -------------------------------------------------------------------- */
/** \name Data transfer CPU
 * \{ */

void GHOST_XrGraphicsBindingVulkan::submitToSwapchainImageCpu(
    XrSwapchainImageVulkan2KHR &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  /* Acquire frame buffer image. */
  GHOST_VulkanOpenXRData openxr_data = {GHOST_kVulkanXRModeCPU};
  ghost_ctx_.openxr_acquire_framebuffer_image_callback_(&openxr_data);

  /* Import render result. */
  VkDeviceSize component_size = 4 * sizeof(uint8_t);
  if (draw_info.swapchain_format == GHOST_kXrSwapchainFormatRGBA16F ||
      draw_info.swapchain_format == GHOST_kXrSwapchainFormatRGBA16)
  {
    component_size = 4 * sizeof(uint16_t);
  }
  VkDeviceSize image_data_size = openxr_data.extent.width * openxr_data.extent.height *
                                 component_size;

  if (vk_buffer_ != VK_NULL_HANDLE && vk_buffer_allocation_info_.size < image_data_size) {
    vmaUnmapMemory(vma_allocator_, vk_buffer_allocation_);
    vmaDestroyBuffer(vma_allocator_, vk_buffer_, vk_buffer_allocation_);
    vk_buffer_ = VK_NULL_HANDLE;
    vk_buffer_allocation_ = VK_NULL_HANDLE;
  }

  if (vk_buffer_ == VK_NULL_HANDLE) {
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
    vmaCreateBuffer(vma_allocator_,
                    &vk_buffer_create_info,
                    &allocation_create_info,
                    &vk_buffer_,
                    &vk_buffer_allocation_,
                    &vk_buffer_allocation_info_);
    vmaMapMemory(vma_allocator_, vk_buffer_allocation_, &vk_buffer_allocation_info_.pMappedData);
  }
  std::memcpy(vk_buffer_allocation_info_.pMappedData, openxr_data.cpu.image_data, image_data_size);

  /* Copy frame buffer image to swapchain image. */
  VkCommandBuffer vk_command_buffer = vk_command_buffer_;

  /* - Begin command recording */
  VkCommandBufferBeginInfo vk_command_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr};
  vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);

  /* Transfer imported render result & swap-chain image (UNDEFINED -> GENERAL). */
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
                         vk_buffer_,
                         swapchain_image.image,
                         VK_IMAGE_LAYOUT_GENERAL,
                         1,
                         &vk_buffer_image_copy);

  /* - End command recording */
  vkEndCommandBuffer(vk_command_buffer);
  /* - Submit command buffer to queue. */
  VkSubmitInfo vk_submit_info = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &vk_command_buffer};
  vkQueueSubmit(vk_queue_, 1, &vk_submit_info, VK_NULL_HANDLE);

  /* - Wait until device is idle. */
  vkQueueWaitIdle(vk_queue_);

  /* - Reset command buffer for next eye/frame */
  vkResetCommandBuffer(vk_command_buffer, 0);

  /* Release frame buffer image. */
  ghost_ctx_.openxr_release_framebuffer_image_callback_(&openxr_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data transfer render graph
 * \{ */

void GHOST_XrGraphicsBindingVulkan::submitToSwapchainImageRenderGraph(
    XrSwapchainImageVulkan2KHR &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  GHOST_VulkanSwapChainData swap_chain_data = {};
  swap_chain_data.image = swapchain_image.image;
  swap_chain_data.extent = {uint32_t(draw_info.width), uint32_t(draw_info.height)};
  swap_chain_data.surface_format.format = VkFormat(draw_info.gpu_swapchain_format);
  swap_chain_data.surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

  ghost_ctx_.swap_buffer_draw_callback_(&swap_chain_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data transfer GPU
 * \{ */

void GHOST_XrGraphicsBindingVulkan::submitToSwapchainImageGpu(
    XrSwapchainImageVulkan2KHR &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  /* Check for previous imported memory. */
  ImportedMemory *imported_memory = nullptr;
  for (ImportedMemory &item : imported_memory_) {
    if (item.view_idx == draw_info.view_idx) {
      imported_memory = &item;
    }
  }
  /* No previous imported memory found, creating a new. */
  if (imported_memory == nullptr) {
    imported_memory_.push_back(
        {draw_info.view_idx, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE});
    imported_memory = &imported_memory_.back();
  }

  GHOST_VulkanOpenXRData openxr_data = {data_transfer_mode_};
  openxr_data.gpu.vk_image_blender = imported_memory->vk_image_blender;
  ghost_ctx_.openxr_acquire_framebuffer_image_callback_(&openxr_data);
  imported_memory->vk_image_blender = openxr_data.gpu.vk_image_blender;

  /* Create an image handle */
  if (openxr_data.gpu.new_handle) {
    if (imported_memory->vk_image_xr) {
      vkDestroyImage(vk_device_, imported_memory->vk_image_xr, nullptr);
      vkFreeMemory(vk_device_, imported_memory->vk_device_memory_xr, nullptr);
      imported_memory->vk_device_memory_xr = VK_NULL_HANDLE;
      imported_memory->vk_image_xr = VK_NULL_HANDLE;
    }

    VkExternalMemoryImageCreateInfo vk_external_memory_image_info = {
        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, nullptr, 0};

    switch (data_transfer_mode_) {
      case GHOST_kVulkanXRModeFD:
        vk_external_memory_image_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        break;
      case GHOST_kVulkanXRModeWin32:
        vk_external_memory_image_info.handleTypes =
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        break;
      case GHOST_kVulkanXRModeCPU:
      case GHOST_kVulkanXRModeRenderGraph:
        break;
    }

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

    vkCreateImage(vk_device_, &vk_image_info, nullptr, &imported_memory->vk_image_xr);

    /* Get the memory requirements */
    VkMemoryRequirements vk_memory_requirements = {};
    vkGetImageMemoryRequirements(
        vk_device_, imported_memory->vk_image_xr, &vk_memory_requirements);

    /* Import the memory */
    VkMemoryDedicatedAllocateInfo vk_memory_dedicated_allocation_info = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        nullptr,
        imported_memory->vk_image_xr,
        VK_NULL_HANDLE};
    switch (data_transfer_mode_) {
      case GHOST_kVulkanXRModeFD: {
        VkImportMemoryFdInfoKHR import_memory_info = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                                      &vk_memory_dedicated_allocation_info,
                                                      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                                                      int(openxr_data.gpu.image_handle)};
        VkMemoryAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                              &import_memory_info,
                                              vk_memory_requirements.size};
        vkAllocateMemory(
            vk_device_, &allocate_info, nullptr, &imported_memory->vk_device_memory_xr);
        break;
      }

      case GHOST_kVulkanXRModeWin32: {
#ifdef _WIN32
        VkImportMemoryWin32HandleInfoKHR import_memory_info = {
            VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
            &vk_memory_dedicated_allocation_info,
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            HANDLE(openxr_data.gpu.image_handle)};
        VkMemoryAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                              &import_memory_info,
                                              vk_memory_requirements.size};
        vkAllocateMemory(
            vk_device_, &allocate_info, nullptr, &imported_memory->vk_device_memory_xr);
#endif
        break;
      }

      case GHOST_kVulkanXRModeCPU:
      case GHOST_kVulkanXRModeRenderGraph:
        break;
    }

    /* Bind the imported memory to the image. */
    vkBindImageMemory(vk_device_,
                      imported_memory->vk_image_xr,
                      imported_memory->vk_device_memory_xr,
                      openxr_data.gpu.memory_offset);
  }

  /* Copy frame buffer image to swapchain image. */
  VkCommandBuffer vk_command_buffer = vk_command_buffer_;

  /* Begin command recording */
  VkCommandBufferBeginInfo vk_command_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr};
  vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);

  /* Transfer imported render result & swap-chain image (UNDEFINED -> GENERAL). */
  VkImageMemoryBarrier vk_image_memory_barrier[] = {{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                     nullptr,
                                                     0,
                                                     VK_ACCESS_TRANSFER_READ_BIT,
                                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                                     VK_IMAGE_LAYOUT_GENERAL,
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     imported_memory->vk_image_xr,
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
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       2,
                       vk_image_memory_barrier);

  /* Copy image to swap-chain. */
  VkImageCopy vk_image_copy = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                               {0, 0, 0},
                               {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                               {draw_info.ofsx, draw_info.ofsy, 0},
                               {openxr_data.extent.width, openxr_data.extent.height, 1}};
  vkCmdCopyImage(vk_command_buffer,
                 imported_memory->vk_image_xr,
                 VK_IMAGE_LAYOUT_GENERAL,
                 swapchain_image.image,
                 VK_IMAGE_LAYOUT_GENERAL,
                 1,
                 &vk_image_copy);

  /* Swap-chain needs to be in an VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL compatible layout. */
  VkImageMemoryBarrier vk_image_memory_barrier2 = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                   nullptr,
                                                   VK_ACCESS_TRANSFER_WRITE_BIT,
                                                   0,
                                                   VK_IMAGE_LAYOUT_GENERAL,
                                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                   VK_QUEUE_FAMILY_IGNORED,
                                                   VK_QUEUE_FAMILY_IGNORED,
                                                   swapchain_image.image,
                                                   {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(vk_command_buffer,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &vk_image_memory_barrier2);

  /* End command recording. */
  vkEndCommandBuffer(vk_command_buffer);
  /* Submit command buffer to queue. */
  VkSubmitInfo vk_submit_info = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &vk_command_buffer};
  vkQueueSubmit(vk_queue_, 1, &vk_submit_info, VK_NULL_HANDLE);

  /* Wait until device is idle. */
  vkQueueWaitIdle(vk_queue_);

  /* Reset command buffer for next eye/frame. */
  vkResetCommandBuffer(vk_command_buffer, 0);
}

/** \} */

bool GHOST_XrGraphicsBindingVulkan::needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const
{
  if (data_transfer_mode_ == GHOST_kVulkanXRModeRenderGraph) {
    return !ghost_ctx.isUpsideDown();
  }
  return ghost_ctx.isUpsideDown();
}
