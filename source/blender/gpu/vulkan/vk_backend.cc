/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <sstream>

#include "GHOST_C-api.h"

#include "BLI_path_utils.hh"
#include "BLI_threads.h"

#include "CLG_log.h"

#include "GPU_capabilities.hh"
#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_fence.hh"
#include "vk_framebuffer.hh"
#include "vk_ghost_api.hh"
#include "vk_index_buffer.hh"
#include "vk_pixel_buffer.hh"
#include "vk_query.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

#include "vk_backend.hh"

static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {

static const char *vk_extension_get(int index)
{
  return VKBackend::get().device.extension_name_get(index);
}

bool GPU_vulkan_is_supported_driver(VkPhysicalDevice vk_physical_device)
{
  /* Check for known faulty drivers. */
  VkPhysicalDeviceProperties2 vk_physical_device_properties = {};
  VkPhysicalDeviceDriverProperties vk_physical_device_driver_properties = {};
  vk_physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  vk_physical_device_driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
  vk_physical_device_properties.pNext = &vk_physical_device_driver_properties;
  vkGetPhysicalDeviceProperties2(vk_physical_device, &vk_physical_device_properties);
  uint32_t conformance_version = VK_MAKE_API_VERSION(
      vk_physical_device_driver_properties.conformanceVersion.major,
      vk_physical_device_driver_properties.conformanceVersion.minor,
      vk_physical_device_driver_properties.conformanceVersion.subminor,
      vk_physical_device_driver_properties.conformanceVersion.patch);

  /* Intel IRIS on 10th gen CPU (and older) crashes due to multiple driver issues.
   *
   * 1) Workbench is working, but EEVEE pipelines are failing. Calling vkCreateGraphicsPipelines
   * for certain EEVEE shaders (Shadow, Deferred rendering) would return with VK_SUCCESS, but
   * without a created VkPipeline handle.
   *
   * 2) When vkCmdBeginRendering is called some requirements need to be met, that can only be met
   * when actually calling a vkCmdDraw* command. According to the Vulkan specs the requirements
   * should only be met when calling a vkCmdDraw* command.
   */
  if (vk_physical_device_driver_properties.driverID == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS &&
      vk_physical_device_properties.properties.deviceType ==
          VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
      conformance_version < VK_MAKE_API_VERSION(1, 3, 2, 0))
  {
    return false;
  }

#ifndef _WIN32
  /* NVIDIA drivers below 550 don't work on Linux. When sending command to the GPU there is not
   * always a reply back when they are finished. The issue is reported on the Internet many times,
   * but there is no mention of a solution. This means that on Linux we can only support GTX900 and
   * or use MesaNVK.
   */
  if (vk_physical_device_driver_properties.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY &&
      conformance_version < VK_MAKE_API_VERSION(1, 3, 7, 2))
  {
    return false;
  }

  /* NVIDIA driver 580.76.05 doesn't start using specific Wayland configurations #144625. There are
   * multiple reports also not Blender related and NVIDIA mentions that a new driver will be
   * released. It is unclear if that driver will fix our issue. For now disabling this driver on
   * Linux. This also disables it for configurations that are working as well (including X11). */
  if (vk_physical_device_driver_properties.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY &&
      ((StringRefNull(vk_physical_device_driver_properties.driverInfo).find("580.76.5", 0) !=
        StringRef::not_found) ||
       (StringRefNull(vk_physical_device_driver_properties.driverInfo).find("580.76.05", 0) !=
        StringRef::not_found)))
  {
    return false;
  }
#endif

#ifdef _WIN32
  if (vk_physical_device_driver_properties.driverID == VK_DRIVER_ID_QUALCOMM_PROPRIETARY) {
    /* Any Qualcomm driver older than 31.0.112.0 will not be capable of running blender due
     * to an issue in their semaphore timeline implementation. The driver could return
     * timelines that have not been provided by Blender. As Blender uses timelines for resource
     * management this resulted in resources to be destroyed, that are still in use. */

    /* Public version 31.0.112 uses vulkan driver version 512.827.0. */
    const uint32_t driver_version = vk_physical_device_properties.properties.driverVersion;
    constexpr uint32_t version_31_0_112 = VK_MAKE_VERSION(512, 827, 0);
    if (driver_version < version_31_0_112) {
      CLOG_WARN(&LOG,
                "Detected qualcomm driver is not supported. To run the Vulkan backend "
                "driver 31.0.112.0 or later is required. Switching to OpenGL.");
      return false;
    }
  }
#endif

  return true;
}

static Vector<StringRefNull> missing_capabilities_get(VkPhysicalDevice vk_physical_device)
{
  Vector<StringRefNull> missing_capabilities;
  /* Check device features. */
  VkPhysicalDeviceVulkan12Features features_12 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  VkPhysicalDeviceVulkan11Features features_11 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &features_12};
  VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                        &features_11};

  vkGetPhysicalDeviceFeatures2(vk_physical_device, &features);

