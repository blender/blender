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
  if (unused_handles_.is_empty()) {
    ResourceHandle new_handle = resources_.size();
    Resource new_resource = {};
    resources_.add_new(new_handle, new_resource);
    return new_handle;
  }
  else {
    return unused_handles_.pop_last();
  }
}

void VKResourceStateTracker::add_image(VkImage vk_image,
                                       VkImageLayout vk_image_layout,
                                       ResourceOwner owner)
{
  BLI_assert_msg(!image_resources_.contains(vk_image),
                 "Image resource is added twice to the render graph.");
  std::scoped_lock lock(mutex);
  ResourceHandle handle = create_resource_slot();
  Resource &resource = resources_.lookup(handle);
  image_resources_.add_new(vk_image, handle);

  resource.type = VKResourceType::IMAGE;
  resource.owner = owner;
  resource.image.vk_image = vk_image;
  resource.image.vk_image_layout = vk_image_layout;
  resource.stamp = 0;
}

void VKResourceStateTracker::add_buffer(VkBuffer vk_buffer)
{
  BLI_assert_msg(!buffer_resources_.contains(vk_buffer),
                 "Buffer resource is added twice to the render graph.");
  std::scoped_lock lock(mutex);
  ResourceHandle handle = create_resource_slot();
  Resource &resource = resources_.lookup(handle);
  buffer_resources_.add_new(vk_buffer, handle);

  resource.type = VKResourceType::BUFFER;
  resource.owner = ResourceOwner::APPLICATION;
  resource.buffer.vk_buffer = vk_buffer;
  resource.stamp = 0;
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
}

void VKResourceStateTracker::remove_image(VkImage vk_image)
{
  std::scoped_lock lock(mutex);
  ResourceHandle handle = image_resources_.pop(vk_image);
  resources_.pop(handle);
  unused_handles_.append(handle);
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

ResourceWithStamp VKResourceStateTracker::get_buffer_and_increase_version(VkBuffer vk_buffer)
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

void VKResourceStateTracker::reset_image_layouts()
{
  for (ResourceHandle image_handle : image_resources_.values()) {
    VKResourceStateTracker::Resource &resource = resources_.lookup(image_handle);
    if (resource.owner == ResourceOwner::SWAP_CHAIN) {
      resource.reset_image_layout();
    }
  }
}

}  // namespace blender::gpu::render_graph
