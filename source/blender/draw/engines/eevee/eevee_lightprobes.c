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
 */

#include "DRW_render.h"

#include "BLI_utildefines.h"
#include "BLI_rand.h"

#include "DNA_world_types.h"
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_view3d_types.h"

#include "BKE_collection.h"
#include "BKE_object.h"
#include "MEM_guardedalloc.h"

#include "GPU_material.h"
#include "GPU_texture.h"

#include "DEG_depsgraph_query.h"

#include "eevee_lightcache.h"
#include "eevee_private.h"

#include "WM_api.h"
#include "WM_types.h"

static struct {
  struct GPUTexture *hammersley;
  struct GPUTexture *planar_pool_placeholder;
  struct GPUTexture *depth_placeholder;
  struct GPUTexture *depth_array_placeholder;
  struct GPUTexture *cube_face_minmaxz;

  struct GPUVertFormat *format_probe_display_cube;
  struct GPUVertFormat *format_probe_display_planar;
} e_data = {NULL}; /* Engine data */

/* *********** FUNCTIONS *********** */

/* TODO find a better way than this. This does not support dupli objects if
 * the original object is hidden. */
bool EEVEE_lightprobes_obj_visibility_cb(bool vis_in, void *user_data)
{
  EEVEE_ObjectEngineData *oed = (EEVEE_ObjectEngineData *)user_data;

  /* test disabled if group is NULL */
  if (oed->test_data->collection == NULL) {
    return vis_in;
  }

  if (oed->test_data->cached == false) {
    oed->ob_vis_dirty = true;
  }

  /* early out, don't need to compute ob_vis yet. */
  if (vis_in == false) {
    return vis_in;
  }

  if (oed->ob_vis_dirty) {
    oed->ob_vis_dirty = false;
    oed->ob_vis = BKE_collection_has_object_recursive(oed->test_data->collection, oed->ob);
    oed->ob_vis = (oed->test_data->invert) ? !oed->ob_vis : oed->ob_vis;
  }

  return vis_in && oed->ob_vis;
}

static struct GPUTexture *create_hammersley_sample_texture(int samples)
{
  struct GPUTexture *tex;
  float(*texels)[2] = MEM_mallocN(sizeof(float[2]) * samples, "hammersley_tex");
  int i;

  for (i = 0; i < samples; i++) {
    double dphi;
    BLI_hammersley_1d(i, &dphi);
    float phi = (float)dphi * 2.0f * M_PI;
    texels[i][0] = cosf(phi);
    texels[i][1] = sinf(phi);
  }

  tex = DRW_texture_create_1d(samples, GPU_RG16F, DRW_TEX_WRAP, (float *)texels);
  MEM_freeN(texels);
  return tex;
}

static void planar_pool_ensure_alloc(EEVEE_Data *vedata, int num_planar_ref)
{
  EEVEE_TextureList *txl = vedata->txl;

  /* XXX TODO OPTIMISATION : This is a complete waist of texture memory.
   * Instead of allocating each planar probe for each viewport,
   * only alloc them once using the biggest viewport resolution. */
  const float *viewport_size = DRW_viewport_size_get();

  /* TODO get screen percentage from layer setting */
  // const DRWContextState *draw_ctx = DRW_context_state_get();
  // ViewLayer *view_layer = draw_ctx->view_layer;
  float screen_percentage = 1.0f;

  int width = max_ii(1, (int)(viewport_size[0] * screen_percentage));
  int height = max_ii(1, (int)(viewport_size[1] * screen_percentage));

  /* Fix case were the pool was allocated width the dummy size (1,1,1). */
  if (txl->planar_pool && (num_planar_ref > 0) &&
      (GPU_texture_width(txl->planar_pool) != width ||
       GPU_texture_height(txl->planar_pool) != height)) {
    DRW_TEXTURE_FREE_SAFE(txl->planar_pool);
    DRW_TEXTURE_FREE_SAFE(txl->planar_depth);
  }

  /* We need an Array texture so allocate it ourself */
  if (!txl->planar_pool) {
    if (num_planar_ref > 0) {
      txl->planar_pool = DRW_texture_create_2d_array(width,
                                                     height,
                                                     max_ii(1, num_planar_ref),
                                                     GPU_R11F_G11F_B10F,
                                                     DRW_TEX_FILTER | DRW_TEX_MIPMAP,
                                                     NULL);
      txl->planar_depth = DRW_texture_create_2d_array(
          width, height, max_ii(1, num_planar_ref), GPU_DEPTH_COMPONENT24, 0, NULL);
    }
    else if (num_planar_ref == 0) {
      /* Makes Opengl Happy : Create a placeholder texture that will never be sampled but still
       * bound to shader. */
      txl->planar_pool = DRW_texture_create_2d_array(
          1, 1, 1, GPU_RGBA8, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
      txl->planar_depth = DRW_texture_create_2d_array(1, 1, 1, GPU_DEPTH_COMPONENT24, 0, NULL);
    }
  }
}

void EEVEE_lightprobes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_StorageList *stl = vedata->stl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (!e_data.hammersley) {
    EEVEE_shaders_lightprobe_shaders_init();
    e_data.hammersley = create_hammersley_sample_texture(HAMMERSLEY_SIZE);
  }

  memset(stl->g_data->cube_views, 0, sizeof(stl->g_data->cube_views));
  memset(stl->g_data->planar_views, 0, sizeof(stl->g_data->planar_views));

  /* Use fallback if we don't have gpu texture allocated an we cannot restore them. */
  bool use_fallback_lightcache = (scene_eval->eevee.light_cache == NULL) ||
                                 ((scene_eval->eevee.light_cache->grid_tx.tex == NULL) &&
                                  (scene_eval->eevee.light_cache->grid_tx.data == NULL)) ||
                                 ((scene_eval->eevee.light_cache->cube_tx.tex == NULL) &&
                                  (scene_eval->eevee.light_cache->cube_tx.data == NULL));

  if (use_fallback_lightcache && (sldata->fallback_lightcache == NULL)) {
#if defined(IRRADIANCE_SH_L2)
    int grid_res = 4;
#elif defined(IRRADIANCE_CUBEMAP)
    int grid_res = 8;
#elif defined(IRRADIANCE_HL2)
    int grid_res = 4;
#endif
    int cube_res = OCTAHEDRAL_SIZE_FROM_CUBESIZE(scene_eval->eevee.gi_cubemap_resolution);
    int vis_res = scene_eval->eevee.gi_visibility_resolution;
    sldata->fallback_lightcache = EEVEE_lightcache_create(
        1, 1, cube_res, vis_res, (int[3]){grid_res, grid_res, 1});
  }

  stl->g_data->light_cache = (use_fallback_lightcache) ? sldata->fallback_lightcache :
                                                         scene_eval->eevee.light_cache;

  EEVEE_lightcache_load(stl->g_data->light_cache);

  if (!sldata->probes) {
    sldata->probes = MEM_callocN(sizeof(EEVEE_LightProbesInfo), "EEVEE_LightProbesInfo");
    sldata->probe_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_LightProbe) * MAX_PROBE, NULL);
    sldata->grid_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_LightGrid) * MAX_GRID, NULL);
    sldata->planar_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_PlanarReflection) * MAX_PLANAR,
                                                  NULL);
  }

  common_data->prb_num_planar = 0;
  common_data->prb_num_render_cube = 1;
  common_data->prb_num_render_grid = 1;

  common_data->spec_toggle = true;
  common_data->ssr_toggle = true;
  common_data->sss_toggle = true;

  /* Placeholder planar pool: used when rendering planar reflections (avoid dependency loop). */
  if (!e_data.planar_pool_placeholder) {
    e_data.planar_pool_placeholder = DRW_texture_create_2d_array(
        1, 1, 1, GPU_RGBA8, DRW_TEX_FILTER, NULL);
  }
}

