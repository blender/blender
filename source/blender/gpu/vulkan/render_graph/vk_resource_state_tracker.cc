/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <algorithm>

#include "BKE_global.hh"

#include "BLI_index_range.hh"

#include "vk_resource_state_tracker.hh"

namespace blender::gpu::render_graph {

/* -------------------------------------------------------------------- */
/** \name Adding resources
 * \{ */
ResourceHandle VKResourceStateTracker::create_resource_slot()
{
  ResourceHandle handle;
  static Resource new_resource = {};
  if (unused_handles_.is_empty()) {
    handle = resources_.append_and_get_index(new_resource);
    allocations_.append(VK_NULL_HANDLE);
  }
  else {
    handle = unused_handles_.pop_last();
    resources_[handle] = new_resource;
    allocations_[handle] = VK_NULL_HANDLE;
  }

  return handle;
}

ResourceHandle VKResourceStateTracker::add_image(VkImage vk_image,
                                                 bool use_subresource_tracking,
                                                 VKResourceBarrierState barrier_state,
                                                 VmaAllocation allocation,
                                                 const char *name)
{
  UNUSED_VARS_NDEBUG(name);
  std::scoped_lock lock(mutex);
  BLI_assert_msg(!image_resources_.contains(vk_image),
                 "Image resource is added twice to the render graph.");
  ResourceHandle handle = create_resource_slot();
  Resource &resource = resources_[handle];
  image_resources_.add_new(vk_image, handle);

  resource = {
      .type = VKResourceType::IMAGE,
      .image = {.vk_image = vk_image, .use_subresource_tracking = use_subresource_tracking},
      .ref_count = 1,
      .stamp = 0,
      .barrier_state = barrier_state,
  };
  allocations_[handle] = allocation;

#ifndef NDEBUG
  if (name) {
    resource.name = name;
  }
#endif
  return handle;
}

ResourceHandle VKResourceStateTracker::add_image(VkImage vk_image,
                                                 bool use_subresource_tracking,
                                                 VmaAllocation allocation,
                                                 const char *name)
{
  return add_image(vk_image, use_subresource_tracking, {}, allocation, name);
}

ResourceHandle VKResourceStateTracker::add_aliased_image(VkImage vk_image,
                                                         bool use_subresource_tracking,
                                                         VmaAllocation allocation,
                                                         const char *name)
{
  return add_image(vk_image,
                   use_subresource_tracking,
                   {VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED},
                   allocation,
                   name);
}

ResourceHandle VKResourceStateTracker::add_swapchain_image(VkImage vk_image, const char *name)
{
  return add_image(vk_image,
                   false,
                   {
                       VK_ACCESS_NONE,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                   },
                   VK_NULL_HANDLE,
                   name);
}

ResourceHandle VKResourceStateTracker::add_buffer(VkBuffer vk_buffer,
                                                  VmaAllocation allocation,
                                                  const char *name)
{
  UNUSED_VARS_NDEBUG(name);
  std::scoped_lock lock(mutex);
  ResourceHandle handle = create_resource_slot();
  Resource &resource = resources_[handle];

  resource = {
      .type = VKResourceType::BUFFER,
      .buffer = {.vk_buffer = vk_buffer},
      .ref_count = 1,
      .stamp = 0,
      .barrier_state = {},
  };
  allocations_[handle] = allocation;

#ifndef NDEBUG
  if (name) {
    resource.name = name;
  }
#endif

  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image layout
 * \{ */

void VKResourceStateTracker::update_image_layout(ResourceHandle image_handle,
                                                 VkImageLayout vk_image_layout)
{
  std::scoped_lock lock(mutex);
  Resource &resource = get_image_resource(image_handle);
  resource.barrier_state.image_layout = vk_image_layout;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove resources
 * \{ */

void VKResourceStateTracker::deref_resource(ResourceHandle resource_handle)
{
  std::scoped_lock lock(mutex);
  deref_resource_impl(resource_handle);
}

void VKResourceStateTracker::deref_resources(const BitVector<> &resource_usages)
{
  std::scoped_lock lock(mutex);
  for (ResourceHandle handle : IndexRange(resource_usages.size())) {
    if (resource_usages[handle]) {
      deref_resource_impl(handle);
    }
  }
}

void VKResourceStateTracker::ref_resource_impl(ResourceHandle resource_handle)
{
  Resource &resource = get_resource(resource_handle);
  resource.ref_count += 1;
}

void VKResourceStateTracker::deref_resource_impl(ResourceHandle resource_handle)
{
  Resource &resource = get_resource(resource_handle);
  BLI_assert(resource.ref_count != 0);
  resource.ref_count -= 1;
  if (resource.ref_count == 0) {
    BLI_assert(!discardable_handles_.contains(resource_handle));
    discardable_handles_.append(resource_handle);
  }
}

VkBuffer VKResourceStateTracker::remove_buffer(ResourceHandle buffer_handle)
{
  std::scoped_lock lock(mutex);
  BLI_assert(resources_[buffer_handle].ref_count == 0);
  BLI_assert(!discardable_handles_.contains(buffer_handle));
  BLI_assert(resources_[buffer_handle].views.is_empty());
  VkBuffer vk_buffer = get_buffer_resource(buffer_handle).buffer.vk_buffer;
  resources_[buffer_handle] = {
      .type = VKResourceType::NONE,
      .buffer = {.vk_buffer = VK_NULL_HANDLE},
      .ref_count = 0,
      .stamp = 0,
  };
  allocations_[buffer_handle] = VK_NULL_HANDLE;
  unused_handles_.append(buffer_handle);

  return vk_buffer;
}

void VKResourceStateTracker::remove_image(ResourceHandle image_handle)
{
  std::scoped_lock lock(mutex);
  BLI_assert(resources_[image_handle].ref_count == 0);
  BLI_assert(resources_[image_handle].views.is_empty());
  VkImage vk_image = get_image_resource(image_handle).image.vk_image;
  image_resources_.pop(vk_image);
  resources_[image_handle] = {
      .type = VKResourceType::NONE,
      .image = {.vk_image = VK_NULL_HANDLE, .use_subresource_tracking = false},
      .ref_count = 0,
      .stamp = 0,
  };
  allocations_[image_handle] = VK_NULL_HANDLE;
  unused_handles_.append(image_handle);
}

void VKResourceStateTracker::remove_swapchain_image(ResourceHandle image_handle)
{
  std::scoped_lock lock(mutex);
  BLI_assert(resources_[image_handle].ref_count == 1);
  BLI_assert(resources_[image_handle].views.is_empty());
  VkImage vk_image = get_image_resource(image_handle).image.vk_image;
  image_resources_.pop(vk_image);
  resources_[image_handle] = {
      .type = VKResourceType::NONE,
      .image = {.vk_image = VK_NULL_HANDLE, .use_subresource_tracking = false},
      .ref_count = 0,
      .stamp = 0,
  };
  allocations_[image_handle] = VK_NULL_HANDLE;
  unused_handles_.append(image_handle);
}

Vector<VKResourceStateTracker::DiscardableResource> VKResourceStateTracker::
    pop_discardable_resources()
{
  std::scoped_lock lock(mutex);
  Vector<DiscardableResource> discardable;
  Vector<ResourceHandle> keep;
  for (ResourceHandle handle : discardable_handles_) {
    Resource &resource = resources_[handle];
    BLI_assert(ELEM(resource.type, VKResourceType::IMAGE, VKResourceType::BUFFER));
    if (resource.ref_count != 0) {
      /* A new reference was acquired after the resource was marked discardable, so it stays
       * registered and is collected by a later pop. */
      keep.append(handle);
      continue;
    }
    DiscardableResource discardable_resource = {
        .handle = handle,
        .type = resource.type,
        .allocation = allocations_[handle],
    };
    if (resource.type == VKResourceType::BUFFER) {
      discardable_resource.vk_buffer = resource.buffer.vk_buffer;
    }
    else {
      discardable_resource.vk_image = resource.image.vk_image;
    }
    discardable_resource.views = std::move(resource.views);
    discardable.append(std::move(discardable_resource));
  }
  discardable_handles_ = std::move(keep);
  return discardable;
}

/** \} */

void VKResourceStateTracker::debug_print() const
{
  std::ostream &os = std::cout;
  os << "VKResourceStateTracker\n";
  os << " resources=(" << resources_.size() << "/" << resources_.capacity() << ")\n";
  os << " images=(" << image_resources_.size() << "/" << image_resources_.capacity() << ")\n";
  os << " discardable=(" << discardable_handles_.size() << "/" << discardable_handles_.capacity()
     << ")\n";
  os << " unused=(" << unused_handles_.size() << "/" << unused_handles_.capacity() << ")\n";
}

}  // namespace blender::gpu::render_graph
