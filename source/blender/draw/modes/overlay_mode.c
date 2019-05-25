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

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "BIF_glutil.h"

#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_global.h"

#include "BLI_hash.h"

#include "GPU_shader.h"
#include "DRW_render.h"

#include "ED_view3d.h"

#include "draw_mode_engines.h"

#ifdef __APPLE__
#  define USE_GEOM_SHADER_WORKAROUND 1
#else
#  define USE_GEOM_SHADER_WORKAROUND 0
#endif

/* Structures */

typedef struct OVERLAY_DupliData {
  DRWShadingGroup *shgrp;
  struct GPUBatch *geom;
} OVERLAY_DupliData;

typedef struct OVERLAY_StorageList {
  struct OVERLAY_PrivateData *g_data;
} OVERLAY_StorageList;

typedef struct OVERLAY_PassList {
  struct DRWPass *face_orientation_pass;
  struct DRWPass *face_wireframe_pass;
} OVERLAY_PassList;

typedef struct OVERLAY_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  OVERLAY_PassList *psl;
  OVERLAY_StorageList *stl;
} OVERLAY_Data;

typedef struct OVERLAY_PrivateData {
  DRWShadingGroup *face_orientation_shgrp;
  DRWShadingGroup *face_wires_shgrp;
  DRWView *view_wires;
  BLI_mempool *wire_color_mempool;
  View3DOverlay overlay;
  float wire_step_param;
  bool ghost_stencil_test;
  bool show_overlays;
} OVERLAY_PrivateData; /* Transient data */

typedef struct OVERLAY_Shaders {
  /* Face orientation shader */
  struct GPUShader *face_orientation;
  /* Wireframe shader */
  struct GPUShader *select_wireframe;
  struct GPUShader *face_wireframe;
} OVERLAY_Shaders;

/* *********** STATIC *********** */
static struct {
  OVERLAY_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}};

/* Shaders */
extern char datatoc_overlay_face_orientation_frag_glsl[];
extern char datatoc_overlay_face_orientation_vert_glsl[];

extern char datatoc_overlay_face_wireframe_vert_glsl[];
extern char datatoc_overlay_face_wireframe_geom_glsl[];
extern char datatoc_overlay_face_wireframe_frag_glsl[];
extern char datatoc_gpu_shader_depth_only_frag_glsl[];

extern char datatoc_common_view_lib_glsl[];

/* Functions */
static void overlay_engine_init(void *vedata)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_StorageList *stl = data->stl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }
  stl->g_data->ghost_stencil_test = false;

  const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[draw_ctx->sh_cfg];

  if (!sh_data->face_orientation) {
    /* Face orientation */
    sh_data->face_orientation = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_overlay_face_orientation_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_overlay_face_orientation_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  if (!sh_data->face_wireframe) {
    sh_data->select_wireframe = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_overlay_face_wireframe_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg_data->lib, datatoc_overlay_face_wireframe_geom_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_depth_only_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define SELECT_EDGES\n", NULL},
    });
#if USE_GEOM_SHADER_WORKAROUND
    /* Apple drivers does not support wide wires. Use geometry shader as a workaround. */
    sh_data->face_wireframe = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_overlay_face_wireframe_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg_data->lib, datatoc_overlay_face_wireframe_geom_glsl, NULL},
        .frag = (const char *[]){datatoc_overlay_face_wireframe_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define USE_GEOM\n", NULL},
    });
#else
    sh_data->face_wireframe = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_overlay_face_wireframe_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_overlay_face_wireframe_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
#endif
  }

  stl->g_data->view_wires = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.0f);
}

