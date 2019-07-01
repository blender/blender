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
 * Volumetric effects rendering using frostbite approach.
 */

#include "DRW_render.h"

#include "BLI_rand.h"
#include "BLI_string_utils.h"

#include "DNA_object_force_types.h"
#include "DNA_smoke_types.h"
#include "DNA_world_types.h"

#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_smoke.h"

#include "ED_screen.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"
#include "GPU_draw.h"
#include "GPU_texture.h"
#include "GPU_material.h"

static struct {
  char *volumetric_common_lib;
  char *volumetric_common_lights_lib;

  struct GPUShader *volumetric_clear_sh;
  struct GPUShader *scatter_sh;
  struct GPUShader *scatter_with_lights_sh;
  struct GPUShader *volumetric_integration_sh;
  struct GPUShader *volumetric_resolve_sh;

  GPUTexture *color_src;
  GPUTexture *depth_src;

  GPUTexture *dummy_density;
  GPUTexture *dummy_flame;

  GPUTexture *dummy_scatter;
  GPUTexture *dummy_transmit;

  /* List of all smoke domains rendered within this frame. */
  ListBase smoke_domains;
} e_data = {NULL}; /* Engine data */

extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_lights_lib_glsl[];
extern char datatoc_volumetric_frag_glsl[];
extern char datatoc_volumetric_geom_glsl[];
extern char datatoc_volumetric_vert_glsl[];
extern char datatoc_volumetric_resolve_frag_glsl[];
extern char datatoc_volumetric_scatter_frag_glsl[];
extern char datatoc_volumetric_integration_frag_glsl[];
extern char datatoc_volumetric_lib_glsl[];
extern char datatoc_common_fullscreen_vert_glsl[];

static void eevee_create_shader_volumes(void)
{
  e_data.volumetric_common_lib = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                                  datatoc_common_uniforms_lib_glsl,
                                                  datatoc_bsdf_common_lib_glsl,
                                                  datatoc_volumetric_lib_glsl);

  e_data.volumetric_common_lights_lib = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                                         datatoc_common_uniforms_lib_glsl,
                                                         datatoc_bsdf_common_lib_glsl,
                                                         datatoc_octahedron_lib_glsl,
                                                         datatoc_irradiance_lib_glsl,
                                                         datatoc_lights_lib_glsl,
                                                         datatoc_volumetric_lib_glsl);

  e_data.volumetric_clear_sh = DRW_shader_create_with_lib(datatoc_volumetric_vert_glsl,
                                                          datatoc_volumetric_geom_glsl,
                                                          datatoc_volumetric_frag_glsl,
                                                          e_data.volumetric_common_lib,
                                                          "#define VOLUMETRICS\n"
                                                          "#define CLEAR\n");
  e_data.scatter_sh = DRW_shader_create_with_lib(datatoc_volumetric_vert_glsl,
                                                 datatoc_volumetric_geom_glsl,
                                                 datatoc_volumetric_scatter_frag_glsl,
                                                 e_data.volumetric_common_lights_lib,
                                                 SHADER_DEFINES
                                                 "#define VOLUMETRICS\n"
                                                 "#define VOLUME_SHADOW\n");
  e_data.scatter_with_lights_sh = DRW_shader_create_with_lib(datatoc_volumetric_vert_glsl,
                                                             datatoc_volumetric_geom_glsl,
                                                             datatoc_volumetric_scatter_frag_glsl,
                                                             e_data.volumetric_common_lights_lib,
                                                             SHADER_DEFINES
                                                             "#define VOLUMETRICS\n"
                                                             "#define VOLUME_LIGHTING\n"
                                                             "#define VOLUME_SHADOW\n");
  e_data.volumetric_integration_sh = DRW_shader_create_with_lib(
      datatoc_volumetric_vert_glsl,
      datatoc_volumetric_geom_glsl,
      datatoc_volumetric_integration_frag_glsl,
      e_data.volumetric_common_lib,
      NULL);
  e_data.volumetric_resolve_sh = DRW_shader_create_with_lib(datatoc_common_fullscreen_vert_glsl,
                                                            NULL,
                                                            datatoc_volumetric_resolve_frag_glsl,
                                                            e_data.volumetric_common_lib,
                                                            NULL);

  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  e_data.dummy_density = DRW_texture_create_3d(1, 1, 1, GPU_RGBA8, DRW_TEX_WRAP, color);

  float flame = 0.0f;
  e_data.dummy_flame = DRW_texture_create_3d(1, 1, 1, GPU_R8, DRW_TEX_WRAP, &flame);
}

