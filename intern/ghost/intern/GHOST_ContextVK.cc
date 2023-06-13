/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_ContextVK.hh"

#ifdef _WIN32
#  include <vulkan/vulkan_win32.h>
#elif defined(__APPLE__)
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else /* X11 */
#  include <vulkan/vulkan_xlib.h>
#  ifdef WITH_GHOST_WAYLAND
#    include <vulkan/vulkan_wayland.h>
#  endif
#endif

#include <vector>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>

#include <sys/stat.h>

/* Set to 0 to allow devices that do not have the required features.
 * This allows development on OSX until we really needs these features. */
#define STRICT_REQUIREMENTS true

/*
 * Should we only select surfaces that are known to be compatible. Or should we in case no
 * compatible surfaces have been found select the first one.
 *
 * Currently we also select incompatible surfaces as Vulkan is still experimental.  Assuming we get
 * reports of color differences between OpenGL and Vulkan to narrow down if there are other
 * configurations we need to support.
 */
#define SELECT_COMPATIBLE_SURFACES_ONLY false

using namespace std;

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

enum class VkLayer : uint8_t { KHRONOS_validation };

static bool vklayer_config_exist(const char *vk_extension_config)
{
  const char *ev_val = getenv("VK_LAYER_PATH");
  if (ev_val == nullptr) {
    return false;
  }
  std::stringstream filename;
  filename << ev_val;
  filename << "/" << vk_extension_config;
  struct stat buffer;
  return (stat(filename.str().c_str(), &buffer) == 0);
}

#define __STR(A) "" #A
#define VK_CHECK(__expression) \
  do { \
    VkResult r = (__expression); \
    if (r != VK_SUCCESS) { \
      fprintf(stderr, \
              "Vulkan Error : %s:%d : %s failled with %s\n", \
              __FILE__, \
              __LINE__, \
              __STR(__expression), \
              vulkan_error_as_string(r)); \
      return GHOST_kFailure; \
    } \
  } while (0)

#define DEBUG_PRINTF(...) \
  if (m_debug) { \
    printf(__VA_ARGS__); \
  }

/* Triple buffering. */
const int MAX_FRAMES_IN_FLIGHT = 2;

/* -------------------------------------------------------------------- */
/** \name Vulkan Device
 * \{ */

class GHOST_DeviceVK {
 public:
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;

  VkDevice device = VK_NULL_HANDLE;

  uint32_t generic_queue_family = 0;

  VkPhysicalDeviceProperties properties = {};
  VkPhysicalDeviceFeatures features = {};
  int users = 0;

 public:
  GHOST_DeviceVK(VkInstance vk_instance, VkPhysicalDevice vk_physical_device)
      : instance(vk_instance), physical_device(vk_physical_device)
  {
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vkGetPhysicalDeviceFeatures(physical_device, &features);
  }
  ~GHOST_DeviceVK()
  {
    if (device != VK_NULL_HANDLE) {
      vkDestroyDevice(device, NULL);
    }
  }

  void wait_idle()
  {
    if (device) {
      vkDeviceWaitIdle(device);
    }
  }

  bool extensions_support(const vector<const char *> &required_extensions)
  {
    uint32_t ext_count;
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &ext_count, NULL);

