/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * Workbench Engine:
 *
 * Optimized engine to draw the working viewport with solid and transparent geometry.
 */

#include "DRW_render.h"

#include "BLI_alloca.h"

#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"

#include "DNA_fluid_types.h"
#include "DNA_hair_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "workbench_engine.h"
#include "workbench_private.h"

#define WORKBENCH_ENGINE "BLENDER_WORKBENCH"

void workbench_engine_init(void *ved)
{
  WORKBENCH_Data *vedata = ved;
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_TextureList *txl = vedata->txl;

  workbench_shader_library_ensure();

  if (!stl->wpd) {
    stl->wpd = MEM_callocN(sizeof(*stl->wpd), __func__);
    stl->wpd->taa_sample_len_previous = -1;
    stl->wpd->view_updated = true;
  }

  WORKBENCH_PrivateData *wpd = stl->wpd;
  workbench_private_data_init(wpd);
  workbench_update_world_ubo(wpd);

  if (txl->dummy_image_tx == NULL) {
    float fpixel[4] = {1.0f, 0.0f, 1.0f, 1.0f};
    txl->dummy_image_tx = DRW_texture_create_2d(1, 1, GPU_RGBA8, 0, fpixel);
  }
  wpd->dummy_image_tx = txl->dummy_image_tx;

  if (OBJECT_ID_PASS_ENABLED(wpd)) {
    wpd->object_id_tx = DRW_texture_pool_query_fullscreen(GPU_R16UI, &draw_engine_workbench);
  }
  else {
    /* Dont free because it's a pool texture. */
    wpd->object_id_tx = NULL;
  }

  workbench_opaque_engine_init(vedata);
  workbench_transparent_engine_init(vedata);
  workbench_dof_engine_init(vedata);
  workbench_antialiasing_engine_init(vedata);
  workbench_volume_engine_init(vedata);
}

void workbench_cache_init(void *ved)
{
  WORKBENCH_Data *vedata = ved;

  workbench_opaque_cache_init(vedata);
  workbench_transparent_cache_init(vedata);
  workbench_shadow_cache_init(vedata);
  workbench_cavity_cache_init(vedata);
  workbench_outline_cache_init(vedata);
  workbench_dof_cache_init(vedata);
  workbench_antialiasing_cache_init(vedata);
  workbench_volume_cache_init(vedata);
}

/* TODO(fclem) DRW_cache_object_surface_material_get needs a refactor to allow passing NULL
 * instead of gpumat_array. Avoiding all this boilerplate code. */
static struct GPUBatch **workbench_object_surface_material_get(Object *ob)
{
  const int materials_len = DRW_cache_object_material_count_get(ob);
  struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
  memset(gpumat_array, 0, sizeof(*gpumat_array) * materials_len);

  return DRW_cache_object_surface_material_get(ob, gpumat_array, materials_len);
}

static void workbench_cache_sculpt_populate(WORKBENCH_PrivateData *wpd,
                                            Object *ob,
                                            eV3DShadingColorType color_type)
{
  const bool use_single_drawcall = !ELEM(color_type, V3D_SHADING_MATERIAL_COLOR);
  BLI_assert(wpd->shading.color_type != V3D_SHADING_TEXTURE_COLOR);

  if (use_single_drawcall) {
    DRWShadingGroup *grp = workbench_material_setup(wpd, ob, 0, color_type, NULL);
    DRW_shgroup_call_sculpt(grp, ob, false, false);
  }
  else {
    const int materials_len = DRW_cache_object_material_count_get(ob);
    struct DRWShadingGroup **shgrps = BLI_array_alloca(shgrps, materials_len);
    for (int i = 0; i < materials_len; i++) {
      shgrps[i] = workbench_material_setup(wpd, ob, i + 1, color_type, NULL);
    }
    DRW_shgroup_call_sculpt_with_materials(shgrps, materials_len, ob);
  }
}

