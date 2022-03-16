/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BKE_image_partial_update.hh"

#include "IMB_imbuf_types.h"

#include "BLI_float4x4.hh"
#include "BLI_math_vec_types.hh"

#include "image_batches.hh"
#include "image_private.hh"
#include "image_wrappers.hh"

namespace blender::draw::image_engine {

constexpr float EPSILON_UV_BOUNDS = 0.00001f;

/**
 * \brief Screen space method using a single texture spawning the whole screen.
 */
struct OneTextureMethod {
  IMAGE_InstanceData *instance_data;

  OneTextureMethod(IMAGE_InstanceData *instance_data) : instance_data(instance_data)
  {
  }

  /** \brief Update the texture slot uv and screen space bounds. */
  void update_screen_space_bounds(const ARegion *region)
  {
    /* Create a single texture that covers the visible screen space. */
    BLI_rctf_init(
        &instance_data->texture_infos[0].clipping_bounds, 0, region->winx, 0, region->winy);
    instance_data->texture_infos[0].visible = true;

    /* Mark the other textures as invalid. */
    for (int i = 1; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      BLI_rctf_init_minmax(&instance_data->texture_infos[i].clipping_bounds);
      instance_data->texture_infos[i].visible = false;
    }
  }

  void update_screen_uv_bounds()
  {
    for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      update_screen_uv_bounds(instance_data->texture_infos[0]);
    }
  }

  void update_screen_uv_bounds(TextureInfo &info)
  {
    /* Although this works, computing an inverted matrix adds some precision issues and leads to
     * tearing artifacts. This should be modified to use the scaling and transformation from the
     * not inverted matrix.*/
    float4x4 mat(instance_data->ss_to_texture);
    float4x4 mat_inv = mat.inverted();
    float3 min_uv = mat_inv * float3(0.0f, 0.0f, 0.0f);
    float3 max_uv = mat_inv * float3(1.0f, 1.0f, 0.0f);
    rctf new_clipping_bounds;
    BLI_rctf_init(&new_clipping_bounds, min_uv[0], max_uv[0], min_uv[1], max_uv[1]);

    if (!BLI_rctf_compare(&info.clipping_uv_bounds, &new_clipping_bounds, EPSILON_UV_BOUNDS)) {
      info.clipping_uv_bounds = new_clipping_bounds;
      info.dirty = true;
    }
  }
};

using namespace blender::bke::image::partial_update;

