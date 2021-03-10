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
 * Depth of field post process effect.
 *
 * There are 2 methods to achieve this effect.
 * - The first uses projection matrix offsetting and sample accumulation to give reference quality
 *   depth of field. But this needs many samples to hide the under-sampling.
 * - The second one is a post-processing based one. It follows the implementation described in
 *   the presentation "Life of a Bokeh - Siggraph 2018" from Guillaume Abadie. There are some
 *   difference with our actual implementation that prioritize quality.
 */

#include "DRW_render.h"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_camera.h"

#include "BLI_string_utils.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"
#include "eevee_private.h"

#define CAMERA_JITTER_RING_DENSITY 6

static float coc_radius_from_camera_depth(bool is_ortho, EEVEE_EffectsInfo *fx, float camera_depth)
{
  float multiplier = fx->dof_coc_params[0];
  float bias = fx->dof_coc_params[1];
  if (multiplier == 0.0f || bias == 0.0f) {
    return 0.0f;
  }
  if (is_ortho) {
    return (camera_depth + multiplier / bias) * multiplier;
  }
  return multiplier / camera_depth - bias;
}

static float polygon_sides_length(float sides_count)
{
  return 2.0 * sin(M_PI / sides_count);
}

/* Returns intersection ratio between the radius edge at theta and the polygon edge.
 * Start first corners at theta == 0. */
static float circle_to_polygon_radius(float sides_count, float theta)
{
  /* From Graphics Gems from CryENGINE 3 (Siggraph 2013) by Tiago Sousa (slide 36). */
  float side_angle = (2.0f * M_PI) / sides_count;
  return cosf(side_angle * 0.5f) /
         cosf(theta - side_angle * floorf((sides_count * theta + M_PI) / (2.0f * M_PI)));
}

/* Remap input angle to have homogenous spacing of points along a polygon edge.
 * Expect theta to be in [0..2pi] range. */
static float circle_to_polygon_angle(float sides_count, float theta)
{
  float side_angle = (2.0f * M_PI) / sides_count;
  float halfside_angle = side_angle * 0.5f;
  float side = floorf(theta / side_angle);
  /* Length of segment from center to the middle of polygon side. */
  float adjacent = circle_to_polygon_radius(sides_count, 0.0f);

  /* This is the relative position of the sample on the polygon half side. */
  float local_theta = theta - side * side_angle;
  float ratio = (local_theta - halfside_angle) / halfside_angle;

  float halfside_len = polygon_sides_length(sides_count) * 0.5f;
  float opposite = ratio * halfside_len;

  /* NOTE: atan(y_over_x) has output range [-M_PI_2..M_PI_2]. */
  float final_local_theta = atanf(opposite / adjacent);

  return side * side_angle + final_local_theta;
}

static int dof_jitter_total_sample_count(int ring_density, int ring_count)
{
  return ((ring_count * ring_count + ring_count) / 2) * ring_density + 1;
}

bool EEVEE_depth_of_field_jitter_get(EEVEE_EffectsInfo *fx,
                                     float r_jitter[2],
                                     float *r_focus_distance)
{
  if (fx->dof_jitter_radius == 0.0f) {
    return false;
  }

  int ring_density = CAMERA_JITTER_RING_DENSITY;
  int ring_count = fx->dof_jitter_ring_count;
  int sample_count = dof_jitter_total_sample_count(ring_density, ring_count);

  int s = fx->taa_current_sample - 1;

  int ring = 0;
  int ring_sample_count = 1;
  int ring_sample = 1;

  s = s * (ring_density - 1);
  s = s % sample_count;

  int samples_passed = 1;
  while (s >= samples_passed) {
    ring++;
    ring_sample_count = ring * ring_density;
    ring_sample = s - samples_passed;
    ring_sample = (ring_sample + 1) % ring_sample_count;
    samples_passed += ring_sample_count;
  }

  r_jitter[0] = (float)ring / ring_count;
  r_jitter[1] = (float)ring_sample / ring_sample_count;

  {
    /* Bokeh shape parameterization. */
    float r = r_jitter[0];
    float T = r_jitter[1] * 2.0f * M_PI;

    if (fx->dof_jitter_blades >= 3.0f) {
      T = circle_to_polygon_angle(fx->dof_jitter_blades, T);
      r *= circle_to_polygon_radius(fx->dof_jitter_blades, T);
    }

    T += fx->dof_bokeh_rotation;

    r_jitter[0] = r * cosf(T);
    r_jitter[1] = r * sinf(T);

    mul_v2_v2(r_jitter, fx->dof_bokeh_aniso);
  }

  mul_v2_fl(r_jitter, fx->dof_jitter_radius);

  *r_focus_distance = fx->dof_jitter_focus;
  return true;
}

int EEVEE_depth_of_field_sample_count_get(EEVEE_EffectsInfo *effects,
                                          int sample_count,
                                          int *r_ring_count)
{
  if (effects->dof_jitter_radius == 0.0f) {
    if (r_ring_count != NULL) {
      *r_ring_count = 0;
    }
    return 1;
  }

  if (sample_count == TAA_MAX_SAMPLE) {
    /* Special case for viewport continuous rendering. We clamp to a max sample to avoid the
     * jittered dof never converging. */
    sample_count = 1024;
  }
  /* Inversion of dof_jitter_total_sample_count. */
  float x = 2.0f * (sample_count - 1.0f) / CAMERA_JITTER_RING_DENSITY;
  /* Solving polynomial. We only search positive solution. */
  float discriminant = 1.0f + 4.0f * x;
  int ring_count = ceilf(0.5f * (sqrt(discriminant) - 1.0f));

  sample_count = dof_jitter_total_sample_count(CAMERA_JITTER_RING_DENSITY, ring_count);

  if (r_ring_count != NULL) {
    *r_ring_count = ring_count;
  }
  return sample_count;
}

