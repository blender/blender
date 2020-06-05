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

#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_select_buffer.h"

#include "draw_cache_impl.h"

#include "select_private.h"

/* -------------------------------------------------------------------- */
/** \name Draw Utilities
 * \{ */

void select_id_object_min_max(Object *obj, float r_min[3], float r_max[3])
{
  BoundBox *bb;
  BMEditMesh *em = BKE_editmesh_from_object(obj);
  if (em) {
    bb = BKE_editmesh_cage_boundbox_get(em);
  }
  else {
    bb = BKE_object_boundbox_get(obj);
  }
  copy_v3_v3(r_min, bb->vec[0]);
  copy_v3_v3(r_max, bb->vec[6]);
}

short select_id_get_object_select_mode(Scene *scene, Object *ob)
{
  short r_select_mode = 0;
  if (ob->mode & (OB_MODE_WEIGHT_PAINT | OB_MODE_VERTEX_PAINT | OB_MODE_TEXTURE_PAINT)) {
    /* In order to sample flat colors for vertex weights / texture-paint / vertex-paint
     * we need to be in SCE_SELECT_FACE mode so select_cache_init() correctly sets up
     * a shgroup with select_id_flat.
     * Note this is not working correctly for vertex-paint (yet), but has been discussed
     * in T66645 and there is a solution by @mano-wii in P1032.
     * So OB_MODE_VERTEX_PAINT is already included here [required for P1032 I guess]. */
    Mesh *me_orig = DEG_get_original_object(ob)->data;
    if (me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) {
      r_select_mode = SCE_SELECT_VERTEX;
    }
    else {
      r_select_mode = SCE_SELECT_FACE;
    }
  }
  else {
    r_select_mode = scene->toolsettings->selectmode;
  }

  return r_select_mode;
}

static bool check_ob_drawface_dot(short select_mode, const View3D *v3d, eDrawType dt)
{
  if (select_mode & SCE_SELECT_FACE) {
    if ((dt < OB_SOLID) || XRAY_FLAG_ENABLED(v3d)) {
      return true;
    }
    if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_DOT) {
      return true;
    }
  }
  return false;
}

static void draw_select_id_edit_mesh(SELECTID_StorageList *stl,
                                     Object *ob,
                                     short select_mode,
                                     bool draw_facedot,
                                     uint initial_offset,
                                     uint *r_vert_offset,
                                     uint *r_edge_offset,
                                     uint *r_face_offset)
{
  Mesh *me = ob->data;
  BMEditMesh *em = me->edit_mesh;

  BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE | BM_FACE);

  if (select_mode & SCE_SELECT_FACE) {
    struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me);
    DRWShadingGroup *face_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_face_flat);
    DRW_shgroup_uniform_int_copy(face_shgrp, "offset", *(int *)&initial_offset);
    DRW_shgroup_call_no_cull(face_shgrp, geom_faces, ob);

    if (draw_facedot) {
      struct GPUBatch *geom_facedots = DRW_mesh_batch_cache_get_facedots_with_select_id(me);
      DRW_shgroup_call_no_cull(face_shgrp, geom_facedots, ob);
    }
    *r_face_offset = initial_offset + em->bm->totface;
  }
  else {
    if (ob->dt >= OB_SOLID) {
#ifdef USE_CAGE_OCCLUSION
      struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me);
#else
      struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_surface(me);
#endif
      DRWShadingGroup *face_shgrp = stl->g_data->shgrp_face_unif;
      DRW_shgroup_call_no_cull(face_shgrp, geom_faces, ob);
    }
    *r_face_offset = initial_offset;
  }

  /* Unlike faces, only draw edges if edge select mode. */
  if (select_mode & SCE_SELECT_EDGE) {
    struct GPUBatch *geom_edges = DRW_mesh_batch_cache_get_edges_with_select_id(me);
    DRWShadingGroup *edge_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_edge);
    DRW_shgroup_uniform_int_copy(edge_shgrp, "offset", *(int *)r_face_offset);
    DRW_shgroup_call_no_cull(edge_shgrp, geom_edges, ob);
    *r_edge_offset = *r_face_offset + em->bm->totedge;
  }
  else {
    /* Note that `r_vert_offset` is calculated from `r_edge_offset`.
     * Otherwise the first vertex is never selected, see: T53512. */
    *r_edge_offset = *r_face_offset;
  }

  /* Unlike faces, only verts if vert select mode. */
  if (select_mode & SCE_SELECT_VERTEX) {
    struct GPUBatch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(me);
    DRWShadingGroup *vert_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_vert);
    DRW_shgroup_uniform_int_copy(vert_shgrp, "offset", *(int *)r_edge_offset);
    DRW_shgroup_call_no_cull(vert_shgrp, geom_verts, ob);
    *r_vert_offset = *r_edge_offset + em->bm->totvert;
  }
  else {
    *r_vert_offset = *r_edge_offset;
  }
}

static void draw_select_id_mesh(SELECTID_StorageList *stl,
                                Object *ob,
                                short select_mode,
                                uint initial_offset,
                                uint *r_vert_offset,
                                uint *r_edge_offset,
                                uint *r_face_offset)
{
  Mesh *me = ob->data;

  struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me);
  DRWShadingGroup *face_shgrp;
  if (select_mode & SCE_SELECT_FACE) {
    face_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_face_flat);
    DRW_shgroup_uniform_int_copy(face_shgrp, "offset", *(int *)&initial_offset);
    *r_face_offset = initial_offset + me->totpoly;
  }
  else {
    /* Only draw faces to mask out verts, we don't want their selection ID's. */
    face_shgrp = stl->g_data->shgrp_face_unif;
    *r_face_offset = initial_offset;
  }
  DRW_shgroup_call_no_cull(face_shgrp, geom_faces, ob);

  if (select_mode & SCE_SELECT_EDGE) {
    struct GPUBatch *geom_edges = DRW_mesh_batch_cache_get_edges_with_select_id(me);
    DRWShadingGroup *edge_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_edge);
    DRW_shgroup_uniform_int_copy(edge_shgrp, "offset", *(int *)r_face_offset);
    DRW_shgroup_call_no_cull(edge_shgrp, geom_edges, ob);
    *r_edge_offset = *r_face_offset + me->totedge;
  }
  else {
    *r_edge_offset = *r_face_offset;
  }

  if (select_mode & SCE_SELECT_VERTEX) {
    struct GPUBatch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(me);
    DRWShadingGroup *vert_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_vert);
    DRW_shgroup_uniform_int_copy(vert_shgrp, "offset", *r_edge_offset);
    DRW_shgroup_call_no_cull(vert_shgrp, geom_verts, ob);
    *r_vert_offset = *r_edge_offset + me->totvert;
  }
  else {
    *r_vert_offset = *r_edge_offset;
  }
}

void select_id_draw_object(void *vedata,
                           View3D *v3d,
                           Object *ob,
                           short select_mode,
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
        bool draw_facedot = check_ob_drawface_dot(select_mode, v3d, ob->dt);
        draw_select_id_edit_mesh(stl,
                                 ob,
                                 select_mode,
                                 draw_facedot,
                                 initial_offset,
                                 r_vert_offset,
                                 r_edge_offset,
                                 r_face_offset);
      }
      else {
        draw_select_id_mesh(
            stl, ob, select_mode, initial_offset, r_vert_offset, r_edge_offset, r_face_offset);
      }
      break;
    case OB_CURVE:
    case OB_SURF:
      break;
  }
}

/** \} */

#undef SELECT_ENGINE
