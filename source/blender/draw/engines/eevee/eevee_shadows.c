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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup EEVEE
 */

#include "BLI_string_utils.h"
#include "BLI_sys_types.h" /* bool */

// #include "BLI_dynstr.h"
// #include "BLI_rand.h"

#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"

#define SH_CASTER_ALLOC_CHUNK 32

static struct {
  struct GPUShader *shadow_sh;
  struct GPUShader *shadow_accum_sh;
} e_data = {NULL}; /* Engine data */

extern char datatoc_shadow_vert_glsl[];
extern char datatoc_shadow_frag_glsl[];
extern char datatoc_shadow_accum_frag_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_lights_lib_glsl[];
extern char datatoc_raytrace_lib_glsl[];

void eevee_contact_shadow_setup(const Light *la, EEVEE_Shadow *evsh)
{
  evsh->contact_dist = (la->mode & LA_SHAD_CONTACT) ? la->contact_dist : 0.0f;
  evsh->contact_bias = 0.05f * la->contact_bias;
  evsh->contact_thickness = la->contact_thickness;
}

void EEVEE_shadows_init(EEVEE_ViewLayerData *sldata)
{
  const uint shadow_ubo_size = sizeof(EEVEE_Shadow) * MAX_SHADOW +
                               sizeof(EEVEE_ShadowCube) * MAX_SHADOW_CUBE +
                               sizeof(EEVEE_ShadowCascade) * MAX_SHADOW_CASCADE;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (!e_data.shadow_sh) {
    e_data.shadow_sh = DRW_shader_create_with_lib(datatoc_shadow_vert_glsl,
                                                  NULL,
                                                  datatoc_shadow_frag_glsl,
                                                  datatoc_common_view_lib_glsl,
                                                  NULL);
  }

  if (!e_data.shadow_accum_sh) {
    char *frag_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                      datatoc_common_uniforms_lib_glsl,
                                      datatoc_bsdf_common_lib_glsl,
                                      datatoc_raytrace_lib_glsl,
                                      datatoc_lights_lib_glsl,
                                      datatoc_shadow_accum_frag_glsl);

    e_data.shadow_accum_sh = DRW_shader_create_fullscreen(frag_str, SHADER_DEFINES);
    MEM_freeN(frag_str);
  }

  if (!sldata->lights) {
    sldata->lights = MEM_callocN(sizeof(EEVEE_LightsInfo), "EEVEE_LightsInfo");
    sldata->light_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_Light) * MAX_LIGHT, NULL);
    sldata->shadow_ubo = DRW_uniformbuffer_create(shadow_ubo_size, NULL);

    for (int i = 0; i < 2; i++) {
      sldata->shcasters_buffers[i].bbox = MEM_callocN(
          sizeof(EEVEE_BoundBox) * SH_CASTER_ALLOC_CHUNK, __func__);
      sldata->shcasters_buffers[i].update = BLI_BITMAP_NEW(SH_CASTER_ALLOC_CHUNK, __func__);
      sldata->shcasters_buffers[i].alloc_count = SH_CASTER_ALLOC_CHUNK;
      sldata->shcasters_buffers[i].count = 0;
    }
    sldata->lights->shcaster_frontbuffer = &sldata->shcasters_buffers[0];
    sldata->lights->shcaster_backbuffer = &sldata->shcasters_buffers[1];
  }

  /* Flip buffers */
  SWAP(EEVEE_ShadowCasterBuffer *,
       sldata->lights->shcaster_frontbuffer,
       sldata->lights->shcaster_backbuffer);

  int sh_cube_size = scene_eval->eevee.shadow_cube_size;
  int sh_cascade_size = scene_eval->eevee.shadow_cascade_size;
  const bool sh_high_bitdepth = (scene_eval->eevee.flag & SCE_EEVEE_SHADOW_HIGH_BITDEPTH) != 0;
  sldata->lights->soft_shadows = (scene_eval->eevee.flag & SCE_EEVEE_SHADOW_SOFT) != 0;

  EEVEE_LightsInfo *linfo = sldata->lights;
  if ((linfo->shadow_cube_size != sh_cube_size) ||
      (linfo->shadow_high_bitdepth != sh_high_bitdepth)) {
    BLI_assert((sh_cube_size > 0) && (sh_cube_size <= 4096));
    DRW_TEXTURE_FREE_SAFE(sldata->shadow_cube_pool);
    CLAMP(sh_cube_size, 1, 4096);
  }

  if ((linfo->shadow_cascade_size != sh_cascade_size) ||
      (linfo->shadow_high_bitdepth != sh_high_bitdepth)) {
    BLI_assert((sh_cascade_size > 0) && (sh_cascade_size <= 4096));
    DRW_TEXTURE_FREE_SAFE(sldata->shadow_cascade_pool);
    CLAMP(sh_cascade_size, 1, 4096);
  }

  linfo->shadow_high_bitdepth = sh_high_bitdepth;
  linfo->shadow_cube_size = sh_cube_size;
  linfo->shadow_cascade_size = sh_cascade_size;
}

