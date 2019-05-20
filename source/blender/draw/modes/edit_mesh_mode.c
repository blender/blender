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

#include "GPU_extensions.h"

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "draw_common.h"

#include "draw_cache_impl.h"
#include "draw_mode_engines.h"

#include "edit_mesh_mode_intern.h" /* own include */

#include "BKE_editmesh.h"
#include "BKE_object.h"

#include "BIF_glutil.h"

#include "BLI_dynstr.h"
#include "BLI_string_utils.h"

#include "ED_view3d.h"

extern char datatoc_paint_weight_vert_glsl[];
extern char datatoc_paint_weight_frag_glsl[];

extern char datatoc_edit_mesh_overlay_common_lib_glsl[];
extern char datatoc_edit_mesh_overlay_frag_glsl[];
extern char datatoc_edit_mesh_overlay_vert_glsl[];
extern char datatoc_edit_mesh_overlay_geom_glsl[];
extern char datatoc_edit_mesh_overlay_mix_frag_glsl[];
extern char datatoc_edit_mesh_overlay_facefill_vert_glsl[];
extern char datatoc_edit_mesh_overlay_facefill_frag_glsl[];
extern char datatoc_edit_mesh_overlay_mesh_analysis_frag_glsl[];
extern char datatoc_edit_mesh_overlay_mesh_analysis_vert_glsl[];
extern char datatoc_edit_normals_vert_glsl[];
extern char datatoc_edit_normals_geom_glsl[];
extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];
extern char datatoc_gpu_shader_flat_color_frag_glsl[];
extern char datatoc_gpu_shader_point_varying_color_frag_glsl[];

/* *********** LISTS *********** */
typedef struct EDIT_MESH_PassList {
  struct DRWPass *weight_faces;
  struct DRWPass *depth_hidden_wire;
  struct DRWPass *depth_hidden_wire_in_front;
  struct DRWPass *edit_face_overlay;
  struct DRWPass *edit_face_overlay_in_front;
  struct DRWPass *edit_face_in_front;
  struct DRWPass *edit_face_occluded;
  struct DRWPass *mix_occlude;
  struct DRWPass *facefill_occlude;
  struct DRWPass *mesh_analysis_pass;
  struct DRWPass *normals;
} EDIT_MESH_PassList;

typedef struct EDIT_MESH_FramebufferList {
  struct GPUFrameBuffer *occlude_wire_fb;
  struct GPUFrameBuffer *ghost_wire_fb;
} EDIT_MESH_FramebufferList;

typedef struct EDIT_MESH_StorageList {
  struct EDIT_MESH_PrivateData *g_data;
} EDIT_MESH_StorageList;

typedef struct EDIT_MESH_Data {
  void *engine_type;
  EDIT_MESH_FramebufferList *fbl;
  DRWViewportEmptyList *txl;
  EDIT_MESH_PassList *psl;
  EDIT_MESH_StorageList *stl;
} EDIT_MESH_Data;

#define MAX_SHADERS 16

/** Can only contain shaders (freed as array). */
typedef struct EDIT_MESH_Shaders {
  /* weight */
  GPUShader *weight_face;

  /* Geometry */
  GPUShader *overlay_vert;
  GPUShader *overlay_edge;
  GPUShader *overlay_edge_flat;
  GPUShader *overlay_face;
  GPUShader *overlay_facedot;

  GPUShader *overlay_mix;
  GPUShader *overlay_facefill;
  GPUShader *normals_face;
  GPUShader *normals_loop;
  GPUShader *normals;
  GPUShader *depth;

  /* Mesh analysis shader */
  GPUShader *mesh_analysis_face;
  GPUShader *mesh_analysis_vertex;
} EDIT_MESH_Shaders;

/* *********** STATIC *********** */
static struct {
  EDIT_MESH_Shaders sh_data[GPU_SHADER_CFG_LEN];

  /* temp buffer texture */
  struct GPUTexture *occlude_wire_depth_tx;
  struct GPUTexture *occlude_wire_color_tx;
} e_data = {{{NULL}}}; /* Engine data */

