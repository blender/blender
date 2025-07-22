/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_view_data.hh"

#include "image_drawing_mode.hh"
#include "image_instance.hh"
#include "image_shader.hh"

#include "BKE_image.hh"
#include "BKE_image_partial_update.hh"

namespace blender::image_engine {

void ScreenSpaceDrawingMode::add_shgroups() const
{
  PassSimple &pass = instance_.state.image_ps;
  GPUShader *shader = ShaderModule::module_get().color.get();
  const ShaderParameters &sh_params = instance_.state.sh_params;
  DefaultTextureList *dtxl = DRW_context_get()->viewport_texture_list_get();

  pass.shader_set(shader);
  pass.push_constant("far_near_distances", sh_params.far_near);
  pass.push_constant("shuffle", sh_params.shuffle);
  pass.push_constant("draw_flags", int32_t(sh_params.flags));
  pass.push_constant("is_image_premultiplied", sh_params.use_premul_alpha);
  pass.bind_texture("depth_tx", dtxl->depth);

  float4x4 image_mat = float4x4::identity();
  ResourceHandleRange handle = instance_.manager->resource_handle(image_mat);
  for (const TextureInfo &info : instance_.state.texture_infos) {
    PassSimple::Sub &sub = pass.sub("Texture");
    sub.push_constant("offset", info.offset());
    sub.bind_texture("image_tx", info.texture);
    sub.draw(info.batch, handle);
  }
}

void ScreenSpaceDrawingMode::add_depth_shgroups(::Image *image, ImageUser *image_user) const
{
  PassSimple &pass = instance_.state.depth_ps;
  GPUShader *shader = ShaderModule::module_get().depth.get();
  pass.shader_set(shader);

  float4x4 image_mat = float4x4::identity();
  ResourceHandleRange handle = instance_.manager->resource_handle(image_mat);

  ImageUser tile_user = {nullptr};
  if (image_user) {
    tile_user = *image_user;
  }

  for (const TextureInfo &info : instance_.state.texture_infos) {
    LISTBASE_FOREACH (ImageTile *, image_tile_ptr, &image->tiles) {
      const ImageTileWrapper image_tile(image_tile_ptr);
      const int tile_x = image_tile.get_tile_x_offset();
      const int tile_y = image_tile.get_tile_y_offset();
      tile_user.tile = image_tile.get_tile_number();

      /* NOTE: `BKE_image_has_ibuf` doesn't work as it fails for render results. That could be a
       * bug or a feature. For now we just acquire to determine if there is a texture. */
      void *lock;
      ImBuf *tile_buffer = BKE_image_acquire_ibuf(image, &tile_user, &lock);
      if (tile_buffer != nullptr) {
        instance_.state.float_buffers.mark_used(tile_buffer);
        PassSimple::Sub &sub = pass.sub("Tile");
        float4 min_max_uv(tile_x, tile_y, tile_x + 1, tile_y + 1);
        sub.push_constant("min_max_uv", min_max_uv);
        sub.draw(info.batch, handle);
      }
      BKE_image_release_ibuf(image, tile_buffer, lock);
    }
  }
}

void ScreenSpaceDrawingMode::update_textures(::Image *image, ImageUser *image_user) const
{
  State &state = instance_.state;
  PartialUpdateChecker<ImageTileData> checker(image, image_user, state.partial_update.user);
  PartialUpdateChecker<ImageTileData>::CollectResult changes = checker.collect_changes();

  switch (changes.get_result_code()) {
    case ePartialUpdateCollectResult::FullUpdateNeeded:
      state.mark_all_texture_slots_dirty();
      state.float_buffers.clear();
      break;
    case ePartialUpdateCollectResult::NoChangesDetected:
      break;
    case ePartialUpdateCollectResult::PartialChangesDetected:
      /* Partial update when wrap repeat is enabled is not supported. */
      if (state.flags.do_tile_drawing) {
        state.float_buffers.clear();
        state.mark_all_texture_slots_dirty();
      }
      else {
        do_partial_update(changes);
      }
      break;
  }
  do_full_update_for_dirty_textures(image_user);
}

void ScreenSpaceDrawingMode::do_partial_update_float_buffer(
    ImBuf *float_buffer, PartialUpdateChecker<ImageTileData>::CollectResult &iterator) const
{
  ImBuf *src = iterator.tile_data.tile_buffer;
  BLI_assert(float_buffer->float_buffer.data != nullptr);
  BLI_assert(float_buffer->byte_buffer.data == nullptr);
  BLI_assert(src->float_buffer.data == nullptr);
  BLI_assert(src->byte_buffer.data != nullptr);

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

  IMB_float_from_byte_ex(float_buffer, src, &clipped_update_region);
}

void ScreenSpaceDrawingMode::do_partial_update(
    PartialUpdateChecker<ImageTileData>::CollectResult &iterator) const
{
  while (iterator.get_next_change() == ePartialUpdateIterResult::ChangeAvailable) {
    /* Quick exit when tile_buffer isn't available. */
    if (iterator.tile_data.tile_buffer == nullptr) {
      continue;
    }
    ImBuf *tile_buffer = instance_.state.float_buffers.cached_float_buffer(
        iterator.tile_data.tile_buffer);
    if (tile_buffer != iterator.tile_data.tile_buffer) {
      do_partial_update_float_buffer(tile_buffer, iterator);
    }

    const float tile_width = float(iterator.tile_data.tile_buffer->x);
    const float tile_height = float(iterator.tile_data.tile_buffer->y);

    for (const TextureInfo &info : instance_.state.texture_infos) {
      /* Dirty images will receive a full update. No need to do a partial one now. */
      if (info.need_full_update) {
        continue;
      }
      gpu::Texture *texture = info.texture;
      const float texture_width = GPU_texture_width(texture);
      const float texture_height = GPU_texture_height(texture);
      /* TODO: early bound check. */
      ImageTileWrapper tile_accessor(iterator.tile_data.tile);
      float tile_offset_x = float(tile_accessor.get_tile_x_offset());
      float tile_offset_y = float(tile_accessor.get_tile_y_offset());
      rcti *changed_region_in_texel_space = &iterator.changed_region.region;
      rctf changed_region_in_uv_space;
      BLI_rctf_init(
          &changed_region_in_uv_space,
          float(changed_region_in_texel_space->xmin) / float(iterator.tile_data.tile_buffer->x) +
              tile_offset_x,
          float(changed_region_in_texel_space->xmax) / float(iterator.tile_data.tile_buffer->x) +
              tile_offset_x,
          float(changed_region_in_texel_space->ymin) / float(iterator.tile_data.tile_buffer->y) +
              tile_offset_y,
          float(changed_region_in_texel_space->ymax) / float(iterator.tile_data.tile_buffer->y) +
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
          &extracted_buffer, texture_region_width, texture_region_height, 32, IB_float_data);

      int offset = 0;
      for (int y = gpu_texture_region_to_update.ymin; y < gpu_texture_region_to_update.ymax; y++) {
        float yf = y / float(texture_height);
        float v = info.clipping_uv_bounds.ymax * yf + info.clipping_uv_bounds.ymin * (1.0 - yf) -
                  tile_offset_y;
        for (int x = gpu_texture_region_to_update.xmin; x < gpu_texture_region_to_update.xmax; x++)
        {
          float xf = x / float(texture_width);
          float u = info.clipping_uv_bounds.xmax * xf + info.clipping_uv_bounds.xmin * (1.0 - xf) -
                    tile_offset_x;
          imbuf::interpolate_nearest_border_fl(tile_buffer,
                                               &extracted_buffer.float_buffer.data[offset * 4],
                                               u * tile_buffer->x,
                                               v * tile_buffer->y);
          offset++;
        }
      }
      IMB_gpu_clamp_half_float(&extracted_buffer);

      GPU_texture_update_sub(texture,
                             GPU_DATA_FLOAT,
                             extracted_buffer.float_buffer.data,
                             gpu_texture_region_to_update.xmin,
                             gpu_texture_region_to_update.ymin,
                             0,
                             extracted_buffer.x,
                             extracted_buffer.y,
                             0);
      IMB_free_all_data(&extracted_buffer);
    }
  }
}

void ScreenSpaceDrawingMode::do_full_update_for_dirty_textures(const ImageUser *image_user) const
{
  for (TextureInfo &info : instance_.state.texture_infos) {
    if (!info.need_full_update) {
      continue;
    }
    do_full_update_gpu_texture(info, image_user);
  }
}

void ScreenSpaceDrawingMode::do_full_update_gpu_texture(TextureInfo &info,
                                                        const ImageUser *image_user) const
{
  ImBuf texture_buffer;
  const int texture_width = GPU_texture_width(info.texture);
  const int texture_height = GPU_texture_height(info.texture);
  IMB_initImBuf(&texture_buffer, texture_width, texture_height, 0, IB_float_data);
  ImageUser tile_user = {nullptr};
  if (image_user) {
    tile_user = *image_user;
  }

  void *lock;

  ::Image *image = instance_.state.image;
  LISTBASE_FOREACH (ImageTile *, image_tile_ptr, &image->tiles) {
    const ImageTileWrapper image_tile(image_tile_ptr);
    tile_user.tile = image_tile.get_tile_number();

    ImBuf *tile_buffer = BKE_image_acquire_ibuf(image, &tile_user, &lock);
    if (tile_buffer != nullptr) {
      do_full_update_texture_slot(info, texture_buffer, *tile_buffer, image_tile);
    }
    BKE_image_release_ibuf(image, tile_buffer, lock);
  }
  IMB_gpu_clamp_half_float(&texture_buffer);
  GPU_texture_update(info.texture, GPU_DATA_FLOAT, texture_buffer.float_buffer.data);
  IMB_free_all_data(&texture_buffer);
}

void ScreenSpaceDrawingMode::do_full_update_texture_slot(const TextureInfo &texture_info,
                                                         ImBuf &texture_buffer,
                                                         ImBuf &tile_buffer,
                                                         const ImageTileWrapper &image_tile) const
{
  const int texture_width = texture_buffer.x;
  const int texture_height = texture_buffer.y;
  ImBuf *float_tile_buffer = instance_.state.float_buffers.cached_float_buffer(&tile_buffer);

  /* IMB_transform works in a non-consistent space. This should be documented or fixed!.
   * Construct a variant of the info_uv_to_texture that adds the texel space
   * transformation. */
  float3x3 uv_to_texel;
  rctf texture_area;
  rctf tile_area;

  BLI_rctf_init(&texture_area, 0.0, texture_width, 0.0, texture_height);
  BLI_rctf_init(
      &tile_area,
      tile_buffer.x * (texture_info.clipping_uv_bounds.xmin - image_tile.get_tile_x_offset()),
      tile_buffer.x * (texture_info.clipping_uv_bounds.xmax - image_tile.get_tile_x_offset()),
      tile_buffer.y * (texture_info.clipping_uv_bounds.ymin - image_tile.get_tile_y_offset()),
      tile_buffer.y * (texture_info.clipping_uv_bounds.ymax - image_tile.get_tile_y_offset()));
  BLI_rctf_transform_calc_m3_pivot_min(&tile_area, &texture_area, uv_to_texel.ptr());
  uv_to_texel = math::invert(uv_to_texel);

  rctf crop_rect;
  const rctf *crop_rect_ptr = nullptr;
  eIMBTransformMode transform_mode;
  if (instance_.state.flags.do_tile_drawing) {
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

void ScreenSpaceDrawingMode::begin_sync() const
{
  {
    DefaultTextureList *dtxl = DRW_context_get()->viewport_texture_list_get();
    instance_.state.depth_fb.ensure(GPU_ATTACHMENT_TEXTURE(dtxl->depth));
    instance_.state.color_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(dtxl->color));
  }
  {
    PassSimple &pass = instance_.state.image_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND_ALPHA_PREMUL);
  }
  {
    PassSimple &pass = instance_.state.depth_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
  }
}

void ScreenSpaceDrawingMode::image_sync(::Image *image, ImageUser *iuser) const
{
  State &state = instance_.state;

  state.partial_update.ensure_image(image);
  state.clear_need_full_update_flag();
  state.float_buffers.reset_usage_flags();

  /* Step: Find out which screen space textures are needed to draw on the screen. Recycle
   * textures that are not on screen anymore. */
  OneTexture method(&state);
  method.ensure_texture_infos();
  method.update_bounds(instance_.region);

  /* Step: Check for changes in the image user compared to the last time. */
  state.update_image_usage(iuser);

  /* Step: Update the GPU textures based on the changes in the image. */
  method.ensure_gpu_textures_allocation();
  update_textures(image, iuser);

  /* Step: Add the GPU textures to the shgroup. */
  state.update_batches();
  if (!state.flags.do_tile_drawing) {
    add_depth_shgroups(image, iuser);
  }
  add_shgroups();
}

void ScreenSpaceDrawingMode::draw_finish() const
{
  instance_.state.float_buffers.remove_unused_buffers();
}

void ScreenSpaceDrawingMode::draw_viewport() const
{
  float clear_depth = instance_.state.flags.do_tile_drawing ? 0.75 : 1.0f;
  GPU_framebuffer_bind(instance_.state.depth_fb);
  instance_.state.depth_fb.clear_depth(clear_depth);
  instance_.manager->submit(instance_.state.depth_ps, instance_.state.view);

  GPU_framebuffer_bind(instance_.state.color_fb);
  float4 clear_color = float4(0.0);
  GPU_framebuffer_clear_color(instance_.state.color_fb, clear_color);
  instance_.manager->submit(instance_.state.image_ps, instance_.state.view);
}

}  // namespace blender::image_engine