void EEVEE_shadows_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_LightsInfo *linfo = sldata->lights;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;

  EEVEE_ShadowCasterBuffer *backbuffer = linfo->shcaster_backbuffer;
  EEVEE_ShadowCasterBuffer *frontbuffer = linfo->shcaster_frontbuffer;

  frontbuffer->count = 0;
  linfo->num_cube_layer = 0;
  linfo->num_cascade_layer = 0;
  linfo->cube_len = linfo->cascade_len = linfo->shadow_len = 0;

  /* Shadow Casters: Reset flags. */
  BLI_bitmap_set_all(backbuffer->update, true, backbuffer->alloc_count);
  /* Is this one needed? */
  BLI_bitmap_set_all(frontbuffer->update, false, frontbuffer->alloc_count);

  INIT_MINMAX(linfo->shcaster_aabb.min, linfo->shcaster_aabb.max);

  {
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_SHADOW_OFFSET;
    DRW_PASS_CREATE(psl->shadow_pass, state);

    stl->g_data->shadow_shgrp = DRW_shgroup_create(e_data.shadow_sh, psl->shadow_pass);
  }
}

/* Make that object update shadow casting lights inside its influence bounding box. */
void EEVEE_shadows_caster_register(EEVEE_ViewLayerData *sldata, Object *ob)
{
  EEVEE_LightsInfo *linfo = sldata->lights;
  EEVEE_ShadowCasterBuffer *backbuffer = linfo->shcaster_backbuffer;
  EEVEE_ShadowCasterBuffer *frontbuffer = linfo->shcaster_frontbuffer;
  bool update = true;
  int id = frontbuffer->count;

  /* Make sure shadow_casters is big enough. */
  if (id + 1 >= frontbuffer->alloc_count) {
    frontbuffer->alloc_count += SH_CASTER_ALLOC_CHUNK;
    frontbuffer->bbox = MEM_reallocN(frontbuffer->bbox,
                                     sizeof(EEVEE_BoundBox) * frontbuffer->alloc_count);
    BLI_BITMAP_RESIZE(frontbuffer->update, frontbuffer->alloc_count);
  }

  if (ob->base_flag & BASE_FROM_DUPLI) {
    /* Duplis will always refresh the shadowmaps as if they were deleted each frame. */
    /* TODO(fclem) fix this. */
    update = true;
  }
  else {
    EEVEE_ObjectEngineData *oedata = EEVEE_object_data_ensure(ob);
    int past_id = oedata->shadow_caster_id;
    oedata->shadow_caster_id = id;
    /* Update flags in backbuffer. */
    if (past_id > -1 && past_id < backbuffer->count) {
      BLI_BITMAP_SET(backbuffer->update, past_id, oedata->need_update);
    }
    update = oedata->need_update;
    oedata->need_update = false;
  }

  if (update) {
    BLI_BITMAP_ENABLE(frontbuffer->update, id);
  }

  /* Update World AABB in frontbuffer. */
  BoundBox *bb = BKE_object_boundbox_get(ob);
  float min[3], max[3];
  INIT_MINMAX(min, max);
  for (int i = 0; i < 8; i++) {
    float vec[3];
    copy_v3_v3(vec, bb->vec[i]);
    mul_m4_v3(ob->obmat, vec);
    minmax_v3v3_v3(min, max, vec);
  }

  EEVEE_BoundBox *aabb = &frontbuffer->bbox[id];
  add_v3_v3v3(aabb->center, min, max);
  mul_v3_fl(aabb->center, 0.5f);
  sub_v3_v3v3(aabb->halfdim, aabb->center, max);

  aabb->halfdim[0] = fabsf(aabb->halfdim[0]);
  aabb->halfdim[1] = fabsf(aabb->halfdim[1]);
  aabb->halfdim[2] = fabsf(aabb->halfdim[2]);

  minmax_v3v3_v3(linfo->shcaster_aabb.min, linfo->shcaster_aabb.max, min);
  minmax_v3v3_v3(linfo->shcaster_aabb.min, linfo->shcaster_aabb.max, max);

  frontbuffer->count++;
}