typedef struct EDIT_MESH_PrivateData {
  /* weight */
  DRWShadingGroup *fweights_shgrp;
  DRWShadingGroup *depth_shgrp_hidden_wire;
  DRWShadingGroup *depth_shgrp_hidden_wire_in_front;

  DRWShadingGroup *fnormals_shgrp;
  DRWShadingGroup *vnormals_shgrp;
  DRWShadingGroup *lnormals_shgrp;

  DRWShadingGroup *vert_shgrp;
  DRWShadingGroup *edge_shgrp;
  DRWShadingGroup *face_shgrp;
  DRWShadingGroup *face_cage_shgrp;
  DRWShadingGroup *facedot_shgrp;

  DRWShadingGroup *vert_shgrp_in_front;
  DRWShadingGroup *edge_shgrp_in_front;
  DRWShadingGroup *face_shgrp_in_front;
  DRWShadingGroup *face_cage_shgrp_in_front;
  DRWShadingGroup *facedot_shgrp_in_front;

  DRWShadingGroup *facefill_occluded_shgrp;
  DRWShadingGroup *mesh_analysis_shgrp;

  int data_mask[4];
  int ghost_ob;
  int edit_ob;
  bool do_zbufclip;
  bool do_faces;
  bool do_edges;
} EDIT_MESH_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void EDIT_MESH_engine_init(void *vedata)
{
  EDIT_MESH_FramebufferList *fbl = ((EDIT_MESH_Data *)vedata)->fbl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  EDIT_MESH_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  const float *viewport_size = DRW_viewport_size_get();
  const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

  e_data.occlude_wire_depth_tx = DRW_texture_pool_query_2d(
      size[0], size[1], GPU_DEPTH_COMPONENT24, &draw_engine_edit_mesh_type);
  e_data.occlude_wire_color_tx = DRW_texture_pool_query_2d(
      size[0], size[1], GPU_RGBA8, &draw_engine_edit_mesh_type);

  GPU_framebuffer_ensure_config(&fbl->occlude_wire_fb,
                                {GPU_ATTACHMENT_TEXTURE(e_data.occlude_wire_depth_tx),
                                 GPU_ATTACHMENT_TEXTURE(e_data.occlude_wire_color_tx)});

  if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_state_clip_planes_set_from_rv3d(draw_ctx->rv3d);
  }

  const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[draw_ctx->sh_cfg];

  if (!sh_data->weight_face) {
    sh_data->weight_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_weight_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_globals_lib_glsl,
                                 datatoc_paint_weight_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });

    char *lib = BLI_string_joinN(sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_mesh_overlay_common_lib_glsl);
    /* Use geometry shader to draw edge wireframe. This ensure us
     * the same result accross platforms and more flexibility. But
     * we pay the cost of running a geometry shader.
     * In the future we might consider using only the vertex shader
     * and loading data manually with buffer textures. */
    const bool use_geom_shader = true;
    const char *geom_sh_code[] = {lib, datatoc_edit_mesh_overlay_geom_glsl, NULL};
    if (!use_geom_shader) {
      geom_sh_code[0] = NULL;
    }
    const char *use_geom_def = use_geom_shader ? "#define USE_GEOM_SHADER\n" : "";
    const char *use_smooth_def = (U.gpu_flag & USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE) ?
                                     "" :
                                     "#define USE_SMOOTH_WIRE\n";
    sh_data->overlay_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define FACE\n", NULL},
    });
    sh_data->overlay_edge = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_vert_glsl, NULL},
        .frag = (const char *[]){lib, datatoc_edit_mesh_overlay_frag_glsl, NULL},
        .defs = (const char
                     *[]){sh_cfg_data->def, use_geom_def, use_smooth_def, "#define EDGE\n", NULL},
        .geom = (use_geom_shader) ? geom_sh_code : NULL,
    });
    sh_data->overlay_edge_flat = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_vert_glsl, NULL},
        .frag = (const char *[]){lib, datatoc_edit_mesh_overlay_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def,
                                 use_geom_def,
                                 use_smooth_def,
                                 "#define EDGE\n",
                                 "#define FLAT\n",
                                 NULL},
        .geom = (use_geom_shader) ? geom_sh_code : NULL,
    });
    sh_data->overlay_vert = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define VERT\n", NULL},
    });
    sh_data->overlay_facedot = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define FACEDOT\n", NULL},
    });
    sh_data->overlay_facefill = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_facefill_vert_glsl, NULL},
        .frag = (const char *[]){lib, datatoc_edit_mesh_overlay_facefill_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
    MEM_freeN(lib);

    sh_data->overlay_mix = DRW_shader_create_fullscreen(datatoc_edit_mesh_overlay_mix_frag_glsl,
                                                        NULL);

    lib = BLI_string_joinN(sh_cfg_data->lib, datatoc_common_view_lib_glsl);

    sh_data->normals_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_normals_vert_glsl, NULL},
        .geom = (const char *[]){lib, datatoc_edit_normals_geom_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define FACE_NORMALS\n", NULL},
    });

    sh_data->normals_loop = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_normals_vert_glsl, NULL},
        .geom = (const char *[]){lib, datatoc_edit_normals_geom_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define LOOP_NORMALS\n", NULL},
    });

    sh_data->normals = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_normals_vert_glsl, NULL},
        .geom = (const char *[]){lib, datatoc_edit_normals_geom_glsl, NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });

    /* Mesh Analysis */
    sh_data->mesh_analysis_face = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_mesh_analysis_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_edit_mesh_overlay_mesh_analysis_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define FACE_COLOR\n", NULL},
    });
    sh_data->mesh_analysis_vertex = GPU_shader_create_from_arrays({
        .vert = (const char *[]){lib, datatoc_edit_mesh_overlay_mesh_analysis_vert_glsl, NULL},
        .frag = (const char *[]){datatoc_edit_mesh_overlay_mesh_analysis_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define VERTEX_COLOR\n", NULL},
    });

    MEM_freeN(lib);

    sh_data->depth = DRW_shader_create_3d_depth_only(draw_ctx->sh_cfg);
  }
}

