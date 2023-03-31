/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_memory.hh"

#include "MEM_guardedalloc.h"

namespace blender::gpu {

#ifdef WITH_VULKAN_GUARDEDALLOC

void *vk_memory_allocation(void *user_data,
                           size_t size,
                           size_t alignment,
                           VkSystemAllocationScope /*scope*/)
{
  const char *name = static_cast<const char *>(const_cast<const void *>(user_data));
  if (alignment) {
    return MEM_mallocN_aligned(size, alignment, name);
  }
  return MEM_mallocN(size, name);
}

void *vk_memory_reallocation(void *user_data,
                             void *original,
                             size_t size,
                             size_t /*alignment*/,
                             VkSystemAllocationScope /*scope*/)
{
  const char *name = static_cast<const char *>(const_cast<const void *>(user_data));
  return MEM_reallocN_id(original, size, name);
}

void vk_memory_free(void * /*user_data*/, void *memory)
{
  if (memory != nullptr) {
    MEM_freeN(memory);
  }
}

#endif

}  // namespace blender::gpu
