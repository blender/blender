/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Gather all screen space effects technique such as Bloom, Motion Blur, DoF, SSAO, SSR, ...
 */

#include "DRW_render.hh"

#include "BLI_rand.h"
#include "BLI_string_utils.hh"

#include "BKE_animsys.h"
#include "BKE_camera.h"
#include "BKE_duplilist.hh"
#include "BKE_object.hh"
#include "BKE_screen.hh"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_screen_types.h"

#include "ED_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "GPU_batch.hh"
#include "GPU_texture.hh"
#include "eevee_private.hh"

int EEVEE_motion_blur_init(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
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

  effects->motion_blur_max = max_ii(0, scene->eevee.motion_blur_max);

  if ((effects->motion_blur_max > 0) && (scene->r.mode & R_MBLUR)) {
    if (DRW_state_is_scene_render()) {
      int mb_step = effects->motion_blur_step;
      DRW_view_viewmat_get(nullptr, effects->motion_blur.camera[mb_step].viewmat, false);
      DRW_view_persmat_get(nullptr, effects->motion_blur.camera[mb_step].persmat, false);
      DRW_view_persmat_get(nullptr, effects->motion_blur.camera[mb_step].persinv, true);
    }

    const float *fs_size = DRW_viewport_size_get();
    const int tx_size[2] = {
        1 + (int(fs_size[0]) / EEVEE_VELOCITY_TILE_SIZE),
        1 + (int(fs_size[1]) / EEVEE_VELOCITY_TILE_SIZE),
    };
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    effects->velocity_tiles_x_tx = DRW_texture_pool_query_2d_ex(
        tx_size[0], fs_size[1], GPU_RGBA16, usage, &draw_engine_eevee_type);
    GPU_framebuffer_ensure_config(&fbl->velocity_tiles_fb[0],
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(effects->velocity_tiles_x_tx),
                                  });

    effects->velocity_tiles_tx = DRW_texture_pool_query_2d_ex(
        tx_size[0], tx_size[1], GPU_RGBA16, usage, &draw_engine_eevee_type);
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
  vedata->stl->effects->motion_blur_step = step;
}

static void eevee_motion_blur_sync_camera(EEVEE_Data *vedata)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  if (DRW_state_is_scene_render()) {
    int mb_step = effects->motion_blur_step;
    DRW_view_viewmat_get(nullptr, effects->motion_blur.camera[mb_step].viewmat, false);
    DRW_view_persmat_get(nullptr, effects->motion_blur.camera[mb_step].persmat, false);
    DRW_view_persmat_get(nullptr, effects->motion_blur.camera[mb_step].persinv, true);
  }

  effects->motion_blur_near_far[0] = fabsf(DRW_view_near_distance_get(nullptr));
  effects->motion_blur_near_far[1] = fabsf(DRW_view_far_distance_get(nullptr));
}

