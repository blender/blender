/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Draw engine to draw the Image/UV editor
 */

#include "DRW_render.h"

#include <memory>
#include <optional>

#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_object.h"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_image.h"

#include "GPU_batch.h"

#include "image_drawing_mode.hh"
#include "image_engine.h"
#include "image_private.hh"
#include "image_space_image.hh"
#include "image_space_node.hh"

namespace blender::draw::image_engine {

static std::unique_ptr<AbstractSpaceAccessor> space_accessor_from_context(
    const DRWContextState *draw_ctx)
{
  const char space_type = draw_ctx->space_data->spacetype;
  if (space_type == SPACE_IMAGE) {
    return std::make_unique<SpaceImageAccessor>((SpaceImage *)draw_ctx->space_data);
  }
  if (space_type == SPACE_NODE) {
    return std::make_unique<SpaceNodeAccessor>((SpaceNode *)draw_ctx->space_data);
  }
  BLI_assert_unreachable();
  return nullptr;
}

template<
    /** \brief Drawing mode to use.
     *
     * Useful during development to switch between drawing implementations.
     */
    typename DrawingMode = ScreenSpaceDrawingMode<OneTexture>>
class ImageEngine {
 private:
  const DRWContextState *draw_ctx;
  IMAGE_Data *vedata;
  std::unique_ptr<AbstractSpaceAccessor> space;
  DrawingMode drawing_mode;

 public:
  ImageEngine(const DRWContextState *draw_ctx, IMAGE_Data *vedata)
      : draw_ctx(draw_ctx), vedata(vedata), space(space_accessor_from_context(draw_ctx))
  {
  }

  virtual ~ImageEngine() = default;

  void begin_sync()
  {
    IMAGE_InstanceData *instance_data = vedata->instance_data;
    drawing_mode.begin_sync(vedata);

    /* Setup full screen view matrix. */
    const ARegion *region = draw_ctx->region;
    float winmat[4][4], viewmat[4][4];
    orthographic_m4(viewmat, 0.0, region->winx, 0.0, region->winy, 0.0, 1.0);
    unit_m4(winmat);
    instance_data->view = DRW_view_create(viewmat, winmat, nullptr, nullptr, nullptr);
  }

  void image_sync()
  {
    IMAGE_InstanceData *instance_data = vedata->instance_data;
    Main *bmain = CTX_data_main(draw_ctx->evil_C);
    instance_data->image = space->get_image(bmain);
    if (instance_data->image == nullptr) {
      /* Early exit, nothing to draw. */
      return;
    }
    instance_data->flags.do_tile_drawing = instance_data->image->source != IMA_SRC_TILED &&
                                           space->use_tile_drawing();
    void *lock;
    ImBuf *image_buffer = space->acquire_image_buffer(instance_data->image, &lock);

    /* Setup the matrix to go from screen UV coordinates to UV texture space coordinates. */
    float image_resolution[2] = {image_buffer ? image_buffer->x : 1024.0f,
                                 image_buffer ? image_buffer->y : 1024.0f};
    float image_offset[2] = {float(instance_data->image->offset_x),
                             float(instance_data->image->offset_y)};
    space->init_ss_to_texture_matrix(
        draw_ctx->region, image_offset, image_resolution, instance_data->ss_to_texture);

    const Scene *scene = DRW_context_state_get()->scene;
    instance_data->sh_params.update(space.get(), scene, instance_data->image, image_buffer);
    space->release_buffer(instance_data->image, image_buffer, lock);

    ImageUser *iuser = space->get_image_user();
    if (instance_data->image->rr != nullptr) {
      BKE_image_multilayer_index(instance_data->image->rr, iuser);
    }
    else {
      BKE_image_multiview_index(instance_data->image, iuser);
    }
    drawing_mode.image_sync(vedata, instance_data->image, iuser);
  }

  void draw_finish()
  {
    drawing_mode.draw_finish(vedata);

    IMAGE_InstanceData *instance_data = vedata->instance_data;
    instance_data->image = nullptr;
  }

  void draw_viewport()
  {
    drawing_mode.draw_viewport(vedata);
  }
};

/* -------------------------------------------------------------------- */
/** \name Engine Callbacks
 * \{ */

static void IMAGE_engine_init(void *ved)
{
  IMAGE_Data *vedata = (IMAGE_Data *)ved;
  if (vedata->instance_data == nullptr) {
    vedata->instance_data = MEM_new<IMAGE_InstanceData>(__func__);
  }
}

static void IMAGE_cache_init(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ImageEngine image_engine(draw_ctx, static_cast<IMAGE_Data *>(vedata));
  image_engine.begin_sync();
  image_engine.image_sync();
}

static void IMAGE_cache_populate(void * /*vedata*/, Object * /*ob*/)
{
  /* Function intentional left empty. `cache_populate` is required to be implemented. */
}

static void IMAGE_draw_scene(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ImageEngine image_engine(draw_ctx, static_cast<IMAGE_Data *>(vedata));
  image_engine.draw_viewport();
  image_engine.draw_finish();
}

static void IMAGE_engine_free()
{
  IMAGE_shader_free();
}

static void IMAGE_instance_free(void *_instance_data)
{
  IMAGE_InstanceData *instance_data = reinterpret_cast<IMAGE_InstanceData *>(_instance_data);
  MEM_delete(instance_data);
}

/** \} */

static const DrawEngineDataSize IMAGE_data_size = DRW_VIEWPORT_DATA_SIZE(IMAGE_Data);

}  // namespace blender::draw::image_engine

extern "C" {

using namespace blender::draw::image_engine;

DrawEngineType draw_engine_image_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("UV/Image"),
    /*vedata_size*/ &IMAGE_data_size,
    /*engine_init*/ &IMAGE_engine_init,
    /*engine_free*/ &IMAGE_engine_free,
    /*instance_free*/ &IMAGE_instance_free,
    /*cache_init*/ &IMAGE_cache_init,
    /*cache_populate*/ &IMAGE_cache_populate,
    /*cache_finish*/ nullptr,
    /*draw_scene*/ &IMAGE_draw_scene,
    /*view_update*/ nullptr,
    /*id_update*/ nullptr,
    /*render_to_image*/ nullptr,
    /*store_metadata*/ nullptr,
};
}