static void overlay_cache_init(void *vedata)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_PassList *psl = data->psl;
  OVERLAY_StorageList *stl = data->stl;
  OVERLAY_PrivateData *g_data = stl->g_data;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  RegionView3D *rv3d = draw_ctx->rv3d;
  OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  const DRWContextState *DCS = DRW_context_state_get();

  View3D *v3d = DCS->v3d;
  if (v3d) {
    g_data->overlay = v3d->overlay;
    g_data->show_overlays = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
  }
  else {
    memset(&g_data->overlay, 0, sizeof(g_data->overlay));
    g_data->show_overlays = false;
  }

  if (g_data->show_overlays == false) {
    g_data->overlay.flag = 0;
  }

  if (v3d->shading.type == OB_WIRE) {
    g_data->overlay.flag |= V3D_OVERLAY_WIREFRAMES;

    if (ELEM(v3d->shading.wire_color_type, V3D_SHADING_OBJECT_COLOR, V3D_SHADING_RANDOM_COLOR)) {
      g_data->wire_color_mempool = BLI_mempool_create(sizeof(float[3]), 0, 512, 0);
    }
  }

  {
    /* Face Orientation Pass */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND;
    psl->face_orientation_pass = DRW_pass_create("Face Orientation", state);
    g_data->face_orientation_shgrp = DRW_shgroup_create(sh_data->face_orientation,
                                                        psl->face_orientation_pass);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(g_data->face_orientation_shgrp, rv3d);
    }
  }

  {
    /* Wireframe */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS |
                     DRW_STATE_FIRST_VERTEX_CONVENTION;
    float wire_size = U.pixelsize * 0.5f;

    float winmat[4][4];
    float viewdist = rv3d->dist;
    DRW_view_winmat_get(NULL, winmat, false);
    /* special exception for ortho camera (viewdist isnt used for perspective cameras) */
    if (rv3d->persp == RV3D_CAMOB && rv3d->is_persp == false) {
      viewdist = 1.0f / max_ff(fabsf(rv3d->winmat[0][0]), fabsf(rv3d->winmat[1][1]));
    }
    const float depth_ofs = bglPolygonOffsetCalc((float *)winmat, viewdist, 1.0f);

    const bool use_select = (DRW_state_is_select() || DRW_state_is_depth());
    GPUShader *face_wires_sh = use_select ? sh_data->select_wireframe : sh_data->face_wireframe;

    psl->face_wireframe_pass = DRW_pass_create("Face Wires", state);

    g_data->face_wires_shgrp = DRW_shgroup_create(face_wires_sh, psl->face_wireframe_pass);
    DRW_shgroup_uniform_float(
        g_data->face_wires_shgrp, "wireStepParam", &g_data->wire_step_param, 1);
    DRW_shgroup_uniform_float_copy(g_data->face_wires_shgrp, "ofs", depth_ofs);
    if (use_select || USE_GEOM_SHADER_WORKAROUND) {
      DRW_shgroup_uniform_float_copy(g_data->face_wires_shgrp, "wireSize", wire_size);
      DRW_shgroup_uniform_vec2(
          g_data->face_wires_shgrp, "viewportSize", DRW_viewport_size_get(), 1);
      DRW_shgroup_uniform_vec2(
          g_data->face_wires_shgrp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
    }
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(g_data->face_wires_shgrp, rv3d);
    }

    g_data->wire_step_param = stl->g_data->overlay.wireframe_threshold - 254.0f / 255.0f;
  }
}

static void overlay_wire_color_get(const View3D *v3d,
                                   const OVERLAY_PrivateData *pd,
                                   const Object *ob,
                                   const bool use_coloring,
                                   float **rim_col,
                                   float **wire_col)
{
#ifndef NDEBUG
  *rim_col = NULL;
  *wire_col = NULL;
#endif
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (UNLIKELY(ob->base_flag & BASE_FROM_SET)) {
    *rim_col = G_draw.block.colorDupli;
    *wire_col = G_draw.block.colorDupli;
  }
  else if (UNLIKELY(ob->base_flag & BASE_FROM_DUPLI)) {
    if (ob->base_flag & BASE_SELECTED) {
      if (G.moving & G_TRANSFORM_OBJ) {
        *rim_col = G_draw.block.colorTransform;
      }
      else {
        *rim_col = G_draw.block.colorDupliSelect;
      }
    }
    else {
      *rim_col = G_draw.block.colorDupli;
    }
    *wire_col = G_draw.block.colorDupli;
  }
  else if ((ob->base_flag & BASE_SELECTED) && use_coloring) {
    if (G.moving & G_TRANSFORM_OBJ) {
      *rim_col = G_draw.block.colorTransform;
    }
    else if (ob == draw_ctx->obact) {
      *rim_col = G_draw.block.colorActive;
    }
    else {
      *rim_col = G_draw.block.colorSelect;
    }
    *wire_col = G_draw.block.colorWire;
  }
  else {
    *rim_col = G_draw.block.colorWire;
    *wire_col = G_draw.block.colorBackground;
  }

  if (v3d->shading.type == OB_WIRE) {
    if (ELEM(v3d->shading.wire_color_type, V3D_SHADING_OBJECT_COLOR, V3D_SHADING_RANDOM_COLOR)) {
      *wire_col = BLI_mempool_alloc(pd->wire_color_mempool);
      *rim_col = BLI_mempool_alloc(pd->wire_color_mempool);

      if (v3d->shading.wire_color_type == V3D_SHADING_OBJECT_COLOR) {
        linearrgb_to_srgb_v3_v3(*wire_col, ob->color);
        mul_v3_fl(*wire_col, 0.5f);
        copy_v3_v3(*rim_col, *wire_col);
      }
      else {
        uint hash = BLI_ghashutil_strhash_p_murmur(ob->id.name);
        if (ob->id.lib) {
          hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob->id.lib->name);
        }

        float hue = BLI_hash_int_01(hash);
        float hsv[3] = {hue, 0.75f, 0.8f};
        hsv_to_rgb_v(hsv, *wire_col);
        copy_v3_v3(*rim_col, *wire_col);
      }

      if ((ob->base_flag & BASE_SELECTED) && use_coloring) {
        /* "Normalize" color. */
        add_v3_fl(*wire_col, 1e-4f);
        float brightness = max_fff((*wire_col)[0], (*wire_col)[1], (*wire_col)[2]);
        mul_v3_fl(*wire_col, (0.5f / brightness));
        add_v3_fl(*rim_col, 0.75f);
      }
      else {
        mul_v3_fl(*rim_col, 0.5f);
        add_v3_fl(*wire_col, 0.5f);
      }
    }
  }
  BLI_assert(*rim_col && *wire_col);
}

