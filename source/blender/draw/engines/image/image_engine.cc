/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Draw engine to draw the Image/UV editor
 */

#include "DRW_render.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "ED_image.hh"

#include "draw_view_data.hh"

#include "image_drawing_mode.hh"
#include "image_engine.h"
#include "image_instance.hh"
#include "image_shader.hh"

namespace blender::image_engine {

struct IMAGE_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  Instance *instance;
  char info[GPU_INFO_SIZE];
};

/* -------------------------------------------------------------------- */
/** \name Engine Callbacks
 * \{ */

static void IMAGE_engine_init(void *vedata)
{
  IMAGE_Data *ved = reinterpret_cast<IMAGE_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new image_engine::Instance();
  }

  const DRWContextState *ctx_state = DRW_context_state_get();
  Main *bmain = CTX_data_main(ctx_state->evil_C);
  ved->instance->init(bmain, ctx_state->space_data, ctx_state->region);
}

static void IMAGE_cache_init(void *vedata)
{
  IMAGE_Data *ved = reinterpret_cast<IMAGE_Data *>(vedata);
  ved->instance->begin_sync();
  ved->instance->image_sync();
}

static void IMAGE_cache_populate(void * /*vedata*/, Object * /*ob*/)
{
  /* Function intentional left empty. `cache_populate` is required to be implemented. */
}

static void IMAGE_draw_scene(void *vedata)
{
  IMAGE_Data *ved = reinterpret_cast<IMAGE_Data *>(vedata);
  ved->instance->draw_viewport();
  ved->instance->draw_finish();
}

static void IMAGE_engine_free()
{
  ShaderModule::module_free();
}

static void IMAGE_instance_free(void *instance)
{
  delete reinterpret_cast<image_engine::Instance *>(instance);
}

/** \} */

static const DrawEngineDataSize IMAGE_data_size = DRW_VIEWPORT_DATA_SIZE(IMAGE_Data);

}  // namespace blender::image_engine

extern "C" {

using namespace blender::image_engine;

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
