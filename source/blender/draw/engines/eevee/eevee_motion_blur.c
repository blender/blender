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
 * Gather all screen space effects technique such as Bloom, Motion Blur, DoF, SSAO, SSR, ...
 */

#include "DRW_render.h"

#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_camera.h"
#include "BKE_object.h"
#include "BKE_screen.h"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "ED_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "GPU_batch.h"
#include "GPU_texture.h"
#include "eevee_private.h"

static struct {
  /* Motion Blur */
  struct GPUShader *motion_blur_sh;
  struct GPUShader *motion_blur_object_sh;
  struct GPUShader *velocity_tiles_sh;
  struct GPUShader *velocity_tiles_expand_sh;
} e_data = {NULL}; /* Engine data */

extern char datatoc_effect_velocity_tile_frag_glsl[];
extern char datatoc_effect_motion_blur_frag_glsl[];
extern char datatoc_object_motion_frag_glsl[];
extern char datatoc_object_motion_vert_glsl[];
extern char datatoc_common_view_lib_glsl[];

#define EEVEE_VELOCITY_TILE_SIZE 32

static void eevee_create_shader_motion_blur(void)
{
  e_data.motion_blur_sh = DRW_shader_create_fullscreen(
      datatoc_effect_motion_blur_frag_glsl,
      "#define EEVEE_VELOCITY_TILE_SIZE " STRINGIFY(EEVEE_VELOCITY_TILE_SIZE) "\n");
  e_data.motion_blur_object_sh = DRW_shader_create_with_lib(datatoc_object_motion_vert_glsl,
                                                            NULL,
                                                            datatoc_object_motion_frag_glsl,
                                                            datatoc_common_view_lib_glsl,
                                                            NULL);
  e_data.velocity_tiles_sh = DRW_shader_create_fullscreen(
      datatoc_effect_velocity_tile_frag_glsl,
      "#define TILE_GATHER\n"
      "#define EEVEE_VELOCITY_TILE_SIZE " STRINGIFY(EEVEE_VELOCITY_TILE_SIZE) "\n");
  e_data.velocity_tiles_expand_sh = DRW_shader_create_fullscreen(
      datatoc_effect_velocity_tile_frag_glsl,
      "#define TILE_EXPANSION\n"
      "#define EEVEE_VELOCITY_TILE_SIZE " STRINGIFY(EEVEE_VELOCITY_TILE_SIZE) "\n");
}

int EEVEE_motion_blur_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata, Object *camera)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  /* Viewport not supported for now. */
  if (!DRW_state_is_scene_render()) {
    return 0;
  }

  if (scene->eevee.flag & SCE_EEVEE_MOTION_BLUR_ENABLED) {
    if (!e_data.motion_blur_sh) {
      eevee_create_shader_motion_blur();
    }

    if (DRW_state_is_scene_render()) {
      int mb_step = effects->motion_blur_step;
      DRW_view_viewmat_get(NULL, effects->motion_blur.camera[mb_step].viewmat, false);
      DRW_view_persmat_get(NULL, effects->motion_blur.camera[mb_step].persmat, false);
      DRW_view_persmat_get(NULL, effects->motion_blur.camera[mb_step].persinv, true);
    }

    if (camera != NULL) {
      Camera *cam = camera->data;
      effects->motion_blur_near_far[0] = cam->clip_start;
      effects->motion_blur_near_far[1] = cam->clip_end;
    }
    else {
      /* Not supported yet. */
      BLI_assert(0);
    }

    effects->motion_blur_max = max_ii(0, scene->eevee.motion_blur_max);
    const float *fs_size = DRW_viewport_size_get();
    int tx_size[2] = {1 + ((int)fs_size[0] / EEVEE_VELOCITY_TILE_SIZE),
                      1 + ((int)fs_size[1] / EEVEE_VELOCITY_TILE_SIZE)};

    effects->velocity_tiles_x_tx = DRW_texture_pool_query_2d(
        tx_size[0], fs_size[1], GPU_RGBA16, &draw_engine_eevee_type);
    GPU_framebuffer_ensure_config(&fbl->velocity_tiles_fb[0],
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(effects->velocity_tiles_x_tx),
                                  });

    effects->velocity_tiles_tx = DRW_texture_pool_query_2d(
        tx_size[0], tx_size[1], GPU_RGBA16, &draw_engine_eevee_type);
    GPU_framebuffer_ensure_config(&fbl->velocity_tiles_fb[1],
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(effects->velocity_tiles_tx),
                                  });

    return EFFECT_MOTION_BLUR | EFFECT_POST_BUFFER | EFFECT_VELOCITY_BUFFER;
  }
  return 0;
}

