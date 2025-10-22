/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * The state of resources needs to be tracked on device level.
 *
 * The state that are being tracked include:
 * - Modification stamps: Each time a resource is modified, this stamp is increased. Inside the
 *   render graph nodes track the resources including this stamp.
 * - Image layouts: The layout of pixels of an image on the GPU depends on the command being
 *   executed. A certain `vkCmd*` requires the image to be in a certain layout. Using incorrect
 *   layouts could lead to rendering artifacts.
 * - Resource ownership: Resources that are externally managed (swap-chain or external) uses a
 *   different workflow as its state can be altered externally and needs to be reset.
 * - Read/Write access masks: To generate correct and performing pipeline barriers the src/dst
 *   access masks needs to be accurate and precise. When creating pipeline barriers the resource
 *   usage up to that point should be known and the resource usage from that point on.
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"

/**
 * Enable VK_RESOURCE_STATE_TRACKER_VALIDATION to perform a consistency check
 * on the state. The consistency check is time consuming and should only be
 * turned on when needed.
 */
// #define VK_RESOURCE_STATE_TRACKER_VALIDATION

namespace blender::gpu::render_graph {

class VKCommandBuilder;
struct VKRenderGraphLink;
class VKScheduler;

using ResourceHandle = uint64_t;

/**
 * ModificationStamp is used to track resource modifications.
 *
 * When a resource is modified it will generate a new stamp by incrementing the previous stamp
 * with 1. Consecutive reads should use this new stamp. The stamp stays active until the next
 * modification to the resources is added to any render graph.
 */
using ModificationStamp = uint64_t;

/**
 * Resource with a stamp.
 *
 * This struct represents an image or buffer (handle) and its modification stamp.
 */
struct ResourceWithStamp {
  ResourceHandle handle;
  ModificationStamp stamp;
};

/**
 * Enum containing the different resource types that are being tracked.
 */
enum class VKResourceType { NONE = (0 << 0), IMAGE = (1 << 0), BUFFER = (1 << 1) };
ENUM_OPERATORS(VKResourceType);

/**
 * State being tracked for a resource.
 */
struct VKResourceBarrierState {
  /** Last used access flags. Will be reset by the last write. Reads will accumulate flags. */
  VkAccessFlags vk_access = VK_ACCESS_NONE;
  /* Last known pipeline stage. Will be reset by the last write. Reads will accumulate flags. */
  VkPipelineStageFlags vk_pipeline_stages = VK_PIPELINE_STAGE_NONE;
  /** Last known image layout of an image resource. */
  VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  bool is_new_stamp() const
  {
    return bool(vk_access &
                (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
                 VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT));
  }
};

/**
 * Class to track resources.
 *
 * Resources are tracked on device level. Their are two kind of resources, namely buffers and
 * images. Each resource can have multiple versions; every time a resource is changed (written to)
 * a new version is tracked.
 */
class VKResourceStateTracker {
  /* When a command buffer is reset the resources are re-synced.
   * During the syncing the command builder attributes are resized to reduce reallocations. */
  friend class VKCommandBuilder;
  friend struct VKRenderGraphLink;
  friend class VKScheduler;

  /**
   * A render resource can be a buffer or an image that needs to be tracked during rendering.
   *
   * Resources needs to be tracked as usage can alter the content of the resource. For example an
   * image can be optimized for data transfer, or optimized for sampling which can use a different
   * pixel layout on the device.
   */
  struct Resource {
    /** Is this resource a buffer or an image. */
    VKResourceType type;
    union {
      struct {
        /** VkBuffer handle of the resource being tracked. */
        VkBuffer vk_buffer = VK_NULL_HANDLE;
      } buffer;

      struct {
        /** VkImage handle of the resource being tracked. */
        VkImage vk_image = VK_NULL_HANDLE;
        /** Do we need to track subresources (layers/mipmaps). */
        bool use_subresource_tracking = false;
      } image;
    };

    /** Current modification stamp of the resource. */
    ModificationStamp stamp = 0;

    /**
     * State tracking to ensure correct pipeline barriers and command creation.
     */
    VKResourceBarrierState barrier_state = {};

#ifndef NDEBUG
    const char *name = nullptr;
#endif

