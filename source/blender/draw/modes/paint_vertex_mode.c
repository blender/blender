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

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "DEG_depsgraph_query.h"

extern char datatoc_paint_face_vert_glsl[];
extern char datatoc_paint_weight_vert_glsl[];
extern char datatoc_paint_weight_frag_glsl[];
extern char datatoc_paint_vertex_vert_glsl[];
extern char datatoc_paint_vertex_frag_glsl[];
extern char datatoc_paint_wire_vert_glsl[];
extern char datatoc_paint_wire_frag_glsl[];
extern char datatoc_paint_vert_frag_glsl[];
extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** LISTS *********** */

enum {
  VERTEX_MODE = 0,
  WEIGHT_MODE = 1,
};
#define MODE_LEN (WEIGHT_MODE + 1)

typedef struct PAINT_VERTEX_PassList {
  struct {
    struct DRWPass *color_faces;
  } by_mode[MODE_LEN];
  struct DRWPass *wire_overlay;
  struct DRWPass *wire_select_overlay;
  struct DRWPass *face_select_overlay;
  struct DRWPass *vert_select_overlay;
} PAINT_VERTEX_PassList;

typedef struct PAINT_VERTEX_StorageList {
  struct PAINT_VERTEX_PrivateData *g_data;
} PAINT_VERTEX_StorageList;

typedef struct PAINT_VERTEX_Data {
  void *engine_type; /* Required */
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  PAINT_VERTEX_PassList *psl;
  PAINT_VERTEX_StorageList *stl;
} PAINT_VERTEX_Data;

typedef struct PAINT_VERTEX_Shaders {
  struct {
    struct GPUShader *color_face;
    struct GPUShader *wire_overlay;
    struct GPUShader *wire_select_overlay;
  } by_mode[MODE_LEN];
  struct GPUShader *face_select_overlay;
  struct GPUShader *vert_select_overlay;
} PAINT_VERTEX_Shaders;

/* *********** STATIC *********** */

static struct {
  PAINT_VERTEX_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{{{NULL}}}}}; /* Engine data */

typedef struct PAINT_VERTEX_PrivateData {
  struct {
    DRWShadingGroup *color_shgrp;
    DRWShadingGroup *lwire_shgrp;
    DRWShadingGroup *lwire_select_shgrp;
  } by_mode[MODE_LEN];
  DRWShadingGroup *face_select_shgrp;
  DRWShadingGroup *vert_select_shgrp;
  DRWView *view_wires;
} PAINT_VERTEX_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void PAINT_VERTEX_engine_init(void *vedata)
{
  PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  PAINT_VERTEX_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[draw_ctx->sh_cfg];

  if (!sh_data->face_select_overlay) {
    sh_data->by_mode[VERTEX_MODE].color_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_vertex_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_paint_vertex_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
    sh_data->by_mode[WEIGHT_MODE].color_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_paint_weight_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_paint_weight_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });

    sh_data->face_select_overlay = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_face_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
    sh_data->vert_select_overlay = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_wire_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_paint_vert_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define USE_SELECT\n", NULL},
    });

    const char *mode_defs[MODE_LEN] = {
        "#define VERTEX_MODE\n",
        "#define WEIGHT_MODE\n",
    };
    for (int i = 0; i < MODE_LEN; i++) {
      sh_data->by_mode[i].wire_overlay = GPU_shader_create_from_arrays({
          .vert = (const char *[]){sh_cfg_data->lib,
                                   datatoc_common_globals_lib_glsl,
                                   datatoc_common_view_lib_glsl,
                                   datatoc_paint_wire_vert_glsl,
                                   NULL},
          .frag = (const char *[]){datatoc_paint_wire_frag_glsl, NULL},
          .defs = (const char *[]){sh_cfg_data->def, mode_defs[i], NULL},
      });
      sh_data->by_mode[i].wire_select_overlay = GPU_shader_create_from_arrays({
          .vert = (const char *[]){sh_cfg_data->lib,
                                   datatoc_common_globals_lib_glsl,
                                   datatoc_common_view_lib_glsl,
                                   datatoc_paint_wire_vert_glsl,
                                   NULL},
          .frag = (const char *[]){datatoc_paint_wire_frag_glsl, NULL},
          .defs = (const char *[]){sh_cfg_data->def, mode_defs[i], "#define USE_SELECT\n", NULL},
      });
    }
  }

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  stl->g_data->view_wires = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.0f);
}

