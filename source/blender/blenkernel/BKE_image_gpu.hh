/* SPDX-FileCopyrightText: 2001-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * GPU texture API for #Image. Splits the GPU-side concerns
 * (texture lifecycle, material/viewer/2D texture retrieval, GC) off
 * #BKE_image.hh so non-GPU consumers don't pull GPU declarations in.
 */

namespace blender {

namespace gpu {
class Texture;
}  // namespace gpu

struct ImBuf;
struct Image;
struct ImageTile;
struct ImageUser;
struct Main;

/**
 * Not to be use directly.
 */
gpu::Texture *BKE_image_create_gpu_texture_from_ibuf(Image *image, ImBuf *ibuf);

/**
 * Ensure that the cached GPU texture inside the image matches the pass, layer, and view of the
 * given image user, if not, invalidate the cache such that the next call to the GPU texture
 * retrieval functions such as BKE_image_get_gpu_texture updates the cache with an image that
 * matches the give image user.
 *
 * This is provided as a separate function and not implemented as part of the GPU texture retrieval
 * functions because the current cache system only allows a single pass, layer, and stereo view to
 * be cached, so possible frequent cache invalidation can have performance implications,
 * and making invalidation explicit by calling this function will help make that clear and pave the
 * way for a more complete cache system in the future.
 */
void BKE_image_ensure_gpu_texture(Image *image, ImageUser *iuser);

/**
 * Get the #gpu::Texture for a given `Image`.
 *
 *
 *
 * The requested GPU texture will be cached for subsequent calls, but only a single layer, pass,
 * and view can be cached at a time, so the cache should be invalidated in operators and RNA
 * callbacks that change the layer, pass, or view of the image to maintain a correct cache state.
 * However, in some cases, multiple layers, passes, or views might be needed at the same time, like
 * is the case for the compositor. This is currently not supported, so the caller should
 * ensure that the requested layer is indeed the cached one and invalidated the cached otherwise by
 * calling BKE_image_ensure_gpu_texture. This is a workaround until image can support a more
 * complete caching system.
 */
gpu::Texture *BKE_image_get_gpu_texture(Image *image, ImageUser *iuser);

/*
 * Like BKE_image_get_gpu_texture, but can also get render or compositing result.
 */
gpu::Texture *BKE_image_get_gpu_viewer_texture(Image *image, ImageUser *iuser);

/*
 * Like BKE_image_get_gpu_viewer_texture, but the image buffer is provided explicitly.
 */
gpu::Texture *BKE_image_get_gpu_viewer_texture(Image *image,
                                               ImageUser *iuser,
                                               ImBuf *image_buffer);

/*
 * Like BKE_image_get_gpu_texture, but can also return array and tile mapping texture for UDIM
 * tiles as used in material shaders.
 */
struct ImageGPUTextures {
  gpu::Texture **texture;
  gpu::Texture **tile_mapping;
};

ImageGPUTextures BKE_image_get_gpu_material_texture(Image *image,
                                                    ImageUser *iuser,
                                                    const bool use_tile_mapping);

/* Same as BKE_image_get_gpu_material_texture but will not load the texture if it isn't already. */
ImageGPUTextures BKE_image_get_gpu_material_texture_try(Image *image,
                                                        ImageUser *iuser,
                                                        const bool use_tile_mapping);

/**
 * Is the alpha of the `gpu::Texture` for a given image/ibuf premultiplied.
 */
bool BKE_image_has_gpu_texture_premultiplied_alpha(Image *image, ImBuf *ibuf);

/**
 * Check if image has an associated GPU texture.
 */
bool BKE_image_has_gpu_texture(Image *ima);

void BKE_image_free_gputextures(Image *ima);
void BKE_image_free_all_gputextures(Main *bmain);
/**
 * Same as #BKE_image_free_all_gputextures but only free animated images.
 */
void BKE_image_free_anim_gputextures(Main *bmain);

/**
 * Partial update of texture for texture painting.
 * This is often much quicker than fully updating the texture for high resolution images.
 */
void BKE_image_update_gputexture(Image *ima, ImageUser *iuser, int x, int y, int w, int h);

/**
 * Mark areas on the #gpu::Texture that need to be updated. The areas are marked in chunks.
 * The next time the #gpu::Texture is used these tiles will be refreshed. This saves time
 * when writing to the same place multiple times during foreground rendering.
 */
void BKE_image_update_gputexture_delayed(
    Image *ima, ImageTile *image_tile, ImBuf *ibuf, int x, int y, int w, int h);

/**
 * Called on entering and exiting texture paint mode, temporarily disabling/enabling
 * mipmapping on all images for quick partial texture updates. Images that didn't
 * change don't have to be re-uploaded to the GPU.
 */
void BKE_image_paint_set_mipmap(Main *bmain, bool mipmap);

}  // namespace blender
