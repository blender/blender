/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_ContextVK.hh"

#ifdef _WIN32
#  include <vulkan/vulkan_win32.h>
#else /* X11/WAYLAND. */
#  ifdef WITH_GHOST_X11
#    include <vulkan/vulkan_xlib.h>
#  endif
#  ifdef WITH_GHOST_WAYLAND
#    include <vulkan/vulkan_wayland.h>
#  endif
#endif

#include "vulkan/vk_ghost_api.hh"

#if !defined(_WIN32) or defined(_M_ARM64)
/* Silence compilation warning on non-windows x64 systems. */
#  define VMA_EXTERNAL_MEMORY_WIN32 0
#endif
#include "vk_mem_alloc.h"

#include "CLG_log.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <vector>

#include <sys/stat.h>

using namespace std;

static CLG_LogRef LOG = {"ghost.context"};

static const char *vulkan_error_as_string(VkResult result)
{
#define FORMAT_ERROR(X) \
  case X: { \
    return "" #X; \
  }

  switch (result) {
    FORMAT_ERROR(VK_NOT_READY);
    FORMAT_ERROR(VK_TIMEOUT);
    FORMAT_ERROR(VK_EVENT_SET);
    FORMAT_ERROR(VK_EVENT_RESET);
    FORMAT_ERROR(VK_INCOMPLETE);
    FORMAT_ERROR(VK_ERROR_OUT_OF_HOST_MEMORY);
    FORMAT_ERROR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    FORMAT_ERROR(VK_ERROR_INITIALIZATION_FAILED);
    FORMAT_ERROR(VK_ERROR_DEVICE_LOST);
    FORMAT_ERROR(VK_ERROR_MEMORY_MAP_FAILED);
    FORMAT_ERROR(VK_ERROR_LAYER_NOT_PRESENT);
    FORMAT_ERROR(VK_ERROR_EXTENSION_NOT_PRESENT);
    FORMAT_ERROR(VK_ERROR_FEATURE_NOT_PRESENT);
    FORMAT_ERROR(VK_ERROR_INCOMPATIBLE_DRIVER);
    FORMAT_ERROR(VK_ERROR_TOO_MANY_OBJECTS);
    FORMAT_ERROR(VK_ERROR_FORMAT_NOT_SUPPORTED);
    FORMAT_ERROR(VK_ERROR_FRAGMENTED_POOL);
    FORMAT_ERROR(VK_ERROR_UNKNOWN);
    FORMAT_ERROR(VK_ERROR_OUT_OF_POOL_MEMORY);
    FORMAT_ERROR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
    FORMAT_ERROR(VK_ERROR_FRAGMENTATION);
    FORMAT_ERROR(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    FORMAT_ERROR(VK_ERROR_SURFACE_LOST_KHR);
    FORMAT_ERROR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    FORMAT_ERROR(VK_SUBOPTIMAL_KHR);
    FORMAT_ERROR(VK_ERROR_OUT_OF_DATE_KHR);
    FORMAT_ERROR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    FORMAT_ERROR(VK_ERROR_VALIDATION_FAILED_EXT);
    FORMAT_ERROR(VK_ERROR_INVALID_SHADER_NV);
    FORMAT_ERROR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    FORMAT_ERROR(VK_ERROR_NOT_PERMITTED_EXT);
    FORMAT_ERROR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    FORMAT_ERROR(VK_THREAD_IDLE_KHR);
    FORMAT_ERROR(VK_THREAD_DONE_KHR);
    FORMAT_ERROR(VK_OPERATION_DEFERRED_KHR);
    FORMAT_ERROR(VK_OPERATION_NOT_DEFERRED_KHR);
    FORMAT_ERROR(VK_PIPELINE_COMPILE_REQUIRED_EXT);
    default:
      return "Unknown Error";
  }
}

#define __STR(A) "" #A
#define VK_CHECK(__expression, fail_value) \
  do { \
    VkResult r = (__expression); \
    if (r != VK_SUCCESS) { \
      CLOG_ERROR(&LOG, \
                 "Vulkan: %s resulted in code %s.", \
                 __STR(__expression), \
                 vulkan_error_as_string(r)); \
      return fail_value; \
    } \
  } while (0)

/* -------------------------------------------------------------------- */
/** \name Swap-chain resources
 * \{ */

void GHOST_SwapchainImage::destroy(VkDevice vk_device)
{
  vkDestroySemaphore(vk_device, present_semaphore, nullptr);
  present_semaphore = VK_NULL_HANDLE;
  vk_image = VK_NULL_HANDLE;
}

void GHOST_FrameDiscard::destroy(VkDevice vk_device)
{
  while (!swapchains.empty()) {
    VkSwapchainKHR vk_swapchain = swapchains.back();
    swapchains.pop_back();
    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
  }
  while (!semaphores.empty()) {
    VkSemaphore vk_semaphore = semaphores.back();
    semaphores.pop_back();
    vkDestroySemaphore(vk_device, vk_semaphore, nullptr);
  }
}

void GHOST_Frame::destroy(VkDevice vk_device)
{
  vkDestroyFence(vk_device, submission_fence, nullptr);
  submission_fence = VK_NULL_HANDLE;
  vkDestroySemaphore(vk_device, acquire_semaphore, nullptr);
  acquire_semaphore = VK_NULL_HANDLE;
  discard_pile.destroy(vk_device);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extension list
 * \{ */

struct GHOST_ExtensionsVK {
  vector<VkExtensionProperties> extensions;
  vector<const char *> enabled;

  bool is_supported(const char *extension_name) const
  {
    for (const VkExtensionProperties &extension : extensions) {
      if (strcmp(extension.extensionName, extension_name) == 0) {
        return true;
      }
    }
    return false;
  }

  bool is_supported(const vector<const char *> &extension_names)
  {
    for (const char *extension_name : extension_names) {
      if (!is_supported(extension_name)) {
        return false;
      }
    }
    return true;
  }

  bool enable(const char *extension_name, bool optional = false)
  {
    bool supported = is_supported(extension_name);
    if (supported) {
      CLOG_TRACE(&LOG,
                 "Vulkan: %s extension enabled: name=%s",
                 optional ? "optional" : "required",
                 extension_name);
      enabled.push_back(extension_name);
      return true;
    }

    CLOG_AT_LEVEL(&LOG,
                  (optional ? CLG_LEVEL_TRACE : CLG_LEVEL_ERROR),
                  "Vulkan: %s extension not available: name=%s",
                  optional ? "optional" : "required",
                  extension_name);

    return false;
  }

  bool enable(const vector<const char *> &extension_names, bool optional = false)
  {
    bool failure = false;
    for (const char *extension_name : extension_names) {
      failure |= !enable(extension_name, optional);
    }
    return !failure;
  }

  bool is_enabled(const char *extension_name) const
  {
    for (const char *enabled_extension_name : enabled) {
      if (strcmp(enabled_extension_name, extension_name) == 0) {
        return true;
      }
    }
    return false;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vulkan Device
 * \{ */

class GHOST_DeviceVK {
 public:
  VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
  GHOST_ExtensionsVK extensions;

  VkDevice vk_device = VK_NULL_HANDLE;

  uint32_t generic_queue_family = 0;
  VkQueue generic_queue = VK_NULL_HANDLE;
  VmaAllocator vma_allocator = VK_NULL_HANDLE;

  VkPhysicalDeviceProperties2 properties = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
  };
  VkPhysicalDeviceVulkan12Properties properties_12 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
  };
  VkPhysicalDeviceFeatures2 features = {};
  VkPhysicalDeviceVulkan11Features features_11 = {};
  VkPhysicalDeviceVulkan12Features features_12 = {};
  VkPhysicalDeviceRobustness2FeaturesEXT features_robustness2 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};

  int users = 0;

  /** Mutex to externally synchronize access to queue. */
  std::mutex queue_mutex;

  bool use_vk_ext_swapchain_maintenance_1 = false;
  bool use_vk_ext_swapchain_colorspace = false;

 public:
  GHOST_DeviceVK(VkPhysicalDevice vk_physical_device, const bool use_vk_ext_swapchain_colorspace)
      : vk_physical_device(vk_physical_device),
        use_vk_ext_swapchain_colorspace(use_vk_ext_swapchain_colorspace)
  {
    properties.pNext = &properties_12;
    vkGetPhysicalDeviceProperties2(vk_physical_device, &properties);

    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features.pNext = &features_11;
    features_11.pNext = &features_12;
    features_12.pNext = &features_robustness2;

    vkGetPhysicalDeviceFeatures2(vk_physical_device, &features);
    init_extensions();
  }

  ~GHOST_DeviceVK()
  {
    if (vma_allocator != VK_NULL_HANDLE) {
      vmaDestroyAllocator(vma_allocator);
      vma_allocator = VK_NULL_HANDLE;
    }
    if (vk_device != VK_NULL_HANDLE) {
      vkDestroyDevice(vk_device, nullptr);
      vk_device = VK_NULL_HANDLE;
    }
  }

  bool init_extensions()
  {
    uint32_t extensions_count;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
                 vk_physical_device, nullptr, &extensions_count, nullptr),
             false);
    extensions.extensions.resize(extensions_count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
                 vk_physical_device, nullptr, &extensions_count, extensions.extensions.data()),
             false);
    return true;
  }

  void wait_idle()
  {
    if (vk_device) {
      std::scoped_lock lock(queue_mutex);
      vkDeviceWaitIdle(vk_device);
    }
  }

  void init_generic_queue_family()
  {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, nullptr);

    vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        vk_physical_device, &queue_family_count, queue_families.data());

    generic_queue_family = 0;
    for (const auto &queue_family : queue_families) {
      /* Every VULKAN implementation by spec must have one queue family that support both graphics
       * and compute pipelines. We select this one; compute only queue family hints at asynchronous
       * compute implementations. */
      if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
          (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT))
      {
        return;
      }
      generic_queue_family++;
    }
  }

  void init_generic_queue()
  {
    vkGetDeviceQueue(vk_device, generic_queue_family, 0, &generic_queue);
  }

  void init_memory_allocator(VkInstance vk_instance)
  {
    VmaAllocatorCreateInfo vma_allocator_create_info = {};
    vma_allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_2;
    vma_allocator_create_info.physicalDevice = vk_physical_device;
    vma_allocator_create_info.device = vk_device;
    vma_allocator_create_info.instance = vk_instance;
    vma_allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    if (extensions.is_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
      vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    }
    if (extensions.is_enabled(VK_KHR_MAINTENANCE_4_EXTENSION_NAME)) {
      vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
    }
    vmaCreateAllocator(&vma_allocator_create_info, &vma_allocator);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vulkan Instance
 * \{ */

struct GHOST_InstanceVK {
  VkInstance vk_instance = VK_NULL_HANDLE;
  VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;

  GHOST_ExtensionsVK extensions;

  std::optional<GHOST_DeviceVK> device;

  GHOST_InstanceVK()
  {
    init_extensions();
  }

  ~GHOST_InstanceVK()
  {
    device.reset();
    vkDestroyInstance(vk_instance, nullptr);
    vk_physical_device = VK_NULL_HANDLE;
    vk_instance = VK_NULL_HANDLE;
  }

  bool init_extensions()
  {
    uint32_t extension_count = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr), false);
    extensions.extensions.resize(extension_count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(
                 nullptr, &extension_count, extensions.extensions.data()),
             false);
    return true;
  }

  bool create_instance(uint32_t vulkan_api_version)
  {
    VkApplicationInfo vk_application_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                             nullptr,
                                             "Blender",
                                             VK_MAKE_VERSION(1, 0, 0),
                                             "Blender",
                                             VK_MAKE_VERSION(1, 0, 0),
                                             vulkan_api_version};
    VkInstanceCreateInfo vk_instance_create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                    nullptr,
                                                    0,
                                                    &vk_application_info,
                                                    0,
                                                    nullptr,
                                                    uint32_t(extensions.enabled.size()),
                                                    extensions.enabled.data()

    };

    VK_CHECK(vkCreateInstance(&vk_instance_create_info, nullptr, &vk_instance), false);
    return true;
  }

  bool select_physical_device(const GHOST_GPUDevice &preferred_device,
                              const vector<const char *> &required_extensions)
  {
    VkPhysicalDevice best_physical_device = VK_NULL_HANDLE;

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr);

    vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(vk_instance, &device_count, physical_devices.data());

    int best_device_score = -1;
    int device_index = -1;
    for (const auto &physical_device : physical_devices) {
      GHOST_DeviceVK device_vk(physical_device, false);
      device_index++;

      if (!device_vk.extensions.is_supported(required_extensions)) {
        continue;
      }
      if (!blender::gpu::GPU_vulkan_is_supported_driver(physical_device)) {
        continue;
      }

      if (!device_vk.features.features.geometryShader ||
          !device_vk.features.features.vertexPipelineStoresAndAtomics ||
          !device_vk.features.features.dualSrcBlend || !device_vk.features.features.logicOp ||
          !device_vk.features.features.imageCubeArray)
      {
        continue;
      }

      int device_score = 0;
      switch (device_vk.properties.properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
          device_score = 400;
          break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
          device_score = 300;
          break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
          device_score = 200;
          break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
          device_score = 100;
          break;
        default:
          break;
      }

      /* User has configured a preferred device. Add bonus score when vendor and device match.
       * Driver id isn't considered as drivers update more frequently and can break the device
       * selection. */
      if (device_vk.properties.properties.deviceID == preferred_device.device_id &&
          device_vk.properties.properties.vendorID == preferred_device.vendor_id)
      {
        device_score += 500;
        if (preferred_device.index == device_index) {
          device_score += 10;
        }
      }
      if (device_score > best_device_score) {
        best_physical_device = physical_device;
        best_device_score = device_score;
      }
    }

    if (best_physical_device == VK_NULL_HANDLE) {
      CLOG_ERROR(&LOG, "No suitable Vulkan Device found!");
      return GHOST_kFailure;
    }

    vk_physical_device = best_physical_device;

    return GHOST_kSuccess;
  }

  bool create_device(const bool use_vk_ext_swapchain_maintenance1,
                     vector<const char *> &required_device_extensions,
                     vector<const char *> &optional_device_extensions)
  {
    device.emplace(vk_physical_device, use_vk_ext_swapchain_maintenance1);
    GHOST_DeviceVK &device = *this->device;

    device.extensions.enable(required_device_extensions);
    device.extensions.enable(optional_device_extensions, true);
    device.init_generic_queue_family();

    float queue_priorities[] = {1.0f};
    vector<VkDeviceQueueCreateInfo> queue_create_infos;
    VkDeviceQueueCreateInfo graphic_queue_create_info = {};
    graphic_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphic_queue_create_info.queueFamilyIndex = device.generic_queue_family;
    graphic_queue_create_info.queueCount = 1;
    graphic_queue_create_info.pQueuePriorities = queue_priorities;
    queue_create_infos.push_back(graphic_queue_create_info);

    VkPhysicalDeviceFeatures device_features = {};
    device_features.geometryShader = VK_TRUE;
    device_features.logicOp = VK_TRUE;
    device_features.dualSrcBlend = VK_TRUE;
    device_features.imageCubeArray = VK_TRUE;
    device_features.multiDrawIndirect = VK_TRUE;
    device_features.multiViewport = VK_TRUE;
    device_features.shaderClipDistance = VK_TRUE;
    device_features.drawIndirectFirstInstance = VK_TRUE;
    device_features.vertexPipelineStoresAndAtomics = VK_TRUE;
    device_features.fragmentStoresAndAtomics = VK_TRUE;
    device_features.samplerAnisotropy = device.features.features.samplerAnisotropy;
    device_features.wideLines = device.features.features.wideLines;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = uint32_t(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.enabledExtensionCount = uint32_t(device.extensions.enabled.size());
    device_create_info.ppEnabledExtensionNames = device.extensions.enabled.data();
    device_create_info.pEnabledFeatures = &device_features;

    std::vector<void *> feature_struct_ptr;

    /* Enable vulkan 11 features when supported on physical device. */
    VkPhysicalDeviceVulkan11Features vulkan_11_features = {};
    vulkan_11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan_11_features.shaderDrawParameters = VK_TRUE;
    feature_struct_ptr.push_back(&vulkan_11_features);

    /* Enable optional vulkan 12 features when supported on physical device. */
    VkPhysicalDeviceVulkan12Features vulkan_12_features = {};
    vulkan_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan_12_features.shaderOutputLayer = device.features_12.shaderOutputLayer;
    vulkan_12_features.shaderOutputViewportIndex = device.features_12.shaderOutputViewportIndex;
    vulkan_12_features.bufferDeviceAddress = device.features_12.bufferDeviceAddress;
    vulkan_12_features.timelineSemaphore = VK_TRUE;
    feature_struct_ptr.push_back(&vulkan_12_features);

    /* Enable provoking vertex. */
    VkPhysicalDeviceProvokingVertexFeaturesEXT provoking_vertex_features = {};
    provoking_vertex_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT;
    provoking_vertex_features.provokingVertexLast = VK_TRUE;
    feature_struct_ptr.push_back(&provoking_vertex_features);

    /* Enable dynamic rendering. */
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering = {};
    dynamic_rendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering.dynamicRendering = VK_TRUE;
    feature_struct_ptr.push_back(&dynamic_rendering);

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT
        dynamic_rendering_unused_attachments = {};
    dynamic_rendering_unused_attachments.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    dynamic_rendering_unused_attachments.dynamicRenderingUnusedAttachments = VK_TRUE;
    if (device.extensions.is_enabled(VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME)) {
      feature_struct_ptr.push_back(&dynamic_rendering_unused_attachments);
    }

    VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR dynamic_rendering_local_read = {};
    dynamic_rendering_local_read.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR;
    dynamic_rendering_local_read.dynamicRenderingLocalRead = VK_TRUE;
    if (device.extensions.is_enabled(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME)) {
      feature_struct_ptr.push_back(&dynamic_rendering_local_read);
    }

    /* VK_EXT_robustness2 */
    VkPhysicalDeviceRobustness2FeaturesEXT robustness_2_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
    if (device.extensions.is_enabled(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
      robustness_2_features.nullDescriptor = device.features_robustness2.nullDescriptor;
      feature_struct_ptr.push_back(&robustness_2_features);
    }

    /* Query for Mainenance4 (core in Vulkan 1.3). */
    VkPhysicalDeviceMaintenance4FeaturesKHR maintenance_4 = {};
    maintenance_4.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR;
    maintenance_4.maintenance4 = VK_TRUE;
    if (device.extensions.is_enabled(VK_KHR_MAINTENANCE_4_EXTENSION_NAME)) {
      feature_struct_ptr.push_back(&maintenance_4);
    }

    /* Swap-chain maintenance 1 is optional. */
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance_1 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT, nullptr, VK_TRUE};
    if (device.extensions.is_enabled(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
      feature_struct_ptr.push_back(&swapchain_maintenance_1);
      device.use_vk_ext_swapchain_maintenance_1 = true;
    }

    /* Query and enable Fragment Shader Barycentrics. */
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR fragment_shader_barycentric = {};
    fragment_shader_barycentric.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR;
    fragment_shader_barycentric.fragmentShaderBarycentric = VK_TRUE;
    if (device.extensions.is_enabled(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)) {
      feature_struct_ptr.push_back(&fragment_shader_barycentric);
    }

    /* VK_EXT_memory_priority */
    VkPhysicalDeviceMemoryPriorityFeaturesEXT memory_priority = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT, nullptr, VK_TRUE};
    if (device.extensions.is_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
      feature_struct_ptr.push_back(&memory_priority);
    }

    /* VK_EXT_pageable_device_local_memory */
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageable_device_local_memory = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,
        nullptr,
        VK_TRUE};
    if (device.extensions.is_enabled(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME)) {
      feature_struct_ptr.push_back(&pageable_device_local_memory);
    }

    /* Link all registered feature structs. */
    for (int i = 1; i < feature_struct_ptr.size(); i++) {
      ((VkBaseInStructure *)(feature_struct_ptr[i - 1]))->pNext =
          (VkBaseInStructure *)(feature_struct_ptr[i]);
    }

    device_create_info.pNext = feature_struct_ptr[0];
    VK_CHECK(vkCreateDevice(vk_physical_device, &device_create_info, nullptr, &device.vk_device),
             GHOST_kFailure);
    device.init_generic_queue();
    device.init_memory_allocator(vk_instance);
    return true;
  }
};

