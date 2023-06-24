/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 * DEPRECATED: Use DRW_texture_pool_texture_acquire instead and do it just before rendering.
 */
GPUTexture *DRW_texture_pool_query(DRWTexturePool *pool,
                                   int width,
                                   int height,
                                   eGPUTextureFormat format,
                                   eGPUTextureUsage usage,
                                   void *user);

/**
 * Returns a temporary texture that needs to be released after use. Texture content is undefined.
 */
GPUTexture *DRW_texture_pool_texture_acquire(
    DRWTexturePool *pool, int width, int height, eGPUTextureFormat format, eGPUTextureUsage usage);
/**
 * Releases a previously acquired texture.
 */
void DRW_texture_pool_texture_release(DRWTexturePool *pool, GPUTexture *tmp_tex);

/**
 * This effectively remove a texture from the texture pool, giving full ownership to the caller.
 * The given texture needs to be been acquired through DRW_texture_pool_texture_acquire().
 * IMPORTANT: This removes the need for a DRW_texture_pool_texture_release() call on this texture.
 */
void DRW_texture_pool_take_texture_ownership(DRWTexturePool *pool, GPUTexture *tex);
/**
 * This Inserts a texture into the texture pool, giving full ownership to the texture pool.
 * The texture needs not to be in the pool already.
 * The texture may be reused in a latter call to DRW_texture_pool_texture_acquire();
 * IMPORTANT: DRW_texture_pool_texture_release() still needs to be called on this texture
 * after usage.
 */
void DRW_texture_pool_give_texture_ownership(DRWTexturePool *pool, GPUTexture *tex);

/**
 * Resets the user bits for each texture in the pool and delete unused ones.
 */
void DRW_texture_pool_reset(DRWTexturePool *pool);

#ifdef __cplusplus
}
#endif