/* Only init the passes useful for rendering the light cache. */
void EEVEE_lightbake_cache_init(EEVEE_ViewLayerData *sldata,
                                EEVEE_Data *vedata,
                                GPUTexture *rt_color,
                                GPUTexture *rt_depth)
{
  EEVEE_PassList *psl = vedata->psl;
  LightCache *light_cache = vedata->stl->g_data->light_cache;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;

  {
    DRW_PASS_CREATE(psl->probe_glossy_compute, DRW_STATE_WRITE_COLOR);

    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_probe_filter_glossy_sh_get(),
                                              psl->probe_glossy_compute);

    DRW_shgroup_uniform_float(grp, "intensityFac", &pinfo->intensity_fac, 1);
    DRW_shgroup_uniform_float(grp, "sampleCount", &pinfo->samples_len, 1);
    DRW_shgroup_uniform_float(grp, "invSampleCount", &pinfo->samples_len_inv, 1);
    DRW_shgroup_uniform_float(grp, "roughnessSquared", &pinfo->roughness, 1);
    DRW_shgroup_uniform_float(grp, "lodFactor", &pinfo->lodfactor, 1);
    DRW_shgroup_uniform_float(grp, "lodMax", &pinfo->lod_rt_max, 1);
    DRW_shgroup_uniform_float(grp, "texelSize", &pinfo->texel_size, 1);
    DRW_shgroup_uniform_float(grp, "paddingSize", &pinfo->padding_size, 1);
    DRW_shgroup_uniform_float(grp, "fireflyFactor", &pinfo->firefly_fac, 1);
    DRW_shgroup_uniform_int(grp, "Layer", &pinfo->layer, 1);
    DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
    // DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);
    DRW_shgroup_uniform_texture(grp, "probeHdr", rt_color);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRW_shgroup_call(grp, geom, NULL);
  }

  {
    DRW_PASS_CREATE(psl->probe_diffuse_compute, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_probe_filter_diffuse_sh_get(),
                                              psl->probe_diffuse_compute);
#ifdef IRRADIANCE_SH_L2
    DRW_shgroup_uniform_int(grp, "probeSize", &pinfo->shres, 1);
#else
    DRW_shgroup_uniform_float(grp, "sampleCount", &pinfo->samples_len, 1);
    DRW_shgroup_uniform_float(grp, "invSampleCount", &pinfo->samples_len_inv, 1);
    DRW_shgroup_uniform_float(grp, "lodFactor", &pinfo->lodfactor, 1);
    DRW_shgroup_uniform_float(grp, "lodMax", &pinfo->lod_rt_max, 1);
    DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
#endif
    DRW_shgroup_uniform_float(grp, "intensityFac", &pinfo->intensity_fac, 1);
    DRW_shgroup_uniform_texture(grp, "probeHdr", rt_color);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRW_shgroup_call(grp, geom, NULL);
  }

  {
    DRW_PASS_CREATE(psl->probe_visibility_compute, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_probe_filter_visibility_sh_get(),
                                              psl->probe_visibility_compute);
    DRW_shgroup_uniform_int(grp, "outputSize", &pinfo->shres, 1);
    DRW_shgroup_uniform_float(grp, "visibilityRange", &pinfo->visibility_range, 1);
    DRW_shgroup_uniform_float(grp, "visibilityBlur", &pinfo->visibility_blur, 1);
    DRW_shgroup_uniform_float(grp, "sampleCount", &pinfo->samples_len, 1);
    DRW_shgroup_uniform_float(grp, "invSampleCount", &pinfo->samples_len_inv, 1);
    DRW_shgroup_uniform_float(grp, "storedTexelSize", &pinfo->texel_size, 1);
    DRW_shgroup_uniform_float(grp, "nearClip", &pinfo->near_clip, 1);
    DRW_shgroup_uniform_float(grp, "farClip", &pinfo->far_clip, 1);
    DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
    DRW_shgroup_uniform_texture(grp, "probeDepth", rt_depth);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRW_shgroup_call(grp, geom, NULL);
  }

  {
    DRW_PASS_CREATE(psl->probe_grid_fill, DRW_STATE_WRITE_COLOR);

    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_probe_grid_fill_sh_get(),
                                              psl->probe_grid_fill);

    DRW_shgroup_uniform_texture_ref(grp, "irradianceGrid", &light_cache->grid_tx.tex);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRW_shgroup_call(grp, geom, NULL);
  }
}