    vector<VkExtensionProperties> available_exts(ext_count);
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &ext_count, available_exts.data());

    for (const auto &extension_needed : required_extensions) {
      bool found = false;
      for (const auto &extension : available_exts) {
        if (strcmp(extension_needed, extension.extensionName) == 0) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
    }
    return true;
  }

  void ensure_device(vector<const char *> &layers_enabled, vector<const char *> &extensions_device)
  {
    if (device != VK_NULL_HANDLE) {
      return;
    }
    init_generic_queue_family();

    vector<VkDeviceQueueCreateInfo> queue_create_infos;

    float queue_priorities[] = {1.0f};
    VkDeviceQueueCreateInfo graphic_queue_create_info = {};
    graphic_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphic_queue_create_info.queueFamilyIndex = generic_queue_family;
    graphic_queue_create_info.queueCount = 1;
    graphic_queue_create_info.pQueuePriorities = queue_priorities;
    queue_create_infos.push_back(graphic_queue_create_info);

    VkPhysicalDeviceFeatures device_features = {};
#if STRICT_REQUIREMENTS
    device_features.geometryShader = VK_TRUE;
    device_features.dualSrcBlend = VK_TRUE;
    device_features.logicOp = VK_TRUE;
    device_features.imageCubeArray = VK_TRUE;
#endif

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    /* layers_enabled are the same as instance extensions.
     * This is only needed for 1.0 implementations. */
    device_create_info.enabledLayerCount = static_cast<uint32_t>(layers_enabled.size());
    device_create_info.ppEnabledLayerNames = layers_enabled.data();
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions_device.size());
    device_create_info.ppEnabledExtensionNames = extensions_device.data();
    device_create_info.pEnabledFeatures = &device_features;

    vkCreateDevice(physical_device, &device_create_info, NULL, &device);
  }

  void init_generic_queue_family()
  {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

    vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.data());

    generic_queue_family = 0;
    for (const auto &queue_family : queue_families) {
      /* Every vulkan implementation by spec must have one queue family that support both graphics
       * and compute pipelines. We select this one; compute only queue family hints at async
       * compute implementations. */
      if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
          (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT))
      {
        return;
      }
      generic_queue_family++;
    }

    fprintf(stderr, "Couldn't find any Graphic queue family on selected device\n");
    return;
  }
};

/**
 * A shared device between multiple contexts.
 *
 * The logical device needs to be shared as multiple contexts can be created and the logical vulkan
 * device they share should be the same otherwise memory operations might be done on the incorrect
 * device.
 */
static std::optional<GHOST_DeviceVK> vulkan_device;

static GHOST_TSuccess ensure_vulkan_device(VkInstance vk_instance,
                                           VkSurfaceKHR vk_surface,
                                           vector<const char *> required_extensions)
{
  if (vulkan_device.has_value()) {
    return GHOST_kSuccess;
  }

  VkPhysicalDevice best_physical_device = VK_NULL_HANDLE;

  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(vk_instance, &device_count, NULL);

  vector<VkPhysicalDevice> physical_devices(device_count);
  vkEnumeratePhysicalDevices(vk_instance, &device_count, physical_devices.data());

  int best_device_score = -1;
  for (const auto &physical_device : physical_devices) {
    GHOST_DeviceVK device_vk(vk_instance, physical_device);

    if (!device_vk.extensions_support(required_extensions)) {
      continue;
    }

    if (vk_surface != VK_NULL_HANDLE) {
      uint32_t format_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR(
          device_vk.physical_device, vk_surface, &format_count, NULL);

      uint32_t present_count;
      vkGetPhysicalDeviceSurfacePresentModesKHR(
          device_vk.physical_device, vk_surface, &present_count, NULL);

      /* For now anything will do. */
      if (format_count == 0 || present_count == 0) {
        continue;
      }
    }

#if STRICT_REQUIREMENTS
    if (!device_vk.features.geometryShader || !device_vk.features.dualSrcBlend ||
        !device_vk.features.logicOp || !device_vk.features.imageCubeArray)
    {
      continue;
    }
#endif

    int device_score = 0;
    switch (device_vk.properties.deviceType) {
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
    if (device_score > best_device_score) {
      best_physical_device = physical_device;
      best_device_score = device_score;
    }
  }

  if (best_physical_device == VK_NULL_HANDLE) {
    fprintf(stderr, "Error: No suitable Vulkan Device found!\n");
    return GHOST_kFailure;
  }

  vulkan_device = std::make_optional<GHOST_DeviceVK>(vk_instance, best_physical_device);

  return GHOST_kSuccess;
}

/** \} */

GHOST_ContextVK::GHOST_ContextVK(bool stereoVisual,
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
#endif
                                 int contextMajorVersion,
                                 int contextMinorVersion,
                                 int debug)
    : GHOST_Context(stereoVisual),
#ifdef _WIN32
      m_hwnd(hwnd),
#elif defined(__APPLE__)
      m_metal_layer(metal_layer),
#else
      m_platform(platform),
      /* X11 */
      m_display(display),
      m_window(window),
      /* Wayland */
      m_wayland_surface(wayland_surface),
      m_wayland_display(wayland_display),
#endif
      m_context_major_version(contextMajorVersion),
      m_context_minor_version(contextMinorVersion),
      m_debug(debug),
      m_command_pool(VK_NULL_HANDLE),
      m_surface(VK_NULL_HANDLE),
      m_swapchain(VK_NULL_HANDLE),
      m_render_pass(VK_NULL_HANDLE)
{
}

GHOST_ContextVK::~GHOST_ContextVK()
{
  if (vulkan_device.has_value()) {
    GHOST_DeviceVK &device_vk = *vulkan_device;
    device_vk.wait_idle();

    destroySwapchain();

    if (m_command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_vk.device, m_command_pool, NULL);
    }
    if (m_surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(device_vk.instance, m_surface, NULL);
    }

    device_vk.users--;
    if (device_vk.users == 0) {
      vulkan_device.reset();
    }
  }
}

