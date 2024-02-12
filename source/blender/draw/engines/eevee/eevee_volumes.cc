/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Volumetric effects rendering using frostbite approach.
 */

#include "DRW_render.hh"

#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_string_utils.hh"

#include "DNA_fluid_types.h"
#include "DNA_object_force_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "BKE_fluid.h"
#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_volume.hh"
#include "BKE_volume_render.hh"

#include "ED_screen.hh"

#include "DEG_depsgraph_query.hh"

#include "GPU_capabilities.h"
#include "GPU_context.h"
#include "GPU_material.hh"
#include "GPU_texture.h"
#include "eevee_private.h"

static struct {
  GPUTexture *depth_src;

  GPUTexture *dummy_zero;
  GPUTexture *dummy_one;
  GPUTexture *dummy_flame;

  GPUTexture *dummy_scatter;
  GPUTexture *dummy_transmit;
} e_data = {nullptr}; /* Engine data */

void EEVEE_volumes_set_jitter(EEVEE_ViewLayerData *sldata, uint current_sample)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

  double ht_point[3];
  double ht_offset[3] = {0.0, 0.0};
  const uint ht_primes[3] = {3, 7, 2};

  BLI_halton_3d(ht_primes, ht_offset, current_sample, ht_point);

  common_data->vol_jitter[0] = float(ht_point[0]);
  common_data->vol_jitter[1] = float(ht_point[1]);
  common_data->vol_jitter[2] = float(ht_point[2]);
}

