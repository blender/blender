/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_memblock.h"
#include "BLI_rand.h"
#include "BLI_string_utils.hh"

#include "BKE_global.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"

#include "DNA_curves_types.h"
#include "DNA_modifier_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "GPU_material.hh"

#include "DEG_depsgraph_query.hh"

#include "../eevee_next/eevee_lut.hh"
#include "eevee_engine.h"
#include "eevee_private.hh"

/* *********** STATIC *********** */
static struct {
  /* 64*64 array texture containing all LUTs and other utilitarian arrays.
   * Packing enables us to same precious textures slots. */
  GPUTexture *util_tex;
  GPUTexture *noise_tex;

  float noise_offsets[3];
} e_data = {nullptr}; /* Engine data */

struct EeveeMaterialCache {
  DRWShadingGroup *depth_grp;
  DRWShadingGroup *shading_grp;
  DRWShadingGroup *shadow_grp;
  GPUMaterial *shading_gpumat;
  /* Meh, Used by hair to ensure draw order when calling DRW_shgroup_create_sub.
   * Pointers to ghash values. */
  DRWShadingGroup **depth_grp_p;
  DRWShadingGroup **shading_grp_p;
  DRWShadingGroup **shadow_grp_p;
};

/* *********** FUNCTIONS *********** */

/* XXX TODO: define all shared resources in a shared place without duplication. */
GPUTexture *EEVEE_materials_get_util_tex()
{
  return e_data.util_tex;
}

void EEVEE_material_bind_resources(DRWShadingGroup *shgrp,
                                   GPUMaterial *gpumat,
                                   EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   const int *ssr_id,
                                   const float *refract_depth,
                                   const float alpha_clip_threshold,
                                   bool use_ssrefraction,
                                   bool use_alpha_blend)
{
  bool use_diffuse = GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE);
  bool use_glossy = GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY);
  bool use_refract = GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT);
  bool use_ao = GPU_material_flag_get(gpumat, GPU_MATFLAG_AO);

#ifdef __APPLE__
  /* NOTE: Some implementation do not optimize out the unused samplers. */
  use_diffuse = use_glossy = use_refract = use_ao = true;
#endif
  LightCache *lcache = vedata->stl->g_data->light_cache;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_PrivateData *pd = vedata->stl->g_data;

  DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
  DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block_ref(shgrp, "renderpass_block", &pd->renderpass_ubo);

  DRW_shgroup_uniform_float_copy(shgrp, "alphaClipThreshold", alpha_clip_threshold);

  DRW_shgroup_uniform_int_copy(shgrp, "outputSssId", 1);
  DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);
  if (use_diffuse || use_glossy || use_refract) {
    DRW_shgroup_uniform_texture_ref(shgrp, "shadowCubeTexture", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref(shgrp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);
  }
  if (use_diffuse || use_glossy || use_refract || use_ao) {
    DRW_shgroup_uniform_texture_ref(shgrp, "maxzBuffer", &vedata->txl->maxzbuffer);
  }
  if ((use_diffuse || use_glossy) && !use_ssrefraction) {
    DRW_shgroup_uniform_texture_ref(shgrp, "horizonBuffer", &effects->gtao_horizons);
  }
  if (use_diffuse) {
    DRW_shgroup_uniform_texture_ref(shgrp, "irradianceGrid", &lcache->grid_tx.tex);
  }
  if (use_glossy || use_refract) {
    DRW_shgroup_uniform_texture_ref(shgrp, "probeCubes", &lcache->cube_tx.tex);
  }
  if (use_glossy) {
    DRW_shgroup_uniform_texture_ref(shgrp, "probePlanars", &vedata->txl->planar_pool);
    DRW_shgroup_uniform_int_copy(shgrp, "outputSsrId", ssr_id ? *ssr_id : 0);
  }
  else {
    DRW_shgroup_uniform_int_copy(shgrp, "outputSsrId", 1);
  }
  if (use_refract) {
    DRW_shgroup_uniform_float_copy(
        shgrp, "refractionDepth", (refract_depth) ? *refract_depth : 0.0);
    if (use_ssrefraction) {
      DRW_shgroup_uniform_texture_ref(
          shgrp, "refractColorBuffer", &vedata->txl->filtered_radiance);
    }
  }
  if (use_alpha_blend) {
    DRW_shgroup_uniform_texture_ref(shgrp, "inScattering", &effects->volume_scatter);
    DRW_shgroup_uniform_texture_ref(shgrp, "inTransmittance", &effects->volume_transmit);
  }
}

static void eevee_init_noise_texture()
{
  e_data.noise_tex = DRW_texture_create_2d(
      64, 64, GPU_RGBA16F, DRWTextureFlag(0), (float *)blender::eevee::lut::blue_noise);
}

#define RUNTIME_LUT_CREATION 0

