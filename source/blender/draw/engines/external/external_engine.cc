/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Base engine for external render engines.
 * We use it for depth and non-mesh objects.
 */

#include "DRW_render.h"

#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_object.h"
#include "BKE_particle.h"

#include "ED_image.h"
#include "ED_screen.h"

#include "GPU_batch.h"
#include "GPU_debug.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_viewport.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "external_engine.h" /* own include */

/* Shaders */

#define EXTERNAL_ENGINE "BLENDER_EXTERNAL"

extern char datatoc_basic_depth_frag_glsl[];
extern char datatoc_basic_depth_vert_glsl[];

extern char datatoc_common_view_lib_glsl[];

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed every time the viewport engine changes. */
typedef struct EXTERNAL_Storage {
  int dummy;
} EXTERNAL_Storage;

typedef struct EXTERNAL_StorageList {
  EXTERNAL_Storage *storage;
  struct EXTERNAL_PrivateData *g_data;
} EXTERNAL_StorageList;

typedef struct EXTERNAL_FramebufferList {
  GPUFrameBuffer *depth_buffer_fb;
} EXTERNAL_FramebufferList;

typedef struct EXTERNAL_TextureList {
  /* default */
  GPUTexture *depth_buffer_tx;
} EXTERNAL_TextureList;

typedef struct EXTERNAL_PassList {
  DRWPass *depth_pass;
} EXTERNAL_PassList;

typedef struct EXTERNAL_Data {
  void *engine_type;
  EXTERNAL_FramebufferList *fbl;
  EXTERNAL_TextureList *txl;
  EXTERNAL_PassList *psl;
  EXTERNAL_StorageList *stl;
  void *instance_data;

  char info[GPU_INFO_SIZE];
} EXTERNAL_Data;

/* *********** STATIC *********** */

static struct {
  /* Depth Pre Pass */
  GPUShader *depth_sh;
} e_data = {nullptr}; /* Engine data */

typedef struct EXTERNAL_PrivateData {
  DRWShadingGroup *depth_shgrp;

  /* Do we need to update the depth or can we reuse the last calculated texture. */
  bool need_depth;
  bool update_depth;
} EXTERNAL_PrivateData; /* Transient data */

/* Functions */

static void external_engine_init(void *vedata)
{
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  /* Depth pre-pass. */
  if (!e_data.depth_sh) {
    /* NOTE: Reuse Basic engine depth only shader. */
    e_data.depth_sh = GPU_shader_create_from_info_name("basic_depth_mesh");
  }

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_cnew<EXTERNAL_PrivateData>(__func__);
    stl->g_data->need_depth = true;
  }

  stl->g_data->update_depth = true;

  /* Progressive render samples are tagged with no rebuild, in that case we
   * can skip updating the depth buffer */
  if (region && (region->do_draw & RGN_DRAW_NO_REBUILD)) {
    stl->g_data->update_depth = false;
  }
}

/* Add shading group call which will take care of writing to the depth buffer, so that the
 * alpha-under overlay will happen for the render buffer. */
static void external_cache_image_add(DRWShadingGroup *grp)
{
  float obmat[4][4];
  unit_m4(obmat);
  scale_m4_fl(obmat, 0.5f);

  /* NOTE: Use the same Z-depth value as in the regular image drawing engine. */
  translate_m4(obmat, 1.0f, 1.0f, 0.75f);

  GPUBatch *geom = DRW_cache_quad_get();

  DRW_shgroup_call_obmat(grp, geom, obmat);
}

static void external_cache_init(void *vedata)
{
  EXTERNAL_PassList *psl = ((EXTERNAL_Data *)vedata)->psl;
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  EXTERNAL_TextureList *txl = ((EXTERNAL_Data *)vedata)->txl;
  EXTERNAL_FramebufferList *fbl = ((EXTERNAL_Data *)vedata)->fbl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;

  {
    DRW_texture_ensure_fullscreen_2d(
        &txl->depth_buffer_tx, GPU_DEPTH24_STENCIL8, DRWTextureFlag(0));

    GPU_framebuffer_ensure_config(&fbl->depth_buffer_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(txl->depth_buffer_tx),
                                  });
  }

  /* Depth Pass */
  {
    psl->depth_pass = DRW_pass_create("Depth Pass",
                                      DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
    stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
  }

  if (v3d != nullptr) {
    /* Do not draw depth pass when overlays are turned off. */
    stl->g_data->need_depth = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
  }
  else if (draw_ctx->space_data != nullptr) {
    const eSpace_Type space_type = eSpace_Type(draw_ctx->space_data->spacetype);
    if (space_type == SPACE_IMAGE) {
      external_cache_image_add(stl->g_data->depth_shgrp);

      stl->g_data->need_depth = true;
      stl->g_data->update_depth = true;
    }
  }
}