void EEVEE_volumes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  const float *viewport_size = DRW_viewport_size_get();

  const int tile_size = scene_eval->eevee.volumetric_tile_size;

  /* Find Froxel Texture resolution. */
  int tex_size[3];

  tex_size[0] = int(ceilf(fmaxf(1.0f, viewport_size[0] / float(tile_size))));
  tex_size[1] = int(ceilf(fmaxf(1.0f, viewport_size[1] / float(tile_size))));
  tex_size[2] = max_ii(scene_eval->eevee.volumetric_samples, 1);

  /* Clamp 3D texture size based on device maximum. */
  int maxSize = GPU_max_texture_3d_size();
  BLI_assert(tex_size[0] <= maxSize);
  tex_size[0] = tex_size[0] > maxSize ? maxSize : tex_size[0];
  tex_size[1] = tex_size[1] > maxSize ? maxSize : tex_size[1];
  tex_size[2] = tex_size[2] > maxSize ? maxSize : tex_size[2];

  common_data->vol_coord_scale[0] = viewport_size[0] / float(tile_size * tex_size[0]);
  common_data->vol_coord_scale[1] = viewport_size[1] / float(tile_size * tex_size[1]);
  common_data->vol_coord_scale[2] = 1.0f / viewport_size[0];
  common_data->vol_coord_scale[3] = 1.0f / viewport_size[1];

  /* TODO: compute snap to maxZBuffer for clustered rendering. */
  if ((common_data->vol_tex_size[0] != tex_size[0]) ||
      (common_data->vol_tex_size[1] != tex_size[1]) ||
      (common_data->vol_tex_size[2] != tex_size[2]))
  {
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_scattering);
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_extinction);
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_emission);
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_phase);
    DRW_TEXTURE_FREE_SAFE(txl->volume_scatter);
    DRW_TEXTURE_FREE_SAFE(txl->volume_transmit);
    DRW_TEXTURE_FREE_SAFE(txl->volume_scatter_history);
    DRW_TEXTURE_FREE_SAFE(txl->volume_transmit_history);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->volumetric_fb);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->volumetric_scat_fb);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->volumetric_integ_fb);
    copy_v3_v3_int(common_data->vol_tex_size, tex_size);

    common_data->vol_inv_tex_size[0] = 1.0f / float(tex_size[0]);
    common_data->vol_inv_tex_size[1] = 1.0f / float(tex_size[1]);
    common_data->vol_inv_tex_size[2] = 1.0f / float(tex_size[2]);
  }

  /* Like frostbite's paper, 5% blend of the new frame. */
  common_data->vol_history_alpha = (txl->volume_prop_scattering == nullptr) ? 0.0f : 0.95f;

  /* Temporal Super sampling jitter */
  uint ht_primes[3] = {3, 7, 2};
  uint current_sample = 0;

  /* If TAA is in use do not use the history buffer. */
  bool do_taa = ((effects->enabled_effects & EFFECT_TAA) != 0);

  if (draw_ctx->evil_C != nullptr) {
    wmWindowManager *wm = CTX_wm_manager(draw_ctx->evil_C);
    do_taa = do_taa && (ED_screen_animation_no_scrub(wm) == nullptr);
  }

  if (do_taa) {
    common_data->vol_history_alpha = 0.0f;
    current_sample = effects->taa_current_sample - 1;
    effects->volume_current_sample = -1;
  }
  else if (DRW_state_is_image_render()) {
    const uint max_sample = (ht_primes[0] * ht_primes[1] * ht_primes[2]);
    current_sample = effects->volume_current_sample = (effects->volume_current_sample + 1) %
                                                      max_sample;
    if (current_sample != max_sample - 1) {
      DRW_viewport_request_redraw();
    }
  }

  EEVEE_volumes_set_jitter(sldata, current_sample);

  float integration_start = scene_eval->eevee.volumetric_start;
  float integration_end = scene_eval->eevee.volumetric_end;
  effects->volume_light_clamp = scene_eval->eevee.volumetric_light_clamp;
  common_data->vol_shadow_steps = float(scene_eval->eevee.volumetric_shadow_samples);
  if ((scene_eval->eevee.flag & SCE_EEVEE_VOLUMETRIC_SHADOWS) == 0) {
    common_data->vol_shadow_steps = 0;
  }

  if (DRW_view_is_persp_get(nullptr)) {
    float sample_distribution = scene_eval->eevee.volumetric_sample_distribution;
    sample_distribution = 4.0f * max_ff(1.0f - sample_distribution, 1e-2f);

    const float clip_start = DRW_view_near_distance_get(nullptr);
    /* Negate */
    float near = integration_start = min_ff(-integration_start, clip_start - 1e-4f);
    float far = integration_end = min_ff(-integration_end, near - 1e-4f);

    common_data->vol_depth_param[0] = (far - near * exp2(1.0f / sample_distribution)) /
                                      (far - near);
    common_data->vol_depth_param[1] = (1.0f - common_data->vol_depth_param[0]) / near;
    common_data->vol_depth_param[2] = sample_distribution;
  }
  else {
    const float clip_start = DRW_view_near_distance_get(nullptr);
    const float clip_end = DRW_view_far_distance_get(nullptr);
    integration_start = min_ff(integration_end, clip_start);
    integration_end = max_ff(-integration_end, clip_end);

    common_data->vol_depth_param[0] = integration_start;
    common_data->vol_depth_param[1] = integration_end;
    common_data->vol_depth_param[2] = 1.0f / (integration_end - integration_start);
  }

  /* Disable clamp if equal to 0. */
  if (effects->volume_light_clamp == 0.0) {
    effects->volume_light_clamp = FLT_MAX;
  }

  common_data->vol_use_lights = (scene_eval->eevee.flag & SCE_EEVEE_VOLUMETRIC_LIGHTS) != 0;
  common_data->vol_use_soft_shadows = (scene_eval->eevee.flag & SCE_EEVEE_SHADOW_SOFT) != 0;

  if (!e_data.dummy_scatter) {
    const float scatter[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float transmit[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    eGPUTextureUsage dummy_usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
    e_data.dummy_scatter = DRW_texture_create_3d_ex(
        1, 1, 1, GPU_RGBA8, dummy_usage, DRW_TEX_WRAP, scatter);
    e_data.dummy_transmit = DRW_texture_create_3d_ex(
        1, 1, 1, GPU_RGBA8, dummy_usage, DRW_TEX_WRAP, transmit);
  }
}

void EEVEE_volumes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  DRWShadingGroup *grp = nullptr;

  /* Quick breakdown of the Volumetric rendering:
   *
   * The rendering is separated in 4 stages:
   *
   * - Material Parameters : we collect volume properties of
   *   all participating media in the scene and store them in
   *   a 3D texture aligned with the 3D frustum.
   *   This is done in 2 passes, one that clear the texture
   *   and/or evaluate the world volumes, and the 2nd one that
   *   additively render object volumes.
   *
   * - Light Scattering : the volume properties then are sampled
   *   and light scattering is evaluated for each cell of the
   *   volume texture. Temporal super-sampling (if enabled) occurs here.
   *
   * - Volume Integration : the scattered light and extinction is
   *   integrated (accumulated) along the view-rays. The result is stored
   *   for every cell in another texture.
   *
   * - Full-screen Resolve : From the previous stage, we get two
   *   3D textures that contains integrated scattered light and extinction
   *   for "every" positions in the frustum. We only need to sample
   *   them and blend the scene color with those factors. This also
   *   work for alpha blended materials.
   */

  /* World pass is not additive as it also clear the buffer. */
  DRW_PASS_CREATE(psl->volumetric_world_ps, DRW_STATE_WRITE_COLOR);
  DRW_PASS_CREATE(psl->volumetric_objects_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD);

  /* World Volumetric */
  World *wo = scene->world;
  if (wo != nullptr && wo->use_nodes && wo->nodetree &&
      !LOOK_DEV_STUDIO_LIGHT_ENABLED(draw_ctx->v3d))
  {
    GPUMaterial *mat = EEVEE_material_get(vedata, scene, nullptr, wo, VAR_MAT_VOLUME);

    if (mat && GPU_material_has_volume_output(mat)) {
      grp = DRW_shgroup_material_create(mat, psl->volumetric_world_ps);
    }

    if (grp) {
      DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
      /* TODO(fclem): remove those (need to clean the GLSL files). */
      DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
      DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
      DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
      DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
      DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
      DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);

      /* Fix principle volumetric not working with world materials. */
      grp = DRW_shgroup_volume_create_sub(nullptr, nullptr, grp, mat);

      DRW_shgroup_call_procedural_triangles(grp, nullptr, common_data->vol_tex_size[2]);

      effects->enabled_effects |= (EFFECT_VOLUMETRIC | EFFECT_POST_BUFFER);
    }
  }

  if (grp == nullptr) {
    /* If no world or volume material is present just clear the buffer with this drawcall */
    grp = DRW_shgroup_create(EEVEE_shaders_volumes_clear_sh_get(), psl->volumetric_world_ps);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
    DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
    DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);

    DRW_shgroup_call_procedural_triangles(grp, nullptr, common_data->vol_tex_size[2]);
  }
}