static void workbench_cache_texpaint_populate(WORKBENCH_PrivateData *wpd, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  const ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;
  const bool use_single_drawcall = imapaint->mode == IMAGEPAINT_MODE_IMAGE;

  if (use_single_drawcall) {
    struct GPUBatch *geom = DRW_cache_mesh_surface_texpaint_single_get(ob);
    if (geom) {
      Image *ima = imapaint->canvas;
      eGPUSamplerState state = GPU_SAMPLER_REPEAT;
      SET_FLAG_FROM_TEST(state, imapaint->interp == IMAGEPAINT_INTERP_LINEAR, GPU_SAMPLER_FILTER);

      DRWShadingGroup *grp = workbench_image_setup(wpd, ob, 0, ima, NULL, state);
      DRW_shgroup_call(grp, geom, ob);
    }
  }
  else {
    struct GPUBatch **geoms = DRW_cache_mesh_surface_texpaint_get(ob);
    if (geoms) {
      const int materials_len = DRW_cache_object_material_count_get(ob);
      for (int i = 0; i < materials_len; i++) {
        if (geoms[i] == NULL) {
          continue;
        }
        DRWShadingGroup *grp = workbench_image_setup(wpd, ob, i + 1, NULL, NULL, 0);
        DRW_shgroup_call(grp, geoms[i], ob);
      }
    }
  }
}

static void workbench_cache_common_populate(WORKBENCH_PrivateData *wpd,
                                            Object *ob,
                                            eV3DShadingColorType color_type,
                                            bool *r_transp)
{
  const bool use_tex = ELEM(color_type, V3D_SHADING_TEXTURE_COLOR);
  const bool use_vcol = ELEM(color_type, V3D_SHADING_VERTEX_COLOR);
  const bool use_single_drawcall = !ELEM(
      color_type, V3D_SHADING_MATERIAL_COLOR, V3D_SHADING_TEXTURE_COLOR);

  if (use_single_drawcall) {
    struct GPUBatch *geom;
    if (use_vcol) {
      if (ob->mode & OB_MODE_VERTEX_PAINT) {
        geom = DRW_cache_mesh_surface_vertpaint_get(ob);
      }
      else {
        geom = DRW_cache_mesh_surface_sculptcolors_get(ob);
      }
    }
    else {
      geom = DRW_cache_object_surface_get(ob);
    }

    if (geom) {
      DRWShadingGroup *grp = workbench_material_setup(wpd, ob, 0, color_type, r_transp);
      DRW_shgroup_call(grp, geom, ob);
    }
  }
  else {
    struct GPUBatch **geoms = (use_tex) ? DRW_cache_mesh_surface_texpaint_get(ob) :
                                          workbench_object_surface_material_get(ob);
    if (geoms) {
      const int materials_len = DRW_cache_object_material_count_get(ob);
      for (int i = 0; i < materials_len; i++) {
        if (geoms[i] == NULL) {
          continue;
        }
        DRWShadingGroup *grp = workbench_material_setup(wpd, ob, i + 1, color_type, r_transp);
        DRW_shgroup_call(grp, geoms[i], ob);
      }
    }
  }
}

static void workbench_cache_hair_populate(WORKBENCH_PrivateData *wpd,
                                          Object *ob,
                                          ParticleSystem *psys,
                                          ModifierData *md,
                                          eV3DShadingColorType color_type,
                                          bool use_texpaint_mode,
                                          const int matnr)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;

  const ImagePaintSettings *imapaint = use_texpaint_mode ? &scene->toolsettings->imapaint : NULL;
  Image *ima = (imapaint && imapaint->mode == IMAGEPAINT_MODE_IMAGE) ? imapaint->canvas : NULL;
  eGPUSamplerState state = 0;
  state |= (imapaint && imapaint->interp == IMAGEPAINT_INTERP_LINEAR) ? GPU_SAMPLER_FILTER : 0;
  DRWShadingGroup *grp = (use_texpaint_mode) ?
                             workbench_image_hair_setup(wpd, ob, matnr, ima, NULL, state) :
                             workbench_material_hair_setup(wpd, ob, matnr, color_type);

  DRW_shgroup_hair_create_sub(ob, psys, md, grp);
}

/**
 * Decide what color-type to draw the object with.
 * In some cases it can be overwritten by #workbench_material_setup().
 */
