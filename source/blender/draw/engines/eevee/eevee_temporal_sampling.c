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
 * Temporal super sampling technique
 */

#include "DRW_render.h"

#include "ED_screen.h"

#include "BLI_rand.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"
#include "GPU_texture.h"

#define FILTER_CDF_TABLE_SIZE 512

static struct {
  /* Pixel filter table: Only blackman-harris for now. */
  bool inited;
  float inverted_cdf[FILTER_CDF_TABLE_SIZE];
} e_data = {false}; /* Engine data */

extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];

static float UNUSED_FUNCTION(filter_box)(float UNUSED(x))
{
  return 1.0f;
}

static float filter_blackman_harris(float x)
{
  /* Hardcoded 1px footprint [-0.5..0.5]. We resize later. */
  const float width = 1.0f;
  x = 2.0f * M_PI * (x / width + 0.5f);
  return 0.35875f - 0.48829f * cosf(x) + 0.14128f * cosf(2.0f * x) - 0.01168f * cosf(3.0f * x);
}

/* Compute cumulative distribution function of a discrete function. */
static void compute_cdf(float (*func)(float x), float cdf[FILTER_CDF_TABLE_SIZE])
{
  cdf[0] = 0.0f;
  /* Actual CDF evaluation. */
  for (int u = 0; u < FILTER_CDF_TABLE_SIZE - 1; ++u) {
    float x = (float)(u + 1) / (float)(FILTER_CDF_TABLE_SIZE - 1);
    cdf[u + 1] = cdf[u] + func(x - 0.5f); /* [-0.5..0.5]. We resize later. */
  }
  /* Normalize the CDF. */
  for (int u = 0; u < FILTER_CDF_TABLE_SIZE - 1; u++) {
    cdf[u] /= cdf[FILTER_CDF_TABLE_SIZE - 1];
  }
  /* Just to make sure. */
  cdf[FILTER_CDF_TABLE_SIZE - 1] = 1.0f;
}

static void invert_cdf(const float cdf[FILTER_CDF_TABLE_SIZE],
                       float invert_cdf[FILTER_CDF_TABLE_SIZE])
{
  for (int u = 0; u < FILTER_CDF_TABLE_SIZE; u++) {
    float x = (float)u / (float)(FILTER_CDF_TABLE_SIZE - 1);
    for (int i = 0; i < FILTER_CDF_TABLE_SIZE; ++i) {
      if (cdf[i] >= x) {
        if (i == FILTER_CDF_TABLE_SIZE - 1) {
          invert_cdf[u] = 1.0f;
        }
        else {
          float t = (x - cdf[i]) / (cdf[i + 1] - cdf[i]);
          invert_cdf[u] = ((float)i + t) / (float)(FILTER_CDF_TABLE_SIZE - 1);
        }
        break;
      }
    }
  }
}

/* Evaluate a discrete function table with linear interpolation. */
static float eval_table(float *table, float x)
{
  CLAMP(x, 0.0f, 1.0f);
  x = x * (FILTER_CDF_TABLE_SIZE - 1);

  int index = min_ii((int)(x), FILTER_CDF_TABLE_SIZE - 1);
  int nindex = min_ii(index + 1, FILTER_CDF_TABLE_SIZE - 1);
  float t = x - index;

  return (1.0f - t) * table[index] + t * table[nindex];
}

static void eevee_create_cdf_table_temporal_sampling(void)
{
  float *cdf_table = MEM_mallocN(sizeof(float) * FILTER_CDF_TABLE_SIZE, "Eevee Filter CDF table");

  float filter_width = 2.0f; /* Use a 2 pixel footprint by default. */

  {
    /* Use blackman-harris filter. */
    filter_width *= 2.0f;
    compute_cdf(filter_blackman_harris, cdf_table);
  }

  invert_cdf(cdf_table, e_data.inverted_cdf);

  /* Scale and offset table. */
  for (int i = 0; i < FILTER_CDF_TABLE_SIZE; ++i) {
    e_data.inverted_cdf[i] = (e_data.inverted_cdf[i] - 0.5f) * filter_width;
  }

  MEM_freeN(cdf_table);
  e_data.inited = true;
}