template<typename TextureMethod> class ScreenSpaceDrawingMode : public AbstractDrawingMode {
 private:
  DRWPass *create_image_pass() const
  {
    DRWState state = static_cast<DRWState>(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS |
                                           DRW_STATE_BLEND_ALPHA_PREMUL);
    return DRW_pass_create("Image", state);
  }

  DRWPass *create_depth_pass() const
  {
    /* Depth is needed for background overlay rendering. Near depth is used for
     * transparency checker and Far depth is used for indicating the image size. */
    DRWState state = static_cast<DRWState>(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
    return DRW_pass_create("Depth", state);
  }

  void add_shgroups(const IMAGE_InstanceData *instance_data) const
  {
    const ShaderParameters &sh_params = instance_data->sh_params;
    GPUShader *shader = IMAGE_shader_image_get();
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

    DRWShadingGroup *shgrp = DRW_shgroup_create(shader, instance_data->passes.image_pass);
    DRW_shgroup_uniform_vec2_copy(shgrp, "farNearDistances", sh_params.far_near);
    DRW_shgroup_uniform_vec4_copy(shgrp, "shuffle", sh_params.shuffle);
    DRW_shgroup_uniform_int_copy(shgrp, "drawFlags", sh_params.flags);
    DRW_shgroup_uniform_bool_copy(shgrp, "imgPremultiplied", sh_params.use_premul_alpha);
    DRW_shgroup_uniform_texture(shgrp, "depth_texture", dtxl->depth);
    float image_mat[4][4];
    unit_m4(image_mat);
    for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      const TextureInfo &info = instance_data->texture_infos[i];
      if (!info.visible) {
        continue;
      }

      DRWShadingGroup *shgrp_sub = DRW_shgroup_create_sub(shgrp);
      DRW_shgroup_uniform_texture_ex(shgrp_sub, "imageTexture", info.texture, GPU_SAMPLER_DEFAULT);
      DRW_shgroup_call_obmat(shgrp_sub, info.batch, image_mat);
    }
  }

  /**
   * \brief add depth drawing calls.
   *
   * The depth is used to identify if the tile exist or transparent.
   */
  void add_depth_shgroups(IMAGE_InstanceData &instance_data,
                          Image *image,
                          ImageUser *image_user) const
  {
    GPUShader *shader = IMAGE_shader_depth_get();
    DRWShadingGroup *shgrp = DRW_shgroup_create(shader, instance_data.passes.depth_pass);
    float image_mat[4][4];
    unit_m4(image_mat);

    ImageUser tile_user = {0};
    if (image_user) {
      tile_user = *image_user;
    }

    for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      const TextureInfo &info = instance_data.texture_infos[i];
      if (!info.visible) {
        continue;
      }

      LISTBASE_FOREACH (ImageTile *, image_tile_ptr, &image->tiles) {
        const ImageTileWrapper image_tile(image_tile_ptr);
        const int tile_x = image_tile.get_tile_x_offset();
        const int tile_y = image_tile.get_tile_y_offset();
        tile_user.tile = image_tile.get_tile_number();

        /* NOTE: `BKE_image_has_ibuf` doesn't work as it fails for render results. That could be a
         * bug or a feature. For now we just acquire to determine if there is a texture. */
        void *lock;
        ImBuf *tile_buffer = BKE_image_acquire_ibuf(image, &tile_user, &lock);
        if (tile_buffer == nullptr) {
          continue;
        }
        instance_data.float_buffers.mark_used(tile_buffer);
        BKE_image_release_ibuf(image, tile_buffer, lock);

        DRWShadingGroup *shsub = DRW_shgroup_create_sub(shgrp);
        float4 min_max_uv(tile_x, tile_y, tile_x + 1, tile_y + 1);
        DRW_shgroup_uniform_vec4_copy(shsub, "min_max_uv", min_max_uv);
        DRW_shgroup_call_obmat(shsub, info.batch, image_mat);
      }
    }
  }

  /**
   * \brief Update GPUTextures for drawing the image.
   *
   * GPUTextures that are marked dirty are rebuild. GPUTextures that aren't marked dirty are
   * updated with changed region of the image.
   */
  void update_textures(IMAGE_InstanceData &instance_data,
                       Image *image,
                       ImageUser *image_user) const
  {
    PartialUpdateChecker<ImageTileData> checker(
        image, image_user, instance_data.partial_update.user);
    PartialUpdateChecker<ImageTileData>::CollectResult changes = checker.collect_changes();

    switch (changes.get_result_code()) {
      case ePartialUpdateCollectResult::FullUpdateNeeded:
        instance_data.mark_all_texture_slots_dirty();
        instance_data.float_buffers.clear();
        break;
      case ePartialUpdateCollectResult::NoChangesDetected:
        break;
      case ePartialUpdateCollectResult::PartialChangesDetected:
        /* Partial update when wrap repeat is enabled is not supported. */
        if (instance_data.flags.do_tile_drawing) {
          instance_data.float_buffers.clear();
          instance_data.mark_all_texture_slots_dirty();
        }
        else {
          do_partial_update(changes, instance_data);
        }
        break;
    }
    do_full_update_for_dirty_textures(instance_data, image_user);
  }

  /**
   * Update the float buffer in the region given by the partial update checker.
   */
  void do_partial_update_float_buffer(
      ImBuf *float_buffer, PartialUpdateChecker<ImageTileData>::CollectResult &iterator) const
  {
    ImBuf *src = iterator.tile_data.tile_buffer;
    BLI_assert(float_buffer->rect_float != nullptr);
    BLI_assert(float_buffer->rect == nullptr);
    BLI_assert(src->rect_float == nullptr);
    BLI_assert(src->rect != nullptr);

    /* Calculate the overlap between the updated region and the buffer size. Partial Update Checker
     * always returns a tile (256x256). Which could lay partially outside the buffer when using
     * different resolutions.
     */
    rcti buffer_rect;
    BLI_rcti_init(&buffer_rect, 0, float_buffer->x, 0, float_buffer->y);
    rcti clipped_update_region;
    const bool has_overlap = BLI_rcti_isect(
        &buffer_rect, &iterator.changed_region.region, &clipped_update_region);
    if (!has_overlap) {
      return;
    }

    IMB_float_from_rect_ex(float_buffer, src, &clipped_update_region);
  }

  void do_partial_update(PartialUpdateChecker<ImageTileData>::CollectResult &iterator,
                         IMAGE_InstanceData &instance_data) const
  {
    while (iterator.get_next_change() == ePartialUpdateIterResult::ChangeAvailable) {
      /* Quick exit when tile_buffer isn't available. */
      if (iterator.tile_data.tile_buffer == nullptr) {
        continue;
      }
      ImBuf *tile_buffer = ensure_float_buffer(instance_data, iterator.tile_data.tile_buffer);
      if (tile_buffer != iterator.tile_data.tile_buffer) {
        do_partial_update_float_buffer(tile_buffer, iterator);
      }

      const float tile_width = static_cast<float>(iterator.tile_data.tile_buffer->x);
      const float tile_height = static_cast<float>(iterator.tile_data.tile_buffer->y);

      for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
        const TextureInfo &info = instance_data.texture_infos[i];
        /* Dirty images will receive a full update. No need to do a partial one now. */
        if (info.dirty) {
          continue;
        }
        if (!info.visible) {
          continue;
        }
        GPUTexture *texture = info.texture;
        const float texture_width = GPU_texture_width(texture);
        const float texture_height = GPU_texture_height(texture);
        /* TODO: early bound check. */
        ImageTileWrapper tile_accessor(iterator.tile_data.tile);
        float tile_offset_x = static_cast<float>(tile_accessor.get_tile_x_offset());
        float tile_offset_y = static_cast<float>(tile_accessor.get_tile_y_offset());
        rcti *changed_region_in_texel_space = &iterator.changed_region.region;
        rctf changed_region_in_uv_space;
        BLI_rctf_init(&changed_region_in_uv_space,
                      static_cast<float>(changed_region_in_texel_space->xmin) /
                              static_cast<float>(iterator.tile_data.tile_buffer->x) +
                          tile_offset_x,
                      static_cast<float>(changed_region_in_texel_space->xmax) /
                              static_cast<float>(iterator.tile_data.tile_buffer->x) +
                          tile_offset_x,
                      static_cast<float>(changed_region_in_texel_space->ymin) /
                              static_cast<float>(iterator.tile_data.tile_buffer->y) +
                          tile_offset_y,
                      static_cast<float>(changed_region_in_texel_space->ymax) /
                              static_cast<float>(iterator.tile_data.tile_buffer->y) +
                          tile_offset_y);
        rctf changed_overlapping_region_in_uv_space;
        const bool region_overlap = BLI_rctf_isect(&info.clipping_uv_bounds,
                                                   &changed_region_in_uv_space,
                                                   &changed_overlapping_region_in_uv_space);
        if (!region_overlap) {
          continue;
        }
        /* Convert the overlapping region to texel space and to ss_pixel space...
         * TODO: first convert to ss_pixel space as integer based. and from there go back to texel
         * space. But perhaps this isn't needed and we could use an extraction offset somehow. */
        rcti gpu_texture_region_to_update;
        BLI_rcti_init(
            &gpu_texture_region_to_update,
            floor((changed_overlapping_region_in_uv_space.xmin - info.clipping_uv_bounds.xmin) *
                  texture_width / BLI_rctf_size_x(&info.clipping_uv_bounds)),
            floor((changed_overlapping_region_in_uv_space.xmax - info.clipping_uv_bounds.xmin) *
                  texture_width / BLI_rctf_size_x(&info.clipping_uv_bounds)),
            ceil((changed_overlapping_region_in_uv_space.ymin - info.clipping_uv_bounds.ymin) *
                 texture_height / BLI_rctf_size_y(&info.clipping_uv_bounds)),
            ceil((changed_overlapping_region_in_uv_space.ymax - info.clipping_uv_bounds.ymin) *
                 texture_height / BLI_rctf_size_y(&info.clipping_uv_bounds)));

        rcti tile_region_to_extract;
        BLI_rcti_init(
            &tile_region_to_extract,
            floor((changed_overlapping_region_in_uv_space.xmin - tile_offset_x) * tile_width),
            floor((changed_overlapping_region_in_uv_space.xmax - tile_offset_x) * tile_width),
            ceil((changed_overlapping_region_in_uv_space.ymin - tile_offset_y) * tile_height),
            ceil((changed_overlapping_region_in_uv_space.ymax - tile_offset_y) * tile_height));

        /* Create an image buffer with a size.
         * Extract and scale into an imbuf. */
        const int texture_region_width = BLI_rcti_size_x(&gpu_texture_region_to_update);
        const int texture_region_height = BLI_rcti_size_y(&gpu_texture_region_to_update);

        ImBuf extracted_buffer;
        IMB_initImBuf(
            &extracted_buffer, texture_region_width, texture_region_height, 32, IB_rectfloat);

        int offset = 0;
        for (int y = gpu_texture_region_to_update.ymin; y < gpu_texture_region_to_update.ymax;
             y++) {
          float yf = y / (float)texture_height;
          float v = info.clipping_uv_bounds.ymax * yf + info.clipping_uv_bounds.ymin * (1.0 - yf) -
                    tile_offset_y;
          for (int x = gpu_texture_region_to_update.xmin; x < gpu_texture_region_to_update.xmax;
               x++) {
            float xf = x / (float)texture_width;
            float u = info.clipping_uv_bounds.xmax * xf +
                      info.clipping_uv_bounds.xmin * (1.0 - xf) - tile_offset_x;
            nearest_interpolation_color(tile_buffer,
                                        nullptr,
                                        &extracted_buffer.rect_float[offset * 4],
                                        u * tile_buffer->x,
                                        v * tile_buffer->y);
            offset++;
          }
        }

        GPU_texture_update_sub(texture,
                               GPU_DATA_FLOAT,
                               extracted_buffer.rect_float,
                               gpu_texture_region_to_update.xmin,
                               gpu_texture_region_to_update.ymin,
                               0,
                               extracted_buffer.x,
                               extracted_buffer.y,
                               0);
        imb_freerectImbuf_all(&extracted_buffer);
      }
    }
  }

  void do_full_update_for_dirty_textures(IMAGE_InstanceData &instance_data,
                                         const ImageUser *image_user) const
  {
    for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      TextureInfo &info = instance_data.texture_infos[i];
      if (!info.dirty) {
        continue;
      }
      if (!info.visible) {
        continue;
      }
      do_full_update_gpu_texture(info, instance_data, image_user);
    }
  }

  void do_full_update_gpu_texture(TextureInfo &info,
                                  IMAGE_InstanceData &instance_data,
                                  const ImageUser *image_user) const
  {
    ImBuf texture_buffer;
    const int texture_width = GPU_texture_width(info.texture);
    const int texture_height = GPU_texture_height(info.texture);
    IMB_initImBuf(&texture_buffer, texture_width, texture_height, 0, IB_rectfloat);
    ImageUser tile_user = {0};
    if (image_user) {
      tile_user = *image_user;
    }

    void *lock;

    Image *image = instance_data.image;
    LISTBASE_FOREACH (ImageTile *, image_tile_ptr, &image->tiles) {
      const ImageTileWrapper image_tile(image_tile_ptr);
      tile_user.tile = image_tile.get_tile_number();

      ImBuf *tile_buffer = BKE_image_acquire_ibuf(image, &tile_user, &lock);
      if (tile_buffer == nullptr) {
        /* Couldn't load the image buffer of the tile. */
        continue;
      }
      do_full_update_texture_slot(instance_data, info, texture_buffer, *tile_buffer, image_tile);
      BKE_image_release_ibuf(image, tile_buffer, lock);
    }
    GPU_texture_update(info.texture, GPU_DATA_FLOAT, texture_buffer.rect_float);
    imb_freerectImbuf_all(&texture_buffer);
  }

  /**
   * \brief Ensure that the float buffer of the given image buffer is available.
   *
   * Returns true when a float buffer was created. Somehow the sequencer cache increases the ref
   * counter, but might use a different mechanism for destructing the image, that doesn't free the
   * rect_float as the reference-counter isn't 0. To work around this we destruct any created local
   * buffers ourself.
   */
  ImBuf *ensure_float_buffer(IMAGE_InstanceData &instance_data, ImBuf *image_buffer) const
  {
    return instance_data.float_buffers.ensure_float_buffer(image_buffer);
  }

  void do_full_update_texture_slot(IMAGE_InstanceData &instance_data,
                                   const TextureInfo &texture_info,
                                   ImBuf &texture_buffer,
                                   ImBuf &tile_buffer,
                                   const ImageTileWrapper &image_tile) const
  {
    const int texture_width = texture_buffer.x;
    const int texture_height = texture_buffer.y;
    ImBuf *float_tile_buffer = ensure_float_buffer(instance_data, &tile_buffer);

    /* IMB_transform works in a non-consistent space. This should be documented or fixed!.
     * Construct a variant of the info_uv_to_texture that adds the texel space
     * transformation.*/
    float uv_to_texel[4][4];
    copy_m4_m4(uv_to_texel, instance_data.ss_to_texture);
    float scale[3] = {static_cast<float>(texture_width) / static_cast<float>(tile_buffer.x),
                      static_cast<float>(texture_height) / static_cast<float>(tile_buffer.y),
                      1.0f};
    rescale_m4(uv_to_texel, scale);
    uv_to_texel[3][0] += image_tile.get_tile_x_offset() /
                         BLI_rctf_size_x(&texture_info.clipping_uv_bounds);
    uv_to_texel[3][1] += image_tile.get_tile_y_offset() /
                         BLI_rctf_size_y(&texture_info.clipping_uv_bounds);
    uv_to_texel[3][0] *= texture_width;
    uv_to_texel[3][1] *= texture_height;
    invert_m4(uv_to_texel);

    rctf crop_rect;
    rctf *crop_rect_ptr = nullptr;
    eIMBTransformMode transform_mode;
    if (instance_data.flags.do_tile_drawing) {
      transform_mode = IMB_TRANSFORM_MODE_WRAP_REPEAT;
    }
    else {
      BLI_rctf_init(&crop_rect, 0.0, tile_buffer.x, 0.0, tile_buffer.y);
      crop_rect_ptr = &crop_rect;
      transform_mode = IMB_TRANSFORM_MODE_CROP_SRC;
    }

    IMB_transform(float_tile_buffer,
                  &texture_buffer,
                  transform_mode,
                  IMB_FILTER_NEAREST,
                  uv_to_texel,
                  crop_rect_ptr);
  }

 public:
  void cache_init(IMAGE_Data *vedata) const override
  {
    IMAGE_InstanceData *instance_data = vedata->instance_data;
    instance_data->passes.image_pass = create_image_pass();
    instance_data->passes.depth_pass = create_depth_pass();
  }

  void cache_image(IMAGE_Data *vedata, Image *image, ImageUser *iuser) const override
  {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    IMAGE_InstanceData *instance_data = vedata->instance_data;
    TextureMethod method(instance_data);

    instance_data->partial_update.ensure_image(image);
    instance_data->clear_dirty_flag();
    instance_data->float_buffers.reset_usage_flags();

    /* Step: Find out which screen space textures are needed to draw on the screen. Remove the
     * screen space textures that aren't needed. */
    const ARegion *region = draw_ctx->region;
    method.update_screen_space_bounds(region);
    method.update_screen_uv_bounds();

    /* Check for changes in the image user compared to the last time. */
    instance_data->update_image_usage(iuser);

    /* Step: Update the GPU textures based on the changes in the image. */
    instance_data->update_gpu_texture_allocations();
    update_textures(*instance_data, image, iuser);

    /* Step: Add the GPU textures to the shgroup. */
    instance_data->update_batches();
    if (!instance_data->flags.do_tile_drawing) {
      add_depth_shgroups(*instance_data, image, iuser);
    }
    add_shgroups(instance_data);
  }

  void draw_finish(IMAGE_Data *vedata) const override
  {
    IMAGE_InstanceData *instance_data = vedata->instance_data;
    instance_data->float_buffers.remove_unused_buffers();
  }

  void draw_scene(IMAGE_Data *vedata) const override
  {
    IMAGE_InstanceData *instance_data = vedata->instance_data;

    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    GPU_framebuffer_bind(dfbl->default_fb);

    static float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float clear_depth = instance_data->flags.do_tile_drawing ? 0.75 : 1.0f;
    GPU_framebuffer_clear_color_depth(dfbl->default_fb, clear_col, clear_depth);

    DRW_view_set_active(instance_data->view);
    DRW_draw_pass(instance_data->passes.depth_pass);
    GPU_framebuffer_bind(dfbl->color_only_fb);
    DRW_draw_pass(instance_data->passes.image_pass);
    DRW_view_set_active(nullptr);
    GPU_framebuffer_bind(dfbl->default_fb);
  }
};  // namespace clipping

}  // namespace blender::draw::image_engine
