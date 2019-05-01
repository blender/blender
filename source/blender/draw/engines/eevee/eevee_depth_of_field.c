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
 */

#include "DRW_render.h"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_camera.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "eevee_private.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"

static struct {
  /* Depth Of Field */
  struct GPUShader *dof_downsample_sh[2];
  struct GPUShader *dof_scatter_sh[2];
  struct GPUShader *dof_resolve_sh[2];
} e_data = {{NULL}}; /* Engine data */

extern char datatoc_effect_dof_vert_glsl[];
extern char datatoc_effect_dof_frag_glsl[];

static void eevee_create_shader_depth_of_field(const bool use_alpha)
{
  e_data.dof_downsample_sh[use_alpha] = DRW_shader_create_fullscreen(
      datatoc_effect_dof_frag_glsl,
      use_alpha ? "#define USE_ALPHA_DOF\n"
                  "#define STEP_DOWNSAMPLE\n" :
                  "#define STEP_DOWNSAMPLE\n");
  e_data.dof_scatter_sh[use_alpha] = DRW_shader_create(datatoc_effect_dof_vert_glsl,
                                                       NULL,
                                                       datatoc_effect_dof_frag_glsl,
                                                       use_alpha ? "#define USE_ALPHA_DOF\n"
                                                                   "#define STEP_SCATTER\n" :
                                                                   "#define STEP_SCATTER\n");
  e_data.dof_resolve_sh[use_alpha] = DRW_shader_create_fullscreen(datatoc_effect_dof_frag_glsl,
                                                                  use_alpha ?
                                                                      "#define USE_ALPHA_DOF\n"
                                                                      "#define STEP_RESOLVE\n" :
                                                                      "#define STEP_RESOLVE\n");
}

int EEVEE_depth_of_field_init(EEVEE_ViewLayerData *UNUSED(sldata),
                              EEVEE_Data *vedata,
                              Object *camera)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_DOF_ENABLED) {
    RegionView3D *rv3d = draw_ctx->rv3d;
    const bool use_alpha = !DRW_state_draw_background();

    if (!e_data.dof_downsample_sh[use_alpha]) {
      eevee_create_shader_depth_of_field(use_alpha);
    }

    if (camera) {
      const float *viewport_size = DRW_viewport_size_get();
      Camera *cam = (Camera *)camera->data;

      /* Retrieve Near and Far distance */
      effects->dof_near_far[0] = -cam->clip_start;
      effects->dof_near_far[1] = -cam->clip_end;

      int buffer_size[2] = {(int)viewport_size[0] / 2, (int)viewport_size[1] / 2};

      eGPUTextureFormat down_format = DRW_state_draw_background() ? GPU_R11F_G11F_B10F :
                                                                    GPU_RGBA16F;

      effects->dof_down_near = DRW_texture_pool_query_2d(
          buffer_size[0], buffer_size[1], down_format, &draw_engine_eevee_type);
      effects->dof_down_far = DRW_texture_pool_query_2d(
          buffer_size[0], buffer_size[1], down_format, &draw_engine_eevee_type);
      effects->dof_coc = DRW_texture_pool_query_2d(
          buffer_size[0], buffer_size[1], GPU_RG16F, &draw_engine_eevee_type);

      GPU_framebuffer_ensure_config(&fbl->dof_down_fb,
                                    {GPU_ATTACHMENT_NONE,
                                     GPU_ATTACHMENT_TEXTURE(effects->dof_down_near),
                                     GPU_ATTACHMENT_TEXTURE(effects->dof_down_far),
                                     GPU_ATTACHMENT_TEXTURE(effects->dof_coc)});

      /* Go full 32bits for rendering and reduce the color artifacts. */
      eGPUTextureFormat fb_format = DRW_state_is_image_render() ? GPU_RGBA32F : GPU_RGBA16F;

      effects->dof_blur = DRW_texture_pool_query_2d(
          buffer_size[0] * 2, buffer_size[1], fb_format, &draw_engine_eevee_type);

      GPU_framebuffer_ensure_config(&fbl->dof_scatter_fb,
                                    {
                                        GPU_ATTACHMENT_NONE,
                                        GPU_ATTACHMENT_TEXTURE(effects->dof_blur),
                                    });

      if (!DRW_state_draw_background()) {
        effects->dof_blur_alpha = DRW_texture_pool_query_2d(
            buffer_size[0] * 2, buffer_size[1], GPU_R32F, &draw_engine_eevee_type);
        GPU_framebuffer_texture_attach(fbl->dof_scatter_fb, effects->dof_blur_alpha, 1, 0);
      }

      /* Parameters */
      /* TODO UI Options */
      float fstop = cam->gpu_dof.fstop;
      float blades = cam->gpu_dof.num_blades;
      float rotation = cam->gpu_dof.rotation;
      float ratio = 1.0f / cam->gpu_dof.ratio;
      float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
      float focus_dist = BKE_camera_object_dof_distance(camera);
      float focal_len = cam->lens;

      /* this is factor that converts to the scene scale. focal length and sensor are expressed in
       * mm unit.scale_length is how many meters per blender unit we have. We want to convert to
       * blender units though because the shader reads coordinates in world space, which is in
       * blender units.
       * Note however that focus_distance is already in blender units and shall not be scaled here
       * (see T48157). */
      float scale = (scene_eval->unit.system) ? scene_eval->unit.scale_length : 1.0f;
      float scale_camera = 0.001f / scale;
      /* we want radius here for the aperture number  */
      float aperture = 0.5f * scale_camera * focal_len / fstop;
      float focal_len_scaled = scale_camera * focal_len;
      float sensor_scaled = scale_camera * sensor;

      if (rv3d != NULL) {
        sensor_scaled *= rv3d->viewcamtexcofac[0];
      }

      effects->dof_params[1] = aperture *
                               fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
      effects->dof_params[1] *= viewport_size[0] / sensor_scaled;
      effects->dof_params[0] = -focus_dist * effects->dof_params[1];

      effects->dof_bokeh[0] = rotation;
      effects->dof_bokeh[1] = ratio;
      effects->dof_bokeh[2] = scene_eval->eevee.bokeh_max_size;

      /* Precompute values to save instructions in fragment shader. */
      effects->dof_bokeh_sides[0] = blades;
      effects->dof_bokeh_sides[1] = blades > 0.0f ? 2.0f * M_PI / blades : 0.0f;
      effects->dof_bokeh_sides[2] = blades / (2.0f * M_PI);
      effects->dof_bokeh_sides[3] = blades > 0.0f ? cosf(M_PI / blades) : 0.0f;

      return EFFECT_DOF | EFFECT_POST_BUFFER;
    }
  }

  /* Cleanup to release memory */
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_down_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->dof_scatter_fb);

  return 0;
}