int EEVEE_depth_of_field_init(EEVEE_ViewLayerData *UNUSED(sldata),
                              EEVEE_Data *vedata,
                              Object *camera)
{
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  Camera *cam = (camera != NULL) ? camera->data : NULL;

  if (cam && (cam->dof.flag & CAM_DOF_ENABLED)) {
    RegionView3D *rv3d = draw_ctx->rv3d;
    const float *viewport_size = DRW_viewport_size_get();

    effects->dof_hq_slight_focus = (scene_eval->eevee.flag & SCE_EEVEE_DOF_HQ_SLIGHT_FOCUS) != 0;

    /* Retrieve Near and Far distance */
    effects->dof_coc_near_dist = -cam->clip_start;
    effects->dof_coc_far_dist = -cam->clip_end;

    /* Parameters */
    bool is_ortho = cam->type == CAM_ORTHO;
    float fstop = cam->dof.aperture_fstop;
    float blades = cam->dof.aperture_blades;
    float rotation = cam->dof.aperture_rotation;
    float ratio = 1.0f / max_ff(cam->dof.aperture_ratio, 0.00001f);
    float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
    float focus_dist = BKE_camera_object_dof_distance(camera);
    float focal_len = cam->lens;

    if (is_ortho) {
      /* (fclem) A bit of black magic here. I don't know if this is correct. */
      fstop *= 1.3f;
      focal_len = 1.0f;
      sensor = cam->ortho_scale;
    }

    const float scale_camera = (is_ortho) ? 1.0 : 0.001f;
    /* we want radius here for the aperture number  */
    float aperture = 0.5f * scale_camera * focal_len / fstop;
    float focal_len_scaled = scale_camera * focal_len;
    float sensor_scaled = scale_camera * sensor;

    if (rv3d != NULL) {
      sensor_scaled *= rv3d->viewcamtexcofac[0];
    }

    if (ratio > 1.0) {
      /* If ratio is scaling the bokeh outwards, we scale the aperture so that the gather
       * kernel size will encompass the maximum axis. */
      aperture *= ratio;
    }

    effects->dof_coc_params[1] = -aperture *
                                 fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
    /* FIXME(fclem) This is broken for vertically fit sensor. */
    effects->dof_coc_params[1] *= viewport_size[0] / sensor_scaled;

    if ((scene_eval->eevee.flag & SCE_EEVEE_DOF_JITTER) != 0) {
      effects->dof_jitter_radius = effects->dof_coc_params[1];
      effects->dof_jitter_focus = focus_dist;
      effects->dof_jitter_blades = blades;

      int sample_count = EEVEE_temporal_sampling_sample_count_get(scene_eval, stl);
      sample_count = EEVEE_depth_of_field_sample_count_get(
          effects, sample_count, &effects->dof_jitter_ring_count);

      if (effects->dof_jitter_ring_count == 0) {
        effects->dof_jitter_radius = 0.0f;
      }
      else {
        /* Compute a minimal overblur radius to fill the gaps between the samples.
         * This is just the simplified form of dividing the area of the bokeh
         * by the number of samples. */
        float minimal_overblur = 1.0f / sqrtf(sample_count);
        float user_overblur = scene_eval->eevee.bokeh_overblur / 100.0f;

        minimal_overblur *= effects->dof_coc_params[1];
        user_overblur *= effects->dof_coc_params[1];

        effects->dof_coc_params[1] = minimal_overblur + user_overblur;
        /* Avoid dilating the shape. Over-blur only soften. */
        effects->dof_jitter_radius -= minimal_overblur + user_overblur * 0.5f;
      }
    }
    else {
      effects->dof_jitter_radius = 0.0f;
    }

    if (is_ortho) {
      /* (fclem) A bit of black magic here. Needed to match cycles. */
      effects->dof_coc_params[1] *= 0.225;
    }

    effects->dof_coc_params[0] = -focus_dist * effects->dof_coc_params[1];

    effects->dof_bokeh_blades = blades;
    effects->dof_bokeh_rotation = rotation;
    effects->dof_bokeh_aniso[0] = min_ff(ratio, 1.0f);
    effects->dof_bokeh_aniso[1] = min_ff(1.0f / ratio, 1.0f);
    effects->dof_bokeh_max_size = scene_eval->eevee.bokeh_max_size;

    copy_v2_v2(effects->dof_bokeh_aniso_inv, effects->dof_bokeh_aniso);
    invert_v2(effects->dof_bokeh_aniso_inv);

    effects->dof_scatter_color_threshold = scene_eval->eevee.bokeh_threshold;
    effects->dof_scatter_neighbor_max_color = scene_eval->eevee.bokeh_neighbor_max;
    effects->dof_denoise_factor = clamp_f(scene_eval->eevee.bokeh_denoise_fac, 0.0f, 1.0f);

    float max_abs_fg_coc, max_abs_bg_coc;
    if (is_ortho) {
      max_abs_fg_coc = fabsf(coc_radius_from_camera_depth(true, effects, -cam->clip_start));
      max_abs_bg_coc = fabsf(coc_radius_from_camera_depth(true, effects, -cam->clip_end));
    }
    else {
      max_abs_fg_coc = fabsf(coc_radius_from_camera_depth(false, effects, -cam->clip_start));
      /* Background is at infinity so maximum CoC is the limit of the function at -inf. */
      max_abs_bg_coc = fabsf(effects->dof_coc_params[1]);
    }

    float max_coc = max_ff(max_abs_bg_coc, max_abs_fg_coc);
    /* Clamp with user defined max. */
    effects->dof_fx_max_coc = min_ff(scene_eval->eevee.bokeh_max_size, max_coc);

    if (effects->dof_fx_max_coc < 0.5f) {
      return 0;
    }

    return EFFECT_DOF | EFFECT_POST_BUFFER;
  }

  effects->dof_jitter_radius = 0.0f;

  /* Cleanup to release memory */
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_setup_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_flatten_tiles_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_dilate_tiles_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_reduce_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_reduce_copy_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_gather_fg_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_gather_bg_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_scatter_bg_fb);
  DRW_TEXTURE_FREE_SAFE(txl->dof_reduced_color);
  DRW_TEXTURE_FREE_SAFE(txl->dof_reduced_coc);

  return 0;
}

