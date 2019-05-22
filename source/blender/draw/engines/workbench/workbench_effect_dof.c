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

#include "workbench_private.h"

#include "BKE_camera.h"
#include "DEG_depsgraph_query.h"

#include "DNA_camera_types.h"

/* *********** STATIC *********** */
static struct {
  struct GPUShader *effect_dof_prepare_sh;
  struct GPUShader *effect_dof_downsample_sh;
  struct GPUShader *effect_dof_flatten_v_sh;
  struct GPUShader *effect_dof_flatten_h_sh;
  struct GPUShader *effect_dof_dilate_v_sh;
  struct GPUShader *effect_dof_dilate_h_sh;
  struct GPUShader *effect_dof_blur1_sh;
  struct GPUShader *effect_dof_blur2_sh;
  struct GPUShader *effect_dof_resolve_sh;
} e_data = {NULL};

/* Shaders */
extern char datatoc_workbench_effect_dof_frag_glsl[];

/* *********** Functions *********** */

/**
 * Transform [-1..1] square to unit circle.
 */
static void square_to_circle(float x, float y, float *r, float *T)
{
  if (x > -y) {
    if (x > y) {
      *r = x;
      *T = (M_PI / 4.0f) * (y / x);
    }
    else {
      *r = y;
      *T = (M_PI / 4.0f) * (2 - (x / y));
    }
  }
  else {
    if (x < y) {
      *r = -x;
      *T = (M_PI / 4.0f) * (4 + (y / x));
    }
    else {
      *r = -y;
      if (y != 0) {
        *T = (M_PI / 4.0f) * (6 - (x / y));
      }
      else {
        *T = 0.0f;
      }
    }
  }
}

#define KERNEL_RAD 3
#define SAMP_LEN SQUARE(KERNEL_RAD * 2 + 1)

static void workbench_dof_setup_samples(struct GPUUniformBuffer **ubo,
                                        float **data,
                                        float bokeh_sides,
                                        float bokeh_rotation,
                                        float bokeh_ratio)
{
  if (*data == NULL) {
    *data = MEM_callocN(sizeof(float) * 4 * SAMP_LEN, "workbench dof samples");
  }
  if (*ubo == NULL) {
    *ubo = DRW_uniformbuffer_create(sizeof(float) * 4 * SAMP_LEN, NULL);
  }

  float *samp = *data;
  for (int i = 0; i <= KERNEL_RAD; ++i) {
    for (int j = -KERNEL_RAD; j <= KERNEL_RAD; ++j) {
      for (int k = -KERNEL_RAD; k <= KERNEL_RAD; ++k) {
        if (abs(j) > i || abs(k) > i) {
          continue;
        }
        if (abs(j) < i && abs(k) < i) {
          continue;
        }
        float x = ((float)j) / KERNEL_RAD;
        float y = ((float)k) / KERNEL_RAD;

        float r, T;
        square_to_circle(x, y, &r, &T);
        samp[2] = r;

        /* Bokeh shape parametrisation */
        if (bokeh_sides > 1.0f) {
          float denom = T - (2.0 * M_PI / bokeh_sides) *
                                floorf((bokeh_sides * T + M_PI) / (2.0 * M_PI));
          r *= cosf(M_PI / bokeh_sides) / cosf(denom);
        }

        T += bokeh_rotation;

        samp[0] = r * cosf(T) * bokeh_ratio;
        samp[1] = r * sinf(T);
        samp += 4;
      }
    }
  }

  DRW_uniformbuffer_update(*ubo, *data);
}