void EEVEE_volumes_cache_object_add(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    Scene *scene,
                                    Object *ob)
{
  Material *ma = BKE_object_material_get_eval(ob, 1);

  if (ma == nullptr) {
    if (ob->type == OB_VOLUME) {
      ma = BKE_material_default_volume();
    }
    else {
      return;
    }
  }

  float size[3];
  mat4_to_size(size, ob->object_to_world);
  /* Check if any of the axes have 0 length. (see #69070) */
  const float epsilon = 1e-8f;
  if ((size[0] < epsilon) || (size[1] < epsilon) || (size[2] < epsilon)) {
    return;
  }

  int mat_options = VAR_MAT_VOLUME | VAR_MAT_MESH;
  GPUMaterial *mat = EEVEE_material_get(vedata, scene, ma, nullptr, mat_options);

  /* If shader failed to compile or is currently compiling. */
  if (mat == nullptr) {
    return;
  }

  GPUShader *sh = GPU_material_get_shader(mat);
  if (sh == nullptr) {
    return;
  }

  /* TODO(fclem): Reuse main shading group to avoid shading binding cost just like for surface
   * shaders. */
  DRWShadingGroup *grp = DRW_shgroup_create(sh, vedata->psl->volumetric_objects_ps);

  grp = DRW_shgroup_volume_create_sub(scene, ob, grp, mat);

  if (grp == nullptr) {
    return;
  }

  DRW_shgroup_add_material_resources(grp, mat);

  /* TODO(fclem): remove those "unnecessary" UBOs */
  DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
  DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  /* TODO: Reduce to number of slices intersecting. */
  /* TODO: Preemptive culling. */
  DRW_shgroup_call_procedural_triangles(grp, ob, sldata->common_data.vol_tex_size[2]);

  vedata->stl->effects->enabled_effects |= (EFFECT_VOLUMETRIC | EFFECT_POST_BUFFER);
}

