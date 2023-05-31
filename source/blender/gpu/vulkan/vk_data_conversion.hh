/* SPDX-FileCopyrightText: 2023 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/**
 * Are all attributes of the given vertex format natively supported or does conversion needs to
 * happen.
 *
 * \param vertex_format: the vertex format to check if an associated buffer requires conversion
 *                       being done on the host.
 */
bool conversion_needed(const GPUVertFormat &vertex_format);

/**
 * Convert the given `data` to contain Vulkan natively supported data formats.
 *
 * When for an vertex attribute the fetch mode is set to GPU_FETCH_INT_TO_FLOAT and the attribute
 * is an int32_t or uint32_t the conversion will be done. Attributes of 16 or 8 bits are supported
 * natively and will be done in Vulkan.
 *
 * \param data: Buffer to convert. Data will be converted in place.
 * \param vertex_format: Vertex format of the given data. Attributes that aren't supported will be
 *        converted to a supported one.
 *  \param vertex_len: Number of vertices of the given data buffer;
 *        The number of vertices to convert.
 */
void convert_in_place(void *data, const GPUVertFormat &vertex_format, const uint vertex_len);

};  // namespace blender::gpu
