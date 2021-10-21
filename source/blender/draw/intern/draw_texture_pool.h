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
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup draw
 *
 * Texture pool
 * A pool that gives temporary render targets that can be reused through other parts of the
 * render pipeline.
 * Expect texture data is garbage when acquiring it.
 */

#pragma once

#include "GPU_texture.h"

typedef struct DRWTexturePool DRWTexturePool;

#ifdef __cplusplus
extern "C" {
#endif

DRWTexturePool *DRW_texture_pool_create(void);
void DRW_texture_pool_free(DRWTexturePool *pool);

GPUTexture *DRW_texture_pool_query(
    DRWTexturePool *pool, int width, int height, eGPUTextureFormat format, void *user);
void DRW_texture_pool_reset(DRWTexturePool *pool);

#ifdef __cplusplus
}
#endif