void EEVEE_motion_blur_step_set(EEVEE_Data *vedata, int step)
{
  BLI_assert(step < 3);
  /* Meh, code duplication. Could be avoided if render init would not contain cache init. */
  if (vedata->stl->effects == NULL) {
    vedata->stl->effects = MEM_callocN(sizeof(*vedata->stl->effects), __func__);
  }
  vedata->stl->effects->motion_blur_step = step;
}

void EEVEE_motion_blur_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_MotionBlurData *mb_data = &effects->motion_blur;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  if ((effects->enabled_effects & EFFECT_MOTION_BLUR) != 0) {
    const float *fs_size = DRW_viewport_size_get();
    int tx_size[2] = {GPU_texture_width(effects->velocity_tiles_tx),
                      GPU_texture_height(effects->velocity_tiles_tx)};
    DRWShadingGroup *grp;
    {
      DRW_PASS_CREATE(psl->velocity_tiles_x, DRW_STATE_WRITE_COLOR);
      DRW_PASS_CREATE(psl->velocity_tiles, DRW_STATE_WRITE_COLOR);

      /* Create max velocity tiles in 2 passes. One for X and one for Y */
      GPUShader *sh = e_data.velocity_tiles_sh;
      grp = DRW_shgroup_create(sh, psl->velocity_tiles_x);
      DRW_shgroup_uniform_texture(grp, "velocityBuffer", effects->velocity_tx);
      DRW_shgroup_uniform_ivec2_copy(grp, "velocityBufferSize", (int[2]){fs_size[0], fs_size[1]});
      DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
      DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
      DRW_shgroup_uniform_ivec2_copy(grp, "gatherStep", (int[2]){1, 0});
      DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

      grp = DRW_shgroup_create(sh, psl->velocity_tiles);
      DRW_shgroup_uniform_texture(grp, "velocityBuffer", effects->velocity_tiles_x_tx);
      DRW_shgroup_uniform_ivec2_copy(grp, "velocityBufferSize", (int[2]){tx_size[0], fs_size[1]});
      DRW_shgroup_uniform_ivec2_copy(grp, "gatherStep", (int[2]){0, 1});
      DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

      /* Expand max tiles by keeping the max tile in each tile neighborhood. */
      DRW_PASS_CREATE(psl->velocity_tiles_expand[0], DRW_STATE_WRITE_COLOR);
      DRW_PASS_CREATE(psl->velocity_tiles_expand[1], DRW_STATE_WRITE_COLOR);
      for (int i = 0; i < 2; i++) {
        GPUTexture *tile_tx = (i == 0) ? effects->velocity_tiles_tx : effects->velocity_tiles_x_tx;
        GPUShader *sh_expand = e_data.velocity_tiles_expand_sh;
        grp = DRW_shgroup_create(sh_expand, psl->velocity_tiles_expand[i]);
        DRW_shgroup_uniform_ivec2_copy(grp, "velocityBufferSize", tx_size);
        DRW_shgroup_uniform_texture(grp, "velocityBuffer", tile_tx);
        DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
        DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
        DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
      }
    }
    {
      DRW_PASS_CREATE(psl->motion_blur, DRW_STATE_WRITE_COLOR);
      eGPUSamplerState state = 0;
      int expand_steps = 1 + (max_ii(0, effects->motion_blur_max - 1) / EEVEE_VELOCITY_TILE_SIZE);
      GPUTexture *tile_tx = (expand_steps & 1) ? effects->velocity_tiles_x_tx :
                                                 effects->velocity_tiles_tx;

      grp = DRW_shgroup_create(e_data.motion_blur_sh, psl->motion_blur);
      DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
      DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &effects->source_buffer, state);
      DRW_shgroup_uniform_texture_ref_ex(grp, "depthBuffer", &dtxl->depth, state);
      DRW_shgroup_uniform_texture_ref_ex(grp, "velocityBuffer", &effects->velocity_tx, state);
      DRW_shgroup_uniform_texture(grp, "tileMaxBuffer", tile_tx);
      DRW_shgroup_uniform_float_copy(grp, "depthScale", scene->eevee.motion_blur_depth_scale);
      DRW_shgroup_uniform_vec2(grp, "nearFar", effects->motion_blur_near_far, 1);
      DRW_shgroup_uniform_bool_copy(grp, "isPerspective", DRW_view_is_persp_get(NULL));
      DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
      DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
      DRW_shgroup_uniform_ivec2_copy(grp, "tileBufferSize", tx_size);
      DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
    }
    {
      DRW_PASS_CREATE(psl->velocity_object, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

      grp = DRW_shgroup_create(e_data.motion_blur_object_sh, psl->velocity_object);
      DRW_shgroup_uniform_mat4(grp, "prevViewProjMatrix", mb_data->camera[MB_PREV].persmat);
      DRW_shgroup_uniform_mat4(grp, "currViewProjMatrix", mb_data->camera[MB_CURR].persmat);
      DRW_shgroup_uniform_mat4(grp, "nextViewProjMatrix", mb_data->camera[MB_NEXT].persmat);
    }

    EEVEE_motion_blur_data_init(mb_data);
  }
  else {
    psl->motion_blur = NULL;
    psl->velocity_object = NULL;
  }
}

