/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex format
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUVertFormat;

void VertexFormat_pack(struct GPUVertFormat *format);
void VertexFormat_texture_buffer_pack(struct GPUVertFormat *format);
uint padding(uint offset, uint alignment);
uint vertex_buffer_size(const struct GPUVertFormat *format, uint vertex_len);

#ifdef __cplusplus
}
#endif