void EEVEE_volumes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  LightCache *lcache = vedata->stl->g_data->light_cache;
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    DRWShadingGroup *grp;
    GPUShader *sh;

    DRW_PASS_CREATE(psl->volumetric_scatter_ps, DRW_STATE_WRITE_COLOR);
    sh = (common_data->vol_use_lights) ? EEVEE_shaders_volumes_scatter_with_lights_sh_get() :
                                         EEVEE_shaders_volumes_scatter_sh_get();
    grp = DRW_shgroup_create(sh, psl->volumetric_scatter_ps);
    DRW_shgroup_uniform_texture_ref(grp, "irradianceGrid", &lcache->grid_tx.tex);
    DRW_shgroup_uniform_texture_ref(grp, "shadowCubeTexture", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref(grp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);
    DRW_shgroup_uniform_texture_ref(grp, "volumeScattering", &txl->volume_prop_scattering);
    DRW_shgroup_uniform_texture_ref(grp, "volumeExtinction", &txl->volume_prop_extinction);
    DRW_shgroup_uniform_texture_ref(grp, "volumeEmission", &txl->volume_prop_emission);
    DRW_shgroup_uniform_texture_ref(grp, "volumePhase", &txl->volume_prop_phase);
    DRW_shgroup_uniform_texture_ref(grp, "historyScattering", &txl->volume_scatter_history);
    DRW_shgroup_uniform_texture_ref(grp, "historyTransmittance", &txl->volume_transmit_history);
    DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
    DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);

    DRW_shgroup_call_procedural_triangles(grp, nullptr, common_data->vol_tex_size[2]);

    DRW_PASS_CREATE(psl->volumetric_integration_ps, DRW_STATE_WRITE_COLOR);
    grp = DRW_shgroup_create(EEVEE_shaders_volumes_integration_sh_get(),
                             psl->volumetric_integration_ps);
    DRW_shgroup_uniform_texture_ref(grp, "volumeScattering", &txl->volume_scatter);
    DRW_shgroup_uniform_texture_ref(grp, "volumeExtinction", &txl->volume_transmit);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    if (USE_VOLUME_OPTI) {
      DRW_shgroup_uniform_image_ref(grp, "finalScattering_img", &txl->volume_scatter_history);
      DRW_shgroup_uniform_image_ref(grp, "finalTransmittance_img", &txl->volume_transmit_history);
    }

    DRW_shgroup_call_procedural_triangles(
        grp, nullptr, USE_VOLUME_OPTI ? 1 : common_data->vol_tex_size[2]);

    DRW_PASS_CREATE(psl->volumetric_resolve_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    grp = DRW_shgroup_create(EEVEE_shaders_volumes_resolve_sh_get(false),
                             psl->volumetric_resolve_ps);
    DRW_shgroup_uniform_texture_ref(grp, "inScattering", &txl->volume_scatter);
    DRW_shgroup_uniform_texture_ref(grp, "inTransmittance", &txl->volume_transmit);
    DRW_shgroup_uniform_texture_ref(grp, "inSceneDepth", &e_data.depth_src);
    DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);

    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
}