static void overlay_cache_populate(void *vedata, Object *ob)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_StorageList *stl = data->stl;
  OVERLAY_PrivateData *pd = stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;

  if ((ob->dt < OB_WIRE) || (!DRW_object_is_renderable(ob) && (ob->dt != OB_WIRE))) {
    return;
  }

  if (DRW_object_is_renderable(ob) && pd->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION) {
    struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      DRW_shgroup_call_object(pd->face_orientation_shgrp, geom, ob);
    }
  }

  if ((pd->overlay.flag & V3D_OVERLAY_WIREFRAMES) || (v3d->shading.type == OB_WIRE) ||
      (ob->dtx & OB_DRAWWIRE) || (ob->dt == OB_WIRE)) {

    /* Fast path for duplis. */
    OVERLAY_DupliData **dupli_data = (OVERLAY_DupliData **)DRW_duplidata_get(vedata);
    if (dupli_data) {
      if (*dupli_data == NULL) {
        *dupli_data = MEM_callocN(sizeof(OVERLAY_DupliData), "OVERLAY_DupliData");
      }
      else {
        if ((*dupli_data)->shgrp && (*dupli_data)->geom) {
          DRW_shgroup_call_object((*dupli_data)->shgrp, (*dupli_data)->geom, ob);
        }
        return;
      }
    }

    const bool is_edit_mode = BKE_object_is_in_editmode(ob);
    bool has_edit_mesh_cage = false;
    if (ob->type == OB_MESH) {
      /* TODO: Should be its own function. */
      Mesh *me = (Mesh *)ob->data;
      BMEditMesh *embm = me->edit_mesh;
      if (embm) {
        has_edit_mesh_cage = embm->mesh_eval_cage &&
                             (embm->mesh_eval_cage != embm->mesh_eval_final);
      }
    }

    /* Don't do that in edit Mesh mode, unless there is a modifier preview. */
    if ((!pd->show_overlays) ||
        (((ob != draw_ctx->object_edit) && !is_edit_mode) || has_edit_mesh_cage) ||
        ob->type != OB_MESH) {
      const bool is_sculpt_mode = DRW_object_use_pbvh_drawing(ob);
      const bool all_wires = (ob->dtx & OB_DRAW_ALL_EDGES);
      const bool is_wire = (ob->dt < OB_SOLID);
      const bool use_coloring = (pd->show_overlays && !is_edit_mode && !is_sculpt_mode &&
                                 !has_edit_mesh_cage);
      const int stencil_mask = (ob->dtx & OB_DRAWXRAY) ? 0x00 : 0xFF;
      float *rim_col, *wire_col;
      DRWShadingGroup *shgrp = NULL;

      overlay_wire_color_get(v3d, pd, ob, use_coloring, &rim_col, &wire_col);

      struct GPUBatch *geom;
      geom = DRW_cache_object_face_wireframe_get(ob);

      if (geom || is_sculpt_mode) {
        shgrp = DRW_shgroup_create_sub(pd->face_wires_shgrp);

        float wire_step_param = 10.0f;
        if (!is_sculpt_mode) {
          wire_step_param = (all_wires) ? 1.0f : pd->wire_step_param;
        }
        DRW_shgroup_uniform_float_copy(shgrp, "wireStepParam", wire_step_param);

        if (!(DRW_state_is_select() || DRW_state_is_depth())) {
          DRW_shgroup_stencil_mask(shgrp, stencil_mask);
          DRW_shgroup_uniform_vec3(shgrp, "wireColor", wire_col, 1);
          DRW_shgroup_uniform_vec3(shgrp, "rimColor", rim_col, 1);
        }

        if (is_sculpt_mode) {
          DRW_shgroup_call_sculpt(shgrp, ob, true, false, false);
        }
        else {
          DRW_shgroup_call_object(shgrp, geom, ob);
        }
      }

      if (dupli_data) {
        (*dupli_data)->shgrp = shgrp;
        (*dupli_data)->geom = geom;
      }

      if (is_wire && shgrp != NULL) {
        /* If object is wireframe, don't try to use stencil test. */
        DRW_shgroup_state_disable(shgrp, DRW_STATE_STENCIL_EQUAL);

        if (ob->dtx & OB_DRAWXRAY) {
          DRW_shgroup_state_disable(shgrp, DRW_STATE_DEPTH_LESS_EQUAL);
        }
      }
      else if ((ob->dtx & OB_DRAWXRAY) && shgrp != NULL) {
        pd->ghost_stencil_test = true;
      }
    }
  }
}