static void PAINT_VERTEX_cache_init(void *vedata)
{
  PAINT_VERTEX_PassList *psl = ((PAINT_VERTEX_Data *)vedata)->psl;
  PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;
  const RegionView3D *rv3d = draw_ctx->rv3d;
  PAINT_VERTEX_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  /* Vertex color pass */
  {
    DRWPass *pass = DRW_pass_create(
        "Vert Color Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_MULTIPLY);
    DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->by_mode[VERTEX_MODE].color_face, pass);
    DRW_shgroup_uniform_float_copy(
        shgrp, "white_factor", 1.0f - v3d->overlay.vertex_paint_mode_opacity);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
    }
    psl->by_mode[VERTEX_MODE].color_faces = pass;
    stl->g_data->by_mode[VERTEX_MODE].color_shgrp = shgrp;
  }

  /* Weight color pass */
  {
    DRWPass *pass = DRW_pass_create(
        "Weight Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_MULTIPLY);
    DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->by_mode[WEIGHT_MODE].color_face, pass);
    DRW_shgroup_uniform_bool_copy(
        shgrp, "drawContours", (v3d->overlay.wpaint_flag & V3D_OVERLAY_WPAINT_CONTOURS) != 0);
    DRW_shgroup_uniform_float(shgrp, "opacity", &v3d->overlay.weight_paint_mode_opacity, 1);
    DRW_shgroup_uniform_texture(shgrp, "colorramp", G_draw.weight_ramp);
    DRW_shgroup_uniform_block(shgrp, "globalsBlock", G_draw.block_ubo);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
    }
    psl->by_mode[WEIGHT_MODE].color_faces = pass;
    stl->g_data->by_mode[WEIGHT_MODE].color_shgrp = shgrp;
  }

  {
    DRWPass *pass = DRW_pass_create(
        "Wire Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
    for (int i = 0; i < MODE_LEN; i++) {
      DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->by_mode[i].wire_overlay, pass);
      DRW_shgroup_uniform_block(shgrp, "globalsBlock", G_draw.block_ubo);
      if (rv3d->rflag & RV3D_CLIPPING) {
        DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
      }
      stl->g_data->by_mode[i].lwire_shgrp = shgrp;
    }
    psl->wire_overlay = pass;
  }

  {
    DRWPass *pass = DRW_pass_create("Wire Mask Pass",
                                    DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                        DRW_STATE_DEPTH_LESS_EQUAL);
    for (int i = 0; i < MODE_LEN; i++) {
      DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->by_mode[i].wire_select_overlay, pass);
      DRW_shgroup_uniform_block(shgrp, "globalsBlock", G_draw.block_ubo);
      if (rv3d->rflag & RV3D_CLIPPING) {
        DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
      }
      stl->g_data->by_mode[i].lwire_select_shgrp = shgrp;
    }
    psl->wire_select_overlay = pass;
  }

  {
    static float col[4] = {1.0f, 1.0f, 1.0f, 0.2f};
    DRWPass *pass = DRW_pass_create("Face Mask Pass",
                                    DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                        DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND);
    DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->face_select_overlay, pass);
    DRW_shgroup_uniform_vec4(shgrp, "color", col, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
    }
    psl->face_select_overlay = pass;
    stl->g_data->face_select_shgrp = shgrp;
  }

  {
    DRWPass *pass = DRW_pass_create("Vert Mask Pass",
                                    DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                        DRW_STATE_DEPTH_LESS_EQUAL);
    DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->vert_select_overlay, pass);
    DRW_shgroup_uniform_block(shgrp, "globalsBlock", G_draw.block_ubo);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
    }
    psl->vert_select_overlay = pass;
    stl->g_data->vert_select_shgrp = shgrp;
  }
}