#define WITH_FILTERING (GPU_SAMPLER_MIPMAP | GPU_SAMPLER_FILTER)
#define NO_FILTERING GPU_SAMPLER_MIPMAP
#define COLOR_FORMAT fx->dof_color_format
#define FG_TILE_FORMAT GPU_RGBA16F
#define BG_TILE_FORMAT GPU_R11F_G11F_B10F

/**
 * Create bokeh texture.
 **/
static void dof_bokeh_pass_init(EEVEE_FramebufferList *fbl,
                                EEVEE_PassList *psl,
                                EEVEE_EffectsInfo *fx)
{
  if ((fx->dof_bokeh_aniso[0] == 1.0f) && (fx->dof_bokeh_aniso[1] == 1.0f) &&
      (fx->dof_bokeh_blades == 0.0)) {
    fx->dof_bokeh_gather_lut_tx = NULL;
    fx->dof_bokeh_scatter_lut_tx = NULL;
    fx->dof_bokeh_resolve_lut_tx = NULL;
    return;
  }

  void *owner = (void *)&EEVEE_depth_of_field_init;
  int res[2] = {DOF_BOKEH_LUT_SIZE, DOF_BOKEH_LUT_SIZE};

  DRW_PASS_CREATE(psl->dof_bokeh, DRW_STATE_WRITE_COLOR);

  GPUShader *sh = EEVEE_shaders_depth_of_field_bokeh_get();
  DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_bokeh);
  DRW_shgroup_uniform_float_copy(grp, "bokehSides", fx->dof_bokeh_blades);
  DRW_shgroup_uniform_float_copy(grp, "bokehRotation", fx->dof_bokeh_rotation);
  DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropyInv", fx->dof_bokeh_aniso_inv);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

  fx->dof_bokeh_gather_lut_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_RG16F, owner);
  fx->dof_bokeh_scatter_lut_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_R16F, owner);
  fx->dof_bokeh_resolve_lut_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_R16F, owner);

  GPU_framebuffer_ensure_config(&fbl->dof_bokeh_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_bokeh_gather_lut_tx),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_bokeh_scatter_lut_tx),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_bokeh_resolve_lut_tx),
                                });
}

/**
 * Outputs halfResColorBuffer and halfResCocBuffer.
 **/
static void dof_setup_pass_init(EEVEE_FramebufferList *fbl,
                                EEVEE_PassList *psl,
                                EEVEE_EffectsInfo *fx)
{
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  void *owner = (void *)&EEVEE_depth_of_field_init;
  const float *fullres = DRW_viewport_size_get();
  int res[2] = {divide_ceil_u(fullres[0], 2), divide_ceil_u(fullres[1], 2)};

  DRW_PASS_CREATE(psl->dof_setup, DRW_STATE_WRITE_COLOR);

  GPUShader *sh = EEVEE_shaders_depth_of_field_setup_get();
  DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_setup);
  DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &fx->source_buffer, NO_FILTERING);
  DRW_shgroup_uniform_texture_ref_ex(grp, "depthBuffer", &dtxl->depth, NO_FILTERING);
  DRW_shgroup_uniform_vec4_copy(grp, "cocParams", fx->dof_coc_params);
  DRW_shgroup_uniform_float_copy(grp, "bokehMaxSize", fx->dof_bokeh_max_size);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

  fx->dof_half_res_color_tx = DRW_texture_pool_query_2d(UNPACK2(res), COLOR_FORMAT, owner);
  fx->dof_half_res_coc_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_RG16F, owner);

  GPU_framebuffer_ensure_config(&fbl->dof_setup_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_half_res_color_tx),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_half_res_coc_tx),
                                });
}

/**
 * Outputs min & max COC in each 8x8 half res pixel tiles (so 1/16th of full resolution).
 **/
static void dof_flatten_tiles_pass_init(EEVEE_FramebufferList *fbl,
                                        EEVEE_PassList *psl,
                                        EEVEE_EffectsInfo *fx)
{
  void *owner = (void *)&EEVEE_depth_of_field_init;
  const float *fullres = DRW_viewport_size_get();
  int res[2] = {divide_ceil_u(fullres[0], DOF_TILE_DIVISOR),
                divide_ceil_u(fullres[1], DOF_TILE_DIVISOR)};

  DRW_PASS_CREATE(psl->dof_flatten_tiles, DRW_STATE_WRITE_COLOR);

  GPUShader *sh = EEVEE_shaders_depth_of_field_flatten_tiles_get();
  DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_flatten_tiles);
  DRW_shgroup_uniform_texture_ref_ex(
      grp, "halfResCocBuffer", &fx->dof_half_res_coc_tx, NO_FILTERING);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

  fx->dof_coc_tiles_fg_tx = DRW_texture_pool_query_2d(UNPACK2(res), FG_TILE_FORMAT, owner);
  fx->dof_coc_tiles_bg_tx = DRW_texture_pool_query_2d(UNPACK2(res), BG_TILE_FORMAT, owner);

  GPU_framebuffer_ensure_config(&fbl->dof_flatten_tiles_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_coc_tiles_fg_tx),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_coc_tiles_bg_tx),
                                });
}

