/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <DRW_render.hh>

#include "BKE_context.hh"

#include "GPU_capabilities.hh"

#include "DRW_engine.hh"
#include "draw_view_data.hh"

#include "image_drawing_mode_image_space.hh"
#include "image_drawing_mode_screen_space.hh"
#include "image_private.hh"
#include "image_space.hh"
#include "image_space_image.hh"
#include "image_space_node.hh"

#include "BLI_math_matrix.hh"

#include "DNA_space_types.h"

namespace blender::image_engine {

static inline std::unique_ptr<AbstractSpaceAccessor> space_accessor_from_space(
    SpaceLink *space_link)
{
  if (space_link->spacetype == SPACE_IMAGE) {
    return std::make_unique<SpaceImageAccessor>(
        static_cast<SpaceImage *>(static_cast<void *>(space_link)));
  }
  if (space_link->spacetype == SPACE_NODE) {
    return std::make_unique<SpaceNodeAccessor>(
        static_cast<SpaceNode *>(static_cast<void *>(space_link)));
  }
  BLI_assert_unreachable();
  return nullptr;
}

class Instance : public DrawEngine {
 private:
  std::unique_ptr<AbstractSpaceAccessor> space_;
  std::unique_ptr<AbstractDrawingMode> drawing_mode_;
  Main *main_;

 public:
  const ARegion *region;
  State state;
  Manager *manager = nullptr;

 public:
  StringRefNull name_get() final
  {
    return "UV/Image";
  }

  void init() final
  {
    const DRWContext *ctx_state = DRW_context_get();
    main_ = CTX_data_main(ctx_state->evil_C);
    region = ctx_state->region;
    space_ = space_accessor_from_space(ctx_state->space_data);
    manager = DRW_manager_get();
  }

  /* Constructs either a screen space or an image space drawing mode depending on if the image can
   * fit in a GPU texture. So we just need to retrieve the image buffer and check if its size is
   * safe for GPU use. */
  std::unique_ptr<AbstractDrawingMode> get_drawing_mode()
  {
    if (this->state.image->source != IMA_SRC_TILED) {
      void *lock;
      ImBuf *buffer = BKE_image_acquire_ibuf_gpu(
          this->state.image, space_->get_image_user(), &lock);
      BLI_SCOPED_DEFER([&]() { BKE_image_release_ibuf(this->state.image, buffer, lock); });

      /* The image buffer already has a GPU texture, so use image space drawing. */
      if (buffer && buffer->gpu.texture) {
        return std::make_unique<ImageSpaceDrawingMode>(*this, buffer->gpu.texture);
      }

      /* Buffer does not exist or image will not fit in a GPU texture, use screen space drawing. */
      if (!buffer || (!buffer->float_data() && !buffer->byte_data()) ||
          !GPU_is_safe_texture_size(buffer->x, buffer->y))
      {
        return std::make_unique<ScreenSpaceDrawingMode>(*this);
      }

      /* Image can fit in a GPU texture, use image space drawing. */
      BKE_image_ensure_gpu_texture(this->state.image, space_->get_image_user());
      gpu::Texture *texture = BKE_image_get_gpu_viewer_texture(
          this->state.image, space_->get_image_user(), buffer);
      return std::make_unique<ImageSpaceDrawingMode>(*this, texture);
    }

    for (ImageTile &tile : this->state.image->tiles) {
      ImageTileWrapper image_tile(&tile);
      ImageUser tile_user = space_->get_image_user() ? *space_->get_image_user() :
                                                       ImageUser{.scene = nullptr};
      tile_user.tile = image_tile.get_tile_number();
      ImBuf *buffer = BKE_image_acquire_ibuf_gpu(this->state.image, &tile_user, nullptr);
      BLI_SCOPED_DEFER([&]() { BKE_image_release_ibuf(this->state.image, buffer, nullptr); });
      if (!buffer) {
        continue;
      }

      /* Image will not fit in a GPU texture, use screen space drawing. */
      if (!GPU_is_safe_texture_size(buffer->x, buffer->y)) {
        return std::make_unique<ScreenSpaceDrawingMode>(*this);
      }
    }

    /* Image can fit in a GPU texture, use image space drawing. */
    BKE_image_ensure_gpu_texture(this->state.image, space_->get_image_user());
    ImageGPUTextures gpu_tiles_textures = BKE_image_get_gpu_material_texture(
        this->state.image, space_->get_image_user(), true);
    return std::make_unique<ImageSpaceDrawingMode>(
        *this, *gpu_tiles_textures.texture, *gpu_tiles_textures.tile_mapping);
  }

  void begin_sync() final
  {
    /* Setup full screen view matrix. */
    float4x4 viewmat = math::projection::orthographic(
        0.0f, float(region->winx), 0.0f, float(region->winy), 0.0f, 1.0f);
    float4x4 winmat = float4x4::identity();
    state.view.sync(viewmat, winmat);
    state.flags.do_tile_drawing = false;

    this->image_sync();
    drawing_mode_.reset();
    this->state.float_buffers.reset_usage_flags();
    if (this->state.image) {
      this->drawing_mode_ = this->get_drawing_mode();
      drawing_mode_->begin_sync();
      drawing_mode_->image_sync(state.image, space_->get_image_user());
    }
  }