/** \} */

/**
 * A shared device between multiple contexts.
 *
 * The logical device needs to be shared as multiple contexts can be created and the logical
 * vulkan device they share should be the same otherwise memory operations might be done on the
 * incorrect device.
 */
static std::optional<GHOST_InstanceVK> vulkan_instance;

/** \} */

GHOST_ContextVK::GHOST_ContextVK(const GHOST_ContextParams &context_params,
#ifdef _WIN32
                                 HWND hwnd,
#elif defined(__APPLE__)
                                 CAMetalLayer *metal_layer,
#else
                                 GHOST_TVulkanPlatformType platform,
                                 /* X11 */
                                 Window window,
                                 Display *display,
                                 /* Wayland */
                                 wl_surface *wayland_surface,
                                 wl_display *wayland_display,
                                 const GHOST_ContextVK_WindowInfo *wayland_window_info,
#endif
                                 int contextMajorVersion,
                                 int contextMinorVersion,
                                 const GHOST_GPUDevice &preferred_device,
                                 const GHOST_WindowHDRInfo *hdr_info)
    : GHOST_Context(context_params),
#ifdef _WIN32
      hwnd_(hwnd),
#elif defined(__APPLE__)
      metal_layer_(metal_layer),
#else
      platform_(platform),
      /* X11 */
      display_(display),
      window_(window),
      /* Wayland */
      wayland_surface_(wayland_surface),
      wayland_display_(wayland_display),
      wayland_window_info_(wayland_window_info),
