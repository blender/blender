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

#include "BLI_bit_vector.hh"
#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"

namespace blender::gpu::render_graph {

class VKCommandBuilder;
struct VKRenderGraphLink;
class VKScheduler;
class VKRenderGraph;

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
  friend class VKRenderGraph;

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

    int ref_count = 0;
    /** Current modification stamp of the resource. */
    ModificationStamp stamp = 0;

    /**
     * State tracking to ensure correct pipeline barriers and command creation.
     */
    VKResourceBarrierState barrier_state = {};

    /**
     * Views registered on this resource (image views for images, buffer views for buffers).
     */
    Vector<std::variant<VkImageView, VkBufferView>> views;

#ifndef NDEBUG
    std::string name;
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

  Vector<Resource> resources_;
  /**
   * VMA allocation of each tracked resource, indexed like `resources_`.
   *
   * `VK_NULL_HANDLE` when the allocation is owned elsewhere (aliased memory, swapchain) or
   * never set.
   */
  Vector<VmaAllocation> allocations_;
  Vector<ResourceHandle> unused_handles_;
  Map<VkImage, ResourceHandle> image_resources_;
  /**
   * \brief Resource handles with a zero ref count, not yet discarded.
   */
  Vector<ResourceHandle> discardable_handles_;

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
  ResourceHandle add_buffer(VkBuffer vk_buffer,
                            VmaAllocation allocation = VK_NULL_HANDLE,
                            const char *name = nullptr);

  /**
   * Register an image resource.
   *
   * When an image is created in VKTexture, it needs to be registered in the device resources so
   * the resource state can be tracked during its lifetime.
   */
  ResourceHandle add_image(VkImage vk_image,
                           bool use_subresource_tracking,
                           VmaAllocation allocation = VK_NULL_HANDLE,
                           const char *name = nullptr);

  /**
   * \brief Register an image resource that can have aliased memory.
   *
   * The aliased memory can still be in use and requires the image to wait for all commands to be
   * completed, before it can start writing to the aliased memory.
   */
  ResourceHandle add_aliased_image(VkImage vk_image,
                                   bool use_subresource_tracking,
                                   VmaAllocation allocation = VK_NULL_HANDLE,
                                   const char *name = nullptr);
  ResourceHandle add_swapchain_image(VkImage vk_image, const char *name = nullptr);

  /**
   * Register an image or buffer view on a resource; the view is destroyed together with it.
   */
  template<typename VkView> void register_view(ResourceHandle resource_handle, VkView vk_view)
  {
    std::scoped_lock lock(mutex);
    Resource &resource = get_resource(resource_handle);
    resource.views.append(vk_view);
  }

  /**
   * \brief Update the layout of an image that has been externally modified.
   *
   * 'vkTransitionImageLayout' changes the image layout. When used the image layout needs to be
   * updated to match the current layout, ensuring correct generation of pipeline barriers.
   *
   * \name image_handle: ResourceHandle of the Image to update the layout for.
   * \name vk_image_layout: The layout the resource state tracker should now be using, matching the
   *                        current layout of the image.
   */
  void update_image_layout(ResourceHandle image_handle, VkImageLayout vk_image_layout);

  /**
   * A resource that reached zero ref count, including its registered views. The caller owns
   * destruction (e.g. via a discard pool); the resource stays registered until then.
   */
  struct DiscardableResource {
    ResourceHandle handle = 0;
    VKResourceType type = VKResourceType::NONE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    union {
      VkBuffer vk_buffer = VK_NULL_HANDLE;
      VkImage vk_image;
    };
    /** Views to destroy together with the resource. */
    Vector<std::variant<VkImageView, VkBufferView>> views;
  };

  /**
   * Release one reference of a resource.
   *
   * When the ref count reaches zero the resource is marked discardable, to be collected with
   * `pop_discardable_resources`.
   */
  void deref_resource(ResourceHandle resource_handle);

  /**
   * Pop all zero-ref resources.
   *
   * Entries that received a new reference are skipped; popped resources stay registered until
   * `remove_image` or `remove_buffer`.
   */
  Vector<DiscardableResource> pop_discardable_resources();

  /**
   * Unregister an image that has been destroyed externally (e.g. via `vmaDestroyImage`).
   */
  void remove_image(ResourceHandle image_handle);

  /**
   * Unregister a buffer that has been destroyed externally (e.g. via `vmaDestroyBuffer`).
   */
  VkBuffer remove_buffer(ResourceHandle buffer_handle);

