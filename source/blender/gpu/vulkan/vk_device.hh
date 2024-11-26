/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "render_graph/vk_render_graph.hh"
#include "render_graph/vk_resource_state_tracker.hh"
#include "vk_buffer.hh"
#include "vk_common.hh"
#include "vk_debug.hh"
#include "vk_descriptor_pools.hh"
#include "vk_descriptor_set_layouts.hh"
#include "vk_pipeline_pool.hh"
#include "vk_resource_pool.hh"
#include "vk_samplers.hh"

namespace blender::gpu {
class VKBackend;

/* TODO: Split into VKWorkarounds and VKExtensions to remove the negating when an extension isn't
 * supported. */
struct VKWorkarounds {
  /**
   * Some devices don't support pixel formats that are aligned to 24 and 48 bits.
   * In this case we need to use a different texture format.
   *
   * If set to true we should work around this issue by using a different texture format.
   */
  bool not_aligned_pixel_formats = false;

  /**
   * Is the workaround for devices that don't support
   * #VkPhysicalDeviceVulkan12Features::shaderOutputViewportIndex enabled.
   */
  bool shader_output_viewport_index = false;

  /**
   * Is the workaround for devices that don't support
   * #VkPhysicalDeviceVulkan12Features::shaderOutputLayer enabled.
   */
  bool shader_output_layer = false;

  struct {
    /**
     * Is the workaround enabled for devices that don't support using VK_FORMAT_R8G8B8_* as vertex
     * buffer.
     */
    bool r8g8b8 = false;
  } vertex_formats;

  /**
   * Is the workaround for devices that don't support
   * #VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR::fragmentShaderBarycentric enabled.
   * If set to true, the backend would inject a geometry shader to produce barycentric coordinates.
   */
  bool fragment_shader_barycentric = false;

  /**
   * Is the workarounds for devices that don't support VK_KHR_dynamic_rendering enabled.
   */
  bool dynamic_rendering = false;

  /**
   * Is the workarounds for devices that don't support VK_EXT_dynamic_rendering_unused_attachments
   * enabled.
   */
  bool dynamic_rendering_unused_attachments = false;
};

/**
 * Shared resources between contexts that run in the same thread.
 */
class VKThreadData : public NonCopyable, NonMovable {
  static constexpr uint32_t resource_pools_count = 3;

 public:
  /** Thread ID this instance belongs to. */
  pthread_t thread_id;
  /**
   * Index of the active resource pool. Is in sync with the active swap chain image or cycled when
   * rendering.
   *
   * NOTE: Initialized to `UINT32_MAX` to detect first change.
   */
  uint32_t resource_pool_index = UINT32_MAX;
  std::array<VKResourcePool, resource_pools_count> resource_pools;

  /**
   * The current rendering depth.
   *
   * GPU_rendering_begin can be called multiple times forming a hierarchy. The same resource pool
   * should be used for the whole hierarchy. rendering_depth is increased for every
   * GPU_rendering_begin and decreased when GPU_rendering_end is called. Resources pools are cycled
   * when the rendering_depth set to 0.
   */
  int32_t rendering_depth = 0;

  VKThreadData(VKDevice &device, pthread_t thread_id);
  void deinit(VKDevice &device);

  /**
   * Get the active resource pool.
   */
  VKResourcePool &resource_pool_get()
  {
    if (resource_pool_index >= resource_pools.size()) {
      return resource_pools[0];
    }
    return resource_pools[resource_pool_index];
  }

  /** Activate the next resource pool. */
  void resource_pool_next()
  {
    if (resource_pool_index == UINT32_MAX) {
      resource_pool_index = 1;
    }
    else {
      resource_pool_index = (resource_pool_index + 1) % resource_pools_count;
    }
  }
};

class VKDevice : public NonCopyable {
 private:
  /** Copies of the handles owned by the GHOST context. */
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  uint32_t vk_queue_family_ = 0;
  VkQueue vk_queue_ = VK_NULL_HANDLE;
  std::mutex *queue_mutex_ = nullptr;

  VKSamplers samplers_;
  VKDescriptorSetLayouts descriptor_set_layouts_;

  /**
   * Available Contexts for this device.
   *
   * Device keeps track of each contexts. When buffers/images are freed they need to be removed
   * from all contexts state managers.
   *
   * The contexts inside this list aren't owned by the VKDevice. Caller of `GPU_context_create`
   * holds the ownership.
   */
  Vector<std::reference_wrapper<VKContext>> contexts_;

  /** Allocator used for texture and buffers and other resources. */
  VmaAllocator mem_allocator_ = VK_NULL_HANDLE;

  /** Limits of the device linked to this context. */
  VkPhysicalDeviceProperties vk_physical_device_properties_ = {};
  VkPhysicalDeviceDriverProperties vk_physical_device_driver_properties_ = {};
  VkPhysicalDeviceMemoryProperties vk_physical_device_memory_properties_ = {};
  /** Features support. */
  VkPhysicalDeviceFeatures vk_physical_device_features_ = {};
  VkPhysicalDeviceVulkan11Features vk_physical_device_vulkan_11_features_ = {};
  VkPhysicalDeviceVulkan12Features vk_physical_device_vulkan_12_features_ = {};
  Array<VkExtensionProperties> device_extensions_;

