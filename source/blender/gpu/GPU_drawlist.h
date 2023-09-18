/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/** Opaque type hiding blender::gpu::DrawList. */
typedef struct GPUDrawList GPUDrawList;

/* Create a list with at least length drawcalls. Length can affect performance. */
GPUDrawList *GPU_draw_list_create(int length);
void GPU_draw_list_discard(GPUDrawList *list);

void GPU_draw_list_append(GPUDrawList *list, GPUBatch *batch, int i_first, int i_count);
void GPU_draw_list_submit(GPUDrawList *list);

#ifdef __cplusplus
}
#endif