static eV3DShadingColorType workbench_color_type_get(WORKBENCH_PrivateData *wpd,
                                                     Object *ob,
                                                     bool *r_sculpt_pbvh,
                                                     bool *r_texpaint_mode,
                                                     bool *r_draw_shadow)
{
  eV3DShadingColorType color_type = wpd->shading.color_type;
  const Mesh *me = (ob->type == OB_MESH) ? ob->data : NULL;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_active = (ob == draw_ctx->obact);
  const bool is_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->v3d) &&
                              !DRW_state_is_image_render();
  const bool is_render = DRW_state_is_image_render() && (draw_ctx->v3d == NULL);
  const bool is_texpaint_mode = is_active && (wpd->ctx_mode == CTX_MODE_PAINT_TEXTURE);
  const bool is_vertpaint_mode = is_active && (wpd->ctx_mode == CTX_MODE_PAINT_VERTEX);

  if ((color_type == V3D_SHADING_TEXTURE_COLOR) && (ob->dt < OB_TEXTURE)) {
    color_type = V3D_SHADING_MATERIAL_COLOR;
  }
  /* Disable color mode if data layer is unavailable. */
  if ((color_type == V3D_SHADING_TEXTURE_COLOR) && (me == NULL || me->mloopuv == NULL)) {
    color_type = V3D_SHADING_MATERIAL_COLOR;
  }
  if (color_type == V3D_SHADING_VERTEX_COLOR) {
    if (me == NULL) {
      color_type = V3D_SHADING_OBJECT_COLOR;
    }
    if (!CustomData_has_layer(&me->vdata, CD_PROP_COLOR)) {
      color_type = V3D_SHADING_OBJECT_COLOR;
    }
  }

  if (r_sculpt_pbvh) {
    *r_sculpt_pbvh = is_sculpt_pbvh;
  }
  if (r_texpaint_mode) {
    *r_texpaint_mode = false;
  }

  if (!is_sculpt_pbvh && !is_render) {
    /* Force texture or vertex mode if object is in paint mode. */
    if (is_texpaint_mode && me && me->mloopuv) {
      color_type = V3D_SHADING_TEXTURE_COLOR;
      if (r_texpaint_mode) {
        *r_texpaint_mode = true;
      }
    }
    else if (is_vertpaint_mode && me && me->mloopcol) {
      color_type = V3D_SHADING_VERTEX_COLOR;
    }
  }

  if (r_draw_shadow) {
    *r_draw_shadow = (ob->dtx & OB_DRAW_NO_SHADOW_CAST) == 0 && SHADOW_ENABLED(wpd);
    /* Currently unsupported in sculpt mode. We could revert to the slow
     * method in this case but I'm not sure if it's a good idea given that
     * sculpted meshes are heavy to begin with. */
    if (is_sculpt_pbvh) {
      *r_draw_shadow = false;
    }

    if (is_active && DRW_object_use_hide_faces(ob)) {
      *r_draw_shadow = false;
    }
  }

  return color_type;
}

void workbench_cache_populate(void *ved, Object *ob)
{
  WORKBENCH_Data *vedata = ved;
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PrivateData *wpd = stl->wpd;

  if (!DRW_object_is_renderable(ob)) {
    return;
  }

  if (ob->type == OB_MESH && ob->modifiers.first != NULL) {
    bool use_texpaint_mode;
    int color_type = workbench_color_type_get(wpd, ob, NULL, &use_texpaint_mode, NULL);

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
        workbench_cache_hair_populate(
            wpd, ob, psys, md, color_type, use_texpaint_mode, part->omat);
      }
    }
  }

  if (!(ob->base_flag & BASE_FROM_DUPLI)) {
    ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Fluid);
    if (md && BKE_modifier_is_enabled(wpd->scene, md, eModifierMode_Realtime)) {
      FluidModifierData *fmd = (FluidModifierData *)md;
      if (fmd->domain && fmd->domain->type == FLUID_DOMAIN_TYPE_GAS) {
        workbench_volume_cache_populate(vedata, wpd->scene, ob, md, V3D_SHADING_SINGLE_COLOR);
        return; /* Do not draw solid in this case. */
      }
    }
  }

  if (!(DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF)) {
    return;
  }

  if ((ob->dt < OB_SOLID) && !DRW_state_is_scene_render()) {
    return;
  }

  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_POINTCLOUD)) {
    bool use_sculpt_pbvh, use_texpaint_mode, draw_shadow, has_transp_mat = false;
    eV3DShadingColorType color_type = workbench_color_type_get(
        wpd, ob, &use_sculpt_pbvh, &use_texpaint_mode, &draw_shadow);

    if (use_sculpt_pbvh) {
      workbench_cache_sculpt_populate(wpd, ob, color_type);
    }
    else if (use_texpaint_mode) {
      workbench_cache_texpaint_populate(wpd, ob);
    }
    else {
      workbench_cache_common_populate(wpd, ob, color_type, &has_transp_mat);
    }

    if (draw_shadow) {
      workbench_shadow_cache_populate(vedata, ob, has_transp_mat);
    }
  }
  else if (ob->type == OB_HAIR) {
    int color_type = workbench_color_type_get(wpd, ob, NULL, NULL, NULL);
    workbench_cache_hair_populate(wpd, ob, NULL, NULL, color_type, false, HAIR_MATERIAL_NR);
  }
  else if (ob->type == OB_VOLUME) {
    if (wpd->shading.type != OB_WIRE) {
      int color_type = workbench_color_type_get(wpd, ob, NULL, NULL, NULL);
      workbench_volume_cache_populate(vedata, wpd->scene, ob, NULL, color_type);
    }
  }
}