GHOST_TSuccess GHOST_ContextVK::destroySwapchain()
{
  assert(vulkan_device.has_value() && vulkan_device->device != VK_NULL_HANDLE);
  VkDevice device = vulkan_device->device;

  for (auto semaphore : m_image_available_semaphores) {
    vkDestroySemaphore(device, semaphore, NULL);
  }
  for (auto semaphore : m_render_finished_semaphores) {
    vkDestroySemaphore(device, semaphore, NULL);
  }
  for (auto fence : m_in_flight_fences) {
    vkDestroyFence(device, fence, NULL);
  }
  for (auto framebuffer : m_swapchain_framebuffers) {
    vkDestroyFramebuffer(device, framebuffer, NULL);
  }
  if (m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_render_pass, NULL);
  }
  for (auto command_buffer : m_command_buffers) {
    vkFreeCommandBuffers(device, m_command_pool, 1, &command_buffer);
  }
  for (auto imageView : m_swapchain_image_views) {
    vkDestroyImageView(device, imageView, NULL);
  }
  if (m_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, m_swapchain, NULL);
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::swapBuffers()
{
  if (m_swapchain == VK_NULL_HANDLE) {
    return GHOST_kFailure;
  }

  if (m_lastFrame != m_currentFrame) {
    return GHOST_kSuccess;
  }

  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};

  /* Image should be in present src layout before presenting to screen. */
  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  VK_CHECK(vkBeginCommandBuffer(m_command_buffers[m_currentImage], &begin_info));
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.image = m_swapchain_images[m_currentImage];
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  vkCmdPipelineBarrier(m_command_buffers[m_currentImage],
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_DEPENDENCY_BY_REGION_BIT,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &barrier);
  VK_CHECK(vkEndCommandBuffer(m_command_buffers[m_currentImage]));

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &m_command_buffers[m_currentImage];
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &m_render_finished_semaphores[m_currentFrame];

  VkDevice device = vulkan_device->device;

  VkResult result;
  VK_CHECK(vkQueueSubmit(m_graphic_queue, 1, &submit_info, m_in_flight_fences[m_currentFrame]));
  do {
    result = vkWaitForFences(device, 1, &m_in_flight_fences[m_currentFrame], VK_TRUE, 10000);
  } while (result == VK_TIMEOUT);

  VK_CHECK(vkQueueWaitIdle(m_graphic_queue));

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &m_render_finished_semaphores[m_currentFrame];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &m_swapchain;
  present_info.pImageIndices = &m_currentImage;
  present_info.pResults = NULL;

  result = vkQueuePresentKHR(m_present_queue, &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    /* Swap-chain is out of date. Recreate swap-chain and skip this frame. */
    destroySwapchain();
    createSwapchain();
    return GHOST_kSuccess;
  }
  else if (result != VK_SUCCESS) {
    fprintf(stderr,
            "Error: Failed to present swap chain image : %s\n",
            vulkan_error_as_string(result));
    return GHOST_kFailure;
  }

  m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  vkResetFences(device, 1, &m_in_flight_fences[m_currentFrame]);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::getVulkanBackbuffer(
    void *image, void *framebuffer, void *render_pass, void *extent, uint32_t *fb_id)
{
  if (m_swapchain == VK_NULL_HANDLE) {
    return GHOST_kFailure;
  }

  if (m_currentFrame != m_lastFrame) {
    assert(vulkan_device.has_value() && vulkan_device->device != VK_NULL_HANDLE);
    VkDevice device = vulkan_device->device;
    vkAcquireNextImageKHR(device,
                          m_swapchain,
                          UINT64_MAX,
                          m_image_available_semaphores[m_currentFrame],
                          VK_NULL_HANDLE,
                          &m_currentImage);

    m_lastFrame = m_currentFrame;
  }

  *((VkImage *)image) = m_swapchain_images[m_currentImage];
  *((VkFramebuffer *)framebuffer) = m_swapchain_framebuffers[m_currentImage];
  *((VkRenderPass *)render_pass) = m_render_pass;
  *((VkExtent2D *)extent) = m_render_extent;
  *fb_id = m_swapchain_id * 10 + m_currentFrame;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::getVulkanHandles(void *r_instance,
                                                 void *r_physical_device,
                                                 void *r_device,
                                                 uint32_t *r_graphic_queue_family,
                                                 void *r_queue)
{
  *((VkInstance *)r_instance) = VK_NULL_HANDLE;
  *((VkPhysicalDevice *)r_physical_device) = VK_NULL_HANDLE;
  *((VkDevice *)r_device) = VK_NULL_HANDLE;

  if (vulkan_device.has_value()) {
    *((VkInstance *)r_instance) = vulkan_device->instance;
    *((VkPhysicalDevice *)r_physical_device) = vulkan_device->physical_device;
    *((VkDevice *)r_device) = vulkan_device->device;
    *r_graphic_queue_family = vulkan_device->generic_queue_family;
  }

  *((VkQueue *)r_queue) = m_graphic_queue;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::getVulkanCommandBuffer(void *r_command_buffer)
{
  if (m_command_buffers.empty()) {
    return GHOST_kFailure;
  }

  if (m_swapchain == VK_NULL_HANDLE) {
    *((VkCommandBuffer *)r_command_buffer) = m_command_buffers[0];
  }
  else {
    *((VkCommandBuffer *)r_command_buffer) = m_command_buffers[m_currentImage];
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::activateDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::releaseDrawingContext()
{
  return GHOST_kSuccess;
}

static vector<VkExtensionProperties> getExtensionsAvailable()
{
  uint32_t extension_count = 0;
  vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);

  vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extensions.data());

  return extensions;
}

static bool checkExtensionSupport(vector<VkExtensionProperties> &extensions_available,
                                  const char *extension_name)
{
  for (const auto &extension : extensions_available) {
    if (strcmp(extension_name, extension.extensionName) == 0) {
      return true;
    }
  }
  return false;
}

static void requireExtension(vector<VkExtensionProperties> &extensions_available,
                             vector<const char *> &extensions_enabled,
                             const char *extension_name)
{
  if (checkExtensionSupport(extensions_available, extension_name)) {
    extensions_enabled.push_back(extension_name);
  }
  else {
    fprintf(stderr, "Error: %s not found.\n", extension_name);
  }
}

static vector<VkLayerProperties> getLayersAvailable()
{
  uint32_t layer_count = 0;
  vkEnumerateInstanceLayerProperties(&layer_count, NULL);

  vector<VkLayerProperties> layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

  return layers;
}

static bool checkLayerSupport(vector<VkLayerProperties> &layers_available, const char *layer_name)
{
  for (const auto &layer : layers_available) {
    if (strcmp(layer_name, layer.layerName) == 0) {
      return true;
    }
  }
  return false;
}

static void enableLayer(vector<VkLayerProperties> &layers_available,
                        vector<const char *> &layers_enabled,
                        const VkLayer layer,
                        const bool display_warning)
{
#define PUSH_VKLAYER(name, name2) \
  if (vklayer_config_exist("VkLayer_" #name ".json") && \
      checkLayerSupport(layers_available, "VK_LAYER_" #name2)) \
  { \
    layers_enabled.push_back("VK_LAYER_" #name2); \
    enabled = true; \
  } \
  else { \
    warnings << "VK_LAYER_" #name2; \
  }

  bool enabled = false;
  std::stringstream warnings;

  switch (layer) {
    case VkLayer::KHRONOS_validation:
      PUSH_VKLAYER(khronos_validation, KHRONOS_validation);
  };

  if (enabled) {
    return;
  }

  if (display_warning) {
    fprintf(stderr,
            "Warning: Layer requested, but not supported by the platform. [%s] \n",
            warnings.str().c_str());
  }

#undef PUSH_VKLAYER
}

static GHOST_TSuccess create_render_pass(VkDevice device,
                                         VkFormat format,
                                         VkRenderPass *r_renderPass)
{
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = format;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, NULL, r_renderPass));

  return GHOST_kSuccess;
}

static GHOST_TSuccess selectPresentMode(VkPhysicalDevice device,
                                        VkSurfaceKHR surface,
                                        VkPresentModeKHR *r_presentMode)
{
  // TODO cleanup: we are not going to use MAILBOX as it isn't supported by renderdoc.
  uint32_t present_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count, NULL);
  vector<VkPresentModeKHR> presents(present_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count, presents.data());
  /* MAILBOX is the lowest latency V-Sync enabled mode so use it if available */
  for (auto present_mode : presents) {
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR) {
      *r_presentMode = present_mode;
      return GHOST_kSuccess;
    }
  }
  /* FIFO present mode is always available. */
  for (auto present_mode : presents) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      *r_presentMode = present_mode;
      return GHOST_kSuccess;
    }
  }

  fprintf(stderr, "Error: FIFO present mode is not supported by the swap chain!\n");

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextVK::createCommandPools()
{
  assert(vulkan_device.has_value() && vulkan_device->device != VK_NULL_HANDLE);
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = vulkan_device->generic_queue_family;

  VK_CHECK(vkCreateCommandPool(vulkan_device->device, &poolInfo, NULL, &m_command_pool));
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::createGraphicsCommandBuffer()
{
  assert(vulkan_device.has_value() && vulkan_device->device != VK_NULL_HANDLE);
  assert(m_command_pool != VK_NULL_HANDLE);
  assert(m_command_buffers.size() == 0);
  m_command_buffers.resize(1);
  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = m_command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

  VK_CHECK(vkAllocateCommandBuffers(vulkan_device->device, &alloc_info, m_command_buffers.data()));
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::createGraphicsCommandBuffers()
{
  assert(vulkan_device.has_value() && vulkan_device->device != VK_NULL_HANDLE);
  assert(m_command_pool != VK_NULL_HANDLE);
  m_command_buffers.resize(m_swapchain_image_views.size());

  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = m_command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

  VK_CHECK(vkAllocateCommandBuffers(vulkan_device->device, &alloc_info, m_command_buffers.data()));
  return GHOST_kSuccess;
}

static bool surfaceFormatSupported(const VkSurfaceFormatKHR &surface_format)
{
  if (surface_format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
    return false;
  }

  if (surface_format.format == VK_FORMAT_R8G8B8A8_UNORM ||
      surface_format.format == VK_FORMAT_B8G8R8A8_UNORM)
  {
    return true;
  }

  return false;
}

/**
 * Select the surface format that we will use.
 *
 * We will select any 8bit UNORM surface.
 */
static bool selectSurfaceFormat(const VkPhysicalDevice physical_device,
                                const VkSurfaceKHR surface,
                                VkSurfaceFormatKHR &r_surfaceFormat)
{
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, NULL);
  vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());

  for (VkSurfaceFormatKHR &format : formats) {
    if (surfaceFormatSupported(format)) {
      r_surfaceFormat = format;
      return true;
    }
  }

#if !SELECT_COMPATIBLE_SURFACES_ONLY
  r_surfaceFormat = formats[0];
#endif

  return false;
}

GHOST_TSuccess GHOST_ContextVK::createSwapchain()
{
  assert(vulkan_device.has_value() && vulkan_device->device != VK_NULL_HANDLE);
  m_swapchain_id++;

  VkPhysicalDevice physical_device = vulkan_device->physical_device;

  VkSurfaceFormatKHR format = {};
#if SELECT_COMPATIBLE_SURFACES_ONLY
  if (!selectSurfaceFormat(physical_device, m_surface, format)) {
    return GHOST_kFailure;
  }
#else
  selectSurfaceFormat(physical_device, m_surface, format);
#endif

  VkPresentModeKHR present_mode;
  if (!selectPresentMode(physical_device, m_surface, &present_mode)) {
    return GHOST_kFailure;
  }

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, m_surface, &capabilities);

  m_render_extent = capabilities.currentExtent;
  if (m_render_extent.width == UINT32_MAX) {
    /* Window Manager is going to set the surface size based on the given size.
     * Choose something between minImageExtent and maxImageExtent. */
    m_render_extent.width = 1280;
    m_render_extent.height = 720;
    if (capabilities.minImageExtent.width > m_render_extent.width) {
      m_render_extent.width = capabilities.minImageExtent.width;
    }
    if (capabilities.minImageExtent.height > m_render_extent.height) {
      m_render_extent.height = capabilities.minImageExtent.height;
    }
  }

  /* Driver can stall if only using minimal image count. */
  uint32_t image_count = capabilities.minImageCount + 1;
  /* Note: maxImageCount == 0 means no limit. */
  if (image_count > capabilities.maxImageCount && capabilities.maxImageCount > 0) {
    image_count = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = m_surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = format.format;
  create_info.imageColorSpace = format.colorSpace;
  create_info.imageExtent = m_render_extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  create_info.preTransform = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE; /* TODO Window resize */
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 0;
  create_info.pQueueFamilyIndices = NULL;

  VkDevice device = vulkan_device->device;
  VK_CHECK(vkCreateSwapchainKHR(device, &create_info, NULL, &m_swapchain));

  create_render_pass(device, format.format, &m_render_pass);

  /* image_count may not be what we requested! Getter for final value. */
  vkGetSwapchainImagesKHR(device, m_swapchain, &image_count, NULL);
  m_swapchain_images.resize(image_count);
  vkGetSwapchainImagesKHR(device, m_swapchain, &image_count, m_swapchain_images.data());

  m_swapchain_image_views.resize(image_count);
  m_swapchain_framebuffers.resize(image_count);
  for (int i = 0; i < image_count; i++) {
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_swapchain_images[i];
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = format.format;
    view_create_info.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(device, &view_create_info, NULL, &m_swapchain_image_views[i]));

    VkImageView attachments[] = {m_swapchain_image_views[i]};

    VkFramebufferCreateInfo fb_create_info = {};
    fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_create_info.renderPass = m_render_pass;
    fb_create_info.attachmentCount = 1;
    fb_create_info.pAttachments = attachments;
    fb_create_info.width = m_render_extent.width;
    fb_create_info.height = m_render_extent.height;
    fb_create_info.layers = 1;

    VK_CHECK(vkCreateFramebuffer(device, &fb_create_info, NULL, &m_swapchain_framebuffers[i]));
  }

  m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphore_info = {};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

    VK_CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &m_image_available_semaphores[i]));
    VK_CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &m_render_finished_semaphores[i]));

    VK_CHECK(vkCreateFence(device, &fence_info, NULL, &m_in_flight_fences[i]));
  }

  createGraphicsCommandBuffers();

  return GHOST_kSuccess;
}

