/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. */

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

/**
 * Try to find a texture corresponding to params into the texture pool.
 * If no texture was found, create one and add it to the pool.
 */
GPUTexture *DRW_texture_pool_query(
    DRWTexturePool *pool, int width, int height, eGPUTextureFormat format, void *user);
/**
 * Returns a temporary texture that needs to be released after use. Texture content is undefined.
 */
GPUTexture *DRW_texture_pool_texture_acquire(DRWTexturePool *pool,
                                             int width,
                                             int height,
                                             eGPUTextureFormat format);
/**
 * Releases a previously acquired texture.
 */
void DRW_texture_pool_texture_release(DRWTexturePool *pool, GPUTexture *tmp_tex);
/**
 * Resets the user bits for each texture in the pool and delete unused ones.
 */
void DRW_texture_pool_reset(DRWTexturePool *pool);

#ifdef __cplusplus
}
#endif