static void overlay_cache_finish(void *vedata)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_PassList *psl = data->psl;
  OVERLAY_StorageList *stl = data->stl;

  const DRWContextState *ctx = DRW_context_state_get();
  View3D *v3d = ctx->v3d;

  /* only in solid mode */
  if (v3d->shading.type == OB_SOLID && !XRAY_FLAG_ENABLED(v3d)) {
    if (stl->g_data->ghost_stencil_test) {
      DRW_pass_state_add(psl->face_wireframe_pass, DRW_STATE_STENCIL_EQUAL);
    }
  }
}

static void overlay_draw_scene(void *vedata)
{
  OVERLAY_Data *data = vedata;
  OVERLAY_PassList *psl = data->psl;
  OVERLAY_StorageList *stl = data->stl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(dfbl->default_fb);
  }
  DRW_draw_pass(psl->face_orientation_pass);

  /* This is replaced by the next code block  */
  // MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl);

  if (dfbl->multisample_fb != NULL) {
    DRW_stats_query_start("Multisample Blit");
    GPU_framebuffer_bind(dfbl->multisample_fb);
    GPU_framebuffer_clear_color(dfbl->multisample_fb, (const float[4]){0.0f});
    /* Special blit: we need the original depth and stencil
     * in the Multisample buffer. */
    GPU_framebuffer_blit(
        dfbl->default_fb, 0, dfbl->multisample_fb, 0, GPU_DEPTH_BIT | GPU_STENCIL_BIT);
    DRW_stats_query_end();
  }

  DRW_view_set_active(stl->g_data->view_wires);
  DRW_draw_pass(psl->face_wireframe_pass);

  DRW_view_set_active(NULL);

  /* TODO(fclem): find a way to unify the multisample pass together
   * (non meshes + armature + wireframe) */
  MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl);

  /* XXX TODO(fclem) do not discard data after drawing! Store them per viewport. */
  if (stl->g_data->wire_color_mempool) {
    BLI_mempool_destroy(stl->g_data->wire_color_mempool);
    stl->g_data->wire_color_mempool = NULL;
  }
}

static void overlay_engine_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    OVERLAY_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    GPUShader **sh_data_as_array = (GPUShader **)sh_data;
    for (int i = 0; i < (sizeof(OVERLAY_Shaders) / sizeof(GPUShader *)); i++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
    }
  }
}

static const DrawEngineDataSize overlay_data_size = DRW_VIEWPORT_DATA_SIZE(OVERLAY_Data);

DrawEngineType draw_engine_overlay_type = {
    NULL,
    NULL,
    N_("OverlayEngine"),
    &overlay_data_size,
    &overlay_engine_init,
    &overlay_engine_free,
    &overlay_cache_init,
    &overlay_cache_populate,
    &overlay_cache_finish,
    NULL,
    &overlay_draw_scene,
    NULL,
    NULL,
    NULL,
};