/**
 * Dilates the min & max COCS to cover maximum COC values.
 * Output format/dimensions should be the same as coc_flatten_pass as they are swapped for
 * doing multiple dilation passes.
 **/
static void dof_dilate_tiles_pass_init(EEVEE_FramebufferList *fbl,
                                       EEVEE_PassList *psl,
                                       EEVEE_EffectsInfo *fx)
{
  void *owner = (void *)&EEVEE_depth_of_field_init;
  const float *fullres = DRW_viewport_size_get();
  int res[2] = {divide_ceil_u(fullres[0], DOF_TILE_DIVISOR),
                divide_ceil_u(fullres[1], DOF_TILE_DIVISOR)};

  DRW_PASS_CREATE(psl->dof_dilate_tiles_minmax, DRW_STATE_WRITE_COLOR);
  DRW_PASS_CREATE(psl->dof_dilate_tiles_minabs, DRW_STATE_WRITE_COLOR);

  for (int pass = 0; pass < 2; pass++) {
    DRWPass *drw_pass = (pass == 0) ? psl->dof_dilate_tiles_minmax : psl->dof_dilate_tiles_minabs;
    GPUShader *sh = EEVEE_shaders_depth_of_field_dilate_tiles_get(pass);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, drw_pass);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesFgBuffer", &fx->dof_coc_tiles_fg_tx);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesBgBuffer", &fx->dof_coc_tiles_bg_tx);
    DRW_shgroup_uniform_bool(grp, "dilateSlightFocus", &fx->dof_dilate_slight_focus, 1);
    DRW_shgroup_uniform_int(grp, "ringCount", &fx->dof_dilate_ring_count, 1);
    DRW_shgroup_uniform_int(grp, "ringWidthMultiplier", &fx->dof_dilate_ring_width_multiplier, 1);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }

  fx->dof_coc_dilated_tiles_fg_tx = DRW_texture_pool_query_2d(UNPACK2(res), FG_TILE_FORMAT, owner);
  fx->dof_coc_dilated_tiles_bg_tx = DRW_texture_pool_query_2d(UNPACK2(res), BG_TILE_FORMAT, owner);

  GPU_framebuffer_ensure_config(&fbl->dof_dilate_tiles_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_coc_dilated_tiles_fg_tx),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_coc_dilated_tiles_bg_tx),
                                });
}

static void dof_dilate_tiles_pass_draw(EEVEE_FramebufferList *fbl,
                                       EEVEE_PassList *psl,
                                       EEVEE_EffectsInfo *fx)
{
  for (int pass = 0; pass < 2; pass++) {
    DRWPass *drw_pass = (pass == 0) ? psl->dof_dilate_tiles_minmax : psl->dof_dilate_tiles_minabs;

    /* Error introduced by gather center jittering. */
    const float error_multiplier = 1.0f + 1.0f / (DOF_GATHER_RING_COUNT + 0.5f);
    int dilation_end_radius = ceilf((fx->dof_fx_max_coc * error_multiplier) / DOF_TILE_DIVISOR);

    /* This algorithm produce the exact dilation radius by dividing it in multiple passes. */
    int dilation_radius = 0;
    while (dilation_radius < dilation_end_radius) {
      /* Dilate slight focus only on first iteration. */
      fx->dof_dilate_slight_focus = (dilation_radius == 0) ? 1 : 0;

      int remainder = dilation_end_radius - dilation_radius;
      /* Do not step over any unvisited tile. */
      int max_multiplier = dilation_radius + 1;

      int ring_count = min_ii(DOF_DILATE_RING_COUNT, ceilf(remainder / (float)max_multiplier));
      int multiplier = min_ii(max_multiplier, floor(remainder / (float)ring_count));

      dilation_radius += ring_count * multiplier;

      fx->dof_dilate_ring_count = ring_count;
      fx->dof_dilate_ring_width_multiplier = multiplier;

      GPU_framebuffer_bind(fbl->dof_dilate_tiles_fb);
      DRW_draw_pass(drw_pass);

      SWAP(GPUFrameBuffer *, fbl->dof_dilate_tiles_fb, fbl->dof_flatten_tiles_fb);
      SWAP(GPUTexture *, fx->dof_coc_dilated_tiles_bg_tx, fx->dof_coc_tiles_bg_tx);
      SWAP(GPUTexture *, fx->dof_coc_dilated_tiles_fg_tx, fx->dof_coc_tiles_fg_tx);
    }
  }
  /* Swap again so that final textures are dof_coc_dilated_tiles_*_tx. */
  SWAP(GPUFrameBuffer *, fbl->dof_dilate_tiles_fb, fbl->dof_flatten_tiles_fb);
  SWAP(GPUTexture *, fx->dof_coc_dilated_tiles_bg_tx, fx->dof_coc_tiles_bg_tx);
  SWAP(GPUTexture *, fx->dof_coc_dilated_tiles_fg_tx, fx->dof_coc_tiles_fg_tx);
}

/**
 * Create mipmapped color & COC textures for gather passes.
 **/