static void eevee_init_util_texture()
{
  const int layers = 4 + 16;
  float(*texels)[4] = static_cast<float(*)[4]>(
      MEM_mallocN(sizeof(float[4]) * 64 * 64 * layers, "utils texels"));
  float(*texels_layer)[4] = texels;

  /* Copy ltc_mat_ggx into 1st layer */
  memcpy(texels_layer, blender::eevee::lut::ltc_mat_ggx, sizeof(float[4]) * 64 * 64);
  texels_layer += 64 * 64;

  /* Copy brdf_ggx into 2nd layer red, green and blue channels. */
  for (int x = 0; x < 64; x++) {
    for (int y = 0; y < 64; y++) {
      texels_layer[y * 64 + x][0] = blender::eevee::lut::brdf_ggx[y][x][0];
      texels_layer[y * 64 + x][1] = blender::eevee::lut::brdf_ggx[y][x][1];
      texels_layer[y * 64 + x][2] = blender::eevee::lut::brdf_ggx[y][x][2];
      texels_layer[y * 64 + x][3] = 0.0f; /* UNUSED */
    }
  }
  texels_layer += 64 * 64;

  /* Copy blue noise in 3rd layer. */
  for (int x = 0; x < 64; x++) {
    for (int y = 0; y < 64; y++) {
      texels_layer[y * 64 + x][0] = blender::eevee::lut::blue_noise[y][x][0];
      texels_layer[y * 64 + x][1] = blender::eevee::lut::blue_noise[y][x][2];
      texels_layer[y * 64 + x][2] = cosf(blender::eevee::lut::blue_noise[y][x][1] * 2.0f * M_PI);
      texels_layer[y * 64 + x][3] = sinf(blender::eevee::lut::blue_noise[y][x][1] * 2.0f * M_PI);
    }
  }
  texels_layer += 64 * 64;

  /* Copy ltc_disk_integral in 4th layer.
   * Copy ltc_mag_ggx into blue and alpha channel. */
  for (int x = 0; x < 64; x++) {
    for (int y = 0; y < 64; y++) {
      float ltc_sum = blender::eevee::lut::ltc_mag_ggx[y][x][0] +
                      blender::eevee::lut::ltc_mag_ggx[y][x][1];
      float brdf_sum = blender::eevee::lut::brdf_ggx[y][x][0] +
                       blender::eevee::lut::brdf_ggx[y][x][1];
      texels_layer[y * 64 + x][0] = blender::eevee::lut::ltc_disk_integral[y][x][0];
      texels_layer[y * 64 + x][1] = ltc_sum / brdf_sum;
      texels_layer[y * 64 + x][2] = 0.0f; /* UNUSED */
      texels_layer[y * 64 + x][3] = 0.0f; /* UNUSED */
    }
  }
  texels_layer += 64 * 64;

  /* Copy BSDF GGX LUT in layer 5 - 21 */
  for (int j = 0; j < 16; j++) {
    for (int x = 0; x < 64; x++) {
      for (int y = 0; y < 64; y++) {
        /* BSDF LUT for `IOR < 1`. */
        texels_layer[y * 64 + x][0] = blender::eevee::lut::bsdf_ggx[j][y][x][0];
        texels_layer[y * 64 + x][1] = blender::eevee::lut::bsdf_ggx[j][y][x][1];
        texels_layer[y * 64 + x][2] = blender::eevee::lut::bsdf_ggx[j][y][x][2];
        /* BTDF LUT for `IOR > 1`, parameterized differently as above.
         * See `eevee_lut_comp.glsl`. */
        texels_layer[y * 64 + x][3] = blender::eevee::lut::btdf_ggx[j][y][x][0];
      }
    }
    texels_layer += 64 * 64;
  }

  eGPUTextureUsage util_usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
  e_data.util_tex = DRW_texture_create_2d_array_ex(64,
                                                   64,
                                                   layers,
                                                   GPU_RGBA16F,
                                                   util_usage,
                                                   DRWTextureFlag(DRW_TEX_FILTER | DRW_TEX_WRAP),
                                                   (float *)texels);

  MEM_freeN(texels);
}

void EEVEE_update_noise(EEVEE_PassList *psl, EEVEE_FramebufferList *fbl, const double offsets[3])
{
  e_data.noise_offsets[0] = offsets[0];
  e_data.noise_offsets[1] = offsets[1];
  e_data.noise_offsets[2] = offsets[2];

  GPU_framebuffer_bind(fbl->update_noise_fb);
  DRW_draw_pass(psl->update_noise_pass);
}

void EEVEE_materials_init(EEVEE_ViewLayerData *sldata,
                          EEVEE_Data *vedata,
                          EEVEE_StorageList *stl,
                          EEVEE_FramebufferList *fbl)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EEVEE_PrivateData *g_data = stl->g_data;

  if (!e_data.util_tex) {
    EEVEE_shaders_material_shaders_init();

    eevee_init_util_texture();
    eevee_init_noise_texture();
  }

  if (draw_ctx->rv3d) {
    copy_v4_v4(sldata->common_data.camera_uv_scale, draw_ctx->rv3d->viewcamtexcofac);
  }
  else {
    copy_v4_fl4(sldata->common_data.camera_uv_scale, 1.0f, 1.0f, 0.0f, 0.0f);
  }

  if (!DRW_state_is_image_render() && ((stl->effects->enabled_effects & EFFECT_TAA) == 0)) {
    sldata->common_data.alpha_hash_offset = 0.0f;
    sldata->common_data.alpha_hash_scale = 1.0f;
  }
  else {
    double r;
    BLI_halton_1d(5, 0.0, stl->effects->taa_current_sample - 1, &r);
    sldata->common_data.alpha_hash_offset = float(r);
    sldata->common_data.alpha_hash_scale = 0.01f;
  }

  {
    /* Update noise Frame-buffer. */
    GPU_framebuffer_ensure_config(
        &fbl->update_noise_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE_LAYER(e_data.util_tex, 2)});
  }

  {
    /* Create RenderPass UBO */
    if (sldata->renderpass_ubo.combined == nullptr) {
      EEVEE_RenderPassData data;
      data = EEVEE_RenderPassData{true, true, true, true, true, false, false, false, 0};
      sldata->renderpass_ubo.combined = GPU_uniformbuf_create_ex(
          sizeof(data), &data, "renderpass_ubo.combined");

      data = EEVEE_RenderPassData{true, false, false, false, false, true, false, false, 0};
      sldata->renderpass_ubo.diff_color = GPU_uniformbuf_create_ex(
          sizeof(data), &data, "renderpass_ubo.diff_color");

      data = EEVEE_RenderPassData{true, true, false, false, false, false, false, false, 0};
      sldata->renderpass_ubo.diff_light = GPU_uniformbuf_create_ex(
          sizeof(data), &data, "renderpass_ubo.diff_light");

      data = EEVEE_RenderPassData{false, false, true, false, false, false, false, false, 0};
      sldata->renderpass_ubo.spec_color = GPU_uniformbuf_create_ex(
          sizeof(data), &data, "renderpass_ubo.spec_color");

      data = EEVEE_RenderPassData{false, false, true, true, false, false, false, false, 0};
      sldata->renderpass_ubo.spec_light = GPU_uniformbuf_create_ex(
          sizeof(data), &data, "renderpass_ubo.spec_light");

      data = EEVEE_RenderPassData{false, false, false, false, true, false, false, false, 0};
      sldata->renderpass_ubo.emit = GPU_uniformbuf_create_ex(
          sizeof(data), &data, "renderpass_ubo.emit");

      data = EEVEE_RenderPassData{true, true, true, true, true, false, true, false, 0};
      sldata->renderpass_ubo.environment = GPU_uniformbuf_create_ex(
          sizeof(data), &data, "renderpass_ubo.environment");
    }

    /* Used combined pass by default. */
    g_data->renderpass_ubo = sldata->renderpass_ubo.combined;

    {
      g_data->num_aovs_used = 0;
      if ((stl->g_data->render_passes & EEVEE_RENDER_PASS_AOV) != 0) {
        EEVEE_RenderPassData data = {true, true, true, true, true, false, false, true, 0};
        if (stl->g_data->aov_hash == EEVEE_AOV_HASH_ALL) {
          ViewLayer *view_layer = draw_ctx->view_layer;
          int aov_index = 0;
          LISTBASE_FOREACH (ViewLayerAOV *, aov, &view_layer->aovs) {
            if ((aov->flag & AOV_CONFLICT) != 0) {
              continue;
            }
            if (aov_index == MAX_AOVS) {
              break;
            }
            data.renderPassAOVActive = EEVEE_renderpasses_aov_hash(aov);
            if (sldata->renderpass_ubo.aovs[aov_index]) {
              GPU_uniformbuf_update(sldata->renderpass_ubo.aovs[aov_index], &data);
            }
            else {
              sldata->renderpass_ubo.aovs[aov_index] = GPU_uniformbuf_create_ex(
                  sizeof(data), &data, "renderpass_ubo.aovs");
            }
            aov_index++;
          }
          g_data->num_aovs_used = aov_index;
        }
        else {
          /* Rendering a single AOV in the 3d viewport */
          data.renderPassAOVActive = stl->g_data->aov_hash;
          if (sldata->renderpass_ubo.aovs[0]) {
            GPU_uniformbuf_update(sldata->renderpass_ubo.aovs[0], &data);
          }
          else {
            sldata->renderpass_ubo.aovs[0] = GPU_uniformbuf_create_ex(
                sizeof(data), &data, "renderpass_ubo.aovs");
          }
          g_data->num_aovs_used = 1;
        }
      }
      /* Free AOV UBO's that are not in use. */
      for (int aov_index = g_data->num_aovs_used; aov_index < MAX_AOVS; aov_index++) {
        DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.aovs[aov_index]);
      }
    }

    /* HACK: EEVEE_material_get can create a new context. This can only be
     * done when there is no active framebuffer. We do this here otherwise
     * `EEVEE_renderpasses_output_init` will fail. It cannot be done in
     * `EEVEE_renderpasses_init` as the `e_data.vertcode` can be uninitialized.
     */
    if (g_data->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
      Scene *scene = draw_ctx->scene;
      World *wo = scene->world;
      if (wo && wo->use_nodes) {
        EEVEE_material_get(vedata, scene, nullptr, wo, VAR_WORLD_BACKGROUND);
      }
    }
  }
}

