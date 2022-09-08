/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

#include "BKE_global.h"
#include "BLI_rect.h"

#include "GPU_capabilities.h"
#include "GPU_framebuffer.h"

#include "ED_view3d.h"

#include "DRW_render.h"

#include "RE_pipeline.h"

#include "eevee_engine.h" /* Own include. */

#include "eevee_instance.hh"

using namespace blender;

struct EEVEE_Data {
  DrawEngineType *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  eevee::Instance *instance;

  char info[GPU_INFO_SIZE];
};

static void eevee_engine_init(void *vedata)
{
  /* TODO(fclem): Remove once it is minimum required. */
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  EEVEE_Data *ved = reinterpret_cast<EEVEE_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new eevee::Instance();
  }

  const DRWContextState *ctx_state = DRW_context_state_get();
  Depsgraph *depsgraph = ctx_state->depsgraph;
  Scene *scene = ctx_state->scene;
  View3D *v3d = ctx_state->v3d;
  const ARegion *region = ctx_state->region;
  RegionView3D *rv3d = ctx_state->rv3d;

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  int2 size = int2(GPU_texture_width(dtxl->color), GPU_texture_height(dtxl->color));

  const DRWView *default_view = DRW_view_default_get();

  Object *camera = nullptr;
  /* Get render borders. */
  rcti rect;
  BLI_rcti_init(&rect, 0, size[0], 0, size[1]);
  if (v3d) {
    if (rv3d && (rv3d->persp == RV3D_CAMOB)) {
      camera = v3d->camera;
    }

    if (v3d->flag2 & V3D_RENDER_BORDER) {
      if (camera) {
        rctf viewborder;
        /* TODO(fclem) Might be better to get it from DRW. */
        ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, &viewborder, false);
        float viewborder_sizex = BLI_rctf_size_x(&viewborder);
        float viewborder_sizey = BLI_rctf_size_y(&viewborder);
        rect.xmin = floorf(viewborder.xmin + (scene->r.border.xmin * viewborder_sizex));
        rect.ymin = floorf(viewborder.ymin + (scene->r.border.ymin * viewborder_sizey));
        rect.xmax = floorf(viewborder.xmin + (scene->r.border.xmax * viewborder_sizex));
        rect.ymax = floorf(viewborder.ymin + (scene->r.border.ymax * viewborder_sizey));
      }
      else {
        rect.xmin = v3d->render_border.xmin * size[0];
        rect.ymin = v3d->render_border.ymin * size[1];
        rect.xmax = v3d->render_border.xmax * size[0];
        rect.ymax = v3d->render_border.ymax * size[1];
      }
    }
  }

  ved->instance->init(
      size, &rect, nullptr, depsgraph, nullptr, camera, nullptr, default_view, v3d, rv3d);
}

static void eevee_draw_scene(void *vedata)
{
  EEVEE_Data *ved = reinterpret_cast<EEVEE_Data *>(vedata);
  if (!GPU_shader_storage_buffer_objects_support()) {
    STRNCPY(ved->info, "Error: No shader storage buffer support");
    return;
  }
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  ved->instance->draw_viewport(dfbl);
  STRNCPY(ved->info, ved->instance->info.c_str());
  /* Reset view for other following engines. */
  DRW_view_set_active(nullptr);
}

static void eevee_cache_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<EEVEE_Data *>(vedata)->instance->begin_sync();
}

static void eevee_cache_populate(void *vedata, Object *object)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<EEVEE_Data *>(vedata)->instance->object_sync(object);
}

static void eevee_cache_finish(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<EEVEE_Data *>(vedata)->instance->end_sync();
}

static void eevee_engine_free()
{
  eevee::ShaderModule::module_free();
}

static void eevee_instance_free(void *instance)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  delete reinterpret_cast<eevee::Instance *>(instance);
}

static void eevee_render_to_image(void *UNUSED(vedata),
                                  struct RenderEngine *engine,
                                  struct RenderLayer *layer,
                                  const struct rcti *UNUSED(rect))
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  eevee::Instance *instance = new eevee::Instance();

  Render *render = engine->re;
  Depsgraph *depsgraph = DRW_context_state_get()->depsgraph;
  Object *camera_original_ob = RE_GetCamera(engine->re);
  const char *viewname = RE_GetActiveRenderView(engine->re);
  int size[2] = {engine->resolution_x, engine->resolution_y};

  rctf view_rect;
  rcti rect;
  RE_GetViewPlane(render, &view_rect, &rect);

  instance->init(size, &rect, engine, depsgraph, nullptr, camera_original_ob, layer);
  instance->render_frame(layer, viewname);

  delete instance;
}

static void eevee_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);

#define CHECK_PASS_LEGACY(name, type, channels, chanid) \
  if (view_layer->passflag & (SCE_PASS_##name)) { \
    RE_engine_register_pass( \
        engine, scene, view_layer, RE_PASSNAME_##name, channels, chanid, type); \
  } \
  ((void)0)
#define CHECK_PASS_EEVEE(name, type, channels, chanid) \
  if (view_layer->eevee.render_passes & (EEVEE_RENDER_PASS_##name)) { \
    RE_engine_register_pass( \
        engine, scene, view_layer, RE_PASSNAME_##name, channels, chanid, type); \
  } \
  ((void)0)

  CHECK_PASS_LEGACY(Z, SOCK_FLOAT, 1, "Z");
  CHECK_PASS_LEGACY(MIST, SOCK_FLOAT, 1, "Z");
  CHECK_PASS_LEGACY(NORMAL, SOCK_VECTOR, 3, "XYZ");
  CHECK_PASS_LEGACY(DIFFUSE_DIRECT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(DIFFUSE_COLOR, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(GLOSSY_DIRECT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(GLOSSY_COLOR, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_EEVEE(VOLUME_LIGHT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(EMIT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(ENVIRONMENT, SOCK_RGBA, 3, "RGB");
  /* TODO: CHECK_PASS_LEGACY(SHADOW, SOCK_RGBA, 3, "RGB");
   * CHECK_PASS_LEGACY(AO, SOCK_RGBA, 3, "RGB");
   * When available they should be converted from Value textures to RGB. */

  LISTBASE_FOREACH (ViewLayerAOV *, aov, &view_layer->aovs) {
    if ((aov->flag & AOV_CONFLICT) != 0) {
      continue;
    }
    switch (aov->type) {
      case AOV_TYPE_COLOR:
        RE_engine_register_pass(engine, scene, view_layer, aov->name, 4, "RGBA", SOCK_RGBA);
        break;
      case AOV_TYPE_VALUE:
        RE_engine_register_pass(engine, scene, view_layer, aov->name, 1, "X", SOCK_FLOAT);
        break;
      default:
        break;
    }
  }
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