static void PAINT_VERTEX_cache_populate(void *vedata, Object *ob)
{
  PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;

  if ((ob->type == OB_MESH) && (ob == draw_ctx->obact)) {
    const int draw_mode = (ob->mode == OB_MODE_VERTEX_PAINT) ? VERTEX_MODE : WEIGHT_MODE;
    const Mesh *me = ob->data;
    const Mesh *me_orig = DEG_get_original_object(ob)->data;
    const bool use_wire = (v3d->overlay.paint_flag & V3D_OVERLAY_PAINT_WIRE) != 0;
    const bool use_face_sel = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
    const bool use_vert_sel = (me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

    struct GPUBatch *geom = NULL;
    if (draw_mode == VERTEX_MODE) {
      if (me->mloopcol == NULL) {
        return;
      }
      if (v3d->overlay.vertex_paint_mode_opacity != 0.0f) {
        geom = DRW_cache_mesh_surface_vertpaint_get(ob);
      }
    }
    else {
      if (v3d->overlay.weight_paint_mode_opacity != 0.0f) {
        geom = DRW_cache_mesh_surface_weights_get(ob);
      }
    }
    if (geom != NULL) {
      DRW_shgroup_call(stl->g_data->by_mode[draw_mode].color_shgrp, geom, ob->obmat);
    }

    if (use_face_sel || use_wire) {
      DRWShadingGroup *shgrp = use_face_sel ? stl->g_data->by_mode[draw_mode].lwire_select_shgrp :
                                              stl->g_data->by_mode[draw_mode].lwire_shgrp;
      geom = DRW_cache_mesh_surface_edges_get(ob);
      DRW_shgroup_call(shgrp, geom, ob->obmat);
    }

    if (use_face_sel) {
      geom = DRW_cache_mesh_surface_get(ob);
      DRW_shgroup_call(stl->g_data->face_select_shgrp, geom, ob->obmat);
    }

    if (use_vert_sel) {
      geom = DRW_cache_mesh_all_verts_get(ob);
      DRW_shgroup_call(stl->g_data->vert_select_shgrp, geom, ob->obmat);
    }
  }
}

static void PAINT_VERTEX_draw_scene(void *vedata)
{
  PAINT_VERTEX_PassList *psl = ((PAINT_VERTEX_Data *)vedata)->psl;
  PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
  for (int i = 0; i < MODE_LEN; i++) {
    DRW_draw_pass(psl->by_mode[i].color_faces);
  }
  DRW_draw_pass(psl->face_select_overlay);

  DRW_view_set_active(stl->g_data->view_wires);
  DRW_draw_pass(psl->wire_overlay);
  DRW_draw_pass(psl->wire_select_overlay);
  DRW_draw_pass(psl->vert_select_overlay);

  DRW_view_set_active(NULL);
}

static void PAINT_VERTEX_engine_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    PAINT_VERTEX_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    GPUShader **sh_data_as_array = (GPUShader **)sh_data;
    for (int i = 0; i < (sizeof(PAINT_VERTEX_Shaders) / sizeof(GPUShader *)); i++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
    }
  }
}

static const DrawEngineDataSize PAINT_VERTEX_data_size = DRW_VIEWPORT_DATA_SIZE(PAINT_VERTEX_Data);

DrawEngineType draw_engine_paint_vertex_type = {
    NULL,
    NULL,
    N_("PaintVertexMode"),
    &PAINT_VERTEX_data_size,
    &PAINT_VERTEX_engine_init,
    &PAINT_VERTEX_engine_free,
    &PAINT_VERTEX_cache_init,
    &PAINT_VERTEX_cache_populate,
    NULL,
    NULL,
    &PAINT_VERTEX_draw_scene,
    NULL,
    NULL,
};
