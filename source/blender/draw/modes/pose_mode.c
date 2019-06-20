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
 * \ingroup draw
 */
#include "BKE_modifier.h"

#include "DNA_modifier_types.h"
#include "DNA_view3d_types.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "ED_view3d.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

extern char datatoc_common_view_lib_glsl[];
extern char datatoc_gpu_shader_uniform_color_frag_glsl[];
extern char datatoc_pose_selection_vert_glsl[];

/* *********** LISTS *********** */
/**
 * All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use #POSE_engine_init() to
 * initialize most of them and #POSE_cache_init()
 * for #POSE_PassList
 */

typedef struct POSE_PassList {
  struct DRWPass *bone_solid[2];
  struct DRWPass *bone_transp[2];
  struct DRWPass *bone_outline[2];
  struct DRWPass *bone_wire[2];
  struct DRWPass *bone_envelope[2];
  struct DRWPass *bone_axes;
  struct DRWPass *relationship[2];
  struct DRWPass *bone_selection;
} POSE_PassList;

typedef struct POSE_StorageList {
  struct POSE_PrivateData *g_data;
} POSE_StorageList;

typedef struct POSE_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  POSE_PassList *psl;
  POSE_StorageList *stl;
} POSE_Data;

/* *********** STATIC *********** */

typedef struct POSE_PrivateData {
  DRWShadingGroup *bone_selection_shgrp;
  DRWShadingGroup *bone_selection_invert_shgrp;
  GHash *custom_shapes[2];
  float blend_color[4];
  float blend_color_invert[4];
  bool transparent_bones;
} POSE_PrivateData; /* Transient data */

static struct {
  struct GPUShader *bone_selection_sh;
} e_data = {NULL};

/* *********** FUNCTIONS *********** */
static bool POSE_is_bone_selection_overlay_active(void)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;
  return v3d && (v3d->overlay.flag & V3D_OVERLAY_BONE_SELECT) && draw_ctx->object_pose;
}

static void POSE_engine_init(void *UNUSED(vedata))
{
  if (!e_data.bone_selection_sh) {
    e_data.bone_selection_sh = DRW_shader_create_with_lib(
        datatoc_pose_selection_vert_glsl,
        NULL,
        datatoc_gpu_shader_uniform_color_frag_glsl,
        datatoc_common_view_lib_glsl,
        NULL);
  }
}

