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

typedef struct EDIT_MESH_ComponentPassList {
  struct DRWPass *faces;
  struct DRWPass *faces_cage;
  struct DRWPass *edges;
  struct DRWPass *verts;
} EDIT_MESH_ComponentPassList;

typedef struct EDIT_MESH_PassList {
  struct DRWPass *weight_faces;
  struct DRWPass *depth_hidden_wire;
  struct DRWPass *depth_hidden_wire_in_front;

  EDIT_MESH_ComponentPassList edit_passes;
  EDIT_MESH_ComponentPassList edit_passes_in_front;

  struct DRWPass *mix_occlude;
  struct DRWPass *facefill_occlude;
  struct DRWPass *facefill_occlude_cage;
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

typedef struct EDIT_MESH_ComponentShadingGroupList {
  DRWShadingGroup *verts;
  DRWShadingGroup *edges;
  DRWShadingGroup *faces;
  DRWShadingGroup *faces_cage;
  DRWShadingGroup *facedots;
} EDIT_MESH_ComponentShadingGroupList;

typedef struct EDIT_MESH_PrivateData {
  /* weight */
  DRWShadingGroup *fweights_shgrp;
  DRWShadingGroup *depth_shgrp_hidden_wire;
  DRWShadingGroup *depth_shgrp_hidden_wire_in_front;

  DRWShadingGroup *fnormals_shgrp;
  DRWShadingGroup *vnormals_shgrp;
  DRWShadingGroup *lnormals_shgrp;

  EDIT_MESH_ComponentShadingGroupList edit_shgrps;
  EDIT_MESH_ComponentShadingGroupList edit_in_front_shgrps;

  DRWShadingGroup *vert_shgrp_in_front;
  DRWShadingGroup *edge_shgrp_in_front;
  DRWShadingGroup *face_shgrp_in_front;
  DRWShadingGroup *face_cage_shgrp_in_front;
  DRWShadingGroup *facedot_shgrp_in_front;

  DRWShadingGroup *facefill_occluded_shgrp;
  DRWShadingGroup *facefill_occluded_cage_shgrp;
  DRWShadingGroup *mesh_analysis_shgrp;

  DRWView *view_faces;
  DRWView *view_faces_cage;
  DRWView *view_edges;
  DRWView *view_wires;

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
  EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;

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

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  {
    /* Create view with depth offset */
    stl->g_data->view_faces = (DRWView *)DRW_view_default_get();
    stl->g_data->view_faces_cage = DRW_view_create_with_zoffset(draw_ctx->rv3d, 0.5f);
    stl->g_data->view_edges = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.0f);
    stl->g_data->view_wires = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.5f);
  }
}