void EEVEE_materials_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  /* Create Material #GHash. */
  {
    stl->g_data->material_hash = BLI_ghash_ptr_new("Eevee_material ghash");

    if (sldata->material_cache == nullptr) {
      sldata->material_cache = BLI_memblock_create(sizeof(EeveeMaterialCache));
    }
    else {
      BLI_memblock_clear(sldata->material_cache, nullptr);
    }
  }

  {
    DRW_PASS_CREATE(psl->background_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

    DRWShadingGroup *grp = nullptr;
    EEVEE_lookdev_cache_init(vedata, sldata, psl->background_ps, nullptr, &grp);

    if (grp == nullptr) {
      Scene *scene = draw_ctx->scene;
      World *world = (scene->world) ? scene->world : EEVEE_world_default_get();

      const int options = VAR_WORLD_BACKGROUND;
      GPUMaterial *gpumat = EEVEE_material_get(vedata, scene, nullptr, world, options);

      grp = DRW_shgroup_material_create(gpumat, psl->background_ps);
      DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
    }

    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
    DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
    DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
    DRW_shgroup_uniform_block_ref(grp, "renderpass_block", &stl->g_data->renderpass_ubo);
    DRW_shgroup_uniform_texture(grp, "utilTex", e_data.util_tex);
    DRW_shgroup_uniform_texture_ref(grp, "shadowCubeTexture", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref(grp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);
    DRW_shgroup_uniform_texture_ref(grp, "probePlanars", &vedata->txl->planar_pool);
    DRW_shgroup_uniform_texture_ref(grp, "probeCubes", &stl->g_data->light_cache->cube_tx.tex);
    DRW_shgroup_uniform_texture_ref(grp, "irradianceGrid", &stl->g_data->light_cache->grid_tx.tex);
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &vedata->txl->maxzbuffer);
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), nullptr);
  }