void EEVEE_lightprobes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  LightCache *lcache = stl->g_data->light_cache;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  pinfo->num_planar = 0;
  pinfo->vis_data.collection = NULL;
  pinfo->do_grid_update = false;
  pinfo->do_cube_update = false;

  {
    DRW_PASS_CREATE(psl->probe_background, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRWShadingGroup *grp = NULL;

    Scene *scene = draw_ctx->scene;
    World *wo = scene->world;

    const float *col = G_draw.block.colorBackground;

    /* LookDev */
    EEVEE_lookdev_cache_init(vedata, &grp, psl->probe_background, 1.0f, wo, pinfo);
    /* END */
    if (!grp && wo) {
      col = &wo->horr;

      if (wo->use_nodes && wo->nodetree) {
        static float error_col[3] = {1.0f, 0.0f, 1.0f};
        struct GPUMaterial *gpumat = EEVEE_material_world_lightprobe_get(scene, wo);

        eGPUMaterialStatus status = GPU_material_status(gpumat);

        switch (status) {
          case GPU_MAT_SUCCESS:
            grp = DRW_shgroup_material_create(gpumat, psl->probe_background);
            DRW_shgroup_uniform_float_copy(grp, "backgroundAlpha", 1.0f);
            /* TODO (fclem): remove those (need to clean the GLSL files). */
            DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
            DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
            DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
            DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
            DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
            DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
            DRW_shgroup_call(grp, geom, NULL);
            break;
          default:
            col = error_col;
            break;
        }
      }
    }

    /* Fallback if shader fails or if not using nodetree. */
    if (grp == NULL) {
      grp = DRW_shgroup_create(EEVEE_shaders_probe_default_sh_get(), psl->probe_background);
      DRW_shgroup_uniform_vec3(grp, "color", col, 1);
      DRW_shgroup_uniform_float_copy(grp, "backgroundAlpha", 1.0f);
      DRW_shgroup_call(grp, geom, NULL);
    }
  }

  if (DRW_state_draw_support() && !LOOK_DEV_STUDIO_LIGHT_ENABLED(draw_ctx->v3d)) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->probe_display, state);

    /* Cube Display */
    if (scene_eval->eevee.flag & SCE_EEVEE_SHOW_CUBEMAPS && lcache->cube_len > 1) {
      int cube_len = lcache->cube_len - 1; /* don't count the world. */
      DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_probe_cube_display_sh_get(),
                                                psl->probe_display);

      DRW_shgroup_uniform_texture_ref(grp, "probeCubes", &lcache->cube_tx.tex);
      DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
      DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
      DRW_shgroup_uniform_vec3(grp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);
      DRW_shgroup_uniform_float_copy(
          grp, "sphere_size", scene_eval->eevee.gi_cubemap_draw_size * 0.5f);
      /* TODO (fclem) get rid of those UBO. */
      DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
      DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);

      DRW_shgroup_call_procedural_triangles(grp, cube_len * 2, NULL);
    }

    /* Grid Display */
    if (scene_eval->eevee.flag & SCE_EEVEE_SHOW_IRRADIANCE) {
      EEVEE_LightGrid *egrid = lcache->grid_data + 1;
      for (int p = 1; p < lcache->grid_len; ++p, egrid++) {
        DRWShadingGroup *shgrp = DRW_shgroup_create(EEVEE_shaders_probe_grid_display_sh_get(),
                                                    psl->probe_display);

        DRW_shgroup_uniform_int(shgrp, "offset", &egrid->offset, 1);
        DRW_shgroup_uniform_ivec3(shgrp, "grid_resolution", egrid->resolution, 1);
        DRW_shgroup_uniform_vec3(shgrp, "corner", egrid->corner, 1);
        DRW_shgroup_uniform_vec3(shgrp, "increment_x", egrid->increment_x, 1);
        DRW_shgroup_uniform_vec3(shgrp, "increment_y", egrid->increment_y, 1);
        DRW_shgroup_uniform_vec3(shgrp, "increment_z", egrid->increment_z, 1);
        DRW_shgroup_uniform_vec3(shgrp, "screen_vecs[0]", DRW_viewport_screenvecs_get(), 2);
        DRW_shgroup_uniform_texture_ref(shgrp, "irradianceGrid", &lcache->grid_tx.tex);
        DRW_shgroup_uniform_float_copy(
            shgrp, "sphere_size", scene_eval->eevee.gi_irradiance_draw_size * 0.5f);
        /* TODO (fclem) get rid of those UBO. */
        DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
        DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
        DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
        DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
        int tri_count = egrid->resolution[0] * egrid->resolution[1] * egrid->resolution[2] * 2;
        DRW_shgroup_call_procedural_triangles(shgrp, tri_count, NULL);
      }
    }

    /* Planar Display */
    DRW_shgroup_instance_format(e_data.format_probe_display_planar,
                                {
                                    {"probe_id", DRW_ATTR_INT, 1},
                                    {"probe_mat", DRW_ATTR_FLOAT, 16},
                                });

    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_probe_planar_display_sh_get(),
                                              psl->probe_display);
    DRW_shgroup_uniform_texture_ref(grp, "probePlanars", &txl->planar_pool);

    stl->g_data->planar_display_shgrp = DRW_shgroup_call_buffer_instance(
        grp, e_data.format_probe_display_planar, DRW_cache_quad_get());
  }
  else {
    stl->g_data->planar_display_shgrp = NULL;
  }
}

static bool eevee_lightprobes_culling_test(Object *ob)
{
  LightProbe *probe = (LightProbe *)ob->data;

  switch (probe->type) {
    case LIGHTPROBE_TYPE_PLANAR: {
      /* See if this planar probe is inside the view frustum. If not, no need to update it. */
      /* NOTE: this could be bypassed if we want feedback loop mirrors for rendering. */
      BoundBox bbox;
      float tmp[4][4];
      const float min[3] = {-1.0f, -1.0f, -1.0f};
      const float max[3] = {1.0f, 1.0f, 1.0f};
      BKE_boundbox_init_from_minmax(&bbox, min, max);

      copy_m4_m4(tmp, ob->obmat);
      normalize_v3(tmp[2]);
      mul_v3_fl(tmp[2], probe->distinf);

      for (int v = 0; v < 8; ++v) {
        mul_m4_v3(tmp, bbox.vec[v]);
      }
      const DRWView *default_view = DRW_view_default_get();
      return DRW_culling_box_test(default_view, &bbox);
    }
    case LIGHTPROBE_TYPE_CUBE:
      return true; /* TODO */
    case LIGHTPROBE_TYPE_GRID:
      return true; /* TODO */
  }
  BLI_assert(0);
  return true;
}

void EEVEE_lightprobes_cache_add(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *ob)
{
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  LightProbe *probe = (LightProbe *)ob->data;

  if ((probe->type == LIGHTPROBE_TYPE_CUBE && pinfo->num_cube >= MAX_PROBE) ||
      (probe->type == LIGHTPROBE_TYPE_GRID && pinfo->num_grid >= MAX_PROBE) ||
      (probe->type == LIGHTPROBE_TYPE_PLANAR && pinfo->num_planar >= MAX_PLANAR)) {
    printf("Too many probes in the view !!!\n");
    return;
  }

  if (probe->type == LIGHTPROBE_TYPE_PLANAR) {
    /* TODO(fclem): Culling should be done after cache generation.
     * This is needed for future draw cache persistence. */
    if (!eevee_lightprobes_culling_test(ob)) {
      return; /* Culled */
    }
    EEVEE_lightprobes_planar_data_from_object(
        ob, &pinfo->planar_data[pinfo->num_planar], &pinfo->planar_vis_tests[pinfo->num_planar]);
    /* Debug Display */
    DRWCallBuffer *grp = vedata->stl->g_data->planar_display_shgrp;
    if (grp && (probe->flag & LIGHTPROBE_FLAG_SHOW_DATA)) {
      DRW_buffer_add_entry(grp, &pinfo->num_planar, ob->obmat);
    }

    pinfo->num_planar++;
  }
  else {
    EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
    if (ped->need_update) {
      if (probe->type == LIGHTPROBE_TYPE_GRID) {
        pinfo->do_grid_update = true;
      }
      else {
        pinfo->do_cube_update = true;
      }
      ped->need_update = false;
    }
  }
}