static void edit_mesh_create_overlay_passes(float face_alpha,
                                            int *data_mask,
                                            bool do_edges,
                                            DRWState statemod,
                                            EDIT_MESH_ComponentPassList *passes,
                                            EDIT_MESH_ComponentShadingGroupList *shgrps)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  RegionView3D *rv3d = draw_ctx->rv3d;
  Scene *scene = draw_ctx->scene;
  ToolSettings *tsettings = scene->toolsettings;
  EDIT_MESH_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  const bool select_vert = (tsettings->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool select_face = (tsettings->selectmode & SCE_SELECT_FACE) != 0;
  const bool select_edge = (tsettings->selectmode & SCE_SELECT_EDGE) != 0;

  DRWShadingGroup *grp;

  GPUShader *vert_sh = sh_data->overlay_vert;
  GPUShader *edge_sh = (select_vert) ? sh_data->overlay_edge : sh_data->overlay_edge_flat;
  GPUShader *face_sh = sh_data->overlay_face;
  GPUShader *facedot_sh = sh_data->overlay_facedot;

  /* Faces */
  passes->faces = DRW_pass_create("Edit Mesh Faces", DRW_STATE_WRITE_COLOR | statemod);
  grp = shgrps->faces = DRW_shgroup_create(face_sh, passes->faces);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_float_copy(grp, "faceAlphaMod", face_alpha);
  DRW_shgroup_uniform_ivec4(grp, "dataMask", data_mask, 1);
  DRW_shgroup_uniform_bool_copy(grp, "selectFaces", select_face);
  if (rv3d->rflag & RV3D_CLIPPING) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }

  /* Cage geom needs to be offseted to avoid Z-fighting. */
  passes->faces_cage = DRW_pass_create("Edit Mesh Faces Cage", DRW_STATE_WRITE_COLOR | statemod);
  grp = shgrps->faces_cage = DRW_shgroup_create(face_sh, passes->faces_cage);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_float_copy(grp, "faceAlphaMod", face_alpha);
  DRW_shgroup_uniform_ivec4(grp, "dataMask", data_mask, 1);
  DRW_shgroup_uniform_bool_copy(grp, "selectFaces", select_face);
  if (rv3d->rflag & RV3D_CLIPPING) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }

  /* Edges */
  /* Change first vertex convention to match blender loop structure. */
  passes->edges = DRW_pass_create(
      "Edit Mesh Edges", DRW_STATE_WRITE_COLOR | DRW_STATE_FIRST_VERTEX_CONVENTION | statemod);
  grp = shgrps->edges = DRW_shgroup_create(edge_sh, passes->edges);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
  DRW_shgroup_uniform_vec2(grp, "viewportSizeInv", DRW_viewport_invert_size_get(), 1);
  DRW_shgroup_uniform_ivec4(grp, "dataMask", data_mask, 1);
  DRW_shgroup_uniform_bool_copy(grp, "selectEdges", do_edges || select_edge);
  if (rv3d->rflag & RV3D_CLIPPING) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }

  /* Verts */
  passes->verts = DRW_pass_create("Edit Mesh Verts",
                                  (DRW_STATE_WRITE_COLOR | statemod) & ~DRW_STATE_BLEND);
  if (select_vert) {
    grp = shgrps->verts = DRW_shgroup_create(vert_sh, passes->verts);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }
  }
  if (select_face) {
    grp = shgrps->facedots = DRW_shgroup_create(facedot_sh, passes->verts);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_state_enable(grp, DRW_STATE_WRITE_DEPTH);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }
  }
}