static DRWPass *edit_mesh_create_overlay_pass(float *face_alpha,
                                              int *data_mask,
                                              bool do_edges,
                                              DRWState statemod,
                                              DRWShadingGroup **r_face_shgrp,
                                              DRWShadingGroup **r_face_cage_shgrp,
                                              DRWShadingGroup **r_facedot_shgrp,
                                              DRWShadingGroup **r_edge_shgrp,
                                              DRWShadingGroup **r_vert_shgrp)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  RegionView3D *rv3d = draw_ctx->rv3d;
  Scene *scene = draw_ctx->scene;
  ToolSettings *tsettings = scene->toolsettings;
  EDIT_MESH_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  const bool select_vert = (tsettings->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool select_face = (tsettings->selectmode & SCE_SELECT_FACE) != 0;
  const bool select_edge = (tsettings->selectmode & SCE_SELECT_EDGE) != 0;

  float winmat[4][4];
  float viewdist = rv3d->dist;
  DRW_view_winmat_get(NULL, winmat, false);

  /* special exception for ortho camera (viewdist isnt used for perspective cameras) */
  if (rv3d->persp == RV3D_CAMOB && rv3d->is_persp == false) {
    viewdist = 1.0f / max_ff(fabsf(rv3d->winmat[0][0]), fabsf(rv3d->winmat[1][1]));
  }
  const float depth_ofs = bglPolygonOffsetCalc((float *)winmat, viewdist, 1.0f);

  DRWPass *pass = DRW_pass_create("Edit Mesh Face Overlay Pass", DRW_STATE_WRITE_COLOR | statemod);

  DRWShadingGroup *grp;

  GPUShader *vert_sh = sh_data->overlay_vert;
  GPUShader *edge_sh = (select_vert) ? sh_data->overlay_edge : sh_data->overlay_edge_flat;
  GPUShader *face_sh = sh_data->overlay_face;
  GPUShader *facedot_sh = sh_data->overlay_facedot;

  /* Faces */
  if (select_face) {
    grp = *r_facedot_shgrp = DRW_shgroup_create(facedot_sh, pass);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_state_enable(grp, DRW_STATE_WRITE_DEPTH);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(grp, rv3d);
    }
  }

  grp = *r_face_shgrp = DRW_shgroup_create(face_sh, pass);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_float(grp, "faceAlphaMod", face_alpha, 1);
  DRW_shgroup_uniform_ivec4(grp, "dataMask", data_mask, 1);
  DRW_shgroup_uniform_float_copy(grp, "ofs", 0.0f);
  DRW_shgroup_uniform_bool_copy(grp, "selectFaces", select_face);
  if (rv3d->rflag & RV3D_CLIPPING) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, rv3d);
  }

  /* Cage geom needs to be offseted to avoid Z-fighting. */
  grp = *r_face_cage_shgrp = DRW_shgroup_create_sub(*r_face_shgrp);
  DRW_shgroup_state_enable(grp, DRW_STATE_OFFSET_NEGATIVE);

  /* Edges */
  grp = *r_edge_shgrp = DRW_shgroup_create(edge_sh, pass);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
  DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
  DRW_shgroup_uniform_ivec4(grp, "dataMask", data_mask, 1);
  DRW_shgroup_uniform_bool_copy(grp, "doEdges", do_edges);
  DRW_shgroup_uniform_float_copy(grp, "ofs", depth_ofs);
  DRW_shgroup_uniform_bool_copy(grp, "selectEdges", select_edge);

  DRW_shgroup_state_enable(grp, DRW_STATE_OFFSET_NEGATIVE);
  /* To match blender loop structure. */
  DRW_shgroup_state_enable(grp, DRW_STATE_FIRST_VERTEX_CONVENTION);
  if (rv3d->rflag & RV3D_CLIPPING) {
    DRW_shgroup_world_clip_planes_from_rv3d(grp, rv3d);
  }

  /* Verts */
  if (select_vert) {
    grp = *r_vert_shgrp = DRW_shgroup_create(vert_sh, pass);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
    DRW_shgroup_uniform_float_copy(grp, "ofs", depth_ofs * 1.5f);
    DRW_shgroup_state_enable(grp, DRW_STATE_OFFSET_NEGATIVE | DRW_STATE_WRITE_DEPTH);
    DRW_shgroup_state_disable(grp, DRW_STATE_BLEND);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(grp, rv3d);
    }
  }

  return pass;
}