void workbench_dof_engine_init(WORKBENCH_Data *vedata, Object *camera)
{
  WORKBENCH_TextureList *txl = vedata->txl;
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PrivateData *wpd = stl->g_data;
  WORKBENCH_FramebufferList *fbl = vedata->fbl;

  Camera *cam = camera != NULL ? camera->data : NULL;
  if ((wpd->shading.flag & V3D_SHADING_DEPTH_OF_FIELD) == 0 || (cam == NULL) ||
      ((cam->dof.flag & CAM_DOF_ENABLED) == 0)) {
    wpd->dof_enabled = false;
    return;
  }

  if (e_data.effect_dof_prepare_sh == NULL) {
    e_data.effect_dof_prepare_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define PREPARE\n");

    e_data.effect_dof_downsample_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define DOWNSAMPLE\n");

    e_data.effect_dof_flatten_v_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define FLATTEN_VERTICAL\n");

    e_data.effect_dof_flatten_h_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define FLATTEN_HORIZONTAL\n");

    e_data.effect_dof_dilate_v_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define DILATE_VERTICAL\n");

    e_data.effect_dof_dilate_h_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define DILATE_HORIZONTAL\n");

    e_data.effect_dof_blur1_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define BLUR1\n");

    e_data.effect_dof_blur2_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define BLUR2\n");

    e_data.effect_dof_resolve_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_effect_dof_frag_glsl, "#define RESOLVE\n");
  }

  const float *full_size = DRW_viewport_size_get();
  int size[2] = {full_size[0] / 2, full_size[1] / 2};
#if 0
  /* NOTE: We Ceil here in order to not miss any edge texel if using a NPO2 texture.  */
  int shrink_h_size[2] = {ceilf(size[0] / 8.0f), size[1]};
  int shrink_w_size[2] = {shrink_h_size[0], ceilf(size[1] / 8.0f)};
#endif

  DRW_texture_ensure_2d(
      &txl->dof_source_tx, size[0], size[1], GPU_R11F_G11F_B10F, DRW_TEX_FILTER | DRW_TEX_MIPMAP);
  DRW_texture_ensure_2d(
      &txl->coc_halfres_tx, size[0], size[1], GPU_RG8, DRW_TEX_FILTER | DRW_TEX_MIPMAP);
  wpd->dof_blur_tx = DRW_texture_pool_query_2d(
      size[0], size[1], GPU_R11F_G11F_B10F, &draw_engine_workbench_solid);
#if 0
  wpd->coc_temp_tx = DRW_texture_pool_query_2d(
      shrink_h_size[0], shrink_h_size[1], GPU_RG8, &draw_engine_workbench_solid);
  wpd->coc_tiles_tx[0] = DRW_texture_pool_query_2d(
      shrink_w_size[0], shrink_w_size[1], GPU_RG8, &draw_engine_workbench_solid);
  wpd->coc_tiles_tx[1] = DRW_texture_pool_query_2d(
      shrink_w_size[0], shrink_w_size[1], GPU_RG8, &draw_engine_workbench_solid);
#endif

  GPU_framebuffer_ensure_config(&fbl->dof_downsample_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(txl->dof_source_tx),
                                    GPU_ATTACHMENT_TEXTURE(txl->coc_halfres_tx),
                                });
#if 0
  GPU_framebuffer_ensure_config(&fbl->dof_coc_tile_h_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(wpd->coc_temp_tx),
                                });
  GPU_framebuffer_ensure_config(&fbl->dof_coc_tile_v_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(wpd->coc_tiles_tx[0]),
                                });
  GPU_framebuffer_ensure_config(&fbl->dof_coc_dilate_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(wpd->coc_tiles_tx[1]),
                                });