static void EDIT_MESH_cache_init(void *vedata)
{
  EDIT_MESH_PassList *psl = ((EDIT_MESH_Data *)vedata)->psl;
  EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;
  EDIT_MESH_PrivateData *g_data = stl->g_data;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  RegionView3D *rv3d = draw_ctx->rv3d;
  Scene *scene = draw_ctx->scene;
  ToolSettings *tsettings = scene->toolsettings;
  EDIT_MESH_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  bool do_occlude_wire = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_OCCLUDE_WIRE) != 0;

  g_data->ghost_ob = 0;
  g_data->edit_ob = 0;
  g_data->do_faces = true;
  g_data->do_edges = true;

  g_data->do_zbufclip = XRAY_FLAG_ENABLED(v3d);

  g_data->data_mask[0] = 0xFF; /* Face Flag */
  g_data->data_mask[1] = 0xFF; /* Edge Flag */
  g_data->data_mask[2] = 0xFF; /* Crease */
  g_data->data_mask[3] = 0xFF; /* BWeight */

  if (draw_ctx->object_edit->type == OB_MESH) {
    if (BKE_object_is_in_editmode(draw_ctx->object_edit)) {
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FREESTYLE_FACE) == 0) {
        g_data->data_mask[0] &= ~VFLAG_FACE_FREESTYLE;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACES) == 0) {
        g_data->data_mask[0] &= ~(VFLAG_FACE_SELECTED & VFLAG_FACE_FREESTYLE);
        g_data->do_faces = false;
        g_data->do_zbufclip = false;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_SEAMS) == 0) {
        g_data->data_mask[1] &= ~VFLAG_EDGE_SEAM;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_SHARP) == 0) {
        g_data->data_mask[1] &= ~VFLAG_EDGE_SHARP;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FREESTYLE_EDGE) == 0) {
        g_data->data_mask[1] &= ~VFLAG_EDGE_FREESTYLE;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGES) == 0) {
        if ((tsettings->selectmode & SCE_SELECT_EDGE) == 0) {
          g_data->do_edges = false;
        }
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_CREASES) == 0) {
        g_data->data_mask[2] = 0x0;
      }
      if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_BWEIGHTS) == 0) {
        g_data->data_mask[3] = 0x0;
      }
    }
  }

  float backwire_opacity = v3d->overlay.backwire_opacity;
  float size_normal = v3d->overlay.normals_length;
  float face_mod = (do_occlude_wire || !g_data->do_faces) ? 0.0f : 1.0f;

  {
    psl->weight_faces = DRW_pass_create(
        "Weight Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);

    g_data->fweights_shgrp = DRW_shgroup_create(sh_data->weight_face, psl->weight_faces);

    static float alpha = 1.0f;
    DRW_shgroup_uniform_float(g_data->fweights_shgrp, "opacity", &alpha, 1);
    DRW_shgroup_uniform_texture(g_data->fweights_shgrp, "colorramp", G_draw.weight_ramp);
    DRW_shgroup_uniform_block(g_data->fweights_shgrp, "globalsBlock", G_draw.block_ubo);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(g_data->fweights_shgrp, DRW_STATE_CLIP_PLANES);
    }
  }

  {
    /* Complementary Depth Pass */
    psl->depth_hidden_wire = DRW_pass_create("Depth Pass Hidden Wire",
                                             DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                                                 DRW_STATE_CULL_BACK);
    g_data->depth_shgrp_hidden_wire = DRW_shgroup_create(sh_data->depth, psl->depth_hidden_wire);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(g_data->depth_shgrp_hidden_wire, DRW_STATE_CLIP_PLANES);
    }

    psl->depth_hidden_wire_in_front = DRW_pass_create(
        "Depth Pass Hidden Wire In Front",
        DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK);
    g_data->depth_shgrp_hidden_wire_in_front = DRW_shgroup_create(sh_data->depth,
                                                                  psl->depth_hidden_wire_in_front);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(g_data->depth_shgrp_hidden_wire_in_front, DRW_STATE_CLIP_PLANES);
    }
  }

  {
    /* Normals */
    psl->normals = DRW_pass_create("Edit Mesh Normals Pass",
                                   DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR |
                                       DRW_STATE_DEPTH_LESS_EQUAL);

    g_data->fnormals_shgrp = DRW_shgroup_create(sh_data->normals_face, psl->normals);
    DRW_shgroup_uniform_float(g_data->fnormals_shgrp, "normalSize", &size_normal, 1);
    DRW_shgroup_uniform_vec4(g_data->fnormals_shgrp, "color", G_draw.block.colorNormal, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(g_data->fnormals_shgrp, DRW_STATE_CLIP_PLANES);
    }

    g_data->vnormals_shgrp = DRW_shgroup_create(sh_data->normals, psl->normals);
    DRW_shgroup_uniform_float(g_data->vnormals_shgrp, "normalSize", &size_normal, 1);
    DRW_shgroup_uniform_vec4(g_data->vnormals_shgrp, "color", G_draw.block.colorVNormal, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(g_data->vnormals_shgrp, DRW_STATE_CLIP_PLANES);
    }

    g_data->lnormals_shgrp = DRW_shgroup_create(sh_data->normals_loop, psl->normals);
    DRW_shgroup_uniform_float(g_data->lnormals_shgrp, "normalSize", &size_normal, 1);
    DRW_shgroup_uniform_vec4(g_data->lnormals_shgrp, "color", G_draw.block.colorLNormal, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(g_data->lnormals_shgrp, DRW_STATE_CLIP_PLANES);
    }
  }

  {
    /* Mesh Analysis Pass */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND;
    psl->mesh_analysis_pass = DRW_pass_create("Mesh Analysis", state);
    const bool is_vertex_color = scene->toolsettings->statvis.type == SCE_STATVIS_SHARP;
    g_data->mesh_analysis_shgrp = DRW_shgroup_create(
        is_vertex_color ? sh_data->mesh_analysis_vertex : sh_data->mesh_analysis_face,
        psl->mesh_analysis_pass);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(g_data->mesh_analysis_shgrp, DRW_STATE_CLIP_PLANES);
    }
  }
  /* For in front option */
  edit_mesh_create_overlay_passes(face_mod,
                                  g_data->data_mask,
                                  g_data->do_edges,
                                  DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND,
                                  &psl->edit_passes_in_front,
                                  &g_data->edit_in_front_shgrps);

  if (!g_data->do_zbufclip) {
    edit_mesh_create_overlay_passes(face_mod,
                                    g_data->data_mask,
                                    g_data->do_edges,
                                    DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND,
                                    &psl->edit_passes,
                                    &g_data->edit_shgrps);
  }
  else {
    /* We render all wires with depth and opaque to a new fbo and blend the result based on depth
     * values */
    edit_mesh_create_overlay_passes(0.0f,
                                    g_data->data_mask,
                                    g_data->do_edges,
                                    DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
                                    &psl->edit_passes,
                                    &g_data->edit_shgrps);

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND;
    psl->facefill_occlude = DRW_pass_create("Front Face Color", state);
    psl->facefill_occlude_cage = DRW_pass_create("Front Face Cage Color", state);

    if (g_data->do_faces) {
      DRWShadingGroup *shgrp;

      /* however we loose the front faces value (because we need the depth of occluded wires and
       * faces are alpha blended ) so we recover them in a new pass. */
      shgrp = g_data->facefill_occluded_shgrp = DRW_shgroup_create(sh_data->overlay_facefill,
                                                                   psl->facefill_occlude);
      DRW_shgroup_uniform_block(shgrp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_ivec4(shgrp, "dataMask", g_data->data_mask, 1);
      if (rv3d->rflag & RV3D_CLIPPING) {
        DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
      }

      shgrp = g_data->facefill_occluded_cage_shgrp = DRW_shgroup_create(
          sh_data->overlay_facefill, psl->facefill_occlude_cage);
      DRW_shgroup_uniform_block(shgrp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_ivec4(shgrp, "dataMask", g_data->data_mask, 1);
      if (rv3d->rflag & RV3D_CLIPPING) {
        DRW_shgroup_state_enable(shgrp, DRW_STATE_CLIP_PLANES);
      }
    }
    else {
      g_data->facefill_occluded_shgrp = NULL;
    }

    /* we need a full screen pass to combine the result */
    struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

    psl->mix_occlude = DRW_pass_create("Mix Occluded Wires",
                                       DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);
    DRWShadingGroup *mix_shgrp = DRW_shgroup_create(sh_data->overlay_mix, psl->mix_occlude);
    DRW_shgroup_call(mix_shgrp, quad, NULL);
    DRW_shgroup_uniform_float_copy(mix_shgrp, "alpha", backwire_opacity);
    DRW_shgroup_uniform_texture_ref(mix_shgrp, "wireColor", &e_data.occlude_wire_color_tx);
    DRW_shgroup_uniform_texture_ref(mix_shgrp, "wireDepth", &e_data.occlude_wire_depth_tx);
    DRW_shgroup_uniform_texture_ref(mix_shgrp, "sceneDepth", &dtxl->depth);
  }

  bool show_face_dots = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) != 0;

  if (g_data->do_faces == false && g_data->do_edges == false &&
      (tsettings->selectmode & SCE_SELECT_FACE)) {
    /* Force display of face centers in this case because that's
     * the only way to see if a face is selected. */
    show_face_dots = true;
  }

  /* HACK: set pointers to NULL even if generated. */
  if (!show_face_dots) {
    g_data->edit_shgrps.facedots = NULL;
    g_data->edit_in_front_shgrps.facedots = NULL;
  }
}

