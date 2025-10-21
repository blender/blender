/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_index_range.hh"

#include "vk_resource_state_tracker.hh"

namespace blender::gpu::render_graph {

/* -------------------------------------------------------------------- */
/** \name Adding resources
 * \{ */
ResourceHandle VKResourceStateTracker::create_resource_slot()
{
  ResourceHandle handle;
  if (unused_handles_.is_empty()) {
    handle = resources_.size();
  }
  else {
    handle = unused_handles_.pop_last();
  }

  Resource new_resource = {};
  resources_.add_new(handle, new_resource);
  return handle;
}

void VKResourceStateTracker::add_image(VkImage vk_image,
                                       bool use_subresource_tracking,
                                       VKResourceBarrierState barrier_state,
                                       const char *name)
{
  UNUSED_VARS_NDEBUG(name);
  BLI_assert_msg(!image_resources_.contains(vk_image),
                 "Image resource is added twice to the render graph.");
  std::scoped_lock lock(mutex);
  ResourceHandle handle = create_resource_slot();
  Resource &resource = resources_.lookup(handle);
  image_resources_.add_new(vk_image, handle);

  resource.type = VKResourceType::IMAGE;
  resource.image.vk_image = vk_image;
  resource.image.use_subresource_tracking = use_subresource_tracking;
  resource.barrier_state = barrier_state;
#ifndef NDEBUG
  resource.name = name;
#endif

#ifdef VK_RESOURCE_STATE_TRACKER_VALIDATION
  validate();
#endif
}

void VKResourceStateTracker::add_image(VkImage vk_image,
                                       bool use_subresource_tracking,
                                       const char *name)
{
  add_image(vk_image, use_subresource_tracking, {}, name);
}

void VKResourceStateTracker::add_swapchain_image(VkImage vk_image, const char *name)
{
  add_image(vk_image,
            false,
            {
                VK_ACCESS_NONE,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            },
            name);
}

void VKResourceStateTracker::add_buffer(VkBuffer vk_buffer, const char *name)
{
  UNUSED_VARS_NDEBUG(name);
  BLI_assert_msg(!buffer_resources_.contains(vk_buffer),
                 "Buffer resource is added twice to the render graph.");
  std::scoped_lock lock(mutex);
  ResourceHandle handle = create_resource_slot();
  Resource &resource = resources_.lookup(handle);
  buffer_resources_.add_new(vk_buffer, handle);

  resource.type = VKResourceType::BUFFER;
  resource.buffer.vk_buffer = vk_buffer;
  resource.stamp = 0;
#ifndef NDEBUG
  resource.name = name;
#endif

#ifdef VK_RESOURCE_STATE_TRACKER_VALIDATION
  validate();
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove resources
 * \{ */

void VKResourceStateTracker::remove_buffer(VkBuffer vk_buffer)
{
  std::scoped_lock lock(mutex);
  ResourceHandle handle = buffer_resources_.pop(vk_buffer);
  resources_.pop(handle);
  unused_handles_.append(handle);

#ifdef VK_RESOURCE_STATE_TRACKER_VALIDATION
  validate();
#endif
}

void VKResourceStateTracker::remove_image(VkImage vk_image)
{
  std::scoped_lock lock(mutex);
  ResourceHandle handle = image_resources_.pop(vk_image);
  resources_.pop(handle);
  unused_handles_.append(handle);

#ifdef VK_RESOURCE_STATE_TRACKER_VALIDATION
  validate();
#endif
}

/** \} */

ResourceWithStamp VKResourceStateTracker::get_stamp(ResourceHandle handle,
                                                    const Resource &resource)
{
  ResourceWithStamp result;
  result.handle = handle;
  result.stamp = resource.stamp;
  return result;
}

ResourceWithStamp VKResourceStateTracker::get_and_increase_stamp(ResourceHandle handle,
                                                                 Resource &resource)
{
  ResourceWithStamp result = get_stamp(handle, resource);
  resource.stamp += 1;
  return result;
}

ResourceWithStamp VKResourceStateTracker::get_image_and_increase_stamp(VkImage vk_image)
{
  ResourceHandle handle = image_resources_.lookup(vk_image);
  Resource &resource = resources_.lookup(handle);
  return get_and_increase_stamp(handle, resource);
}

ResourceWithStamp VKResourceStateTracker::get_buffer_and_increase_stamp(VkBuffer vk_buffer)
{
  ResourceHandle handle = buffer_resources_.lookup(vk_buffer);
  Resource &resource = resources_.lookup(handle);
  return get_and_increase_stamp(handle, resource);
}

ResourceWithStamp VKResourceStateTracker::get_buffer(VkBuffer vk_buffer) const
{
  ResourceHandle handle = buffer_resources_.lookup(vk_buffer);
  const Resource &resource = resources_.lookup(handle);
  return get_stamp(handle, resource);
}

ResourceWithStamp VKResourceStateTracker::get_image(VkImage vk_image) const
{
  ResourceHandle handle = image_resources_.lookup(vk_image);
  const Resource &resource = resources_.lookup(handle);
  return get_stamp(handle, resource);
}

#ifdef VK_RESOURCE_STATE_TRACKER_VALIDATION
void VKResourceStateTracker::validate() const
{
  for (const Map<VkImage, ResourceHandle>::Item &item : image_resources_.items()) {
    for (ResourceHandle buffer_handle : buffer_resources_.values()) {
      BLI_assert(item.value != buffer_handle);
    }
    BLI_assert(resources_.contains(item.value));
    const Resource &resource = resources_.lookup(item.value);
    BLI_assert(resource.type == VKResourceType::IMAGE);
  }

  for (const Map<VkBuffer, ResourceHandle>::Item &item : buffer_resources_.items()) {
    for (ResourceHandle image_handle : image_resources_.values()) {
      BLI_assert(item.value != image_handle);
    }
    BLI_assert(resources_.contains(item.value));
    const Resource &resource = resources_.lookup(item.value);
    BLI_assert(resource.type == VKResourceType::BUFFER);
  }

  BLI_assert(resources_.size() == image_resources_.size() + buffer_resources_.size());
}
#endif

void VKResourceStateTracker::debug_print() const
{
  std::ostream &os = std::cout;
  os << "VKResourceStateTracker\n";
  os << " resources=(" << resources_.size() << "/" << resources_.capacity() << ")\n";
  os << " buffers=(" << buffer_resources_.size() << "/" << buffer_resources_.capacity() << ")\n";
  os << " images=(" << image_resources_.size() << "/" << image_resources_.capacity() << ")\n";
  os << " unused=(" << unused_handles_.size() << "/" << unused_handles_.capacity() << ")\n";
}

}  // namespace blender::gpu::render_graph