static float backwire_opacity;
static float face_mod;
static float size_normal;

static void EDIT_MESH_cache_init(void *vedata)
{
  EDIT_MESH_PassList *psl = ((EDIT_MESH_Data *)vedata)->psl;
  EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  RegionView3D *rv3d = draw_ctx->rv3d;
  Scene *scene = draw_ctx->scene;
  ToolSettings *tsettings = scene->toolsettings;
  EDIT_MESH_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  static float zero = 0.0f;

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }
  stl->g_data->ghost_ob = 0;
  stl->g_data->edit_ob = 0;
  stl->g_data->do_faces = true;
  stl->g_data->do_edges = true;

  stl->g_data->do_zbufclip = XRAY_FLAG_ENABLED(v3d);

  stl->g_data->data_mask[0] = 0xFF; /* Face Flag */
  stl->g_data->data_mask[1] = 0xFF; /* Edge Flag */
  stl->g_data->data_mask[2] = 0xFF; /* Crease */
  stl->g_data->data_mask[3] = 0xFF; /* BWeight */

  if (draw_ctx->object_edit->type == OB_MESH) {
    if (BKE_object_is_in_editmode(draw_ctx->object_edit)) {
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FREESTYLE_FACE) == 0) {
        stl->g_data->data_mask[0] &= ~VFLAG_FACE_FREESTYLE;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACES) == 0) {
        stl->g_data->data_mask[0] &= ~(VFLAG_FACE_SELECTED & VFLAG_FACE_FREESTYLE);
        stl->g_data->do_faces = false;
        stl->g_data->do_zbufclip = false;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_SEAMS) == 0) {
        stl->g_data->data_mask[1] &= ~VFLAG_EDGE_SEAM;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_SHARP) == 0) {
        stl->g_data->data_mask[1] &= ~VFLAG_EDGE_SHARP;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FREESTYLE_EDGE) == 0) {
        stl->g_data->data_mask[1] &= ~VFLAG_EDGE_FREESTYLE;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGES) == 0) {
        if ((tsettings->selectmode & SCE_SELECT_EDGE) == 0) {
          stl->g_data->data_mask[1] &= ~(VFLAG_EDGE_ACTIVE & VFLAG_EDGE_SELECTED);
          stl->g_data->do_edges = false;
        }
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_CREASES) == 0) {
        stl->g_data->data_mask[2] = 0x0;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_BWEIGHTS) == 0) {
        stl->g_data->data_mask[3] = 0x0;
      }
    }
  }

  {
    psl->weight_faces = DRW_pass_create(
        "Weight Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);

    stl->g_data->fweights_shgrp = DRW_shgroup_create(sh_data->weight_face, psl->weight_faces);

    static float alpha = 1.0f;
    DRW_shgroup_uniform_float(stl->g_data->fweights_shgrp, "opacity", &alpha, 1);
    DRW_shgroup_uniform_texture(stl->g_data->fweights_shgrp, "colorramp", G_draw.weight_ramp);
    DRW_shgroup_uniform_block(stl->g_data->fweights_shgrp, "globalsBlock", G_draw.block_ubo);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->fweights_shgrp, rv3d);
    }
  }

  {
    /* Complementary Depth Pass */
    psl->depth_hidden_wire = DRW_pass_create("Depth Pass Hidden Wire",
                                             DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                                                 DRW_STATE_CULL_BACK);
    stl->g_data->depth_shgrp_hidden_wire = DRW_shgroup_create(sh_data->depth,
                                                              psl->depth_hidden_wire);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->depth_shgrp_hidden_wire, rv3d);
    }

    psl->depth_hidden_wire_in_front = DRW_pass_create(
        "Depth Pass Hidden Wire In Front",
        DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK);
    stl->g_data->depth_shgrp_hidden_wire_in_front = DRW_shgroup_create(
        sh_data->depth, psl->depth_hidden_wire_in_front);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->depth_shgrp_hidden_wire_in_front, rv3d);
    }
  }

  {
    /* Normals */
    psl->normals = DRW_pass_create("Edit Mesh Normals Pass",
                                   DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR |
                                       DRW_STATE_DEPTH_LESS_EQUAL);

    stl->g_data->fnormals_shgrp = DRW_shgroup_create(sh_data->normals_face, psl->normals);
    DRW_shgroup_uniform_float(stl->g_data->fnormals_shgrp, "normalSize", &size_normal, 1);
    DRW_shgroup_uniform_vec4(stl->g_data->fnormals_shgrp, "color", G_draw.block.colorNormal, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->fnormals_shgrp, rv3d);
    }

    stl->g_data->vnormals_shgrp = DRW_shgroup_create(sh_data->normals, psl->normals);
    DRW_shgroup_uniform_float(stl->g_data->vnormals_shgrp, "normalSize", &size_normal, 1);
    DRW_shgroup_uniform_vec4(stl->g_data->vnormals_shgrp, "color", G_draw.block.colorVNormal, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->vnormals_shgrp, rv3d);
    }

    stl->g_data->lnormals_shgrp = DRW_shgroup_create(sh_data->normals_loop, psl->normals);
    DRW_shgroup_uniform_float(stl->g_data->lnormals_shgrp, "normalSize", &size_normal, 1);
    DRW_shgroup_uniform_vec4(stl->g_data->lnormals_shgrp, "color", G_draw.block.colorLNormal, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->lnormals_shgrp, rv3d);
    }
  }

  {
    /* Mesh Analysis Pass */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND;
    psl->mesh_analysis_pass = DRW_pass_create("Mesh Analysis", state);
    const bool is_vertex_color = scene->toolsettings->statvis.type == SCE_STATVIS_SHARP;
    stl->g_data->mesh_analysis_shgrp = DRW_shgroup_create(
        is_vertex_color ? sh_data->mesh_analysis_vertex : sh_data->mesh_analysis_face,
        psl->mesh_analysis_pass);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->mesh_analysis_shgrp, rv3d);
    }
  }
  /* For in front option */
  psl->edit_face_overlay_in_front = edit_mesh_create_overlay_pass(
      &face_mod,
      stl->g_data->data_mask,
      stl->g_data->do_edges,
      DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND,
      &stl->g_data->face_shgrp_in_front,
      &stl->g_data->face_cage_shgrp_in_front,
      &stl->g_data->facedot_shgrp_in_front,
      &stl->g_data->edge_shgrp_in_front,
      &stl->g_data->vert_shgrp_in_front);

  if (!stl->g_data->do_zbufclip) {
    psl->edit_face_overlay = edit_mesh_create_overlay_pass(&face_mod,
                                                           stl->g_data->data_mask,
                                                           stl->g_data->do_edges,
                                                           DRW_STATE_DEPTH_LESS_EQUAL |
                                                               DRW_STATE_BLEND,
                                                           &stl->g_data->face_shgrp,
                                                           &stl->g_data->face_cage_shgrp,
                                                           &stl->g_data->facedot_shgrp,
                                                           &stl->g_data->edge_shgrp,
                                                           &stl->g_data->vert_shgrp);
  }
  else {
    /* We render all wires with depth and opaque to a new fbo and blend the result based on depth
     * values */
    psl->edit_face_occluded = edit_mesh_create_overlay_pass(&zero,
                                                            stl->g_data->data_mask,
                                                            stl->g_data->do_edges,
                                                            DRW_STATE_DEPTH_LESS_EQUAL |
                                                                DRW_STATE_WRITE_DEPTH,
                                                            &stl->g_data->face_shgrp,
                                                            &stl->g_data->face_cage_shgrp,
                                                            &stl->g_data->facedot_shgrp,
                                                            &stl->g_data->edge_shgrp,
                                                            &stl->g_data->vert_shgrp);

    /* however we loose the front faces value (because we need the depth of occluded wires and
     * faces are alpha blended ) so we recover them in a new pass. */
    psl->facefill_occlude = DRW_pass_create(
        "Front Face Color", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND);
    stl->g_data->facefill_occluded_shgrp = DRW_shgroup_create(sh_data->overlay_facefill,
                                                              psl->facefill_occlude);
    DRW_shgroup_uniform_block(
        stl->g_data->facefill_occluded_shgrp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_ivec4(
        stl->g_data->facefill_occluded_shgrp, "dataMask", stl->g_data->data_mask, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->facefill_occluded_shgrp, rv3d);
    }

    /* we need a full screen pass to combine the result */
    struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

    psl->mix_occlude = DRW_pass_create("Mix Occluded Wires",
                                       DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);
    DRWShadingGroup *mix_shgrp = DRW_shgroup_create(sh_data->overlay_mix, psl->mix_occlude);
    DRW_shgroup_call(mix_shgrp, quad, NULL);
    DRW_shgroup_uniform_float(mix_shgrp, "alpha", &backwire_opacity, 1);
    DRW_shgroup_uniform_texture_ref(mix_shgrp, "wireColor", &e_data.occlude_wire_color_tx);
    DRW_shgroup_uniform_texture_ref(mix_shgrp, "wireDepth", &e_data.occlude_wire_depth_tx);
    DRW_shgroup_uniform_texture_ref(mix_shgrp, "sceneDepth", &dtxl->depth);
  }
}