void EEVEE_motion_blur_cache_populate(EEVEE_ViewLayerData *UNUSED(sldata),
                                      EEVEE_Data *vedata,
                                      Object *ob)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DRWShadingGroup *grp = NULL;

  if (!DRW_state_is_scene_render() || psl->velocity_object == NULL) {
    return;
  }

  const bool is_dupli = (ob->base_flag & BASE_FROM_DUPLI) != 0;
  /* For now we assume dupli objects are moving. */
  const bool object_moves = is_dupli || BKE_object_moves_in_time(ob, true);
  const bool is_deform = BKE_object_is_deform_modified(DRW_context_state_get()->scene, ob);

  if (!(object_moves || is_deform)) {
    return;
  }

  EEVEE_ObjectMotionData *mb_data = EEVEE_motion_blur_object_data_get(&effects->motion_blur, ob);

  if (mb_data) {
    int mb_step = effects->motion_blur_step;
    /* Store transform  */
    copy_m4_m4(mb_data->obmat[mb_step], ob->obmat);

    EEVEE_GeometryMotionData *mb_geom = EEVEE_motion_blur_geometry_data_get(&effects->motion_blur,
                                                                            ob);

    if (mb_step == MB_CURR) {
      GPUBatch *batch = DRW_cache_object_surface_get(ob);
      if (batch == NULL) {
        return;
      }

      /* Fill missing matrices if the object was hidden in previous or next frame. */
      if (is_zero_m4(mb_data->obmat[MB_PREV])) {
        copy_m4_m4(mb_data->obmat[MB_PREV], mb_data->obmat[MB_CURR]);
      }
      if (is_zero_m4(mb_data->obmat[MB_NEXT])) {
        copy_m4_m4(mb_data->obmat[MB_NEXT], mb_data->obmat[MB_CURR]);
      }

      grp = DRW_shgroup_create(e_data.motion_blur_object_sh, psl->velocity_object);
      DRW_shgroup_uniform_mat4(grp, "prevModelMatrix", mb_data->obmat[MB_PREV]);
      DRW_shgroup_uniform_mat4(grp, "currModelMatrix", mb_data->obmat[MB_CURR]);
      DRW_shgroup_uniform_mat4(grp, "nextModelMatrix", mb_data->obmat[MB_NEXT]);
      DRW_shgroup_uniform_bool(grp, "useDeform", &mb_geom->use_deform, 1);

      DRW_shgroup_call(grp, batch, ob);

      if (mb_geom->use_deform) {
        /* Keep to modify later (after init). */
        mb_geom->batch = batch;
      }
    }
    else if (is_deform) {
      /* Store vertex position buffer. */
      mb_geom->vbo[mb_step] = DRW_cache_object_pos_vertbuf_get(ob);
      mb_geom->use_deform = (mb_geom->vbo[mb_step] != NULL);
    }
    else {
      mb_geom->vbo[mb_step] = NULL;
      mb_geom->use_deform = false;
    }
  }
}