static void dof_reduce_pass_init(EEVEE_FramebufferList *fbl,
                                 EEVEE_PassList *psl,
                                 EEVEE_TextureList *txl,
                                 EEVEE_EffectsInfo *fx)
{
  const float *fullres = DRW_viewport_size_get();

  /* Divide by 2 because dof_fx_max_coc is in fullres CoC radius and the reduce texture begins at
   * half resolution. */
  float max_space_between_sample = fx->dof_fx_max_coc * 0.5f / DOF_GATHER_RING_COUNT;

  int mip_count = max_ii(1, log2_ceil_u(max_space_between_sample));

  fx->dof_reduce_steps = mip_count - 1;
  /* This ensure the mipmaps are aligned for the needed 4 mip levels.
   * Starts at 2 because already at half resolution. */
  int multiple = 2 << (mip_count - 1);
  int res[2] = {(multiple * divide_ceil_u(fullres[0], multiple)) / 2,
                (multiple * divide_ceil_u(fullres[1], multiple)) / 2};

  int quater_res[2] = {divide_ceil_u(fullres[0], 4), divide_ceil_u(fullres[1], 4)};

  /* TODO(fclem): Make this dependent of the quality of the gather pass. */
  fx->dof_scatter_coc_threshold = 4.0f;

  {
    DRW_PASS_CREATE(psl->dof_downsample, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = EEVEE_shaders_depth_of_field_downsample_get();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_downsample);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "colorBuffer", &fx->dof_reduce_input_color_tx, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "cocBuffer", &fx->dof_reduce_input_coc_tx, NO_FILTERING);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    void *owner = (void *)&EEVEE_depth_of_field_init;
    fx->dof_downsample_tx = DRW_texture_pool_query_2d(UNPACK2(quater_res), COLOR_FORMAT, owner);

    GPU_framebuffer_ensure_config(&fbl->dof_downsample_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_downsample_tx),
                                  });
  }

  {
    DRW_PASS_CREATE(psl->dof_reduce_copy, DRW_STATE_WRITE_COLOR);

    const bool is_copy_pass = true;
    GPUShader *sh = EEVEE_shaders_depth_of_field_reduce_get(is_copy_pass);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_reduce_copy);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "colorBuffer", &fx->dof_reduce_input_color_tx, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "cocBuffer", &fx->dof_reduce_input_coc_tx, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "downsampledBuffer", &fx->dof_downsample_tx, NO_FILTERING);
    DRW_shgroup_uniform_float_copy(grp, "scatterColorThreshold", fx->dof_scatter_color_threshold);
    DRW_shgroup_uniform_float_copy(
        grp, "scatterColorNeighborMax", fx->dof_scatter_neighbor_max_color);
    DRW_shgroup_uniform_float_copy(grp, "scatterCocThreshold", fx->dof_scatter_coc_threshold);
    DRW_shgroup_uniform_float_copy(grp, "colorNeighborClamping", fx->dof_denoise_factor);
    DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropy", fx->dof_bokeh_aniso);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    void *owner = (void *)&EEVEE_depth_of_field_init;
    fx->dof_scatter_src_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_R11F_G11F_B10F, owner);
  }

  {
    DRW_PASS_CREATE(psl->dof_reduce, DRW_STATE_WRITE_COLOR);

    const bool is_copy_pass = false;
    GPUShader *sh = EEVEE_shaders_depth_of_field_reduce_get(is_copy_pass);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_reduce);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "colorBuffer", &fx->dof_reduce_input_color_tx, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "cocBuffer", &fx->dof_reduce_input_coc_tx, NO_FILTERING);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }

  if (txl->dof_reduced_color) {
    /* TODO(fclem) In the future, we need to check if mip_count did not change.
     * For now it's ok as we always define all mip level.*/
    if (res[0] != GPU_texture_width(txl->dof_reduced_color) ||
        res[1] != GPU_texture_width(txl->dof_reduced_color)) {
      DRW_TEXTURE_FREE_SAFE(txl->dof_reduced_color);
      DRW_TEXTURE_FREE_SAFE(txl->dof_reduced_coc);
    }
  }

  if (txl->dof_reduced_color == NULL) {
    /* Color needs to be signed format here. See note in shader for explanation. */
    /* Do not use texture pool because of needs mipmaps. */
    txl->dof_reduced_color = GPU_texture_create_2d(
        "dof_reduced_color", UNPACK2(res), mip_count, GPU_RGBA16F, NULL);
    txl->dof_reduced_coc = GPU_texture_create_2d(
        "dof_reduced_coc", UNPACK2(res), mip_count, GPU_R16F, NULL);

    /* TODO(fclem) Remove once we have immutable storage or when mips are generated on creation. */
    GPU_texture_generate_mipmap(txl->dof_reduced_color);
    GPU_texture_generate_mipmap(txl->dof_reduced_coc);
  }

  GPU_framebuffer_ensure_config(&fbl->dof_reduce_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(txl->dof_reduced_color),
                                    GPU_ATTACHMENT_TEXTURE(txl->dof_reduced_coc),
                                });

  GPU_framebuffer_ensure_config(&fbl->dof_reduce_copy_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(txl->dof_reduced_color),
                                    GPU_ATTACHMENT_TEXTURE(txl->dof_reduced_coc),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_scatter_src_tx),
                                });
}

/**
 * Do the gather convolution. For each pixels we gather multiple pixels in its neighborhood
 * depending on the min & max CoC tiles.
 **/
