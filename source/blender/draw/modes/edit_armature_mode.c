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

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_armature_types.h"
#include "DNA_view3d_types.h"

#include "ED_view3d.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

/* *********** LISTS *********** */
typedef struct EDIT_ARMATURE_PassList {
  struct DRWPass *bone_solid[2];
  struct DRWPass *bone_transp[2];
  struct DRWPass *bone_wire[2];
  struct DRWPass *bone_outline[2];
  struct DRWPass *bone_envelope[2];
  struct DRWPass *bone_axes;
  struct DRWPass *relationship[2];
} EDIT_ARMATURE_PassList;

typedef struct EDIT_ARMATURE_StorageList {
  struct EDIT_ARMATURE_PrivateData *g_data;
} EDIT_ARMATURE_StorageList;

typedef struct EDIT_ARMATURE_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  EDIT_ARMATURE_PassList *psl;
  EDIT_ARMATURE_StorageList *stl;
} EDIT_ARMATURE_Data;

/* *********** STATIC *********** */

typedef struct EDIT_ARMATURE_PrivateData {
  bool transparent_bones;
} EDIT_ARMATURE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void EDIT_ARMATURE_cache_init(void *vedata)
{
  EDIT_ARMATURE_PassList *psl = ((EDIT_ARMATURE_Data *)vedata)->psl;
  EDIT_ARMATURE_StorageList *stl = ((EDIT_ARMATURE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }
  stl->g_data->transparent_bones = (draw_ctx->v3d->shading.type == OB_WIRE);

  for (int i = 0; i < 2; ++i) {
    /* Solid bones */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK;
    psl->bone_solid[i] = DRW_pass_create("Bone Solid Pass", state | DRW_STATE_WRITE_DEPTH);
    psl->bone_transp[i] = DRW_pass_create("Bone Transp Pass", state | DRW_STATE_BLEND);

    /* Bones Outline */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    psl->bone_outline[i] = DRW_pass_create("Bone Outline Pass", state);

    /* Wire bones */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_BLEND;
    psl->bone_wire[i] = DRW_pass_create("Bone Wire Pass", state);

    /* distance outline around envelope bones */
    state = DRW_STATE_ADDITIVE | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_CULL_FRONT;
    psl->bone_envelope[i] = DRW_pass_create("Bone Envelope Outline Pass", state);

    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
            DRW_STATE_BLEND | DRW_STATE_WIRE;
    psl->relationship[i] = DRW_pass_create("Bone Relationship Pass", state);
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WIRE_SMOOTH | DRW_STATE_BLEND;
    psl->bone_axes = DRW_pass_create("Bone Axes Pass", state);
  }
}

static void EDIT_ARMATURE_cache_populate(void *vedata, Object *ob)
{
  bArmature *arm = ob->data;

  if (ob->type == OB_ARMATURE) {
    if (arm->edbo) {
      EDIT_ARMATURE_PassList *psl = ((EDIT_ARMATURE_Data *)vedata)->psl;
      EDIT_ARMATURE_StorageList *stl = ((EDIT_ARMATURE_Data *)vedata)->stl;
      const DRWContextState *draw_ctx = DRW_context_state_get();

      int ghost = (ob->dtx & OB_DRAWXRAY) ? 1 : 0;
      bool transp = (stl->g_data->transparent_bones || (ob->dt <= OB_WIRE)) ||
                    XRAY_FLAG_ENABLED(draw_ctx->v3d);

      DRWArmaturePasses passes = {
          .bone_solid = (transp) ? psl->bone_transp[ghost] : psl->bone_solid[ghost],
          .bone_outline = psl->bone_outline[ghost],
          .bone_wire = psl->bone_wire[ghost],
          .bone_envelope = psl->bone_envelope[ghost],
          .bone_axes = psl->bone_axes,
          .relationship_lines = psl->relationship[ghost],
          .custom_shapes = NULL, /* Not needed in edit mode. */
      };
      DRW_shgroup_armature_edit(ob, passes, transp);
    }
  }
}

static void EDIT_ARMATURE_draw_scene(void *vedata)
{
  EDIT_ARMATURE_PassList *psl = ((EDIT_ARMATURE_Data *)vedata)->psl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (DRW_state_is_select()) {
    DRW_draw_pass(psl->bone_outline[0]);
    DRW_draw_pass(psl->bone_solid[0]);
    DRW_draw_pass(psl->bone_wire[0]);
    DRW_draw_pass(psl->bone_outline[1]);
    DRW_draw_pass(psl->bone_solid[1]);
    DRW_draw_pass(psl->bone_wire[1]);
    return;
  }

  DRW_draw_pass(psl->bone_envelope[0]);

  /* For performance reason, avoid blending on MS target. */
  DRW_draw_pass(psl->bone_transp[0]);

  MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl);

  DRW_draw_pass(psl->bone_solid[0]);
  DRW_draw_pass(psl->bone_outline[0]);
  DRW_draw_pass(psl->bone_wire[0]);
  DRW_draw_pass(psl->relationship[0]);

  MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl);

  if (!DRW_pass_is_empty(psl->bone_envelope[1]) || !DRW_pass_is_empty(psl->bone_solid[1]) ||
      !DRW_pass_is_empty(psl->bone_transp[1]) || !DRW_pass_is_empty(psl->bone_outline[1]) ||
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

static const DrawEngineDataSize EDIT_ARMATURE_data_size = DRW_VIEWPORT_DATA_SIZE(
    EDIT_ARMATURE_Data);

DrawEngineType draw_engine_edit_armature_type = {
    NULL,
    NULL,
    N_("EditArmatureMode"),
    &EDIT_ARMATURE_data_size,
    NULL,
    NULL,
    &EDIT_ARMATURE_cache_init,
    &EDIT_ARMATURE_cache_populate,
    NULL,
    NULL,
    &EDIT_ARMATURE_draw_scene,
    NULL,
    NULL,
};