#endif
      context_major_version_(contextMajorVersion),
      context_minor_version_(contextMinorVersion),
      preferred_device_(preferred_device),
      hdr_info_(hdr_info),
      surface_(VK_NULL_HANDLE),
      swapchain_(VK_NULL_HANDLE),
      frame_data_(2),
      render_frame_(0),
      use_hdr_swapchain_(false)
{
  frame_data_.reserve(5);
}

GHOST_ContextVK::~GHOST_ContextVK()
{
  if (vulkan_instance.has_value()) {
    GHOST_InstanceVK &instance_vk = vulkan_instance.value();
    GHOST_DeviceVK &device_vk = instance_vk.device.value();
    device_vk.wait_idle();
    for (VkFence fence : fence_pile_) {
      vkDestroyFence(device_vk.vk_device, fence, nullptr);
    }
    fence_pile_.clear();
    destroySwapchain();

    if (surface_ != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(instance_vk.vk_instance, surface_, nullptr);
    }

    device_vk.users--;
    if (device_vk.users == 0) {
      vulkan_instance.reset();
    }
  }
}

GHOST_TSuccess GHOST_ContextVK::swapBufferAcquire()
{
  if (acquired_swapchain_image_index_.has_value()) {
    assert(false);
    return GHOST_kFailure;
  }

  GHOST_DeviceVK &device_vk = vulkan_instance->device.value();
  VkDevice vk_device = device_vk.vk_device;

  /* This method is called after all the draw calls in the application, and it signals that
   * we are ready to both (1) submit commands for those draw calls to the device and
   * (2) begin building the next frame. It is assumed as an invariant that the submission fence
   * in the current GHOST_Frame has been signaled. So, we wait for the *next* GHOST_Frame's
   * submission fence to be signaled, to ensure the invariant holds for the next call to
   * `swapBuffers`.
   *
   * We will pass the current GHOST_Frame to the swap_buffer_draw_callback_ for command buffer
   * submission, and it is the responsibility of that callback to use the current GHOST_Frame's
   * fence for it's submission fence. Since the callback is called after we wait for the next frame
   * to be complete, it is also safe in the callback to clean up resources associated with the next
   * frame.
   */
  render_frame_ = (render_frame_ + 1) % frame_data_.size();
  GHOST_Frame &submission_frame_data = frame_data_[render_frame_];
  /* Wait for previous time that the frame was used to finish rendering. Presenting can
   * still happen in parallel, but acquiring needs can only happen when the frame acquire semaphore
   * has been signaled and waited for. */
  if (submission_frame_data.submission_fence) {
    vkWaitForFences(vk_device, 1, &submission_frame_data.submission_fence, true, UINT64_MAX);
  }
  for (VkSwapchainKHR swapchain : submission_frame_data.discard_pile.swapchains) {
    this->destroySwapchainPresentFences(swapchain);
  }
  submission_frame_data.discard_pile.destroy(vk_device);

  const bool use_hdr_swapchain = hdr_info_ &&
                                 (hdr_info_->wide_gamut_enabled || hdr_info_->hdr_enabled) &&
                                 device_vk.use_vk_ext_swapchain_colorspace;
  if (use_hdr_swapchain != use_hdr_swapchain_) {
    /* Re-create swapchain if HDR mode was toggled in the system settings. */
    recreateSwapchain(use_hdr_swapchain);
  }
  else {
#ifdef WITH_GHOST_WAYLAND
    /* Wayland doesn't provide a WSI with windowing capabilities, therefore cannot detect whether
     * the swap-chain needs to be recreated. But as a side effect we can recreate the swap-chain
     * before presenting. */
    if (wayland_window_info_) {
      const bool recreate_swapchain =
          ((wayland_window_info_->size[0] !=
            std::max(render_extent_.width, render_extent_min_.width)) ||
           (wayland_window_info_->size[1] !=
            std::max(render_extent_.height, render_extent_min_.height)));

      if (recreate_swapchain) {
        /* Swap-chain is out of date. Recreate swap-chain. */
        recreateSwapchain(use_hdr_swapchain);
      }
    }
#endif
  }
  /* there is no valid swapchain when the previous window was minimized. User can have maximized
   * the window so we need to check if the swapchain has to be created. */
  if (swapchain_ == VK_NULL_HANDLE) {
    recreateSwapchain(use_hdr_swapchain);
  }

  /* Acquiree next image, swapchain can be (or become) invalid when minimizing window.*/
  uint32_t image_index = 0;
  if (swapchain_ != VK_NULL_HANDLE) {
    /* Some platforms (NVIDIA/Wayland) can receive an out of date swapchain when acquiring the next
     * swapchain image. Other do it when calling vkQueuePresent. */
    VkResult acquire_result = VK_ERROR_OUT_OF_DATE_KHR;
    while (swapchain_ != VK_NULL_HANDLE &&
           (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR))
    {
      acquire_result = vkAcquireNextImageKHR(vk_device,
                                             swapchain_,
                                             UINT64_MAX,
                                             submission_frame_data.acquire_semaphore,
                                             VK_NULL_HANDLE,
                                             &image_index);
      if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(use_hdr_swapchain);
      }
    }
  }

  /* Acquired callback is also called when there is no swapchain.
   *
   * When acquiring swap chain (image) and the swap chain is discarded (window has been minimized).
   * We have trigger a last acquired callback to reduce the attachments of the GPUFramebuffer.
   * Vulkan backend will retrieve the data (getVulkanSwapChainFormat) containing a render extent of
   * 0,0.
   *
   * The next frame window manager will detect that the window is minimized and doesn't draw the
   * window at all.
   */
  if (swap_buffer_acquired_callback_) {
    swap_buffer_acquired_callback_();
  }

  if (swapchain_ == VK_NULL_HANDLE) {
    CLOG_TRACE(&LOG, "Swap-chain unavailable (minimized window).");
    return GHOST_kSuccess;
  }

  CLOG_DEBUG(&LOG,
             "Acquired swap-chain image (render_frame=%lu, image_index=%u)",
             render_frame_,
             image_index);
  acquired_swapchain_image_index_ = image_index;

  return GHOST_kSuccess;
}
VkFence GHOST_ContextVK::getFence()
{
  if (!fence_pile_.empty()) {
    VkFence fence = fence_pile_.back();
    fence_pile_.pop_back();
    return fence;
  }
  GHOST_DeviceVK &device_vk = vulkan_instance->device.value();
  VkFence fence = VK_NULL_HANDLE;
  const VkFenceCreateInfo fence_create_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  vkCreateFence(device_vk.vk_device, &fence_create_info, nullptr, &fence);
  return fence;
}