static void dof_gather_pass_init(EEVEE_FramebufferList *fbl,
                                 EEVEE_PassList *psl,
                                 EEVEE_TextureList *txl,
                                 EEVEE_EffectsInfo *fx)
{
  void *owner = (void *)&EEVEE_depth_of_field_init;
  const float *fullres = DRW_viewport_size_get();
  int res[2] = {divide_ceil_u(fullres[0], 2), divide_ceil_u(fullres[1], 2)};
  int input_size[2];
  GPU_texture_get_mipmap_size(txl->dof_reduced_color, 0, input_size);
  float uv_correction_fac[2] = {res[0] / (float)input_size[0], res[1] / (float)input_size[1]};
  float output_texel_size[2] = {1.0f / res[0], 1.0f / res[1]};
  const bool use_bokeh_tx = (fx->dof_bokeh_gather_lut_tx != NULL);

  {
    DRW_PASS_CREATE(psl->dof_gather_fg_holefill, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_HOLEFILL, false);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_gather_fg_holefill);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "colorBufferBilinear", &txl->dof_reduced_color, WITH_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &txl->dof_reduced_color, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "cocBuffer", &txl->dof_reduced_coc, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesFgBuffer", &fx->dof_coc_dilated_tiles_fg_tx);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesBgBuffer", &fx->dof_coc_dilated_tiles_bg_tx);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_vec2_copy(grp, "gatherInputUvCorrection", uv_correction_fac);
    DRW_shgroup_uniform_vec2_copy(grp, "gatherOutputTexelSize", output_texel_size);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    /* Reuse textures from the setup pass. */
    /* NOTE: We could use the texture pool do that for us but it does not track usage and it might
     * backfire (it does in practice). */
    fx->dof_fg_holefill_color_tx = fx->dof_half_res_color_tx;
    fx->dof_fg_holefill_weight_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_R16F, owner);

    GPU_framebuffer_ensure_config(&fbl->dof_gather_fg_holefill_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_holefill_color_tx),
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_holefill_weight_tx),
                                  });
  }
  {
    DRW_PASS_CREATE(psl->dof_gather_fg, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_FOREGROUND, use_bokeh_tx);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_gather_fg);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "colorBufferBilinear", &txl->dof_reduced_color, WITH_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &txl->dof_reduced_color, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "cocBuffer", &txl->dof_reduced_coc, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesFgBuffer", &fx->dof_coc_dilated_tiles_fg_tx);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesBgBuffer", &fx->dof_coc_dilated_tiles_bg_tx);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_vec2_copy(grp, "gatherInputUvCorrection", uv_correction_fac);
    DRW_shgroup_uniform_vec2_copy(grp, "gatherOutputTexelSize", output_texel_size);
    if (use_bokeh_tx) {
      /* Negate to flip bokeh shape. Mimics optical phenomenon. */
      negate_v2(fx->dof_bokeh_aniso);
      DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropy", fx->dof_bokeh_aniso);
      DRW_shgroup_uniform_texture_ref(grp, "bokehLut", &fx->dof_bokeh_gather_lut_tx);
      /* Restore. */
      negate_v2(fx->dof_bokeh_aniso);
    }
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    fx->dof_fg_color_tx = DRW_texture_pool_query_2d(UNPACK2(res), COLOR_FORMAT, owner);
    fx->dof_fg_weight_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_R16F, owner);
    /* Reuse textures from the setup pass. */
    /* NOTE: We could use the texture pool do that for us but it does not track usage and it might
     * backfire (it does in practice). */
    fx->dof_fg_occlusion_tx = fx->dof_half_res_coc_tx;

    /* NOTE: First target is holefill texture so we can use the median filter on it.
     * See the filter function. */
    GPU_framebuffer_ensure_config(&fbl->dof_gather_fg_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_holefill_color_tx),
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_holefill_weight_tx),
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_occlusion_tx),
                                  });
  }
  {
    DRW_PASS_CREATE(psl->dof_gather_bg, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_BACKGROUND, use_bokeh_tx);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_gather_bg);
    DRW_shgroup_uniform_texture_ref_ex(
        grp, "colorBufferBilinear", &txl->dof_reduced_color, WITH_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &txl->dof_reduced_color, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "cocBuffer", &txl->dof_reduced_coc, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesFgBuffer", &fx->dof_coc_dilated_tiles_fg_tx);
    DRW_shgroup_uniform_texture_ref(grp, "cocTilesBgBuffer", &fx->dof_coc_dilated_tiles_bg_tx);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_vec2_copy(grp, "gatherInputUvCorrection", uv_correction_fac);
    DRW_shgroup_uniform_vec2_copy(grp, "gatherOutputTexelSize", output_texel_size);
    if (use_bokeh_tx) {
      DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropy", fx->dof_bokeh_aniso);
      DRW_shgroup_uniform_texture_ref(grp, "bokehLut", &fx->dof_bokeh_gather_lut_tx);
    }
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    fx->dof_bg_color_tx = DRW_texture_pool_query_2d(UNPACK2(res), COLOR_FORMAT, owner);
    fx->dof_bg_weight_tx = DRW_texture_pool_query_2d(UNPACK2(res), GPU_R16F, owner);
    /* Reuse, since only used for scatter. Foreground is processed before background. */
    fx->dof_bg_occlusion_tx = fx->dof_fg_occlusion_tx;

    /* NOTE: First target is holefill texture so we can use the median filter on it.
     * See the filter function. */
    GPU_framebuffer_ensure_config(&fbl->dof_gather_bg_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_holefill_color_tx),
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_holefill_weight_tx),
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_bg_occlusion_tx),
                                  });
  }
}

/**
 * Filter an input buffer using a median filter to reduce noise.
 * NOTE: We use the holefill texture as our input to reduce memory usage.
 * Thus, the holefill pass cannot be filtered.
 **/
