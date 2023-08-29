/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Anti-Aliasing:
 *
 * We use SMAA (Smart Morphological Anti-Aliasing) as a fast antialiasing solution.
 *
 * If the viewport stays static, the engine ask for multiple redraw and will progressively
 * converge to a much more accurate image without aliasing.
 * We call this one TAA (Temporal Anti-Aliasing).
 *
 * This is done using an accumulation buffer and a final pass that will output the final color
 * to the scene buffer. We softly blend between SMAA and TAA to avoid really harsh transitions.
 */

#include "ED_screen.hh"

#include "BLI_jitter_2d.h"

#include "BLI_smaa_textures.h"

#include "workbench_private.h"

static struct {
  bool init;
  float jitter_5[5][2];
  float jitter_8[8][2];
  float jitter_11[11][2];
  float jitter_16[16][2];
  float jitter_32[32][2];
} e_data = {false};

static void workbench_taa_jitter_init_order(float (*table)[2], int num)
{
  BLI_jitter_init(table, num);

  /* find closest element to center */
  int closest_index = 0;
  float closest_squared_distance = 1.0f;

  for (int index = 0; index < num; index++) {
    const float squared_dist = square_f(table[index][0]) + square_f(table[index][1]);
    if (squared_dist < closest_squared_distance) {
      closest_squared_distance = squared_dist;
      closest_index = index;
    }
  }

  float closest_sample[2];
  copy_v2_v2(closest_sample, table[closest_index]);
  for (int index = 0; index < num; index++) {
    /* move jitter table so that closest sample is in center */
    sub_v2_v2(table[index], closest_sample);
    for (int i = 0; i < 2; i++) {
      /* Avoid samples outside range (wrap around). */
      table[index][i] = fmodf(table[index][i] + 0.5f, 1.0f);
      /* Recenter the distribution[-1..1]. */
      table[index][i] = table[index][i] * 2.0f - 1.0f;
    }
  }

  /* swap center sample to the start of the table */
  if (closest_index != 0) {
    swap_v2_v2(table[0], table[closest_index]);
  }

  /* Sort list based on farthest distance with previous. */
  for (int i = 0; i < num - 2; i++) {
    float f_squared_dist = 0.0;
    int f_index = i;
    for (int j = i + 1; j < num; j++) {
      const float squared_dist = square_f(table[i][0] - table[j][0]) +
                                 square_f(table[i][1] - table[j][1]);
      if (squared_dist > f_squared_dist) {
        f_squared_dist = squared_dist;
        f_index = j;
      }
    }
    swap_v2_v2(table[i + 1], table[f_index]);
  }
}

static void workbench_taa_jitter_init()
{
  if (e_data.init == false) {
    e_data.init = true;
    workbench_taa_jitter_init_order(e_data.jitter_5, 5);
    workbench_taa_jitter_init_order(e_data.jitter_8, 8);
    workbench_taa_jitter_init_order(e_data.jitter_11, 11);
    workbench_taa_jitter_init_order(e_data.jitter_16, 16);
    workbench_taa_jitter_init_order(e_data.jitter_32, 32);
  }
}

int workbench_antialiasing_sample_count_get(WORKBENCH_PrivateData *wpd)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;

  if (wpd->is_navigating || wpd->is_playback) {
    /* Only draw using SMAA or no AA when navigating. */
    return min_ii(wpd->preferences->viewport_aa, 1);
  }
  if (DRW_state_is_image_render()) {
    if (draw_ctx->v3d) {
      return scene->display.viewport_aa;
    }

    return scene->display.render_aa;
  }

  return wpd->preferences->viewport_aa;
}

void workbench_antialiasing_view_updated(WORKBENCH_Data *vedata)
{
  WORKBENCH_StorageList *stl = vedata->stl;
  if (stl && stl->wpd) {
    stl->wpd->view_updated = true;
  }
}

/* This function checks if the overlay engine should need center in front depth's.
 * When that is the case the in front depth are stored and restored. Otherwise it
 * will be filled with the current sample data. */
static bool workbench_in_front_history_needed(WORKBENCH_Data *vedata)
{
  WORKBENCH_StorageList *stl = vedata->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;

  if (!v3d || (v3d->flag2 & V3D_HIDE_OVERLAYS)) {
    return false;
  }

  if (stl->wpd->is_playback) {
    return false;
  }

  return true;
}

