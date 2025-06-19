/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

/** This file contains API that the GHOST_ContextVK can invoke directly. */

namespace blender::gpu {

/**
 * Is the driver of the given physical device supported?
 *
 * There are some drivers that have known issues and should not be used. This check needs to be
 * identical between GPU module and GHOST, otherwise GHOST can still select a device which isn't
 * supported.
 *
 * For example on a Linux machine where LLVMPIPE is installed and an not supported NVIDIA driver
 * Blender would detect a supported configuration using LLVMPIPE, but GHOST could still select the
 * unsupported NVIDIA driver.
 *
 * Returns true when supported, false when not supported.
 */
bool GPU_vulkan_is_supported_driver(VkPhysicalDevice vk_physical_device);

}  // namespace blender::gpu
