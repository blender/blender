/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPUDrawList is an API to do lots of similar draw-calls very fast using
 * multi-draw-indirect. There is a fallback if the feature is not supported.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUBatch;

typedef void *GPUDrawList; /* Opaque pointer. */

/* Create a list with at least length drawcalls. Length can affect performance. */
GPUDrawList GPU_draw_list_create(int length);
void GPU_draw_list_discard(GPUDrawList list);

void GPU_draw_list_append(GPUDrawList list, GPUBatch *batch, int i_first, int i_count);
void GPU_draw_list_submit(GPUDrawList list);

#ifdef __cplusplus
}
#endif