void EEVEE_motion_blur_cache_init(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
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
    const int tx_size[2] = {
        GPU_texture_width(effects->velocity_tiles_tx),
        GPU_texture_height(effects->velocity_tiles_tx),
    };

    eevee_motion_blur_sync_camera(vedata);

    DRWShadingGroup *grp;
    {
      DRW_PASS_CREATE(psl->velocity_tiles_x, DRW_STATE_WRITE_COLOR);
      DRW_PASS_CREATE(psl->velocity_tiles, DRW_STATE_WRITE_COLOR);

      /* Create max velocity tiles in 2 passes. One for X and one for Y */
      GPUShader *sh = EEVEE_shaders_effect_motion_blur_velocity_tiles_sh_get();
      grp = DRW_shgroup_create(sh, psl->velocity_tiles_x);
      DRW_shgroup_uniform_texture(grp, "velocityBuffer", effects->velocity_tx);
      DRW_shgroup_uniform_ivec2_copy(
          grp, "velocityBufferSize", blender::int2{int(fs_size[0]), int(fs_size[1])});
      DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
      DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
      DRW_shgroup_uniform_ivec2_copy(grp, "gatherStep", blender::int2{1, 0});
      DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);

      grp = DRW_shgroup_create(sh, psl->velocity_tiles);
      DRW_shgroup_uniform_texture(grp, "velocityBuffer", effects->velocity_tiles_x_tx);
      DRW_shgroup_uniform_ivec2_copy(
          grp, "velocityBufferSize", blender::int2{tx_size[0], int(fs_size[1])});
      DRW_shgroup_uniform_ivec2_copy(grp, "gatherStep", blender::int2{0, 1});
      DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);

      /* Expand max tiles by keeping the max tile in each tile neighborhood. */
      DRW_PASS_CREATE(psl->velocity_tiles_expand[0], DRW_STATE_WRITE_COLOR);
      DRW_PASS_CREATE(psl->velocity_tiles_expand[1], DRW_STATE_WRITE_COLOR);
      for (int i = 0; i < 2; i++) {
        GPUTexture *tile_tx = (i == 0) ? effects->velocity_tiles_tx : effects->velocity_tiles_x_tx;
        GPUShader *sh_expand = EEVEE_shaders_effect_motion_blur_velocity_tiles_expand_sh_get();
        grp = DRW_shgroup_create(sh_expand, psl->velocity_tiles_expand[i]);
        DRW_shgroup_uniform_ivec2_copy(grp, "velocityBufferSize", tx_size);
        DRW_shgroup_uniform_texture(grp, "velocityBuffer", tile_tx);
        DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
        DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
        DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
      }
    }
    {
      DRW_PASS_CREATE(psl->motion_blur, DRW_STATE_WRITE_COLOR);
      const GPUSamplerState state = GPUSamplerState::default_sampler();
      int expand_steps = 1 + (max_ii(0, effects->motion_blur_max - 1) / EEVEE_VELOCITY_TILE_SIZE);
      GPUTexture *tile_tx = (expand_steps & 1) ? effects->velocity_tiles_x_tx :
                                                 effects->velocity_tiles_tx;

      grp = DRW_shgroup_create(EEVEE_shaders_effect_motion_blur_sh_get(), psl->motion_blur);
      DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
      DRW_shgroup_uniform_texture_ref_ex(grp, "colorBuffer", &effects->source_buffer, state);
      DRW_shgroup_uniform_texture_ref_ex(grp, "depthBuffer", &dtxl->depth, state);
      DRW_shgroup_uniform_texture_ref_ex(grp, "velocityBuffer", &effects->velocity_tx, state);
      DRW_shgroup_uniform_texture(grp, "tileMaxBuffer", tile_tx);
      DRW_shgroup_uniform_float_copy(grp, "depthScale", scene->eevee.motion_blur_depth_scale);
      DRW_shgroup_uniform_vec2(grp, "nearFar", effects->motion_blur_near_far, 1);
      DRW_shgroup_uniform_bool_copy(grp, "isPerspective", DRW_view_is_persp_get(nullptr));
      DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
      DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
      DRW_shgroup_uniform_ivec2_copy(grp, "tileBufferSize", tx_size);
      DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
    }
    {
      DRW_PASS_CREATE(psl->velocity_object, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

      grp = DRW_shgroup_create(EEVEE_shaders_effect_motion_blur_object_sh_get(),
                               psl->velocity_object);
      DRW_shgroup_uniform_mat4(grp, "prevViewProjMatrix", mb_data->camera[MB_PREV].persmat);
      DRW_shgroup_uniform_mat4(grp, "currViewProjMatrix", mb_data->camera[MB_CURR].persmat);
      DRW_shgroup_uniform_mat4(grp, "nextViewProjMatrix", mb_data->camera[MB_NEXT].persmat);

      DRW_PASS_CREATE(psl->velocity_hair, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

      mb_data->hair_grp = grp = DRW_shgroup_create(EEVEE_shaders_effect_motion_blur_hair_sh_get(),
                                                   psl->velocity_hair);
      DRW_shgroup_uniform_mat4(grp, "prevViewProjMatrix", mb_data->camera[MB_PREV].persmat);
      DRW_shgroup_uniform_mat4(grp, "currViewProjMatrix", mb_data->camera[MB_CURR].persmat);
      DRW_shgroup_uniform_mat4(grp, "nextViewProjMatrix", mb_data->camera[MB_NEXT].persmat);

      DRW_pass_link(psl->velocity_object, psl->velocity_hair);
    }

    EEVEE_motion_blur_data_init(mb_data);
  }
  else {
    psl->motion_blur = nullptr;
    psl->velocity_object = nullptr;
    psl->velocity_hair = nullptr;
  }
}