void EEVEE_volumes_set_jitter(EEVEE_ViewLayerData *sldata, uint current_sample)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

  double ht_point[3];
  double ht_offset[3] = {0.0, 0.0};
  uint ht_primes[3] = {3, 7, 2};

  BLI_halton_3d(ht_primes, ht_offset, current_sample, ht_point);

  common_data->vol_jitter[0] = (float)ht_point[0];
  common_data->vol_jitter[1] = (float)ht_point[1];
  common_data->vol_jitter[2] = (float)ht_point[2];
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

  BLI_listbase_clear(&e_data.smoke_domains);

  const int tile_size = scene_eval->eevee.volumetric_tile_size;

  /* Find Froxel Texture resolution. */
  int tex_size[3];

  tex_size[0] = (int)ceilf(fmaxf(1.0f, viewport_size[0] / (float)tile_size));
  tex_size[1] = (int)ceilf(fmaxf(1.0f, viewport_size[1] / (float)tile_size));
  tex_size[2] = max_ii(scene_eval->eevee.volumetric_samples, 1);

  common_data->vol_coord_scale[0] = viewport_size[0] / (float)(tile_size * tex_size[0]);
  common_data->vol_coord_scale[1] = viewport_size[1] / (float)(tile_size * tex_size[1]);
  common_data->vol_coord_scale[2] = 1.0f / viewport_size[0];
  common_data->vol_coord_scale[3] = 1.0f / viewport_size[1];

  /* TODO compute snap to maxZBuffer for clustered rendering */
  if ((common_data->vol_tex_size[0] != tex_size[0]) ||
      (common_data->vol_tex_size[1] != tex_size[1]) ||
      (common_data->vol_tex_size[2] != tex_size[2])) {
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

    common_data->vol_inv_tex_size[0] = 1.0f / (float)(tex_size[0]);
    common_data->vol_inv_tex_size[1] = 1.0f / (float)(tex_size[1]);
    common_data->vol_inv_tex_size[2] = 1.0f / (float)(tex_size[2]);
  }

  /* Like frostbite's paper, 5% blend of the new frame. */
  common_data->vol_history_alpha = (txl->volume_prop_scattering == NULL) ? 0.0f : 0.95f;

  /* Temporal Super sampling jitter */
  uint ht_primes[3] = {3, 7, 2};
  uint current_sample = 0;

  /* If TAA is in use do not use the history buffer. */
  bool do_taa = ((effects->enabled_effects & EFFECT_TAA) != 0);

  if (draw_ctx->evil_C != NULL) {
    struct wmWindowManager *wm = CTX_wm_manager(draw_ctx->evil_C);
    do_taa = do_taa && (ED_screen_animation_no_scrub(wm) == NULL);
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
  common_data->vol_light_clamp = scene_eval->eevee.volumetric_light_clamp;
  common_data->vol_shadow_steps = (float)scene_eval->eevee.volumetric_shadow_samples;
  if ((scene_eval->eevee.flag & SCE_EEVEE_VOLUMETRIC_SHADOWS) == 0) {
    common_data->vol_shadow_steps = 0;
  }

  /* Update view_vecs */
  float invproj[4][4], winmat[4][4];
  DRW_view_winmat_get(NULL, winmat, false);
  DRW_view_winmat_get(NULL, invproj, true);
  EEVEE_update_viewvecs(invproj, winmat, sldata->common_data.view_vecs);

  if (DRW_view_is_persp_get(NULL)) {
    float sample_distribution = scene_eval->eevee.volumetric_sample_distribution;
    sample_distribution = 4.0f * (1.00001f - sample_distribution);

    const float clip_start = common_data->view_vecs[0][2];
    /* Negate */
    float near = integration_start = min_ff(-integration_start, clip_start - 1e-4f);
    float far = integration_end = min_ff(-integration_end, near - 1e-4f);

    common_data->vol_depth_param[0] = (far - near * exp2(1.0f / sample_distribution)) /
                                      (far - near);
    common_data->vol_depth_param[1] = (1.0f - common_data->vol_depth_param[0]) / near;
    common_data->vol_depth_param[2] = sample_distribution;
  }
  else {
    const float clip_start = common_data->view_vecs[0][2];
    const float clip_end = clip_start + common_data->view_vecs[1][2];
    integration_start = min_ff(integration_end, clip_start);
    integration_end = max_ff(-integration_end, clip_end);

    common_data->vol_depth_param[0] = integration_start;
    common_data->vol_depth_param[1] = integration_end;
    common_data->vol_depth_param[2] = 1.0f / (integration_end - integration_start);
  }

  /* Disable clamp if equal to 0. */
  if (common_data->vol_light_clamp == 0.0) {
    common_data->vol_light_clamp = FLT_MAX;
  }

  common_data->vol_use_lights = (scene_eval->eevee.flag & SCE_EEVEE_VOLUMETRIC_LIGHTS) != 0;

  if (!e_data.dummy_scatter) {
    float scatter[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float transmit[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    e_data.dummy_scatter = DRW_texture_create_3d(1, 1, 1, GPU_RGBA8, DRW_TEX_WRAP, scatter);
    e_data.dummy_transmit = DRW_texture_create_3d(1, 1, 1, GPU_RGBA8, DRW_TEX_WRAP, transmit);
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
  DRWShadingGroup *grp = NULL;

  /* Shaders */
  if (!e_data.scatter_sh) {
    eevee_create_shader_volumes();
  }

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
  struct World *wo = scene->world;
  if (wo != NULL && wo->use_nodes && wo->nodetree &&
      !LOOK_DEV_STUDIO_LIGHT_ENABLED(draw_ctx->v3d)) {
    struct GPUMaterial *mat = EEVEE_material_world_volume_get(scene, wo);

    if (GPU_material_use_domain_volume(mat)) {
      grp = DRW_shgroup_material_create(mat, psl->volumetric_world_ps);
    }

    if (grp) {
      DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
      /* TODO (fclem): remove those (need to clean the GLSL files). */
      DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
      DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
      DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
      DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
      DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);

      /* Fix principle volumetric not working with world materials. */
      DRW_shgroup_uniform_texture(grp, "sampdensity", e_data.dummy_density);
      DRW_shgroup_uniform_texture(grp, "sampflame", e_data.dummy_flame);
      DRW_shgroup_uniform_vec2_copy(grp, "unftemperature", (float[2]){0.0f, 1.0f});

      DRW_shgroup_call_procedural_triangles(grp, NULL, common_data->vol_tex_size[2]);

      effects->enabled_effects |= (EFFECT_VOLUMETRIC | EFFECT_POST_BUFFER);
    }
  }

  if (grp == NULL) {
    /* If no world or volume material is present just clear the buffer with this drawcall */
    grp = DRW_shgroup_create(e_data.volumetric_clear_sh, psl->volumetric_world_ps);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

    DRW_shgroup_call_procedural_triangles(grp, NULL, common_data->vol_tex_size[2]);
  }
}

typedef struct EEVEE_InstanceVolumeMatrix {
  DrawData dd;
  float volume_mat[4][4];
} EEVEE_InstanceVolumeMatrix;

void EEVEE_volumes_cache_object_add(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    Scene *scene,
                                    Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  static float white[3] = {1.0f, 1.0f, 1.0f};

  float *texcoloc = NULL;
  float *texcosize = NULL;
  struct ModifierData *md = NULL;
  Material *ma = give_current_material(ob, 1);

  if (ma == NULL) {
    return;
  }

  struct GPUMaterial *mat = EEVEE_material_mesh_volume_get(scene, ma);
  eGPUMaterialStatus status = GPU_material_status(mat);

  if (status == GPU_MAT_QUEUED) {
    vedata->stl->g_data->queued_shaders_count++;
  }
  /* If shader failed to compile or is currently compiling. */
  if (status != GPU_MAT_SUCCESS) {
    return;
  }

  DRWShadingGroup *grp = DRW_shgroup_material_create(mat, vedata->psl->volumetric_objects_ps);

  BKE_mesh_texspace_get_reference((struct Mesh *)ob->data, NULL, &texcoloc, NULL, &texcosize);

  /* TODO(fclem) remove those "unnecessary" UBOs */
  DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);

  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_vec3(grp, "volumeOrcoLoc", texcoloc, 1);
  DRW_shgroup_uniform_vec3(grp, "volumeOrcoSize", texcosize, 1);

  /* Smoke Simulation */
  if (((ob->base_flag & BASE_FROM_DUPLI) == 0) &&
      (md = modifiers_findByType(ob, eModifierType_Smoke)) &&
      (modifier_isEnabled(scene, md, eModifierMode_Realtime)) &&
      ((SmokeModifierData *)md)->domain != NULL) {
    SmokeModifierData *smd = (SmokeModifierData *)md;
    SmokeDomainSettings *sds = smd->domain;

    /* Don't show smoke before simulation starts, this could be made an option in the future. */
    const bool show_smoke = ((int)DEG_get_ctime(draw_ctx->depsgraph) >=
                             sds->point_cache[0]->startframe);

    if (sds->fluid && show_smoke) {
      const bool show_highres = BKE_smoke_show_highres(scene, sds);
      if (!sds->wt || !show_highres) {
        GPU_create_smoke(smd, 0);
      }
      else if (sds->wt && show_highres) {
        GPU_create_smoke(smd, 1);
      }
      BLI_addtail(&e_data.smoke_domains, BLI_genericNodeN(smd));
    }

    DRW_shgroup_uniform_texture_ref(
        grp, "sampdensity", sds->tex ? &sds->tex : &e_data.dummy_density);
    DRW_shgroup_uniform_texture_ref(
        grp, "sampflame", sds->tex_flame ? &sds->tex_flame : &e_data.dummy_flame);

    /* Constant Volume color. */
    bool use_constant_color = ((sds->active_fields & SM_ACTIVE_COLORS) == 0 &&
                               (sds->active_fields & SM_ACTIVE_COLOR_SET) != 0);

    DRW_shgroup_uniform_vec3(
        grp, "volumeColor", (use_constant_color) ? sds->active_color : white, 1);

    /* Output is such that 0..1 maps to 0..1000K */
    DRW_shgroup_uniform_vec2(grp, "unftemperature", &sds->flame_ignition, 1);
  }
  else {
    DRW_shgroup_uniform_texture(grp, "sampdensity", e_data.dummy_density);
    DRW_shgroup_uniform_texture(grp, "sampflame", e_data.dummy_flame);
    DRW_shgroup_uniform_vec3(grp, "volumeColor", white, 1);
    DRW_shgroup_uniform_vec2(grp, "unftemperature", (float[2]){0.0f, 1.0f}, 1);
  }

  /* TODO Reduce to number of slices intersecting. */
  /* TODO Preemptive culling. */
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
    struct GPUShader *sh;

    DRW_PASS_CREATE(psl->volumetric_scatter_ps, DRW_STATE_WRITE_COLOR);
    sh = (common_data->vol_use_lights) ? e_data.scatter_with_lights_sh : e_data.scatter_sh;
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

    DRW_shgroup_call_procedural_triangles(grp, NULL, common_data->vol_tex_size[2]);

    DRW_PASS_CREATE(psl->volumetric_integration_ps, DRW_STATE_WRITE_COLOR);
    grp = DRW_shgroup_create(e_data.volumetric_integration_sh, psl->volumetric_integration_ps);
    DRW_shgroup_uniform_texture_ref(grp, "volumeScattering", &txl->volume_scatter);
    DRW_shgroup_uniform_texture_ref(grp, "volumeExtinction", &txl->volume_transmit);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

    DRW_shgroup_call_procedural_triangles(grp, NULL, common_data->vol_tex_size[2]);

    DRW_PASS_CREATE(psl->volumetric_resolve_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    grp = DRW_shgroup_create(e_data.volumetric_resolve_sh, psl->volumetric_resolve_ps);
    DRW_shgroup_uniform_texture_ref(grp, "inScattering", &txl->volume_scatter);
    DRW_shgroup_uniform_texture_ref(grp, "inTransmittance", &txl->volume_transmit);
    DRW_shgroup_uniform_texture_ref(grp, "inSceneDepth", &e_data.depth_src);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
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

    if (txl->volume_prop_scattering == NULL) {
      /* Volume properties: We evaluate all volumetric objects
       * and store their final properties into each froxel */
      txl->volume_prop_scattering = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_R11F_G11F_B10F, DRW_TEX_FILTER, NULL);
      txl->volume_prop_extinction = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_R11F_G11F_B10F, DRW_TEX_FILTER, NULL);
      txl->volume_prop_emission = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_R11F_G11F_B10F, DRW_TEX_FILTER, NULL);
      txl->volume_prop_phase = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_RG16F, DRW_TEX_FILTER, NULL);

      /* Volume scattering: We compute for each froxel the
       * Scattered light towards the view. We also resolve temporal
       * super sampling during this stage. */
      txl->volume_scatter = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_R11F_G11F_B10F, DRW_TEX_FILTER, NULL);
      txl->volume_transmit = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_R11F_G11F_B10F, DRW_TEX_FILTER, NULL);

      /* Final integration: We compute for each froxel the
       * amount of scattered light and extinction coef at this
       * given depth. We use theses textures as double buffer
       * for the volumetric history. */
      txl->volume_scatter_history = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_R11F_G11F_B10F, DRW_TEX_FILTER, NULL);
      txl->volume_transmit_history = DRW_texture_create_3d(
          tex_size[0], tex_size[1], tex_size[2], GPU_R11F_G11F_B10F, DRW_TEX_FILTER, NULL);
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