void EEVEE_temporal_sampling_offset_calc(const double ht_point[2],
                                         const float filter_size,
                                         float r_offset[2])
{
  r_offset[0] = eval_table(e_data.inverted_cdf, (float)(ht_point[0])) * filter_size;
  r_offset[1] = eval_table(e_data.inverted_cdf, (float)(ht_point[1])) * filter_size;
}

void EEVEE_temporal_sampling_matrices_calc(EEVEE_EffectsInfo *effects,
                                           float viewmat[4][4],
                                           float persmat[4][4],
                                           const double ht_point[2])
{
  const float *viewport_size = DRW_viewport_size_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  RenderData *rd = &scene->r;

  float ofs[2];
  EEVEE_temporal_sampling_offset_calc(ht_point, rd->gauss, ofs);

  window_translate_m4(
      effects->overide_winmat, persmat, ofs[0] / viewport_size[0], ofs[1] / viewport_size[1]);

  mul_m4_m4m4(effects->overide_persmat, effects->overide_winmat, viewmat);
  invert_m4_m4(effects->overide_persinv, effects->overide_persmat);
  invert_m4_m4(effects->overide_wininv, effects->overide_winmat);
}

/* Update the matrices based on the current sample.
 * Note: `DRW_MAT_PERS` and `DRW_MAT_VIEW` needs to read the original matrices. */
void EEVEE_temporal_sampling_update_matrices(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  float persmat[4][4], viewmat[4][4];
  double ht_point[2];
  double ht_offset[2] = {0.0, 0.0};
  uint ht_primes[2] = {2, 3};

  DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);
  DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
  DRW_viewport_matrix_get(effects->overide_winmat, DRW_MAT_WIN);

  BLI_halton_2d(ht_primes, ht_offset, effects->taa_current_sample - 1, ht_point);

  EEVEE_temporal_sampling_matrices_calc(effects, viewmat, persmat, ht_point);

  DRW_viewport_matrix_override_set(effects->overide_persmat, DRW_MAT_PERS);
  DRW_viewport_matrix_override_set(effects->overide_persinv, DRW_MAT_PERSINV);
  DRW_viewport_matrix_override_set(effects->overide_winmat, DRW_MAT_WIN);
  DRW_viewport_matrix_override_set(effects->overide_wininv, DRW_MAT_WININV);
}

void EEVEE_temporal_sampling_reset(EEVEE_Data *vedata)
{
  vedata->stl->effects->taa_render_sample = 1;
  vedata->stl->effects->taa_current_sample = 1;
}

int EEVEE_temporal_sampling_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  // EEVEE_FramebufferList *fbl = vedata->fbl;
  // EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  int repro_flag = 0;

  if (!e_data.inited) {
    eevee_create_cdf_table_temporal_sampling();
  }

  /* Reset for each "redraw". When rendering using ogl render,
   * we accumulate the redraw inside the drawing loop in eevee_draw_background().
   * But we do NOT accumulate between "redraw" (as in full draw manager drawloop)
   * because the opengl render already does that. */
  effects->taa_render_sample = 1;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if ((scene_eval->eevee.taa_samples != 1) || DRW_state_is_image_render()) {
    float persmat[4][4], viewmat[4][4];

    if (!DRW_state_is_image_render() && (scene_eval->eevee.flag & SCE_EEVEE_TAA_REPROJECTION)) {
      repro_flag = EFFECT_TAA_REPROJECT | EFFECT_VELOCITY_BUFFER | EFFECT_DEPTH_DOUBLE_BUFFER |
                   EFFECT_DOUBLE_BUFFER | EFFECT_POST_BUFFER;
      effects->taa_reproject_sample = ((effects->taa_reproject_sample + 1) % 16);
    }

    /* Until we support reprojection, we need to make sure
     * that the history buffer contains correct information. */
    bool view_is_valid = stl->g_data->valid_double_buffer;

    view_is_valid = view_is_valid && (stl->g_data->view_updated == false);

    if (draw_ctx->evil_C != NULL) {
      struct wmWindowManager *wm = CTX_wm_manager(draw_ctx->evil_C);
      view_is_valid = view_is_valid && (ED_screen_animation_no_scrub(wm) == NULL);
    }

    effects->taa_total_sample = scene_eval->eevee.taa_samples;
    MAX2(effects->taa_total_sample, 0);

    DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);
    DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
    view_is_valid = view_is_valid && compare_m4m4(persmat, effects->prev_drw_persmat, FLT_MIN);
    copy_m4_m4(effects->prev_drw_persmat, persmat);

    /* Prevent ghosting from probe data. */
    view_is_valid = view_is_valid && (effects->prev_drw_support == DRW_state_draw_support());
    effects->prev_drw_support = DRW_state_draw_support();

    if (((effects->taa_total_sample == 0) ||
         (effects->taa_current_sample < effects->taa_total_sample)) ||
        DRW_state_is_image_render()) {
      if (view_is_valid) {
        /* Viewport rendering updates the matrices in `eevee_draw_background` */
        if (!DRW_state_is_image_render()) {
          effects->taa_current_sample += 1;
          repro_flag = 0;
          EEVEE_temporal_sampling_update_matrices(vedata);
        }
      }
      else {
        effects->taa_current_sample = 1;
      }
    }
    else {
      effects->taa_current_sample = 1;
    }

    return repro_flag | EFFECT_TAA | EFFECT_DOUBLE_BUFFER | EFFECT_DEPTH_DOUBLE_BUFFER |
           EFFECT_POST_BUFFER;
  }

  effects->taa_current_sample = 1;

  return repro_flag;
}