void EEVEE_lightprobes_grid_data_from_object(Object *ob, EEVEE_LightGrid *egrid, int *offset)
{
  LightProbe *probe = (LightProbe *)ob->data;

  copy_v3_v3_int(egrid->resolution, &probe->grid_resolution_x);

  /* Save current offset and advance it for the next grid. */
  egrid->offset = *offset;
  *offset += egrid->resolution[0] * egrid->resolution[1] * egrid->resolution[2];

  /* Add one for level 0 */
  float fac = 1.0f / max_ff(1e-8f, probe->falloff);
  egrid->attenuation_scale = fac / max_ff(1e-8f, probe->distinf);
  egrid->attenuation_bias = fac;

  /* Update transforms */
  float cell_dim[3], half_cell_dim[3];
  cell_dim[0] = 2.0f / egrid->resolution[0];
  cell_dim[1] = 2.0f / egrid->resolution[1];
  cell_dim[2] = 2.0f / egrid->resolution[2];

  mul_v3_v3fl(half_cell_dim, cell_dim, 0.5f);

  /* Matrix converting world space to cell ranges. */
  invert_m4_m4(egrid->mat, ob->obmat);

  /* First cell. */
  copy_v3_fl(egrid->corner, -1.0f);
  add_v3_v3(egrid->corner, half_cell_dim);
  mul_m4_v3(ob->obmat, egrid->corner);

  /* Opposite neighbor cell. */
  copy_v3_fl3(egrid->increment_x, cell_dim[0], 0.0f, 0.0f);
  add_v3_v3(egrid->increment_x, half_cell_dim);
  add_v3_fl(egrid->increment_x, -1.0f);
  mul_m4_v3(ob->obmat, egrid->increment_x);
  sub_v3_v3(egrid->increment_x, egrid->corner);

  copy_v3_fl3(egrid->increment_y, 0.0f, cell_dim[1], 0.0f);
  add_v3_v3(egrid->increment_y, half_cell_dim);
  add_v3_fl(egrid->increment_y, -1.0f);
  mul_m4_v3(ob->obmat, egrid->increment_y);
  sub_v3_v3(egrid->increment_y, egrid->corner);

  copy_v3_fl3(egrid->increment_z, 0.0f, 0.0f, cell_dim[2]);
  add_v3_v3(egrid->increment_z, half_cell_dim);
  add_v3_fl(egrid->increment_z, -1.0f);
  mul_m4_v3(ob->obmat, egrid->increment_z);
  sub_v3_v3(egrid->increment_z, egrid->corner);

  /* Visibility bias */
  egrid->visibility_bias = 0.05f * probe->vis_bias;
  egrid->visibility_bleed = probe->vis_bleedbias;
  egrid->visibility_range = 1.0f + sqrtf(max_fff(len_squared_v3(egrid->increment_x),
                                                 len_squared_v3(egrid->increment_y),
                                                 len_squared_v3(egrid->increment_z)));
}

