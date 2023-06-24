/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Cavity Effect:
 *
 * We use Screen Space Ambient Occlusion (SSAO) to enhance geometric details of the surfaces.
 * We also use a Curvature effect computed only using the surface normals.
 *
 * This is done after the opaque pass. It only affects the opaque surfaces.
 */

#include "DRW_render.h"

#include "BLI_rand.h"

#include "../eevee/eevee_lut.h" /* TODO: find somewhere to share blue noise Table. */

#include "workbench_engine.h"
#include "workbench_private.h"

#define JITTER_TEX_SIZE 64
#define CAVITY_MAX_SAMPLES 512

/* Using Hammersley distribution */
static float *create_disk_samples(int num_samples, int num_iterations)
{
  BLI_assert(num_samples * num_iterations <= CAVITY_MAX_SAMPLES);
  const int total_samples = num_samples * num_iterations;
  const float num_samples_inv = 1.0f / num_samples;
  /* vec4 to ensure memory alignment. */
  float(*texels)[4] = MEM_callocN(sizeof(float[4]) * CAVITY_MAX_SAMPLES, __func__);
  for (int i = 0; i < total_samples; i++) {
    float it_add = (i / num_samples) * 0.499f;
    float r = fmodf((i + 0.5f + it_add) * num_samples_inv, 1.0f);
    double dphi;
    BLI_hammersley_1d(i, &dphi);

    float phi = (float)dphi * 2.0f * M_PI + it_add;
    texels[i][0] = cosf(phi);
    texels[i][1] = sinf(phi);
    /* This deliberately distribute more samples
     * at the center of the disk (and thus the shadow). */
    texels[i][2] = r;
  }

  return (float *)texels;
}

static GPUTexture *create_jitter_texture(int num_samples)
{
  float jitter[64 * 64][4];
  const float num_samples_inv = 1.0f / num_samples;

  for (int i = 0; i < 64 * 64; i++) {
    float phi = blue_noise[i][0] * 2.0f * M_PI;
    /* This rotate the sample per pixels */
    jitter[i][0] = cosf(phi);
    jitter[i][1] = sinf(phi);
    /* This offset the sample along its direction axis (reduce banding) */
    float bn = blue_noise[i][1] - 0.5f;
    CLAMP(bn, -0.499f, 0.499f); /* fix fireflies */
    jitter[i][2] = bn * num_samples_inv;
    jitter[i][3] = blue_noise[i][1];
  }

  UNUSED_VARS(bsdf_split_sum_ggx, btdf_split_sum_ggx, ltc_mag_ggx, ltc_mat_ggx, ltc_disk_integral);

  return DRW_texture_create_2d(64, 64, GPU_RGBA16F, DRW_TEX_WRAP, &jitter[0][0]);
}

BLI_INLINE int workbench_cavity_total_sample_count(const WORKBENCH_PrivateData *wpd,
                                                   const Scene *scene)
{
  return min_ii(max_ii(1, wpd->taa_sample_len) * scene->display.matcap_ssao_samples,
                CAVITY_MAX_SAMPLES);
}

void workbench_cavity_data_update(WORKBENCH_PrivateData *wpd, WORKBENCH_UBO_World *wd)
{
  View3DShading *shading = &wpd->shading;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  if (CAVITY_ENABLED(wpd)) {
    int cavity_sample_count_single_iteration = scene->display.matcap_ssao_samples;
    int cavity_sample_count_total = workbench_cavity_total_sample_count(wpd, scene);
    const int max_iter_count = cavity_sample_count_total / cavity_sample_count_single_iteration;

    int sample = wpd->taa_sample % max_iter_count;
    wd->cavity_sample_start = cavity_sample_count_single_iteration * sample;
    wd->cavity_sample_end = cavity_sample_count_single_iteration * (sample + 1);

    wd->cavity_sample_count_inv = 1.0f / (wd->cavity_sample_end - wd->cavity_sample_start);
    wd->cavity_jitter_scale = 1.0f / 64.0f;

    wd->cavity_valley_factor = shading->cavity_valley_factor;
    wd->cavity_ridge_factor = shading->cavity_ridge_factor;
    wd->cavity_attenuation = scene->display.matcap_ssao_attenuation;
    wd->cavity_distance = scene->display.matcap_ssao_distance;

    wd->curvature_ridge = 0.5f / max_ff(square_f(shading->curvature_ridge_factor), 1e-4f);
    wd->curvature_valley = 0.7f / max_ff(square_f(shading->curvature_valley_factor), 1e-4f);
  }
}

void workbench_cavity_samples_ubo_ensure(WORKBENCH_PrivateData *wpd)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  int cavity_sample_count_single_iteration = scene->display.matcap_ssao_samples;
  int cavity_sample_count = workbench_cavity_total_sample_count(wpd, scene);
  const int max_iter_count = max_ii(1, cavity_sample_count / cavity_sample_count_single_iteration);

  if (wpd->vldata->cavity_sample_count != cavity_sample_count) {
    DRW_UBO_FREE_SAFE(wpd->vldata->cavity_sample_ubo);
    DRW_TEXTURE_FREE_SAFE(wpd->vldata->cavity_jitter_tx);
  }

  if (wpd->vldata->cavity_sample_ubo == NULL) {
    float *samples = create_disk_samples(cavity_sample_count_single_iteration, max_iter_count);
    wpd->vldata->cavity_jitter_tx = create_jitter_texture(cavity_sample_count);
    /* NOTE: Uniform buffer needs to always be filled to be valid. */
    wpd->vldata->cavity_sample_ubo = GPU_uniformbuf_create_ex(
        sizeof(float[4]) * CAVITY_MAX_SAMPLES, samples, "wb_CavitySamples");
    wpd->vldata->cavity_sample_count = cavity_sample_count;
    MEM_freeN(samples);
  }
}

void workbench_cavity_cache_init(WORKBENCH_Data *data)
{
  WORKBENCH_PassList *psl = data->psl;
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  GPUShader *sh;
  DRWShadingGroup *grp;

  if (CAVITY_ENABLED(wpd)) {
    workbench_cavity_samples_ubo_ensure(wpd);

    int state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL;
    DRW_PASS_CREATE(psl->cavity_ps, state);

    sh = workbench_shader_cavity_get(SSAO_ENABLED(wpd), CURVATURE_ENABLED(wpd));

    grp = DRW_shgroup_create(sh, psl->cavity_ps);
    DRW_shgroup_uniform_texture(grp, "normalBuffer", wpd->normal_buffer_tx);
    DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);

    if (SSAO_ENABLED(wpd)) {
      DRW_shgroup_uniform_block(grp, "samples_coords", wpd->vldata->cavity_sample_ubo);
      DRW_shgroup_uniform_texture(grp, "depthBuffer", dtxl->depth);
      DRW_shgroup_uniform_texture(grp, "cavityJitter", wpd->vldata->cavity_jitter_tx);
    }
    if (CURVATURE_ENABLED(wpd)) {
      DRW_shgroup_uniform_texture(grp, "objectIdBuffer", wpd->object_id_tx);
    }
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
  else {
    psl->cavity_ps = NULL;
  }
}