static void edit_mesh_add_ob_to_pass(Scene *scene,
                                     Object *ob,
                                     DRWShadingGroup *vert_shgrp,
                                     DRWShadingGroup *edge_shgrp,
                                     DRWShadingGroup *face_shgrp,
                                     DRWShadingGroup *face_cage_shgrp,
                                     DRWShadingGroup *facedot_shgrp,
                                     DRWShadingGroup *facefill_shgrp)
{
  struct GPUBatch *geom_tris, *geom_verts, *geom_edges, *geom_fcenter;
  ToolSettings *tsettings = scene->toolsettings;

  bool has_edit_mesh_cage = false;
  /* TODO: Should be its own function. */
  Mesh *me = (Mesh *)ob->data;
  BMEditMesh *embm = me->edit_mesh;
  if (embm) {
    has_edit_mesh_cage = embm->mesh_eval_cage && (embm->mesh_eval_cage != embm->mesh_eval_final);
  }

  face_shgrp = (has_edit_mesh_cage) ? face_cage_shgrp : face_shgrp;
  face_shgrp = (facefill_shgrp != NULL) ? facefill_shgrp : face_shgrp;

  geom_tris = DRW_mesh_batch_cache_get_edit_triangles(ob->data);
  geom_edges = DRW_mesh_batch_cache_get_edit_edges(ob->data);
  DRW_shgroup_call(edge_shgrp, geom_edges, ob->obmat);
  DRW_shgroup_call(face_shgrp, geom_tris, ob->obmat);

  if ((tsettings->selectmode & SCE_SELECT_VERTEX) != 0) {
    geom_verts = DRW_mesh_batch_cache_get_edit_vertices(ob->data);
    DRW_shgroup_call(vert_shgrp, geom_verts, ob->obmat);
  }

  if (facedot_shgrp && (tsettings->selectmode & SCE_SELECT_FACE) != 0) {
    geom_fcenter = DRW_mesh_batch_cache_get_edit_facedots(ob->data);
    DRW_shgroup_call(facedot_shgrp, geom_fcenter, ob->obmat);
  }
}