/* Used for checking if object is inside the shadow volume. */
static bool sphere_bbox_intersect(const BoundSphere *bs, const EEVEE_BoundBox *bb)
{
  /* We are testing using a rougher AABB vs AABB test instead of full AABB vs Sphere. */
  /* TODO test speed with AABB vs Sphere. */
  bool x = fabsf(bb->center[0] - bs->center[0]) <= (bb->halfdim[0] + bs->radius);
  bool y = fabsf(bb->center[1] - bs->center[1]) <= (bb->halfdim[1] + bs->radius);
  bool z = fabsf(bb->center[2] - bs->center[2]) <= (bb->halfdim[2] + bs->radius);

  return x && y && z;
}

void EEVEE_shadows_update(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_LightsInfo *linfo = sldata->lights;
  EEVEE_ShadowCasterBuffer *backbuffer = linfo->shcaster_backbuffer;
  EEVEE_ShadowCasterBuffer *frontbuffer = linfo->shcaster_frontbuffer;

  eGPUTextureFormat shadow_pool_format = (linfo->shadow_high_bitdepth) ? GPU_DEPTH_COMPONENT24 :
                                                                         GPU_DEPTH_COMPONENT16;
  /* Setup enough layers. */
  /* Free textures if number mismatch. */
  if (linfo->num_cube_layer != linfo->cache_num_cube_layer) {
    DRW_TEXTURE_FREE_SAFE(sldata->shadow_cube_pool);
    linfo->cache_num_cube_layer = linfo->num_cube_layer;
    /* Update all lights. */
    BLI_bitmap_set_all(&linfo->sh_cube_update[0], true, MAX_LIGHT);
  }

  if (linfo->num_cascade_layer != linfo->cache_num_cascade_layer) {
    DRW_TEXTURE_FREE_SAFE(sldata->shadow_cascade_pool);
    linfo->cache_num_cascade_layer = linfo->num_cascade_layer;
  }

  if (!sldata->shadow_cube_pool) {
    /* TODO shadowcube array. */
    int cube_size = linfo->shadow_cube_size + ((true) ? 2 : 0);
    sldata->shadow_cube_pool = DRW_texture_create_2d_array(cube_size,
                                                           cube_size,
                                                           max_ii(1, linfo->num_cube_layer * 6),
                                                           shadow_pool_format,
                                                           DRW_TEX_FILTER | DRW_TEX_COMPARE,
                                                           NULL);
  }

  if (!sldata->shadow_cascade_pool) {
    sldata->shadow_cascade_pool = DRW_texture_create_2d_array(linfo->shadow_cascade_size,
                                                              linfo->shadow_cascade_size,
                                                              max_ii(1, linfo->num_cascade_layer),
                                                              shadow_pool_format,
                                                              DRW_TEX_FILTER | DRW_TEX_COMPARE,
                                                              NULL);
  }

  if (sldata->shadow_fb == NULL) {
    sldata->shadow_fb = GPU_framebuffer_create();
  }

  /* Gather all light own update bits. to avoid costly intersection check.  */
  for (int j = 0; j < linfo->cube_len; j++) {
    const EEVEE_Light *evli = linfo->light_data + linfo->shadow_cube_light_indices[j];
    /* Setup shadow cube in UBO and tag for update if necessary. */
    if (EEVEE_shadows_cube_setup(linfo, evli, effects->taa_current_sample - 1)) {
      BLI_BITMAP_ENABLE(&linfo->sh_cube_update[0], j);
    }
  }

  /* TODO(fclem) This part can be slow, optimize it. */
  EEVEE_BoundBox *bbox = backbuffer->bbox;
  BoundSphere *bsphere = linfo->shadow_bounds;
  /* Search for deleted shadow casters or if shcaster WAS in shadow radius. */
  for (int i = 0; i < backbuffer->count; i++) {
    /* If the shadowcaster has been deleted or updated. */
    if (BLI_BITMAP_TEST(backbuffer->update, i)) {
      for (int j = 0; j < linfo->cube_len; j++) {
        if (!BLI_BITMAP_TEST(&linfo->sh_cube_update[0], j)) {
          if (sphere_bbox_intersect(&bsphere[j], &bbox[i])) {
            BLI_BITMAP_ENABLE(&linfo->sh_cube_update[0], j);
          }
        }
      }
    }
  }
  /* Search for updates in current shadow casters. */
  bbox = frontbuffer->bbox;
  for (int i = 0; i < frontbuffer->count; i++) {
    /* If the shadowcaster has been updated. */
    if (BLI_BITMAP_TEST(frontbuffer->update, i)) {
      for (int j = 0; j < linfo->cube_len; j++) {
        if (!BLI_BITMAP_TEST(&linfo->sh_cube_update[0], j)) {
          if (sphere_bbox_intersect(&bsphere[j], &bbox[i])) {
            BLI_BITMAP_ENABLE(&linfo->sh_cube_update[0], j);
          }
        }
      }
    }
  }

  /* Resize shcasters buffers if too big. */
  if (frontbuffer->alloc_count - frontbuffer->count > SH_CASTER_ALLOC_CHUNK) {
    frontbuffer->alloc_count = (frontbuffer->count / SH_CASTER_ALLOC_CHUNK) *
                               SH_CASTER_ALLOC_CHUNK;
    frontbuffer->alloc_count += (frontbuffer->count % SH_CASTER_ALLOC_CHUNK != 0) ?
                                    SH_CASTER_ALLOC_CHUNK :
                                    0;
    frontbuffer->bbox = MEM_reallocN(frontbuffer->bbox,
                                     sizeof(EEVEE_BoundBox) * frontbuffer->alloc_count);
    BLI_BITMAP_RESIZE(frontbuffer->update, frontbuffer->alloc_count);
  }
}

