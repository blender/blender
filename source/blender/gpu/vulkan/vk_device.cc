/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <fmt/format.h>
#include <sstream>

#include "CLG_log.h"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_device.hh"
#include "vk_state_manager.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

#include "gpu_shader_dependency_private.hh"

#include "GPU_capabilities.hh"

#include "BLI_math_matrix_types.hh"

#include "GHOST_C-api.h"

static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {

void VKExtensions::log() const
{
  CLOG_DEBUG(&LOG,
             "Device features\n"
             " - [%c] shader output viewport index\n"
             " - [%c] shader output layer\n"
             " - [%c] fragment shader barycentric\n"
             " - [%c] wide lines\n"
             "Device extensions\n"
             " - [%c] dynamic rendering local read\n"
             " - [%c] dynamic rendering unused attachments\n"
             " - [%c] external memory\n"
             " - [%c] maintenance4\n"
             " - [%c] memory priority\n"
             " - [%c] pageable device local memory\n"
             " - [%c] shader stencil export",
             shader_output_viewport_index ? 'X' : ' ',
             shader_output_layer ? 'X' : ' ',
             fragment_shader_barycentric ? 'X' : ' ',
             wide_lines ? 'X' : ' ',
             dynamic_rendering_local_read ? 'X' : ' ',
             dynamic_rendering_unused_attachments ? 'X' : ' ',
             external_memory ? 'X' : ' ',
             maintenance4 ? 'X' : ' ',
             memory_priority ? 'X' : ' ',
             pageable_device_local_memory ? 'X' : ' ',
             GPU_stencil_export_support() ? 'X' : ' ');
}

void VKDevice::reinit()
{
  samplers_.free();
  samplers_.init();
}

void VKDevice::deinit()
{
  if (!is_initialized()) {
    return;
  }

  deinit_submission_pool();

  dummy_buffer.free();
  samplers_.free();
  GPU_SHADER_FREE_SAFE(vk_backbuffer_blit_sh_);

  orphaned_data_render.deinit(*this);
  orphaned_data.deinit(*this);
  {
    while (!thread_data_.is_empty()) {
      VKThreadData *thread_data = thread_data_.pop_last();
      delete thread_data;
    }
    thread_data_.clear();
  }
  pipelines.write_to_disk();
  pipelines.free_data();
  descriptor_set_layouts_.deinit();
  vma_pools.deinit(*this);
  mem_allocator_ = VK_NULL_HANDLE;

  while (!render_graphs_.is_empty()) {
    render_graph::VKRenderGraph *render_graph = render_graphs_.pop_last();
    MEM_delete<render_graph::VKRenderGraph>(render_graph);
  }

  debugging_tools_.deinit(vk_instance_);

  vk_instance_ = VK_NULL_HANDLE;
  vk_physical_device_ = VK_NULL_HANDLE;
  vk_device_ = VK_NULL_HANDLE;
  vk_queue_family_ = 0;
  vk_queue_ = VK_NULL_HANDLE;
  vk_physical_device_properties_ = {};
  glsl_vert_patch_.clear();
  glsl_frag_patch_.clear();
  glsl_geom_patch_.clear();
  glsl_comp_patch_.clear();
  is_initialized_ = false;
}

void VKDevice::init(void *ghost_context)
{
  BLI_assert(!is_initialized());
  GHOST_VulkanHandles handles = {};
  GHOST_GetVulkanHandles((GHOST_ContextHandle)ghost_context, &handles);
  vk_instance_ = handles.instance;
  vk_physical_device_ = handles.physical_device;
  vk_device_ = handles.device;
  vk_queue_family_ = handles.graphic_queue_family;
  vk_queue_ = handles.queue;
  mem_allocator_ = handles.vma_allocator;
  queue_mutex_ = static_cast<std::mutex *>(handles.queue_mutex);

  init_physical_device_extensions();
  init_physical_device_properties();
  init_physical_device_memory_properties();
  init_physical_device_features();
  VKBackend::platform_init(*this);
  VKBackend::capabilities_init(*this);
  init_functions();
  init_debug_callbacks();
  vma_pools.init(*this);
  pipelines.init();
  pipelines.read_from_disk();

  samplers_.init();
  init_dummy_buffer();

  debug::object_label(vk_handle(), "LogicalDevice");
  debug::object_label(vk_queue_, "GenericQueue");

  resources.use_dynamic_rendering_local_read = extensions_.dynamic_rendering_local_read;
  orphaned_data.timeline_ = 0;

  init_submission_pool();
  is_initialized_ = true;
}

void VKDevice::init_functions()
{
#define LOAD_FUNCTION(name) (PFN_##name) vkGetInstanceProcAddr(vk_instance_, STRINGIFY(name))
  /* VK_KHR_dynamic_rendering */
  functions.vkCmdBeginRendering = LOAD_FUNCTION(vkCmdBeginRenderingKHR);
  functions.vkCmdEndRendering = LOAD_FUNCTION(vkCmdEndRenderingKHR);

  /* VK_EXT_debug_utils */
  functions.vkCmdBeginDebugUtilsLabel = LOAD_FUNCTION(vkCmdBeginDebugUtilsLabelEXT);
  functions.vkCmdEndDebugUtilsLabel = LOAD_FUNCTION(vkCmdEndDebugUtilsLabelEXT);
  functions.vkSetDebugUtilsObjectName = LOAD_FUNCTION(vkSetDebugUtilsObjectNameEXT);
  functions.vkCreateDebugUtilsMessenger = LOAD_FUNCTION(vkCreateDebugUtilsMessengerEXT);
  functions.vkDestroyDebugUtilsMessenger = LOAD_FUNCTION(vkDestroyDebugUtilsMessengerEXT);

  if (extensions_.external_memory) {
#ifdef _WIN32
    /* VK_KHR_external_memory_win32 */
    functions.vkGetMemoryWin32Handle = LOAD_FUNCTION(vkGetMemoryWin32HandleKHR);
#elif not defined(__APPLE__)
    /* VK_KHR_external_memory_fd */
    functions.vkGetMemoryFd = LOAD_FUNCTION(vkGetMemoryFdKHR);
#endif
  }

#undef LOAD_FUNCTION
}

void VKDevice::init_debug_callbacks()
{
  debugging_tools_.init(vk_instance_);
}

void VKDevice::init_physical_device_properties()
{
  BLI_assert(vk_physical_device_ != VK_NULL_HANDLE);

  VkPhysicalDeviceProperties2 vk_physical_device_properties = {};
  vk_physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  vk_physical_device_driver_properties_.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
  vk_physical_device_id_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  vk_physical_device_properties.pNext = &vk_physical_device_driver_properties_;
  vk_physical_device_driver_properties_.pNext = &vk_physical_device_id_properties_;

  if (supports_extension(VK_KHR_MAINTENANCE_4_EXTENSION_NAME)) {
    vk_physical_device_maintenance4_properties_.pNext = vk_physical_device_properties.pNext;
    vk_physical_device_properties.pNext = &vk_physical_device_maintenance4_properties_;
  }

  vkGetPhysicalDeviceProperties2(vk_physical_device_, &vk_physical_device_properties);
  vk_physical_device_properties_ = vk_physical_device_properties.properties;
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

void VKDevice::init_physical_device_extensions()
{
  uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(vk_physical_device_, nullptr, &count, nullptr);
  device_extensions_ = Array<VkExtensionProperties>(count);
  vkEnumerateDeviceExtensionProperties(
      vk_physical_device_, nullptr, &count, device_extensions_.data());
}

bool VKDevice::supports_extension(const char *extension_name) const
{
  for (const VkExtensionProperties &vk_extension_properties : device_extensions_) {
    if (STREQ(vk_extension_properties.extensionName, extension_name)) {
      return true;
    }
  }
  return false;
}

void VKDevice::init_dummy_buffer()
{
  dummy_buffer.create(sizeof(float4x4),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                      1.0f);
  debug::object_label(dummy_buffer.vk_handle(), "DummyBuffer");
  /* Default dummy buffer. Set the 4th element to 1 to fix missing orcos. */
  float data[16] = {
      0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  dummy_buffer.update_immediately(static_cast<void *>(data));
}

shader::GeneratedSource VKDevice::extensions_define(StringRefNull stage_define) const
{
  std::stringstream ss;

  ss << "#version 450\n";
  {
    /* Required extension. */
    ss << "#extension GL_ARB_shader_draw_parameters : enable\n";
    ss << "#define GPU_ARB_shader_draw_parameters\n";
    ss << "#define gpu_BaseInstance (gl_BaseInstanceARB)\n";
  }
  ss << "#define GPU_ARB_clip_control\n";

  ss << "#define gl_VertexID gl_VertexIndex\n";
  ss << "#define gpu_InstanceIndex (gl_InstanceIndex)\n";
  ss << "#define gl_InstanceID (gpu_InstanceIndex - gpu_BaseInstance)\n";

  ss << "#extension GL_ARB_shader_viewport_layer_array: enable\n";
  if (GPU_stencil_export_support()) {
    ss << "#extension GL_ARB_shader_stencil_export: enable\n";
    ss << "#define GPU_ARB_shader_stencil_export 1\n";
  }
  if (extensions_.fragment_shader_barycentric) {
    ss << "#extension GL_EXT_fragment_shader_barycentric : require\n";
    ss << "#define gpu_BaryCoord gl_BaryCoordEXT\n";
    ss << "#define gpu_BaryCoordNoPersp gl_BaryCoordNoPerspEXT\n";
  }
  ss << stage_define;

  return shader::GeneratedSource{"gpu_shader_glsl_extension.glsl", {}, ss.str()};
}

std::string VKDevice::glsl_vertex_patch_get() const
{
  shader::GeneratedSourceList sources{extensions_define("#define GPU_VERTEX_SHADER\n")};
  return fmt::to_string(fmt::join(
      gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
}

std::string VKDevice::glsl_geometry_patch_get() const
{
  shader::GeneratedSourceList sources{extensions_define("#define GPU_GEOMETRY_SHADER\n")};
  return fmt::to_string(fmt::join(
      gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
}

std::string VKDevice::glsl_fragment_patch_get() const
{
  shader::GeneratedSourceList sources{extensions_define("#define GPU_FRAGMENT_SHADER\n")};
  return fmt::to_string(fmt::join(
      gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
}

std::string VKDevice::glsl_compute_patch_get() const
{
  shader::GeneratedSourceList sources{extensions_define("#define GPU_COMPUTE_SHADER\n")};
  return fmt::to_string(fmt::join(
      gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
}

/* -------------------------------------------------------------------- */
/** \name Platform/driver/device information
 * \{ */

constexpr int32_t PCI_ID_NVIDIA = 0x10de;
constexpr int32_t PCI_ID_INTEL = 0x8086;
constexpr int32_t PCI_ID_AMD = 0x1002;
constexpr int32_t PCI_ID_ATI = 0x1022;
constexpr int32_t PCI_ID_APPLE = 0x106b;

GPUDeviceType VKDevice::device_type() const
{
  switch (vk_physical_device_driver_properties_.driverID) {
    case VK_DRIVER_ID_AMD_PROPRIETARY:
    case VK_DRIVER_ID_AMD_OPEN_SOURCE:
    case VK_DRIVER_ID_MESA_RADV:
      return GPU_DEVICE_ATI;

    case VK_DRIVER_ID_NVIDIA_PROPRIETARY:
    case VK_DRIVER_ID_MESA_NVK:
      return GPU_DEVICE_NVIDIA;

    case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS:
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
      return GPU_DEVICE_INTEL;

    case VK_DRIVER_ID_QUALCOMM_PROPRIETARY:
      return GPU_DEVICE_QUALCOMM;

    case VK_DRIVER_ID_MOLTENVK:
      return GPU_DEVICE_APPLE;

    case VK_DRIVER_ID_MESA_LLVMPIPE:
      return GPU_DEVICE_SOFTWARE;

    default:
      return GPU_DEVICE_UNKNOWN;
  }

  return GPU_DEVICE_UNKNOWN;
}

GPUDriverType VKDevice::driver_type() const
{
  switch (vk_physical_device_driver_properties_.driverID) {
    case VK_DRIVER_ID_AMD_PROPRIETARY:
    case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS:
    case VK_DRIVER_ID_NVIDIA_PROPRIETARY:
    case VK_DRIVER_ID_QUALCOMM_PROPRIETARY:
      return GPU_DRIVER_OFFICIAL;

    case VK_DRIVER_ID_MOLTENVK:
    case VK_DRIVER_ID_AMD_OPEN_SOURCE:
    case VK_DRIVER_ID_MESA_RADV:
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
    case VK_DRIVER_ID_MESA_NVK:
      return GPU_DRIVER_OPENSOURCE;

    case VK_DRIVER_ID_MESA_LLVMPIPE:
      return GPU_DRIVER_SOFTWARE;

    default:
      return GPU_DRIVER_ANY;
  }

  return GPU_DRIVER_ANY;
}

std::string VKDevice::vendor_name() const
{
  /* Below 0x10000 are the PCI vendor IDs (https://pcisig.com/membership/member-companies) */
  if (vk_physical_device_properties_.vendorID < 0x10000) {
    switch (vk_physical_device_properties_.vendorID) {
      case PCI_ID_AMD:
      case PCI_ID_ATI:
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
  return StringRefNull(vk_physical_device_driver_properties_.driverName) + " " +
         StringRefNull(vk_physical_device_driver_properties_.driverInfo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VKThreadData
 * \{ */

VKThreadData::VKThreadData(VKDevice &device, pthread_t thread_id) : thread_id(thread_id)
{
  descriptor_pools.init(device);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resource management
 * \{ */

VKThreadData &VKDevice::current_thread_data()
{
  std::scoped_lock mutex(resources.mutex);
  pthread_t current_thread_id = pthread_self();

  for (VKThreadData *thread_data : thread_data_) {
    if (pthread_equal(thread_data->thread_id, current_thread_id)) {
      return *thread_data;
    }
  }

  VKThreadData *thread_data = new VKThreadData(*this, current_thread_id);
  thread_data_.append(thread_data);
  return *thread_data;
}

void VKDevice::context_register(VKContext &context)
{
  contexts_.append(std::reference_wrapper(context));
}

void VKDevice::context_unregister(VKContext &context)
{
  if (context.render_graph_.has_value()) {
    render_graph::VKRenderGraph &render_graph = context.render_graph();
    context.render_graph_.reset();
    BLI_assert_msg(render_graph.is_empty(),
                   "Unregistering a context that still has an unsubmitted render graph.");
    render_graph.reset();
    BLI_thread_queue_push(
        unused_render_graphs_, &render_graph, BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL);
  }
  {
    std::scoped_lock lock(orphaned_data.mutex_get());
    orphaned_data.move_data(context.discard_pool, timeline_value_ + 1);
  }

  contexts_.remove(contexts_.first_index_of(std::reference_wrapper(context)));
}
Span<std::reference_wrapper<VKContext>> VKDevice::contexts_get() const
{
  return contexts_;
};

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

/* -------------------------------------------------------------------- */
/** \name Debugging/statistics
 * \{ */

void VKDevice::debug_print(std::ostream &os, const VKDiscardPool &discard_pool)
{
  if (discard_pool.images_.is_empty() && discard_pool.buffers_.is_empty() &&
      discard_pool.image_views_.is_empty() && discard_pool.buffer_views_.is_empty() &&
      discard_pool.shader_modules_.is_empty() && discard_pool.pipeline_layouts_.is_empty() &&
      discard_pool.descriptor_pools_.is_empty())
  {
    return;
  }
  os << "  Discardable resources: ";
  if (!discard_pool.images_.is_empty()) {
    os << "VkImage=" << discard_pool.images_.size() << " ";
  }
  if (!discard_pool.image_views_.is_empty()) {
    os << "VkImageView=" << discard_pool.image_views_.size() << " ";
  }
  if (!discard_pool.buffers_.is_empty()) {
    os << "VkBuffer=" << discard_pool.buffers_.size() << " ";
  }
  if (!discard_pool.buffer_views_.is_empty()) {
    os << "VkBufferViews=" << discard_pool.buffer_views_.size() << " ";
  }
  if (!discard_pool.shader_modules_.is_empty()) {
    os << "VkShaderModule=" << discard_pool.shader_modules_.size() << " ";
  }
  if (!discard_pool.pipeline_layouts_.is_empty()) {
    os << "VkPipelineLayout=" << discard_pool.pipeline_layouts_.size() << " ";
  }
  if (!discard_pool.descriptor_pools_.is_empty()) {
    os << "VkDescriptorPool=" << discard_pool.descriptor_pools_.size();
  }
  os << "\n";
}

void VKDevice::debug_print()
{
  BLI_assert_msg(BLI_thread_is_main(),
                 "VKDevice::debug_print can only be called from the main thread.");

  resources.debug_print();
  std::ostream &os = std::cout;
  os << "Pipelines\n";
  os << " Graphics: " << pipelines.graphics_.size() << "\n";
  os << " Compute: " << pipelines.compute_.size() << "\n";
  os << "Descriptor sets\n";
  os << " VkDescriptorSetLayouts: " << descriptor_set_layouts_.size() << "\n";
  for (const VKThreadData *thread_data : thread_data_) {
    /* NOTE: Assumption that this is always called form the main thread. This could be solved by
     * keeping track of the main thread inside the thread data. */
    const bool is_main = pthread_equal(thread_data->thread_id, pthread_self());
    os << "ThreadData" << (is_main ? " (main-thread)" : "") << ")\n";
    os << " Rendering_depth: " << thread_data->rendering_depth << "\n";
  }
  os << "Discard pool\n";
  debug_print(os, orphaned_data);
  os << "Discard pool (render)\n";
  debug_print(os, orphaned_data_render);
  os << "\n";

  for (const std::reference_wrapper<VKContext> &context : contexts_) {
    os << " VKContext \n";
    debug_print(os, context.get().discard_pool);
  }

  int total_mem_kb;
  int free_mem_kb;
  memory_statistics_get(&total_mem_kb, &free_mem_kb);
  os << "\nMemory: total=" << total_mem_kb << ", free=" << free_mem_kb << "\n";
}

/** \} */

}  // namespace blender::gpu