#define EEVEE_PASS_CREATE(pass, state) \
  do { \
    DRW_PASS_CREATE(psl->pass##_ps, state); \
    DRW_PASS_CREATE(psl->pass##_cull_ps, state | DRW_STATE_CULL_BACK); \
    DRW_pass_link(psl->pass##_ps, psl->pass##_cull_ps); \
  } while (0)

#define EEVEE_CLIP_PASS_CREATE(pass, state) \
  do { \
    DRWState st = state | DRW_STATE_CLIP_PLANES; \
    DRW_PASS_INSTANCE_CREATE(psl->pass##_clip_ps, psl->pass##_ps, st); \
    DRW_PASS_INSTANCE_CREATE( \
        psl->pass##_clip_cull_ps, psl->pass##_cull_ps, st | DRW_STATE_CULL_BACK); \
    DRW_pass_link(psl->pass##_clip_ps, psl->pass##_clip_cull_ps); \
  } while (0)

  {
    DRWState state_depth = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRWState state_shading = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES;
    DRWState state_sss = DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS;

    EEVEE_PASS_CREATE(depth, state_depth);
    EEVEE_CLIP_PASS_CREATE(depth, state_depth);

    EEVEE_PASS_CREATE(depth_refract, state_depth);
    EEVEE_CLIP_PASS_CREATE(depth_refract, state_depth);

    EEVEE_PASS_CREATE(material, state_shading);
    EEVEE_PASS_CREATE(material_refract, state_shading);
    EEVEE_PASS_CREATE(material_sss, state_shading | state_sss);
  }
  {
    /* Renderpass accumulation. */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ADD_FULL;
    /* Create an instance of each of these passes and link them together. */
    DRWPass *passes[] = {
        psl->material_ps,
        psl->material_cull_ps,
        psl->material_sss_ps,
        psl->material_sss_cull_ps,
    };
    DRWPass *first = nullptr, *last = nullptr;
    for (int i = 0; i < ARRAY_SIZE(passes); i++) {
      DRWPass *pass = DRW_pass_create_instance("Renderpass Accumulation", passes[i], state);
      if (first == nullptr) {
        first = last = pass;
      }
      else {
        DRW_pass_link(last, pass);
        last = pass;
      }
    }
    psl->material_accum_ps = first;

    /* Same for background */
    DRW_PASS_INSTANCE_CREATE(psl->background_accum_ps, psl->background_ps, state);
  }
  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES;
    DRW_PASS_CREATE(psl->transparent_pass, state);
  }
  {
    DRW_PASS_CREATE(psl->update_noise_pass, DRW_STATE_WRITE_COLOR);
    GPUShader *sh = EEVEE_shaders_update_noise_sh_get();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->update_noise_pass);
    DRW_shgroup_uniform_texture(grp, "blueNoise", e_data.noise_tex);
    DRW_shgroup_uniform_vec3(grp, "offsets", e_data.noise_offsets, 1);
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), nullptr);
  }
}

BLI_INLINE void material_shadow(EEVEE_Data *vedata,
                                EEVEE_ViewLayerData *sldata,
                                Material *ma,
                                bool is_hair,
                                EeveeMaterialCache *emc)
{
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_PassList *psl = vedata->psl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  if (ma->blend_shadow != MA_BS_NONE) {
    /* Shadow Pass */
    const bool use_shadow_shader = ma->use_nodes && ma->nodetree &&
                                   ELEM(ma->blend_shadow, MA_BS_CLIP, MA_BS_HASHED);
    float alpha_clip_threshold = (ma->blend_shadow == MA_BS_CLIP) ? ma->alpha_threshold : -1.0f;

    int mat_options = VAR_MAT_MESH | VAR_MAT_DEPTH;
    SET_FLAG_FROM_TEST(mat_options, use_shadow_shader, VAR_MAT_HASH);
    SET_FLAG_FROM_TEST(mat_options, is_hair, VAR_MAT_HAIR);
    GPUMaterial *gpumat = (use_shadow_shader) ?
                              EEVEE_material_get(vedata, scene, ma, nullptr, mat_options) :
                              EEVEE_material_default_get(scene, ma, mat_options);

    /* Avoid possible confusion with depth pre-pass options. */
    int option = KEY_SHADOW;
    SET_FLAG_FROM_TEST(option, is_hair, KEY_HAIR);

    /* Search for the same shaders usage in the pass. */
    GPUShader *sh = GPU_material_get_shader(gpumat);
    void *cache_key = (char *)sh + option;
    DRWShadingGroup *grp, **grp_p;

    if (BLI_ghash_ensure_p(pd->material_hash, cache_key, (void ***)&grp_p)) {
      /* This GPUShader has already been used by another material.
       * Add new shading group just after to avoid shader switching cost. */
      grp = DRW_shgroup_create_sub(*grp_p);
      /* Per material uniforms. */
      DRW_shgroup_uniform_float_copy(grp, "alphaClipThreshold", alpha_clip_threshold);
    }
    else {
      *grp_p = grp = DRW_shgroup_create(sh, psl->shadow_pass);
      EEVEE_material_bind_resources(
          grp, gpumat, sldata, vedata, nullptr, nullptr, alpha_clip_threshold, false, false);
    }

    DRW_shgroup_add_material_resources(grp, gpumat);

    emc->shadow_grp = grp;
    emc->shadow_grp_p = grp_p;
  }
  else {
    emc->shadow_grp = nullptr;
    emc->shadow_grp_p = nullptr;
  }
}

static EeveeMaterialCache material_opaque(EEVEE_Data *vedata,
                                          EEVEE_ViewLayerData *sldata,
                                          Material *ma,
                                          const bool is_hair)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_PassList *psl = vedata->psl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  const bool do_cull = !is_hair && (ma->blend_flag & MA_BL_CULL_BACKFACE) != 0;
  const bool use_gpumat = (ma->use_nodes && ma->nodetree);
  const bool use_ssrefract = use_gpumat && ((ma->blend_flag & MA_BL_SS_REFRACTION) != 0) &&
                             ((effects->enabled_effects & EFFECT_REFRACT) != 0);
  const bool use_depth_shader = use_gpumat && ELEM(ma->blend_method, MA_BM_CLIP, MA_BM_HASHED);
  float alpha_clip_threshold = (ma->blend_method == MA_BM_CLIP) ? ma->alpha_threshold : -1.0f;

  /* HACK: Assume the struct will never be smaller than our variations.
   * This allow us to only keep one ghash and avoid bigger keys comparisons/hashing. */
  void *key = (char *)ma + is_hair;
  /* Search for other material instances (sharing the same Material data-block). */
  EeveeMaterialCache **emc_p, *emc;
  if (BLI_ghash_ensure_p(pd->material_hash, key, (void ***)&emc_p)) {
    return **emc_p;
  }

  *emc_p = emc = static_cast<EeveeMaterialCache *>(BLI_memblock_alloc(sldata->material_cache));

  material_shadow(vedata, sldata, ma, is_hair, emc);

  {
    /* Depth Pass */
    int mat_options = VAR_MAT_MESH | VAR_MAT_DEPTH;
    SET_FLAG_FROM_TEST(mat_options, use_ssrefract, VAR_MAT_REFRACT);
    SET_FLAG_FROM_TEST(mat_options, use_depth_shader, VAR_MAT_HASH);
    SET_FLAG_FROM_TEST(mat_options, is_hair, VAR_MAT_HAIR);
    GPUMaterial *gpumat = (use_depth_shader) ?
                              EEVEE_material_get(vedata, scene, ma, nullptr, mat_options) :
                              EEVEE_material_default_get(scene, ma, mat_options);

    int option = 0;
    SET_FLAG_FROM_TEST(option, do_cull, KEY_CULL);
    SET_FLAG_FROM_TEST(option, use_ssrefract, KEY_REFRACT);
    DRWPass *depth_ps = std::array{
        psl->depth_ps,
        psl->depth_cull_ps,
        psl->depth_refract_ps,
        psl->depth_refract_cull_ps,
    }[option];
    /* Hair are rendered inside the non-cull pass but needs to have a separate cache key. */
    SET_FLAG_FROM_TEST(option, is_hair, KEY_HAIR);

    /* Search for the same shaders usage in the pass. */
    GPUShader *sh = GPU_material_get_shader(gpumat);
    void *cache_key = (char *)sh + option;
    DRWShadingGroup *grp, **grp_p;

    if (BLI_ghash_ensure_p(pd->material_hash, cache_key, (void ***)&grp_p)) {
      /* This GPUShader has already been used by another material.
       * Add new shading group just after to avoid shader switching cost. */
      grp = DRW_shgroup_create_sub(*grp_p);
      /* Per material uniforms. */
      DRW_shgroup_uniform_float_copy(grp, "alphaClipThreshold", alpha_clip_threshold);
    }
    else {
      *grp_p = grp = DRW_shgroup_create(sh, depth_ps);
      EEVEE_material_bind_resources(
          grp, gpumat, sldata, vedata, nullptr, nullptr, alpha_clip_threshold, false, false);
    }

    DRW_shgroup_add_material_resources(grp, gpumat);

    emc->depth_grp = grp;
    emc->depth_grp_p = grp_p;
  }
  {
    /* Shading Pass */
    int mat_options = VAR_MAT_MESH;
    SET_FLAG_FROM_TEST(mat_options, use_ssrefract, VAR_MAT_REFRACT);
    SET_FLAG_FROM_TEST(mat_options, is_hair, VAR_MAT_HAIR);
    GPUMaterial *gpumat = EEVEE_material_get(vedata, scene, ma, nullptr, mat_options);
    const bool use_sss = GPU_material_flag_get(gpumat, GPU_MATFLAG_SUBSURFACE);

    int ssr_id = (((effects->enabled_effects & EFFECT_SSR) != 0) && !use_ssrefract) ? 1 : 0;
    int option = (use_ssrefract ? 0 : (use_sss ? 1 : 2)) * 2 + do_cull;
    DRWPass *shading_pass = std::array{
        psl->material_refract_ps,
        psl->material_refract_cull_ps,
        psl->material_sss_ps,
        psl->material_sss_cull_ps,
        psl->material_ps,
        psl->material_cull_ps,
    }[option];
    /* Hair are rendered inside the non-cull pass but needs to have a separate cache key */
    option = option * 2 + is_hair;

    /* Search for the same shaders usage in the pass. */
    /* HACK: Assume the struct will never be smaller than our variations.
     * This allow us to only keep one ghash and avoid bigger keys comparisons/hashing. */
    BLI_assert(option <= 16);
    GPUShader *sh = GPU_material_get_shader(gpumat);
    void *cache_key = (char *)sh + option;
    DRWShadingGroup *grp, **grp_p;

    if (BLI_ghash_ensure_p(pd->material_hash, cache_key, (void ***)&grp_p)) {
      /* This GPUShader has already been used by another material.
       * Add new shading group just after to avoid shader switching cost. */
      grp = DRW_shgroup_create_sub(*grp_p);

      /* Per material uniforms. */
      DRW_shgroup_uniform_float_copy(grp, "alphaClipThreshold", alpha_clip_threshold);
      if (use_ssrefract) {
        DRW_shgroup_uniform_float_copy(grp, "refractionDepth", ma->refract_depth);
      }
    }
    else {
      *grp_p = grp = DRW_shgroup_create(sh, shading_pass);
      EEVEE_material_bind_resources(grp,
                                    gpumat,
                                    sldata,
                                    vedata,
                                    &ssr_id,
                                    &ma->refract_depth,
                                    alpha_clip_threshold,
                                    use_ssrefract,
                                    false);
    }
    DRW_shgroup_add_material_resources(grp, gpumat);

    if (use_sss) {
      EEVEE_subsurface_add_pass(sldata, vedata, ma, grp, gpumat);
    }

    emc->shading_grp = grp;
    emc->shading_grp_p = grp_p;
    emc->shading_gpumat = gpumat;
  }
  return *emc;
}