void EEVEE_lightprobes_cube_data_from_object(Object *ob, EEVEE_LightProbe *eprobe)
{
  LightProbe *probe = (LightProbe *)ob->data;

  /* Update transforms */
  copy_v3_v3(eprobe->position, ob->obmat[3]);

  /* Attenuation */
  eprobe->attenuation_type = probe->attenuation_type;
  eprobe->attenuation_fac = 1.0f / max_ff(1e-8f, probe->falloff);

  unit_m4(eprobe->attenuationmat);
  scale_m4_fl(eprobe->attenuationmat, probe->distinf);
  mul_m4_m4m4(eprobe->attenuationmat, ob->obmat, eprobe->attenuationmat);
  invert_m4(eprobe->attenuationmat);

  /* Parallax */
  unit_m4(eprobe->parallaxmat);

  if ((probe->flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0) {
    eprobe->parallax_type = probe->parallax_type;
    scale_m4_fl(eprobe->parallaxmat, probe->distpar);
  }
  else {
    eprobe->parallax_type = probe->attenuation_type;
    scale_m4_fl(eprobe->parallaxmat, probe->distinf);
  }

  mul_m4_m4m4(eprobe->parallaxmat, ob->obmat, eprobe->parallaxmat);
  invert_m4(eprobe->parallaxmat);
}

void EEVEE_lightprobes_planar_data_from_object(Object *ob,
                                               EEVEE_PlanarReflection *eplanar,
                                               EEVEE_LightProbeVisTest *vis_test)
{
  LightProbe *probe = (LightProbe *)ob->data;
  float normat[4][4], imat[4][4];

  vis_test->collection = probe->visibility_grp;
  vis_test->invert = probe->flag & LIGHTPROBE_FLAG_INVERT_GROUP;
  vis_test->cached = false;

  /* Computing mtx : matrix that mirror position around object's XY plane. */
  normalize_m4_m4(normat, ob->obmat); /* object > world */
  invert_m4_m4(imat, normat);         /* world > object */
  /* XY reflection plane */
  imat[0][2] = -imat[0][2];
  imat[1][2] = -imat[1][2];
  imat[2][2] = -imat[2][2];
  imat[3][2] = -imat[3][2];                /* world > object > mirrored obj */
  mul_m4_m4m4(eplanar->mtx, normat, imat); /* world > object > mirrored obj > world */

  /* Compute clip plane equation / normal. */
  copy_v3_v3(eplanar->plane_equation, ob->obmat[2]);
  normalize_v3(eplanar->plane_equation); /* plane normal */
  eplanar->plane_equation[3] = -dot_v3v3(eplanar->plane_equation, ob->obmat[3]);
  eplanar->clipsta = probe->clipsta;

  /* Compute XY clip planes. */
  normalize_v3_v3(eplanar->clip_vec_x, ob->obmat[0]);
  normalize_v3_v3(eplanar->clip_vec_y, ob->obmat[1]);

  float vec[3] = {0.0f, 0.0f, 0.0f};
  vec[0] = 1.0f;
  vec[1] = 0.0f;
  vec[2] = 0.0f;
  mul_m4_v3(ob->obmat, vec); /* Point on the edge */
  eplanar->clip_edge_x_pos = dot_v3v3(eplanar->clip_vec_x, vec);

  vec[0] = 0.0f;
  vec[1] = 1.0f;
  vec[2] = 0.0f;
  mul_m4_v3(ob->obmat, vec); /* Point on the edge */
  eplanar->clip_edge_y_pos = dot_v3v3(eplanar->clip_vec_y, vec);

  vec[0] = -1.0f;
  vec[1] = 0.0f;
  vec[2] = 0.0f;
  mul_m4_v3(ob->obmat, vec); /* Point on the edge */
  eplanar->clip_edge_x_neg = dot_v3v3(eplanar->clip_vec_x, vec);

  vec[0] = 0.0f;
  vec[1] = -1.0f;
  vec[2] = 0.0f;
  mul_m4_v3(ob->obmat, vec); /* Point on the edge */
  eplanar->clip_edge_y_neg = dot_v3v3(eplanar->clip_vec_y, vec);

  /* Facing factors */
  float max_angle = max_ff(1e-2f, 1.0f - probe->falloff) * M_PI * 0.5f;
  float min_angle = 0.0f;
  eplanar->facing_scale = 1.0f / max_ff(1e-8f, cosf(min_angle) - cosf(max_angle));
  eplanar->facing_bias = -min_ff(1.0f - 1e-8f, cosf(max_angle)) * eplanar->facing_scale;

  /* Distance factors */
  float max_dist = probe->distinf;
  float min_dist = min_ff(1.0f - 1e-8f, 1.0f - probe->falloff) * probe->distinf;
  eplanar->attenuation_scale = -1.0f / max_ff(1e-8f, max_dist - min_dist);
  eplanar->attenuation_bias = max_dist * -eplanar->attenuation_scale;
}

static void lightbake_planar_ensure_view(EEVEE_PlanarReflection *eplanar,
                                         const DRWView *main_view,
                                         DRWView **r_planar_view)
{
  float winmat[4][4], viewmat[4][4];
  DRW_view_viewmat_get(main_view, viewmat, false);
  /* Temporal sampling jitter should be already applied to the DRW_MAT_WIN. */
  DRW_view_winmat_get(main_view, winmat, false);
  /* Invert X to avoid flipping the triangle facing direction. */
  winmat[0][0] = -winmat[0][0];
  winmat[1][0] = -winmat[1][0];
  winmat[2][0] = -winmat[2][0];
  winmat[3][0] = -winmat[3][0];
  /* Reflect Camera Matrix. */
  mul_m4_m4m4(viewmat, viewmat, eplanar->mtx);

  if (*r_planar_view == NULL) {
    *r_planar_view = DRW_view_create(
        viewmat, winmat, NULL, NULL, EEVEE_lightprobes_obj_visibility_cb);
    /* Compute offset plane equation (fix missing texels near reflection plane). */
    float clip_plane[4];
    copy_v4_v4(clip_plane, eplanar->plane_equation);
    clip_plane[3] += eplanar->clipsta;
    /* Set clipping plane */
    DRW_view_clip_planes_set(*r_planar_view, &clip_plane, 1);
  }
  else {
    DRW_view_update(*r_planar_view, viewmat, winmat, NULL, NULL);
  }
}

static void eevee_lightprobes_extract_from_cache(EEVEE_LightProbesInfo *pinfo, LightCache *lcache)
{
  /* copy the entire cache for now (up to MAX_PROBE) */
  /* TODO Frutum cull to only add visible probes. */
  memcpy(pinfo->probe_data,
         lcache->cube_data,
         sizeof(EEVEE_LightProbe) * max_ii(1, min_ii(lcache->cube_len, MAX_PROBE)));
  /* TODO compute the max number of grid based on sample count. */
  memcpy(pinfo->grid_data,
         lcache->grid_data,
         sizeof(EEVEE_LightGrid) * max_ii(1, min_ii(lcache->grid_len, MAX_GRID)));
}

void EEVEE_lightprobes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  LightCache *light_cache = stl->g_data->light_cache;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  eevee_lightprobes_extract_from_cache(sldata->probes, light_cache);

  DRW_uniformbuffer_update(sldata->probe_ubo, &sldata->probes->probe_data);
  DRW_uniformbuffer_update(sldata->grid_ubo, &sldata->probes->grid_data);

  /* For shading, save max level of the octahedron map */
  sldata->common_data.prb_lod_cube_max = (float)light_cache->mips_len - 1.0f;
  sldata->common_data.prb_lod_planar_max = (float)MAX_PLANAR_LOD_LEVEL;
  sldata->common_data.prb_irradiance_vis_size = light_cache->vis_res;
  sldata->common_data.prb_irradiance_smooth = SQUARE(scene_eval->eevee.gi_irradiance_smoothing);
  sldata->common_data.prb_num_render_cube = max_ii(1, light_cache->cube_len);
  sldata->common_data.prb_num_render_grid = max_ii(1, light_cache->grid_len);
  sldata->common_data.prb_num_planar = pinfo->num_planar;

  if (pinfo->num_planar != pinfo->cache_num_planar) {
    DRW_TEXTURE_FREE_SAFE(vedata->txl->planar_pool);
    DRW_TEXTURE_FREE_SAFE(vedata->txl->planar_depth);
    pinfo->cache_num_planar = pinfo->num_planar;
  }
  planar_pool_ensure_alloc(vedata, pinfo->num_planar);

  /* If lightcache auto-update is enable we tag the relevant part
   * of the cache to update and fire up a baking job. */
  if (!DRW_state_is_image_render() && !DRW_state_is_opengl_render() &&
      (pinfo->do_grid_update || pinfo->do_cube_update)) {
    BLI_assert(draw_ctx->evil_C);

    if (draw_ctx->scene->eevee.flag & SCE_EEVEE_GI_AUTOBAKE) {
      Scene *scene_orig = DEG_get_input_scene(draw_ctx->depsgraph);
      if (scene_orig->eevee.light_cache != NULL) {
        if (pinfo->do_grid_update) {
          scene_orig->eevee.light_cache->flag |= LIGHTCACHE_UPDATE_GRID;
        }
        /* If we update grid we need to update the cubemaps too.
         * So always refresh cubemaps. */
        scene_orig->eevee.light_cache->flag |= LIGHTCACHE_UPDATE_CUBE;
        /* Tag the lightcache to auto update. */
        scene_orig->eevee.light_cache->flag |= LIGHTCACHE_UPDATE_AUTO;
        /* Use a notifier to trigger the operator after drawing. */
        WM_event_add_notifier(draw_ctx->evil_C, NC_LIGHTPROBE, scene_orig);
      }
    }
  }

  if (pinfo->num_planar > 0) {
    EEVEE_PassList *psl = vedata->psl;
    EEVEE_TextureList *txl = vedata->txl;
    DRW_PASS_CREATE(psl->probe_planar_downsample_ps, DRW_STATE_WRITE_COLOR);

    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_probe_planar_downsample_sh_get(),
                                              psl->probe_planar_downsample_ps);

    DRW_shgroup_uniform_texture_ref(grp, "source", &txl->planar_pool);
    DRW_shgroup_uniform_float(grp, "fireflyFactor", &sldata->common_data.ssr_firefly_fac, 1);
    DRW_shgroup_call_procedural_triangles(grp, pinfo->num_planar, NULL);
  }
}