void EEVEE_depth_of_field_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if ((effects->enabled_effects & EFFECT_DOF) != 0) {
    /**  Depth of Field algorithm
     *
     * Overview :
     * - Downsample the color buffer into 2 buffers weighted with
     *   CoC values. Also output CoC into a texture.
     * - Shoot quads for every pixel and expand it depending on the CoC.
     *   Do one pass for near Dof and one pass for far Dof.
     * - Finally composite the 2 blurred buffers with the original render.
     */
    DRWShadingGroup *grp;
    struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
    const bool use_alpha = !DRW_state_draw_background();

    psl->dof_down = DRW_pass_create("DoF Downsample", DRW_STATE_WRITE_COLOR);

    grp = DRW_shgroup_create(e_data.dof_downsample_sh[use_alpha], psl->dof_down);
    DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", &effects->source_buffer);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_uniform_vec2(grp, "nearFar", effects->dof_near_far, 1);
    DRW_shgroup_uniform_vec2(grp, "dofParams", effects->dof_params, 1);
    DRW_shgroup_call_add(grp, quad, NULL);

    psl->dof_scatter = DRW_pass_create("DoF Scatter",
                                       DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE_FULL);

    /* This create an empty batch of N triangles to be positioned
     * by the vertex shader 0.4ms against 6ms with instancing */
    const float *viewport_size = DRW_viewport_size_get();
    const int sprite_len = ((int)viewport_size[0] / 2) *
                           ((int)viewport_size[1] / 2); /* brackets matters */
    grp = DRW_shgroup_empty_tri_batch_create(
        e_data.dof_scatter_sh[use_alpha], psl->dof_scatter, sprite_len);
    DRW_shgroup_uniform_texture_ref(grp, "nearBuffer", &effects->dof_down_near);
    DRW_shgroup_uniform_texture_ref(grp, "farBuffer", &effects->dof_down_far);
    DRW_shgroup_uniform_texture_ref(grp, "cocBuffer", &effects->dof_coc);
    DRW_shgroup_uniform_vec4(grp, "bokehParams", effects->dof_bokeh, 2);

    psl->dof_resolve = DRW_pass_create("DoF Resolve", DRW_STATE_WRITE_COLOR);

    grp = DRW_shgroup_create(e_data.dof_resolve_sh[use_alpha], psl->dof_resolve);
    DRW_shgroup_uniform_texture_ref(grp, "scatterBuffer", &effects->dof_blur);
    DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", &effects->source_buffer);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_uniform_vec2(grp, "nearFar", effects->dof_near_far, 1);
    DRW_shgroup_uniform_vec2(grp, "dofParams", effects->dof_params, 1);
    DRW_shgroup_call_add(grp, quad, NULL);

    if (use_alpha) {
      DRW_shgroup_uniform_texture_ref(grp, "scatterAlphaBuffer", &effects->dof_blur_alpha);
    }
  }
}

void EEVEE_depth_of_field_draw(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  /* Depth Of Field */
  if ((effects->enabled_effects & EFFECT_DOF) != 0) {
    float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    /* Downsample */
    GPU_framebuffer_bind(fbl->dof_down_fb);
    DRW_draw_pass(psl->dof_down);

    /* Scatter */
    GPU_framebuffer_bind(fbl->dof_scatter_fb);
    GPU_framebuffer_clear_color(fbl->dof_scatter_fb, clear_col);
    DRW_draw_pass(psl->dof_scatter);

    /* Resolve */
    GPU_framebuffer_bind(effects->target_buffer);
    DRW_draw_pass(psl->dof_resolve);
    SWAP_BUFFERS();
  }
}

void EEVEE_depth_of_field_free(void)
{
  for (int i = 0; i < 2; ++i) {
    DRW_SHADER_FREE_SAFE(e_data.dof_downsample_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.dof_scatter_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.dof_resolve_sh[i]);
  }
}