static EeveeMaterialCache material_transparent(EEVEE_Data *vedata,
                                               EEVEE_ViewLayerData *sldata,
                                               Material *ma)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EeveeMaterialCache emc = {nullptr};

  const bool do_cull = (ma->blend_flag & MA_BL_CULL_BACKFACE) != 0;
  const bool use_gpumat = ma->use_nodes && ma->nodetree;
  const bool use_ssrefract = use_gpumat && ((ma->blend_flag & MA_BL_SS_REFRACTION) != 0) &&
                             ((effects->enabled_effects & EFFECT_REFRACT) != 0);
  const bool use_prepass = ((ma->blend_flag & MA_BL_HIDE_BACKFACE) != 0);

  DRWState cur_state;
  DRWState all_state = (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_CULL_BACK |
                        DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_DEPTH_EQUAL |
                        DRW_STATE_BLEND_CUSTOM);

  material_shadow(vedata, sldata, ma, false, &emc);

  if (use_prepass) {
    /* Depth prepass */
    int mat_options = VAR_MAT_MESH | VAR_MAT_DEPTH;
    GPUMaterial *gpumat = EEVEE_material_get(vedata, scene, ma, nullptr, mat_options);
    GPUShader *sh = GPU_material_get_shader(gpumat);

    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->transparent_pass);

    EEVEE_material_bind_resources(
        grp, gpumat, sldata, vedata, nullptr, nullptr, -1.0f, false, true);
    DRW_shgroup_add_material_resources(grp, gpumat);

    cur_state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : DRWState(0);

    DRW_shgroup_state_disable(grp, all_state);
    DRW_shgroup_state_enable(grp, cur_state);

    emc.depth_grp = grp;
  }
  {
    /* Shading */
    int ssr_id = -1; /* TODO: transparent SSR. */
    int mat_options = VAR_MAT_MESH | VAR_MAT_BLEND;
    SET_FLAG_FROM_TEST(mat_options, use_ssrefract, VAR_MAT_REFRACT);
    GPUMaterial *gpumat = EEVEE_material_get(vedata, scene, ma, nullptr, mat_options);

    DRWShadingGroup *grp = DRW_shgroup_create(GPU_material_get_shader(gpumat),
                                              psl->transparent_pass);

    EEVEE_material_bind_resources(
        grp, gpumat, sldata, vedata, &ssr_id, &ma->refract_depth, -1.0f, use_ssrefract, true);
    DRW_shgroup_add_material_resources(grp, gpumat);

    cur_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM;
    cur_state |= (use_prepass) ? DRW_STATE_DEPTH_EQUAL : DRW_STATE_DEPTH_LESS_EQUAL;
    cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : DRWState(0);

    /* Disable other blend modes and use the one we want. */
    DRW_shgroup_state_disable(grp, all_state);
    DRW_shgroup_state_enable(grp, cur_state);

    emc.shading_grp = grp;
    emc.shading_gpumat = gpumat;
  }
  return emc;
}