  /* Computes a transformation matrix from the normalized screen space coordinates with half pixel
   * offsets into the image sampler space. */
  float3x3 compute_screen_space_to_sampler_space_transformation(const float2 output_size,
                                                                const float2 image_offset,
                                                                const float2 image_size,
                                                                const float2 pan_offset,
                                                                const float zoom,
                                                                const float aspect_ratio) const
  {
    /* Transforms output normalized screen coordinates with half pixel offsets into integer data
     * coordinates. */
    const float3x3 output_screen_uv_to_output_texel = math::from_scale<float3x3, 2>(output_size);
    const float3x3 output_texel_to_output_data = math::from_location<float3x3>(float2(-0.5f));

    /* Transforms output data coordinates into the centered virtual space. */
    const float2 output_center = float2(output_size) / 2.0f;
    const float2 output_translation = -output_center;
    const float3x3 output_data_to_virtual = math::from_location<float3x3>(output_translation);

    /* Transform the image in pixel space based on the pan offset, zoom, and aspect ratio. */
    const float3x3 image_transformation = math::from_loc_scale<float3x3>(
        pan_offset, float2(zoom) * float2(1.0f, aspect_ratio));

    /* Transforms image data coordinates into the centered virtual space with the image offset. We
     * also bias translations to avoids the round-to-even behavior of some GPUs at pixel
     * boundaries. */
    const float2 image_center = image_size / 2.0f;
    const float2 corrective_translation = float2(std::numeric_limits<float>::epsilon() * 10e3f);
    const float2 image_translation = image_offset - image_center + corrective_translation;
    const float3x3 image_data_to_virtual = math::translate(image_transformation,
                                                           image_translation);

    /* Transforms the output data space to the image data space. */
    const float3x3 virtual_to_image_data = math::invert(image_data_to_virtual);
    const float3x3 output_data_to_image_data = virtual_to_image_data * output_data_to_virtual;

    /* Transform from image data coordinates to image sampler normalized coordinates with half
     * pixel offsets. */
    const float3x3 image_data_to_image_texel = math::from_location<float3x3>(float2(0.5f));
    const float3x3 image_texel_to_image_sampler = math::from_scale<float3x3, 2>(1.0f / image_size);

    const float3x3 output_screen_uv_to_image_sampler = image_texel_to_image_sampler *
                                                       image_data_to_image_texel *
                                                       output_data_to_image_data *
                                                       output_texel_to_output_data *
                                                       output_screen_uv_to_output_texel;
    return output_screen_uv_to_image_sampler;
  }

  void image_sync()
  {
    state.image = space_->get_image(main_);
    if (state.image == nullptr) {
      /* Early exit, nothing to draw. */
      return;
    }
    state.flags.do_tile_drawing = state.image->source != IMA_SRC_TILED &&
                                  space_->use_tile_drawing();
    void *lock;
    ImBuf *image_buffer = space_->acquire_image_buffer(state.image, &lock);

    const float2 image_size = float2(image_buffer ? image_buffer->x : 1024.0f,
                                     image_buffer ? image_buffer->y : 1024.0f);
    float2 offset = float2(0.0f);
    if (image_buffer && space_->use_display_window() &&
        (image_buffer->flags & IB_has_display_window))
    {
      offset = float2(image_buffer->display_offset);
    }

    state.ss_to_texture = this->compute_screen_space_to_sampler_space_transformation(
        float2(region->winx, region->winy),
        offset,
        image_size,
        space_->get_pan_offset(),
        space_->get_zoom(),
        space_->get_aspect_ratio());

    const Scene *scene = DRW_context_get()->scene;
    state.sh_params.update(space_.get(), scene, state.image, image_buffer);
    space_->release_buffer(state.image, image_buffer, lock);

    ImageUser *iuser = space_->get_image_user();
    if (state.image->rr != nullptr) {
      BKE_image_multilayer_index(state.image->rr, iuser);
    }
    else {
      BKE_image_multiview_index(state.image, iuser);
    }
  }

  void object_sync(ObjectRef & /*obref*/, Manager & /*manager*/) final {}

  void end_sync() final {}

  void draw(Manager & /*manager*/) final
  {
    DRW_submission_start();
    if (drawing_mode_) {
      drawing_mode_->draw_viewport();
    }
    else {
      GPU_framebuffer_clear_color_depth(
          DRW_context_get()->viewport_framebuffer_list_get()->default_fb, double4(0.0), 1.0f);
    }
    this->state.float_buffers.remove_unused_buffers();
    state.image = nullptr;
    drawing_mode_.reset();
    DRW_submission_end();
  }
};
}  // namespace blender::image_engine