static void dof_filter_pass_init(EEVEE_FramebufferList *fbl,
                                 EEVEE_PassList *psl,
                                 EEVEE_EffectsInfo *fx)
{
  DRW_PASS_CREATE(psl->dof_filter, DRW_STATE_WRITE_COLOR);

  GPUShader *sh = EEVEE_shaders_depth_of_field_filter_get();
  DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_filter);
  DRW_shgroup_uniform_texture_ref_ex(
      grp, "colorBuffer", &fx->dof_fg_holefill_color_tx, NO_FILTERING);
  DRW_shgroup_uniform_texture_ref_ex(
      grp, "weightBuffer", &fx->dof_fg_holefill_weight_tx, NO_FILTERING);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

  GPU_framebuffer_ensure_config(&fbl->dof_filter_fg_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_fg_color_tx),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_fg_weight_tx),
                                });

  GPU_framebuffer_ensure_config(&fbl->dof_filter_bg_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_bg_color_tx),
                                    GPU_ATTACHMENT_TEXTURE(fx->dof_bg_weight_tx),
                                });
}

/**
 * Do the Scatter convolution. A sprite is emitted for every 4 pixels but is only expanded if the
 * pixels are bright enough to be scattered.
 **/
static void dof_scatter_pass_init(EEVEE_FramebufferList *fbl,
                                  EEVEE_PassList *psl,
                                  EEVEE_TextureList *txl,
                                  EEVEE_EffectsInfo *fx)
{
  int input_size[2], target_size[2];
  GPU_texture_get_mipmap_size(fx->dof_half_res_color_tx, 0, input_size);
  GPU_texture_get_mipmap_size(fx->dof_bg_color_tx, 0, target_size);
  /* Draw a sprite for every four half-res pixels. */
  int sprite_count = (input_size[0] / 2) * (input_size[1] / 2);
  float target_texel_size[2] = {1.0f / target_size[0], 1.0f / target_size[1]};
  const bool use_bokeh_tx = (fx->dof_bokeh_gather_lut_tx != NULL);

  {
    DRW_PASS_CREATE(psl->dof_scatter_fg, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);

    const bool is_foreground = true;
    GPUShader *sh = EEVEE_shaders_depth_of_field_scatter_get(is_foreground, use_bokeh_tx);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_scatter_fg);
    DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &fx->dof_scatter_src_tx, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "cocBuffer", &txl->dof_reduced_coc, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref(grp, "occlusionBuffer", &fx->dof_fg_occlusion_tx);
    DRW_shgroup_uniform_vec2_copy(grp, "targetTexelSize", target_texel_size);
    DRW_shgroup_uniform_int_copy(grp, "spritePerRow", input_size[0] / 2);
    DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropy", fx->dof_bokeh_aniso);
    if (use_bokeh_tx) {
      /* Negate to flip bokeh shape. Mimics optical phenomenon. */
      negate_v2(fx->dof_bokeh_aniso_inv);
      DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropyInv", fx->dof_bokeh_aniso_inv);
      DRW_shgroup_uniform_texture_ref(grp, "bokehLut", &fx->dof_bokeh_scatter_lut_tx);
      /* Restore. */
      negate_v2(fx->dof_bokeh_aniso_inv);
    }
    DRW_shgroup_call_procedural_triangles(grp, NULL, sprite_count);

    GPU_framebuffer_ensure_config(&fbl->dof_scatter_fg_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_fg_color_tx),
                                  });
  }
  {
    DRW_PASS_CREATE(psl->dof_scatter_bg, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);

    const bool is_foreground = false;
    GPUShader *sh = EEVEE_shaders_depth_of_field_scatter_get(is_foreground, use_bokeh_tx);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_scatter_bg);
    DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &fx->dof_scatter_src_tx, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref_ex(grp, "cocBuffer", &txl->dof_reduced_coc, NO_FILTERING);
    DRW_shgroup_uniform_texture_ref(grp, "occlusionBuffer", &fx->dof_bg_occlusion_tx);
    DRW_shgroup_uniform_vec2_copy(grp, "targetTexelSize", target_texel_size);
    DRW_shgroup_uniform_int_copy(grp, "spritePerRow", input_size[0] / 2);
    DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropy", fx->dof_bokeh_aniso);
    if (use_bokeh_tx) {
      DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropyInv", fx->dof_bokeh_aniso_inv);
      DRW_shgroup_uniform_texture_ref(grp, "bokehLut", &fx->dof_bokeh_scatter_lut_tx);
    }
    DRW_shgroup_call_procedural_triangles(grp, NULL, sprite_count);

    GPU_framebuffer_ensure_config(&fbl->dof_scatter_bg_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(fx->dof_bg_color_tx),
                                  });
  }
}

/**
 * Recombine the result of the foreground and background processing. Also perform a slight out of
 * focus blur to improve geometric continuity.
 **/
static void dof_recombine_pass_init(EEVEE_FramebufferList *UNUSED(fbl),
                                    EEVEE_PassList *psl,
                                    EEVEE_EffectsInfo *fx)
{
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const bool use_bokeh_tx = (fx->dof_bokeh_gather_lut_tx != NULL);

  DRW_PASS_CREATE(psl->dof_resolve, DRW_STATE_WRITE_COLOR);

  GPUShader *sh = EEVEE_shaders_depth_of_field_resolve_get(use_bokeh_tx, fx->dof_hq_slight_focus);
  DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->dof_resolve);
  DRW_shgroup_uniform_texture_ref_ex(grp, "fullResColorBuffer", &fx->source_buffer, NO_FILTERING);
  DRW_shgroup_uniform_texture_ref_ex(grp, "fullResDepthBuffer", &dtxl->depth, NO_FILTERING);
  DRW_shgroup_uniform_texture_ref(grp, "bgColorBuffer", &fx->dof_bg_color_tx);
  DRW_shgroup_uniform_texture_ref(grp, "bgWeightBuffer", &fx->dof_bg_weight_tx);
  DRW_shgroup_uniform_texture_ref(grp, "bgTileBuffer", &fx->dof_coc_dilated_tiles_bg_tx);
  DRW_shgroup_uniform_texture_ref(grp, "fgColorBuffer", &fx->dof_fg_color_tx);
  DRW_shgroup_uniform_texture_ref(grp, "fgWeightBuffer", &fx->dof_fg_weight_tx);
  DRW_shgroup_uniform_texture_ref(grp, "holefillColorBuffer", &fx->dof_fg_holefill_color_tx);
  DRW_shgroup_uniform_texture_ref(grp, "holefillWeightBuffer", &fx->dof_fg_holefill_weight_tx);
  DRW_shgroup_uniform_texture_ref(grp, "fgTileBuffer", &fx->dof_coc_dilated_tiles_fg_tx);
  DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
  DRW_shgroup_uniform_vec4_copy(grp, "cocParams", fx->dof_coc_params);
  DRW_shgroup_uniform_float_copy(grp, "bokehMaxSize", fx->dof_bokeh_max_size);
  if (use_bokeh_tx) {
    DRW_shgroup_uniform_vec2_copy(grp, "bokehAnisotropyInv", fx->dof_bokeh_aniso_inv);
    DRW_shgroup_uniform_texture_ref(grp, "bokehLut", &fx->dof_bokeh_resolve_lut_tx);
  }
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
}