/* Return correct material or empty default material if slot is empty. */
BLI_INLINE Material *eevee_object_material_get(Object *ob, int slot, bool holdout)
{
  if (holdout) {
    return BKE_material_default_holdout();
  }
  Material *ma = BKE_object_material_get_eval(ob, slot + 1);
  if (ma == nullptr) {
    if (ob->type == OB_VOLUME) {
      ma = BKE_material_default_volume();
    }
    else {
      ma = BKE_material_default_surface();
    }
  }
  return ma;
}

BLI_INLINE EeveeMaterialCache eevee_material_cache_get(
    EEVEE_Data *vedata, EEVEE_ViewLayerData *sldata, Object *ob, int slot, bool is_hair)
{
  const bool holdout = ((ob->base_flag & BASE_HOLDOUT) != 0) ||
                       ((ob->visibility_flag & OB_HOLDOUT) != 0);
  EeveeMaterialCache matcache;
  Material *ma = eevee_object_material_get(ob, slot, holdout);
  switch (ma->blend_method) {
    case MA_BM_BLEND:
      if (!is_hair) {
        matcache = material_transparent(vedata, sldata, ma);
        break;
      }
      ATTR_FALLTHROUGH;
    case MA_BM_SOLID:
    case MA_BM_CLIP:
    case MA_BM_HASHED:
    default:
      matcache = material_opaque(vedata, sldata, ma, is_hair);
      break;
  }
  return matcache;
}

#define ADD_SHGROUP_CALL(shgrp, ob, geom, oedata) \
  do { \
    if (oedata) { \
      DRW_shgroup_call_with_callback(shgrp, geom, ob, oedata); \
    } \
    else { \
      DRW_shgroup_call(shgrp, geom, ob); \
    } \
  } while (0)

#define ADD_SHGROUP_CALL_SAFE(shgrp, ob, geom, oedata) \
  do { \
    if (shgrp) { \
      ADD_SHGROUP_CALL(shgrp, ob, geom, oedata); \
    } \
  } while (0)

#define MATCACHE_AS_ARRAY(matcache, member, materials_len, output_array) \
  for (int i = 0; i < materials_len; i++) { \
    output_array[i] = matcache[i].member; \
  }

void EEVEE_materials_cache_populate(EEVEE_Data *vedata,
                                    EEVEE_ViewLayerData *sldata,
                                    Object *ob,
                                    bool *cast_shadow)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d) &&
                         !DRW_state_is_image_render();

  if (ob->sculpt && BKE_object_sculpt_pbvh_get(ob)) {
    BKE_pbvh_is_drawing_set(BKE_object_sculpt_pbvh_get(ob), use_sculpt_pbvh);
  }

  /* First get materials for this mesh. */
  if (ELEM(ob->type, OB_MESH, OB_SURF)) {
    const int materials_len = DRW_cache_object_material_count_get(ob);

    EeveeMaterialCache *matcache = BLI_array_alloca(matcache, materials_len);
    for (int i = 0; i < materials_len; i++) {
      matcache[i] = eevee_material_cache_get(vedata, sldata, ob, i, false);
    }

    /* Only support single volume material for now. */
    /* XXX We rely on the previously compiled surface shader
     * to know if the material has a "volume nodetree".
     */
    bool use_volume_material = (matcache[0].shading_gpumat &&
                                GPU_material_has_volume_output(matcache[0].shading_gpumat));
    if ((ob->dt >= OB_SOLID) || DRW_state_is_scene_render()) {
      if (use_sculpt_pbvh) {
        DRWShadingGroup **shgrps_array = BLI_array_alloca(shgrps_array, materials_len);

        GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
        MATCACHE_AS_ARRAY(matcache, shading_gpumat, materials_len, gpumat_array);

        MATCACHE_AS_ARRAY(matcache, shading_grp, materials_len, shgrps_array);
        DRW_shgroup_call_sculpt_with_materials(shgrps_array, gpumat_array, materials_len, ob);

        MATCACHE_AS_ARRAY(matcache, depth_grp, materials_len, shgrps_array);
        DRW_shgroup_call_sculpt_with_materials(shgrps_array, gpumat_array, materials_len, ob);

        MATCACHE_AS_ARRAY(matcache, shadow_grp, materials_len, shgrps_array);
        DRW_shgroup_call_sculpt_with_materials(shgrps_array, gpumat_array, materials_len, ob);

        *cast_shadow = true;
      }
      else {
        GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
        MATCACHE_AS_ARRAY(matcache, shading_gpumat, materials_len, gpumat_array);
        /* Get per-material split surface */
        blender::gpu::Batch **mat_geom = DRW_cache_object_surface_material_get(
            ob, gpumat_array, materials_len);

        if (mat_geom) {
          for (int i = 0; i < materials_len; i++) {
            if (mat_geom[i] == nullptr) {
              continue;
            }

            /* Do not render surface if we are rendering a volume object
             * and do not have a surface closure. */
            if (use_volume_material &&
                (gpumat_array[i] && !GPU_material_has_surface_output(gpumat_array[i])))
            {
              continue;
            }

            /* XXX TODO: rewrite this to include the dupli objects.
             * This means we cannot exclude dupli objects from reflections!!! */
            EEVEE_ObjectEngineData *oedata = nullptr;
            if ((ob->base_flag & BASE_FROM_DUPLI) == 0) {
              oedata = EEVEE_object_data_ensure(ob);
              oedata->ob = ob;
              oedata->test_data = &sldata->probes->vis_data;
            }

            ADD_SHGROUP_CALL(matcache[i].shading_grp, ob, mat_geom[i], oedata);
            ADD_SHGROUP_CALL_SAFE(matcache[i].depth_grp, ob, mat_geom[i], oedata);
            ADD_SHGROUP_CALL_SAFE(matcache[i].shadow_grp, ob, mat_geom[i], oedata);
            *cast_shadow = *cast_shadow || (matcache[i].shadow_grp != nullptr);
          }
        }

        if (G.debug_value == 889 && ob->sculpt && BKE_object_sculpt_pbvh_get(ob)) {
          int debug_node_nr = 0;
          DRW_debug_modelmat(ob->object_to_world().ptr());
          BKE_pbvh_draw_debug_cb(
              BKE_object_sculpt_pbvh_get(ob), DRW_sculpt_debug_cb, &debug_node_nr);
        }
      }

      /* Motion Blur Vectors. */
      EEVEE_motion_blur_cache_populate(sldata, vedata, ob);
    }

    /* Volumetrics */
    if (use_volume_material) {
      EEVEE_volumes_cache_object_add(sldata, vedata, scene, ob);
    }
  }
}