void EEVEE_motion_blur_hair_cache_populate(EEVEE_ViewLayerData * /*sldata*/,
                                           EEVEE_Data *vedata,
                                           Object *ob,
                                           ParticleSystem *psys,
                                           ModifierData *md)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DRWShadingGroup *grp = nullptr;

  if (!DRW_state_is_scene_render() || psl->velocity_hair == nullptr) {
    return;
  }

  /* For now we assume hair objects are always moving. */
  EEVEE_ObjectMotionData *mb_data = EEVEE_motion_blur_object_data_get(
      &effects->motion_blur, ob, true);

  if (mb_data) {
    int mb_step = effects->motion_blur_step;
    /* Store transform. */
    DRW_hair_duplimat_get(ob, psys, md, mb_data->obmat[mb_step]);

    EEVEE_HairMotionData *mb_hair = EEVEE_motion_blur_hair_data_get(mb_data, ob);
    int psys_id = (md != nullptr) ? BLI_findindex(&ob->modifiers, md) : 0;

    if (psys_id >= mb_hair->psys_len) {
      /* This should never happen. It means the modifier list was changed by frame evaluation. */
      BLI_assert(0);
      return;
    }

    if (mb_step == MB_CURR) {
      /* Fill missing matrices if the object was hidden in previous or next frame. */
      if (is_zero_m4(mb_data->obmat[MB_PREV])) {
        copy_m4_m4(mb_data->obmat[MB_PREV], mb_data->obmat[MB_CURR]);
      }
      if (is_zero_m4(mb_data->obmat[MB_NEXT])) {
        copy_m4_m4(mb_data->obmat[MB_NEXT], mb_data->obmat[MB_CURR]);
      }

      GPUTexture *tex_prev = mb_hair->psys[psys_id].step_data[MB_PREV].hair_pos_tx;
      GPUTexture *tex_next = mb_hair->psys[psys_id].step_data[MB_NEXT].hair_pos_tx;

      grp = DRW_shgroup_hair_create_sub(ob, psys, md, effects->motion_blur.hair_grp, nullptr);
      DRW_shgroup_uniform_mat4(grp, "prevModelMatrix", mb_data->obmat[MB_PREV]);
      DRW_shgroup_uniform_mat4(grp, "currModelMatrix", mb_data->obmat[MB_CURR]);
      DRW_shgroup_uniform_mat4(grp, "nextModelMatrix", mb_data->obmat[MB_NEXT]);
      DRW_shgroup_uniform_texture(grp, "prvBuffer", tex_prev);
      DRW_shgroup_uniform_texture(grp, "nxtBuffer", tex_next);
      DRW_shgroup_uniform_bool(grp, "useDeform", &mb_hair->use_deform, 1);
    }
    else {
      /* Store vertex position buffer. */
      mb_hair->psys[psys_id].step_data[mb_step].hair_pos = DRW_hair_pos_buffer_get(ob, psys, md);
      mb_hair->use_deform = true;
    }
  }
}

