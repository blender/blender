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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "BLI_rect.h"

#include "BKE_editmesh.h"

#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "ED_view3d.h"

#include "GPU_shader.h"
#include "GPU_select.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "UI_resources.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "draw_cache_impl.h"

#include "select_engine.h"
/* Shaders */

#define SELECT_ENGINE "SELECT_ENGINE"

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct SELECTID_StorageList {
  struct SELECTID_PrivateData *g_data;
} SELECTID_StorageList;

typedef struct SELECTID_PassList {
  struct DRWPass *select_id_face_pass;
  struct DRWPass *select_id_edge_pass;
  struct DRWPass *select_id_vert_pass;
} SELECTID_PassList;

typedef struct SELECTID_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  SELECTID_PassList *psl;
  SELECTID_StorageList *stl;
} SELECTID_Data;

typedef struct SELECTID_Shaders {
  /* Depth Pre Pass */
  struct GPUShader *select_id_flat;
  struct GPUShader *select_id_uniform;
} SELECTID_Shaders;

/* *********** STATIC *********** */

static struct {
  SELECTID_Shaders sh_data[GPU_SHADER_CFG_LEN];

  struct GPUFrameBuffer *framebuffer_select_id;
  struct GPUTexture *texture_u32;

  struct {
    struct BaseOffset *base_array_index_offsets;
    uint bases_len;
    uint last_base_drawn;
    /** Total number of items `base_array_index_offsets[bases_len - 1].vert`. */
    uint last_index_drawn;

    struct Depsgraph *depsgraph;
    short select_mode;
  } context;
} e_data = {{{NULL}}}; /* Engine data */

typedef struct SELECTID_PrivateData {
  DRWShadingGroup *shgrp_face_unif;
  DRWShadingGroup *shgrp_face_flat;
  DRWShadingGroup *shgrp_edge;
  DRWShadingGroup *shgrp_vert;

  DRWView *view_faces;
  DRWView *view_edges;
  DRWView *view_verts;
} SELECTID_PrivateData; /* Transient data */

struct BaseOffset {
  /* For convenience only. */
  union {
    uint offset;
    uint face_start;
  };
  union {
    uint face;
    uint edge_start;
  };
  union {
    uint edge;
    uint vert_start;
  };
  uint vert;
};

/* Shaders */
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_selection_id_3D_vert_glsl[];
extern char datatoc_selection_id_frag_glsl[];

/* -------------------------------------------------------------------- */
/** \name Selection Utilities
 * \{ */

static void draw_select_framebuffer_select_id_setup(void)
{
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  int size[2];
  size[0] = GPU_texture_width(dtxl->depth);
  size[1] = GPU_texture_height(dtxl->depth);

  if (e_data.framebuffer_select_id == NULL) {
    e_data.framebuffer_select_id = GPU_framebuffer_create();
  }

  if ((e_data.texture_u32 != NULL) && ((GPU_texture_width(e_data.texture_u32) != size[0]) ||
                                       (GPU_texture_height(e_data.texture_u32) != size[1]))) {

    GPU_texture_free(e_data.texture_u32);
    e_data.texture_u32 = NULL;
  }

  if (e_data.texture_u32 == NULL) {
    e_data.texture_u32 = GPU_texture_create_2d(size[0], size[1], GPU_R32UI, NULL, NULL);

    GPU_framebuffer_texture_attach(e_data.framebuffer_select_id, dtxl->depth, 0, 0);
    GPU_framebuffer_texture_attach(e_data.framebuffer_select_id, e_data.texture_u32, 0, 0);
    GPU_framebuffer_check_valid(e_data.framebuffer_select_id, NULL);
  }
}

