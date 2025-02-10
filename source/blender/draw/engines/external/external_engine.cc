/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Base engine for external render engines.
 * We use it for depth and non-mesh objects.
 */

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "ED_image.hh"
#include "ED_screen.hh"

#include "GPU_debug.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "draw_command.hh"
#include "draw_view.hh"
#include "draw_view_data.hh"

#include "external_engine.h" /* own include */

/* Shaders */

#define EXTERNAL_ENGINE "BLENDER_EXTERNAL"

struct EXTERNAL_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  void *instance_data;

  char info[GPU_INFO_SIZE];
};

/* Functions */

static void external_draw_scene_do_v3d(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  RegionView3D *rv3d = draw_ctx->rv3d;
  ARegion *region = draw_ctx->region;

  blender::draw::command::StateSet::set(DRW_STATE_WRITE_COLOR);

  /* The external engine can use the OpenGL rendering API directly, so make sure the state is
   * already applied. */
  GPU_apply_state();

  /* Create render engine. */
  RenderEngine *render_engine = nullptr;
  if (!rv3d->view_render) {
    RenderEngineType *engine_type = draw_ctx->engine_type;

    if (!(engine_type->view_update && engine_type->view_draw)) {
      return;
    }

    rv3d->view_render = RE_NewViewRender(engine_type);
    render_engine = RE_view_engine_get(rv3d->view_render);
    engine_type->view_update(render_engine, draw_ctx->evil_C, draw_ctx->depsgraph);
  }
  else {
    render_engine = RE_view_engine_get(rv3d->view_render);
  }

  /* Rendered draw. */
  GPU_matrix_push_projection();
  GPU_matrix_push();
  ED_region_pixelspace(region);

  /* Render result draw. */
  const RenderEngineType *type = render_engine->type;
  type->view_draw(render_engine, draw_ctx->evil_C, draw_ctx->depsgraph);

  GPU_bgl_end();

  GPU_matrix_pop();
  GPU_matrix_pop_projection();

  /* Set render info. */
  EXTERNAL_Data *data = static_cast<EXTERNAL_Data *>(vedata);
  if (render_engine->text[0] != '\0') {
    STRNCPY(data->info, render_engine->text);
  }
  else {
    data->info[0] = '\0';
  }
}

/* Configure current matrix stack so that the external engine can use the same drawing code for
 * both viewport and image editor drawing.
 *
 * The engine draws result in the pixel space, and is applying render offset. For image editor we
 * need to switch from normalized space to pixel space, and "un-apply" offset. */
static void external_image_space_matrix_set(const RenderEngine *engine)
{
  BLI_assert(engine != nullptr);

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SpaceImage *space_image = (SpaceImage *)draw_ctx->space_data;

  /* Apply current view as transformation matrix.
   * This will configure drawing for normalized space with current zoom and pan applied. */

  float4x4 view_matrix = blender::draw::View::default_get().viewmat();
  float4x4 projection_matrix = blender::draw::View::default_get().winmat();

  GPU_matrix_projection_set(projection_matrix.ptr());
  GPU_matrix_set(view_matrix.ptr());

  /* Switch from normalized space to pixel space. */
  {
    int width, height;
    ED_space_image_get_size(space_image, &width, &height);

    const float width_inv = width ? 1.0f / width : 0.0f;
    const float height_inv = height ? 1.0f / height : 0.0f;
    GPU_matrix_scale_2f(width_inv, height_inv);
  }

  /* Un-apply render offset. */
  {
    Render *render = engine->re;
    rctf view_rect;
    rcti render_rect;
    RE_GetViewPlane(render, &view_rect, &render_rect);

    GPU_matrix_translate_2f(-render_rect.xmin, -render_rect.ymin);
  }
}

static void external_draw_scene_do_image(void * /*vedata*/)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  Render *re = RE_GetSceneRender(scene);
  RenderEngine *engine = RE_engine_get(re);

  /* Is tested before enabling the drawing engine. */
  BLI_assert(re != nullptr);
  BLI_assert(engine != nullptr);

  blender::draw::command::StateSet::set(DRW_STATE_WRITE_COLOR);

  /* The external engine can use the OpenGL rendering API directly, so make sure the state is
   * already applied. */
  GPU_apply_state();

  const DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  /* Clear the depth buffer to the value used by the background overlay so that the overlay is not
   * happening outside of the drawn image.
   *
   * NOTE: The external engine only draws color. The depth is taken care of using the depth pass
   * which initialized the depth to the values expected by the background overlay. */
  GPU_framebuffer_clear_depth(dfbl->default_fb, 1.0f);

  GPU_matrix_push_projection();
  GPU_matrix_push();

  external_image_space_matrix_set(engine);

  GPU_debug_group_begin("External Engine");

  const RenderEngineType *engine_type = engine->type;
  BLI_assert(engine_type != nullptr);
  BLI_assert(engine_type->draw != nullptr);

  engine_type->draw(engine, draw_ctx->evil_C, draw_ctx->depsgraph);

  GPU_debug_group_end();

  GPU_matrix_pop();
  GPU_matrix_pop_projection();

  blender::draw::command::StateSet::set();
  GPU_bgl_end();

  RE_engine_draw_release(re);
}