static void EDIT_MESH_cache_populate(void *vedata, Object *ob)
{
  EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;
  EDIT_MESH_PrivateData *g_data = stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  Scene *scene = draw_ctx->scene;
  ToolSettings *tsettings = scene->toolsettings;
  struct GPUBatch *geom;

  if (ob->type == OB_MESH) {
    if ((ob == draw_ctx->object_edit) || BKE_object_is_in_editmode(ob)) {
      bool do_in_front = (ob->dtx & OB_DRAWXRAY) != 0;
      bool do_occlude_wire = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_OCCLUDE_WIRE) != 0;
      bool do_show_weight = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_WEIGHT) != 0;
      bool do_show_mesh_analysis = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_STATVIS) != 0;
      bool fnormals_do = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_NORMALS) != 0;
      bool vnormals_do = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_VERT_NORMALS) != 0;
      bool lnormals_do = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_LOOP_NORMALS) != 0;

      bool show_face_dots = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) != 0;

      if (g_data->do_faces == false && g_data->do_edges == false &&
          (tsettings->selectmode & SCE_SELECT_FACE)) {
        /* Force display of face centers in this case because that's
         * the only way to see if a face is selected. */
        show_face_dots = true;
      }

      /* Updating uniform */
      backwire_opacity = v3d->overlay.backwire_opacity;
      size_normal = v3d->overlay.normals_length;

      face_mod = (do_occlude_wire) ? 0.0f : 1.0f;

      if (!g_data->do_faces) {
        face_mod = 0.0f;
      }

      if (do_show_weight) {
        geom = DRW_cache_mesh_surface_weights_get(ob);
        DRW_shgroup_call(g_data->fweights_shgrp, geom, ob->obmat);
      }

      if (do_show_mesh_analysis) {
        Mesh *me = (Mesh *)ob->data;
        BMEditMesh *embm = me->edit_mesh;
        const bool is_original = embm->mesh_eval_final &&
                                 (embm->mesh_eval_final->runtime.is_original == true);
        if (is_original) {
          geom = DRW_cache_mesh_surface_mesh_analysis_get(ob);
          if (geom) {
            DRW_shgroup_call(g_data->mesh_analysis_shgrp, geom, ob->obmat);
          }
        }
      }

      if (do_occlude_wire || do_in_front) {
        geom = DRW_cache_mesh_surface_get(ob);
        DRW_shgroup_call(do_in_front ? g_data->depth_shgrp_hidden_wire_in_front :
                                       g_data->depth_shgrp_hidden_wire,
                         geom,
                         ob->obmat);
      }

      if (vnormals_do) {
        geom = DRW_mesh_batch_cache_get_edit_vertices(ob->data);
        DRW_shgroup_call(g_data->vnormals_shgrp, geom, ob->obmat);
      }
      if (lnormals_do) {
        geom = DRW_mesh_batch_cache_get_edit_lnors(ob->data);
        DRW_shgroup_call(g_data->lnormals_shgrp, geom, ob->obmat);
      }
      if (fnormals_do) {
        geom = DRW_mesh_batch_cache_get_edit_facedots(ob->data);
        DRW_shgroup_call(g_data->fnormals_shgrp, geom, ob->obmat);
      }

      if (g_data->do_zbufclip) {
        edit_mesh_add_ob_to_pass(scene,
                                 ob,
                                 g_data->vert_shgrp,
                                 g_data->edge_shgrp,
                                 g_data->face_shgrp,
                                 g_data->face_cage_shgrp,
                                 g_data->facedot_shgrp,
                                 (g_data->do_faces) ? g_data->facefill_occluded_shgrp : NULL);
      }
      else if (do_in_front) {
        edit_mesh_add_ob_to_pass(scene,
                                 ob,
                                 g_data->vert_shgrp_in_front,
                                 g_data->edge_shgrp_in_front,
                                 g_data->face_shgrp_in_front,
                                 g_data->face_cage_shgrp_in_front,
                                 (show_face_dots) ? g_data->facedot_shgrp_in_front : NULL,
                                 NULL);
      }
      else {
        edit_mesh_add_ob_to_pass(scene,
                                 ob,
                                 g_data->vert_shgrp,
                                 g_data->edge_shgrp,
                                 g_data->face_shgrp,
                                 g_data->face_cage_shgrp,
                                 (show_face_dots) ? g_data->facedot_shgrp : NULL,
                                 NULL);
      }

      g_data->ghost_ob += (ob->dtx & OB_DRAWXRAY) ? 1 : 0;
      g_data->edit_ob += 1;

      /* 3D text overlay */
      if (v3d->overlay.edit_flag &
          (V3D_OVERLAY_EDIT_EDGE_LEN | V3D_OVERLAY_EDIT_FACE_AREA | V3D_OVERLAY_EDIT_FACE_ANG |
           V3D_OVERLAY_EDIT_EDGE_ANG | V3D_OVERLAY_EDIT_INDICES)) {
        if (DRW_state_show_text()) {
          DRW_edit_mesh_mode_text_measure_stats(draw_ctx->ar, v3d, ob, &scene->unit);
        }
      }
    }
  }
}

