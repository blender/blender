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

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "draw_cache_impl.h"

#include "select_private.h"

/* -------------------------------------------------------------------- */
/** \name Draw Utilities
 * \{ */

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

static void draw_select_id_paint_mesh(SELECTID_StorageList *stl,
                                      Object *ob,
                                      uint initial_offset,
                                      uint *r_vert_offset,
                                      uint *r_edge_offset,
                                      uint *r_face_offset)
{
  Mesh *me_orig = DEG_get_original_object(ob)->data;
  Mesh *me_eval = ob->data;

  struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me_eval);
  if ((me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) &&
      /* Currently vertex select supports weight paint and vertex paint. */
      ((ob->mode & OB_MODE_WEIGHT_PAINT) || (ob->mode & OB_MODE_VERTEX_PAINT))) {

    struct GPUBatch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(me_eval);

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
    DRWShadingGroup *face_shgrp = DRW_shgroup_create_sub(stl->g_data->shgrp_face_flat);
    DRW_shgroup_uniform_int_copy(face_shgrp, "offset", *(int *)&initial_offset);
    DRW_shgroup_call(face_shgrp, geom_faces, ob);

    *r_face_offset = initial_offset + me_eval->totpoly;
    *r_edge_offset = *r_vert_offset = *r_face_offset;
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
        draw_select_id_paint_mesh(
            stl, ob, initial_offset, r_vert_offset, r_edge_offset, r_face_offset);
      }
      break;
    case OB_CURVE:
    case OB_SURF:
      break;
  }
}

/** \} */

#undef SELECT_ENGINE