void EEVEE_depth_of_field_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *fx = stl->effects;

  if ((fx->enabled_effects & EFFECT_DOF) != 0) {
    /* GPU_RGBA16F is sufficient now that all scattered bokeh are premultiplied.
     * GPU_R11F_G11F_B10F is not enough when lots of scattered sprites are big and offers
     * relatively small benefits. */
    fx->dof_color_format = GPU_RGBA16F;

    dof_bokeh_pass_init(fbl, psl, fx);
    dof_setup_pass_init(fbl, psl, fx);
    dof_flatten_tiles_pass_init(fbl, psl, fx);
    dof_dilate_tiles_pass_init(fbl, psl, fx);
    dof_reduce_pass_init(fbl, psl, txl, fx);
    dof_gather_pass_init(fbl, psl, txl, fx);
    dof_filter_pass_init(fbl, psl, fx);
    dof_scatter_pass_init(fbl, psl, txl, fx);
    dof_recombine_pass_init(fbl, psl, fx);
  }
}

static void dof_recursive_reduce(void *vedata, int UNUSED(level))
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_TextureList *txl = ((EEVEE_Data *)vedata)->txl;
  EEVEE_EffectsInfo *fx = ((EEVEE_Data *)vedata)->stl->effects;

  fx->dof_reduce_input_color_tx = txl->dof_reduced_color;
  fx->dof_reduce_input_coc_tx = txl->dof_reduced_coc;

  DRW_draw_pass(psl->dof_reduce);
}

void EEVEE_depth_of_field_draw(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects; /* TODO(fclem): Because of silly SWAP_BUFFERS. */
  EEVEE_EffectsInfo *fx = effects;

  /* Depth Of Field */
  if ((effects->enabled_effects & EFFECT_DOF) != 0) {
    DRW_stats_group_start("Depth of Field");

    if (fx->dof_bokeh_gather_lut_tx != NULL) {
      GPU_framebuffer_bind(fbl->dof_bokeh_fb);
      DRW_draw_pass(psl->dof_bokeh);
    }

    GPU_framebuffer_bind(fbl->dof_setup_fb);
    DRW_draw_pass(psl->dof_setup);

    GPU_framebuffer_bind(fbl->dof_flatten_tiles_fb);
    DRW_draw_pass(psl->dof_flatten_tiles);

    dof_dilate_tiles_pass_draw(fbl, psl, fx);

    fx->dof_reduce_input_color_tx = fx->dof_half_res_color_tx;
    fx->dof_reduce_input_coc_tx = fx->dof_half_res_coc_tx;

    /* First step is just a copy. */
    GPU_framebuffer_bind(fbl->dof_downsample_fb);
    DRW_draw_pass(psl->dof_downsample);

    /* First step is just a copy. */
    GPU_framebuffer_bind(fbl->dof_reduce_copy_fb);
    DRW_draw_pass(psl->dof_reduce_copy);

    GPU_framebuffer_recursive_downsample(
        fbl->dof_reduce_fb, fx->dof_reduce_steps, &dof_recursive_reduce, vedata);

    {
      /* Foreground convolution. */
      GPU_framebuffer_bind(fbl->dof_gather_fg_fb);
      DRW_draw_pass(psl->dof_gather_fg);

      GPU_framebuffer_bind(fbl->dof_filter_fg_fb);
      DRW_draw_pass(psl->dof_filter);

      GPU_framebuffer_bind(fbl->dof_scatter_fg_fb);
      DRW_draw_pass(psl->dof_scatter_fg);
    }

    {
      /* Background convolution. */
      GPU_framebuffer_bind(fbl->dof_gather_bg_fb);
      DRW_draw_pass(psl->dof_gather_bg);

      GPU_framebuffer_bind(fbl->dof_filter_bg_fb);
      DRW_draw_pass(psl->dof_filter);

      GPU_framebuffer_bind(fbl->dof_scatter_bg_fb);
      DRW_draw_pass(psl->dof_scatter_bg);
    }

    {
      /* Hole-fill convolution. */
      GPU_framebuffer_bind(fbl->dof_gather_fg_holefill_fb);
      DRW_draw_pass(psl->dof_gather_fg_holefill);

      /* NOTE: do not filter the hole-fill pass as we use it as out filter input buffer. */
    }

    GPU_framebuffer_bind(fx->target_buffer);
    DRW_draw_pass(psl->dof_resolve);

    SWAP_BUFFERS();

    DRW_stats_group_end();
  }
}
