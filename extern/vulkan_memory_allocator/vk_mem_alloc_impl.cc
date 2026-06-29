/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdio>

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION

#define VMA_LEAK_LOG_FORMAT(format, ...) \
  do { \
    fprintf(stderr, "VMA: " format "\n", __VA_ARGS__); \
  } while (false)

#include "vk_mem_alloc.h"
