/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex format
 */

#pragma once

#include "GPU_vertex_buffer.hh"

struct GPUVertFormat;

void VertexFormat_pack(GPUVertFormat *format);
void VertexFormat_texture_buffer_pack(GPUVertFormat *format);
uint padding(uint offset, uint alignment);
uint vertex_buffer_size(const GPUVertFormat *format, uint vertex_len);

namespace blender::gpu {

bool is_fetch_normalized(VertAttrType attr_type);
bool is_fetch_int_to_float(VertAttrType attr_type);
bool is_fetch_float(VertAttrType attr_type);

inline int format_component_len(const VertAttrType format)
{
  return format_component_len(DataFormat(int8_t(format)));
}

inline int to_bytesize(const VertAttrType format)
{
  return to_bytesize(DataFormat(int8_t(format)));
}

}  // namespace blender::gpu