static void draw_select_id_object(void *vedata,
                                  Object *ob,
                                  short select_mode,
                                  bool draw_facedot,
                                  uint initial_offset,
                                  uint *r_vert_offset,
                                  uint *r_edge_offset,
                                  uint *r_face_offset)
{
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;

  BLI_assert(initial_offset > 0);

  switch (ob->type) {
    case OB_MESH:
      if (ob->mode & OB_MODE_EDIT) {
        Mesh *me = ob->data;
        BMEditMesh *em = me->edit_mesh;
        const bool use_faceselect = (select_mode & SCE_SELECT_FACE) != 0;

        DRW_mesh_batch_cache_validate(me);

        BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE | BM_FACE);

        struct GPUBatch *geom_faces, *geom_edges, *geom_verts, *geom_facedots;
        geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me);
        if (select_mode & SCE_SELECT_EDGE) {
          geom_edges = DRW_mesh_batch_cache_get_edges_with_select_id(me);
        }
        if (select_mode & SCE_SELECT_VERTEX) {
          geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(me);
        }
        if (use_faceselect && draw_facedot) {
          geom_facedots = DRW_mesh_batch_cache_get_facedots_with_select_id(me);
        }
        DRW_mesh_batch_cache_create_requested(ob, me, NULL, false, true);

        DRWShadingGroup *face_shgrp;
        if (use_faceselect) {
          face_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_face_flat);
          DRW_shgroup_uniform_int_copy(face_shgrp, "offset", *(int *)&initial_offset);

          if (draw_facedot) {
            DRW_shgroup_call(face_shgrp, geom_facedots, ob);
          }
          *r_face_offset = initial_offset + em->bm->totface;
        }
        else {
          face_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_face_unif);
          DRW_shgroup_uniform_int_copy(face_shgrp, "id", 0);

          *r_face_offset = initial_offset;
        }
        DRW_shgroup_call(face_shgrp, geom_faces, ob);

        /* Unlike faces, only draw edges if edge select mode. */
        if (select_mode & SCE_SELECT_EDGE) {
          DRWShadingGroup *edge_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_edge);
          DRW_shgroup_uniform_int_copy(edge_shgrp, "offset", *(int *)r_face_offset);
          DRW_shgroup_call(edge_shgrp, geom_edges, ob);
          *r_edge_offset = *r_face_offset + em->bm->totedge;
        }
        else {
          /* Note that `r_vert_offset` is calculated from `r_edge_offset`.
           * Otherwise the first vertex is never selected, see: T53512. */
          *r_edge_offset = *r_face_offset;
        }

        /* Unlike faces, only verts if vert select mode. */
        if (select_mode & SCE_SELECT_VERTEX) {
          DRWShadingGroup *vert_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_vert);
          DRW_shgroup_uniform_int_copy(vert_shgrp, "offset", *(int *)r_edge_offset);
          DRW_shgroup_call(vert_shgrp, geom_verts, ob);
          *r_vert_offset = *r_edge_offset + em->bm->totvert;
        }
        else {
          *r_vert_offset = *r_edge_offset;
        }
      }
      else {
        Mesh *me_orig = DEG_get_original_object(ob)->data;
        Mesh *me_eval = ob->data;

        DRW_mesh_batch_cache_validate(me_eval);
        struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me_eval);
        if ((me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) &&
            /* Currently vertex select supports weight paint and vertex paint. */
            ((ob->mode & OB_MODE_WEIGHT_PAINT) || (ob->mode & OB_MODE_VERTEX_PAINT))) {

          struct GPUBatch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(me_eval);
          DRW_mesh_batch_cache_create_requested(ob, me_eval, NULL, false, true);

          /* Only draw faces to mask out verts, we don't want their selection ID's. */
          DRWShadingGroup *face_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_face_unif);
          DRW_shgroup_uniform_int_copy(face_shgrp, "id", 0);
          DRW_shgroup_call(face_shgrp, geom_faces, ob);

          DRWShadingGroup *vert_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_vert);
          DRW_shgroup_uniform_int_copy(vert_shgrp, "offset", 1);
          DRW_shgroup_call(vert_shgrp, geom_verts, ob);

          *r_face_offset = *r_edge_offset = initial_offset;
          *r_vert_offset = me_eval->totvert + 1;
        }
        else {
          const bool use_hide = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL);
          DRW_mesh_batch_cache_create_requested(ob, me_eval, NULL, false, use_hide);

          DRWShadingGroup *face_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_face_flat);
          DRW_shgroup_uniform_int_copy(face_shgrp, "offset", *(int *)&initial_offset);
          DRW_shgroup_call(face_shgrp, geom_faces, ob);

          *r_face_offset = initial_offset + me_eval->totpoly;
          *r_edge_offset = *r_vert_offset = *r_face_offset;
        }
      }
      break;
    case OB_CURVE:
    case OB_SURF:
      break;
  }
}

static bool check_ob_drawface_dot(short select_mode, const View3D *v3d, char dt)
{
  if (select_mode & SCE_SELECT_FACE) {
    if ((dt < OB_SOLID) || XRAY_FLAG_ENABLED(v3d)) {
      return true;
    }
    if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) {
      return true;
    }
    if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGES) == 0) {
      /* Since we can't deduce face selection when edges aren't visible - show dots. */
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Functions
 * \{ */

static void select_engine_init(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  eGPUShaderConfig sh_cfg = draw_ctx->sh_cfg;

  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  SELECTID_Shaders *sh_data = &e_data.sh_data[sh_cfg];

  /* Prepass */
  if (!sh_data->select_id_flat) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->select_id_flat = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_selection_id_3D_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_selection_id_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }
  if (!sh_data->select_id_uniform) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->select_id_uniform = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_selection_id_3D_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_selection_id_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define UNIFORM_ID\n", NULL},
    });
  }

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  {
    /* Create view with depth offset */
    stl->g_data->view_faces = (DRWView *)DRW_view_default_get();
    stl->g_data->view_edges = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.0f);
    stl->g_data->view_verts = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.1f);
  }
}

