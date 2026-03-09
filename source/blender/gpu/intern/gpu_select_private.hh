/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Selection implementations.
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "GPU_select.hh"

namespace blender {

/* gpu_select_pick */

void gpu_select_pick_begin(GPUSelectBuffer *buffer, const rcti *input, GPUSelectMode mode);
bool gpu_select_pick_load_id(uint id, bool end);
uint gpu_select_pick_end();

void gpu_select_pick_cache_begin();
void gpu_select_pick_cache_end();
/**
 * \return true if drawing is not needed.
 */
bool gpu_select_pick_is_cached();
void gpu_select_pick_cache_load_id();

/* gpu_select_sample_query */

void gpu_select_query_begin(GPUSelectBuffer *buffer,
                            const rcti *input,
                            GPUSelectMode mode,
                            int oldhits);
bool gpu_select_query_load_id(uint id);
uint gpu_select_query_end();

/* gpu_select_next */

void gpu_select_next_begin(GPUSelectBuffer *buffer,
                           const rcti *input,
                           int radius,
                           GPUSelectMode mode);
uint gpu_select_next_end();

/**
 * Returns the center relative to the corner of the rect stored in the GPUSelectNextState.
 */
int2 gpu_select_next_get_pick_area_center();
GPUSelectMode gpu_select_next_get_mode();
int gpu_select_next_get_radius();
void gpu_select_next_set_result(GPUSelectResult *hit_buf, uint hit_len);

#define SELECT_ID_NONE ((uint)0xffffffff)

}  // namespace blender