static void EDIT_MESH_draw_scene(void *vedata)
{
  EDIT_MESH_PassList *psl = ((EDIT_MESH_Data *)vedata)->psl;
  EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;
  EDIT_MESH_FramebufferList *fbl = ((EDIT_MESH_Data *)vedata)->fbl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  DRW_draw_pass(psl->weight_faces);
  DRW_draw_pass(psl->mesh_analysis_pass);

  DRW_draw_pass(psl->depth_hidden_wire);

  if (stl->g_data->do_zbufclip) {
    float clearcol[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    DRW_draw_pass(psl->depth_hidden_wire_in_front);

    /* render facefill */
    DRW_draw_pass(psl->facefill_occlude);

    /* Render wires on a separate framebuffer */
    GPU_framebuffer_bind(fbl->occlude_wire_fb);
    GPU_framebuffer_clear_color_depth(fbl->occlude_wire_fb, clearcol, 1.0f);
    DRW_draw_pass(psl->normals);
    DRW_draw_pass(psl->edit_face_occluded);

    /* Combine with scene buffer */
    GPU_framebuffer_bind(dfbl->color_only_fb);
    DRW_draw_pass(psl->mix_occlude);
  }
  else {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    View3D *v3d = draw_ctx->v3d;

    DRW_draw_pass(psl->normals);
    DRW_draw_pass(psl->edit_face_overlay);

    if (v3d->shading.type == OB_SOLID && !XRAY_FLAG_ENABLED(v3d) && stl->g_data->ghost_ob == 1 &&
        stl->g_data->edit_ob == 1) {
      /* In the case of single ghost object edit (common case for retopology):
       * we clear the depth buffer so that only the depth of the retopo mesh
       * is occluding the edit cage. */
      GPU_framebuffer_clear_depth(dfbl->default_fb, 1.0f);
    }

    DRW_draw_pass(psl->depth_hidden_wire_in_front);
    DRW_draw_pass(psl->edit_face_overlay_in_front);
  }

  DRW_state_clip_planes_reset();
}

static void EDIT_MESH_engine_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    EDIT_MESH_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    /* Don't free builtins. */
    sh_data->depth = NULL;
    GPUShader **sh_data_as_array = (GPUShader **)sh_data;
    for (int i = 0; i < (sizeof(EDIT_MESH_Shaders) / sizeof(GPUShader *)); i++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
    }
  }
}

static const DrawEngineDataSize EDIT_MESH_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_MESH_Data);

DrawEngineType draw_engine_edit_mesh_type = {
    NULL,
    NULL,
    N_("EditMeshMode"),
    &EDIT_MESH_data_size,
    &EDIT_MESH_engine_init,
    &EDIT_MESH_engine_free,
    &EDIT_MESH_cache_init,
    &EDIT_MESH_cache_populate,
    NULL,
    NULL,
    &EDIT_MESH_draw_scene,
    NULL,
    NULL,
};
