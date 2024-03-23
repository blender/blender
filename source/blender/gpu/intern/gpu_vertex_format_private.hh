/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex format
 */

#pragma once

struct GPUVertFormat;

void VertexFormat_pack(GPUVertFormat *format);
void VertexFormat_texture_buffer_pack(GPUVertFormat *format);
uint padding(uint offset, uint alignment);
uint vertex_buffer_size(const GPUVertFormat *format, uint vertex_len);