void EEVEE_motion_blur_curves_cache_populate(EEVEE_ViewLayerData * /*sldata*/,
                                             EEVEE_Data *vedata,
                                             Object *ob)
{
  using namespace blender::draw;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if (!DRW_state_is_scene_render() || psl->velocity_hair == nullptr) {
    return;
  }

  /* For now we assume curves objects are always moving. */
  EEVEE_ObjectMotionData *mb_data = EEVEE_motion_blur_object_data_get(
      &effects->motion_blur, ob, false);
  if (mb_data == nullptr) {
    return;
  }

  int mb_step = effects->motion_blur_step;
  /* Store transform. */
  copy_m4_m4(mb_data->obmat[mb_step], ob->object_to_world().ptr());

  EEVEE_HairMotionData *mb_curves = EEVEE_motion_blur_curves_data_get(mb_data);

  if (mb_step == MB_CURR) {
    /* Fill missing matrices if the object was hidden in previous or next frame. */
    if (is_zero_m4(mb_data->obmat[MB_PREV])) {
      copy_m4_m4(mb_data->obmat[MB_PREV], mb_data->obmat[MB_CURR]);
    }
    if (is_zero_m4(mb_data->obmat[MB_NEXT])) {
      copy_m4_m4(mb_data->obmat[MB_NEXT], mb_data->obmat[MB_CURR]);
    }

    GPUTexture *tex_prev = mb_curves->psys[0].step_data[MB_PREV].hair_pos_tx;
    GPUTexture *tex_next = mb_curves->psys[0].step_data[MB_NEXT].hair_pos_tx;

    DRWShadingGroup *grp = DRW_shgroup_curves_create_sub(
        ob, effects->motion_blur.hair_grp, nullptr);
    DRW_shgroup_uniform_mat4(grp, "prevModelMatrix", mb_data->obmat[MB_PREV]);
    DRW_shgroup_uniform_mat4(grp, "currModelMatrix", mb_data->obmat[MB_CURR]);
    DRW_shgroup_uniform_mat4(grp, "nextModelMatrix", mb_data->obmat[MB_NEXT]);
    DRW_shgroup_uniform_texture(grp, "prvBuffer", tex_prev);
    DRW_shgroup_uniform_texture(grp, "nxtBuffer", tex_next);
    DRW_shgroup_uniform_bool(grp, "useDeform", &mb_curves->use_deform, 1);
  }
  else {
    /* Store vertex position buffer. */
    mb_curves->psys[0].step_data[mb_step].hair_pos = DRW_curves_pos_buffer_get(ob);
    mb_curves->use_deform = true;
  }
}

void EEVEE_motion_blur_cache_populate(EEVEE_ViewLayerData * /*sldata*/,
                                      EEVEE_Data *vedata,
                                      Object *ob)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DRWShadingGroup *grp = nullptr;

  if (!DRW_state_is_scene_render() || psl->velocity_object == nullptr) {
    return;
  }

  RigidBodyOb *rbo = ob->rigidbody_object;

  /* active rigidbody objects only, as only those are affected by sim. */
  const bool has_rigidbody = (rbo && (rbo->type == RBO_TYPE_ACTIVE));
#if 0
  /* For now we assume dupli objects are moving. */
  const bool is_dupli = (ob->base_flag & BASE_FROM_DUPLI) != 0;
  const bool object_moves = is_dupli || has_rigidbody || BKE_object_moves_in_time(ob, true);
#else
  /* BKE_object_moves_in_time does not work in some cases.
   * Better detect non moving object after evaluation. */
  const bool object_moves = true;