void EEVEE_particle_hair_cache_populate(EEVEE_Data *vedata,
                                        EEVEE_ViewLayerData *sldata,
                                        Object *ob,
                                        bool *cast_shadow)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (ob->type == OB_MESH) {
    if (ob != draw_ctx->object_edit) {
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
        if (draw_as != PART_DRAW_PATH) {
          continue;
        }
        EeveeMaterialCache matcache = eevee_material_cache_get(
            vedata, sldata, ob, part->omat - 1, true);

        if (matcache.depth_grp) {
          *matcache.depth_grp_p = DRW_shgroup_hair_create_sub(
              ob, psys, md, matcache.depth_grp, nullptr);
        }
        if (matcache.shading_grp) {
          *matcache.shading_grp_p = DRW_shgroup_hair_create_sub(
              ob, psys, md, matcache.shading_grp, matcache.shading_gpumat);
        }
        if (matcache.shadow_grp) {
          *matcache.shadow_grp_p = DRW_shgroup_hair_create_sub(
              ob, psys, md, matcache.shadow_grp, nullptr);
          *cast_shadow = true;
        }

        EEVEE_motion_blur_hair_cache_populate(sldata, vedata, ob, psys, md);
      }
    }
  }
}

void EEVEE_object_curves_cache_populate(EEVEE_Data *vedata,
                                        EEVEE_ViewLayerData *sldata,
                                        Object *ob,
                                        bool *cast_shadow)
{
  using namespace blender::draw;
  EeveeMaterialCache matcache = eevee_material_cache_get(
      vedata, sldata, ob, CURVES_MATERIAL_NR - 1, true);

  if (matcache.depth_grp) {
    *matcache.depth_grp_p = DRW_shgroup_curves_create_sub(ob, matcache.depth_grp, nullptr);
  }
  if (matcache.shading_grp) {
    *matcache.shading_grp_p = DRW_shgroup_curves_create_sub(
        ob, matcache.shading_grp, matcache.shading_gpumat);
  }
  if (matcache.shadow_grp) {
    *matcache.shadow_grp_p = DRW_shgroup_curves_create_sub(ob, matcache.shadow_grp, nullptr);
    *cast_shadow = true;
  }

  EEVEE_motion_blur_curves_cache_populate(sldata, vedata, ob);
}

void EEVEE_materials_cache_finish(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;

  BLI_ghash_free(pd->material_hash, nullptr, nullptr);
  pd->material_hash = nullptr;

  SET_FLAG_FROM_TEST(effects->enabled_effects, effects->sss_surface_count > 0, EFFECT_SSS);
}

void EEVEE_materials_free()
{
  DRW_TEXTURE_FREE_SAFE(e_data.util_tex);
  DRW_TEXTURE_FREE_SAFE(e_data.noise_tex);
}

/* -------------------------------------------------------------------- */
/** \name Render Passes
 * \{ */

void EEVEE_material_renderpasses_init(EEVEE_Data *vedata)
{
  EEVEE_PrivateData *pd = vedata->stl->g_data;

  /* For diffuse and glossy we calculate the final light + color buffer where we extract the
   * light from by dividing by the color buffer. When one the light is requested we also tag
   * the color buffer to do the extraction. */
  if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT) {
    pd->render_passes |= EEVEE_RENDER_PASS_DIFFUSE_COLOR;
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) {
    pd->render_passes |= EEVEE_RENDER_PASS_SPECULAR_COLOR;
  }
}

static void material_renderpass_init(GPUTexture **output_tx, const eGPUTextureFormat format)
{
  DRW_texture_ensure_fullscreen_2d(output_tx, format, DRWTextureFlag(0));
}

void EEVEE_material_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_PrivateData *pd = stl->g_data;

  /* Should be enough precision for many samples. */
  const eGPUTextureFormat texture_format = (tot_samples > 128) ? GPU_RGBA32F : GPU_RGBA16F;

  /* Create FrameBuffer. */
  GPU_framebuffer_ensure_config(&fbl->material_accum_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_LEAVE});

  if (pd->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
    material_renderpass_init(&txl->env_accum, texture_format);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_EMIT) {
    material_renderpass_init(&txl->emit_accum, texture_format);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_COLOR) {
    material_renderpass_init(&txl->diff_color_accum, texture_format);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT) {
    material_renderpass_init(&txl->diff_light_accum, texture_format);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_COLOR) {
    material_renderpass_init(&txl->spec_color_accum, texture_format);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_AOV) {
    for (int aov_index = 0; aov_index < pd->num_aovs_used; aov_index++) {
      material_renderpass_init(&txl->aov_surface_accum[aov_index], texture_format);
    }
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) {
    material_renderpass_init(&txl->spec_light_accum, texture_format);

    if (effects->enabled_effects & EFFECT_SSR) {
      EEVEE_reflection_output_init(sldata, vedata, tot_samples);
    }
  }
}

static void material_renderpass_accumulate(EEVEE_EffectsInfo *effects,
                                           EEVEE_FramebufferList *fbl,
                                           DRWPass *renderpass,
                                           DRWPass *renderpass2,
                                           EEVEE_PrivateData *pd,
                                           GPUTexture *output_tx,
                                           GPUUniformBuf *renderpass_option_ubo)
{
  GPU_framebuffer_texture_attach(fbl->material_accum_fb, output_tx, 0, 0);
  GPU_framebuffer_bind(fbl->material_accum_fb);

  if (effects->taa_current_sample == 1) {
    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_framebuffer_clear_color(fbl->material_accum_fb, clear);
  }

  pd->renderpass_ubo = renderpass_option_ubo;
  DRW_draw_pass(renderpass);
  if (renderpass2) {
    DRW_draw_pass(renderpass2);
  }

  GPU_framebuffer_texture_detach(fbl->material_accum_fb, output_tx);
}

