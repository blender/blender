/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

#ifdef __APPLE__
#  include <MoltenVK/vk_mvk_moltenvk.h>
#else
#  include <vulkan/vulkan.h>
#endif

#define VMA_IMPLEMENTATION

/* 
 * Disabling internal asserts of VMA. 
 * 
 * Blender can destroy logical device before all the resources are freed. This is because static
 * resources are freed as a last step during quiting. As long as Vulkan isn't feature complete
 * we don't want to change this behavior. So for now we just disable the asserts.
 */
#define VMA_ASSERT(test)

#include "vk_mem_alloc.h"