void workbench_antialiasing_engine_init(WORKBENCH_Data *vedata)
{
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_TextureList *txl = vedata->txl;
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  DrawEngineType *owner = (DrawEngineType *)&workbench_antialiasing_engine_init;

  wpd->view = nullptr;

  /* Reset complete drawing when navigating or during viewport playback or when
   * leaving one of those states. In case of multires modifier the navigation
   * mesh differs from the viewport mesh, so we need to be sure to restart. */
  if (wpd->taa_sample != 0) {
    if (wpd->is_navigating || wpd->is_playback) {
      wpd->taa_sample = 0;
      wpd->reset_next_sample = true;
    }
    else if (wpd->reset_next_sample) {
      wpd->taa_sample = 0;
      wpd->reset_next_sample = false;
    }
  }

  /* Reset the TAA when we have already draw a sample, but the sample count differs from previous
   * time. This removes render artifacts when the viewport anti-aliasing in the user preferences is
   * set to a lower value. */
  if (wpd->taa_sample_len != wpd->taa_sample_len_previous) {
    wpd->taa_sample = 0;
    wpd->taa_sample_len_previous = wpd->taa_sample_len;
  }

  if (wpd->view_updated) {
    wpd->taa_sample = 0;
    wpd->view_updated = false;
  }

  if (wpd->taa_sample_len > 0 && wpd->valid_history == false) {
    wpd->taa_sample = 0;
  }

  {
    float persmat[4][4];
    DRW_view_persmat_get(nullptr, persmat, false);
    if (!equals_m4m4(persmat, wpd->last_mat)) {
      copy_m4_m4(wpd->last_mat, persmat);
      wpd->taa_sample = 0;
    }
  }

  if (wpd->taa_sample_len > 0) {
    workbench_taa_jitter_init();
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    DRW_texture_ensure_fullscreen_2d_ex(
        &txl->history_buffer_tx, GPU_RGBA16F, usage, DRW_TEX_FILTER);
    DRW_texture_ensure_fullscreen_2d_ex(
        &txl->depth_buffer_tx, GPU_DEPTH24_STENCIL8, usage, DRWTextureFlag(0));
    const bool in_front_history = workbench_in_front_history_needed(vedata);
    if (in_front_history) {
      DRW_texture_ensure_fullscreen_2d_ex(
          &txl->depth_buffer_in_front_tx, GPU_DEPTH24_STENCIL8, usage, DRWTextureFlag(0));
    }
    else {
      DRW_TEXTURE_FREE_SAFE(txl->depth_buffer_in_front_tx);
    }

    wpd->smaa_edge_tx = DRW_texture_pool_query_fullscreen_ex(GPU_RG8, usage, owner);
    wpd->smaa_weight_tx = DRW_texture_pool_query_fullscreen_ex(GPU_RGBA8, usage, owner);

    GPU_framebuffer_ensure_config(&fbl->antialiasing_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(txl->depth_buffer_tx),
                                      GPU_ATTACHMENT_TEXTURE(txl->history_buffer_tx),
                                  });
    if (in_front_history) {
      GPU_framebuffer_ensure_config(&fbl->antialiasing_in_front_fb,
                                    {
                                        GPU_ATTACHMENT_TEXTURE(txl->depth_buffer_in_front_tx),
                                    });
    }

    GPU_framebuffer_ensure_config(&fbl->smaa_edge_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(wpd->smaa_edge_tx),
                                  });

    GPU_framebuffer_ensure_config(&fbl->smaa_weight_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(wpd->smaa_weight_tx),
                                  });

    /* TODO: could be shared for all viewports. */
    if (txl->smaa_search_tx == nullptr) {
      txl->smaa_search_tx = GPU_texture_create_2d(
          "smaa_search", SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1, GPU_R8, usage, nullptr);
      GPU_texture_update(txl->smaa_search_tx, GPU_DATA_UBYTE, searchTexBytes);

      txl->smaa_area_tx = GPU_texture_create_2d(
          "smaa_area", AREATEX_WIDTH, AREATEX_HEIGHT, 1, GPU_RG8, usage, nullptr);
      GPU_texture_update(txl->smaa_area_tx, GPU_DATA_UBYTE, areaTexBytes);

      GPU_texture_filter_mode(txl->smaa_search_tx, true);
      GPU_texture_filter_mode(txl->smaa_area_tx, true);
    }
  }
  else {
    /* Cleanup */
    DRW_TEXTURE_FREE_SAFE(txl->history_buffer_tx);
    DRW_TEXTURE_FREE_SAFE(txl->depth_buffer_tx);
    DRW_TEXTURE_FREE_SAFE(txl->depth_buffer_in_front_tx);
    DRW_TEXTURE_FREE_SAFE(txl->smaa_search_tx);
    DRW_TEXTURE_FREE_SAFE(txl->smaa_area_tx);
  }
}

static float filter_blackman_harris(float x, const float width)
{
  if (x > width * 0.5f) {
    return 0.0f;
  }
  x = 2.0f * M_PI * clamp_f((x / width + 0.5f), 0.0f, 1.0f);
  return 0.35875f - 0.48829f * cosf(x) + 0.14128f * cosf(2.0f * x) - 0.01168f * cosf(3.0f * x);
}