/* -------------------------------------------------------------------- */
/** \name Rendering
 * \{ */

typedef struct EEVEE_BakeRenderData {
  EEVEE_Data *vedata;
  EEVEE_ViewLayerData *sldata;
  struct GPUFrameBuffer **face_fb; /* should contain 6 framebuffer */
} EEVEE_BakeRenderData;

static void render_cubemap(void (*callback)(int face, EEVEE_BakeRenderData *user_data),
                           EEVEE_BakeRenderData *user_data,
                           const float pos[3],
                           float clipsta,
                           float clipend)
{
  DRWMatrixState matstate;

  /* Move to capture position */
  float posmat[4][4];
  unit_m4(posmat);
  negate_v3_v3(posmat[3], pos);

  perspective_m4(matstate.winmat, -clipsta, clipsta, -clipsta, clipsta, clipsta, clipend);
  invert_m4_m4(matstate.wininv, matstate.winmat);

  /* 1 - Render to each cubeface individually.
   * We do this instead of using geometry shader because a) it's faster,
   * b) it's easier than fixing the nodetree shaders (for view dependent effects). */
  for (int i = 0; i < 6; ++i) {
    /* Setup custom matrices */
    mul_m4_m4m4(matstate.viewmat, cubefacemat[i], posmat);
    mul_m4_m4m4(matstate.persmat, matstate.winmat, matstate.viewmat);
    invert_m4_m4(matstate.persinv, matstate.persmat);
    invert_m4_m4(matstate.viewinv, matstate.viewmat);
    invert_m4_m4(matstate.wininv, matstate.winmat);

    DRW_viewport_matrix_override_set_all(&matstate);

    callback(i, user_data);
  }
}

static void render_reflections(void (*callback)(int face, EEVEE_BakeRenderData *user_data),
                               EEVEE_BakeRenderData *user_data,
                               EEVEE_PlanarReflection *planar_data,
                               int ref_count)
{
  EEVEE_StorageList *stl = user_data->vedata->stl;
  DRWView *main_view = stl->effects->taa_view;
  DRWView **views = stl->g_data->planar_views;
  /* Prepare views at the same time for faster culling. */
  for (int i = 0; i < ref_count; ++i) {
    lightbake_planar_ensure_view(&planar_data[i], main_view, &views[i]);
  }

  for (int i = 0; i < ref_count; ++i) {
    DRW_view_set_active(views[i]);
    callback(i, user_data);
  }
}

static void lightbake_render_world_face(int face, EEVEE_BakeRenderData *user_data)
{
  EEVEE_PassList *psl = user_data->vedata->psl;
  struct GPUFrameBuffer **face_fb = user_data->face_fb;

  /* For world probe, we don't need to clear the color buffer
   * since we render the background directly. */
  GPU_framebuffer_bind(face_fb[face]);
  GPU_framebuffer_clear_depth(face_fb[face], 1.0f);
  DRW_draw_pass(psl->probe_background);
}

void EEVEE_lightbake_render_world(EEVEE_ViewLayerData *UNUSED(sldata),
                                  EEVEE_Data *vedata,
                                  struct GPUFrameBuffer *face_fb[6])
{
  EEVEE_BakeRenderData brdata = {
      .vedata = vedata,
      .face_fb = face_fb,
  };

  render_cubemap(lightbake_render_world_face, &brdata, (float[3]){0.0f}, 1.0f, 10.0f);
}

static void lightbake_render_scene_face(int face, EEVEE_BakeRenderData *user_data)
{
  EEVEE_ViewLayerData *sldata = user_data->sldata;
  EEVEE_PassList *psl = user_data->vedata->psl;
  struct GPUFrameBuffer **face_fb = user_data->face_fb;

  /* Be sure that cascaded shadow maps are updated. */
  EEVEE_draw_shadows(sldata, user_data->vedata, NULL /* TODO */);

  GPU_framebuffer_bind(face_fb[face]);
  GPU_framebuffer_clear_depth(face_fb[face], 1.0f);

  DRW_draw_pass(psl->depth_pass);
  DRW_draw_pass(psl->depth_pass_cull);
  DRW_draw_pass(psl->probe_background);
  DRW_draw_pass(psl->material_pass);
  DRW_draw_pass(psl->material_pass_cull);
  DRW_draw_pass(psl->sss_pass); /* Only output standard pass */
  DRW_draw_pass(psl->sss_pass_cull);
  EEVEE_draw_default_passes(psl);
}

/* Render the scene to the probe_rt texture. */
void EEVEE_lightbake_render_scene(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  struct GPUFrameBuffer *face_fb[6],
                                  const float pos[3],
                                  float near_clip,
                                  float far_clip)
{
  EEVEE_BakeRenderData brdata = {
      .vedata = vedata,
      .sldata = sldata,
      .face_fb = face_fb,
  };

  render_cubemap(lightbake_render_scene_face, &brdata, pos, near_clip, far_clip);
}

static void lightbake_render_scene_reflected(int layer, EEVEE_BakeRenderData *user_data)
{
  EEVEE_Data *vedata = user_data->vedata;
  EEVEE_ViewLayerData *sldata = user_data->sldata;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;

  GPU_framebuffer_ensure_config(&fbl->planarref_fb,
                                {GPU_ATTACHMENT_TEXTURE_LAYER(txl->planar_depth, layer),
                                 GPU_ATTACHMENT_TEXTURE_LAYER(txl->planar_pool, layer)});

  /* Use visibility info for this planar reflection. */
  pinfo->vis_data = pinfo->planar_vis_tests[layer];

  /* Avoid using the texture attached to framebuffer when rendering. */
  /* XXX */
  GPUTexture *tmp_planar_pool = txl->planar_pool;
  GPUTexture *tmp_planar_depth = txl->planar_depth;
  txl->planar_pool = e_data.planar_pool_placeholder;
  txl->planar_depth = e_data.depth_array_placeholder;

  DRW_stats_group_start("Planar Reflection");

  /* Be sure that cascaded shadow maps are updated. */
  EEVEE_draw_shadows(sldata, vedata, stl->g_data->planar_views[layer]);

  GPU_framebuffer_bind(fbl->planarref_fb);
  GPU_framebuffer_clear_depth(fbl->planarref_fb, 1.0);

  float prev_background_alpha = vedata->stl->g_data->background_alpha;
  vedata->stl->g_data->background_alpha = 1.0f;

  /* Slight modification: we handle refraction as normal
   * shading and don't do SSRefraction. */

  DRW_draw_pass(psl->depth_pass_clip);
  DRW_draw_pass(psl->depth_pass_clip_cull);
  DRW_draw_pass(psl->refract_depth_pass);
  DRW_draw_pass(psl->refract_depth_pass_cull);

  DRW_draw_pass(psl->probe_background);
  EEVEE_create_minmax_buffer(vedata, tmp_planar_depth, layer);
  EEVEE_occlusion_compute(sldata, vedata, tmp_planar_depth, layer);

  GPU_framebuffer_bind(fbl->planarref_fb);

  /* Shading pass */
  EEVEE_draw_default_passes(psl);
  DRW_draw_pass(psl->material_pass);
  DRW_draw_pass(psl->material_pass_cull);
  DRW_draw_pass(psl->sss_pass); /* Only output standard pass */
  DRW_draw_pass(psl->sss_pass_cull);
  DRW_draw_pass(psl->refract_pass);

  /* Transparent */
  if (DRW_state_is_image_render()) {
    /* Do the reordering only for offline because it can be costly. */
    DRW_pass_sort_shgroup_z(psl->transparent_pass);
  }
  DRW_draw_pass(psl->transparent_pass);

  DRW_stats_group_end();

  /* Restore */
  txl->planar_pool = tmp_planar_pool;
  txl->planar_depth = tmp_planar_depth;

  vedata->stl->g_data->background_alpha = prev_background_alpha;
}