void GHOST_ContextVK::setPresentFence(VkSwapchainKHR swapchain, VkFence present_fence)
{
  if (present_fence == VK_NULL_HANDLE) {
    return;
  }
  present_fences_[swapchain].push_back(present_fence);
  GHOST_DeviceVK &device_vk = vulkan_instance->device.value();
  /** Recycle signaled fences. */
  for (std::pair<const VkSwapchainKHR, std::vector<VkFence>> &item : present_fences_) {
    std::vector<VkFence>::iterator end = item.second.end();
    std::vector<VkFence>::iterator it = std::remove_if(
        item.second.begin(), item.second.end(), [&](const VkFence fence) {
          if (vkGetFenceStatus(device_vk.vk_device, fence) == VK_NOT_READY) {
            return false;
          }
          vkResetFences(device_vk.vk_device, 1, &fence);
          fence_pile_.push_back(fence);
          return true;
        });
    item.second.erase(it, end);
  }
}

GHOST_TSuccess GHOST_ContextVK::swapBufferRelease()
{
  /* Minimized windows don't have a swapchain and swapchain image. In this case we perform the draw
   * to release render graph and discarded resources. */
  if (swapchain_ == VK_NULL_HANDLE) {
    GHOST_VulkanSwapChainData swap_chain_data = {};
    if (swap_buffer_draw_callback_) {
      swap_buffer_draw_callback_(&swap_chain_data);
    }
    return GHOST_kSuccess;
  }

  if (!acquired_swapchain_image_index_.has_value()) {
    assert(false);
    return GHOST_kFailure;
  }
  GHOST_DeviceVK &device_vk = vulkan_instance->device.value();
  VkDevice vk_device = device_vk.vk_device;

  uint32_t image_index = acquired_swapchain_image_index_.value();
  GHOST_SwapchainImage &swapchain_image = swapchain_images_[image_index];
  GHOST_Frame &submission_frame_data = frame_data_[render_frame_];
  const bool use_hdr_swapchain = hdr_info_ && hdr_info_->hdr_enabled &&
                                 device_vk.use_vk_ext_swapchain_colorspace;

  GHOST_VulkanSwapChainData swap_chain_data;
  swap_chain_data.image = swapchain_image.vk_image;
  swap_chain_data.surface_format = surface_format_;
  swap_chain_data.extent = render_extent_;
  swap_chain_data.submission_fence = submission_frame_data.submission_fence;
  swap_chain_data.acquire_semaphore = submission_frame_data.acquire_semaphore;
  swap_chain_data.present_semaphore = swapchain_image.present_semaphore;
  swap_chain_data.sdr_scale = (hdr_info_) ? hdr_info_->sdr_white_level : 1.0f;

  vkResetFences(vk_device, 1, &submission_frame_data.submission_fence);
  if (swap_buffer_draw_callback_) {
    swap_buffer_draw_callback_(&swap_chain_data);
  }

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &swapchain_image.present_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &image_index;
  present_info.pResults = nullptr;

  VkResult present_result = VK_SUCCESS;
  {
    std::scoped_lock lock(device_vk.queue_mutex);
    VkSwapchainPresentFenceInfoEXT fence_info{VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT};
    VkFence present_fence = VK_NULL_HANDLE;
    if (device_vk.use_vk_ext_swapchain_maintenance_1) {
      present_fence = this->getFence();

      fence_info.swapchainCount = 1;
      fence_info.pFences = &present_fence;

      present_info.pNext = &fence_info;
    }
    present_result = vkQueuePresentKHR(device_vk.generic_queue, &present_info);
    this->setPresentFence(swapchain_, present_fence);
  }
  acquired_swapchain_image_index_.reset();

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain(use_hdr_swapchain);
    return GHOST_kSuccess;
  }
  if (present_result != VK_SUCCESS) {
    CLOG_ERROR(&LOG,
               "Vulkan: failed to present swap-chain image : %s",
               vulkan_error_as_string(present_result));
    return GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::getVulkanSwapChainFormat(
    GHOST_VulkanSwapChainData *r_swap_chain_data)
{
  r_swap_chain_data->image = VK_NULL_HANDLE;
  r_swap_chain_data->surface_format = surface_format_;
  r_swap_chain_data->extent = render_extent_;
  r_swap_chain_data->sdr_scale = (hdr_info_) ? hdr_info_->sdr_white_level : 1.0f;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::getVulkanHandles(GHOST_VulkanHandles &r_handles)
{
  r_handles = {
      VK_NULL_HANDLE, /* instance */
      VK_NULL_HANDLE, /* physical_device */
      VK_NULL_HANDLE, /* device */
      0,              /* queue_family */
      VK_NULL_HANDLE, /* queue */
      nullptr,        /* queue_mutex */
      VK_NULL_HANDLE, /* vma_allocator */
  };

  if (vulkan_instance.has_value() && vulkan_instance.value().device.has_value()) {
    GHOST_InstanceVK &instance_vk = vulkan_instance.value();
    GHOST_DeviceVK &device_vk = instance_vk.device.value();
    r_handles = {
        instance_vk.vk_instance,
        device_vk.vk_physical_device,
        device_vk.vk_device,
        device_vk.generic_queue_family,
        device_vk.generic_queue,
        &device_vk.queue_mutex,
        device_vk.vma_allocator,
    };
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::setVulkanSwapBuffersCallbacks(
    std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffer_draw_callback,
    std::function<void(void)> swap_buffer_acquired_callback,
    std::function<void(GHOST_VulkanOpenXRData *)> openxr_acquire_framebuffer_image_callback,
    std::function<void(GHOST_VulkanOpenXRData *)> openxr_release_framebuffer_image_callback)
{
  swap_buffer_draw_callback_ = swap_buffer_draw_callback;
  swap_buffer_acquired_callback_ = swap_buffer_acquired_callback;
  openxr_acquire_framebuffer_image_callback_ = openxr_acquire_framebuffer_image_callback;
  openxr_release_framebuffer_image_callback_ = openxr_release_framebuffer_image_callback;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::activateDrawingContext()
{
  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::releaseDrawingContext()
{
  active_context_ = nullptr;
  return GHOST_kSuccess;
}

static GHOST_TSuccess selectPresentMode(const GHOST_TVSyncModes vsync,
                                        VkPhysicalDevice device,
                                        VkSurfaceKHR surface,
                                        VkPresentModeKHR *r_presentMode)
{
  uint32_t present_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count, nullptr);
  vector<VkPresentModeKHR> presents(present_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count, presents.data());

  if (vsync != GHOST_kVSyncModeUnset) {
    const bool vsync_off = (vsync == GHOST_kVSyncModeOff);
    if (vsync_off) {
      for (auto present_mode : presents) {
        if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
          *r_presentMode = present_mode;
          return GHOST_kSuccess;
        }
      }
      CLOG_WARN(&LOG,
                "Vulkan: VSync off was requested via --gpu-vsync, "
                "but VK_PRESENT_MODE_IMMEDIATE_KHR is not supported.");
    }
  }

  /* MAILBOX is the lowest latency V-Sync enabled mode. We will use it if available as it fixes
   * some lag on NVIDIA/Intel GPUs. */
  /* TODO: select the correct presentation mode based on the actual being performed by the user.
   * When low latency is required (paint cursor) we should select mailbox, otherwise we can do FIFO
   * to reduce CPU/GPU usage. */
  for (auto present_mode : presents) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      *r_presentMode = present_mode;
      return GHOST_kSuccess;
    }
  }

  /* FIFO present mode is always available and we (should) prefer it as it will keep the main loop
   * running along the monitor refresh rate. Mailbox and FIFO relaxed can generate a lot of frames
   * that will never be displayed. */
  *r_presentMode = VK_PRESENT_MODE_FIFO_KHR;
  return GHOST_kSuccess;
}

/**
 * Select the surface format that we will use.
 *
 * We will select any 8bit UNORM surface.
 */
static bool selectSurfaceFormat(const VkPhysicalDevice physical_device,
                                const VkSurfaceKHR surface,
                                bool use_hdr_swapchain,
                                VkSurfaceFormatKHR &r_surfaceFormat)
{
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
  vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());

  array<pair<VkColorSpaceKHR, VkFormat>, 4> selection_order = {
      make_pair(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, VK_FORMAT_R16G16B16A16_SFLOAT),
      make_pair(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_FORMAT_R16G16B16A16_SFLOAT),
      make_pair(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_FORMAT_R8G8B8A8_UNORM),
      make_pair(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_FORMAT_B8G8R8A8_UNORM),
  };

  for (pair<VkColorSpaceKHR, VkFormat> &pair : selection_order) {
    if (pair.second == VK_FORMAT_R16G16B16A16_SFLOAT && !use_hdr_swapchain) {
      continue;
    }
    for (const VkSurfaceFormatKHR &format : formats) {
      if (format.colorSpace == pair.first && format.format == pair.second) {
        r_surfaceFormat = format;
        return true;
      }
    }
  }

  return false;
}

GHOST_TSuccess GHOST_ContextVK::initializeFrameData()
{
  VkDevice device = vulkan_instance.value().device.value().vk_device;

  const VkSemaphoreCreateInfo vk_semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
  const VkFenceCreateInfo vk_fence_create_info = {
      VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
  for (GHOST_SwapchainImage &swapchain_image : swapchain_images_) {
    /* VK_EXT_swapchain_maintenance1 reuses present semaphores. */
    if (swapchain_image.present_semaphore == VK_NULL_HANDLE) {
      VK_CHECK(vkCreateSemaphore(
                   device, &vk_semaphore_create_info, nullptr, &swapchain_image.present_semaphore),
               GHOST_kFailure);
    }
  }

  for (int index = 0; index < frame_data_.size(); index++) {
    GHOST_Frame &frame_data = frame_data_[index];
    /* VK_EXT_swapchain_maintenance1 reuses acquire semaphores. */
    if (frame_data.acquire_semaphore == VK_NULL_HANDLE) {
      VK_CHECK(vkCreateSemaphore(
                   device, &vk_semaphore_create_info, nullptr, &frame_data.acquire_semaphore),
               GHOST_kFailure);
    }
    if (frame_data.submission_fence == VK_NULL_HANDLE) {
      VK_CHECK(vkCreateFence(device, &vk_fence_create_info, nullptr, &frame_data.submission_fence),
               GHOST_kFailure);
    }
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::recreateSwapchain(bool use_hdr_swapchain)
{
  GHOST_InstanceVK &instance_vk = vulkan_instance.value();
  GHOST_DeviceVK &device_vk = instance_vk.device.value();

  surface_format_ = {};
  if (!selectSurfaceFormat(
          device_vk.vk_physical_device, surface_, use_hdr_swapchain, surface_format_))
  {
    return GHOST_kFailure;
  }

  VkPresentModeKHR present_mode;
  if (!selectPresentMode(getVSync(), device_vk.vk_physical_device, surface_, &present_mode)) {
    return GHOST_kFailure;
  }

  /* Query the surface capabilities for the given present mode on the surface. */
  VkSurfacePresentScalingCapabilitiesEXT vk_surface_present_scaling_capabilities = {
      VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_EXT,
  };
  VkSurfaceCapabilities2KHR vk_surface_capabilities = {
      VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
      &vk_surface_present_scaling_capabilities,
  };
  VkSurfacePresentModeEXT vk_surface_present_mode = {
      VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT, nullptr, present_mode};
  VkPhysicalDeviceSurfaceInfo2KHR vk_physical_device_surface_info = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, &vk_surface_present_mode, surface_};
  VkSurfaceCapabilitiesKHR capabilities = {};

  if (device_vk.use_vk_ext_swapchain_maintenance_1) {
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(device_vk.vk_physical_device,
                                                        &vk_physical_device_surface_info,
                                                        &vk_surface_capabilities),
             GHOST_kFailure);
    capabilities = vk_surface_capabilities.surfaceCapabilities;
  }
  else {
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                 device_vk.vk_physical_device, surface_, &capabilities),
             GHOST_kFailure);
  }

  use_hdr_swapchain_ = use_hdr_swapchain;
  render_extent_ = capabilities.currentExtent;
  render_extent_min_ = capabilities.minImageExtent;
  if (render_extent_.width == UINT32_MAX) {
    /* Window Manager is going to set the surface size based on the given size.
     * Choose something between minImageExtent and maxImageExtent. */
    int width = 0;
    int height = 0;

#ifdef WITH_GHOST_WAYLAND
    /* Wayland doesn't provide a windowing API via WSI. */
    if (wayland_window_info_) {
      width = wayland_window_info_->size[0];
      height = wayland_window_info_->size[1];
    }
#endif

    if (width == 0 || height == 0) {
      width = 1280;
      height = 720;
    }

    render_extent_.width = width;
    render_extent_.height = height;

    if (capabilities.minImageExtent.width > render_extent_.width) {
      render_extent_.width = capabilities.minImageExtent.width;
    }
    if (capabilities.minImageExtent.height > render_extent_.height) {
      render_extent_.height = capabilities.minImageExtent.height;
    }
  }

  if (device_vk.use_vk_ext_swapchain_maintenance_1) {
    if (vk_surface_present_scaling_capabilities.minScaledImageExtent.width > render_extent_.width)
    {
      render_extent_.width = vk_surface_present_scaling_capabilities.minScaledImageExtent.width;
    }
    if (vk_surface_present_scaling_capabilities.minScaledImageExtent.height >
        render_extent_.height)
    {
      render_extent_.height = vk_surface_present_scaling_capabilities.minScaledImageExtent.height;
    }
  }

  /* Discard swapchain resources of current swapchain. */
  GHOST_FrameDiscard &discard_pile = frame_data_[render_frame_].discard_pile;
  for (GHOST_SwapchainImage &swapchain_image : swapchain_images_) {
    swapchain_image.vk_image = VK_NULL_HANDLE;
    if (swapchain_image.present_semaphore != VK_NULL_HANDLE) {
      discard_pile.semaphores.push_back(swapchain_image.present_semaphore);
      swapchain_image.present_semaphore = VK_NULL_HANDLE;
    }
  }

  /* Swap-chains with out any resolution should not be created. In the case the render extent is
   * zero we should not use the swap-chain.
   *
   * VUID-VkSwapchainCreateInfoKHR-imageExtent-01689
   */
  if (render_extent_.width == 0 || render_extent_.height == 0) {
    if (swapchain_) {
      discard_pile.swapchains.push_back(swapchain_);
      swapchain_ = VK_NULL_HANDLE;
    }
    return GHOST_kFailure;
  }

  /* Use double buffering when using FIFO. Increasing the number of images could stall when doing
   * actions that require low latency (paint cursor, UI resizing). MAILBOX prefers triple
   * buffering. */
  uint32_t image_count_requested = present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? 3 : 2;
  /* NOTE: maxImageCount == 0 means no limit. */
  if (capabilities.minImageCount != 0 && image_count_requested < capabilities.minImageCount) {
    image_count_requested = capabilities.minImageCount;
  }
  if (capabilities.maxImageCount != 0 && image_count_requested > capabilities.maxImageCount) {
    image_count_requested = capabilities.maxImageCount;
  }

  VkSwapchainKHR old_swapchain = swapchain_;

  /* First time we stretch the swapchain image as it can happen that the first frame size isn't
   * correctly reported by the initial swapchain. All subsequent creations will use one to one as
   * that can reduce resizing artifacts. */
  VkPresentScalingFlagBitsEXT vk_present_scaling = old_swapchain == VK_NULL_HANDLE ?
                                                       VK_PRESENT_SCALING_STRETCH_BIT_EXT :
                                                       VK_PRESENT_SCALING_ONE_TO_ONE_BIT_EXT;

  VkSwapchainPresentModesCreateInfoEXT vk_swapchain_present_modes = {
      VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT, nullptr, 1, &present_mode};
  VkSwapchainPresentScalingCreateInfoEXT vk_swapchain_present_scaling = {
      VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT,
      &vk_swapchain_present_modes,
      vk_surface_present_scaling_capabilities.supportedPresentScaling & vk_present_scaling,
      vk_surface_present_scaling_capabilities.supportedPresentGravityX &
          VK_PRESENT_GRAVITY_MIN_BIT_EXT,
      vk_surface_present_scaling_capabilities.supportedPresentGravityY &
          VK_PRESENT_GRAVITY_MAX_BIT_EXT,
  };

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  if (device_vk.use_vk_ext_swapchain_maintenance_1) {
    create_info.pNext = &vk_swapchain_present_scaling;
  }
  create_info.surface = surface_;
  create_info.minImageCount = image_count_requested;
  create_info.imageFormat = surface_format_.format;
  create_info.imageColorSpace = surface_format_.colorSpace;
  create_info.imageExtent = render_extent_;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           (use_hdr_swapchain ? VK_IMAGE_USAGE_STORAGE_BIT : 0);
  create_info.preTransform = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = old_swapchain;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 0;
  create_info.pQueueFamilyIndices = nullptr;

  VK_CHECK(vkCreateSwapchainKHR(device_vk.vk_device, &create_info, nullptr, &swapchain_),
           GHOST_kFailure);

  /* image_count may not be what we requested! Getter for final value. */
  uint32_t actual_image_count = 0;
  vkGetSwapchainImagesKHR(device_vk.vk_device, swapchain_, &actual_image_count, nullptr);
  /* Some platforms require a minimum amount of render frames that is larger than we expect. When
   * that happens we should increase the number of frames in flight. We could also consider
   * splitting the frame in flight and image specific data. */
  if (actual_image_count > frame_data_.size()) {
    CLOG_TRACE(&LOG, "Vulkan: Increasing frame data to %u frames", actual_image_count);
    assert(actual_image_count <= frame_data_.capacity());
    frame_data_.resize(actual_image_count);
  }
  swapchain_images_.resize(actual_image_count);
  std::vector<VkImage> swapchain_images(actual_image_count);
  vkGetSwapchainImagesKHR(
      device_vk.vk_device, swapchain_, &actual_image_count, swapchain_images.data());
  for (int index = 0; index < actual_image_count; index++) {
    swapchain_images_[index].vk_image = swapchain_images[index];
  }
  CLOG_DEBUG(&LOG,
             "Vulkan: recreating swapchain: width=%u, height=%u, format=%d, colorSpace=%d, "
             "present_mode=%d, image_count_requested=%u, image_count_acquired=%u, swapchain=%lx, "
             "old_swapchain=%lx",
             render_extent_.width,
             render_extent_.height,
             surface_format_.format,
             surface_format_.colorSpace,
             present_mode,
             image_count_requested,
             actual_image_count,
             uint64_t(swapchain_),
             uint64_t(old_swapchain));
  /* Construct new semaphores. It can be that image_count is larger than previously. We only need
   * to fill in where the handle is `VK_NULL_HANDLE`. */
  /* Previous handles from the frame data cannot be used and should be discarded. */
  for (GHOST_Frame &frame : frame_data_) {
    if (frame.acquire_semaphore != VK_NULL_HANDLE) {
      discard_pile.semaphores.push_back(frame.acquire_semaphore);
    }
    frame.acquire_semaphore = VK_NULL_HANDLE;
  }
  if (old_swapchain) {
    discard_pile.swapchains.push_back(old_swapchain);
  }
  initializeFrameData();

  image_count_ = actual_image_count;

  return GHOST_kSuccess;
}

