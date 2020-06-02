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

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_memblock.h"
#include "BLI_rand.h"
#include "BLI_string_utils.h"

#include "BKE_paint.h"
#include "BKE_particle.h"

#include "DNA_hair_types.h"
#include "DNA_modifier_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "GPU_material.h"

#include "DEG_depsgraph_query.h"

#include "eevee_engine.h"
#include "eevee_lut.h"
#include "eevee_private.h"

/* *********** STATIC *********** */
static struct {
  /* 64*64 array texture containing all LUTs and other utilitarian arrays.
   * Packing enables us to same precious textures slots. */
  struct GPUTexture *util_tex;
  struct GPUTexture *noise_tex;

  float noise_offsets[3];
} e_data = {NULL}; /* Engine data */

extern char datatoc_lights_lib_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_prepass_frag_glsl[];
extern char datatoc_prepass_vert_glsl[];
extern char datatoc_default_frag_glsl[];
extern char datatoc_default_world_frag_glsl[];
extern char datatoc_ltc_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_hair_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_cubemap_lib_glsl[];
extern char datatoc_lit_surface_frag_glsl[];
extern char datatoc_lit_surface_vert_glsl[];
extern char datatoc_raytrace_lib_glsl[];
extern char datatoc_ssr_lib_glsl[];
extern char datatoc_shadow_vert_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_background_vert_glsl[];
extern char datatoc_update_noise_frag_glsl[];
extern char datatoc_volumetric_vert_glsl[];
extern char datatoc_volumetric_geom_glsl[];
extern char datatoc_volumetric_frag_glsl[];
extern char datatoc_volumetric_lib_glsl[];
extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

typedef struct EeveeMaterialCache {
  struct DRWShadingGroup *depth_grp;
  struct DRWShadingGroup *shading_grp;
  struct DRWShadingGroup *shadow_grp;
  struct GPUMaterial *shading_gpumat;
  /* Meh, Used by hair to ensure draw order when calling DRW_shgroup_create_sub.
   * Pointers to ghash values. */
  struct DRWShadingGroup **depth_grp_p;
  struct DRWShadingGroup **shading_grp_p;
  struct DRWShadingGroup **shadow_grp_p;
} EeveeMaterialCache;

/* *********** FUNCTIONS *********** */

/* XXX TODO define all shared resources in a shared place without duplication */
struct GPUTexture *EEVEE_materials_get_util_tex(void)
{
  return e_data.util_tex;
}

/**
 * ssr_id can be null to disable ssr contribution.
 */
