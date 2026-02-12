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
      const bool is_viewer = this->state.image->source == IMA_SRC_VIEWER;
      ImBuf *buffer = BKE_image_acquire_ibuf(
          this->state.image, space_->get_image_user(), is_viewer ? &lock : nullptr);
      BLI_SCOPED_DEFER([&]() {
        BKE_image_release_ibuf(this->state.image, buffer, is_viewer ? lock : nullptr);
      });

      /* The image buffer already have a GPU texture, so use image space drawing. */
      if (buffer && buffer->gpu.texture) {
        return std::make_unique<ImageSpaceDrawingMode>(*this);
      }

      /* Buffer does not exist or image will not fit in a GPU texture, use screen space drawing. */
      if (!buffer || (!buffer->float_buffer.data && !buffer->byte_buffer.data) ||
          !GPU_is_safe_texture_size(buffer->x, buffer->y))
      {
        return std::make_unique<ScreenSpaceDrawingMode>(*this);
      }

      /* Image can fit in a GPU texture, use image space drawing. */
      return std::make_unique<ImageSpaceDrawingMode>(*this);
    }

    for (ImageTile &tile : this->state.image->tiles) {
      ImageTileWrapper image_tile(&tile);
      ImageUser tile_user = space_->get_image_user() ? *space_->get_image_user() :
                                                       ImageUser{.scene = nullptr};
      tile_user.tile = image_tile.get_tile_number();
      ImBuf *buffer = BKE_image_acquire_ibuf(this->state.image, &tile_user, nullptr);
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
    return std::make_unique<ImageSpaceDrawingMode>(*this);
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
    if (this->state.image) {
      this->drawing_mode_ = this->get_drawing_mode();
      drawing_mode_->begin_sync();
      drawing_mode_->image_sync(state.image, space_->get_image_user());
    }
    else {
      drawing_mode_.reset();
    }
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

    /* Setup the matrix to go from screen UV coordinates to UV texture space coordinates. */
    float image_resolution[2] = {image_buffer ? image_buffer->x : 1024.0f,
                                 image_buffer ? image_buffer->y : 1024.0f};
    float2 offset = float2(0.0f);
    if (image_buffer && space_->use_display_window() &&
        (image_buffer->flags & IB_has_display_window))
    {
      offset = float2(image_buffer->display_offset);
    }
    space_->init_ss_to_texture_matrix(region, offset, image_resolution, state.ss_to_texture);

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
      drawing_mode_->draw_finish();
    }
    else {
      GPU_framebuffer_clear_color_depth(
          DRW_context_get()->viewport_framebuffer_list_get()->default_fb, float4(0.0), 1.0f);
    }
    state.image = nullptr;
    DRW_submission_end();
  }
};
}  // namespace blender::image_engine