#ifndef __APPLE__
  if (features.features.geometryShader == VK_FALSE) {
    missing_capabilities.append("geometry shaders");
  }
  if (features.features.logicOp == VK_FALSE) {
    missing_capabilities.append("logical operations");
  }
#endif
  if (features.features.dualSrcBlend == VK_FALSE) {
    missing_capabilities.append("dual source blending");
  }
  if (features.features.imageCubeArray == VK_FALSE) {
    missing_capabilities.append("image cube array");
  }
  if (features.features.multiDrawIndirect == VK_FALSE) {
    missing_capabilities.append("multi draw indirect");
  }
  if (features.features.multiViewport == VK_FALSE) {
    missing_capabilities.append("multi viewport");
  }
  if (features.features.shaderClipDistance == VK_FALSE) {
    missing_capabilities.append("shader clip distance");
  }
  if (features.features.drawIndirectFirstInstance == VK_FALSE) {
    missing_capabilities.append("draw indirect first instance");
  }
  if (features.features.fragmentStoresAndAtomics == VK_FALSE) {
    missing_capabilities.append("fragment stores and atomics");
  }
  if (features.features.vertexPipelineStoresAndAtomics == VK_FALSE) {
    missing_capabilities.append("vertex pipeline stores and atomics");
  }
  if (features_11.shaderDrawParameters == VK_FALSE) {
    missing_capabilities.append("shader draw parameters");
  }
  if (features_12.timelineSemaphore == VK_FALSE) {
    missing_capabilities.append("timeline semaphores");
  }
  if (features_12.bufferDeviceAddress == VK_FALSE) {
    missing_capabilities.append("buffer device address");
  }

  /* Check device extensions. */
  uint32_t vk_extension_count;
  vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr, &vk_extension_count, nullptr);

  Array<VkExtensionProperties> vk_extensions(vk_extension_count);
  vkEnumerateDeviceExtensionProperties(
      vk_physical_device, nullptr, &vk_extension_count, vk_extensions.data());
  Set<StringRefNull> extensions;
  for (VkExtensionProperties &vk_extension : vk_extensions) {
    extensions.add(vk_extension.extensionName);
  }

  if (!extensions.contains(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    missing_capabilities.append(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
  if (!extensions.contains(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
    missing_capabilities.append(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
  }
#ifndef __APPLE__
  /* Metal doesn't support provoking vertex. */
  if (!extensions.contains(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME)) {
    missing_capabilities.append(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
  }
#endif

  return missing_capabilities;
}

bool VKBackend::is_supported()
{
  CLG_logref_init(&LOG);

  /*
   * Disable implicit layers and only allow layers that we trust.
   *
   * Render doc layer is hidden behind a debug flag. There are malicious layers that impersonate
   * RenderDoc and can crash when loaded. See #139543
   */
  std::stringstream allowed_layers;
  allowed_layers << "VK_LAYER_KHRONOS_*";
  allowed_layers << ",VK_LAYER_AMD_*";
  allowed_layers << ",VK_LAYER_INTEL_*";
  allowed_layers << ",VK_LAYER_NV_*";
  allowed_layers << ",VK_LAYER_MESA_*";
  if (bool(G.debug & G_DEBUG_GPU)) {
    allowed_layers << ",VK_LAYER_LUNARG_*";
    allowed_layers << ",VK_LAYER_RENDERDOC_*";
  }
  BLI_setenv("VK_LOADER_LAYERS_DISABLE", "~implicit~");
  BLI_setenv("VK_LOADER_LAYERS_ALLOW", allowed_layers.str().c_str());

  /* Initialize an vulkan 1.2 instance. */
  VkApplicationInfo vk_application_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  vk_application_info.pApplicationName = "Blender";
  vk_application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  vk_application_info.pEngineName = "Blender";
  vk_application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  vk_application_info.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo vk_instance_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  vk_instance_info.pApplicationInfo = &vk_application_info;

  VkInstance vk_instance = VK_NULL_HANDLE;
  vkCreateInstance(&vk_instance_info, nullptr, &vk_instance);
  if (vk_instance == VK_NULL_HANDLE) {
    CLOG_ERROR(&LOG, "Unable to initialize a Vulkan 1.2 instance.");
    return false;
  }

  /* Go over all the devices. */
  uint32_t physical_devices_count = 0;
  vkEnumeratePhysicalDevices(vk_instance, &physical_devices_count, nullptr);
  Array<VkPhysicalDevice> vk_physical_devices(physical_devices_count);
  vkEnumeratePhysicalDevices(vk_instance, &physical_devices_count, vk_physical_devices.data());

  for (VkPhysicalDevice vk_physical_device : vk_physical_devices) {
    VkPhysicalDeviceProperties vk_properties = {};
    vkGetPhysicalDeviceProperties(vk_physical_device, &vk_properties);

    if (!GPU_vulkan_is_supported_driver(vk_physical_device)) {
      CLOG_WARN(&LOG,
                "Installed driver for device [%s] has known issues and will not be used. Updating "
                "driver might improve compatibility.",
                vk_properties.deviceName);
      continue;
    }
    Vector<StringRefNull> missing_capabilities = missing_capabilities_get(vk_physical_device);

    /* Report result. */
    if (missing_capabilities.is_empty()) {
      /* This device meets minimum requirements. */
      CLOG_DEBUG(
          &LOG,
          "Device [%s] supports minimum requirements. Skip checking other GPUs. Another GPU "
          "can still be selected during auto-detection.",
          vk_properties.deviceName);

      vkDestroyInstance(vk_instance, nullptr);
      return true;
    }

    std::stringstream ss;
    ss << "Device [" << vk_properties.deviceName
       << "] does not meet minimum requirements. Missing features are [";
    for (StringRefNull &feature : missing_capabilities) {
      ss << feature << ", ";
    }
    ss.seekp(-2, std::ios_base::end);
    ss << "]";
    CLOG_WARN(&LOG, "%s", ss.str().c_str());
  }

  /* No device found meeting the minimum requirements. */

  vkDestroyInstance(vk_instance, nullptr);
  CLOG_ERROR(&LOG,
             "No Vulkan device found that meets the minimum requirements. "
             "Updating GPU driver can improve compatibility.");
  return false;
}

static GPUOSType determine_os_type()
{
#ifdef _WIN32
  return GPU_OS_WIN;
#elif defined(__APPLE__)
  return GPU_OS_MAC;
#else
  return GPU_OS_UNIX;
#endif
}

void VKBackend::platform_init()
{
  GPG.init(GPU_DEVICE_ANY,
           determine_os_type(),
           GPU_DRIVER_ANY,
           GPU_SUPPORT_LEVEL_SUPPORTED,
           GPU_BACKEND_VULKAN,
           "",
           "",
           "",
           GPU_ARCHITECTURE_IMR);
}

static void init_device_list(GHOST_ContextHandle ghost_context)
{
  GHOST_VulkanHandles vulkan_handles = {};
  GHOST_GetVulkanHandles(ghost_context, &vulkan_handles);

  uint32_t physical_devices_count = 0;
  vkEnumeratePhysicalDevices(vulkan_handles.instance, &physical_devices_count, nullptr);
  Array<VkPhysicalDevice> vk_physical_devices(physical_devices_count);
  vkEnumeratePhysicalDevices(
      vulkan_handles.instance, &physical_devices_count, vk_physical_devices.data());
  int index = 0;
  for (VkPhysicalDevice vk_physical_device : vk_physical_devices) {
    if (missing_capabilities_get(vk_physical_device).is_empty() &&
        GPU_vulkan_is_supported_driver(vk_physical_device))
    {
      VkPhysicalDeviceProperties vk_properties = {};
      vkGetPhysicalDeviceProperties(vk_physical_device, &vk_properties);
      std::stringstream identifier;
      identifier << std::hex << vk_properties.vendorID << "/" << vk_properties.deviceID << "/"
                 << index;
      GPG.devices.append({identifier.str(),
                          index,
                          vk_properties.vendorID,
                          vk_properties.deviceID,
                          std::string(vk_properties.deviceName)});
    }
    index++;
  }

  std::sort(GPG.devices.begin(), GPG.devices.end(), [&](const GPUDevice &a, const GPUDevice &b) {
    if (a.name == b.name) {
      return a.index < b.index;
    }
    return a.name < b.name;
  });
}

void VKBackend::platform_init(const VKDevice &device)
{
  const VkPhysicalDeviceProperties &properties = device.physical_device_properties_get();

  GPUDeviceType device_type = device.device_type();
  GPUDriverType driver = device.driver_type();
  GPUOSType os = determine_os_type();
  GPUSupportLevel support_level = GPU_SUPPORT_LEVEL_SUPPORTED;

  std::string vendor_name = device.vendor_name();
  std::string driver_version = device.driver_version();

  /* GPG has already been initialized, but without a specific device. Calling init twice will
   * clear the list of devices. Making a copy of the device list and set it after initialization to
   * make sure the list isn't destroyed at this moment, but only when the backend is destroyed. */
  Vector<GPUDevice> devices = GPG.devices;
  GPG.init(device_type,
           os,
           driver,
           support_level,
           GPU_BACKEND_VULKAN,
           vendor_name.c_str(),
           properties.deviceName,
           driver_version.c_str(),
           GPU_ARCHITECTURE_IMR);
  GPG.devices = devices;

  const VkPhysicalDeviceIDProperties &id_properties = device.physical_device_id_properties_get();

  GPG.device_uuid = Array<uint8_t, 16>(Span<uint8_t>(id_properties.deviceUUID, VK_UUID_SIZE));

  if (id_properties.deviceLUIDValid) {
    GPG.device_luid = Array<uint8_t, 8>(Span<uint8_t>(id_properties.deviceUUID, VK_LUID_SIZE));
    GPG.device_luid_node_mask = id_properties.deviceNodeMask;
  }
  else {
    GPG.device_luid.reinitialize(0);
    GPG.device_luid_node_mask = 0;
  }

  CLOG_INFO(&LOG,
            "Using vendor [%s] device [%s] driver version [%s].",
            vendor_name.c_str(),
            device.vk_physical_device_properties_.deviceName,
            driver_version.c_str());
}

void VKBackend::detect_workarounds(VKDevice &device)
{
  VKWorkarounds workarounds;
  VKExtensions extensions;

  if (G.debug & G_DEBUG_GPU_FORCE_WORKAROUNDS) {
    printf("\n");
    printf("VK: Forcing workaround usage and disabling features and extensions.\n");
    printf("    Vendor: %s\n", device.vendor_name().c_str());
    printf("    Device: %s\n", device.physical_device_properties_get().deviceName);
    printf("    Driver: %s\n", device.driver_version().c_str());
    /* Force workarounds and disable extensions. */
    workarounds.not_aligned_pixel_formats = true;
    workarounds.vertex_formats.r8g8b8 = true;
    extensions.shader_output_layer = false;
    extensions.shader_output_viewport_index = false;
    extensions.fragment_shader_barycentric = false;
    extensions.dynamic_rendering_local_read = false;
    extensions.dynamic_rendering_unused_attachments = false;
    extensions.pageable_device_local_memory = false;
    extensions.wide_lines = false;
    GCaps.stencil_export_support = false;

    device.workarounds_ = workarounds;
    device.extensions_ = extensions;
    return;
  }

  extensions.shader_output_layer =
      device.physical_device_vulkan_12_features_get().shaderOutputLayer;
  extensions.shader_output_viewport_index =
      device.physical_device_vulkan_12_features_get().shaderOutputViewportIndex;
  extensions.wide_lines = device.physical_device_features_get().wideLines;
  extensions.fragment_shader_barycentric = device.supports_extension(
      VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
  extensions.dynamic_rendering_local_read = device.supports_extension(
      VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME);
  extensions.dynamic_rendering_unused_attachments = device.supports_extension(
      VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME);
  extensions.logic_ops = device.physical_device_features_get().logicOp;
  extensions.maintenance4 = device.supports_extension(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
  extensions.memory_priority = device.supports_extension(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
  extensions.pageable_device_local_memory = device.supports_extension(
      VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
#ifdef _WIN32
  extensions.external_memory = device.supports_extension(
      VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
#elif not defined(__APPLE__)
  extensions.external_memory = device.supports_extension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#else
  extensions.external_memory = false;
#endif

  /* AMD GPUs don't support texture formats that use are aligned to 24 or 48 bits. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY) ||
      GPU_type_matches(GPU_DEVICE_APPLE, GPU_OS_MAC, GPU_DRIVER_ANY))
  {
    workarounds.not_aligned_pixel_formats = true;
  }

  /* Only enable by default dynamic rendering local read on Qualcomm devices. NVIDIA, AMD and Intel
   * performance is better when disabled (20%). On Qualcomm devices the improvement can be
   * substantial (16% on shader_balls.blend).
   *
   * `--debug-gpu-vulkan-local-read` can be used to use dynamic rendering local read on any
   * supported platform.
   *
   * TODO: Check if bottleneck is during command building. If so we could fine-tune this after the
   * device command building landed (T132682).
   */
  if ((G.debug & G_DEBUG_GPU_FORCE_VULKAN_LOCAL_READ) == 0 &&
      !GPU_type_matches(GPU_DEVICE_QUALCOMM, GPU_OS_ANY, GPU_DRIVER_ANY))
  {
    extensions.dynamic_rendering_local_read = false;
  }

  VkFormatProperties format_properties = {};
  vkGetPhysicalDeviceFormatProperties(
      device.physical_device_get(), VK_FORMAT_R8G8B8_UNORM, &format_properties);
  workarounds.vertex_formats.r8g8b8 = (format_properties.bufferFeatures &
                                       VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == 0;

  device.workarounds_ = workarounds;
  device.extensions_ = extensions;
}

void VKBackend::platform_exit()
{
  GPG.clear();
  VKDevice &device = VKBackend::get().device;
  if (device.is_initialized()) {
    device.deinit();
  }
}

void VKBackend::init_resources()
{
  compiler_ = MEM_new<ShaderCompiler>(
      __func__, GPU_max_parallel_compilations(), GPUWorker::ContextType::Main);
}

void VKBackend::delete_resources()
{
  MEM_delete(compiler_);
}

void VKBackend::samplers_update()
{
  VKDevice &device = VKBackend::get().device;
  if (device.is_initialized()) {
    device.reinit();
  }
}

void VKBackend::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  VKContext &context = *VKContext::get();
  render_graph::VKResourceAccessInfo &resources = context.reset_and_get_access_info();
  render_graph::VKDispatchNode::CreateInfo dispatch_info(resources);
  context.update_pipeline_data(dispatch_info.dispatch_node.pipeline_data);
  dispatch_info.dispatch_node.group_count_x = groups_x_len;
  dispatch_info.dispatch_node.group_count_y = groups_y_len;
  dispatch_info.dispatch_node.group_count_z = groups_z_len;
  context.render_graph().add_node(dispatch_info);
}

void VKBackend::compute_dispatch_indirect(StorageBuf *indirect_buf)
{
  BLI_assert(indirect_buf);
  VKContext &context = *VKContext::get();
  VKStorageBuffer &indirect_buffer = *unwrap(indirect_buf);
  render_graph::VKResourceAccessInfo &resources = context.reset_and_get_access_info();
  render_graph::VKDispatchIndirectNode::CreateInfo dispatch_indirect_info(resources);
  context.update_pipeline_data(dispatch_indirect_info.dispatch_indirect_node.pipeline_data);
  dispatch_indirect_info.dispatch_indirect_node.buffer = indirect_buffer.vk_handle();
  dispatch_indirect_info.dispatch_indirect_node.offset = 0;
  context.render_graph().add_node(dispatch_indirect_info);
}

Context *VKBackend::context_alloc(void *ghost_window, void *ghost_context)
{
  if (ghost_window) {
    BLI_assert(ghost_context == nullptr);
    ghost_context = GHOST_GetDrawingContext((GHOST_WindowHandle)ghost_window);
  }

  BLI_assert(ghost_context != nullptr);
  if (!device.is_initialized()) {
    device.init(ghost_context);
    device.extensions_get().log();
    init_device_list((GHOST_ContextHandle)ghost_context);
  }

  VKContext *context = new VKContext(ghost_window, ghost_context);
  device.context_register(*context);
  GHOST_SetVulkanSwapBuffersCallbacks((GHOST_ContextHandle)ghost_context,
                                      VKContext::swap_buffer_draw_callback,
                                      VKContext::swap_buffer_acquired_callback,
                                      VKContext::openxr_acquire_framebuffer_image_callback,
                                      VKContext::openxr_release_framebuffer_image_callback);

  return context;
}

Batch *VKBackend::batch_alloc()
{
  return new VKBatch();
}

Fence *VKBackend::fence_alloc()
{
  return new VKFence();
}

FrameBuffer *VKBackend::framebuffer_alloc(const char *name)
{
  return new VKFrameBuffer(name);
}

IndexBuf *VKBackend::indexbuf_alloc()
{
  return new VKIndexBuffer();
}

PixelBuffer *VKBackend::pixelbuf_alloc(size_t size)
{
  return new VKPixelBuffer(size);
}

QueryPool *VKBackend::querypool_alloc()
{
  return new VKQueryPool();
}

Shader *VKBackend::shader_alloc(const char *name)
{
  return new VKShader(name);
}

Texture *VKBackend::texture_alloc(const char *name)
{
  return new VKTexture(name);
}

UniformBuf *VKBackend::uniformbuf_alloc(size_t size, const char *name)
{
  return new VKUniformBuffer(size, name);
}

StorageBuf *VKBackend::storagebuf_alloc(size_t size, GPUUsageType usage, const char *name)
{
  return new VKStorageBuffer(size, usage, name);
}

VertBuf *VKBackend::vertbuf_alloc()
{
  return new VKVertexBuffer();
}

void VKBackend::render_begin()
{
  VKThreadData &thread_data = device.current_thread_data();
  BLI_assert_msg(thread_data.rendering_depth >= 0, "Unbalanced `GPU_render_begin/end`");
  thread_data.rendering_depth += 1;
}

void VKBackend::render_end()
{
  VKThreadData &thread_data = device.current_thread_data();
  thread_data.rendering_depth -= 1;
  BLI_assert_msg(thread_data.rendering_depth >= 0, "Unbalanced `GPU_render_begin/end`");
  if (G.background) {
    if (thread_data.rendering_depth == 0) {
      VKContext *context = VKContext::get();
      if (context != nullptr) {
        context->flush();
      }
      std::scoped_lock lock(device.orphaned_data.mutex_get());
      device.orphaned_data.move_data(device.orphaned_data_render,
                                     device.orphaned_data.timeline_ + 1);
    }
  }

  /* When performing animation render we want to release any discarded resources during rendering
   * after each frame.
   */
  if (G.is_rendering && thread_data.rendering_depth == 0 && !BLI_thread_is_main()) {
    std::scoped_lock lock(device.orphaned_data.mutex_get());
    device.orphaned_data.move_data(device.orphaned_data_render,
                                   device.orphaned_data.timeline_ + 1);
  }
}

void VKBackend::render_step(bool force_resource_release)
{
  if (force_resource_release) {
    std::scoped_lock lock(device.orphaned_data.mutex_get());
    device.orphaned_data.move_data(device.orphaned_data_render,
                                   device.orphaned_data.timeline_ + 1);
  }
}

void VKBackend::capabilities_init(VKDevice &device)
{
  const VkPhysicalDeviceProperties &properties = device.physical_device_properties_get();
  const VkPhysicalDeviceLimits &limits = properties.limits;

  /* Reset all capabilities from previous context. */
  GCaps = {};
  GCaps.geometry_shader_support = true;
  GCaps.stencil_export_support = device.supports_extension(
      VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);

  GCaps.max_texture_size = max_ii(limits.maxImageDimension1D, limits.maxImageDimension2D);
  GCaps.max_texture_3d_size = min_uu(limits.maxImageDimension3D, INT_MAX);
  GCaps.max_buffer_texture_size = min_uu(limits.maxTexelBufferElements, UINT_MAX);
  GCaps.max_texture_layers = min_uu(limits.maxImageArrayLayers, INT_MAX);
  GCaps.max_textures = min_uu(limits.maxDescriptorSetSampledImages, INT_MAX);
  GCaps.max_textures_vert = GCaps.max_textures_geom = GCaps.max_textures_frag = min_uu(
      limits.maxPerStageDescriptorSampledImages, INT_MAX);
  GCaps.max_samplers = min_uu(limits.maxSamplerAllocationCount, INT_MAX);
  GCaps.max_images = min_uu(limits.maxPerStageDescriptorStorageImages, INT_MAX);
  for (int i = 0; i < 3; i++) {
    GCaps.max_work_group_count[i] = min_uu(limits.maxComputeWorkGroupCount[i], INT_MAX);
    GCaps.max_work_group_size[i] = min_uu(limits.maxComputeWorkGroupSize[i], INT_MAX);
  }
  GCaps.max_uniforms_vert = GCaps.max_uniforms_frag = min_uu(
      limits.maxPerStageDescriptorUniformBuffers, INT_MAX);
  GCaps.max_batch_indices = min_uu(limits.maxDrawIndirectCount, INT_MAX);
  GCaps.max_batch_vertices = min_uu(limits.maxDrawIndexedIndexValue, INT_MAX);
  GCaps.max_vertex_attribs = min_uu(limits.maxVertexInputAttributes, INT_MAX);
  GCaps.max_varying_floats = min_uu(limits.maxVertexOutputComponents, INT_MAX);
  GCaps.max_shader_storage_buffer_bindings = GCaps.max_compute_shader_storage_blocks = min_uu(
      limits.maxPerStageDescriptorStorageBuffers, INT_MAX);
  GCaps.max_uniform_buffer_size = size_t(limits.maxUniformBufferRange);
  GCaps.max_storage_buffer_size = size_t(limits.maxStorageBufferRange);
  GCaps.storage_buffer_alignment = limits.minStorageBufferOffsetAlignment;

  GCaps.max_parallel_compilations = BLI_system_thread_count();
  GCaps.mem_stats_support = true;

  uint32_t vk_extension_count;
  vkEnumerateDeviceExtensionProperties(
      device.physical_device_get(), nullptr, &vk_extension_count, nullptr);
  GCaps.extensions_len = vk_extension_count;
  GCaps.extension_get = vk_extension_get;

  detect_workarounds(device);
}

}  // namespace blender::gpu