#endif
  GPU_framebuffer_ensure_config(&fbl->dof_blur1_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(wpd->dof_blur_tx),
                                });
  GPU_framebuffer_ensure_config(&fbl->dof_blur2_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(txl->dof_source_tx),
                                });

  {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    RegionView3D *rv3d = draw_ctx->rv3d;

    /* Parameters */
    /* TODO UI Options */
    float fstop = cam->dof.aperture_fstop;
    float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
    float focus_dist = BKE_camera_object_dof_distance(camera);
    float focal_len = cam->lens;

    /* TODO(fclem) deduplicate with eevee */
    const float scale_camera = 0.001f;
    /* we want radius here for the aperture number  */
    float aperture = 0.5f * scale_camera * focal_len / fstop;
    float focal_len_scaled = scale_camera * focal_len;
    float sensor_scaled = scale_camera * sensor;

    if (rv3d != NULL) {
      sensor_scaled *= rv3d->viewcamtexcofac[0];
    }

    wpd->dof_aperturesize = aperture * fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
    wpd->dof_distance = -focus_dist;
    wpd->dof_invsensorsize = full_size[0] / sensor_scaled;

    wpd->dof_near_far[0] = -cam->clip_start;
    wpd->dof_near_far[1] = -cam->clip_end;

    float blades = cam->dof.aperture_blades;
    float rotation = cam->dof.aperture_rotation;
    float ratio = 1.0f / cam->dof.aperture_ratio;

    if (wpd->dof_ubo == NULL || blades != wpd->dof_blades || rotation != wpd->dof_rotation ||
        ratio != wpd->dof_ratio) {
      wpd->dof_blades = blades;
      wpd->dof_rotation = rotation;
      wpd->dof_ratio = ratio;
      workbench_dof_setup_samples(&wpd->dof_ubo, &stl->dof_ubo_data, blades, rotation, ratio);
    }
  }

  wpd->dof_enabled = true;
}

void workbench_dof_create_pass(WORKBENCH_Data *vedata,
                               GPUTexture **dof_input,
                               GPUTexture *noise_tex)
{
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_TextureList *txl = vedata->txl;
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PrivateData *wpd = stl->g_data;
  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

  if (!wpd->dof_enabled) {
    return;
  }

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  psl->dof_down_ps = DRW_pass_create("DoF DownSample", DRW_STATE_WRITE_COLOR);
  psl->dof_down2_ps = DRW_pass_create("DoF DownSample", DRW_STATE_WRITE_COLOR);
  psl->dof_flatten_h_ps = DRW_pass_create("DoF Flatten Coc H", DRW_STATE_WRITE_COLOR);
  psl->dof_flatten_v_ps = DRW_pass_create("DoF Flatten Coc V", DRW_STATE_WRITE_COLOR);
  psl->dof_dilate_h_ps = DRW_pass_create("DoF Dilate Coc H", DRW_STATE_WRITE_COLOR);
  psl->dof_dilate_v_ps = DRW_pass_create("DoF Dilate Coc V", DRW_STATE_WRITE_COLOR);
  psl->dof_blur1_ps = DRW_pass_create("DoF Blur 1", DRW_STATE_WRITE_COLOR);
  psl->dof_blur2_ps = DRW_pass_create("DoF Blur 2", DRW_STATE_WRITE_COLOR);
  psl->dof_resolve_ps = DRW_pass_create("DoF Resolve", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);

  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_prepare_sh, psl->dof_down_ps);
    DRW_shgroup_uniform_texture_ref(grp, "sceneColorTex", dof_input);
    DRW_shgroup_uniform_texture(grp, "sceneDepthTex", dtxl->depth);
    DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
    DRW_shgroup_uniform_vec3(grp, "dofParams", &wpd->dof_aperturesize, 1);
    DRW_shgroup_uniform_vec2(grp, "nearFar", wpd->dof_near_far, 1);
    DRW_shgroup_call(grp, quad, NULL);
  }

  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_downsample_sh, psl->dof_down2_ps);
    DRW_shgroup_uniform_texture(grp, "sceneColorTex", txl->dof_source_tx);
    DRW_shgroup_uniform_texture(grp, "inputCocTex", txl->coc_halfres_tx);
    DRW_shgroup_call(grp, quad, NULL);
  }
#if 0
  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_flatten_h_sh,
                                              psl->dof_flatten_h_ps);
    DRW_shgroup_uniform_texture(grp, "inputCocTex", txl->coc_halfres_tx);
    DRW_shgroup_call(grp, quad, NULL);
  }
  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_flatten_v_sh,
                                              psl->dof_flatten_v_ps);
    DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_temp_tx);
    DRW_shgroup_call(grp, quad, NULL);
  }
  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_dilate_v_sh, psl->dof_dilate_v_ps);
    DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_tiles_tx[0]);
    DRW_shgroup_call(grp, quad, NULL);
  }
  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_dilate_h_sh, psl->dof_dilate_h_ps);
    DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_tiles_tx[1]);
    DRW_shgroup_call(grp, quad, NULL);
  }
