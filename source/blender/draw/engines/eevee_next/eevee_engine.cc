/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

#include "BKE_global.h"
#include "BLI_rect.h"

#include "GPU_framebuffer.h"

#include "ED_view3d.h"

#include "DRW_render.h"

struct EEVEE_Data {
  DrawEngineType *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  void *instance;
};

static void eevee_engine_init(void *vedata)
{
  UNUSED_VARS(vedata);
}

static void eevee_draw_scene(void *vedata)
{
  UNUSED_VARS(vedata);
}

static void eevee_cache_init(void *vedata)
{
  UNUSED_VARS(vedata);
}

static void eevee_cache_populate(void *vedata, Object *object)
{
  UNUSED_VARS(vedata, object);
}

static void eevee_cache_finish(void *vedata)
{
  UNUSED_VARS(vedata);
}

static void eevee_engine_free()
{
}

static void eevee_instance_free(void *instance)
{
  UNUSED_VARS(instance);
}

static void eevee_render_to_image(void *UNUSED(vedata),
                                  struct RenderEngine *engine,
                                  struct RenderLayer *layer,
                                  const struct rcti *UNUSED(rect))
{
  UNUSED_VARS(engine, layer);
}

static void eevee_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  UNUSED_VARS(engine, scene, view_layer);
}

static const DrawEngineDataSize eevee_data_size = DRW_VIEWPORT_DATA_SIZE(EEVEE_Data);

extern "C" {

DrawEngineType draw_engine_eevee_next_type = {
    nullptr,
    nullptr,
    N_("Eevee"),
    &eevee_data_size,
    &eevee_engine_init,
    &eevee_engine_free,
    &eevee_instance_free,
    &eevee_cache_init,
    &eevee_cache_populate,
    &eevee_cache_finish,
    &eevee_draw_scene,
    nullptr,
    nullptr,
    &eevee_render_to_image,
    nullptr,
};

RenderEngineType DRW_engine_viewport_eevee_next_type = {
    nullptr,
    nullptr,
    "BLENDER_EEVEE_NEXT",
    N_("Eevee Next"),
    RE_INTERNAL | RE_USE_PREVIEW | RE_USE_STEREO_VIEWPORT | RE_USE_GPU_CONTEXT,
    nullptr,
    &DRW_render_to_image,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &eevee_render_update_passes,
    &draw_engine_eevee_next_type,
    {nullptr, nullptr, nullptr},
};
}
