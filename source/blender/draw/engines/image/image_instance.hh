/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <DRW_render.hh>

#include "BKE_context.hh"

#include "DRW_engine.hh"

#include "image_drawing_mode.hh"
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
  Main *main_;

  ScreenSpaceDrawingMode drawing_mode_;

 public:
  const ARegion *region;
  State state;
  Manager *manager = nullptr;

 public:
  Instance() : drawing_mode_(*this) {}

  virtual ~Instance() = default;

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

  void begin_sync() final
  {
    drawing_mode_.begin_sync();

    /* Setup full screen view matrix. */
    float4x4 viewmat = math::projection::orthographic(
        0.0f, float(region->winx), 0.0f, float(region->winy), 0.0f, 1.0f);
    float4x4 winmat = float4x4::identity();
    state.view.sync(viewmat, winmat);
    state.flags.do_tile_drawing = false;

    image_sync();
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
    space_->init_ss_to_texture_matrix(
        region, state.image->runtime->backdrop_offset, image_resolution, state.ss_to_texture);

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
    drawing_mode_.image_sync(state.image, iuser);
  }

  void object_sync(ObjectRef & /*obref*/, Manager & /*manager*/) final {}

  void end_sync() final {}

  void draw(Manager & /*manager*/) final
  {
    DRW_submission_start();
    drawing_mode_.draw_viewport();
    drawing_mode_.draw_finish();
    state.image = nullptr;
    DRW_submission_end();
  }
};
}  // namespace blender::image_engine