static void POSE_engine_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.bone_selection_sh);
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void POSE_cache_init(void *vedata)
{
  POSE_PassList *psl = ((POSE_Data *)vedata)->psl;
  POSE_StorageList *stl = ((POSE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }
  POSE_PrivateData *ppd = stl->g_data;
  ppd->transparent_bones = (draw_ctx->v3d->shading.type == OB_WIRE);

  for (int i = 0; i < 2; ++i) {
    /* Solid bones */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK;
    psl->bone_solid[i] = DRW_pass_create("Bone Solid Pass", state | DRW_STATE_WRITE_DEPTH);
    psl->bone_transp[i] = DRW_pass_create("Bone Transp Pass", state | DRW_STATE_BLEND_ALPHA);

    /* Bones Outline */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    psl->bone_outline[i] = DRW_pass_create("Bone Outline Pass", state);

    /* Wire bones */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_BLEND_ALPHA;
    psl->bone_wire[i] = DRW_pass_create("Bone Wire Pass", state);

    /* distance outline around envelope bones */
    state = DRW_STATE_BLEND_ADD | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_CULL_FRONT;
    psl->bone_envelope[i] = DRW_pass_create("Bone Envelope Outline Pass", state);

    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_BLEND_ALPHA;
    psl->relationship[i] = DRW_pass_create("Bone Relationship Pass", state);

    ppd->custom_shapes[i] = BLI_ghash_ptr_new(__func__);
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WIRE_SMOOTH | DRW_STATE_BLEND_ALPHA;
    psl->bone_axes = DRW_pass_create("Bone Axes Pass", state);
  }

  {
    if (POSE_is_bone_selection_overlay_active()) {
      const float alpha = (draw_ctx->object_mode & OB_MODE_WEIGHT_PAINT) ?
                              0.0f :
                              v3d->overlay.xray_alpha_bone;
      copy_v4_fl4(ppd->blend_color, 0.0f, 0.0f, 0.0f, alpha);
      copy_v4_fl4(ppd->blend_color_invert, 0.0f, 0.0f, 0.0f, pow(alpha, 4));
      DRWShadingGroup *grp;
      psl->bone_selection = DRW_pass_create(
          "Bone Selection", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA);
      grp = DRW_shgroup_create(e_data.bone_selection_sh, psl->bone_selection);
      DRW_shgroup_uniform_vec4(grp, "color", ppd->blend_color, 1);
      stl->g_data->bone_selection_shgrp = grp;
      grp = DRW_shgroup_create(e_data.bone_selection_sh, psl->bone_selection);
      DRW_shgroup_uniform_vec4(grp, "color", ppd->blend_color_invert, 1);
      stl->g_data->bone_selection_invert_shgrp = grp;
    }
  }
}

static bool POSE_is_driven_by_active_armature(Object *ob)
{
  Object *ob_arm = modifiers_isDeformedByArmature(ob);
  if (ob_arm) {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    bool is_active = DRW_pose_mode_armature(ob_arm, draw_ctx->obact);
    if (!is_active && ob_arm->proxy_from) {
      is_active = DRW_pose_mode_armature(ob_arm->proxy_from, draw_ctx->obact);
    }
    return is_active;
  }
  else {
    Object *ob_mesh_deform = modifiers_isDeformedByMeshDeform(ob);
    if (ob_mesh_deform) {
      return POSE_is_driven_by_active_armature(ob_mesh_deform);
    }
  }
  return false;
}

/* Add geometry to shading groups. Execute for each objects */
static void POSE_cache_populate(void *vedata, Object *ob)
{
  POSE_PassList *psl = ((POSE_Data *)vedata)->psl;
  POSE_PrivateData *ppd = ((POSE_Data *)vedata)->stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  /* In the future this will allow us to implement face gizmos,
   * and similar functionalities. For now we handle only pose bones. */

  if (ob->type == OB_ARMATURE) {
    if ((draw_ctx->v3d->flag2 & V3D_HIDE_OVERLAYS) ||
        (draw_ctx->v3d->overlay.flag & V3D_OVERLAY_HIDE_BONES)) {
      return;
    }
    if (DRW_pose_mode_armature(ob, draw_ctx->obact)) {
      int ghost = (ob->dtx & OB_DRAWXRAY) ? 1 : 0;
      bool transp = (ppd->transparent_bones || (ob->dt <= OB_WIRE)) ||
                    XRAY_FLAG_ENABLED(draw_ctx->v3d);

      DRWArmaturePasses passes = {
          .bone_solid = (transp) ? psl->bone_transp[ghost] : psl->bone_solid[ghost],
          .bone_outline = psl->bone_outline[ghost],
          .bone_wire = psl->bone_wire[ghost],
          .bone_envelope = psl->bone_envelope[ghost],
          .bone_axes = psl->bone_axes,
          .relationship_lines = psl->relationship[ghost],
          .custom_shapes = ppd->custom_shapes[transp],
      };
      DRW_shgroup_armature_pose(ob, passes, transp);
    }
  }
  else if (ob->type == OB_MESH && !DRW_state_is_select() &&
           POSE_is_bone_selection_overlay_active()) {
    struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      if (POSE_is_driven_by_active_armature(ob)) {
        DRW_shgroup_call(ppd->bone_selection_shgrp, geom, ob);
      }
      else {
        DRW_shgroup_call(ppd->bone_selection_invert_shgrp, geom, ob);
      }
    }
  }
}

