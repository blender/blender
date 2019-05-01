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

#include "DRW_render.h"

#include "DNA_meta_types.h"

#include "BKE_object.h"

#include "ED_mball.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use EDIT_METABALL_engine_init() to
 * initialize most of them and EDIT_METABALL_cache_init()
 * for EDIT_METABALL_PassList */

typedef struct EDIT_METABALL_PassList {
  /* Declare all passes here and init them in
   * EDIT_METABALL_cache_init().
   * Only contains (DRWPass *) */
  struct DRWPass *pass;
} EDIT_METABALL_PassList;

typedef struct EDIT_METABALL_FramebufferList {
  /* Contains all framebuffer objects needed by this engine.
   * Only contains (GPUFrameBuffer *) */
  struct GPUFrameBuffer *fb;
} EDIT_METABALL_FramebufferList;

typedef struct EDIT_METABALL_TextureList {
  /* Contains all framebuffer textures / utility textures
   * needed by this engine. Only viewport specific textures
   * (not per object). Only contains (GPUTexture *) */
  struct GPUTexture *texture;
} EDIT_METABALL_TextureList;

typedef struct EDIT_METABALL_StorageList {
  /* Contains any other memory block that the engine needs.
   * Only directly MEM_(m/c)allocN'ed blocks because they are
   * free with MEM_freeN() when viewport is freed.
   * (not per object) */
  // struct CustomStruct *block;
  struct EDIT_METABALL_PrivateData *g_data;
} EDIT_METABALL_StorageList;

typedef struct EDIT_METABALL_Data {
  /* Struct returned by DRW_viewport_engine_data_ensure.
   * If you don't use one of these, just make it a (void *) */
  // void *fbl;
  void *engine_type; /* Required */
  EDIT_METABALL_FramebufferList *fbl;
  EDIT_METABALL_TextureList *txl;
  EDIT_METABALL_PassList *psl;
  EDIT_METABALL_StorageList *stl;
} EDIT_METABALL_Data;

/* *********** STATIC *********** */

typedef struct EDIT_METABALL_PrivateData {
  /* This keeps the references of the shading groups for
   * easy access in EDIT_METABALL_cache_populate() */
  DRWShadingGroup *group;
} EDIT_METABALL_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void EDIT_METABALL_engine_init(void *UNUSED(vedata))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_state_clip_planes_set_from_rv3d(draw_ctx->rv3d);
  }
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void EDIT_METABALL_cache_init(void *vedata)
{
  EDIT_METABALL_PassList *psl = ((EDIT_METABALL_Data *)vedata)->psl;
  EDIT_METABALL_StorageList *stl = ((EDIT_METABALL_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  {
    /* Create a pass */
    DRWState state = (DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                      DRW_STATE_BLEND | DRW_STATE_WIRE);
    psl->pass = DRW_pass_create("My Pass", state);

    /* Create a shadingGroup using a function in draw_common.c or custom one */
    stl->g_data->group = shgroup_instance_mball_handles(psl->pass, draw_ctx->sh_cfg);
  }
}

/* Add geometry to shadingGroups. Execute for each objects */
static void EDIT_METABALL_cache_populate(void *vedata, Object *ob)
{
  // EDIT_METABALL_PassList *psl = ((EDIT_METABALL_Data *)vedata)->psl;
  EDIT_METABALL_StorageList *stl = ((EDIT_METABALL_Data *)vedata)->stl;

  if (ob->type == OB_MBALL) {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    DRWShadingGroup *group = stl->g_data->group;

    if ((ob == draw_ctx->object_edit) || BKE_object_is_in_editmode(ob)) {
      MetaBall *mb = ob->data;

      const float *color;
      const float col_radius[3] = {0.63, 0.19, 0.19};           /* 0x3030A0 */
      const float col_radius_select[3] = {0.94, 0.63, 0.63};    /* 0xA0A0F0 */
      const float col_stiffness[3] = {0.19, 0.63, 0.19};        /* 0x30A030 */
      const float col_stiffness_select[3] = {0.63, 0.94, 0.63}; /* 0xA0F0A0 */

      const bool is_select = DRW_state_is_select();

      float draw_scale_xform[3][4]; /* Matrix of Scale and Translation */
      {
        float scamat[3][3];
        copy_m3_m4(scamat, ob->obmat);
        /* Get the normalized inverse matrix to extract only
         * the scale of Scamat */
        float iscamat[3][3];
        invert_m3_m3(iscamat, scamat);
        normalize_m3(iscamat);
        mul_m3_m3_post(scamat, iscamat);

        copy_v3_v3(draw_scale_xform[0], scamat[0]);
        copy_v3_v3(draw_scale_xform[1], scamat[1]);
        copy_v3_v3(draw_scale_xform[2], scamat[2]);
      }

      int select_id = ob->select_id;
      for (MetaElem *ml = mb->editelems->first; ml != NULL; ml = ml->next, select_id += 0x10000) {
        float world_pos[3];
        mul_v3_m4v3(world_pos, ob->obmat, &ml->x);
        draw_scale_xform[0][3] = world_pos[0];
        draw_scale_xform[1][3] = world_pos[1];
        draw_scale_xform[2][3] = world_pos[2];

        float draw_stiffness_radius = ml->rad * atanf(ml->s) / (float)M_PI_2;

        if ((ml->flag & SELECT) && (ml->flag & MB_SCALE_RAD)) {
          color = col_radius_select;
        }
        else {
          color = col_radius;
        }

        if (is_select) {
          DRW_select_load_id(select_id | MBALLSEL_RADIUS);
        }

        DRW_shgroup_call_dynamic_add(group, draw_scale_xform, &ml->rad, color);

        if ((ml->flag & SELECT) && !(ml->flag & MB_SCALE_RAD)) {
          color = col_stiffness_select;
        }
        else {
          color = col_stiffness;
        }

        if (is_select) {
          DRW_select_load_id(select_id | MBALLSEL_STIFF);
        }

        DRW_shgroup_call_dynamic_add(group, draw_scale_xform, &draw_stiffness_radius, color);
      }
    }
  }
}

/* Draw time ! Control rendering pipeline from here */
static void EDIT_METABALL_draw_scene(void *vedata)
{
  EDIT_METABALL_PassList *psl = ((EDIT_METABALL_Data *)vedata)->psl;
  /* render passes on default framebuffer. */
  DRW_draw_pass(psl->pass);

  /* If you changed framebuffer, double check you rebind
   * the default one with its textures attached before finishing */

  DRW_state_clip_planes_reset();
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void EDIT_METABALL_engine_free(void)
{
  // DRW_SHADER_FREE_SAFE(custom_shader);
}

static const DrawEngineDataSize EDIT_METABALL_data_size = DRW_VIEWPORT_DATA_SIZE(
    EDIT_METABALL_Data);

DrawEngineType draw_engine_edit_metaball_type = {
    NULL,
    NULL,
    N_("EditMetaballMode"),
    &EDIT_METABALL_data_size,
    &EDIT_METABALL_engine_init,
    &EDIT_METABALL_engine_free,
    &EDIT_METABALL_cache_init,
    &EDIT_METABALL_cache_populate,
    NULL,
    NULL, /* draw_background but not needed by mode engines */
    &EDIT_METABALL_draw_scene,
    NULL,
    NULL,
};