/* Compute weights for the 3x3 neighborhood using a 1.5px filter. */
static void workbench_antialiasing_weights_get(const float offset[2],
                                               float r_weights[9],
                                               float *r_weight_sum)
{
  /* NOTE: If filter width is bigger than 2.0f, then we need to sample more neighborhood. */
  const float filter_width = 2.0f;
  *r_weight_sum = 0.0f;
  int i = 0;
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++, i++) {
      float sample_co[2] = {float(x), float(y)};
      sub_v2_v2(sample_co, offset);
      float r = len_v2(sample_co);
      /* fclem: is radial distance ok here? */
      float weight = filter_blackman_harris(r, filter_width);
      *r_weight_sum += weight;
      r_weights[i] = weight;
    }
  }
}

void workbench_antialiasing_cache_init(WORKBENCH_Data *vedata)
{
  WORKBENCH_TextureList *txl = vedata->txl;
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  WORKBENCH_PassList *psl = vedata->psl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DRWShadingGroup *grp = nullptr;

  if (wpd->taa_sample_len == 0) {
    return;
  }

  {
    DRW_PASS_CREATE(psl->aa_accum_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    DRW_PASS_INSTANCE_CREATE(psl->aa_accum_replace_ps, psl->aa_accum_ps, DRW_STATE_WRITE_COLOR);

    GPUShader *shader = workbench_shader_antialiasing_accumulation_get();
    grp = DRW_shgroup_create(shader, psl->aa_accum_ps);
    DRW_shgroup_uniform_texture_ex(
        grp, "colorBuffer", dtxl->color, GPUSamplerState::default_sampler());
    DRW_shgroup_uniform_float(grp, "samplesWeights", wpd->taa_weights, 9);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }

  const float *size = DRW_viewport_size_get();
  const float *sizeinv = DRW_viewport_invert_size_get();
  const float metrics[4] = {sizeinv[0], sizeinv[1], size[0], size[1]};

  {
    /* Stage 1: Edge detection. */
    DRW_PASS_CREATE(psl->aa_edge_ps, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = workbench_shader_antialiasing_get(0);
    grp = DRW_shgroup_create(sh, psl->aa_edge_ps);
    DRW_shgroup_uniform_texture(grp, "colorTex", txl->history_buffer_tx);
    DRW_shgroup_uniform_vec4_copy(grp, "viewportMetrics", metrics);

    DRW_shgroup_clear_framebuffer(grp, GPU_COLOR_BIT, 0, 0, 0, 0, 0.0f, 0x0);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
  {
    /* Stage 2: Blend Weight/Coord. */
    DRW_PASS_CREATE(psl->aa_weight_ps, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = workbench_shader_antialiasing_get(1);
    grp = DRW_shgroup_create(sh, psl->aa_weight_ps);
    DRW_shgroup_uniform_texture(grp, "edgesTex", wpd->smaa_edge_tx);
    DRW_shgroup_uniform_texture(grp, "areaTex", txl->smaa_area_tx);
    DRW_shgroup_uniform_texture(grp, "searchTex", txl->smaa_search_tx);
    DRW_shgroup_uniform_vec4_copy(grp, "viewportMetrics", metrics);

    DRW_shgroup_clear_framebuffer(grp, GPU_COLOR_BIT, 0, 0, 0, 0, 0.0f, 0x0);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
  {
    /* Stage 3: Resolve. */
    DRW_PASS_CREATE(psl->aa_resolve_ps, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = workbench_shader_antialiasing_get(2);
    grp = DRW_shgroup_create(sh, psl->aa_resolve_ps);
    DRW_shgroup_uniform_texture(grp, "blendTex", wpd->smaa_weight_tx);
    DRW_shgroup_uniform_texture(grp, "colorTex", txl->history_buffer_tx);
    DRW_shgroup_uniform_vec4_copy(grp, "viewportMetrics", metrics);
    DRW_shgroup_uniform_float(grp, "mixFactor", &wpd->smaa_mix_factor, 1);
    DRW_shgroup_uniform_float(grp, "taaAccumulatedWeight", &wpd->taa_weight_accum, 1);

    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
}

bool workbench_antialiasing_setup(WORKBENCH_Data *vedata)
{
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;

  if (wpd->taa_sample_len == 0) {
    /* AA disabled. */
    return true;
  }

  if (wpd->taa_sample >= wpd->taa_sample_len) {
    /* TAA accumulation has finish. Just copy the result back */
    return false;
  }

  const float *viewport_size = DRW_viewport_size_get();
  const DRWView *default_view = DRW_view_default_get();
  float *transform_offset;

  switch (wpd->taa_sample_len) {
    default:
    case 5:
      transform_offset = e_data.jitter_5[min_ii(wpd->taa_sample, 5)];
      break;
    case 8:
      transform_offset = e_data.jitter_8[min_ii(wpd->taa_sample, 8)];
      break;
    case 11:
      transform_offset = e_data.jitter_11[min_ii(wpd->taa_sample, 11)];
      break;
    case 16:
      transform_offset = e_data.jitter_16[min_ii(wpd->taa_sample, 16)];
      break;
    case 32:
      transform_offset = e_data.jitter_32[min_ii(wpd->taa_sample, 32)];
      break;
  }

  workbench_antialiasing_weights_get(transform_offset, wpd->taa_weights, &wpd->taa_weights_sum);

  /* construct new matrices from transform delta */
  float winmat[4][4], viewmat[4][4], persmat[4][4];
  DRW_view_winmat_get(default_view, winmat, false);
  DRW_view_viewmat_get(default_view, viewmat, false);
  DRW_view_persmat_get(default_view, persmat, false);

  window_translate_m4(winmat,
                      persmat,
                      transform_offset[0] / viewport_size[0],
                      transform_offset[1] / viewport_size[1]);

  if (wpd->view) {
    /* When rendering just update the view. This avoids recomputing the culling. */
    DRW_view_update_sub(wpd->view, viewmat, winmat);
  }
  else {
    /* TAA is not making a big change to the matrices.
     * Reuse the main view culling by creating a sub-view. */
    wpd->view = DRW_view_create_sub(default_view, viewmat, winmat);
  }
  DRW_view_set_active(wpd->view);
  return true;
}

void workbench_antialiasing_draw_pass(WORKBENCH_Data *vedata)
{
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_TextureList *txl = vedata->txl;
  WORKBENCH_PassList *psl = vedata->psl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (wpd->taa_sample_len == 0) {
    /* AA disabled. */
    /* Just set sample to 1 to avoid rendering indefinitely. */
    wpd->taa_sample = 1;
    wpd->valid_history = false;
    return;
  }

  /**
   * We always do SMAA on top of TAA accumulation, unless the number of samples of TAA is already
   * high. This ensure a smoother transition.
   * If TAA accumulation is finished, we only blit the result.
   */

  const bool last_sample = wpd->taa_sample + 1 == wpd->taa_sample_len;
  const bool taa_finished = wpd->taa_sample >= wpd->taa_sample_len;
  if (wpd->taa_sample == 0) {
    wpd->taa_weight_accum = wpd->taa_weights_sum;
    wpd->valid_history = true;

    GPU_framebuffer_bind(fbl->antialiasing_fb);
    DRW_draw_pass(psl->aa_accum_replace_ps);
    /* In playback mode, we are sure the next redraw will not use the same viewmatrix.
     * In this case no need to save the depth buffer. */
    if (!wpd->is_playback) {
      GPU_texture_copy(txl->depth_buffer_tx, dtxl->depth);
    }
    if (workbench_in_front_history_needed(vedata)) {
      GPU_texture_copy(txl->depth_buffer_in_front_tx, dtxl->depth_in_front);
    }
  }
  else {
    if (!taa_finished) {
      /* Accumulate result to the TAA buffer. */
      GPU_framebuffer_bind(fbl->antialiasing_fb);
      DRW_draw_pass(psl->aa_accum_ps);
      wpd->taa_weight_accum += wpd->taa_weights_sum;
    }
    /* Copy back the saved depth buffer for correct overlays. */
    GPU_texture_copy(dtxl->depth, txl->depth_buffer_tx);
    if (workbench_in_front_history_needed(vedata)) {
      GPU_texture_copy(dtxl->depth_in_front, txl->depth_buffer_in_front_tx);
    }
  }

  if (!DRW_state_is_image_render() || last_sample) {
    /* After a certain point SMAA is no longer necessary. */
    wpd->smaa_mix_factor = 1.0f - clamp_f(wpd->taa_sample / 4.0f, 0.0f, 1.0f);

    if (wpd->smaa_mix_factor > 0.0f) {
      GPU_framebuffer_bind(fbl->smaa_edge_fb);
      DRW_draw_pass(psl->aa_edge_ps);

      GPU_framebuffer_bind(fbl->smaa_weight_fb);
      DRW_draw_pass(psl->aa_weight_ps);
    }

    GPU_framebuffer_bind(dfbl->default_fb);
    DRW_draw_pass(psl->aa_resolve_ps);
  }

  if (!taa_finished) {
    wpd->taa_sample++;
  }

  if (!DRW_state_is_image_render() && wpd->taa_sample < wpd->taa_sample_len) {
    DRW_viewport_request_redraw();
  }
}
