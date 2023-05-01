/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_math_vector_types.hh"

#include "gpu_texture_private.hh"

namespace blender::gpu {

/**
 * Convert host buffer to device buffer.
 *
 * \param dst_buffer: device buffer.
 * \param src_buffer: host buffer.
 * \param buffer_size: number of pixels to convert from the start of the given buffer.
 * \param host_format: format of the host buffer.
 * \param device_format: format of the device buffer.
 *
 * \note Will assert when the host_format/device_format combination isn't valid
 * (#validate_data_format) or supported. Some combinations aren't supported in Vulkan due to
 * platform incompatibility.
 */
void convert_host_to_device(void *dst_buffer,
                            const void *src_buffer,
                            size_t buffer_size,
                            eGPUDataFormat host_format,
                            eGPUTextureFormat device_format);

/**
 * Convert host buffer to device buffer with row length.
 *
 * \param dst_buffer: device buffer.
 * \param src_buffer: host buffer.
 * \param src_size: size of the host buffer.
 * \param src_row_length: Length of a single row of the buffer (in pixels).
 * \param host_format: format of the host buffer.
 * \param device_format: format of the device buffer.
 *
 * \note Will assert when the host_format/device_format combination isn't valid
 * (#validate_data_format) or supported. Some combinations aren't supported in Vulkan due to
 * platform incompatibility.
 */
void convert_host_to_device(void *dst_buffer,
                            const void *src_buffer,
                            uint2 src_size,
                            uint src_row_length,
                            eGPUDataFormat host_format,
                            eGPUTextureFormat device_format);

/**
 * Convert device buffer to host buffer.
 *
 * \param dst_buffer: host buffer
 * \param src_buffer: device buffer.
 * \param buffer_size: number of pixels to convert from the start of the given buffer.
 * \param host_format: format of the host buffer
 * \param device_format: format of the device buffer.
 *
 * \note Will assert when the host_format/device_format combination isn't valid
 * (#validate_data_format) or supported. Some combinations aren't supported in Vulkan due to
 * platform incompatibility.
 */
void convert_device_to_host(void *dst_buffer,
                            const void *src_buffer,
                            size_t buffer_size,
                            eGPUDataFormat host_format,
                            eGPUTextureFormat device_format);

};  // namespace blender::gpu