void EEVEE_volumes_compute(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    DRW_stats_group_start("Volumetrics");

    GPU_framebuffer_bind(fbl->volumetric_fb);
    DRW_draw_pass(psl->volumetric_world_ps);
    DRW_draw_pass(psl->volumetric_objects_ps);

    GPU_framebuffer_bind(fbl->volumetric_scat_fb);
    DRW_draw_pass(psl->volumetric_scatter_ps);

    GPU_framebuffer_bind(fbl->volumetric_integ_fb);
    DRW_draw_pass(psl->volumetric_integration_ps);

    SWAP(struct GPUFrameBuffer *, fbl->volumetric_scat_fb, fbl->volumetric_integ_fb);
    SWAP(GPUTexture *, txl->volume_scatter, txl->volume_scatter_history);
    SWAP(GPUTexture *, txl->volume_transmit, txl->volume_transmit_history);

    effects->volume_scatter = txl->volume_scatter;
    effects->volume_transmit = txl->volume_transmit;

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);

    DRW_stats_group_end();
  }
}

void EEVEE_volumes_resolve(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    e_data.depth_src = dtxl->depth;

    /* Apply for opaque geometry. */
    GPU_framebuffer_bind(fbl->main_color_fb);
    DRW_draw_pass(psl->volumetric_resolve_ps);

    /* Restore. */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_volumes_free_smoke_textures(void)
{
  /* Free Smoke Textures after rendering */
  for (LinkData *link = e_data.smoke_domains.first; link; link = link->next) {
    SmokeModifierData *smd = (SmokeModifierData *)link->data;
    GPU_free_smoke(smd);
  }
  BLI_freelistN(&e_data.smoke_domains);
}

void EEVEE_volumes_free(void)
{
  MEM_SAFE_FREE(e_data.volumetric_common_lib);
  MEM_SAFE_FREE(e_data.volumetric_common_lights_lib);

  DRW_TEXTURE_FREE_SAFE(e_data.dummy_scatter);
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_transmit);

  DRW_TEXTURE_FREE_SAFE(e_data.dummy_density);
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_flame);

  DRW_SHADER_FREE_SAFE(e_data.volumetric_clear_sh);
  DRW_SHADER_FREE_SAFE(e_data.scatter_sh);
  DRW_SHADER_FREE_SAFE(e_data.scatter_with_lights_sh);
  DRW_SHADER_FREE_SAFE(e_data.volumetric_integration_sh);
  DRW_SHADER_FREE_SAFE(e_data.volumetric_resolve_sh);
}