#endif
  const bool is_deform = BKE_object_is_deform_modified(DRW_context_state_get()->scene, ob) ||
                         (has_rigidbody && (rbo->flag & RBO_FLAG_USE_DEFORM) != 0);

  if (!(object_moves || is_deform)) {
    return;
  }

  EEVEE_ObjectMotionData *mb_data = EEVEE_motion_blur_object_data_get(
      &effects->motion_blur, ob, false);

  if (mb_data) {
    int mb_step = effects->motion_blur_step;
    /* Store transform. */
    copy_m4_m4(mb_data->obmat[mb_step], ob->object_to_world().ptr());

    EEVEE_GeometryMotionData *mb_geom = EEVEE_motion_blur_geometry_data_get(mb_data);

    if (mb_step == MB_CURR) {
      blender::gpu::Batch *batch = DRW_cache_object_surface_get(ob);
      if (batch == nullptr) {
        return;
      }

      /* Fill missing matrices if the object was hidden in previous or next frame. */
      if (is_zero_m4(mb_data->obmat[MB_PREV])) {
        copy_m4_m4(mb_data->obmat[MB_PREV], mb_data->obmat[MB_CURR]);
      }
      if (is_zero_m4(mb_data->obmat[MB_NEXT])) {
        copy_m4_m4(mb_data->obmat[MB_NEXT], mb_data->obmat[MB_CURR]);
      }

      if (mb_geom->use_deform) {
        /* Keep to modify later (after init). */
        mb_geom->batch = batch;
      }

      /* Avoid drawing object that has no motions since object_moves is always true. */
      if (!mb_geom->use_deform && /* Object deformation can happen without transform. */
          equals_m4m4(mb_data->obmat[MB_PREV], mb_data->obmat[MB_CURR]) &&
          equals_m4m4(mb_data->obmat[MB_NEXT], mb_data->obmat[MB_CURR]))
      {
        return;
      }

      grp = DRW_shgroup_create(EEVEE_shaders_effect_motion_blur_object_sh_get(),
                               psl->velocity_object);
      DRW_shgroup_uniform_mat4(grp, "prevModelMatrix", mb_data->obmat[MB_PREV]);
      DRW_shgroup_uniform_mat4(grp, "currModelMatrix", mb_data->obmat[MB_CURR]);
      DRW_shgroup_uniform_mat4(grp, "nextModelMatrix", mb_data->obmat[MB_NEXT]);
      DRW_shgroup_uniform_bool(grp, "useDeform", &mb_geom->use_deform, 1);

      DRW_shgroup_call(grp, batch, ob);
    }
    else if (is_deform) {
      /* Store vertex position buffer. */
      mb_geom->vbo[mb_step] = DRW_cache_object_pos_vertbuf_get(ob);
      mb_geom->use_deform = (mb_geom->vbo[mb_step] != nullptr);
    }
    else {
      mb_geom->vbo[mb_step] = nullptr;
      mb_geom->use_deform = false;
    }
  }
}

static void motion_blur_remove_vbo_reference_from_batch(blender::gpu::Batch *batch,
                                                        blender::gpu::VertBuf *vbo1,
                                                        blender::gpu::VertBuf *vbo2)
{

  for (int i = 0; i < GPU_BATCH_VBO_MAX_LEN; i++) {
    if (ELEM(batch->verts[i], vbo1, vbo2)) {
      /* Avoid double reference of the VBOs. */
      batch->verts[i] = nullptr;
    }
  }
}