  /** Functions of vk_ext_debugutils for this device/instance. */
  debug::VKDebuggingTools debugging_tools_;

  /* Workarounds */
  VKWorkarounds workarounds_;

  std::string glsl_patch_;
  Vector<VKThreadData *> thread_data_;

 public:
  render_graph::VKResourceStateTracker resources;
  VKDiscardPool orphaned_data;
  VKPipelinePool pipelines;
  /** Buffer to bind to unbound resource locations. */
  VKBuffer dummy_buffer;

  /**
   * This struct contains the functions pointer to extension provided functions.
   */
  struct {
    /* Extension: VK_KHR_dynamic_rendering */
    PFN_vkCmdBeginRendering vkCmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering vkCmdEndRendering = nullptr;

    /* Extension: VK_EXT_debug_utils */
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabel = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabel = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectName = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger = nullptr;
  } functions;

  const char *extension_name_get(int index) const
  {
    return device_extensions_[index].extensionName;
  }

  VkPhysicalDevice physical_device_get() const
  {
    return vk_physical_device_;
  }

  const VkPhysicalDeviceProperties &physical_device_properties_get() const
  {
    return vk_physical_device_properties_;
  }

  const VkPhysicalDeviceFeatures &physical_device_features_get() const
  {
    return vk_physical_device_features_;
  }

  const VkPhysicalDeviceVulkan11Features &physical_device_vulkan_11_features_get() const
  {
    return vk_physical_device_vulkan_11_features_;
  }

  const VkPhysicalDeviceVulkan12Features &physical_device_vulkan_12_features_get() const
  {
    return vk_physical_device_vulkan_12_features_;
  }

  VkInstance instance_get() const
  {
    return vk_instance_;
  };

  VkDevice vk_handle() const
  {
    return vk_device_;
  }

  VkQueue queue_get() const
  {
    return vk_queue_;
  }
  std::mutex &queue_mutex_get()
  {
    return *queue_mutex_;
  }

  const uint32_t queue_family_get() const
  {
    return vk_queue_family_;
  }

  VmaAllocator mem_allocator_get() const
  {
    return mem_allocator_;
  }

  VKDescriptorSetLayouts &descriptor_set_layouts_get()
  {
    return descriptor_set_layouts_;
  }

  debug::VKDebuggingTools &debugging_tools_get()
  {
    return debugging_tools_;
  }

  const debug::VKDebuggingTools &debugging_tools_get() const
  {
    return debugging_tools_;
  }

  const VKSamplers &samplers() const
  {
    return samplers_;
  }

  bool is_initialized() const;
  void init(void *ghost_context);
  void reinit();
  void deinit();

  eGPUDeviceType device_type() const;
  eGPUDriverType driver_type() const;
  std::string vendor_name() const;
  std::string driver_version() const;

  /**
   * Check if a specific extension is supported by the device.
   *
   * This should be called from vk_backend to set the correct capabilities and workarounds needed
   * for this device.
   */
  bool supports_extension(const char *extension_name) const;

  const VKWorkarounds &workarounds_get() const
  {
    return workarounds_;
  }

  const char *glsl_patch_get() const;
  void init_glsl_patch();

  /* -------------------------------------------------------------------- */
  /** \name Resource management
   * \{ */

  /**
   * Get or create current thread data.
   */
  VKThreadData &current_thread_data();

  /**
   * Get the discard pool for the current thread.
   *
   * When the active thread has a context a discard pool associated to the thread is returned.
   * When there is no context the orphan discard pool is returned.
   *
   * A thread with a context can have multiple discard pools. One for each swap-chain image.
   * A thread without a context is most likely a discarded resource triggered during dependency
   * graph update. A dependency graph update from the viewport during playback or editing;
   * or a dependency graph update when rendering.
   * These can happen from a different thread which will don't have a context at all.
   * \param thread_safe: Caller thread already owns the resources mutex and is safe to run this
   * function without trying to reacquire resources mutex making a deadlock.
   */
  VKDiscardPool &discard_pool_for_current_thread(bool thread_safe = false);

  void context_register(VKContext &context);
  void context_unregister(VKContext &context);
  Span<std::reference_wrapper<VKContext>> contexts_get() const;

  void memory_statistics_get(int *r_total_mem_kb, int *r_free_mem_kb) const;
  static void debug_print(std::ostream &os, const VKDiscardPool &discard_pool);
  void debug_print();

  void free_command_pool_buffers(VkCommandPool vk_command_pool);

  /** \} */

 private:
  void init_physical_device_properties();
  void init_physical_device_memory_properties();
  void init_physical_device_features();
  void init_physical_device_extensions();
  void init_debug_callbacks();
  void init_memory_allocator();
  /**
   * Initialize the functions struct with extension specific function pointer.
   */
  void init_functions();

  /**
   * Initialize a dummy buffer that can be bound for missing attributes.
   */
  void init_dummy_buffer();

  /* During initialization the backend requires access to update the workarounds. */
  friend VKBackend;
};

}  // namespace blender::gpu