void EEVEE_motion_blur_cache_finish(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  GHashIterator ghi;

  if ((effects->enabled_effects & EFFECT_MOTION_BLUR) == 0) {
    return;
  }

  for (BLI_ghashIterator_init(&ghi, effects->motion_blur.geom);
       BLI_ghashIterator_done(&ghi) == false;
       BLI_ghashIterator_step(&ghi)) {
    EEVEE_GeometryMotionData *mb_geom = BLI_ghashIterator_getValue(&ghi);

    int mb_step = effects->motion_blur_step;

    if (!mb_geom->use_deform) {
      continue;
    }

    if (mb_step == MB_CURR) {
      /* Modify batch to have data from adjacent frames. */
      GPUBatch *batch = mb_geom->batch;
      for (int i = 0; i < MB_CURR; i++) {
        GPUVertBuf *vbo = mb_geom->vbo[i];
        if (vbo && batch) {
          if (vbo->vertex_len != batch->verts[0]->vertex_len) {
            /* Vertex count mismatch, disable deform motion blur. */
            mb_geom->use_deform = false;
            GPU_VERTBUF_DISCARD_SAFE(mb_geom->vbo[MB_PREV]);
            GPU_VERTBUF_DISCARD_SAFE(mb_geom->vbo[MB_NEXT]);
            break;
          }
          /* Modify the batch to include the previous position. */
          GPU_batch_vertbuf_add_ex(batch, vbo, true);
          /* TODO(fclem) keep the vbo around for next (sub)frames. */
          /* Only do once. */
          mb_geom->vbo[i] = NULL;
        }
      }
    }
    else {
      GPUVertBuf *vbo = mb_geom->vbo[mb_step];
      /* If this assert fails, it means that different EEVEE_GeometryMotionDatas
       * has been used for each motion blur step.  */
      BLI_assert(vbo);
      if (vbo) {
        /* Use the vbo to perform the copy on the GPU. */
        GPU_vertbuf_use(vbo);
        /* Perform a copy to avoid loosing it after RE_engine_frame_set(). */
        mb_geom->vbo[mb_step] = vbo = GPU_vertbuf_duplicate(vbo);
        /* Find and replace "pos" attrib name. */
        int attrib_id = GPU_vertformat_attr_id_get(&vbo->format, "pos");
        GPU_vertformat_attr_rename(&vbo->format, attrib_id, (mb_step == MB_PREV) ? "prv" : "nxt");
      }
    }
  }
}

void EEVEE_motion_blur_draw(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  /* Motion Blur */
  if ((effects->enabled_effects & EFFECT_MOTION_BLUR) != 0) {
    int sample = DRW_state_is_image_render() ? effects->taa_render_sample :
                                               effects->taa_current_sample;
    double r;
    BLI_halton_1d(2, 0.0, sample - 1, &r);
    effects->motion_blur_sample_offset = r;

    /* Create velocity max tiles in 2 passes. One for each dimension. */
    GPU_framebuffer_bind(fbl->velocity_tiles_fb[0]);
    DRW_draw_pass(psl->velocity_tiles_x);

    GPU_framebuffer_bind(fbl->velocity_tiles_fb[1]);
    DRW_draw_pass(psl->velocity_tiles);

    /* Expand the tiles by reading the neighborhood. Do as many passes as required. */
    int buf = 0;
    for (int i = effects->motion_blur_max; i > 0; i -= EEVEE_VELOCITY_TILE_SIZE) {
      GPU_framebuffer_bind(fbl->velocity_tiles_fb[buf]);

      /* Change viewport to avoid invoking more pixel shaders than necessary since in one of the
       * buffer the texture is way bigger in height. This avoid creating another texture and
       * reduce VRAM usage. */
      int w = GPU_texture_width(effects->velocity_tiles_tx);
      int h = GPU_texture_height(effects->velocity_tiles_tx);
      GPU_framebuffer_viewport_set(fbl->velocity_tiles_fb[buf], 0, 0, w, h);

      DRW_draw_pass(psl->velocity_tiles_expand[buf]);

      buf = buf ? 0 : 1;
    }

    GPU_framebuffer_bind(effects->target_buffer);
    DRW_draw_pass(psl->motion_blur);
    SWAP_BUFFERS();
  }
}

void EEVEE_motion_blur_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.motion_blur_sh);
  DRW_SHADER_FREE_SAFE(e_data.motion_blur_object_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_tiles_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_tiles_expand_sh);
}