void GHOST_ContextVK::destroySwapchainPresentFences(VkSwapchainKHR swapchain)
{
  GHOST_DeviceVK &device_vk = vulkan_instance.value().device.value();
  const std::vector<VkFence> &fences = present_fences_[swapchain];
  if (!fences.empty()) {
    vkWaitForFences(device_vk.vk_device, fences.size(), fences.data(), VK_TRUE, UINT64_MAX);
    for (VkFence fence : fences) {
      vkDestroyFence(device_vk.vk_device, fence, nullptr);
    }
  }
  present_fences_.erase(swapchain);
}

GHOST_TSuccess GHOST_ContextVK::destroySwapchain()
{
  GHOST_DeviceVK &device_vk = vulkan_instance.value().device.value();

  if (swapchain_ != VK_NULL_HANDLE) {
    this->destroySwapchainPresentFences(swapchain_);
    vkDestroySwapchainKHR(device_vk.vk_device, swapchain_, nullptr);
  }
  device_vk.wait_idle();
  for (GHOST_SwapchainImage &swapchain_image : swapchain_images_) {
    swapchain_image.destroy(device_vk.vk_device);
  }
  swapchain_images_.clear();
  for (GHOST_Frame &frame_data : frame_data_) {
    for (VkSwapchainKHR swapchain : frame_data.discard_pile.swapchains) {
      this->destroySwapchainPresentFences(swapchain);
    }
    frame_data.destroy(device_vk.vk_device);
  }
  frame_data_.clear();

  return GHOST_kSuccess;
}