  inline const VkImage get_vk_image(ResourceHandle image_handle)
  {
    std::scoped_lock lock(mutex);
    return get_image_resource(image_handle).image.vk_image;
  }

  /**
   * Unregister a swapchain image that has been destroyed externally.
   */
  void remove_swapchain_image(ResourceHandle image_handle);

  /**
   * Take ownership of the views registered on a resource; the caller must destroy them.
   * Only valid while the resource is still registered.
   */
  Vector<std::variant<VkImageView, VkBufferView>> take_views(ResourceHandle resource_handle)
  {
    std::scoped_lock lock(mutex);
    return std::move(get_resource(resource_handle).views);
  }

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
  inline ResourceWithStamp get_image_and_increase_stamp(VkImage vk_image)
  {
    ResourceHandle handle = image_resources_.lookup(vk_image);
    return {.handle = handle, .stamp = resources_[handle].stamp++};
  }

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
  inline ResourceWithStamp get_buffer_and_increase_stamp(ResourceHandle buffer_handle)
  {
    return {.handle = buffer_handle, .stamp = resources_[buffer_handle].stamp++};
  }

  /**
   * Return the current stamp of the resource.
   *
   * When a node reads from a buffer, this method is called to get the current stamp the buffer.
   *
   * This function is called when adding a node to the render graph, during building resource
   * dependencies. See `VKNodeInfo.build_links`
   */
  inline ResourceWithStamp get_buffer(ResourceHandle buffer_handle) const
  {
    return ResourceWithStamp{.handle = buffer_handle, .stamp = resources_[buffer_handle].stamp};
  }

  /**
   * Return the current stamp of the resource.
   *
   * When a node reads from an image, this method is called to get the current stamp the image.
   *
   * This function is called when adding a node to the render graph, during building resource
   * dependencies. See `VKNodeInfo.build_links`
   */
  inline ResourceWithStamp get_image(VkImage vk_image) const
  {
    ResourceHandle handle = image_resources_.lookup(vk_image);
    return {.handle = handle, .stamp = resources_[handle].stamp};
  }

  bool use_dynamic_rendering_local_read = true;

  void debug_print() const;

 private:
  ResourceHandle add_image(VkImage vk_image,
                           bool use_subresource_tracking,
                           VKResourceBarrierState barrier_state,
                           VmaAllocation allocation,
                           const char *name = nullptr);

  ResourceHandle create_resource_slot();

  /**
   * Release one reference of each resource with a usage bit set in `resource_usages`.
   *
   * The mutex is taken once for the whole operation. The usage bits index resources by
   * `ResourceHandle`, as in `VKRenderGraph::resource_usages_`.
   */
  void deref_resources(const BitVector<> &resource_usages);

  /**
   * Release one reference of a resource, without locking.
   *
   * NOTE: Can only be called when the mutex has been locked in the same thread.
   */
  void deref_resource_impl(ResourceHandle resource_handle);

  /**
   * Take one reference of a resource, without locking.
   *
   * NOTE: Can only be called when the mutex has been locked in the same thread.
   */
  void ref_resource_impl(ResourceHandle resource_handle);

  /**
   * Get the ref to an image resource state.
   *
   * NOTE: Can only be called when the mutex has been locked in the same thread.
   */
  inline Resource &get_image_resource(ResourceHandle resource_handle)
  {
    BLI_assert(resources_[resource_handle].type == VKResourceType::IMAGE);
    return resources_[resource_handle];
  }
  inline const Resource &get_image_resource(ResourceHandle resource_handle) const
  {
    BLI_assert(resources_[resource_handle].type == VKResourceType::IMAGE);
    return resources_[resource_handle];
  }
  inline Resource &get_resource(ResourceHandle resource_handle)
  {
    BLI_assert(resources_[resource_handle].type == VKResourceType::IMAGE ||
               resources_[resource_handle].type == VKResourceType::BUFFER);
    return resources_[resource_handle];
  }
  /**
   * Get the ref to a resource state.
   *
   * NOTE: Can only be called when the mutex has been locked in the same thread.
   */
  inline Resource &get_buffer_resource(ResourceHandle resource_handle)
  {
    BLI_assert(resources_[resource_handle].type == VKResourceType::BUFFER);
    return resources_[resource_handle];
  }
  inline const Resource &get_buffer_resource(ResourceHandle resource_handle) const
  {
    BLI_assert(resources_[resource_handle].type == VKResourceType::BUFFER);
    return resources_[resource_handle];
  }
};

}  // namespace blender::gpu::render_graph