void EEVEE_material_bind_resources(DRWShadingGroup *shgrp,
                                   GPUMaterial *gpumat,
                                   EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   int *ssr_id,
                                   float *refract_depth,
                                   bool use_ssrefraction,
                                   bool use_alpha_blend)
{
  bool use_diffuse = GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE);
  bool use_glossy = GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY);
  bool use_refract = GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT);

  LightCache *lcache = vedata->stl->g_data->light_cache;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_PrivateData *pd = vedata->stl->g_data;

  DRW_shgroup_uniform_block_persistent(shgrp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block_persistent(shgrp, "grid_block", sldata->grid_ubo);
  DRW_shgroup_uniform_block_persistent(shgrp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block_persistent(shgrp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block_persistent(shgrp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block_persistent(shgrp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block_ref_persistent(shgrp, "renderpass_block", &pd->renderpass_ubo);

  DRW_shgroup_uniform_int_copy(shgrp, "outputSssId", 1);
  DRW_shgroup_uniform_texture_persistent(shgrp, "utilTex", e_data.util_tex);
  if (use_diffuse || use_glossy || use_refract) {
    DRW_shgroup_uniform_texture_ref_persistent(
        shgrp, "shadowCubeTexture", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref_persistent(
        shgrp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);
    DRW_shgroup_uniform_texture_ref_persistent(shgrp, "maxzBuffer", &vedata->txl->maxzbuffer);
  }
  if ((use_diffuse || use_glossy) && !use_ssrefraction) {
    DRW_shgroup_uniform_texture_ref_persistent(shgrp, "horizonBuffer", &effects->gtao_horizons);
  }
  if (use_diffuse) {
    DRW_shgroup_uniform_texture_ref_persistent(shgrp, "irradianceGrid", &lcache->grid_tx.tex);
  }
  if (use_glossy || use_refract) {
    DRW_shgroup_uniform_texture_ref_persistent(shgrp, "probeCubes", &lcache->cube_tx.tex);
  }
  if (use_glossy) {
    DRW_shgroup_uniform_texture_ref_persistent(shgrp, "probePlanars", &vedata->txl->planar_pool);
    DRW_shgroup_uniform_int_copy(shgrp, "outputSsrId", ssr_id ? *ssr_id : 0);
  }
  if (use_refract) {
    DRW_shgroup_uniform_float_copy(
        shgrp, "refractionDepth", (refract_depth) ? *refract_depth : 0.0);
    if (use_ssrefraction) {
      DRW_shgroup_uniform_texture_ref_persistent(
          shgrp, "colorBuffer", &vedata->txl->refract_color);
    }
  }
  if (use_alpha_blend) {
    DRW_shgroup_uniform_texture_ref_persistent(shgrp, "inScattering", &effects->volume_scatter);
    DRW_shgroup_uniform_texture_ref_persistent(
        shgrp, "inTransmittance", &effects->volume_transmit);
  }
}

static void eevee_init_noise_texture(void)
{
  e_data.noise_tex = DRW_texture_create_2d(64, 64, GPU_RGBA16F, 0, (float *)blue_noise);
}

static void eevee_init_util_texture(void)
{
  const int layers = 4 + 16;
  float(*texels)[4] = MEM_mallocN(sizeof(float[4]) * 64 * 64 * layers, "utils texels");
  float(*texels_layer)[4] = texels;

  /* Copy ltc_mat_ggx into 1st layer */
  memcpy(texels_layer, ltc_mat_ggx, sizeof(float[4]) * 64 * 64);
  texels_layer += 64 * 64;

  /* Copy bsdf_split_sum_ggx into 2nd layer red and green channels.
   * Copy ltc_mag_ggx into 2nd layer blue and alpha channel. */
  for (int i = 0; i < 64 * 64; i++) {
    texels_layer[i][0] = bsdf_split_sum_ggx[i * 2 + 0];
    texels_layer[i][1] = bsdf_split_sum_ggx[i * 2 + 1];
    texels_layer[i][2] = ltc_mag_ggx[i * 2 + 0];
    texels_layer[i][3] = ltc_mag_ggx[i * 2 + 1];
  }
  texels_layer += 64 * 64;

  /* Copy blue noise in 3rd layer  */
  for (int i = 0; i < 64 * 64; i++) {
    texels_layer[i][0] = blue_noise[i][0];
    texels_layer[i][1] = blue_noise[i][2];
    texels_layer[i][2] = cosf(blue_noise[i][1] * 2.0f * M_PI);
    texels_layer[i][3] = sinf(blue_noise[i][1] * 2.0f * M_PI);
  }
  texels_layer += 64 * 64;

  /* Copy ltc_disk_integral in 4th layer  */
  for (int i = 0; i < 64 * 64; i++) {
    texels_layer[i][0] = ltc_disk_integral[i];
    texels_layer[i][1] = 0.0; /* UNUSED */
    texels_layer[i][2] = 0.0; /* UNUSED */
    texels_layer[i][3] = 0.0; /* UNUSED */
  }
  texels_layer += 64 * 64;

  /* Copy Refraction GGX LUT in layer 5 - 21 */
  for (int j = 0; j < 16; j++) {
    for (int i = 0; i < 64 * 64; i++) {
      texels_layer[i][0] = btdf_split_sum_ggx[j * 2][i];
      texels_layer[i][1] = 0.0; /* UNUSED */
      texels_layer[i][2] = 0.0; /* UNUSED */
      texels_layer[i][3] = 0.0; /* UNUSED */
    }
    texels_layer += 64 * 64;
  }

  e_data.util_tex = DRW_texture_create_2d_array(
      64, 64, layers, GPU_RGBA16F, DRW_TEX_FILTER | DRW_TEX_WRAP, (float *)texels);

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

void EEVEE_update_viewvecs(float invproj[4][4], float winmat[4][4], float (*r_viewvecs)[4])
{
  /* view vectors for the corners of the view frustum.
   * Can be used to recreate the world space position easily */
  float view_vecs[4][4] = {
      {-1.0f, -1.0f, -1.0f, 1.0f},
      {1.0f, -1.0f, -1.0f, 1.0f},
      {-1.0f, 1.0f, -1.0f, 1.0f},
      {-1.0f, -1.0f, 1.0f, 1.0f},
  };

  /* convert the view vectors to view space */
  const bool is_persp = (winmat[3][3] == 0.0f);
  for (int i = 0; i < 4; i++) {
    mul_project_m4_v3(invproj, view_vecs[i]);
    /* normalized trick see:
     * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
    if (is_persp) {
      /* Divide XY by Z. */
      mul_v2_fl(view_vecs[i], 1.0f / view_vecs[i][2]);
    }
  }

  /**
   * If ortho : view_vecs[0] is the near-bottom-left corner of the frustum and
   *            view_vecs[1] is the vector going from the near-bottom-left corner to
   *            the far-top-right corner.
   * If Persp : view_vecs[0].xy and view_vecs[1].xy are respectively the bottom-left corner
   *            when Z = 1, and top-left corner if Z = 1.
   *            view_vecs[0].z the near clip distance and view_vecs[1].z is the (signed)
   *            distance from the near plane to the far clip plane.
   */
  copy_v4_v4(r_viewvecs[0], view_vecs[0]);

  /* we need to store the differences */
  r_viewvecs[1][0] = view_vecs[1][0] - view_vecs[0][0];
  r_viewvecs[1][1] = view_vecs[2][1] - view_vecs[0][1];
  r_viewvecs[1][2] = view_vecs[3][2] - view_vecs[0][2];
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

  if (!DRW_state_is_image_render() && ((stl->effects->enabled_effects & EFFECT_TAA) == 0)) {
    sldata->common_data.alpha_hash_offset = 0.0f;
    sldata->common_data.alpha_hash_scale = 1.0f;
  }
  else {
    double r;
    BLI_halton_1d(5, 0.0, stl->effects->taa_current_sample - 1, &r);
    sldata->common_data.alpha_hash_offset = (float)r;
    sldata->common_data.alpha_hash_scale = 0.01f;
  }

  {
    /* Update view_vecs */
    float invproj[4][4], winmat[4][4];
    DRW_view_winmat_get(NULL, winmat, false);
    DRW_view_winmat_get(NULL, invproj, true);

    EEVEE_update_viewvecs(invproj, winmat, sldata->common_data.view_vecs);
  }

  {
    /* Update noise Framebuffer. */
    GPU_framebuffer_ensure_config(
        &fbl->update_noise_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE_LAYER(e_data.util_tex, 2)});
  }

  {
    /* Create RenderPass UBO */
    if (sldata->renderpass_ubo.combined == NULL) {
      sldata->renderpass_ubo.combined = DRW_uniformbuffer_create(
          sizeof(EEVEE_RenderPassData),
          &(const EEVEE_RenderPassData){true, true, true, true, true, false});

      sldata->renderpass_ubo.diff_color = DRW_uniformbuffer_create(
          sizeof(EEVEE_RenderPassData),
          &(const EEVEE_RenderPassData){true, false, false, false, false, true});

      sldata->renderpass_ubo.diff_light = DRW_uniformbuffer_create(
          sizeof(EEVEE_RenderPassData),
          &(const EEVEE_RenderPassData){true, true, false, false, false, false});

      sldata->renderpass_ubo.spec_color = DRW_uniformbuffer_create(
          sizeof(EEVEE_RenderPassData),
          &(const EEVEE_RenderPassData){false, false, true, false, false, false});

      sldata->renderpass_ubo.spec_light = DRW_uniformbuffer_create(
          sizeof(EEVEE_RenderPassData),
          &(const EEVEE_RenderPassData){false, false, true, true, false, false});

      sldata->renderpass_ubo.emit = DRW_uniformbuffer_create(
          sizeof(EEVEE_RenderPassData),
          &(const EEVEE_RenderPassData){false, false, false, false, true, false});
    }

    /* Used combined pass by default. */
    g_data->renderpass_ubo = sldata->renderpass_ubo.combined;

    /* HACK: EEVEE_material_get can create a new context. This can only be
     * done when there is no active framebuffer. We do this here otherwise
     * `EEVEE_renderpasses_output_init` will fail. It cannot be done in
     * `EEVEE_renderpasses_init` as the `e_data.vertcode` can be uninitialized.
     */
    if (g_data->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
      struct Scene *scene = draw_ctx->scene;
      struct World *wo = scene->world;
      if (wo && wo->use_nodes) {
        EEVEE_material_get(vedata, scene, NULL, wo, VAR_WORLD_BACKGROUND);
      }
    }
  }
}

void EEVEE_materials_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  /* Create Material Ghash */
  {
    stl->g_data->material_hash = BLI_ghash_ptr_new("Eevee_material ghash");

    if (sldata->material_cache == NULL) {
      sldata->material_cache = BLI_memblock_create(sizeof(EeveeMaterialCache));
    }
    else {
      BLI_memblock_clear(sldata->material_cache, NULL);
    }
  }

  {
    DRW_PASS_CREATE(psl->background_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRWShadingGroup *grp = NULL;

    Scene *scene = draw_ctx->scene;
    World *wo = scene->world;

    EEVEE_lookdev_cache_init(vedata, sldata, &grp, psl->background_ps, wo, NULL);

    if (!grp && wo) {
      struct GPUMaterial *gpumat = EEVEE_material_get(
          vedata, scene, NULL, wo, VAR_WORLD_BACKGROUND);

      grp = DRW_shgroup_material_create(gpumat, psl->background_ps);
      DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
      /* TODO (fclem): remove those (need to clean the GLSL files). */
      DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
      DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
      DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
      DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
      DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
      DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
      DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
      DRW_shgroup_call(grp, geom, NULL);
    }

    /* Fallback if shader fails or if not using nodetree. */
    if (grp == NULL) {
      GPUShader *sh = EEVEE_shaders_default_background_sh_get();
      grp = DRW_shgroup_create(sh, psl->background_ps);
      DRW_shgroup_uniform_vec3(grp, "color", G_draw.block.colorBackground, 1);
      DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
      DRW_shgroup_call(grp, geom, NULL);
    }
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
    /* Create an instance of each of theses passes and link them together. */
    DRWPass *passes[] = {
        psl->material_ps,
        psl->material_cull_ps,
        psl->material_sss_ps,
        psl->material_sss_cull_ps,
    };
    DRWPass *first = NULL, *last = NULL;
    for (int i = 0; i < ARRAY_SIZE(passes); i++) {
      DRWPass *pass = DRW_pass_create_instance("Renderpass Accumulation", passes[i], state);
      if (first == NULL) {
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
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
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
    int mat_options = VAR_MAT_MESH | VAR_MAT_DEPTH;
    SET_FLAG_FROM_TEST(mat_options, use_shadow_shader, VAR_MAT_HASH);
    SET_FLAG_FROM_TEST(mat_options, is_hair, VAR_MAT_HAIR);
    GPUMaterial *gpumat = (use_shadow_shader) ?
                              EEVEE_material_get(vedata, scene, ma, NULL, mat_options) :
                              EEVEE_material_default_get(scene, ma, mat_options);

    /* Avoid possible confusion with depth pre-pass options. */
    int option = KEY_SHADOW;
    SET_FLAG_FROM_TEST(option, is_hair, KEY_HAIR);

    /* Search for the same shaders usage in the pass. */
    struct GPUShader *sh = GPU_material_get_shader(gpumat);
    void *cache_key = (char *)sh + option;
    DRWShadingGroup *grp, **grp_p;

    if (BLI_ghash_ensure_p(pd->material_hash, cache_key, (void ***)&grp_p)) {
      /* This GPUShader has already been used by another material.
       * Add new shading group just after to avoid shader switching cost. */
      grp = DRW_shgroup_create_sub(*grp_p);
    }
    else {
      *grp_p = grp = DRW_shgroup_create(sh, psl->shadow_pass);
      EEVEE_material_bind_resources(grp, gpumat, sldata, vedata, NULL, NULL, false, false);
    }

    DRW_shgroup_add_material_resources(grp, gpumat);

    emc->shadow_grp = grp;
    emc->shadow_grp_p = grp_p;
  }
  else {
    emc->shadow_grp = NULL;
    emc->shadow_grp_p = NULL;
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

  /* HACK: Assume the struct will never be smaller than our variations.
   * This allow us to only keep one ghash and avoid bigger keys comparissons/hashing. */
  void *key = (char *)ma + is_hair;
  /* Search for other material instances (sharing the same Material datablock). */
  EeveeMaterialCache **emc_p, *emc;
  if (BLI_ghash_ensure_p(pd->material_hash, key, (void ***)&emc_p)) {
    return **emc_p;
  }
  else {
    *emc_p = emc = BLI_memblock_alloc(sldata->material_cache);
  }

  material_shadow(vedata, sldata, ma, is_hair, emc);

  {
    /* Depth Pass */
    int mat_options = VAR_MAT_MESH | VAR_MAT_DEPTH;
    SET_FLAG_FROM_TEST(mat_options, use_ssrefract, VAR_MAT_REFRACT);
    SET_FLAG_FROM_TEST(mat_options, use_depth_shader, VAR_MAT_HASH);
    SET_FLAG_FROM_TEST(mat_options, is_hair, VAR_MAT_HAIR);
    GPUMaterial *gpumat = (use_depth_shader) ?
                              EEVEE_material_get(vedata, scene, ma, NULL, mat_options) :
                              EEVEE_material_default_get(scene, ma, mat_options);

    int option = 0;
    SET_FLAG_FROM_TEST(option, do_cull, KEY_CULL);
    SET_FLAG_FROM_TEST(option, use_ssrefract, KEY_REFRACT);
    DRWPass *depth_ps = (DRWPass *[]){
        psl->depth_ps,
        psl->depth_cull_ps,
        psl->depth_refract_ps,
        psl->depth_refract_cull_ps,
    }[option];
    /* Hair are rendered inside the non-cull pass but needs to have a separate cache key. */
    SET_FLAG_FROM_TEST(option, is_hair, KEY_HAIR);

    /* Search for the same shaders usage in the pass. */
    struct GPUShader *sh = GPU_material_get_shader(gpumat);
    void *cache_key = (char *)sh + option;
    DRWShadingGroup *grp, **grp_p;

    if (BLI_ghash_ensure_p(pd->material_hash, cache_key, (void ***)&grp_p)) {
      /* This GPUShader has already been used by another material.
       * Add new shading group just after to avoid shader switching cost. */
      grp = DRW_shgroup_create_sub(*grp_p);
    }
    else {
      *grp_p = grp = DRW_shgroup_create(sh, depth_ps);
      EEVEE_material_bind_resources(grp, gpumat, sldata, vedata, NULL, NULL, false, false);
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
    GPUMaterial *gpumat = EEVEE_material_get(vedata, scene, ma, NULL, mat_options);
    const bool use_sss = GPU_material_flag_get(gpumat, GPU_MATFLAG_SSS);

    int ssr_id = (((effects->enabled_effects & EFFECT_SSR) != 0) && !use_ssrefract) ? 1 : 0;
    int option = (use_ssrefract ? 0 : (use_sss ? 1 : 2)) * 2 + do_cull;
    DRWPass *shading_pass = (DRWPass *[]){
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
     * This allow us to only keep one ghash and avoid bigger keys comparissons/hashing. */
    BLI_assert(option <= 16);
    struct GPUShader *sh = GPU_material_get_shader(gpumat);
    void *cache_key = (char *)sh + option;
    DRWShadingGroup *grp, **grp_p;

    if (BLI_ghash_ensure_p(pd->material_hash, cache_key, (void ***)&grp_p)) {
      /* This GPUShader has already been used by another material.
       * Add new shading group just after to avoid shader switching cost. */
      grp = DRW_shgroup_create_sub(*grp_p);
    }
    else {
      *grp_p = grp = DRW_shgroup_create(sh, shading_pass);
      EEVEE_material_bind_resources(
          grp, gpumat, sldata, vedata, &ssr_id, &ma->refract_depth, use_ssrefract, false);
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
  EeveeMaterialCache emc = {0};

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
    GPUMaterial *gpumat = EEVEE_material_get(vedata, scene, ma, NULL, mat_options);
    struct GPUShader *sh = GPU_material_get_shader(gpumat);

    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->transparent_pass);

    EEVEE_material_bind_resources(grp, gpumat, sldata, vedata, NULL, NULL, false, true);
    DRW_shgroup_add_material_resources(grp, gpumat);

    cur_state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : 0;

    DRW_shgroup_state_disable(grp, all_state);
    DRW_shgroup_state_enable(grp, cur_state);

    emc.depth_grp = grp;
  }
  {
    /* Shading */
    int ssr_id = -1; /* TODO transparent SSR */
    int mat_options = VAR_MAT_MESH | VAR_MAT_BLEND;
    SET_FLAG_FROM_TEST(mat_options, use_ssrefract, VAR_MAT_REFRACT);
    GPUMaterial *gpumat = EEVEE_material_get(vedata, scene, ma, NULL, mat_options);

    DRWShadingGroup *grp = DRW_shgroup_create(GPU_material_get_shader(gpumat),
                                              psl->transparent_pass);

    EEVEE_material_bind_resources(
        grp, gpumat, sldata, vedata, &ssr_id, &ma->refract_depth, use_ssrefract, true);
    DRW_shgroup_add_material_resources(grp, gpumat);

    cur_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM;
    cur_state |= (use_prepass) ? DRW_STATE_DEPTH_EQUAL : DRW_STATE_DEPTH_LESS_EQUAL;
    cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : 0;

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
  Material *ma = BKE_object_material_get(ob, slot + 1);
  if (ma == NULL) {
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
  const bool holdout = (ob->base_flag & BASE_HOLDOUT) != 0;
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

static void eevee_hair_cache_populate(EEVEE_Data *vedata,
                                      EEVEE_ViewLayerData *sldata,
                                      Object *ob,
                                      ParticleSystem *psys,
                                      ModifierData *md,
                                      int matnr,
                                      bool *cast_shadow)
{
  EeveeMaterialCache matcache = eevee_material_cache_get(vedata, sldata, ob, matnr - 1, true);

  if (matcache.depth_grp) {
    *matcache.depth_grp_p = DRW_shgroup_hair_create_sub(ob, psys, md, matcache.depth_grp);
  }
  if (matcache.shading_grp) {
    *matcache.shading_grp_p = DRW_shgroup_hair_create_sub(ob, psys, md, matcache.shading_grp);
  }
  if (matcache.shadow_grp) {
    *matcache.shadow_grp_p = DRW_shgroup_hair_create_sub(ob, psys, md, matcache.shadow_grp);
    *cast_shadow = true;
  }
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

  bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->v3d) &&
                         !DRW_state_is_image_render();

  /* First get materials for this mesh. */
  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) {
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

    if ((ob->dt >= OB_SOLID) || DRW_state_is_image_render()) {
      if (use_sculpt_pbvh) {
        struct DRWShadingGroup **shgrps_array = BLI_array_alloca(shgrps_array, materials_len);

        MATCACHE_AS_ARRAY(matcache, shading_grp, materials_len, shgrps_array);
        DRW_shgroup_call_sculpt_with_materials(shgrps_array, materials_len, ob);

        MATCACHE_AS_ARRAY(matcache, depth_grp, materials_len, shgrps_array);
        DRW_shgroup_call_sculpt_with_materials(shgrps_array, materials_len, ob);

        MATCACHE_AS_ARRAY(matcache, shadow_grp, materials_len, shgrps_array);
        DRW_shgroup_call_sculpt_with_materials(shgrps_array, materials_len, ob);
      }
      else {
        struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
        MATCACHE_AS_ARRAY(matcache, shading_gpumat, materials_len, gpumat_array);
        /* Get per-material split surface */
        struct GPUBatch **mat_geom = DRW_cache_object_surface_material_get(
            ob, gpumat_array, materials_len);

        if (mat_geom) {
          for (int i = 0; i < materials_len; i++) {
            if (mat_geom[i] == NULL) {
              continue;
            }

            /* Do not render surface if we are rendering a volume object
             * and do not have a surface closure. */
            if (use_volume_material &&
                (gpumat_array[i] && !GPU_material_has_surface_output(gpumat_array[i]))) {
              continue;
            }

            /* XXX TODO rewrite this to include the dupli objects.
             * This means we cannot exclude dupli objects from reflections!!! */
            EEVEE_ObjectEngineData *oedata = NULL;
            if ((ob->base_flag & BASE_FROM_DUPLI) == 0) {
              oedata = EEVEE_object_data_ensure(ob);
              oedata->ob = ob;
              oedata->test_data = &sldata->probes->vis_data;
            }

            ADD_SHGROUP_CALL(matcache[i].shading_grp, ob, mat_geom[i], oedata);
            ADD_SHGROUP_CALL_SAFE(matcache[i].depth_grp, ob, mat_geom[i], oedata);
            ADD_SHGROUP_CALL_SAFE(matcache[i].shadow_grp, ob, mat_geom[i], oedata);
            *cast_shadow = (matcache[i].shadow_grp != NULL);
          }
        }
      }
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
        eevee_hair_cache_populate(vedata, sldata, ob, psys, md, part->omat, cast_shadow);
      }
    }
  }
}

void EEVEE_object_hair_cache_populate(EEVEE_Data *vedata,
                                      EEVEE_ViewLayerData *sldata,
                                      Object *ob,
                                      bool *cast_shadow)
{
  eevee_hair_cache_populate(vedata, sldata, ob, NULL, NULL, HAIR_MATERIAL_NR, cast_shadow);
}

void EEVEE_materials_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;

  BLI_ghash_free(pd->material_hash, NULL, NULL);

  SET_FLAG_FROM_TEST(effects->enabled_effects, effects->sss_surface_count > 0, EFFECT_SSS);

  /* TODO(fclem) this is not really clean. Init should not be done in cache finish. */
  EEVEE_subsurface_draw_init(sldata, vedata);
}

void EEVEE_materials_free(void)
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

static void material_renderpass_init(EEVEE_FramebufferList *fbl,
                                     GPUTexture **output_tx,
                                     const eGPUTextureFormat format,
                                     const bool do_clear)
{
  DRW_texture_ensure_fullscreen_2d(output_tx, format, 0);
  /* Clear texture. */
  if (do_clear) {
    float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    /* TODO(fclem) replace by GPU_texture_clear once it is fast. */
    GPU_framebuffer_texture_attach(fbl->material_accum_fb, *output_tx, 0, 0);
    GPU_framebuffer_bind(fbl->material_accum_fb);
    GPU_framebuffer_clear_color(fbl->material_accum_fb, clear);
    GPU_framebuffer_bind(fbl->main_fb);
    GPU_framebuffer_texture_detach(fbl->material_accum_fb, *output_tx);
  }
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

  const bool do_clear = DRW_state_is_image_render() || (effects->taa_current_sample == 1);
  /* Create FrameBuffer. */
  GPU_framebuffer_ensure_config(&fbl->material_accum_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_LEAVE});

  if (pd->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
    material_renderpass_init(fbl, &txl->env_accum, texture_format, do_clear);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_EMIT) {
    material_renderpass_init(fbl, &txl->emit_accum, texture_format, do_clear);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_COLOR) {
    material_renderpass_init(fbl, &txl->diff_color_accum, texture_format, do_clear);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT) {
    material_renderpass_init(fbl, &txl->diff_light_accum, texture_format, do_clear);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_COLOR) {
    material_renderpass_init(fbl, &txl->spec_color_accum, texture_format, do_clear);
  }
  if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) {
    material_renderpass_init(fbl, &txl->spec_light_accum, texture_format, do_clear);

    if (effects->enabled_effects & EFFECT_SSR) {
      EEVEE_reflection_output_init(sldata, vedata, tot_samples);
    }
  }
}

static void material_renderpass_accumulate(EEVEE_FramebufferList *fbl,
                                           DRWPass *renderpass,
                                           EEVEE_PrivateData *pd,
                                           GPUTexture *output_tx,
                                           struct GPUUniformBuffer *renderpass_option_ubo)
{
  GPU_framebuffer_texture_attach(fbl->material_accum_fb, output_tx, 0, 0);
  GPU_framebuffer_bind(fbl->material_accum_fb);

  pd->renderpass_ubo = renderpass_option_ubo;
  DRW_draw_pass(renderpass);

  GPU_framebuffer_texture_detach(fbl->material_accum_fb, output_tx);
}

void EEVEE_material_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_PrivateData *pd = vedata->stl->g_data;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_TextureList *txl = vedata->txl;

  if (fbl->material_accum_fb != NULL) {
    DRWPass *material_accum_ps = psl->material_accum_ps;
    if (pd->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
      material_renderpass_accumulate(
          fbl, psl->background_accum_ps, pd, txl->env_accum, sldata->renderpass_ubo.combined);
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_EMIT) {
      material_renderpass_accumulate(
          fbl, material_accum_ps, pd, txl->emit_accum, sldata->renderpass_ubo.emit);
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_COLOR) {
      material_renderpass_accumulate(
          fbl, material_accum_ps, pd, txl->diff_color_accum, sldata->renderpass_ubo.diff_color);
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT) {
      material_renderpass_accumulate(
          fbl, material_accum_ps, pd, txl->diff_light_accum, sldata->renderpass_ubo.diff_light);

      if (effects->enabled_effects & EFFECT_SSS) {
        EEVEE_subsurface_output_accumulate(sldata, vedata);
      }
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_COLOR) {
      material_renderpass_accumulate(
          fbl, material_accum_ps, pd, txl->spec_color_accum, sldata->renderpass_ubo.spec_color);
    }
    if (pd->render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) {
      material_renderpass_accumulate(
          fbl, material_accum_ps, pd, txl->spec_light_accum, sldata->renderpass_ubo.spec_light);

      if (effects->enabled_effects & EFFECT_SSR) {
        EEVEE_reflection_output_accumulate(sldata, vedata);
      }
    }

    /* Restore default. */
    pd->renderpass_ubo = sldata->renderpass_ubo.combined;
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

/* \} */
