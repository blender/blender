/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_buffer.hh"
#include "vk_common.hh"

namespace blender::gpu {

/**
 * Utility class to copy data from host to device and vise versa.
 *
 * This is a common as buffers on device are more performant than when located inside host memory.
 */
class VKStagingBuffer {
 public:
  /**
   * Direction of the transfer.
   */
  enum class Direction {
    /**
     * Transferring data from host to device.
     */
    HostToDevice,
    /**
     * Transferring data from device to host.
     */
    DeviceToHost,
  };

 private:
  /**
   * Reference to the device buffer.
   */
  const VKBuffer &device_buffer_;

  /**
   * The temporary buffer on host for the transfer. Also called the staging buffer.
   */
  VKBuffer host_buffer_;

  VkDeviceSize device_buffer_offset_;
  VkDeviceSize region_size_;

 public:
  VKStagingBuffer(const VKBuffer &device_buffer,
                  Direction direction,
                  VkDeviceSize device_buffer_offset = 0,
                  VkDeviceSize region_size = UINT64_MAX);

  /**
   * Copy the content of the host buffer to the device buffer.
   */
  void copy_to_device(VKContext &context);

  /**
   * Copy the content of the device buffer to the host buffer.
   */
  void copy_from_device(VKContext &context);

  /**
   * Get the reference to the host buffer to update/load the data.
   */
  VKBuffer &host_buffer_get()
  {
    return host_buffer_;
  }

  /**
   * Free the host memory.
   *
   * In case a reference of the staging buffer is kept, but the host resource isn't needed anymore.
   */
  void free();

  VkDeviceSize size_in_bytes_get() const
  {
    return region_size_;
  }
};
}  // namespace blender::gpu