#endif
  {
    float offset = stl->effects->jitter_index /
                   (float)workbench_taa_calculate_num_iterations(vedata);
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_blur1_sh, psl->dof_blur1_ps);
    DRW_shgroup_uniform_block(grp, "dofSamplesBlock", wpd->dof_ubo);
    DRW_shgroup_uniform_texture(grp, "noiseTex", noise_tex);
    DRW_shgroup_uniform_texture(grp, "inputCocTex", txl->coc_halfres_tx);
    DRW_shgroup_uniform_texture(grp, "halfResColorTex", txl->dof_source_tx);
    DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
    DRW_shgroup_uniform_float_copy(grp, "noiseOffset", offset);
    DRW_shgroup_call(grp, quad, NULL);
  }
  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_blur2_sh, psl->dof_blur2_ps);
    DRW_shgroup_uniform_texture(grp, "inputCocTex", txl->coc_halfres_tx);
    DRW_shgroup_uniform_texture(grp, "blurTex", wpd->dof_blur_tx);
    DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
    DRW_shgroup_call(grp, quad, NULL);
  }
  {
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_resolve_sh, psl->dof_resolve_ps);
    DRW_shgroup_uniform_texture(grp, "halfResColorTex", txl->dof_source_tx);
    DRW_shgroup_uniform_texture(grp, "sceneDepthTex", dtxl->depth);
    DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
    DRW_shgroup_uniform_vec3(grp, "dofParams", &wpd->dof_aperturesize, 1);
    DRW_shgroup_uniform_vec2(grp, "nearFar", wpd->dof_near_far, 1);
    DRW_shgroup_call(grp, quad, NULL);
  }
}

void workbench_dof_engine_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_prepare_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_downsample_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_flatten_v_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_flatten_h_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_dilate_v_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_dilate_h_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_blur1_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_blur2_sh);
  DRW_SHADER_FREE_SAFE(e_data.effect_dof_resolve_sh);
}

static void workbench_dof_downsample_level(void *userData, int UNUSED(level))
{
  WORKBENCH_PassList *psl = (WORKBENCH_PassList *)userData;
  DRW_draw_pass(psl->dof_down2_ps);
}

void workbench_dof_draw_pass(WORKBENCH_Data *vedata)
{
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_PrivateData *wpd = stl->g_data;

  if (!wpd->dof_enabled) {
    return;
  }

  DRW_stats_group_start("Depth Of Field");

  GPU_framebuffer_bind(fbl->dof_downsample_fb);
  DRW_draw_pass(psl->dof_down_ps);

  GPU_framebuffer_recursive_downsample(
      fbl->dof_downsample_fb, 2, workbench_dof_downsample_level, psl);

#if 0
  GPU_framebuffer_bind(fbl->dof_coc_tile_h_fb);
  DRW_draw_pass(psl->dof_flatten_h_ps);

  GPU_framebuffer_bind(fbl->dof_coc_tile_v_fb);
  DRW_draw_pass(psl->dof_flatten_v_ps);

  GPU_framebuffer_bind(fbl->dof_coc_dilate_fb);
  DRW_draw_pass(psl->dof_dilate_v_ps);

  GPU_framebuffer_bind(fbl->dof_coc_tile_v_fb);
  DRW_draw_pass(psl->dof_dilate_h_ps);
#endif

  GPU_framebuffer_bind(fbl->dof_blur1_fb);
  DRW_draw_pass(psl->dof_blur1_ps);

  GPU_framebuffer_bind(fbl->dof_blur2_fb);
  DRW_draw_pass(psl->dof_blur2_ps);

  GPU_framebuffer_bind(fbl->color_only_fb);
  DRW_draw_pass(psl->dof_resolve_ps);

  DRW_stats_group_end();
}