static void eevee_lightbake_render_scene_to_planars(EEVEE_ViewLayerData *sldata,
                                                    EEVEE_Data *vedata)
{
  EEVEE_BakeRenderData brdata = {
      .vedata = vedata,
      .sldata = sldata,
  };

  render_reflections(lightbake_render_scene_reflected,
                     &brdata,
                     sldata->probes->planar_data,
                     sldata->probes->num_planar);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Filtering
 * \{ */

/* Glossy filter rt_color to light_cache->cube_tx.tex at index probe_idx */
void EEVEE_lightbake_filter_glossy(EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   struct GPUTexture *rt_color,
                                   struct GPUFrameBuffer *fb,
                                   int probe_idx,
                                   float intensity,
                                   int maxlevel,
                                   float filter_quality,
                                   float firefly_fac)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  LightCache *light_cache = vedata->stl->g_data->light_cache;

  float target_size = (float)GPU_texture_width(rt_color);

  /* Max lod used from the render target probe */
  pinfo->lod_rt_max = floorf(log2f(target_size)) - 2.0f;
  pinfo->intensity_fac = intensity;

  /* Start fresh */
  GPU_framebuffer_ensure_config(&fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_NONE});

  /* 2 - Let gpu create Mipmaps for Filtered Importance Sampling. */
  /* Bind next framebuffer to be able to gen. mips for probe_rt. */
  EEVEE_downsample_cube_buffer(vedata, rt_color, (int)(pinfo->lod_rt_max));

  /* 3 - Render to probe array to the specified layer, do prefiltering. */
  int mipsize = GPU_texture_width(light_cache->cube_tx.tex);
  for (int i = 0; i < maxlevel + 1; i++) {
    float bias = (i == 0) ? -1.0f : 1.0f;
    pinfo->texel_size = 1.0f / (float)mipsize;
    pinfo->padding_size = (i == maxlevel) ? 0 : (float)(1 << (maxlevel - i - 1));
    pinfo->padding_size *= pinfo->texel_size;
    pinfo->layer = probe_idx;
    pinfo->roughness = i / (float)maxlevel;
    pinfo->roughness *= pinfo->roughness;     /* Disney Roughness */
    pinfo->roughness *= pinfo->roughness;     /* Distribute Roughness accros lod more evenly */
    CLAMP(pinfo->roughness, 1e-8f, 0.99999f); /* Avoid artifacts */

#if 1 /* Variable Sample count (fast) */
    switch (i) {
      case 0:
        pinfo->samples_len = 1.0f;
        break;
      case 1:
        pinfo->samples_len = 16.0f;
        break;
      case 2:
        pinfo->samples_len = 32.0f;
        break;
      case 3:
        pinfo->samples_len = 64.0f;
        break;
      default:
        pinfo->samples_len = 128.0f;
        break;
    }
#else /* Constant Sample count (slow) */
    pinfo->samples_len = 1024.0f;
#endif
    /* Cannot go higher than HAMMERSLEY_SIZE */
    CLAMP(filter_quality, 1.0f, 8.0f);
    pinfo->samples_len *= filter_quality;

    pinfo->samples_len_inv = 1.0f / pinfo->samples_len;
    pinfo->lodfactor = bias +
                       0.5f * log((float)(target_size * target_size) * pinfo->samples_len_inv) /
                           log(2);
    pinfo->firefly_fac = (firefly_fac > 0.0) ? firefly_fac : 1e16;

    GPU_framebuffer_ensure_config(
        &fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE_MIP(light_cache->cube_tx.tex, i)});
    GPU_framebuffer_bind(fb);
    GPU_framebuffer_viewport_set(fb, 0, 0, mipsize, mipsize);
    DRW_draw_pass(psl->probe_glossy_compute);

    mipsize /= 2;
    CLAMP_MIN(mipsize, 1);
  }
}

/* Diffuse filter rt_color to light_cache->grid_tx.tex at index grid_offset */
void EEVEE_lightbake_filter_diffuse(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    struct GPUTexture *rt_color,
                                    struct GPUFrameBuffer *fb,
                                    int grid_offset,
                                    float intensity)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  LightCache *light_cache = vedata->stl->g_data->light_cache;

  float target_size = (float)GPU_texture_width(rt_color);

  pinfo->intensity_fac = intensity;

  /* find cell position on the virtual 3D texture */
  /* NOTE : Keep in sync with load_irradiance_cell() */
#if defined(IRRADIANCE_SH_L2)
  int size[2] = {3, 3};
#elif defined(IRRADIANCE_CUBEMAP)
  int size[2] = {8, 8};
  pinfo->samples_len = 1024.0f;
#elif defined(IRRADIANCE_HL2)
  int size[2] = {3, 2};
  pinfo->samples_len = 1024.0f;
#endif

  int cell_per_row = GPU_texture_width(light_cache->grid_tx.tex) / size[0];
  int x = size[0] * (grid_offset % cell_per_row);
  int y = size[1] * (grid_offset / cell_per_row);

#ifndef IRRADIANCE_SH_L2
  /* Tweaking parameters to balance perf. vs precision */
  const float bias = 0.0f;
  pinfo->samples_len_inv = 1.0f / pinfo->samples_len;
  pinfo->lodfactor = bias + 0.5f *
                                log((float)(target_size * target_size) * pinfo->samples_len_inv) /
                                log(2);
  pinfo->lod_rt_max = floorf(log2f(target_size)) - 2.0f;
