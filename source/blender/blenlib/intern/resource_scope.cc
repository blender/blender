/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_resource_scope.hh"

namespace blender {

ResourceScope::ResourceScope(const int64_t initial_size)
    : allocator_(ResourceScope::create_own_allocator(resources_, initial_size))
{
}

ResourceScope::ResourceScope(void *buffer, const int64_t size)
    : allocator_(
          /* At least the allocator itself has to fit into this buffer, otherwise it's not used. */
          size >= sizeof(LinearAllocator<>) ?
              ResourceScope::create_own_allocator_in_buffer(resources_, buffer, size, false) :
              ResourceScope::create_own_allocator(resources_, 0))
{
}

LinearAllocator<> &ResourceScope::create_own_allocator_in_buffer(ResourceDataList &r_resources,
                                                                 void *data,
                                                                 const int64_t size,
                                                                 const bool free_on_destruct)
{
  BLI_assert(data);
  BLI_assert(size >= sizeof(LinearAllocator<>));

  /* Actually construct the allocator. */
  LinearAllocator<> *allocator = new (data) LinearAllocator<>();

  /* Provide the initial buffer to the allocator. */
  const int64_t extra_size = size - sizeof(LinearAllocator<>);
  if (extra_size > 0) {
    void *initial_data = static_cast<char *>(data) + sizeof(LinearAllocator<>);
    allocator->provide_buffer(initial_data, extra_size);
  }

  /* Give ownership of the allocator to r_resources. This seems a bit recursive but works out
   * because `ChunkedList` is trivially destructible by design. */
  static_assert(std::is_trivially_destructible_v<ResourceDataList>);

  /* The allocator has to be destructed. However, the memory only has to be freed if it's actually
   * owned by the #ResourceScope. */
  auto free_fn = free_on_destruct ?
                     [](void *buffer) {
                       LinearAllocator<> *allocator = static_cast<LinearAllocator<> *>(buffer);
                       std::destroy_at(allocator);
                       MEM_delete_void(buffer);
                     } :
                     [](void *buffer) {
                       LinearAllocator<> *allocator = static_cast<LinearAllocator<> *>(buffer);
                       std::destroy_at(allocator);
                     };
  r_resources.append(*allocator, ResourceData{allocator, free_fn});

  return *allocator;
}

LinearAllocator<> &ResourceScope::create_own_allocator(ResourceDataList &r_resources,
                                                       const int64_t initial_size)
{
  /* Since we are doing an allocation already, we might as well allocate a slightly larger buffer
   * to initialize memory in the linear allocator. */
  const int64_t alloc_size = sizeof(LinearAllocator<>) + sizeof(ResourceDataList::Segment) +
                             initial_size;
  void *buffer = MEM_new_uninitialized_aligned(alloc_size, alignof(LinearAllocator<>), __func__);

  return ResourceScope::create_own_allocator_in_buffer(r_resources, buffer, alloc_size, true);
}

ResourceScope::~ResourceScope()
{
  /* Frees all resources in reverse order of construction. Note that #ChunkedList always iterates
   * in reverse order. */
  LinearAllocator<> *used_allocator = &allocator_;
  for (ResourceData &resource : resources_) {
    void *data = resource.data;
    resource.free(data);
    if (data == used_allocator) {
      break;
    }
  }
}

}  // namespace blender
