/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * Selection implementations.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* gpu_select_pick */

void gpu_select_pick_begin(GPUSelectResult *buffer,
                           uint buffer_len,
                           const rcti *input,
                           eGPUSelectMode mode);
bool gpu_select_pick_load_id(uint id, bool end);
uint gpu_select_pick_end(void);

void gpu_select_pick_cache_begin(void);
void gpu_select_pick_cache_end(void);
/**
 * \return true if drawing is not needed.
 */
bool gpu_select_pick_is_cached(void);
void gpu_select_pick_cache_load_id(void);

/* gpu_select_sample_query */

void gpu_select_query_begin(
    GPUSelectResult *buffer, uint buffer_len, const rcti *input, eGPUSelectMode mode, int oldhits);
bool gpu_select_query_load_id(uint id);
uint gpu_select_query_end(void);

#define SELECT_ID_NONE ((uint)0xffffffff)

#ifdef __cplusplus
}
#endif