void workbench_cache_finish(void *ved)
{
  WORKBENCH_Data *vedata = ved;
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_PrivateData *wpd = stl->wpd;

  /* TODO(fclem) Only do this when really needed. */
  {
    /* HACK we allocate the infront depth here to avoid the overhead when if is not needed. */
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

    DRW_texture_ensure_fullscreen_2d(&dtxl->depth_in_front, GPU_DEPTH24_STENCIL8, 0);

    GPU_framebuffer_ensure_config(&dfbl->in_front_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                      GPU_ATTACHMENT_TEXTURE(dtxl->color),
                                  });

    GPU_framebuffer_ensure_config(&fbl->opaque_infront_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                      GPU_ATTACHMENT_TEXTURE(wpd->material_buffer_tx),
                                      GPU_ATTACHMENT_TEXTURE(wpd->normal_buffer_tx),
                                      GPU_ATTACHMENT_TEXTURE(wpd->object_id_tx),
                                  });

    GPU_framebuffer_ensure_config(&fbl->transp_accum_infront_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                      GPU_ATTACHMENT_TEXTURE(wpd->accum_buffer_tx),
                                      GPU_ATTACHMENT_TEXTURE(wpd->reveal_buffer_tx),
                                  });
  }

  if (wpd->object_id_tx) {
    GPU_framebuffer_ensure_config(&fbl->id_clear_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(wpd->object_id_tx),
                                  });
  }
  else {
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->id_clear_fb);
  }

  workbench_update_material_ubos(wpd);

  /* TODO don't free reuse next redraw. */
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      for (int k = 0; k < 2; k++) {
        if (wpd->prepass[i][j][k].material_hash) {
          BLI_ghash_free(wpd->prepass[i][j][k].material_hash, NULL, NULL);
          wpd->prepass[i][j][k].material_hash = NULL;
        }
      }
    }
  }
}

/* Used by viewport rendering & final rendering.
 * Do one render loop iteration (i.e: One TAA sample). */