static void edit_mesh_add_ob_to_pass(Scene *scene,
                                     Object *ob,
                                     DRWShadingGroup *vert_shgrp,
                                     DRWShadingGroup *edge_shgrp,
                                     DRWShadingGroup *face_shgrp,
                                     DRWShadingGroup *face_cage_shgrp,
                                     DRWShadingGroup *facedot_shgrp)
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
                                 g_data->edit_shgrps.verts,
                                 g_data->edit_shgrps.edges,
                                 g_data->facefill_occluded_shgrp,
                                 g_data->facefill_occluded_cage_shgrp,
                                 g_data->edit_shgrps.facedots);
      }
      else if (do_in_front) {
        edit_mesh_add_ob_to_pass(scene,
                                 ob,
                                 g_data->edit_in_front_shgrps.verts,
                                 g_data->edit_in_front_shgrps.edges,
                                 g_data->edit_in_front_shgrps.faces,
                                 g_data->edit_in_front_shgrps.faces_cage,
                                 g_data->edit_in_front_shgrps.facedots);
      }
      else {
        edit_mesh_add_ob_to_pass(scene,
                                 ob,
                                 g_data->edit_shgrps.verts,
                                 g_data->edit_shgrps.edges,
                                 g_data->edit_shgrps.faces,
                                 g_data->edit_shgrps.faces_cage,
                                 g_data->edit_shgrps.facedots);
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

static void edit_mesh_draw_components(EDIT_MESH_ComponentPassList *passes,
                                      EDIT_MESH_PrivateData *g_data)
{
  DRW_view_set_active(g_data->view_faces);
  DRW_draw_pass(passes->faces);

  DRW_view_set_active(g_data->view_faces_cage);
  DRW_draw_pass(passes->faces_cage);

  DRW_view_set_active(g_data->view_edges);
  DRW_draw_pass(passes->edges);

  DRW_view_set_active(g_data->view_wires);
  DRW_draw_pass(passes->verts);

  DRW_view_set_active(NULL);
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
    DRW_view_set_active(stl->g_data->view_faces);
    DRW_draw_pass(psl->facefill_occlude);

    DRW_view_set_active(stl->g_data->view_faces_cage);
    DRW_draw_pass(psl->facefill_occlude_cage);

    DRW_view_set_active(NULL);

    /* Render wires on a separate framebuffer */
    GPU_framebuffer_bind(fbl->occlude_wire_fb);
    GPU_framebuffer_clear_color_depth(fbl->occlude_wire_fb, clearcol, 1.0f);
    DRW_draw_pass(psl->normals);

    edit_mesh_draw_components(&psl->edit_passes, stl->g_data);

    /* Combine with scene buffer */
    GPU_framebuffer_bind(dfbl->color_only_fb);
    DRW_draw_pass(psl->mix_occlude);
  }
  else {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    View3D *v3d = draw_ctx->v3d;

    DRW_draw_pass(psl->normals);
    edit_mesh_draw_components(&psl->edit_passes, stl->g_data);

    if (v3d->shading.type == OB_SOLID && !XRAY_FLAG_ENABLED(v3d) && stl->g_data->ghost_ob == 1 &&
        stl->g_data->edit_ob == 1) {
      /* In the case of single ghost object edit (common case for retopology):
       * we clear the depth buffer so that only the depth of the retopo mesh
       * is occluding the edit cage. */
      GPU_framebuffer_clear_depth(dfbl->default_fb, 1.0f);
    }

    DRW_draw_pass(psl->depth_hidden_wire_in_front);
    edit_mesh_draw_components(&psl->edit_passes_in_front, stl->g_data);
  }
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