/* this refresh lights shadow buffers */
void EEVEE_shadows_draw(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, DRWView *view)
{
  EEVEE_LightsInfo *linfo = sldata->lights;

  int saved_ray_type = sldata->common_data.ray_type;

  /* Precompute all shadow/view test before rendering and trashing the culling cache. */
  BLI_bitmap *cube_visible = BLI_BITMAP_NEW_ALLOCA(MAX_SHADOW_CUBE);
  bool any_visible = linfo->cascade_len > 0;
  for (int cube = 0; cube < linfo->cube_len; cube++) {
    if (DRW_culling_sphere_test(view, linfo->shadow_bounds + cube)) {
      BLI_BITMAP_ENABLE(cube_visible, cube);
      any_visible = true;
    }
  }

  if (any_visible) {
    sldata->common_data.ray_type = EEVEE_RAY_SHADOW;
    DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);
  }

  DRW_stats_group_start("Cube Shadow Maps");
  {
    for (int cube = 0; cube < linfo->cube_len; cube++) {
      if (BLI_BITMAP_TEST(cube_visible, cube) && BLI_BITMAP_TEST(linfo->sh_cube_update, cube)) {
        EEVEE_shadows_draw_cubemap(sldata, vedata, cube);
      }
    }
  }
  DRW_stats_group_end();

  DRW_stats_group_start("Cascaded Shadow Maps");
  {
    for (int cascade = 0; cascade < linfo->cascade_len; cascade++) {
      EEVEE_shadows_draw_cascades(sldata, vedata, view, cascade);
    }
  }
  DRW_stats_group_end();

  DRW_view_set_active(view);

  DRW_uniformbuffer_update(sldata->shadow_ubo, &linfo->shadow_data); /* Update all data at once */

  if (any_visible) {
    sldata->common_data.ray_type = saved_ray_type;
    DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);
  }
}

/* -------------------------------------------------------------------- */

/** \name Render Passes
 * \{ */

void EEVEE_shadow_output_init(EEVEE_ViewLayerData *sldata,
                              EEVEE_Data *vedata,
                              uint UNUSED(tot_samples))
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  /* Create FrameBuffer. */
  const eGPUTextureFormat texture_format = GPU_R32F;
  DRW_texture_ensure_fullscreen_2d(&txl->shadow_accum, texture_format, 0);

  GPU_framebuffer_ensure_config(&fbl->shadow_accum_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->shadow_accum)});

  /* Clear texture. */
  if (DRW_state_is_image_render() || effects->taa_current_sample == 1) {
    GPU_framebuffer_bind(fbl->shadow_accum_fb);
    GPU_framebuffer_clear_color(fbl->shadow_accum_fb, clear);
  }

  /* Create Pass and shgroup. */
  DRW_PASS_CREATE(psl->shadow_accum_pass,
                  DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND_ADD_FULL);
  DRWShadingGroup *grp = DRW_shgroup_create(e_data.shadow_accum_sh, psl->shadow_accum_pass);
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
  DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
  DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
  DRW_shgroup_uniform_texture_ref(grp, "shadowCubeTexture", &sldata->shadow_cube_pool);
  DRW_shgroup_uniform_texture_ref(grp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);

  DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
}

void EEVEE_shadow_output_accumulate(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;

  if (fbl->shadow_accum_fb != NULL) {
    GPU_framebuffer_bind(fbl->shadow_accum_fb);
    DRW_draw_pass(psl->shadow_accum_pass);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

/* \} */

void EEVEE_shadows_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.shadow_sh);
  DRW_SHADER_FREE_SAFE(e_data.shadow_accum_sh);
}