const char *GHOST_ContextVK::getPlatformSpecificSurfaceExtension() const
{
#ifdef _WIN32
  return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__APPLE__)
  return VK_EXT_METAL_SURFACE_EXTENSION_NAME;
#else /* UNIX/Linux */
  switch (m_platform) {
    case GHOST_kVulkanPlatformX11:
      return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
      break;
#  ifdef WITH_GHOST_WAYLAND
    case GHOST_kVulkanPlatformWayland:
      return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
      break;
#  endif
  }
#endif
  return NULL;
}

GHOST_TSuccess GHOST_ContextVK::initializeDrawingContext()
{
#ifdef _WIN32
  const bool use_window_surface = (m_hwnd != NULL);
#elif defined(__APPLE__)
  const bool use_window_surface = (m_metal_layer != NULL);
#else /* UNIX/Linux */
  bool use_window_surface = false;
  switch (m_platform) {
    case GHOST_kVulkanPlatformX11:
      use_window_surface = (m_display != NULL) && (m_window != (Window)NULL);
      break;
#  ifdef WITH_GHOST_WAYLAND
    case GHOST_kVulkanPlatformWayland:
      use_window_surface = (m_wayland_display != NULL) && (m_wayland_surface != NULL);
      break;
#  endif
  }
#endif

  auto layers_available = getLayersAvailable();
  auto extensions_available = getExtensionsAvailable();

  vector<const char *> layers_enabled;
  vector<const char *> extensions_device;
  vector<const char *> extensions_enabled;

  if (m_debug) {
    enableLayer(layers_available, layers_enabled, VkLayer::KHRONOS_validation, m_debug);
    requireExtension(extensions_available, extensions_enabled, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  if (use_window_surface) {
    const char *native_surface_extension_name = getPlatformSpecificSurfaceExtension();

    requireExtension(extensions_available, extensions_enabled, "VK_KHR_surface");
    requireExtension(extensions_available, extensions_enabled, native_surface_extension_name);

    extensions_device.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
  extensions_device.push_back("VK_KHR_dedicated_allocation");
  extensions_device.push_back("VK_KHR_get_memory_requirements2");
  /* Enable MoltenVK required instance extensions. */
#ifdef VK_MVK_MOLTENVK_EXTENSION_NAME
  requireExtension(
      extensions_available, extensions_enabled, "VK_KHR_get_physical_device_properties2");
#endif

  VkInstance instance = VK_NULL_HANDLE;
  if (!vulkan_device.has_value()) {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Blender";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Blender";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(m_context_major_version, m_context_minor_version, 0);

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(layers_enabled.size());
    create_info.ppEnabledLayerNames = layers_enabled.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions_enabled.size());
    create_info.ppEnabledExtensionNames = extensions_enabled.data();
    VK_CHECK(vkCreateInstance(&create_info, NULL, &instance));
  }
  else {
    instance = vulkan_device->instance;
  }

  if (use_window_surface) {
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandle(NULL);
    surface_create_info.hwnd = m_hwnd;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surface_create_info, NULL, &m_surface));
#elif defined(__APPLE__)
    VkMetalSurfaceCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    info.pNext = NULL;
    info.flags = 0;
    info.pLayer = m_metal_layer;
    VK_CHECK(vkCreateMetalSurfaceEXT(instance, &info, nullptr, &m_surface));
#else
    switch (m_platform) {
      case GHOST_kVulkanPlatformX11: {
        VkXlibSurfaceCreateInfoKHR surface_create_info = {};
        surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        surface_create_info.dpy = m_display;
        surface_create_info.window = m_window;
        VK_CHECK(vkCreateXlibSurfaceKHR(instance, &surface_create_info, NULL, &m_surface));
        break;
      }
#  ifdef WITH_GHOST_WAYLAND
      case GHOST_kVulkanPlatformWayland: {
        VkWaylandSurfaceCreateInfoKHR surface_create_info = {};
        surface_create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surface_create_info.display = m_wayland_display;
        surface_create_info.surface = m_wayland_surface;
        VK_CHECK(vkCreateWaylandSurfaceKHR(instance, &surface_create_info, NULL, &m_surface));
        break;
      }
#  endif
    }

#endif
  }

  if (!ensure_vulkan_device(instance, m_surface, extensions_device)) {
    return GHOST_kFailure;
  }

#ifdef VK_MVK_MOLTENVK_EXTENSION_NAME
  /* According to the Vulkan specs, when `VK_KHR_portability_subset` is available it should be
   * enabled. See
   * https://vulkan.lunarg.com/doc/view/1.2.198.1/mac/1.2-extensions/vkspec.html#VUID-VkDeviceCreateInfo-pProperties-04451*/
  if (vulkan_device->extensions_support({VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME})) {
    extensions_device.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
  }
#endif
  vulkan_device->users++;
  vulkan_device->ensure_device(layers_enabled, extensions_device);

  vkGetDeviceQueue(
      vulkan_device->device, vulkan_device->generic_queue_family, 0, &m_graphic_queue);

  createCommandPools();
  if (use_window_surface) {
    vkGetDeviceQueue(
        vulkan_device->device, vulkan_device->generic_queue_family, 0, &m_present_queue);
    createSwapchain();
  }
  else {
    createGraphicsCommandBuffer();
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextVK::releaseNativeHandles()
{
  return GHOST_kSuccess;
}