void EEVEE_motion_blur_cache_finish(EEVEE_Data *vedata)
{
  using namespace blender::draw;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  GHashIterator ghi;

  if ((effects->enabled_effects & EFFECT_MOTION_BLUR) == 0) {
    return;
  }

  int mb_step = effects->motion_blur_step;

  if (mb_step != MB_CURR) {
    /* Push instances attributes to the GPU. */
    DRW_render_instance_buffer_finish();

    /* Need to be called after #DRW_render_instance_buffer_finish() */
    /* Also we weed to have a correct FBO bound for #DRW_curves_update. */
    GPU_framebuffer_bind(vedata->fbl->main_fb);
    DRW_curves_update();

    DRW_cache_restart();
  }

  for (BLI_ghashIterator_init(&ghi, effects->motion_blur.object);
       BLI_ghashIterator_done(&ghi) == false;
       BLI_ghashIterator_step(&ghi))
  {
    EEVEE_ObjectMotionData *mb_data = static_cast<EEVEE_ObjectMotionData *>(
        BLI_ghashIterator_getValue(&ghi));
    EEVEE_HairMotionData *mb_hair = mb_data->hair_data;
    EEVEE_GeometryMotionData *mb_geom = mb_data->geometry_data;
    if (mb_hair != nullptr && mb_hair->use_deform) {
      if (mb_step == MB_CURR) {
        /* TODO(fclem): Check if vertex count mismatch. */
        mb_hair->use_deform = true;
      }
      else {
        for (int i = 0; i < mb_hair->psys_len; i++) {
          blender::gpu::VertBuf *vbo = mb_hair->psys[i].step_data[mb_step].hair_pos;
          if (vbo == nullptr) {
            continue;
          }
          EEVEE_HairMotionStepData **step_data_cache_ptr;
          if (!BLI_ghash_ensure_p(effects->motion_blur.hair_motion_step_cache[mb_step],
                                  vbo,
                                  (void ***)&step_data_cache_ptr))
          {
            EEVEE_HairMotionStepData *new_step_data = static_cast<EEVEE_HairMotionStepData *>(
                MEM_callocN(sizeof(EEVEE_HairMotionStepData), __func__));
            /* Duplicate the vbo, otherwise it would be lost when evaluating another frame. */
            new_step_data->hair_pos = GPU_vertbuf_duplicate(vbo);
            /* Create vbo immediately to bind to texture buffer. */
            GPU_vertbuf_use(new_step_data->hair_pos);
            new_step_data->hair_pos_tx = GPU_texture_create_from_vertbuf("hair_pos_motion_blur",
                                                                         new_step_data->hair_pos);
            *step_data_cache_ptr = new_step_data;
          }
          mb_hair->psys[i].step_data[mb_step] = **step_data_cache_ptr;
        }
      }
    }
    if (mb_geom != nullptr && mb_geom->use_deform) {
      if (mb_step == MB_CURR) {
        /* Modify batch to have data from adjacent frames. */
        blender::gpu::Batch *batch = mb_geom->batch;
        for (int i = 0; i < MB_CURR; i++) {
          blender::gpu::VertBuf *vbo = mb_geom->vbo[i];
          if (vbo && batch) {
            if (GPU_vertbuf_get_vertex_len(vbo) != GPU_vertbuf_get_vertex_len(batch->verts[0])) {
              /* Vertex count mismatch, disable deform motion blur. */
              mb_geom->use_deform = false;
            }

            if (mb_geom->use_deform == false) {
              motion_blur_remove_vbo_reference_from_batch(
                  batch, mb_geom->vbo[MB_PREV], mb_geom->vbo[MB_NEXT]);
              break;
            }
            /* Avoid adding the same vbo more than once when the batch is used by multiple
             * instances. */
            if (!GPU_batch_vertbuf_has(batch, vbo)) {
              /* Currently, the code assumes that all objects that share the same mesh in the
               * current frame also share the same mesh on other frames. */
              GPU_batch_vertbuf_add(batch, vbo, false);
            }
          }
        }
      }
      else {
        blender::gpu::VertBuf *vbo = mb_geom->vbo[mb_step];
        if (vbo) {
          /* Use the vbo to perform the copy on the GPU. */
          GPU_vertbuf_use(vbo);
          /* Perform a copy to avoid losing it after RE_engine_frame_set(). */
          blender::gpu::VertBuf **vbo_cache_ptr;
          if (!BLI_ghash_ensure_p(
                  effects->motion_blur.position_vbo_cache[mb_step], vbo, (void ***)&vbo_cache_ptr))
          {
            /* Duplicate the vbo, otherwise it would be lost when evaluating another frame. */
            blender::gpu::VertBuf *duplicated_vbo = GPU_vertbuf_duplicate(vbo);
            *vbo_cache_ptr = duplicated_vbo;
            /* Find and replace "pos" attrib name. */
            GPUVertFormat *format = (GPUVertFormat *)GPU_vertbuf_get_format(duplicated_vbo);
            int attrib_id = GPU_vertformat_attr_id_get(format, "pos");
            GPU_vertformat_attr_rename(format, attrib_id, (mb_step == MB_PREV) ? "prv" : "nxt");
          }
          mb_geom->vbo[mb_step] = vbo = *vbo_cache_ptr;
        }
        else {
          /* This might happen if the object visibility has been animated. */
          mb_geom->use_deform = false;
        }
      }
    }
  }
}