void EEVEE_temporal_sampling_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & (EFFECT_TAA | EFFECT_TAA_REPROJECT)) != 0) {
    struct GPUShader *sh = EEVEE_shaders_taa_resolve_sh_get(effects->enabled_effects);

    DRW_PASS_CREATE(psl->taa_resolve, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->taa_resolve);

    DRW_shgroup_uniform_texture_ref(grp, "colorHistoryBuffer", &txl->taa_history);
    DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", &effects->source_buffer);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

    if (effects->enabled_effects & EFFECT_TAA_REPROJECT) {
      // DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
      DRW_shgroup_uniform_texture_ref(grp, "velocityBuffer", &effects->velocity_tx);
    }
    else {
      DRW_shgroup_uniform_float(grp, "alpha", &effects->taa_alpha, 1);
    }
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
  }
}

void EEVEE_temporal_sampling_draw(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & (EFFECT_TAA | EFFECT_TAA_REPROJECT)) != 0) {
    if ((effects->enabled_effects & EFFECT_TAA) != 0 && effects->taa_current_sample != 1) {
      if (DRW_state_is_image_render()) {
        /* See EEVEE_temporal_sampling_init() for more details. */
        effects->taa_alpha = 1.0f / (float)(effects->taa_render_sample);
      }
      else {
        effects->taa_alpha = 1.0f / (float)(effects->taa_current_sample);
      }

      GPU_framebuffer_bind(effects->target_buffer);
      DRW_draw_pass(psl->taa_resolve);

      /* Restore the depth from sample 1. */
      GPU_framebuffer_blit(fbl->double_buffer_depth_fb, 0, fbl->main_fb, 0, GPU_DEPTH_BIT);

      SWAP_BUFFERS_TAA();
    }
    else {
      /* Save the depth buffer for the next frame.
       * This saves us from doing anything special
       * in the other mode engines. */
      GPU_framebuffer_blit(fbl->main_fb, 0, fbl->double_buffer_depth_fb, 0, GPU_DEPTH_BIT);

      /* Do reprojection for noise reduction */
      /* TODO : do AA jitter if in only render view. */
      if (!DRW_state_is_image_render() && (effects->enabled_effects & EFFECT_TAA_REPROJECT) != 0 &&
          stl->g_data->valid_taa_history) {
        GPU_framebuffer_bind(effects->target_buffer);
        DRW_draw_pass(psl->taa_resolve);
        SWAP_BUFFERS_TAA();
      }
      else {
        struct GPUFrameBuffer *source_fb = (effects->target_buffer == fbl->main_color_fb) ?
                                               fbl->effect_color_fb :
                                               fbl->main_color_fb;
        GPU_framebuffer_blit(source_fb, 0, fbl->taa_history_color_fb, 0, GPU_COLOR_BIT);
      }
    }

    /* Make each loop count when doing a render. */
    if (DRW_state_is_image_render()) {
      effects->taa_render_sample += 1;
      effects->taa_current_sample += 1;
    }
    else {
      if ((effects->taa_total_sample == 0) ||
          (effects->taa_current_sample < effects->taa_total_sample)) {
        DRW_viewport_request_redraw();
      }
    }
  }
}