void EEVEE_volumes_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    int *tex_size = common_data->vol_tex_size;

    if (txl->volume_prop_scattering == nullptr) {
      /* Volume properties: We evaluate all volumetric objects
       * and store their final properties into each froxel */
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
      txl->volume_prop_scattering = DRW_texture_create_3d_ex(tex_size[0],
                                                             tex_size[1],
                                                             tex_size[2],
                                                             GPU_R11F_G11F_B10F,
                                                             usage,
                                                             DRW_TEX_FILTER,
                                                             nullptr);
      txl->volume_prop_extinction = DRW_texture_create_3d_ex(tex_size[0],
                                                             tex_size[1],
                                                             tex_size[2],
                                                             GPU_R11F_G11F_B10F,
                                                             usage,
                                                             DRW_TEX_FILTER,
                                                             nullptr);
      txl->volume_prop_emission = DRW_texture_create_3d_ex(tex_size[0],
                                                           tex_size[1],
                                                           tex_size[2],
                                                           GPU_R11F_G11F_B10F,
                                                           usage,
                                                           DRW_TEX_FILTER,
                                                           nullptr);
      txl->volume_prop_phase = DRW_texture_create_3d_ex(
          tex_size[0], tex_size[1], tex_size[2], GPU_RG16F, usage, DRW_TEX_FILTER, nullptr);

      /* Volume scattering: We compute for each froxel the
       * Scattered light towards the view. We also resolve temporal
       * super sampling during this stage. */
      eGPUTextureUsage usage_write = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ |
                                     GPU_TEXTURE_USAGE_SHADER_WRITE;
      txl->volume_scatter = DRW_texture_create_3d_ex(tex_size[0],
                                                     tex_size[1],
                                                     tex_size[2],
                                                     GPU_R11F_G11F_B10F,
                                                     usage_write,
                                                     DRW_TEX_FILTER,
                                                     nullptr);
      txl->volume_transmit = DRW_texture_create_3d_ex(tex_size[0],
                                                      tex_size[1],
                                                      tex_size[2],
                                                      GPU_R11F_G11F_B10F,
                                                      usage_write,
                                                      DRW_TEX_FILTER,
                                                      nullptr);

      /* Final integration: We compute for each froxel the
       * amount of scattered light and extinction coefficient at this
       * given depth. We use these textures as double buffer
       * for the volumetric history. */
      txl->volume_scatter_history = DRW_texture_create_3d_ex(tex_size[0],
                                                             tex_size[1],
                                                             tex_size[2],
                                                             GPU_R11F_G11F_B10F,
                                                             usage_write,
                                                             DRW_TEX_FILTER,
                                                             nullptr);
      txl->volume_transmit_history = DRW_texture_create_3d_ex(tex_size[0],
                                                              tex_size[1],
                                                              tex_size[2],
                                                              GPU_R11F_G11F_B10F,
                                                              usage_write,
                                                              DRW_TEX_FILTER,
                                                              nullptr);
    }

    GPU_framebuffer_ensure_config(&fbl->volumetric_fb,
                                  {GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_prop_scattering),
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_prop_extinction),
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_prop_emission),
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_prop_phase)});
    GPU_framebuffer_ensure_config(&fbl->volumetric_scat_fb,
                                  {GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_scatter),
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_transmit)});
    GPU_framebuffer_ensure_config(&fbl->volumetric_integ_fb,
                                  {GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_scatter_history),
                                   GPU_ATTACHMENT_TEXTURE(txl->volume_transmit_history)});
  }
  else {
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_scattering);
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_extinction);
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_emission);
    DRW_TEXTURE_FREE_SAFE(txl->volume_prop_phase);
    DRW_TEXTURE_FREE_SAFE(txl->volume_scatter);
    DRW_TEXTURE_FREE_SAFE(txl->volume_transmit);
    DRW_TEXTURE_FREE_SAFE(txl->volume_scatter_history);
    DRW_TEXTURE_FREE_SAFE(txl->volume_transmit_history);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->volumetric_fb);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->volumetric_scat_fb);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->volumetric_integ_fb);
  }

  effects->volume_scatter = e_data.dummy_scatter;
  effects->volume_transmit = e_data.dummy_transmit;
}