void EEVEE_motion_blur_swap_data(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  GHashIterator ghi;

  BLI_assert((effects->enabled_effects & EFFECT_MOTION_BLUR) != 0);

  /* Camera Data. */
  effects->motion_blur.camera[MB_PREV] = effects->motion_blur.camera[MB_NEXT];

  /* Swap #position_vbo_cache pointers. */
  if (effects->motion_blur.position_vbo_cache[MB_PREV]) {
    BLI_ghash_free(effects->motion_blur.position_vbo_cache[MB_PREV],
                   nullptr,
                   (GHashValFreeFP)GPU_vertbuf_discard);
  }
  effects->motion_blur.position_vbo_cache[MB_PREV] =
      effects->motion_blur.position_vbo_cache[MB_NEXT];
  effects->motion_blur.position_vbo_cache[MB_NEXT] = nullptr;

  /* Swap #hair_motion_step_cache pointers. */
  if (effects->motion_blur.hair_motion_step_cache[MB_PREV]) {
    BLI_ghash_free(effects->motion_blur.hair_motion_step_cache[MB_PREV],
                   nullptr,
                   (GHashValFreeFP)EEVEE_motion_hair_step_free);
  }
  effects->motion_blur.hair_motion_step_cache[MB_PREV] =
      effects->motion_blur.hair_motion_step_cache[MB_NEXT];
  effects->motion_blur.hair_motion_step_cache[MB_NEXT] = nullptr;

  /* Rename attributes in #position_vbo_cache. */
  for (BLI_ghashIterator_init(&ghi, effects->motion_blur.position_vbo_cache[MB_PREV]);
       !BLI_ghashIterator_done(&ghi);
       BLI_ghashIterator_step(&ghi))
  {
    blender::gpu::VertBuf *vbo = static_cast<blender::gpu::VertBuf *>(
        BLI_ghashIterator_getValue(&ghi));
    GPUVertFormat *format = (GPUVertFormat *)GPU_vertbuf_get_format(vbo);
    int attrib_id = GPU_vertformat_attr_id_get(format, "nxt");
    GPU_vertformat_attr_rename(format, attrib_id, "prv");
  }

  /* Object Data. */
  for (BLI_ghashIterator_init(&ghi, effects->motion_blur.object); !BLI_ghashIterator_done(&ghi);
       BLI_ghashIterator_step(&ghi))
  {
    EEVEE_ObjectMotionData *mb_data = static_cast<EEVEE_ObjectMotionData *>(
        BLI_ghashIterator_getValue(&ghi));
    EEVEE_GeometryMotionData *mb_geom = mb_data->geometry_data;
    EEVEE_HairMotionData *mb_hair = mb_data->hair_data;

    copy_m4_m4(mb_data->obmat[MB_PREV], mb_data->obmat[MB_NEXT]);

    if (mb_hair != nullptr) {
      for (int i = 0; i < mb_hair->psys_len; i++) {
        mb_hair->psys[i].step_data[MB_PREV].hair_pos =
            mb_hair->psys[i].step_data[MB_NEXT].hair_pos;
        mb_hair->psys[i].step_data[MB_PREV].hair_pos_tx =
            mb_hair->psys[i].step_data[MB_NEXT].hair_pos_tx;
        mb_hair->psys[i].step_data[MB_NEXT].hair_pos = nullptr;
        mb_hair->psys[i].step_data[MB_NEXT].hair_pos_tx = nullptr;
      }
    }
    if (mb_geom != nullptr) {
      if (mb_geom->batch != nullptr) {
        motion_blur_remove_vbo_reference_from_batch(
            mb_geom->batch, mb_geom->vbo[MB_PREV], mb_geom->vbo[MB_NEXT]);
      }
      mb_geom->vbo[MB_PREV] = mb_geom->vbo[MB_NEXT];
      mb_geom->vbo[MB_NEXT] = nullptr;
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

      GPU_framebuffer_viewport_reset(fbl->velocity_tiles_fb[buf]);

      buf = buf ? 0 : 1;
    }

    GPU_framebuffer_bind(effects->target_buffer);
    DRW_draw_pass(psl->motion_blur);
    SWAP_BUFFERS();
  }
}