void EEVEE_material_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_TextureList *txl = vedata->txl;

  if (fbl->material_accum_fb != nullptr) {
    DRWPass *material_accum_ps = psl->material_accum_ps;
    DRWPass *background_accum_ps = psl->background_accum_ps;
    if (pd->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
      material_renderpass_accumulate(effects,
                                     fbl,
                                     background_accum_ps,
                                     nullptr,
                                     pd,
                                     txl->env_accum,
                                     sldata->renderpass_ubo.environment);
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_EMIT) {
      material_renderpass_accumulate(effects,
                                     fbl,
                                     material_accum_ps,
                                     nullptr,
                                     pd,
                                     txl->emit_accum,
                                     sldata->renderpass_ubo.emit);
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_COLOR) {
      material_renderpass_accumulate(effects,
                                     fbl,
                                     material_accum_ps,
                                     nullptr,
                                     pd,
                                     txl->diff_color_accum,
                                     sldata->renderpass_ubo.diff_color);
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT) {
      material_renderpass_accumulate(effects,
                                     fbl,
                                     material_accum_ps,
                                     nullptr,
                                     pd,
                                     txl->diff_light_accum,
                                     sldata->renderpass_ubo.diff_light);

      if (effects->enabled_effects & EFFECT_SSS) {
        EEVEE_subsurface_output_accumulate(sldata, vedata);
      }
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_COLOR) {
      bool prev_ssr = sldata->common_data.ssr_toggle;
      if (prev_ssr) {
        /* We need to disable ssr here so output radiance is not directed to the ssr buffer. */
        sldata->common_data.ssr_toggle = false;
        GPU_uniformbuf_update(sldata->common_ubo, &sldata->common_data);
      }
      material_renderpass_accumulate(effects,
                                     fbl,
                                     material_accum_ps,
                                     nullptr,
                                     pd,
                                     txl->spec_color_accum,
                                     sldata->renderpass_ubo.spec_color);
      if (prev_ssr) {
        sldata->common_data.ssr_toggle = prev_ssr;
        GPU_uniformbuf_update(sldata->common_ubo, &sldata->common_data);
      }
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) {
      material_renderpass_accumulate(effects,
                                     fbl,
                                     material_accum_ps,
                                     nullptr,
                                     pd,
                                     txl->spec_light_accum,
                                     sldata->renderpass_ubo.spec_light);

      if (effects->enabled_effects & EFFECT_SSR) {
        EEVEE_reflection_output_accumulate(sldata, vedata);
      }
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_AOV) {
      for (int aov_index = 0; aov_index < pd->num_aovs_used; aov_index++) {
        material_renderpass_accumulate(effects,
                                       fbl,
                                       material_accum_ps,
                                       background_accum_ps,
                                       pd,
                                       txl->aov_surface_accum[aov_index],
                                       sldata->renderpass_ubo.aovs[aov_index]);
      }
    }
    /* Free unused aov textures. */
    for (int aov_index = pd->num_aovs_used; aov_index < MAX_AOVS; aov_index++) {
      DRW_TEXTURE_FREE_SAFE(txl->aov_surface_accum[aov_index]);
    }

    /* Restore default. */
    pd->renderpass_ubo = sldata->renderpass_ubo.combined;
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_material_transparent_output_init(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_TextureList *txl = vedata->txl;

  if (pd->render_passes & EEVEE_RENDER_PASS_TRANSPARENT) {
    /* Intermediate result to blend objects on. */
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    DRW_texture_ensure_fullscreen_2d_ex(
        &txl->transparent_depth_tmp, GPU_DEPTH24_STENCIL8, usage, DRWTextureFlag(0));
    DRW_texture_ensure_fullscreen_2d_ex(
        &txl->transparent_color_tmp, GPU_RGBA16F, usage, DRWTextureFlag(0));
    GPU_framebuffer_ensure_config(&fbl->transparent_rpass_fb,
                                  {GPU_ATTACHMENT_TEXTURE(txl->transparent_depth_tmp),
                                   GPU_ATTACHMENT_TEXTURE(txl->transparent_color_tmp)});
    /* Final result to with AntiAliasing. */
    /* TODO mem usage. */
    const eGPUTextureFormat texture_format = (true) ? GPU_RGBA32F : GPU_RGBA16F;
    eGPUTextureUsage usage_accum = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_HOST_READ |
                                   GPU_TEXTURE_USAGE_ATTACHMENT;
    DRW_texture_ensure_fullscreen_2d_ex(
        &txl->transparent_accum, texture_format, usage_accum, DRWTextureFlag(0));
    GPU_framebuffer_ensure_config(
        &fbl->transparent_rpass_accum_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->transparent_accum)});

    {
      /* This pass Accumulate 1 sample of the transparent pass into the transparent
       * accumulation buffer. */
      DRW_PASS_CREATE(psl->transparent_accum_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
      DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_renderpasses_accumulate_sh_get(),
                                                psl->transparent_accum_ps);
      DRW_shgroup_uniform_texture(grp, "inputBuffer", txl->transparent_color_tmp);
      DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), nullptr);
    }
  }
}

void EEVEE_material_transparent_output_accumulate(EEVEE_Data *vedata)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_TextureList *txl = vedata->txl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (pd->render_passes & EEVEE_RENDER_PASS_TRANSPARENT) {
    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    pd->renderpass_current_sample = effects->taa_current_sample;

    /* Work on a copy of the depth texture to allow re-rendering
     * the transparent object to the main pass. */
    GPU_texture_copy(txl->transparent_depth_tmp, dtxl->depth);

    /* Render transparent objects on a black background. */
    GPU_framebuffer_bind(fbl->transparent_rpass_fb);
    GPU_framebuffer_clear_color(fbl->transparent_rpass_fb, clear);
    DRW_draw_pass(psl->transparent_pass);

    /* Accumulate the resulting color buffer. */
    GPU_framebuffer_bind(fbl->transparent_rpass_accum_fb);
    if (effects->taa_current_sample == 1) {
      GPU_framebuffer_clear_color(fbl->transparent_rpass_accum_fb, clear);
    }
    DRW_draw_pass(psl->transparent_accum_ps);

    /* Restore default. */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

/** \} */