const char *GHOST_ContextVK::getPlatformSpecificSurfaceExtension() const
{
#ifdef _WIN32
  return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__APPLE__)
  return VK_EXT_METAL_SURFACE_EXTENSION_NAME;
#else /* UNIX/Linux */
  switch (platform_) {
#  ifdef WITH_GHOST_X11
    case GHOST_kVulkanPlatformX11:
      return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
      break;
#  endif
#  ifdef WITH_GHOST_WAYLAND
    case GHOST_kVulkanPlatformWayland:
      return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
      break;
#  endif
    case GHOST_kVulkanPlatformHeadless:
      break;
  }
#endif
  return nullptr;
}

GHOST_TSuccess GHOST_ContextVK::initializeDrawingContext()
{
  bool use_vk_ext_swapchain_colorspace = false;
#ifdef _WIN32
  const bool use_window_surface = (hwnd_ != nullptr);
#elif defined(__APPLE__)
  const bool use_window_surface = (metal_layer_ != nullptr);
#else /* UNIX/Linux */
  bool use_window_surface = false;
  switch (platform_) {
#  ifdef WITH_GHOST_X11
    case GHOST_kVulkanPlatformX11:
      use_window_surface = (display_ != nullptr) && (window_ != (Window) nullptr);
      break;
#  endif
#  ifdef WITH_GHOST_WAYLAND
    case GHOST_kVulkanPlatformWayland:
      use_window_surface = (wayland_display_ != nullptr) && (wayland_surface_ != nullptr);
      break;
#  endif
    case GHOST_kVulkanPlatformHeadless:
      use_window_surface = false;
      break;
  }
#endif

  vector<const char *> required_device_extensions;
  vector<const char *> optional_device_extensions;

  /* Initialize VkInstance */
  if (!vulkan_instance.has_value()) {
    vulkan_instance.emplace();
    GHOST_InstanceVK &instance_vk = vulkan_instance.value();
    if (context_params_.is_debug) {
      instance_vk.extensions.enable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
    }

    if (use_window_surface) {
      const char *native_surface_extension_name = getPlatformSpecificSurfaceExtension();
      instance_vk.extensions.enable(VK_KHR_SURFACE_EXTENSION_NAME);
      instance_vk.extensions.enable(native_surface_extension_name);
      /* X11 doesn't use the correct swapchain offset, flipping can squash the first frames. */
      const bool use_vk_ext_swapchain_maintenance1 =
#ifdef WITH_GHOST_X11
          platform_ != GHOST_kVulkanPlatformX11 &&
#endif
          instance_vk.extensions.is_supported(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) &&
          instance_vk.extensions.is_supported(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
      if (use_vk_ext_swapchain_maintenance1) {
        instance_vk.extensions.enable(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        instance_vk.extensions.enable(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
        optional_device_extensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
      }

      use_vk_ext_swapchain_colorspace = instance_vk.extensions.enable(
          VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, true);

      required_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    if (!instance_vk.create_instance(
            VK_MAKE_VERSION(context_major_version_, context_minor_version_, 0)))
    {
      vulkan_instance.reset();
      return GHOST_kFailure;
    }
  }
  GHOST_InstanceVK &instance_vk = vulkan_instance.value();

  /* Initialize VkSurface */
  if (use_window_surface) {
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandle(nullptr);
    surface_create_info.hwnd = hwnd_;
    VK_CHECK(
        vkCreateWin32SurfaceKHR(instance_vk.vk_instance, &surface_create_info, nullptr, &surface_),
        GHOST_kFailure);
#elif defined(__APPLE__)
    VkMetalSurfaceCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    info.pNext = nullptr;
    info.flags = 0;
    info.pLayer = metal_layer_;
    VK_CHECK(vkCreateMetalSurfaceEXT(instance_vk.vk_instance, &info, nullptr, &surface_),
             GHOST_kFailure);
#else
    switch (platform_) {
#  ifdef WITH_GHOST_X11
      case GHOST_kVulkanPlatformX11: {
        VkXlibSurfaceCreateInfoKHR surface_create_info = {};
        surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        surface_create_info.dpy = display_;
        surface_create_info.window = window_;
        VK_CHECK(vkCreateXlibSurfaceKHR(
                     instance_vk.vk_instance, &surface_create_info, nullptr, &surface_),
                 GHOST_kFailure);
        break;
      }
#  endif
#  ifdef WITH_GHOST_WAYLAND
      case GHOST_kVulkanPlatformWayland: {
        VkWaylandSurfaceCreateInfoKHR surface_create_info = {};
        surface_create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surface_create_info.display = wayland_display_;
        surface_create_info.surface = wayland_surface_;
        VK_CHECK(vkCreateWaylandSurfaceKHR(
                     instance_vk.vk_instance, &surface_create_info, nullptr, &surface_),
                 GHOST_kFailure);
        break;
      }
#  endif
      case GHOST_kVulkanPlatformHeadless: {
        surface_ = VK_NULL_HANDLE;
        break;
      }
    }

#endif
  }

  /* Initialize VkDevice */
  if (!vulkan_instance->device.has_value()) {
    /* External memory extensions. */
#ifdef _WIN32
    optional_device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
#else
    optional_device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#endif

    required_device_extensions.push_back(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
    required_device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME);
    optional_device_extensions.push_back(
        VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
    optional_device_extensions.push_back(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);

    if (!instance_vk.select_physical_device(preferred_device_, required_device_extensions)) {
      return GHOST_kFailure;
    }

    if (!instance_vk.create_device(use_vk_ext_swapchain_colorspace,
                                   required_device_extensions,
                                   optional_device_extensions))
    {
      return GHOST_kFailure;
    }
  }
  GHOST_DeviceVK &device_vk = instance_vk.device.value();

  device_vk.users++;

  render_extent_ = {0, 0};
  render_extent_min_ = {0, 0};
  surface_format_ = {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::releaseNativeHandles()
{
  return GHOST_kSuccess;
}