static void external_draw_scene_do(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (draw_ctx->v3d != nullptr) {
    external_draw_scene_do_v3d(vedata);
    return;
  }

  if (draw_ctx->space_data == nullptr) {
    return;
  }

  const eSpace_Type space_type = eSpace_Type(draw_ctx->space_data->spacetype);
  if (space_type == SPACE_IMAGE) {
    external_draw_scene_do_image(vedata);
    return;
  }
}

static void external_draw_scene(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  /* Will be nullptr during OpenGL render.
   * OpenGL render is used for quick preview (thumbnails or sequencer preview)
   * where using the rendering engine to preview doesn't make so much sense. */
  if (draw_ctx->evil_C) {
    const float clear_col[4] = {0, 0, 0, 0};
    /* This is to keep compatibility with external engine. */
    /* TODO(fclem): remove it eventually. */
    GPU_framebuffer_bind(dfbl->default_fb);
    GPU_framebuffer_clear_color(dfbl->default_fb, clear_col);

    external_draw_scene_do(vedata);
  }
}

static const DrawEngineDataSize external_data_size = DRW_VIEWPORT_DATA_SIZE(EXTERNAL_Data);

DrawEngineType draw_engine_external_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("External"),
    /*vedata_size*/ &external_data_size,
    /*engine_init*/ nullptr,
    /*engine_free*/ nullptr,
    /*instance_free*/ nullptr,
    /*cache_init*/ nullptr,
    /*cache_populate*/ nullptr,
    /*cache_finish*/ nullptr,
    /*draw_scene*/ &external_draw_scene,
    /*view_update*/ nullptr,
    /*id_update*/ nullptr,
    /*render_to_image*/ nullptr,
    /*store_metadata*/ nullptr,
};

/* NOTE: currently unused,
 * we should not register unless we want to see this when debugging the view. */

RenderEngineType DRW_engine_viewport_external_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ EXTERNAL_ENGINE,
    /*name*/ N_("External"),
    /*flag*/ RE_INTERNAL | RE_USE_STEREO_VIEWPORT,
    /*update*/ nullptr,
    /*render*/ nullptr,
    /*render_frame_finish*/ nullptr,
    /*draw*/ nullptr,
    /*bake*/ nullptr,
    /*view_update*/ nullptr,
    /*view_draw*/ nullptr,
    /*update_script_node*/ nullptr,
    /*update_render_passes*/ nullptr,
    /*draw_engine*/ &draw_engine_external_type,
    /*rna_ext*/
    {
        /*data*/ nullptr,
        /*srna*/ nullptr,
        /*call*/ nullptr,
    },
};

bool DRW_engine_external_acquire_for_image_editor()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const SpaceLink *space_data = draw_ctx->space_data;
  Scene *scene = draw_ctx->scene;

  if (space_data == nullptr) {
    return false;
  }

  const eSpace_Type space_type = eSpace_Type(draw_ctx->space_data->spacetype);
  if (space_type != SPACE_IMAGE) {
    return false;
  }

  SpaceImage *space_image = (SpaceImage *)space_data;
  const Image *image = ED_space_image(space_image);
  if (image == nullptr || image->type != IMA_TYPE_R_RESULT) {
    return false;
  }

  if (image->render_slot != image->last_render_slot) {
    return false;
  }

  /* Render is allocated on main thread, so it is safe to access it from here. */
  Render *re = RE_GetSceneRender(scene);

  if (re == nullptr) {
    return false;
  }

  return RE_engine_draw_acquire(re);
}

void DRW_engine_external_free(RegionView3D *rv3d)
{
  if (rv3d->view_render) {
    /* Free engine with DRW context enabled, as this may clean up per-context
     * resources like VAOs. */
    DRW_gpu_context_enable_ex(true);
    RE_FreeViewRender(rv3d->view_render);
    rv3d->view_render = nullptr;
    DRW_gpu_context_disable_ex(true);
  }
}

#undef EXTERNAL_ENGINE