static void external_cache_populate(void *vedata, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;

  if (draw_ctx->space_data != nullptr) {
    const eSpace_Type space_type = eSpace_Type(draw_ctx->space_data->spacetype);
    if (space_type == SPACE_IMAGE) {
      return;
    }
  }

  if (!(DRW_object_is_renderable(ob) &&
        DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF))
  {
    return;
  }

  if (ob->type == OB_GPENCIL_LEGACY) {
    /* Grease Pencil objects need correct depth to do the blending. */
    stl->g_data->need_depth = true;
    return;
  }

  if (ob->type == OB_MESH && ob->modifiers.first != nullptr) {
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type != eModifierType_ParticleSystem) {
        continue;
      }
      ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
      if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        continue;
      }
      ParticleSettings *part = psys->part;
      const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

      if (draw_as == PART_DRAW_PATH) {
        GPUBatch *hairs = DRW_cache_particles_get_hair(ob, psys, nullptr);
        DRW_shgroup_call(stl->g_data->depth_shgrp, hairs, nullptr);
      }
    }
  }
  GPUBatch *geom = DRW_cache_object_surface_get(ob);
  if (geom) {
    /* Depth Pre-pass. */
    DRW_shgroup_call(stl->g_data->depth_shgrp, geom, ob);
  }
}

static void external_cache_finish(void * /*vedata*/) {}

static void external_draw_scene_do_v3d(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  RegionView3D *rv3d = draw_ctx->rv3d;
  ARegion *region = draw_ctx->region;

  DRW_state_reset_ex(DRW_STATE_WRITE_COLOR);

  /* The external engine can use the OpenGL rendering API directly, so make sure the state is
   * already applied. */
  GPU_apply_state();

  /* Create render engine. */
  if (!rv3d->render_engine) {
    RenderEngineType *engine_type = draw_ctx->engine_type;

    if (!(engine_type->view_update && engine_type->view_draw)) {
      return;
    }

    RenderEngine *engine = RE_engine_create(engine_type);
    engine_type->view_update(engine, draw_ctx->evil_C, draw_ctx->depsgraph);
    rv3d->render_engine = engine;
  }

  /* Rendered draw. */
  GPU_matrix_push_projection();
  GPU_matrix_push();
  ED_region_pixelspace(region);

  /* Render result draw. */
  const RenderEngineType *type = rv3d->render_engine->type;
  type->view_draw(rv3d->render_engine, draw_ctx->evil_C, draw_ctx->depsgraph);

  GPU_bgl_end();

  GPU_matrix_pop();
  GPU_matrix_pop_projection();

  /* Set render info. */
  EXTERNAL_Data *data = static_cast<EXTERNAL_Data *>(vedata);
  if (rv3d->render_engine->text[0] != '\0') {
    STRNCPY(data->info, rv3d->render_engine->text);
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
  const DRWView *view = DRW_view_get_active();
  SpaceImage *space_image = (SpaceImage *)draw_ctx->space_data;

  /* Apply current view as transformation matrix.
   * This will configure drawing for normalized space with current zoom and pan applied. */

  float view_matrix[4][4];
  DRW_view_viewmat_get(view, view_matrix, false);

  float projection_matrix[4][4];
  DRW_view_winmat_get(view, projection_matrix, false);

  GPU_matrix_projection_set(projection_matrix);
  GPU_matrix_set(view_matrix);

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

  DRW_state_reset_ex(DRW_STATE_WRITE_COLOR);

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

  DRW_state_reset();
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
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  EXTERNAL_PassList *psl = ((EXTERNAL_Data *)vedata)->psl;
  EXTERNAL_FramebufferList *fbl = ((EXTERNAL_Data *)vedata)->fbl;
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

  if (stl->g_data->update_depth && stl->g_data->need_depth) {
    DRW_draw_pass(psl->depth_pass);
    /* Copy main depth buffer to cached framebuffer. */
    GPU_framebuffer_blit(dfbl->depth_only_fb, 0, fbl->depth_buffer_fb, 0, GPU_DEPTH_BIT);
  }

  /* Copy cached depth buffer to main framebuffer. */
  GPU_framebuffer_blit(fbl->depth_buffer_fb, 0, dfbl->depth_only_fb, 0, GPU_DEPTH_BIT);
}

static void external_engine_free()
{
  DRW_SHADER_FREE_SAFE(e_data.depth_sh);
}

static const DrawEngineDataSize external_data_size = DRW_VIEWPORT_DATA_SIZE(EXTERNAL_Data);

DrawEngineType draw_engine_external_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("External"),
    /*vedata_size*/ &external_data_size,
    /*engine_init*/ &external_engine_init,
    /*engine_free*/ &external_engine_free,
    /*instance_free*/ nullptr,
    /*cache_init*/ &external_cache_init,
    /*cache_populate*/ &external_cache_populate,
    /*cache_finish*/ &external_cache_finish,
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

#undef EXTERNAL_ENGINE