static void POSE_cache_finish(void *vedata)
{
  POSE_PrivateData *ppd = ((POSE_Data *)vedata)->stl->g_data;

  /* TODO(fclem): Do not free it for each frame but reuse it. Avoiding alloc cost. */
  for (int i = 0; i < 2; i++) {
    BLI_ghash_free(ppd->custom_shapes[i], NULL, NULL);
  }
}

/**
 * Return true if armature should be handled by the pose mode engine.
 */
bool DRW_pose_mode_armature(Object *ob, Object *active_ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();

  /* Pose armature is handled by pose mode engine. */
  if (((ob == active_ob) || (ob->mode & OB_MODE_POSE)) &&
      ((draw_ctx->object_mode & OB_MODE_POSE) != 0)) {
    return true;
  }

  /* Armature parent is also handled by pose mode engine. */
  if ((active_ob != NULL) && ((draw_ctx->object_mode & OB_MODE_WEIGHT_PAINT) != 0)) {
    if (ob == draw_ctx->object_pose) {
      return true;
    }
  }

  return false;
}

/* Draw time ! Control rendering pipeline from here */
static void POSE_draw_scene(void *vedata)
{
  POSE_PassList *psl = ((POSE_Data *)vedata)->psl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const bool bone_selection_overlay = POSE_is_bone_selection_overlay_active();

  if (DRW_state_is_select()) {
    DRW_draw_pass(psl->bone_outline[0]);
    DRW_draw_pass(psl->bone_solid[0]);
    DRW_draw_pass(psl->bone_wire[0]);
    DRW_draw_pass(psl->bone_outline[1]);
    DRW_draw_pass(psl->bone_solid[1]);
    DRW_draw_pass(psl->bone_wire[1]);
    return;
  }

  if (bone_selection_overlay) {
    GPU_framebuffer_bind(dfbl->default_fb);
    DRW_draw_pass(psl->bone_selection);
    GPU_framebuffer_bind(dfbl->depth_only_fb);
    GPU_framebuffer_clear_depth(dfbl->depth_only_fb, 1.0);
    GPU_framebuffer_bind(dfbl->default_fb);
  }

  DRW_draw_pass(psl->bone_envelope[0]);
  DRW_draw_pass(psl->bone_transp[0]);

  MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl);

  DRW_draw_pass(psl->bone_solid[0]);
  DRW_draw_pass(psl->bone_outline[0]);
  DRW_draw_pass(psl->bone_wire[0]);
  DRW_draw_pass(psl->relationship[0]);

  MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl);

  if (!DRW_pass_is_empty(psl->bone_envelope[1]) || !DRW_pass_is_empty(psl->bone_transp[1]) ||
      !DRW_pass_is_empty(psl->bone_solid[1]) || !DRW_pass_is_empty(psl->bone_outline[1]) ||
      !DRW_pass_is_empty(psl->bone_wire[1]) || !DRW_pass_is_empty(psl->relationship[1])) {
    if (DRW_state_is_fbo()) {
      GPU_framebuffer_bind(dfbl->default_fb);
      GPU_framebuffer_clear_depth(dfbl->default_fb, 1.0f);
    }

    DRW_draw_pass(psl->bone_envelope[1]);
    DRW_draw_pass(psl->bone_solid[1]);
    DRW_draw_pass(psl->bone_transp[1]);
    DRW_draw_pass(psl->bone_outline[1]);
    DRW_draw_pass(psl->bone_wire[1]);
    DRW_draw_pass(psl->relationship[1]);
  }

  /* Draw axes with linesmooth and outside of multisample buffer. */
  DRW_draw_pass(psl->bone_axes);
}

static const DrawEngineDataSize POSE_data_size = DRW_VIEWPORT_DATA_SIZE(POSE_Data);

DrawEngineType draw_engine_pose_type = {
    NULL,
    NULL,
    N_("PoseMode"),
    &POSE_data_size,
    &POSE_engine_init,
    &POSE_engine_free,
    &POSE_cache_init,
    &POSE_cache_populate,
    &POSE_cache_finish,
    NULL,
    &POSE_draw_scene,
    NULL,
    NULL,
};