#else
  pinfo->shres = 32;        /* Less texture fetches & reduce branches */
  pinfo->lod_rt_max = 2.0f; /* Improve cache reuse */
#endif

  /* Start fresh */
  GPU_framebuffer_ensure_config(&fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_NONE});

  /* 4 - Compute diffuse irradiance */
  EEVEE_downsample_cube_buffer(vedata, rt_color, (int)(pinfo->lod_rt_max));

  GPU_framebuffer_ensure_config(
      &fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE_LAYER(light_cache->grid_tx.tex, 0)});
  GPU_framebuffer_bind(fb);
  GPU_framebuffer_viewport_set(fb, x, y, size[0], size[1]);
  DRW_draw_pass(psl->probe_diffuse_compute);
}

/* Filter rt_depth to light_cache->grid_tx.tex at index grid_offset */
void EEVEE_lightbake_filter_visibility(EEVEE_ViewLayerData *sldata,
                                       EEVEE_Data *vedata,
                                       struct GPUTexture *UNUSED(rt_depth),
                                       struct GPUFrameBuffer *fb,
                                       int grid_offset,
                                       float clipsta,
                                       float clipend,
                                       float vis_range,
                                       float vis_blur,
                                       int vis_size)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  LightCache *light_cache = vedata->stl->g_data->light_cache;

  pinfo->samples_len = 512.0f; /* TODO refine */
  pinfo->samples_len_inv = 1.0f / pinfo->samples_len;
  pinfo->shres = vis_size;
  pinfo->visibility_range = vis_range;
  pinfo->visibility_blur = vis_blur;
  pinfo->near_clip = -clipsta;
  pinfo->far_clip = -clipend;
  pinfo->texel_size = 1.0f / (float)vis_size;

  int cell_per_col = GPU_texture_height(light_cache->grid_tx.tex) / vis_size;
  int cell_per_row = GPU_texture_width(light_cache->grid_tx.tex) / vis_size;
  int x = vis_size * (grid_offset % cell_per_row);
  int y = vis_size * ((grid_offset / cell_per_row) % cell_per_col);
  int layer = 1 + ((grid_offset / cell_per_row) / cell_per_col);

  GPU_framebuffer_ensure_config(
      &fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE_LAYER(light_cache->grid_tx.tex, layer)});
  GPU_framebuffer_bind(fb);
  GPU_framebuffer_viewport_set(fb, x, y, vis_size, vis_size);
  DRW_draw_pass(psl->probe_visibility_compute);
}

/* Actually a simple downsampling */
static void downsample_planar(void *vedata, int level)
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

  const float *size = DRW_viewport_size_get();
  copy_v2_v2(stl->g_data->planar_texel_size, size);
  for (int i = 0; i < level - 1; ++i) {
    stl->g_data->planar_texel_size[0] /= 2.0f;
    stl->g_data->planar_texel_size[1] /= 2.0f;
    min_ff(floorf(stl->g_data->planar_texel_size[0]), 1.0f);
    min_ff(floorf(stl->g_data->planar_texel_size[1]), 1.0f);
  }
  invert_v2(stl->g_data->planar_texel_size);

  DRW_draw_pass(psl->probe_planar_downsample_ps);
}

static void EEVEE_lightbake_filter_planar(EEVEE_Data *vedata)
{
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;

  DRW_stats_group_start("Planar Probe Downsample");

  GPU_framebuffer_ensure_config(&fbl->planar_downsample_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->planar_pool)});

  GPU_framebuffer_recursive_downsample(
      fbl->planar_downsample_fb, MAX_PLANAR_LOD_LEVEL, &downsample_planar, vedata);
  DRW_stats_group_end();
}

/** \} */

void EEVEE_lightprobes_refresh_planar(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  DRWMatrixState saved_mats;

  if (pinfo->num_planar == 0) {
    /* Disable SSR if we cannot read previous frame */
    common_data->ssr_toggle = vedata->stl->g_data->valid_double_buffer;
    common_data->prb_num_planar = 0;
    return;
  }

  /* We need to save the Matrices before overidding them */
  DRW_viewport_matrix_get_all(&saved_mats);

  /* Temporary Remove all planar reflections (avoid lag effect). */
  common_data->prb_num_planar = 0;
  /* Turn off ssr to avoid black specular */
  common_data->ssr_toggle = false;
  common_data->sss_toggle = false;

  common_data->ray_type = EEVEE_RAY_GLOSSY;
  common_data->ray_depth = 1.0f;
  DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);

  /* Rendering happens here! */
  eevee_lightbake_render_scene_to_planars(sldata, vedata);

  /* Make sure no aditionnal visibility check runs after this. */
  pinfo->vis_data.collection = NULL;

  DRW_uniformbuffer_update(sldata->planar_ubo, &sldata->probes->planar_data);

  /* Restore */
  common_data->prb_num_planar = pinfo->num_planar;
  common_data->ssr_toggle = true;
  common_data->sss_toggle = true;

  /* Prefilter for SSR */
  if ((vedata->stl->effects->enabled_effects & EFFECT_SSR) != 0) {
    EEVEE_lightbake_filter_planar(vedata);
  }

  DRW_viewport_matrix_override_set_all(&saved_mats);

  if (DRW_state_is_image_render()) {
    /* Sort transparents because planar reflections could have re-sorted them. */
    DRW_pass_sort_shgroup_z(vedata->psl->transparent_pass);
  }

  /* Disable SSR if we cannot read previous frame */
  common_data->ssr_toggle = vedata->stl->g_data->valid_double_buffer;
}

void EEVEE_lightprobes_refresh(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);
  LightCache *light_cache = vedata->stl->g_data->light_cache;

  if ((light_cache->flag & LIGHTCACHE_UPDATE_WORLD) &&
      (light_cache->flag & LIGHTCACHE_BAKED) == 0) {
    DRWMatrixState saved_mats;
    DRW_viewport_matrix_get_all(&saved_mats);
    EEVEE_lightbake_update_world_quick(sldata, vedata, scene_eval);
    DRW_viewport_matrix_override_set_all(&saved_mats);
  }
}

void EEVEE_lightprobes_free(void)
{
  MEM_SAFE_FREE(e_data.format_probe_display_cube);
  MEM_SAFE_FREE(e_data.format_probe_display_planar);
  DRW_TEXTURE_FREE_SAFE(e_data.hammersley);
  DRW_TEXTURE_FREE_SAFE(e_data.planar_pool_placeholder);
  DRW_TEXTURE_FREE_SAFE(e_data.depth_placeholder);
  DRW_TEXTURE_FREE_SAFE(e_data.depth_array_placeholder);
}