void workbench_draw_sample(void *ved)
{
  WORKBENCH_Data *vedata = ved;
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  WORKBENCH_PassList *psl = vedata->psl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float clear_col_with_alpha[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  const bool do_render = workbench_antialiasing_setup(vedata);
  const bool xray_is_visible = wpd->shading.xray_alpha > 0.0f;
  const bool do_transparent_infront_pass = !DRW_pass_is_empty(psl->transp_accum_infront_ps);
  const bool do_transparent_pass = !DRW_pass_is_empty(psl->transp_accum_ps);
  const bool do_opaque_infront_pass = !DRW_pass_is_empty(psl->opaque_infront_ps);
  const bool do_opaque_pass = !DRW_pass_is_empty(psl->opaque_ps) || do_opaque_infront_pass;

  if (dfbl->in_front_fb) {
    GPU_framebuffer_bind(dfbl->in_front_fb);
    GPU_framebuffer_clear_depth(dfbl->in_front_fb, 1.0f);
  }

  if (do_render) {
    GPU_framebuffer_bind(dfbl->default_fb);
    GPU_framebuffer_clear_color_depth_stencil(dfbl->default_fb, wpd->background_color, 1.0f, 0x00);

    if (fbl->id_clear_fb) {
      GPU_framebuffer_bind(fbl->id_clear_fb);
      GPU_framebuffer_clear_color(fbl->id_clear_fb, clear_col);
    }

    if (do_opaque_pass) {
      GPU_framebuffer_bind(fbl->opaque_fb);
      DRW_draw_pass(psl->opaque_ps);

      if (psl->shadow_ps[0]) {
        DRW_draw_pass(psl->shadow_ps[0]);
        DRW_draw_pass(psl->shadow_ps[1]);
      }

      if (do_opaque_infront_pass) {
        GPU_framebuffer_bind(fbl->opaque_infront_fb);
        DRW_draw_pass(psl->opaque_infront_ps);

        GPU_framebuffer_bind(fbl->opaque_fb);
        DRW_draw_pass(psl->merge_infront_ps);
      }

      GPU_framebuffer_bind(dfbl->default_fb);
      DRW_draw_pass(psl->composite_ps);

      if (psl->cavity_ps) {
        GPU_framebuffer_bind(dfbl->color_only_fb);
        DRW_draw_pass(psl->cavity_ps);
      }
    }

    workbench_volume_draw_pass(vedata);

    if (xray_is_visible) {
      if (do_transparent_pass) {
        GPU_framebuffer_bind(fbl->transp_accum_fb);
        GPU_framebuffer_clear_color(fbl->transp_accum_fb, clear_col_with_alpha);

        DRW_draw_pass(psl->transp_accum_ps);

        GPU_framebuffer_bind(dfbl->color_only_fb);
        DRW_draw_pass(psl->transp_resolve_ps);
      }

      if (do_transparent_infront_pass) {
        GPU_framebuffer_bind(fbl->transp_accum_infront_fb);
        GPU_framebuffer_clear_color(fbl->transp_accum_infront_fb, clear_col_with_alpha);

        DRW_draw_pass(psl->transp_accum_infront_ps);

        GPU_framebuffer_bind(dfbl->color_only_fb);
        DRW_draw_pass(psl->transp_resolve_ps);
      }
    }

    workbench_transparent_draw_depth_pass(vedata);

    if (psl->outline_ps) {
      GPU_framebuffer_bind(dfbl->color_only_fb);
      DRW_draw_pass(psl->outline_ps);
    }

    workbench_dof_draw_pass(vedata);
  }

  workbench_antialiasing_draw_pass(vedata);
}

/* Viewport rendering. */
static void workbench_draw_scene(void *ved)
{
  WORKBENCH_Data *vedata = ved;
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;

  if (DRW_state_is_opengl_render()) {
    while (wpd->taa_sample < max_ii(1, wpd->taa_sample_len)) {
      workbench_update_world_ubo(wpd);

      workbench_draw_sample(vedata);
    }
  }
  else {
    workbench_draw_sample(vedata);
  }

  workbench_draw_finish(vedata);
}

void workbench_draw_finish(void *ved)
{
  WORKBENCH_Data *vedata = ved;
  workbench_volume_draw_finish(vedata);
  /* Reset default view. */
  DRW_view_set_active(NULL);
}

static void workbench_engine_free(void)
{
  workbench_shader_free();
}

static void workbench_view_update(void *vedata)
{
  WORKBENCH_Data *data = vedata;
  workbench_antialiasing_view_updated(data);
}

static void workbench_id_update(void *UNUSED(vedata), struct ID *id)
{
  if (GS(id->name) == ID_OB) {
    WORKBENCH_ObjectData *oed = (WORKBENCH_ObjectData *)DRW_drawdata_get(id,
                                                                         &draw_engine_workbench);
    if (oed != NULL && oed->dd.recalc != 0) {
      oed->shadow_bbox_dirty = (oed->dd.recalc & ID_RECALC_ALL) != 0;
      oed->dd.recalc = 0;
    }
  }
}

static const DrawEngineDataSize workbench_data_size = DRW_VIEWPORT_DATA_SIZE(WORKBENCH_Data);

DrawEngineType draw_engine_workbench = {
    NULL,
    NULL,
    N_("Workbench"),
    &workbench_data_size,
    &workbench_engine_init,
    &workbench_engine_free,
    &workbench_cache_init,
    &workbench_cache_populate,
    &workbench_cache_finish,
    &workbench_draw_scene,
    &workbench_view_update,
    &workbench_id_update,
    &workbench_render,
};

RenderEngineType DRW_engine_viewport_workbench_type = {
    NULL,
    NULL,
    WORKBENCH_ENGINE,
    N_("Workbench"),
    RE_INTERNAL | RE_USE_STEREO_VIEWPORT,
    NULL,
    &DRW_render_to_image,
    NULL,
    NULL,
    NULL,
    NULL,
    &workbench_render_update_passes,
    &draw_engine_workbench,
    {NULL, NULL, NULL},
};

#undef WORKBENCH_ENGINE