static void select_cache_init(void *vedata)
{
  SELECTID_PassList *psl = ((SELECTID_Data *)vedata)->psl;
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SELECTID_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  {
    psl->select_id_face_pass = DRW_pass_create("Face Pass", DRW_STATE_DEFAULT);
    stl->g_data->shgrp_face_unif = DRW_shgroup_create(sh_data->select_id_uniform,
                                                      psl->select_id_face_pass);

    stl->g_data->shgrp_face_flat = DRW_shgroup_create(sh_data->select_id_flat,
                                                      psl->select_id_face_pass);

    psl->select_id_edge_pass = DRW_pass_create(
        "Edge Pass", DRW_STATE_DEFAULT | DRW_STATE_FIRST_VERTEX_CONVENTION);

    stl->g_data->shgrp_edge = DRW_shgroup_create(sh_data->select_id_flat,
                                                 psl->select_id_edge_pass);

    psl->select_id_vert_pass = DRW_pass_create("Vert Pass", DRW_STATE_DEFAULT);
    stl->g_data->shgrp_vert = DRW_shgroup_create(sh_data->select_id_flat,
                                                 psl->select_id_vert_pass);

    DRW_shgroup_uniform_float_copy(stl->g_data->shgrp_vert, "sizeVertex", G_draw.block.sizeVertex);

    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(stl->g_data->shgrp_face_unif, DRW_STATE_CLIP_PLANES);
      DRW_shgroup_state_enable(stl->g_data->shgrp_face_flat, DRW_STATE_CLIP_PLANES);
      DRW_shgroup_state_enable(stl->g_data->shgrp_edge, DRW_STATE_CLIP_PLANES);
      DRW_shgroup_state_enable(stl->g_data->shgrp_vert, DRW_STATE_CLIP_PLANES);
    }
  }

  e_data.context.last_base_drawn = 0;
  e_data.context.last_index_drawn = 1;
}

static void select_cache_populate(void *vedata, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  short select_mode = e_data.context.select_mode;

  if (select_mode == -1) {
    ToolSettings *ts = draw_ctx->scene->toolsettings;
    select_mode = ts->selectmode;
  }

  bool draw_facedot = check_ob_drawface_dot(select_mode, draw_ctx->v3d, ob->dt);

  struct BaseOffset *base_ofs =
      &e_data.context.base_array_index_offsets[e_data.context.last_base_drawn++];

  uint offset = e_data.context.last_index_drawn;

  draw_select_id_object(vedata,
                        ob,
                        select_mode,
                        draw_facedot,
                        offset,
                        &base_ofs->vert,
                        &base_ofs->edge,
                        &base_ofs->face);

  base_ofs->offset = offset;
  e_data.context.last_index_drawn = base_ofs->vert;
}

static void select_draw_scene(void *vedata)
{
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  SELECTID_PassList *psl = ((SELECTID_Data *)vedata)->psl;

  /* Setup framebuffer */
  draw_select_framebuffer_select_id_setup();
  GPU_framebuffer_bind(e_data.framebuffer_select_id);

  /* dithering and AA break color coding, so disable */
  glDisable(GL_DITHER);

  GPU_framebuffer_clear_color_depth(e_data.framebuffer_select_id, (const float[4]){0.0f}, 1.0f);

  DRW_view_set_active(stl->g_data->view_faces);
  DRW_draw_pass(psl->select_id_face_pass);

  DRW_view_set_active(stl->g_data->view_edges);
  DRW_draw_pass(psl->select_id_edge_pass);

  DRW_view_set_active(stl->g_data->view_verts);
  DRW_draw_pass(psl->select_id_vert_pass);
}