    /**
     * Check if the given resource handle subresources needs to be tracked.
     *
     * Returns true when
     * - handle is an image with subresource tracking enables.
     *
     * Returns false when
     * - handle isn't an image resource or
     * - handle doesn't have subresource tracking enabled.
     */
    bool use_subresource_tracking()
    {
      if (type == VKResourceType::BUFFER) {
        return false;
      }
      return image.use_subresource_tracking;
    }
  };

  Map<ResourceHandle, Resource> resources_;
  Vector<ResourceHandle> unused_handles_;
  Map<VkImage, ResourceHandle> image_resources_;
  Map<VkBuffer, ResourceHandle> buffer_resources_;

 public:
  /**
   * Device resource mutex
   *
   * The mutex is stored in resources due to:
   * - It protects resources and their state.
   * - Allowing test cases to do testing without setting up a device instance which requires ghost.
   * - Device instance isn't accessible in test cases.
   */
  Mutex mutex;

  /**
   * Register a buffer resource.
   *
   * When a buffer is created in VKBuffer, it needs to be registered in the device resources so the
   * resource state can be tracked during its lifetime.
   */
  void add_buffer(VkBuffer vk_buffer, const char *name = nullptr);

  /**
   * Register an image resource.
   *
   * When an image is created in VKTexture, it needs to be registered in the device resources so
   * the resource state can be tracked during its lifetime.
   */
  void add_image(VkImage vk_image, bool use_subresource_tracking, const char *name = nullptr);
  void add_swapchain_image(VkImage vk_image, const char *name = nullptr);

  /**
   * Remove an registered image.
   *
   * When a image is destroyed by calling `vmaDestroyImage`, a call to `remove_image` is needed to
   * unregister the resource from state tracking.
   */
  void remove_image(VkImage vk_image);

  /**
   * Remove an registered buffer.
   *
   * When a buffer is destroyed by calling `vmaDestroyBuffer`, a call to `remove_buffer` is needed
   * to unregister the resource from state tracking.
   */
  void remove_buffer(VkBuffer vk_buffer);

  /**
   * Return the current stamp of the resource, and increase the stamp.
   *
   * When a node writes to an image, this method is called to increase the stamp of the image.
   * The node that writes to the image will use the current stamp as its input, but generate a new
   * stamp for future nodes.
   *
   * This function is called when adding a node to the render graph, during building resource
   * dependencies. See `VKNodeInfo.build_links`
   */
  ResourceWithStamp get_image_and_increase_stamp(VkImage vk_image);

  /**
   * Return the current stamp of the resource, and increase the stamp.
   *
   * When a node writes to a buffer, this method is called to increase the stamp of the buffer.
   * The node that writes to the buffer will use the current stamp as its input, but generate the
   * new stamp for future nodes.
   *
   * This function is called when adding a node to the render graph, during building resource
   * dependencies. See `VKNodeInfo.build_links`
   */
  ResourceWithStamp get_buffer_and_increase_stamp(VkBuffer vk_buffer);

  /**
   * Return the current stamp of the resource.
   *
   * When a node reads from a buffer, this method is called to get the current stamp the buffer.
   *
   * This function is called when adding a node to the render graph, during building resource
   * dependencies. See `VKNodeInfo.build_links`
   */
  ResourceWithStamp get_buffer(VkBuffer vk_buffer) const;

  /**
   * Return the current stamp of the resource.
   *
   * When a node reads from an image, this method is called to get the current stamp the image.
   *
   * This function is called when adding a node to the render graph, during building resource
   * dependencies. See `VKNodeInfo.build_links`
   */
  ResourceWithStamp get_image(VkImage vk_image) const;

  /** Get the resource type for the given handle. */
  VKResourceType resource_type_get(ResourceHandle resource_handle) const
  {
    return resources_.lookup(resource_handle).type;
  }

  bool use_dynamic_rendering_local_read = true;

  void debug_print() const;

 private:
  void add_image(VkImage vk_image,
                 bool use_subresource_tracking,
                 VKResourceBarrierState barrier_state,
                 const char *name = nullptr);

  /**
   * Get the current stamp of the resource.
   */
  static ResourceWithStamp get_stamp(ResourceHandle handle, const Resource &resource);

  /**
   * Get the current stamp of the resource and increase the stamp.
   */
  static ResourceWithStamp get_and_increase_stamp(ResourceHandle handle, Resource &resource);

  ResourceHandle create_resource_slot();

#ifdef VK_RESOURCE_STATE_TRACKER_VALIDATION
  void validate() const;
#endif
};

}  // namespace blender::gpu::render_graph