void EEVEE_volumes_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    DRW_stats_group_start("Volumetrics");

    /* We sample the shadow-maps using shadow sampler. We need to enable Comparison mode.
     * TODO(fclem): avoid this by using sampler objects. */
    GPU_texture_compare_mode(sldata->shadow_cube_pool, true);
    GPU_texture_compare_mode(sldata->shadow_cascade_pool, true);

    GPU_framebuffer_bind(fbl->volumetric_fb);
    DRW_draw_pass(psl->volumetric_world_ps);
    DRW_draw_pass(psl->volumetric_objects_ps);

    GPU_framebuffer_bind(fbl->volumetric_scat_fb);
    DRW_draw_pass(psl->volumetric_scatter_ps);

    if (USE_VOLUME_OPTI) {
      /* Avoid feedback loop assert. */
      GPU_framebuffer_bind(fbl->volumetric_fb);
    }
    else {
      GPU_framebuffer_bind(fbl->volumetric_integ_fb);
    }

    DRW_draw_pass(psl->volumetric_integration_ps);

    std::swap(fbl->volumetric_scat_fb, fbl->volumetric_integ_fb);
    std::swap(txl->volume_scatter, txl->volume_scatter_history);
    std::swap(txl->volume_transmit, txl->volume_transmit_history);

    effects->volume_scatter = txl->volume_scatter;
    effects->volume_transmit = txl->volume_transmit;

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);

    DRW_stats_group_end();
  }
}

void EEVEE_volumes_resolve(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    e_data.depth_src = dtxl->depth;

    if (USE_VOLUME_OPTI) {
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
    }

    /* Apply for opaque geometry. */
    GPU_framebuffer_bind(fbl->main_color_fb);
    DRW_draw_pass(psl->volumetric_resolve_ps);

    /* Restore. */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_volumes_free()
{
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_scatter);
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_transmit);

  DRW_TEXTURE_FREE_SAFE(e_data.dummy_zero);
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_one);
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_flame);
}

/* -------------------------------------------------------------------- */
/** \name Render Passes
 * \{ */

void EEVEE_volumes_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = stl->effects;

  /* Create FrameBuffer. */

  /* Should be enough precision for many samples. */
  const eGPUTextureFormat texture_format_accum = (tot_samples > 128) ? GPU_RGBA32F : GPU_RGBA16F;
  DRW_texture_ensure_fullscreen_2d(
      &txl->volume_scatter_accum, texture_format_accum, DRWTextureFlag(0));
  DRW_texture_ensure_fullscreen_2d(
      &txl->volume_transmittance_accum, texture_format_accum, DRWTextureFlag(0));

  GPU_framebuffer_ensure_config(&fbl->volumetric_accum_fb,
                                {GPU_ATTACHMENT_NONE,
                                 GPU_ATTACHMENT_TEXTURE(txl->volume_scatter_accum),
                                 GPU_ATTACHMENT_TEXTURE(txl->volume_transmittance_accum)});

  /* Create Pass and shgroup. */
  DRW_PASS_CREATE(psl->volumetric_accum_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
  DRWShadingGroup *grp = nullptr;
  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    grp = DRW_shgroup_create(EEVEE_shaders_volumes_resolve_sh_get(true), psl->volumetric_accum_ps);
    DRW_shgroup_uniform_texture_ref(grp, "inScattering", &txl->volume_scatter);
    DRW_shgroup_uniform_texture_ref(grp, "inTransmittance", &txl->volume_transmit);
    DRW_shgroup_uniform_texture_ref(grp, "inSceneDepth", &e_data.depth_src);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
  }
  else {
    /* There is no volumetrics in the scene. Use a shader to fill the accum textures with a default
     * value. */
    grp = DRW_shgroup_create(EEVEE_shaders_volumes_accum_sh_get(), psl->volumetric_accum_ps);
  }
  DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), nullptr);
}

void EEVEE_volumes_output_accumulate(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;

  if (fbl->volumetric_accum_fb != nullptr) {
    /* Accumulation pass. */
    GPU_framebuffer_bind(fbl->volumetric_accum_fb);

    /* Clear texture. */
    if (effects->taa_current_sample == 1) {
      const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(fbl->volumetric_accum_fb, clear);
    }

    DRW_draw_pass(psl->volumetric_accum_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

/** \} */