static void select_engine_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    SELECTID_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    DRW_SHADER_FREE_SAFE(sh_data->select_id_flat);
    DRW_SHADER_FREE_SAFE(sh_data->select_id_uniform);
  }

  DRW_TEXTURE_FREE_SAFE(e_data.texture_u32);
  GPU_FRAMEBUFFER_FREE_SAFE(e_data.framebuffer_select_id);
  MEM_SAFE_FREE(e_data.context.base_array_index_offsets);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exposed `DRW_engine.h` functions
 * \{ */

bool DRW_select_elem_get(const uint sel_id, uint *r_elem, uint *r_base_index, char *r_elem_type)
{
  char elem_type = 0;
  uint elem_id;
  uint base_index = 0;

  for (; base_index < e_data.context.bases_len; base_index++) {
    struct BaseOffset *base_ofs = &e_data.context.base_array_index_offsets[base_index];

    if (base_ofs->face > sel_id) {
      elem_id = sel_id - base_ofs->face_start;
      elem_type = SCE_SELECT_FACE;
      break;
    }
    if (base_ofs->edge > sel_id) {
      elem_id = sel_id - base_ofs->edge_start;
      elem_type = SCE_SELECT_EDGE;
      break;
    }
    if (base_ofs->vert > sel_id) {
      elem_id = sel_id - base_ofs->vert_start;
      elem_type = SCE_SELECT_VERTEX;
      break;
    }
  }

  if (base_index == e_data.context.bases_len) {
    return false;
  }

  *r_elem = elem_id;

  if (r_base_index) {
    *r_base_index = base_index;
  }

  if (r_elem_type) {
    *r_elem_type = elem_type;
  }

  return true;
}

uint DRW_select_context_offset_for_object_elem(const uint base_index, char elem_type)
{
  struct BaseOffset *base_ofs = &e_data.context.base_array_index_offsets[base_index];

  if (elem_type == SCE_SELECT_VERTEX) {
    return base_ofs->vert_start - 1;
  }
  if (elem_type == SCE_SELECT_EDGE) {
    return base_ofs->edge_start - 1;
  }
  if (elem_type == SCE_SELECT_FACE) {
    return base_ofs->face_start - 1;
  }
  BLI_assert(0);
  return 0;
}

uint DRW_select_context_elem_len(void)
{
  return e_data.context.last_index_drawn;
}

/* Read a block of pixels from the select frame buffer. */
void DRW_framebuffer_select_id_read(const rcti *rect, uint *r_buf)
{
  /* clamp rect by texture */
  rcti r = {
      .xmin = 0,
      .xmax = GPU_texture_width(e_data.texture_u32),
      .ymin = 0,
      .ymax = GPU_texture_height(e_data.texture_u32),
  };

  rcti rect_clamp = *rect;
  if (BLI_rcti_isect(&r, &rect_clamp, &rect_clamp)) {
    DRW_opengl_context_enable();
    GPU_framebuffer_bind(e_data.framebuffer_select_id);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(rect_clamp.xmin,
                 rect_clamp.ymin,
                 BLI_rcti_size_x(&rect_clamp),
                 BLI_rcti_size_y(&rect_clamp),
                 GL_RED_INTEGER,
                 GL_UNSIGNED_INT,
                 r_buf);

    GPU_framebuffer_restore();
    DRW_opengl_context_disable();

    if (!BLI_rcti_compare(rect, &rect_clamp)) {
      GPU_select_buffer_stride_realign(rect, &rect_clamp, r_buf);
    }
  }
  else {
    size_t buf_size = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect) * sizeof(*r_buf);

    memset(r_buf, 0, buf_size);
  }
}

void DRW_select_context_create(Depsgraph *depsgraph,
                               Base **UNUSED(bases),
                               const uint bases_len,
                               short select_mode)
{
  e_data.context.depsgraph = depsgraph;
  e_data.context.select_mode = select_mode;
  e_data.context.bases_len = bases_len;

  MEM_SAFE_FREE(e_data.context.base_array_index_offsets);
  e_data.context.base_array_index_offsets = MEM_mallocN(
      sizeof(*e_data.context.base_array_index_offsets) * bases_len, __func__);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Legacy
 * \{ */

void DRW_draw_select_id_object(Depsgraph *depsgraph,
                               ViewLayer *view_layer,
                               ARegion *ar,
                               View3D *v3d,
                               Object *ob,
                               short select_mode)
{
  Base *base = BKE_view_layer_base_find(view_layer, ob);
  DRW_draw_select_id(depsgraph, ar, v3d, &base, 1, select_mode);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize select_data_size = DRW_VIEWPORT_DATA_SIZE(SELECTID_Data);

DrawEngineType draw_engine_select_type = {
    NULL,
    NULL,
    N_("Select ID"),
    &select_data_size,
    &select_engine_init,
    &select_engine_free,
    &select_cache_init,
    &select_cache_populate,
    NULL,
    NULL,
    &select_draw_scene,
    NULL,
    NULL,
    NULL,
};

/* Note: currently unused, we may want to register so we can see this when debugging the view. */

RenderEngineType DRW_engine_viewport_select_type = {
    NULL,
    NULL,
    SELECT_ENGINE,
    N_("Select ID"),
    RE_INTERNAL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &draw_engine_select_type,
    {NULL, NULL, NULL},
};

/** \} */

#undef SELECT_ENGINE
