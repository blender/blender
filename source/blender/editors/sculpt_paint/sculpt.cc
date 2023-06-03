/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 by Nicholas Bishop. All rights reserved. */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode tools.
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_math_vector.hh"
#include "BLI_set.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_dyntopo.hh"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node_runtime.hh"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "NOD_texture.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "../../bmesh/intern/bmesh_idmap.h"
#include "bmesh.h"

using blender::float3;
using blender::IndexRange;
using blender::int2;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::Vector;

using namespace blender::bke::paint;

static bool is_realtime_restored(Brush *brush)
{
  return ((brush->flag & BRUSH_ANCHORED) ||
          (ELEM(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_ELASTIC_DEFORM) &&
           BKE_brush_use_size_pressure(brush)) ||
          (brush->flag & BRUSH_DRAG_DOT));
}

float SCULPT_calc_radius(ViewContext *vc,
                         const Brush *brush,
                         const Scene *scene,
                         const float3 location)
{
  if (!BKE_brush_use_locked_size(scene, brush)) {
    return paint_calc_object_space_radius(vc, location, BKE_brush_size_get(scene, brush));
  }
  else {
    return BKE_brush_unprojected_radius_get(scene, brush);
  }
}

/* -------------------------------------------------------------------- */
/** \name Sculpt PBVH Abstraction API
 *
 * This is read-only, for writing use PBVH vertex iterators. There vd.index matches
 * the indices used here.
 *
 * For multi-resolution, the same vertex in multiple grids is counted multiple times, with
 * different index for each grid.
 * \{ */

void SCULPT_vertex_random_access_ensure(SculptSession *ss)
{
  if (ss->bm) {
    ss->totfaces = ss->totpoly = ss->bm->totface;
    ss->totvert = ss->bm->totvert;

    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);
    BM_mesh_elem_table_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);
  }
}

void SCULPT_face_normal_get(SculptSession *ss, PBVHFaceRef face, float no[3])
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = (BMFace *)face.i;

      copy_v3_v3(no, f->no);
      break;
    }

    case PBVH_FACES:
    case PBVH_GRIDS: {
      no = blender::bke::mesh::poly_normal_calc(
          {reinterpret_cast<float3 *>(ss->vert_positions), ss->totvert},
          ss->corner_verts.slice(ss->polys[face.i]));
      break;
    }
    default: /* Failed. */
      zero_v3(no);
      break;
  }
}

/* Sculpt PBVH abstraction API
 *
 * This is read-only, for writing use PBVH vertex iterators. There vd.index matches
 * the indices used here.
 *
 * For multi-resolution, the same vertex in multiple grids is counted multiple times, with
 * different index for each grid. */

void SCULPT_face_random_access_ensure(SculptSession *ss)
{
  if (ss->bm) {
    ss->totfaces = ss->totpoly = ss->bm->totface;
    ss->totvert = ss->bm->totvert;

    BM_mesh_elem_index_ensure(ss->bm, BM_FACE);
    BM_mesh_elem_table_ensure(ss->bm, BM_FACE);
  }
}

const float *SCULPT_vertex_origco_get(SculptSession *ss, PBVHVertRef vertex)
{
  return vertex_attr_ptr<float>(vertex, ss->attrs.orig_co);
}

void SCULPT_vertex_origno_get(SculptSession *ss, PBVHVertRef vertex, float r_no[3])
{
  copy_v3_v3(r_no, vertex_attr_ptr<float>(vertex, ss->attrs.orig_no));
}

int SCULPT_vertex_count_get(const SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->totvert;
    case PBVH_BMESH:
      return BM_mesh_elem_count(BKE_pbvh_get_bmesh(ss->pbvh), BM_VERT);
    case PBVH_GRIDS:
      return BKE_pbvh_get_grid_num_verts(ss->pbvh);
  }

  return 0;
}

const float *SCULPT_vertex_co_get(SculptSession *ss, PBVHVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (ss->shapekey_active || ss->deform_modifiers_active) {
        const float(*positions)[3] = BKE_pbvh_get_vert_positions(ss->pbvh);
        return positions[vertex.i];
      }
      return ss->vert_positions[vertex.i];
    }
    case PBVH_BMESH:
      return ((BMVert *)vertex.i)->co;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int vertex_index = vertex.i - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return CCG_elem_co(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }
  return nullptr;
}

void SCULPT_vertex_co_set(SculptSession *ss, PBVHVertRef vertex, const float *co)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (ss->shapekey_active || ss->deform_modifiers_active) {
        float(*positions)[3] = BKE_pbvh_get_vert_positions(ss->pbvh);
        copy_v3_v3(positions[vertex.i], co);
      }

      copy_v3_v3(ss->vert_positions[vertex.i], co);
      break;
    }
    case PBVH_BMESH:
      copy_v3_v3(((BMVert *)vertex.i)->co, co);
      break;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int vertex_index = vertex.i - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      float *vertex_co = CCG_elem_co(key, CCG_elem_offset(key, elem, vertex_index));

      copy_v3_v3(vertex_co, co);
      break;
    }
  }
}

bool SCULPT_has_loop_colors(const Object *ob)
{
  using namespace blender;
  Mesh *me = BKE_object_get_original_mesh(ob);
  const std::optional<bke::AttributeMetaData> meta_data = me->attributes().lookup_meta_data(
      me->active_color_attribute);
  if (!meta_data) {
    return false;
  }
  if (meta_data->domain != ATTR_DOMAIN_CORNER) {
    return false;
  }
  if (!(CD_TYPE_AS_MASK(meta_data->data_type) & CD_MASK_COLOR_ALL)) {
    return false;
  }
  return true;
}

bool SCULPT_has_colors(const SculptSession *ss)
{
  return ss->bm ? ss->cd_vcol_offset >= 0 : (ss->vcol || ss->mcol);
}

void SCULPT_vertex_color_get(const SculptSession *ss, PBVHVertRef vertex, float r_color[4])
{
  BKE_pbvh_vertex_color_get(ss->pbvh, vertex, r_color);
}

void SCULPT_vertex_color_set(SculptSession *ss, PBVHVertRef vertex, const float color[4])
{
  BKE_pbvh_vertex_color_set(ss->pbvh, vertex, color);
}

void SCULPT_vertex_normal_get(SculptSession *ss, PBVHVertRef vertex, float no[3])
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      const float(*vert_normals)[3] = BKE_pbvh_get_vert_normals(ss->pbvh);
      copy_v3_v3(no, vert_normals[vertex.i]);
      break;
    }
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      copy_v3_v3(no, v->no);
      break;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int vertex_index = vertex.i - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      copy_v3_v3(no, CCG_elem_no(key, CCG_elem_offset(key, elem, vertex_index)));
      break;
    }
  }
}

bool SCULPT_has_persistent_base(SculptSession *ss)
{
  return BKE_sculpt_has_persistent_base(ss);
}

const float *SCULPT_vertex_persistent_co_get(SculptSession *ss, PBVHVertRef vertex)
{
  if (ss->attrs.persistent_co) {
    return vertex_attr_ptr<const float>(vertex, ss->attrs.persistent_co);
  }

  return SCULPT_vertex_co_get(ss, vertex);
}

const float *SCULPT_vertex_co_for_grab_active_get(SculptSession *ss, PBVHVertRef vertex)
{
  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    /* Always grab active shape key if the sculpt happens on shapekey. */
    if (ss->shapekey_active) {
      const float(*positions)[3] = BKE_pbvh_get_vert_positions(ss->pbvh);
      return positions[vertex.i];
    }

    /* Sculpting on the base mesh. */
    return ss->vert_positions[vertex.i];
  }

  /* Everything else, such as sculpting on multires. */
  return SCULPT_vertex_co_get(ss, vertex);
}

void SCULPT_vertex_limit_surface_get(SculptSession *ss, PBVHVertRef vertex, float r_co[3])
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS) {
    if (ss->attrs.limit_surface) {
      float *f = vertex_attr_ptr<float>(vertex, ss->attrs.limit_surface);
      copy_v3_v3(r_co, f);
    }
    else {
      copy_v3_v3(r_co, SCULPT_vertex_co_get(ss, vertex));
    }

    return;
  }

  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
  const int grid_index = vertex.i / key->grid_area;
  const int vertex_index = vertex.i - grid_index * key->grid_area;

  SubdivCCGCoord coord{};

  coord.grid_index = grid_index;
  coord.x = vertex_index % key->grid_size;
  coord.y = vertex_index / key->grid_size;

  BKE_subdiv_ccg_eval_limit_point(ss->subdiv_ccg, &coord, r_co);
}

void SCULPT_vertex_persistent_normal_get(SculptSession *ss, PBVHVertRef vertex, float no[3])
{
  if (ss->attrs.persistent_no) {
    copy_v3_v3(no, vertex_attr_ptr<float>(vertex, ss->attrs.persistent_no));
    return;
  }
  SCULPT_vertex_normal_get(ss, vertex, no);
}

float SCULPT_vertex_mask_get(SculptSession *ss, PBVHVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->vmask ? ss->vmask[vertex.i] : 0.0f;
    case PBVH_BMESH: {
      BMVert *v;
      int cd_mask = CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK);

      v = (BMVert *)vertex.i;
      return cd_mask != -1 ? BM_ELEM_CD_GET_FLOAT(v, cd_mask) : 0.0f;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);

      if (key->mask_offset == -1) {
        return 0.0f;
      }

      const int grid_index = vertex.i / key->grid_area;
      const int vertex_index = vertex.i - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return *CCG_elem_mask(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }

  return 0.0f;
}

PBVHVertRef SCULPT_active_vertex_get(SculptSession *ss)
{
  if (ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_BMESH, PBVH_GRIDS)) {
    return ss->active_vertex;
  }

  return BKE_pbvh_make_vref(PBVH_REF_NONE);
}

const float *SCULPT_active_vertex_co_get(SculptSession *ss)
{
  return SCULPT_vertex_co_get(ss, SCULPT_active_vertex_get(ss));
}

void SCULPT_active_vertex_normal_get(SculptSession *ss, float normal[3])
{
  SCULPT_vertex_normal_get(ss, SCULPT_active_vertex_get(ss), normal);
}

float (*SCULPT_mesh_deformed_positions_get(SculptSession *ss))[3]
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      if (ss->shapekey_active || ss->deform_modifiers_active) {
        return BKE_pbvh_get_vert_positions(ss->pbvh);
      }
      return ss->vert_positions;
    case PBVH_BMESH:
    case PBVH_GRIDS:
      return nullptr;
  }
  return nullptr;
}

float *SCULPT_brush_deform_target_vertex_co_get(SculptSession *ss,
                                                const int deform_target,
                                                PBVHVertexIter *iter)
{
  switch (deform_target) {
    case BRUSH_DEFORM_TARGET_GEOMETRY:
      return iter->co;
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      return ss->cache->cloth_sim->deformation_pos[iter->index];
  }
  return iter->co;
}

ePaintSymmetryFlags SCULPT_mesh_symmetry_xyz_get(Object *object)
{
  const Mesh *mesh = BKE_mesh_from_object(object);
  return ePaintSymmetryFlags(mesh->symmetry);
}

/* Sculpt Face Sets and Visibility. */

int SCULPT_active_face_set_get(SculptSession *ss)
{
  if (ss->active_face.i == PBVH_REF_NONE) {
    return SCULPT_FACE_SET_NONE;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      if (!ss->face_sets) {
        return SCULPT_FACE_SET_NONE;
      }
      return ss->face_sets[ss->active_face.i];
    case PBVH_GRIDS: {
      if (!ss->face_sets) {
        return SCULPT_FACE_SET_NONE;
      }
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg,
                                                               ss->active_grid_index);
      return ss->face_sets[face_index];
    }
    case PBVH_BMESH:
      if (ss->cd_faceset_offset != -1 && (ss->active_face.i != PBVH_REF_NONE)) {
        BMFace *f = (BMFace *)ss->active_face.i;
        return BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
      }

      return SCULPT_FACE_SET_NONE;
  }
  return SCULPT_FACE_SET_NONE;
}

void SCULPT_vertex_visible_set(SculptSession *ss, PBVHVertRef vertex, bool visible)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      bool *hide_vert = BKE_pbvh_get_vert_hide_for_write(ss->pbvh);
      hide_vert[vertex.i] = visible;
      break;
    }
    case PBVH_BMESH:
      BM_elem_flag_set((BMVert *)vertex.i, BM_ELEM_HIDDEN, !visible);
      break;
    case PBVH_GRIDS:
      break;
  }
}

bool SCULPT_vertex_visible_get(SculptSession *ss, PBVHVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      const bool *hide_vert = BKE_pbvh_get_vert_hide(ss->pbvh);
      return hide_vert == nullptr || !hide_vert[vertex.i];
    }
    case PBVH_BMESH:
      return !BM_elem_flag_test(((BMVert *)vertex.i), BM_ELEM_HIDDEN);
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int vertex_index = vertex.i - grid_index * key->grid_area;

      BLI_bitmap **grid_hidden = BKE_pbvh_get_grid_visibility(ss->pbvh);
      if (grid_hidden && grid_hidden[grid_index]) {
        return !BLI_BITMAP_TEST(grid_hidden[grid_index], vertex_index);
      }
    }
  }
  return true;
}

void SCULPT_face_set_visibility_set(SculptSession *ss, int face_set, bool visible)
{
  BLI_assert(ss->face_sets != nullptr);
  BLI_assert(ss->hide_poly != nullptr);
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      for (int i = 0; i < ss->totfaces; i++) {
        if (ss->face_sets[i] != face_set) {
          continue;
        }
        ss->hide_poly[i] = !visible;
      }
      break;
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
        int node = BM_ELEM_CD_GET_INT(f, ss->cd_face_node_offset);

        if (fset != face_set) {
          continue;
        }

        BM_elem_flag_set(f, BM_ELEM_HIDDEN, !visible);

        if (node != DYNTOPO_NODE_NONE) {
          BKE_pbvh_vert_tag_update_normal_triangulation(BKE_pbvh_node_from_index(ss->pbvh, node));
        }
      }
      break;
    }
  }
}

void SCULPT_face_visibility_all_invert(SculptSession *ss)
{
  SCULPT_topology_islands_invalidate(ss);

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      BLI_assert(ss->hide_poly != nullptr);

      for (int i = 0; i < ss->totfaces; i++) {
        ss->hide_poly[i] = !ss->hide_poly[i];
      }
      break;
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        bool state = BM_elem_flag_test(f, BM_ELEM_HIDDEN);
        BM_elem_flag_set(f, BM_ELEM_HIDDEN, state ^ true);
      }
      break;
    }
  }
}

void SCULPT_face_visibility_all_set(Object *ob, bool visible)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->bm && visible && !ss->attrs.hide_poly) {
    /* Nothing is hidden. */
    return;
  }

  SCULPT_topology_islands_invalidate(ss);
  SCULPT_face_random_access_ensure(ss);

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      /* Note: no need to use the generic loop in PBVH_BMESH, just memset ss->hide_poly. */

      BLI_assert(ss->hide_poly != nullptr);

      if (visible) {
        if (ss->attrs.hide_poly) {
          BKE_sculpt_attribute_destroy(ob, ss->attrs.hide_poly);
        }
        ss->hide_poly = nullptr;
      }
      else {
        if (!ss->hide_poly) {
          ss->hide_poly = BKE_sculpt_hide_poly_ensure(ob);
        }
        memset(ss->hide_poly, !visible, sizeof(bool) * ss->totfaces);
      }
      break;
    case PBVH_BMESH:
      for (int i = 0; i < ss->totfaces; i++) {
        PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);
        BMFace *f = reinterpret_cast<BMFace *>(face.i);
        BM_elem_flag_set(f, BM_ELEM_HIDDEN, !visible);
      }
      break;
  }
}

bool SCULPT_vertex_any_face_visible_get(SculptSession *ss, PBVHVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (!ss->hide_poly) {
        return true;
      }
      for (const int poly : ss->pmap[vertex.i]) {
        if (!ss->hide_poly[poly]) {
          return true;
        }
      }
      return false;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)vertex.i;

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        if (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
          return true;
        }
      }

      return false;
    }
    case PBVH_GRIDS:
      return true;
  }
  return true;
}

bool SCULPT_vertex_all_faces_visible_get(const SculptSession *ss, PBVHVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (!ss->hide_poly) {
        return true;
      }
      for (const int poly : ss->pmap[vertex.i]) {
        if (ss->hide_poly[poly]) {
          return false;
        }
      }
      return true;
    }
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      BMEdge *e = v->e;

      if (!e) {
        return true;
      }

      do {
        BMLoop *l = e->l;

        if (!l) {
          continue;
        }

        do {
          if (BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
            return false;
          }
        } while ((l = l->radial_next) != e->l);
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

      return true;
    }
    case PBVH_GRIDS: {
      if (!ss->hide_poly) {
        return true;
      }
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      return !ss->hide_poly[face_index];
    }
  }
  return true;
}

void SCULPT_vertex_face_set_set(SculptSession *ss, PBVHVertRef vertex, int face_set)
{
  BKE_sculpt_boundary_flag_update(ss, vertex);

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      BLI_assert(ss->face_sets != nullptr);
      for (const int poly_index : ss->pmap[vertex.i]) {
        if (ss->hide_poly && ss->hide_poly[poly_index]) {
          /* Skip hidden faces connected to the vertex. */
          continue;
        }

        for (int vert_i : ss->corner_verts.slice(ss->polys[poly_index])) {
          BKE_sculpt_boundary_flag_update(ss, BKE_pbvh_make_vref(vert_i));
        }

        ss->face_sets[poly_index] = face_set;
      }

      break;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)vertex.i;

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(l->f, ss->cd_faceset_offset);
        if (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN) && fset != face_set) {
          BM_ELEM_CD_SET_INT(l->f, ss->cd_faceset_offset, abs(face_set));
        }

        PBVHVertRef vertex2 = {(intptr_t)l->v};
        BKE_sculpt_boundary_flag_update(ss, vertex2);
      }

      break;
    }
    case PBVH_GRIDS: {
      BLI_assert(ss->face_sets != nullptr);
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      if (ss->hide_poly && ss->hide_poly[face_index]) {
        /* Skip the vertex if it's in a hidden face. */
        return;
      }
      ss->face_sets[face_index] = face_set;
      break;
    }
  }
}

void SCULPT_vertex_face_set_increase(SculptSession *ss, PBVHVertRef vertex, const int increase)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      for (int poly : ss->pmap[vertex.i]) {
        if (ss->face_sets[poly] > 0) {
          ss->face_sets[poly] += increase;
        }
      }
    } break;
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      BMIter iter;
      BMFace *f;

      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

        if (fset <= 0) {
          continue;
        }

        fset += increase;

        BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      }
      break;
    }
    case PBVH_GRIDS: {
      int index = (int)vertex.i;
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      if (ss->face_sets[face_index] > 0) {
        ss->face_sets[face_index] += increase;
      }

    } break;
  }
}

int SCULPT_vertex_face_set_get(SculptSession *ss, PBVHVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (!ss->face_sets) {
        return SCULPT_FACE_SET_NONE;
      }
      int face_set = 0;
      for (const int poly_index : ss->pmap[vertex.i]) {
        if (ss->face_sets[poly_index] > face_set) {
          face_set = abs(ss->face_sets[poly_index]);
        }
      }
      return face_set;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)vertex.i;
      int ret = -1;

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(l->f, ss->cd_faceset_offset);
        fset = abs(fset);

        if (fset > ret) {
          ret = fset;
        }
      }

      return ret;
    }
    case PBVH_GRIDS: {
      if (!ss->face_sets) {
        return SCULPT_FACE_SET_NONE;
      }
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      return ss->face_sets[face_index];
    }
  }
  return 0;
}

bool SCULPT_vertex_has_face_set(SculptSession *ss, PBVHVertRef vertex, int face_set)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (!ss->face_sets) {
        return face_set == SCULPT_FACE_SET_NONE;
      }
      for (const int poly_index : ss->pmap[vertex.i]) {
        if (ss->face_sets[poly_index] == face_set) {
          return true;
        }
      }
      return false;
    }
    case PBVH_BMESH: {
      BMEdge *e;
      BMVert *v = (BMVert *)vertex.i;

      if (ss->cd_faceset_offset == -1) {
        return false;
      }

      e = v->e;

      if (UNLIKELY(!e)) {
        return false;
      }

      do {
        BMLoop *l = e->l;

        if (UNLIKELY(!l)) {
          continue;
        }

        do {
          BMFace *f = l->f;

          if (abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset)) == abs(face_set)) {
            return true;
          }
        } while ((l = l->radial_next) != e->l);
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

      return false;
    }
    case PBVH_GRIDS: {
      if (!ss->face_sets) {
        return face_set == SCULPT_FACE_SET_NONE;
      }
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      return ss->face_sets[face_index] == face_set;
    }
  }
  return true;
}

void SCULPT_visibility_sync_all_from_faces(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  SCULPT_topology_islands_invalidate(ss);

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      /* We may have adjusted the ".hide_poly" attribute, now make the hide status attributes for
       * vertices and edges consistent. */
      BKE_mesh_flush_hidden_from_polys(mesh);
      BKE_pbvh_update_hide_attributes_from_mesh(ss->pbvh);
      break;
    }
    case PBVH_GRIDS: {
      /* In addition to making the hide status of the base mesh consistent, we also have to
       * propagate the status to the Multires grids. */
      BKE_mesh_flush_hidden_from_polys(mesh);
      BKE_sculpt_sync_face_visibility_to_grids(mesh, ss->subdiv_ccg);
      break;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;

      /* Hide all verts and edges attached to faces. */
      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        BMLoop *l = f->l_first;
        do {
          BM_elem_flag_enable(l->v, BM_ELEM_HIDDEN);
          BM_elem_flag_enable(l->e, BM_ELEM_HIDDEN);
        } while ((l = l->next) != f->l_first);
      }

      /* Unhide verts and edges attached to visible faces. */
      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
          continue;
        }

        BM_elem_flag_disable(f, BM_ELEM_HIDDEN);

        BMLoop *l = f->l_first;
        do {
          BM_elem_flag_disable(l->v, BM_ELEM_HIDDEN);
          BM_elem_flag_disable(l->e, BM_ELEM_HIDDEN);
        } while ((l = l->next) != f->l_first);
      }
      break;
    }
  }
}

bool SCULPT_vertex_has_unique_face_set(const SculptSession *ss, PBVHVertRef vertex)
{
  return !SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_FACE_SET);
}

int SCULPT_face_set_next_available_get(SculptSession *ss)
{
  if (ss->cd_faceset_offset == -1) {
    return 0;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS: {
      if (!ss->face_sets) {
        return 0;
      }
      int next_face_set = 0;
      for (int i = 0; i < ss->totfaces; i++) {
        if (ss->face_sets[i] > next_face_set) {
          next_face_set = ss->face_sets[i];
        }
      }
      next_face_set++;
      return next_face_set;
    }
    case PBVH_BMESH: {
      int next_face_set = 0;
      BMIter iter;
      BMFace *f;
      if (ss->cd_faceset_offset == -1) {
        return 0;
      }

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
        if (fset > next_face_set) {
          next_face_set = fset;
        }
      }

      next_face_set++;
      return next_face_set;
    }
  }
  return 0;
}

/* Sculpt Neighbor Iterators */

static void sculpt_vertex_neighbor_add(SculptVertexNeighborIter *iter,
                                       PBVHVertRef neighbor,
                                       PBVHEdgeRef edge,
                                       int neighbor_index)
{
  for (int i = 0; i < iter->size; i++) {
    if (iter->neighbors[i].vertex.i == neighbor.i) {
      return;
    }
  }

  if (iter->size >= iter->capacity) {
    iter->capacity += SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

    if (iter->neighbors == iter->neighbors_fixed) {
      iter->neighbors = (struct _SculptNeighborRef *)MEM_mallocN(
          iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = (int *)MEM_mallocN(iter->capacity * sizeof(*iter->neighbor_indices),
                                                  "neighbor array");

      memcpy((void *)iter->neighbors,
             (void *)iter->neighbors_fixed,
             sizeof(*iter->neighbors) * iter->size);
      memcpy((void *)iter->neighbor_indices,
             (void *)iter->neighbor_indices_fixed,
             sizeof(*iter->neighbor_indices) * iter->size);
    }
    else {
      iter->neighbors = (struct _SculptNeighborRef *)MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = (int *)MEM_reallocN_id(iter->neighbor_indices,
                                                      iter->capacity *
                                                          sizeof(*iter->neighbor_indices),
                                                      "neighbor array");
    }
  }

  iter->neighbors[iter->size].vertex = neighbor;
  iter->neighbors[iter->size].edge = edge;
  iter->neighbor_indices[iter->size] = neighbor_index;
  iter->size++;
}

static void sculpt_vertex_neighbor_add_nocheck(SculptVertexNeighborIter *iter,
                                               PBVHVertRef neighbor,
                                               PBVHEdgeRef edge,
                                               int neighbor_index)
{
  if (iter->size >= iter->capacity) {
    iter->capacity += SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

    if (iter->neighbors == iter->neighbors_fixed) {
      iter->neighbors = (struct _SculptNeighborRef *)MEM_mallocN(
          iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = (int *)MEM_mallocN(iter->capacity * sizeof(*iter->neighbor_indices),
                                                  "neighbor array");

      memcpy(iter->neighbors, iter->neighbors_fixed, sizeof(*iter->neighbors) * iter->size);

      memcpy(iter->neighbor_indices,
             iter->neighbor_indices_fixed,
             sizeof(*iter->neighbor_indices) * iter->size);
    }
    else {
      iter->neighbors = (struct _SculptNeighborRef *)MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = (int *)MEM_reallocN_id(iter->neighbor_indices,
                                                      iter->capacity *
                                                          sizeof(*iter->neighbor_indices),
                                                      "neighbor array");
    }
  }

  iter->neighbors[iter->size].vertex = neighbor;
  iter->neighbors[iter->size].edge = edge;

  iter->neighbor_indices[iter->size] = neighbor_index;
  iter->size++;
}

static void sculpt_vertex_neighbors_get_bmesh(const SculptSession *ss,
                                              PBVHVertRef index,
                                              SculptVertexNeighborIter *iter)
{
  BMVert *v = (BMVert *)index.i;

  iter->is_duplicate = false;

  iter->size = 0;
  iter->num_duplicates = 0;
  iter->has_edge = true;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;
  iter->i = 0;
  iter->no_free = false;

  // cache profiling revealed a hotspot here, don't use BM_ITER
  BMEdge *e = v->e;

  if (!v->e) {
    return;
  }

  BMEdge *e2 = nullptr;

  do {
    BMVert *v2;
    e2 = BM_DISK_EDGE_NEXT(e, v);
    v2 = v == e->v1 ? e->v2 : e->v1;

    uint8_t flag = *BM_ELEM_CD_PTR<uint8_t *>(v2, ss->attrs.flags->bmesh_cd_offset);

    if (!(flag & SCULPTFLAG_VERT_FSET_HIDDEN)) {
      sculpt_vertex_neighbor_add_nocheck(iter,
                                         BKE_pbvh_make_vref((intptr_t)v2),
                                         BKE_pbvh_make_eref((intptr_t)e),
                                         BM_elem_index_get(v2));
    }
  } while ((e = e2) != v->e);

  if (ss->fake_neighbors.use_fake_neighbors) {
    int index = BM_elem_index_get(v);

    BLI_assert(ss->fake_neighbors.fake_neighbor_index != nullptr);
    if (ss->fake_neighbors.fake_neighbor_index[index].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[index],
                                 BKE_pbvh_make_eref(PBVH_REF_NONE),
                                 ss->fake_neighbors.fake_neighbor_index[index].i);
    }
  }
}

static void sculpt_vertex_neighbors_get_faces(const SculptSession *ss,
                                              PBVHVertRef vertex,
                                              SculptVertexNeighborIter *iter)
{
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;
  iter->is_duplicate = false;
  iter->has_edge = true;
  iter->no_free = false;

  int *edges = (int *)BLI_array_alloca(edges, SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY);
  int *unused_polys = (int *)BLI_array_alloca(unused_polys,
                                              SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY * 2);
  bool heap_alloc = false;
  int len = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

  BKE_pbvh_pmap_to_edges(ss->pbvh, vertex, &edges, &len, &heap_alloc, &unused_polys);

  /* Length of array is now in len. */
  for (int i = 0; i < len; i++) {
    const int2 &e = ss->edges[edges[i]];
    int v2 = e[0] == vertex.i ? e[1] : e[0];

    sculpt_vertex_neighbor_add(iter, BKE_pbvh_make_vref(v2), BKE_pbvh_make_eref(edges[i]), v2);
  }

  if (heap_alloc) {
    MEM_freeN(unused_polys);
    MEM_freeN(edges);
  }

  if (ss->fake_neighbors.use_fake_neighbors) {
    BLI_assert(ss->fake_neighbors.fake_neighbor_index != nullptr);
    if (ss->fake_neighbors.fake_neighbor_index[vertex.i].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[vertex.i],
                                 BKE_pbvh_make_eref(PBVH_REF_NONE),
                                 vertex.i);
    }
  }
}

static void sculpt_vertex_neighbors_get_faces_vemap(const SculptSession *ss,
                                                    PBVHVertRef vertex,
                                                    SculptVertexNeighborIter *iter)
{
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;
  iter->is_duplicate = false;
  iter->no_free = false;

  for (int edge : ss->vemap[vertex.i]) {
    const int2 &e = ss->edges[edge];

    unsigned int v = e[0] == (unsigned int)vertex.i ? e[1] : e[0];
    int8_t flag = vertex_attr_get<uint8_t>(vertex, ss->attrs.flags);

    if (flag & SCULPTFLAG_VERT_FSET_HIDDEN) {
      /* Skip connectivity from hidden faces. */
      continue;
    }

    sculpt_vertex_neighbor_add_nocheck(iter, BKE_pbvh_make_vref(v), BKE_pbvh_make_eref(edge), v);
  }

  if (ss->fake_neighbors.use_fake_neighbors) {
    BLI_assert(ss->fake_neighbors.fake_neighbor_index != nullptr);
    if (ss->fake_neighbors.fake_neighbor_index[vertex.i].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[vertex.i],
                                 BKE_pbvh_make_eref(PBVH_REF_NONE),
                                 ss->fake_neighbors.fake_neighbor_index[vertex.i].i);
    }
  }
}

static void sculpt_vertex_neighbors_get_grids(const SculptSession *ss,
                                              const PBVHVertRef vertex,
                                              const bool include_duplicates,
                                              SculptVertexNeighborIter *iter)
{
  int index = (int)vertex.i;

  /* TODO: optimize this. We could fill #SculptVertexNeighborIter directly,
   * maybe provide coordinate and mask pointers directly rather than converting
   * back and forth between #CCGElem and global index. */
  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
  const int grid_index = index / key->grid_area;
  const int vertex_index = index - grid_index * key->grid_area;

  SubdivCCGCoord coord{};
  coord.grid_index = grid_index;
  coord.x = vertex_index % key->grid_size;
  coord.y = vertex_index / key->grid_size;

  SubdivCCGNeighbors neighbors;
  BKE_subdiv_ccg_neighbor_coords_get(ss->subdiv_ccg, &coord, include_duplicates, &neighbors);

  iter->is_duplicate = include_duplicates;

  iter->size = 0;
  iter->num_duplicates = neighbors.num_duplicates;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;
  iter->no_free = false;

  for (int i = 0; i < neighbors.size; i++) {
    int idx = neighbors.coords[i].grid_index * key->grid_area +
              neighbors.coords[i].y * key->grid_size + neighbors.coords[i].x;

    sculpt_vertex_neighbor_add(
        iter, BKE_pbvh_make_vref(idx), BKE_pbvh_make_eref(PBVH_REF_NONE), idx);
  }

  if (ss->fake_neighbors.use_fake_neighbors) {
    BLI_assert(ss->fake_neighbors.fake_neighbor_index != nullptr);
    if (ss->fake_neighbors.fake_neighbor_index[index].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[index],
                                 BKE_pbvh_make_eref(PBVH_REF_NONE),
                                 ss->fake_neighbors.fake_neighbor_index[index].i);
    }
  }

  if (neighbors.coords != neighbors.coords_fixed) {
    MEM_freeN(neighbors.coords);
  }
}

void SCULPT_vertex_neighbors_get(const SculptSession *ss,
                                 const PBVHVertRef vertex,
                                 const bool include_duplicates,
                                 SculptVertexNeighborIter *iter)
{
  iter->no_free = false;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      /* use vemap if it exists, so result is in disk cycle order */
      if (!ss->vemap.is_empty()) {
        blender::bke::pbvh::set_vemap(ss->pbvh, ss->vemap);
        sculpt_vertex_neighbors_get_faces_vemap(ss, vertex, iter);
      }
      else {
        sculpt_vertex_neighbors_get_faces(ss, vertex, iter);
      }
      return;
    case PBVH_BMESH:
      sculpt_vertex_neighbors_get_bmesh(ss, vertex, iter);
      return;
    case PBVH_GRIDS:
      sculpt_vertex_neighbors_get_grids(ss, vertex, include_duplicates, iter);
      return;
  }
}

static bool sculpt_check_boundary_vertex_in_base_mesh(const SculptSession *ss, const int index)
{
  BLI_assert(ss->vertex_info.boundary);
  return BLI_BITMAP_TEST(ss->vertex_info.boundary, index);
}

/* Utilities */

bool SCULPT_stroke_is_main_symmetry_pass(StrokeCache *cache)
{
  return cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0 &&
         cache->tile_pass == 0;
}

bool SCULPT_stroke_is_first_brush_step(StrokeCache *cache)
{
  return cache->first_time && cache->mirror_symmetry_pass == 0 &&
         cache->radial_symmetry_pass == 0 && cache->tile_pass == 0;
}

bool SCULPT_stroke_is_first_brush_step_of_symmetry_pass(StrokeCache *cache)
{
  return cache->first_time;
}

bool SCULPT_check_vertex_pivot_symmetry(const float vco[3], const float pco[3], const char symm)
{
  bool is_in_symmetry_area = true;
  for (int i = 0; i < 3; i++) {
    char symm_it = 1 << i;
    if (symm & symm_it) {
      if (pco[i] == 0.0f) {
        if (vco[i] > 0.0f) {
          is_in_symmetry_area = false;
        }
      }
      if (vco[i] * pco[i] < 0.0f) {
        is_in_symmetry_area = false;
      }
    }
  }
  return is_in_symmetry_area;
}

struct NearestVertexTLSData {
  PBVHVertRef nearest_vertex;
  float nearest_vertex_distance_squared;
};

static void do_nearest_vertex_get_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;
  NearestVertexTLSData *nvtd = static_cast<NearestVertexTLSData *>(tls->userdata_chunk);
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    float distance_squared = len_squared_v3v3(vd.co, data->nearest_vertex_search_co);
    if (distance_squared < nvtd->nearest_vertex_distance_squared &&
        distance_squared < data->max_distance_squared)
    {
      nvtd->nearest_vertex = vd.vertex;
      nvtd->nearest_vertex_distance_squared = distance_squared;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void nearest_vertex_get_reduce(const void *__restrict /*userdata*/,
                                      void *__restrict chunk_join,
                                      void *__restrict chunk)
{
  NearestVertexTLSData *join = static_cast<NearestVertexTLSData *>(chunk_join);
  NearestVertexTLSData *nvtd = static_cast<NearestVertexTLSData *>(chunk);
  if (join->nearest_vertex.i == PBVH_REF_NONE) {
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
  else if (nvtd->nearest_vertex_distance_squared < join->nearest_vertex_distance_squared) {
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
}

PBVHVertRef SCULPT_nearest_vertex_get(
    Sculpt *sd, Object *ob, const float co[3], float max_distance, bool use_original)
{
  SculptSession *ss = ob->sculpt;
  Vector<PBVHNode *> nodes;

  SculptSearchSphereData data{};
  data.sd = sd;
  data.radius_squared = max_distance * max_distance;
  data.original = use_original;
  data.center = co;

  nodes = blender::bke::pbvh::search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data);
  if (nodes.is_empty()) {
    return BKE_pbvh_make_vref(PBVH_REF_NONE);
  }

  SculptThreadedTaskData task_data{};
  task_data.sd = sd;
  task_data.ob = ob;
  task_data.nodes = nodes;
  task_data.max_distance_squared = max_distance * max_distance;

  copy_v3_v3(task_data.nearest_vertex_search_co, co);
  NearestVertexTLSData nvtd;
  nvtd.nearest_vertex.i = PBVH_REF_NONE;
  nvtd.nearest_vertex_distance_squared = FLT_MAX;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  settings.func_reduce = nearest_vertex_get_reduce;
  settings.userdata_chunk = &nvtd;
  settings.userdata_chunk_size = sizeof(NearestVertexTLSData);
  BLI_task_parallel_range(0, nodes.size(), &task_data, do_nearest_vertex_get_task_cb, &settings);

  return nvtd.nearest_vertex;
}

bool SCULPT_is_symmetry_iteration_valid(char i, char symm)
{
  return i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || !ELEM(i, 3, 5)));
}

bool SCULPT_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                               const float br_co[3],
                                               float radius,
                                               char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    float location[3];
    flip_v3_v3(location, br_co, ePaintSymmetryFlags(i));
    if (len_squared_v3v3(location, vertex) < radius * radius) {
      return true;
    }
  }
  return false;
}

void SCULPT_tag_update_overlays(bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  ED_region_tag_redraw(region);

  Object *ob = CTX_data_active_object(C);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if (!BKE_sculptsession_use_pbvh_draw(ob, rv3d)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Flood Fill API
 *
 * Iterate over connected vertices, starting from one or more initial vertices.
 * \{ */

void SCULPT_floodfill_init(SculptSession *ss, SculptFloodFill *flood)
{
  int vertex_count = SCULPT_vertex_count_get(ss);
  SCULPT_vertex_random_access_ensure(ss);

  flood->queue = BLI_gsqueue_new(sizeof(intptr_t));
  flood->visited_verts = BLI_BITMAP_NEW(vertex_count, "visited verts");
}

void SCULPT_floodfill_add_initial(SculptFloodFill *flood, PBVHVertRef vertex)
{
  BLI_gsqueue_push(flood->queue, &vertex);
}

void SCULPT_floodfill_add_and_skip_initial(SculptSession *ss,
                                           SculptFloodFill *flood,
                                           PBVHVertRef vertex)
{
  BLI_gsqueue_push(flood->queue, &vertex);
  BLI_BITMAP_ENABLE(flood->visited_verts, BKE_pbvh_vertex_to_index(ss->pbvh, vertex));
}

void SCULPT_floodfill_add_initial_with_symmetry(Sculpt *sd,
                                                Object *ob,
                                                SculptSession *ss,
                                                SculptFloodFill *flood,
                                                PBVHVertRef vertex,
                                                float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    PBVHVertRef v = {PBVH_REF_NONE};

    if (i == 0) {
      v = vertex;
    }
    else if (radius > 0.0f) {
      float radius_squared = (radius == FLT_MAX) ? FLT_MAX : radius * radius;
      float location[3];
      flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), ePaintSymmetryFlags(i));
      v = SCULPT_nearest_vertex_get(sd, ob, location, radius_squared, false);
    }

    if (v.i != PBVH_REF_NONE) {
      SCULPT_floodfill_add_initial(flood, v);
    }
  }
}

void SCULPT_floodfill_add_active(
    Sculpt *sd, Object *ob, SculptSession *ss, SculptFloodFill *flood, float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }

    PBVHVertRef v = {PBVH_REF_NONE};

    if (i == 0) {
      v = SCULPT_active_vertex_get(ss);
    }
    else if (radius > 0.0f) {
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), ePaintSymmetryFlags(i));
      v = SCULPT_nearest_vertex_get(sd, ob, location, radius, false);
    }

    if (v.i != PBVH_REF_NONE) {
      SCULPT_floodfill_add_initial(flood, v);
    }
  }
}

void SCULPT_floodfill_execute(SculptSession *ss,
                              SculptFloodFill *flood,
                              bool (*func)(SculptSession *ss,
                                           PBVHVertRef from_v,
                                           PBVHVertRef to_v,
                                           bool is_duplicate,
                                           void *userdata),
                              void *userdata)
{
  while (!BLI_gsqueue_is_empty(flood->queue)) {
    PBVHVertRef from_v;

    BLI_gsqueue_pop(flood->queue, &from_v);
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN (ss, from_v, ni) {
      const PBVHVertRef to_v = ni.vertex;
      int to_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, to_v);

      if (BLI_BITMAP_TEST(flood->visited_verts, to_v_i)) {
        continue;
      }

      if (!SCULPT_vertex_visible_get(ss, to_v)) {
        continue;
      }

      BLI_BITMAP_ENABLE(flood->visited_verts, BKE_pbvh_vertex_to_index(ss->pbvh, to_v));

      if (func(ss, from_v, to_v, ni.is_duplicate, userdata)) {
        BLI_gsqueue_push(flood->queue, &to_v);
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }
}

void SCULPT_floodfill_free(SculptFloodFill *flood)
{
  MEM_SAFE_FREE(flood->visited_verts);
  BLI_gsqueue_free(flood->queue);
  flood->queue = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tool Capabilities
 *
 * Avoid duplicate checks, internal logic only,
 * share logic with #rna_def_sculpt_capabilities where possible.
 * \{ */

static bool sculpt_tool_needs_original(const char sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_GRAB,
              SCULPT_TOOL_ROTATE,
              SCULPT_TOOL_THUMB,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_DRAW_SHARP,
              SCULPT_TOOL_ELASTIC_DEFORM,
              SCULPT_TOOL_SMOOTH,
              SCULPT_TOOL_BOUNDARY,
              SCULPT_TOOL_POSE,
              SCULPT_TOOL_PAINT);
}

static bool sculpt_tool_is_proxy_used(const char sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_SMOOTH,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_POSE,
              SCULPT_TOOL_DISPLACEMENT_SMEAR,
              SCULPT_TOOL_BOUNDARY,
              SCULPT_TOOL_CLOTH,
              SCULPT_TOOL_PAINT,
              SCULPT_TOOL_SMEAR,
              SCULPT_TOOL_DRAW_FACE_SETS);
}

static bool sculpt_brush_use_topology_rake(const SculptSession *ss, const Brush *brush)
{
  return SCULPT_TOOL_HAS_TOPOLOGY_RAKE(brush->sculpt_tool) &&
         (brush->topology_rake_factor > 0.0f) && (ss->bm != nullptr);
}

/**
 * Test whether the #StrokeCache.sculpt_normal needs update in #do_brush_action
 */
static int sculpt_brush_needs_normal(const SculptSession *ss, Sculpt *sd, const Brush *brush)
{
  const MTex *mask_tex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
  return ((SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool) &&
           (ss->cache->normal_weight > 0.0f)) ||
          SCULPT_automasking_needs_normal(ss, sd, brush) ||
          ELEM(brush->sculpt_tool,
               SCULPT_TOOL_BLOB,
               SCULPT_TOOL_CREASE,
               SCULPT_TOOL_DRAW,
               SCULPT_TOOL_DRAW_SHARP,
               SCULPT_TOOL_CLOTH,
               SCULPT_TOOL_LAYER,
               SCULPT_TOOL_NUDGE,
               SCULPT_TOOL_ROTATE,
               SCULPT_TOOL_ELASTIC_DEFORM,
               SCULPT_TOOL_THUMB) ||

          (mask_tex->brush_map_mode == MTEX_MAP_MODE_AREA)) ||
         sculpt_brush_use_topology_rake(ss, brush) ||
         BKE_brush_has_cube_tip(brush, PAINT_MODE_SCULPT);
}

static bool sculpt_brush_needs_rake_rotation(const Brush *brush)
{
  return SCULPT_TOOL_HAS_RAKE(brush->sculpt_tool) && (brush->rake_factor != 0.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Init/Update
 * \{ */

enum StrokeFlags {
  CLIP_X = 1,
  CLIP_Y = 2,
  CLIP_Z = 4,
};

void SCULPT_orig_vert_data_init(SculptOrigVertData *data,
                                Object *ob,
                                PBVHNode * /*node*/,
                                SculptUndoType type)
{
  SculptSession *ss = ob->sculpt;
  BMesh *bm = ss->bm;

  data->ss = ss;
  data->datatype = type;

  if (bm) {
    data->bm_log = ss->bm_log;
  }
}

/**
 * DEPRECATED use Update a #SculptOrigVertData for a particular vertex from the PBVH iterator.
 */
void SCULPT_orig_vert_data_update(SculptSession *ss,
                                  SculptOrigVertData *orig_data,
                                  PBVHVertRef vertex)
{
  float *co = nullptr, *no = nullptr, *color = nullptr, *mask = nullptr;

  get_original_vertex(ss, vertex, &co, &no, &color, &mask);

  if (orig_data->datatype == SCULPT_UNDO_COORDS) {
    orig_data->co = co;
    orig_data->no = no;
  }
  else if (orig_data->datatype == SCULPT_UNDO_COLOR) {
    orig_data->col = color;
  }
  else if (orig_data->datatype == SCULPT_UNDO_MASK) {
    orig_data->mask = *mask;
  }
}

void SCULPT_orig_face_data_unode_init(SculptOrigFaceData *data, Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  BMesh *bm = ss->bm;

  memset(data, 0, sizeof(*data));
  data->unode = unode;

  if (bm) {
    data->bm_log = ss->bm_log;
  }
  else {
    data->face_sets = unode->face_sets;
  }
}

void SCULPT_orig_face_data_init(SculptOrigFaceData *data,
                                Object *ob,
                                PBVHNode *node,
                                SculptUndoType type)
{
  SculptUndoNode *unode;
  unode = SCULPT_undo_push_node(ob, node, type);
  SCULPT_orig_face_data_unode_init(data, ob, unode);
}

void SCULPT_orig_face_data_update(SculptOrigFaceData *orig_data, PBVHFaceIter *iter)
{
  if (orig_data->unode->type == SCULPT_UNDO_FACE_SETS) {
    orig_data->face_set = orig_data->face_sets ? orig_data->face_sets[iter->i] : false;
  }
}

static void sculpt_rake_data_update(SculptRakeData *srd, const float co[3])
{
  float rake_dist = len_v3v3(srd->follow_co, co);
  if (rake_dist > srd->follow_dist) {
    interp_v3_v3v3(srd->follow_co, srd->follow_co, co, rake_dist - srd->follow_dist);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Dynamic Topology
 * \{ */

bool SCULPT_stroke_is_dynamic_topology(const SculptSession *ss, const Brush *brush)
{
  return ((BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) &&

          (!ss->cache || (!ss->cache->alt_smooth)) &&

          /* Requires mesh restore, which doesn't work with
           * dynamic-topology. */
          !(brush->flag & BRUSH_ANCHORED) && !(brush->flag & BRUSH_DRAG_DOT) &&

          SCULPT_TOOL_HAS_DYNTOPO(brush->sculpt_tool));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Paint Mesh
 * \{ */

static void paint_mesh_restore_co_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;

  SculptUndoType type = (SculptUndoType)0;

  switch (SCULPT_get_tool(ss, data->brush)) {
    case SCULPT_TOOL_MASK:
      type |= SCULPT_UNDO_MASK;
      break;
    case SCULPT_TOOL_PAINT:
    case SCULPT_TOOL_SMEAR:
      type |= SCULPT_UNDO_COLOR;
      break;
    case SCULPT_TOOL_DRAW_FACE_SETS:
      type = ss->cache->alt_smooth ? SCULPT_UNDO_COORDS : SCULPT_UNDO_FACE_SETS;
      break;
    default:
      type |= SCULPT_UNDO_COORDS;
      break;
  }

  SculptUndoNode *unode;

  if (ss->bm) {
    unode = SCULPT_undo_push_node(data->ob, data->nodes[n], type);
  }
  else {
    unode = SCULPT_undo_get_node(data->nodes[n], type);
  }

  if (!unode) {
    return;
  }

  switch (type) {
    case SCULPT_UNDO_MASK:
      BKE_pbvh_node_mark_update_mask(data->nodes[n]);
      break;
    case SCULPT_UNDO_COLOR:
      BKE_pbvh_node_mark_update_color(data->nodes[n]);
      break;
    case SCULPT_UNDO_FACE_SETS:
      BKE_pbvh_node_mark_update_face_sets(data->nodes[n]);
      break;
    case SCULPT_UNDO_COORDS:
      BKE_pbvh_node_mark_update(data->nodes[n]);
      break;
    default:
      break;
  }

  PBVHVertexIter vd;

  bool modified = false;

  if (unode->type == SCULPT_UNDO_FACE_SETS) {
    SculptOrigFaceData orig_face_data;
    SCULPT_orig_face_data_unode_init(&orig_face_data, data->ob, unode);

    PBVHFaceIter fd;
    BKE_pbvh_face_iter_begin (ss->pbvh, data->nodes[n], fd) {
      SCULPT_orig_face_data_update(&orig_face_data, &fd);

      if (fd.face_set) {
        *fd.face_set = orig_face_data.face_set;
      }
    }

    BKE_pbvh_face_iter_end(fd);
    return;
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_vertex_check_origdata(ss, vd.vertex);
    float *origco = vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_co);
    float *origno = vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_no);

    if (type & SCULPT_UNDO_COORDS) {
      if (len_squared_v3v3(vd.co, origco) > FLT_EPSILON) {
        modified = true;
      }

      copy_v3_v3(vd.co, origco);

      if (vd.no) {
        copy_v3_v3(vd.no, origno);
      }
      else {
        copy_v3_v3(vd.fno, origno);
      }
      if (vd.is_mesh) {
        BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
      }
    }

    if ((type & SCULPT_UNDO_MASK) && ss->attrs.orig_mask) {
      float origmask = vertex_attr_get<float>(vd.vertex, ss->attrs.orig_mask);

      if ((*vd.mask - origmask) * (*vd.mask - origmask) > FLT_EPSILON) {
        modified = true;
      }

      *vd.mask = origmask;
    }

    if ((type & SCULPT_UNDO_COLOR) && ss->attrs.orig_color) {
      float *origcolor = vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_color);
      float color[4];

      if (SCULPT_has_colors(ss)) {
        SCULPT_vertex_color_get(ss, vd.vertex, color);

        if (len_squared_v4v4(color, origcolor) > FLT_EPSILON) {
          modified = true;
        }

        SCULPT_vertex_color_set(ss, vd.vertex, origcolor);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (modified && (type & SCULPT_UNDO_COORDS)) {
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void paint_mesh_restore_co(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), &data, paint_mesh_restore_co_task_cb, &settings);
}

/*** BVH Tree ***/

static void sculpt_extend_redraw_rect_previous(Object *ob, rcti *rect)
{
  /* Expand redraw \a rect with redraw \a rect from previous step to
   * prevent partial-redraw issues caused by fast strokes. This is
   * needed here (not in sculpt_flush_update) as it was before
   * because redraw rectangle should be the same in both of
   * optimized PBVH draw function and 3d view redraw, if not -- some
   * mesh parts could disappear from screen (sergey). */
  SculptSession *ss = ob->sculpt;

  if (!ss->cache) {
    return;
  }

  if (BLI_rcti_is_empty(&ss->cache->previous_r)) {
    return;
  }

  BLI_rcti_union(rect, &ss->cache->previous_r);
}

bool SCULPT_get_redraw_rect(ARegion *region, RegionView3D *rv3d, Object *ob, rcti *rect)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  float bb_min[3], bb_max[3];

  if (!pbvh) {
    return false;
  }

  BKE_pbvh_redraw_BB(pbvh, bb_min, bb_max);

  /* Convert 3D bounding box to screen space. */
  if (!paint_convert_bb_to_rect(rect, bb_min, bb_max, region, rv3d, ob)) {
    return false;
  }

  return true;
}

void ED_sculpt_redraw_planes_get(float planes[4][4], ARegion *region, Object *ob)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  /* Copy here, original will be used below. */
  rcti rect = ob->sculpt->cache->current_r;

  sculpt_extend_redraw_rect_previous(ob, &rect);

  paint_calc_redraw_planes(planes, region, ob, &rect);

  /* We will draw this \a rect, so now we can set it as the previous partial \a rect.
   * Note that we don't update with the union of previous/current (\a rect), only with
   * the current. Thus we avoid the rectangle needlessly growing to include
   * all the stroke area. */
  ob->sculpt->cache->previous_r = ob->sculpt->cache->current_r;

  /* Clear redraw flag from nodes. */
  if (pbvh) {
    BKE_pbvh_update_bounds(pbvh, PBVH_UpdateRedraw);
  }
}

/************************ Brush Testing *******************/

SculptBrushTestFn SCULPT_brush_test_init(const SculptSession *ss, SculptBrushTest *test)
{
  RegionView3D *rv3d = ss->cache ? ss->cache->vc->rv3d : ss->rv3d;
  View3D *v3d = ss->cache ? ss->cache->vc->v3d : ss->v3d;

  test->radius_squared = ss->cache ? ss->cache->radius_squared :
                                     ss->cursor_radius * ss->cursor_radius;
  test->radius = sqrtf(test->radius_squared);

  if (ss->cache) {
    copy_v3_v3(test->location, ss->cache->location);
    test->mirror_symmetry_pass = ss->cache->mirror_symmetry_pass;
    test->radial_symmetry_pass = ss->cache->radial_symmetry_pass;
    copy_m4_m4(test->symm_rot_mat_inv, ss->cache->symm_rot_mat_inv);
  }
  else {
    copy_v3_v3(test->location, ss->cursor_location);
    test->mirror_symmetry_pass = ePaintSymmetryFlags(0);
    test->radial_symmetry_pass = 0;
    unit_m4(test->symm_rot_mat_inv);
  }

  /* Just for initialize. */
  test->dist = 0.0f;

  /* Only for 2D projection. */
  zero_v4(test->plane_view);
  zero_v4(test->plane_tool);

  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    test->clip_rv3d = rv3d;
  }
  else {
    test->clip_rv3d = nullptr;
  }

  test->falloff_shape = PAINT_FALLOFF_SHAPE_SPHERE;
  return SCULPT_brush_test_sphere_sq;
}

SculptBrushTestFn SCULPT_brush_test_init_ex(const SculptSession *ss,
                                            SculptBrushTest *test,
                                            char falloff_shape,
                                            float tip_roundness,
                                            float tip_scale_x)
{
  SCULPT_brush_test_init_with_falloff_shape(ss, test, falloff_shape);

  test->tip_roundness = tip_roundness;
  test->tip_scale_x = tip_scale_x;
  test->test_cube_z = true;

  /* XXX this is likely wrong. */
  if (ss->cache) {
    copy_m4_m4(test->cube_matrix, ss->cache->brush_local_mat);
  }
  else {
    test->cube_matrix[0][0] = test->cube_matrix[1][1] = test->cube_matrix[2][2] =
        test->cube_matrix[3][3] = 1.0f;
  }

  mul_v3_fl(test->cube_matrix[0], tip_scale_x);

  return SCULPT_brush_test;
}

bool SCULPT_brush_test(SculptBrushTest *test, const float co[3])
{
  if (test->tip_roundness >= 1.0f) {
    return SCULPT_brush_test_sphere_sq(test, co);
  }

  bool ret = SCULPT_brush_test_cube(test,
                                    co,
                                    test->cube_matrix,
                                    test->tip_roundness,
                                    test->falloff_shape != PAINT_FALLOFF_SHAPE_TUBE);

  test->dist *= test->dist;

  return ret;
}

BLI_INLINE bool sculpt_brush_test_clipping(const SculptBrushTest *test, const float co[3])
{
  RegionView3D *rv3d = test->clip_rv3d;
  if (!rv3d) {
    return false;
  }
  float symm_co[3];
  flip_v3_v3(symm_co, co, test->mirror_symmetry_pass);
  if (test->radial_symmetry_pass) {
    mul_m4_v3(test->symm_rot_mat_inv, symm_co);
  }
  return ED_view3d_clipping_test(rv3d, symm_co, true);
}

bool SCULPT_brush_test_sphere(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq > test->radius_squared) {
    return false;
  }

  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }

  test->dist = sqrtf(distsq);
  return true;
}

bool SCULPT_brush_test_sphere_sq(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq > test->radius_squared) {
    return false;
  }
  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }
  test->dist = distsq;
  return true;
}

bool SCULPT_brush_test_sphere_fast(const SculptBrushTest *test, const float co[3])
{
  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }
  return len_squared_v3v3(co, test->location) <= test->radius_squared;
}

bool SCULPT_brush_test_circle_sq(SculptBrushTest *test, const float co[3])
{
  float co_proj[3];
  closest_to_plane_normalized_v3(co_proj, test->plane_view, co);
  float distsq = len_squared_v3v3(co_proj, test->location);

  if (distsq > test->radius_squared) {
    return false;
  }

  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }

  test->dist = distsq;
  return true;
}

bool SCULPT_brush_test_cube(SculptBrushTest *test,
                            const float co[3],
                            const float local[4][4],
                            const float roundness,
                            bool test_z)
{
  float side = 1.0f;
  float local_co[3];

  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }

  mul_v3_m4v3(local_co, local, co);

  local_co[0] = fabsf(local_co[0]);
  local_co[1] = fabsf(local_co[1]);
  local_co[2] = fabsf(local_co[2]);

  /* Keep the square and circular brush tips the same size. */
  side += (1.0f - side) * roundness;

  const float hardness = 1.0f - roundness;
  const float constant_side = hardness * side;
  const float falloff_side = roundness * side;

  if (!(local_co[0] <= side && local_co[1] <= side && (local_co[2] <= side || !test_z))) {
    /* Outside the square. */
    return false;
  }
  if (min_ff(local_co[0], local_co[1]) > constant_side) {
    /* Corner, distance to the center of the corner circle. */
    float r_point[3];
    copy_v3_fl(r_point, constant_side);
    test->dist = len_v2v2(r_point, local_co) / falloff_side;
    return true;
  }
  if (max_ff(local_co[0], local_co[1]) > constant_side) {
    /* Side, distance to the square XY axis. */
    test->dist = (max_ff(local_co[0], local_co[1]) - constant_side) / falloff_side;
    return true;
  }

  /* Inside the square, constant distance. */
  test->dist = 0.0f;
  return true;
}

SculptBrushTestFn SCULPT_brush_test_init_with_falloff_shape(const SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape)
{
  if (!ss->cache && !ss->filter_cache) {
    falloff_shape = PAINT_FALLOFF_SHAPE_SPHERE;
  }

  test->falloff_shape = falloff_shape;

  SCULPT_brush_test_init(ss, test);
  SculptBrushTestFn sculpt_brush_test_sq_fn;
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    sculpt_brush_test_sq_fn = SCULPT_brush_test_sphere_sq;
  }
  else {
    float view_normal[3];

    if (ss->cache) {
      copy_v3_v3(view_normal, ss->cache->view_normal);
    }
    else {
      copy_v3_v3(view_normal, ss->filter_cache->view_normal);
    }

    /* PAINT_FALLOFF_SHAPE_TUBE */
    plane_from_point_normal_v3(test->plane_view, test->location, view_normal);
    sculpt_brush_test_sq_fn = SCULPT_brush_test_circle_sq;
  }
  return sculpt_brush_test_sq_fn;
}

const float *SCULPT_brush_frontface_normal_from_falloff_shape(const SculptSession *ss,
                                                              char falloff_shape)
{
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    return ss->cache->sculpt_normal_symm;
  }
  /* PAINT_FALLOFF_SHAPE_TUBE */
  return ss->cache->view_normal;
}

static float frontface(const Brush *br,
                       const float sculpt_normal[3],
                       const float no[3],
                       const float fno[3])
{
  if (!(br->flag & BRUSH_FRONTFACE)) {
    return 1.0f;
  }

  float dot;
  if (no) {
    dot = dot_v3v3(no, sculpt_normal);
  }
  else {
    dot = dot_v3v3(fno, sculpt_normal);
  }
  return dot > 0.0f ? dot : 0.0f;
}

#if 0

static bool sculpt_brush_test_cyl(SculptBrushTest *test,
                                  float co[3],
                                  float location[3],
                                  const float area_no[3])
{
  if (sculpt_brush_test_sphere_fast(test, co)) {
    float t1[3], t2[3], t3[3], dist;

    sub_v3_v3v3(t1, location, co);
    sub_v3_v3v3(t2, x2, location);

    cross_v3_v3v3(t3, area_no, t1);

    dist = len_v3(t3) / len_v3(t2);

    test->dist = dist;

    return true;
  }

  return false;
}

#endif

/* ===== Sculpting =====
 */

static float calc_overlap(StrokeCache *cache,
                          const ePaintSymmetryFlags symm,
                          const char axis,
                          const float angle)
{
  float mirror[3];
  float distsq;

  flip_v3_v3(mirror, cache->true_location, symm);

  if (axis != 0) {
    float mat[3][3];
    axis_angle_to_mat3_single(mat, axis, angle);
    mul_m3_v3(mat, mirror);
  }

  distsq = len_squared_v3v3(mirror, cache->true_location);

  if (distsq <= 4.0f * (cache->radius_squared)) {
    return (2.0f * (cache->radius) - sqrtf(distsq)) / (2.0f * (cache->radius));
  }
  return 0.0f;
}

static float calc_radial_symmetry_feather(Sculpt *sd,
                                          StrokeCache *cache,
                                          const ePaintSymmetryFlags symm,
                                          const char axis)
{
  float overlap = 0.0f;

  for (int i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd->radial_symm[axis - 'X'];
    overlap += calc_overlap(cache, symm, axis, angle);
  }

  return overlap;
}

static float calc_symmetry_feather(Sculpt *sd, StrokeCache *cache)
{
  if (!(sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER)) {
    return 1.0f;
  }
  float overlap;
  const int symm = cache->symmetry;

  overlap = 0.0f;
  for (int i = 0; i <= symm; i++) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }

    overlap += calc_overlap(cache, ePaintSymmetryFlags(i), 0, 0);

    overlap += calc_radial_symmetry_feather(sd, cache, ePaintSymmetryFlags(i), 'X');
    overlap += calc_radial_symmetry_feather(sd, cache, ePaintSymmetryFlags(i), 'Y');
    overlap += calc_radial_symmetry_feather(sd, cache, ePaintSymmetryFlags(i), 'Z');
  }
  return 1.0f / overlap;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Normal and Center
 *
 * Calculate geometry surrounding the brush center.
 * (optionally using original coordinates).
 *
 * Functions are:
 * - #SCULPT_calc_area_center
 * - #SCULPT_calc_area_normal
 * - #SCULPT_calc_area_normal_and_center
 *
 * \note These are all _very_ similar, when changing one, check others.
 * \{ */

struct AreaNormalCenterTLSData {
  /* 0 = towards view, 1 = flipped */
  float area_cos[2][3];
  float area_nos[2][3];
  int count_no[2];
  int count_co[2];
};

static void calc_area_normal_and_center_task_cb(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;
  AreaNormalCenterTLSData *anctd = static_cast<AreaNormalCenterTLSData *>(tls->userdata_chunk);
  const bool use_area_nos = data->use_area_nos;
  const bool use_area_cos = data->use_area_cos;

  PBVHVertexIter vd;
  SculptUndoNode *unode = nullptr;

  bool use_original = false;
  bool normal_test_r, area_test_r;

  if (ss->cache && !ss->cache->accum) {
    unode = SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
    use_original = (unode->co || unode->bm_entry);
  }

  SculptBrushTest normal_test;
  SculptBrushTestFn sculpt_brush_normal_test_sq_fn = SCULPT_brush_test_init_ex(
      ss, &normal_test, (eBrushFalloffShape)data->brush->falloff_shape, 1.0f, 1.0f);

  /* Update the test radius to sample the normal using the normal radius of the brush. */
  if (data->brush->ob_mode == OB_MODE_SCULPT) {
    float test_radius = sqrtf(normal_test.radius_squared);
    test_radius *= data->brush->normal_radius_factor;
    normal_test.radius = test_radius;
    normal_test.radius_squared = test_radius * test_radius;
  }

  SculptBrushTest area_test;
  SculptBrushTestFn sculpt_brush_area_test_sq_fn = SCULPT_brush_test_init_ex(
      ss, &area_test, (eBrushFalloffShape)data->brush->falloff_shape, 1.0f, 1.0f);

  if (data->brush->ob_mode == OB_MODE_SCULPT) {
    float test_radius = sqrtf(area_test.radius_squared);
    /* Layer brush produces artifacts with normal and area radius */
    /* Enable area radius control only on Scrape for now */
    if (ELEM(SCULPT_get_tool(ss, data->brush), SCULPT_TOOL_SCRAPE, SCULPT_TOOL_FILL) &&
        data->brush->area_radius_factor > 0.0f)
    {
      test_radius *= data->brush->area_radius_factor;
      if (ss->cache && data->brush->flag2 & BRUSH_AREA_RADIUS_PRESSURE) {
        test_radius *= ss->cache->pressure;
      }
    }
    else {
      test_radius *= data->brush->normal_radius_factor;
    }
    area_test.radius = test_radius;
    area_test.radius_squared = test_radius * test_radius;
  }

  /* When the mesh is edited we can't rely on original coords
   * (original mesh may not even have verts in brush radius). */
  if (use_original && data->has_bm_orco) {
    PBVHTriBuf *tribuf = BKE_pbvh_bmesh_get_tris(ss->pbvh, data->nodes[n]);

    for (int i = 0; i < tribuf->tottri; i++) {
      PBVHTri *tri = tribuf->tris + i;
      PBVHVertRef v1 = tribuf->verts[tri->v[0]];
      PBVHVertRef v2 = tribuf->verts[tri->v[1]];
      PBVHVertRef v3 = tribuf->verts[tri->v[2]];

      const float *co_tri[3] = {
          SCULPT_vertex_origco_get(ss, v1),
          SCULPT_vertex_origco_get(ss, v2),
          SCULPT_vertex_origco_get(ss, v3),
      };
      float co[3];

      closest_on_tri_to_point_v3(co, normal_test.location, UNPACK3(co_tri));

      normal_test_r = sculpt_brush_normal_test_sq_fn(&normal_test, co);
      area_test_r = sculpt_brush_area_test_sq_fn(&area_test, co);

      if (!normal_test_r && !area_test_r) {
        continue;
      }

      float no[3];
      int flip_index;

      normal_tri_v3(no, UNPACK3(co_tri));

      flip_index = (dot_v3v3(ss->cache->view_normal, no) <= 0.0f);
      if (use_area_cos && area_test_r) {
        /* Weight the coordinates towards the center. */
        float p = 1.0f - (sqrtf(area_test.dist) / area_test.radius);
        const float afactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);

        float disp[3];
        sub_v3_v3v3(disp, co, area_test.location);
        mul_v3_fl(disp, 1.0f - afactor);
        add_v3_v3v3(co, area_test.location, disp);
        add_v3_v3(anctd->area_cos[flip_index], co);

        anctd->count_co[flip_index] += 1;
      }
      if (use_area_nos && normal_test_r) {
        /* Weight the normals towards the center. */
        float p = 1.0f - (sqrtf(normal_test.dist) / normal_test.radius);
        const float nfactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);
        mul_v3_fl(no, nfactor);

        add_v3_v3(anctd->area_nos[flip_index], no);
        anctd->count_no[flip_index] += 1;
      }
    }
  }
  else {
    BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
      float co[3];

      /* For bm_vert only. */
      float no_s[3];

      if (use_original) {
        copy_v3_v3(co, vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_co));
        copy_v3_v3(no_s, vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_no));
      }
      else {
        copy_v3_v3(co, vd.co);
      }

      normal_test_r = sculpt_brush_normal_test_sq_fn(&normal_test, co);
      area_test_r = sculpt_brush_area_test_sq_fn(&area_test, co);

      if (!normal_test_r && !area_test_r) {
        continue;
      }

      float no[3];
      int flip_index;

      data->any_vertex_sampled = true;

      if (use_original) {
        copy_v3_v3(no, no_s);
      }
      else {
        if (vd.no) {
          copy_v3_v3(no, vd.no);
        }
        else {
          copy_v3_v3(no, vd.fno);
        }
      }

      flip_index = (dot_v3v3(ss->cache ? ss->cache->view_normal : ss->cursor_view_normal, no) <=
                    0.0f);

      if (use_area_cos && area_test_r) {
        /* Weight the coordinates towards the center. */
        float p = 1.0f - (sqrtf(area_test.dist) / area_test.radius);
        const float afactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);

        float disp[3];
        sub_v3_v3v3(disp, co, area_test.location);
        mul_v3_fl(disp, 1.0f - afactor);
        add_v3_v3v3(co, area_test.location, disp);

        add_v3_v3(anctd->area_cos[flip_index], co);
        anctd->count_co[flip_index] += 1;
      }
      if (use_area_nos && normal_test_r) {
        /* Weight the normals towards the center. */
        float p = 1.0f - (sqrtf(normal_test.dist) / normal_test.radius);
        const float nfactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);
        mul_v3_fl(no, nfactor);

        add_v3_v3(anctd->area_nos[flip_index], no);
        anctd->count_no[flip_index] += 1;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_area_normal_and_center_reduce(const void *__restrict /*userdata*/,
                                               void *__restrict chunk_join,
                                               void *__restrict chunk)
{
  AreaNormalCenterTLSData *join = static_cast<AreaNormalCenterTLSData *>(chunk_join);
  AreaNormalCenterTLSData *anctd = static_cast<AreaNormalCenterTLSData *>(chunk);

  /* For flatten center. */
  add_v3_v3(join->area_cos[0], anctd->area_cos[0]);
  add_v3_v3(join->area_cos[1], anctd->area_cos[1]);

  /* For area normal. */
  add_v3_v3(join->area_nos[0], anctd->area_nos[0]);
  add_v3_v3(join->area_nos[1], anctd->area_nos[1]);

  /* Weights. */
  add_v2_v2_int(join->count_no, anctd->count_no);
  add_v2_v2_int(join->count_co, anctd->count_co);
}

void SCULPT_calc_area_center(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_co[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm;
  int n;

  /* Intentionally set 'sd' to nullptr since we share logic with vertex paint. */
  SculptThreadedTaskData data{};
  data.sd = nullptr;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.has_bm_orco = has_bm_orco;
  data.use_area_cos = true;

  AreaNormalCenterTLSData anctd = {{{0}}};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BLI_task_parallel_range(0, nodes.size(), &data, calc_area_normal_and_center_task_cb, &settings);

  /* For flatten center. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.count_co[n] == 0) {
      continue;
    }

    mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
    break;
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss->cache) {
      copy_v3_v3(r_area_co, ss->cache->location);
    }
  }
}

void SCULPT_calc_area_normal(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_no[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  SCULPT_pbvh_calc_area_normal(brush, ob, nodes, true, r_area_no);
}

bool SCULPT_pbvh_calc_area_normal(
    const Brush *brush, Object *ob, Span<PBVHNode *> nodes, bool use_threading, float r_area_no[3])
{
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm;

  /* Intentionally set 'sd' to nullptr since this is used for vertex paint too. */
  SculptThreadedTaskData data{};
  data.sd = nullptr;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.has_bm_orco = has_bm_orco;
  data.use_area_nos = true;
  data.any_vertex_sampled = false;

  AreaNormalCenterTLSData anctd = {{{0}}};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, use_threading, nodes.size());
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BLI_task_parallel_range(0, nodes.size(), &data, calc_area_normal_and_center_task_cb, &settings);

  /* For area normal. */
  for (int i = 0; i < ARRAY_SIZE(anctd.area_nos); i++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[i]) != 0.0f) {
      break;
    }
  }

  return data.any_vertex_sampled;
}

void SCULPT_calc_area_normal_and_center(
    Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_no[3], float r_area_co[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm;
  int n;

  /* Intentionally set 'sd' to nullptr since this is used for vertex paint too. */
  SculptThreadedTaskData data{};
  data.sd = nullptr;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.has_bm_orco = has_bm_orco;
  data.use_area_cos = true;
  data.use_area_nos = true;

  AreaNormalCenterTLSData anctd = {{{0}}};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BLI_task_parallel_range(0, nodes.size(), &data, calc_area_normal_and_center_task_cb, &settings);

  /* For flatten center. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.count_co[n] == 0) {
      continue;
    }

    mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
    break;
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss->cache) {
      copy_v3_v3(r_area_co, ss->cache->location);
    }
  }

  /* For area normal. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_nos); n++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[n]) != 0.0f) {
      break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Brush Utilities
 * \{ */

/**
 * Return modified brush strength. Includes the direction of the brush, positive
 * values pull vertices, negative values push. Uses tablet pressure and a
 * special multiplier found experimentally to scale the strength factor.
 */
static float brush_strength(const Sculpt *sd,
                            const StrokeCache *cache,
                            const float feather,
                            const UnifiedPaintSettings *ups,
                            const PaintModeSettings * /*paint_mode_settings*/)
{
  const Scene *scene = cache->vc->scene;
  const Brush *brush = BKE_paint_brush((Paint *)&sd->paint);

  /* Primary strength input; square it to make lower values more sensitive. */
  const float root_alpha = BKE_brush_alpha_get(scene, brush);
  const float alpha = root_alpha * root_alpha;
  const float dir = (brush->flag & BRUSH_DIR_IN) ? -1.0f : 1.0f;
  const float pressure = BKE_brush_use_alpha_pressure(brush) ? cache->pressure : 1.0f;
  const float pen_flip = cache->pen_flip ? -1.0f : 1.0f;
  const float invert = cache->invert ? -1.0f : 1.0f;
  float overlap = ups->overlap_factor;
  /* Spacing is integer percentage of radius, divide by 50 to get
   * normalized diameter. */

  float flip = dir * invert * pen_flip;
  if (brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
    flip = 1.0f;
  }

  /* Pressure final value after being tweaked depending on the brush. */
  float final_pressure;

  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_CLAY:
      final_pressure = pow4f(pressure);
      overlap = (1.0f + overlap) / 2.0f;
      return 0.25f * alpha * flip * final_pressure * overlap * feather;
    case SCULPT_TOOL_DRAW:
    case SCULPT_TOOL_DRAW_SHARP:
    case SCULPT_TOOL_LAYER:
      return alpha * flip * pressure * overlap * feather;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_CLOTH:
      if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
        /* Grab deform uses the same falloff as a regular grab brush. */
        return root_alpha * feather;
      }
      else if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK) {
        return root_alpha * feather * pressure * overlap;
      }
      else if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_EXPAND) {
        /* Expand is more sensible to strength as it keeps expanding the cloth when sculpting over
         * the same vertices. */
        return 0.1f * alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Multiply by 10 by default to get a larger range of strength depending on the size of the
         * brush and object. */
        return 10.0f * alpha * flip * pressure * overlap * feather;
      }
    case SCULPT_TOOL_DRAW_FACE_SETS:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_SLIDE_RELAX:
      return alpha * pressure * overlap * feather * 2.0f;
    case SCULPT_TOOL_PAINT:
      final_pressure = pressure * pressure;
      return final_pressure * overlap * feather;
    case SCULPT_TOOL_SMEAR:
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_CLAY_STRIPS:
      /* Clay Strips needs less strength to compensate the curve. */
      final_pressure = powf(pressure, 1.5f);
      return alpha * flip * final_pressure * overlap * feather * 0.3f;
    case SCULPT_TOOL_CLAY_THUMB:
      final_pressure = pressure * pressure;
      return alpha * flip * final_pressure * overlap * feather * 1.3f;

    case SCULPT_TOOL_MASK:
      overlap = (1.0f + overlap) / 2.0f;
      switch ((BrushMaskTool)brush->mask_tool) {
        case BRUSH_MASK_DRAW:
          return alpha * flip * pressure * overlap * feather;
        case BRUSH_MASK_SMOOTH:
          return alpha * pressure * feather;
      }
      BLI_assert_msg(0, "Not supposed to happen");
      return 0.0f;

    case SCULPT_TOOL_CREASE:
    case SCULPT_TOOL_BLOB:
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_TOOL_INFLATE:
      if (flip > 0.0f) {
        return 0.250f * alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.125f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_TOOL_FILL:
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FLATTEN:
      if (flip > 0.0f) {
        overlap = (1.0f + overlap) / 2.0f;
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Reduce strength for DEEPEN, PEAKS, and CONTRAST. */
        return 0.5f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_SMOOTH:
      return flip * alpha * pressure * feather;

    case SCULPT_TOOL_PINCH:
      if (flip > 0.0f) {
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.25f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_NUDGE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * pressure * overlap * feather;

    case SCULPT_TOOL_THUMB:
      return alpha * pressure * feather;

    case SCULPT_TOOL_SNAKE_HOOK:
      return root_alpha * feather;

    case SCULPT_TOOL_GRAB:
      return root_alpha * feather;

    case SCULPT_TOOL_ROTATE:
      return alpha * pressure * feather;

    case SCULPT_TOOL_ELASTIC_DEFORM:
    case SCULPT_TOOL_POSE:
    case SCULPT_TOOL_BOUNDARY:
      return root_alpha * feather;

    default:
      return 0.0f;
  }
}

static float sculpt_apply_hardness(const SculptSession *ss, const float input_len)
{
  const StrokeCache *cache = ss->cache;
  float final_len = input_len;
  const float hardness = cache->paint_brush.hardness;
  float p = input_len / cache->radius;
  if (p < hardness) {
    final_len = 0.0f;
  }
  else if (hardness == 1.0f) {
    final_len = cache->radius;
  }
  else {
    p = (p - hardness) / (1.0f - hardness);
    final_len = p * cache->radius;
  }

  return final_len;
}

static void sculpt_apply_texture(const SculptSession *ss,
                                 const Brush *brush,
                                 const float brush_point[3],
                                 const int thread_id,
                                 float *r_value,
                                 float r_rgba[4])
{
  StrokeCache *cache = ss->cache;
  const Scene *scene = cache->vc->scene;
  const MTex *mtex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);

  if (!mtex->tex) {
    *r_value = 1.0f;
    copy_v4_fl(r_rgba, 1.0f);
    return;
  }

  float point[3];
  sub_v3_v3v3(point, brush_point, cache->plane_offset);

  if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    /* Get strength by feeding the vertex location directly into a texture. */
    *r_value = BKE_brush_sample_tex_3d(scene, brush, mtex, point, r_rgba, 0, ss->tex_pool);
  }
  else {
    float symm_point[3];

    /* If the active area is being applied for symmetry, flip it
     * across the symmetry axis and rotate it back to the original
     * position in order to project it. This insures that the
     * brush texture will be oriented correctly. */
    if (cache->radial_symmetry_pass) {
      mul_m4_v3(cache->symm_rot_mat_inv, point);
    }
    flip_v3_v3(symm_point, point, cache->mirror_symmetry_pass);

    /* Still no symmetry supported for other paint modes.
     * Sculpt does it DIY. */
    if (mtex->brush_map_mode == MTEX_MAP_MODE_AREA) {
      /* Similar to fixed mode, but projects from brush angle
       * rather than view direction. */

      mul_m4_v3(cache->brush_local_mat, symm_point);

      float x = symm_point[0];
      float y = symm_point[1];

      x *= mtex->size[0];
      y *= mtex->size[1];

      x += mtex->ofs[0];
      y += mtex->ofs[1];

      paint_get_tex_pixel(mtex, x, y, ss->tex_pool, thread_id, r_value, r_rgba);

      add_v3_fl(r_rgba, brush->texture_sample_bias);  // v3 -> Ignore alpha
      *r_value -= brush->texture_sample_bias;
    }
    else {
      float point_2d[2];
      ED_view3d_project_float_v2_m4(
          cache->vc->region, symm_point, point_2d, cache->projection_mat);
      const float point_3d[3] = {point_2d[0], point_2d[1], 0.0f};
      *r_value = BKE_brush_sample_tex_3d(scene, brush, mtex, point_3d, r_rgba, 0, ss->tex_pool);
    }
  }
}

float SCULPT_brush_strength_factor(SculptSession *ss,
                                   const Brush *brush,
                                   const float brush_point[3],
                                   float len,
                                   const float vno[3],
                                   const float fno[3],
                                   float mask,
                                   const PBVHVertRef vertex,
                                   int thread_id,
                                   AutomaskingNodeData *automask_data)
{
  StrokeCache *cache = ss->cache;

  float avg = 1.0f;
  float rgba[4];
  sculpt_apply_texture(ss, brush, brush_point, thread_id, &avg, rgba);

  /* Hardness. */
  const float final_len = sculpt_apply_hardness(ss, len);

  /* Falloff curve. */
  avg *= BKE_brush_curve_strength(brush, final_len, cache->radius);
  avg *= frontface(brush, cache->view_normal, vno, fno);

  /* Paint mask. */
  avg *= 1.0f - mask;

  /* Auto-masking. */
  avg *= SCULPT_automasking_factor_get(cache->automasking, ss, vertex, automask_data);

  return avg;
}

void SCULPT_brush_strength_color(SculptSession *ss,
                                 const Brush *brush,
                                 const float brush_point[3],
                                 float len,
                                 const float vno[3],
                                 const float fno[3],
                                 float mask,
                                 const PBVHVertRef vertex,
                                 int thread_id,
                                 AutomaskingNodeData *automask_data,
                                 float r_rgba[4])
{
  StrokeCache *cache = ss->cache;

  float avg = 1.0f;
  sculpt_apply_texture(ss, brush, brush_point, thread_id, &avg, r_rgba);

  /* Hardness. */
  const float final_len = sculpt_apply_hardness(ss, len);

  /* Falloff curve. */
  const float falloff = BKE_brush_curve_strength(brush, final_len, cache->radius) *
                        frontface(brush, cache->view_normal, vno, fno);

  /* Paint mask. */
  const float paint_mask = 1.0f - mask;

  /* Auto-masking. */
  const float automasking_factor = SCULPT_automasking_factor_get(
      cache->automasking, ss, vertex, automask_data);

  const float masks_combined = falloff * paint_mask * automasking_factor;

  mul_v4_fl(r_rgba, masks_combined);
}

void SCULPT_calc_vertex_displacement(SculptSession *ss,
                                     const Brush *brush,
                                     float rgba[3],
                                     float out_offset[3])
{
  mul_v3_fl(rgba, ss->cache->bstrength);
  /* Handle brush inversion */
  if (ss->cache->bstrength < 0) {
    rgba[0] *= -1;
    rgba[1] *= -1;
  }

  /* Apply texture size */
  for (int i = 0; i < 3; ++i) {
    rgba[i] *= blender::math::safe_divide(1.0f, pow2f(brush->mtex.size[i]));
  }

  /* Transform vector to object space */
  mul_mat3_m4_v3(ss->cache->brush_local_mat_inv, rgba);

  /* Handle symmetry */
  if (ss->cache->radial_symmetry_pass) {
    mul_m4_v3(ss->cache->symm_rot_mat, rgba);
  }
  flip_v3_v3(out_offset, rgba, ss->cache->mirror_symmetry_pass);
}

bool SCULPT_search_sphere_cb(PBVHNode *node, void *data_v)
{
  SculptSearchSphereData *data = static_cast<SculptSearchSphereData *>(data_v);
  const float *center;
  float nearest[3];
  if (data->center) {
    center = data->center;
  }
  else {
    center = data->ss->cache ? data->ss->cache->location : data->ss->cursor_location;
  }
  float t[3], bb_min[3], bb_max[3];

  if (data->ignore_fully_ineffective) {
    if (BKE_pbvh_node_fully_hidden_get(node)) {
      return false;
    }
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_max);
  }

  for (int i = 0; i < 3; i++) {
    if (bb_min[i] > center[i]) {
      nearest[i] = bb_min[i];
    }
    else if (bb_max[i] < center[i]) {
      nearest[i] = bb_max[i];
    }
    else {
      nearest[i] = center[i];
    }
  }

  sub_v3_v3v3(t, center, nearest);

  return len_squared_v3(t) < data->radius_squared;
}

bool SCULPT_search_circle_cb(PBVHNode *node, void *data_v)
{
  SculptSearchCircleData *data = static_cast<SculptSearchCircleData *>(data_v);
  float bb_min[3], bb_max[3];

  if (data->ignore_fully_ineffective) {
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_min);
  }

  float dummy_co[3], dummy_depth;
  const float dist_sq = dist_squared_ray_to_aabb_v3(
      data->dist_ray_to_aabb_precalc, bb_min, bb_max, dummy_co, &dummy_depth);

  /* Seems like debug code.
   * Maybe this function can just return true if the node is not fully masked. */
  return dist_sq < data->radius_squared || true;
}

void SCULPT_clip(Sculpt *sd, SculptSession *ss, float co[3], const float val[3])
{
  for (int i = 0; i < 3; i++) {
    if (sd->flags & (SCULPT_LOCK_X << i)) {
      continue;
    }

    bool do_clip = false;
    float co_clip[3];
    if (ss->cache && (ss->cache->flag & (CLIP_X << i))) {
      /* Take possible mirror object into account. */
      mul_v3_m4v3(co_clip, ss->cache->clip_mirror_mtx, co);

      if (fabsf(co_clip[i]) <= ss->cache->clip_tolerance[i]) {
        co_clip[i] = 0.0f;
        float imtx[4][4];
        invert_m4_m4(imtx, ss->cache->clip_mirror_mtx);
        mul_m4_v3(imtx, co_clip);
        do_clip = true;
      }
    }

    if (do_clip) {
      co[i] = co_clip[i];
    }
    else {
      co[i] = val[i];
    }
  }
}

static Vector<PBVHNode *> sculpt_pbvh_gather_cursor_update(Object *ob,
                                                           Sculpt *sd,
                                                           bool use_original)
{
  SculptSession *ss = ob->sculpt;
  SculptSearchSphereData data{};
  data.ss = ss;
  data.sd = sd;
  data.radius_squared = ss->cursor_radius;
  data.original = use_original;
  data.ignore_fully_ineffective = false;
  data.center = nullptr;

  return blender::bke::pbvh::search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data);
}

static Vector<PBVHNode *> sculpt_pbvh_gather_generic_intern(Object *ob,
                                                            Sculpt *sd,
                                                            const Brush *brush,
                                                            bool use_original,
                                                            float radius_scale,
                                                            PBVHNodeFlags flag)
{
  SculptSession *ss = ob->sculpt;
  Vector<PBVHNode *> nodes;
  PBVHNodeFlags leaf_flag = PBVH_Leaf;

  if (flag & PBVH_TexLeaf) {
    leaf_flag = PBVH_TexLeaf;
  }

  /* Build a list of all nodes that are potentially within the cursor or brush's area of influence.
   */
  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    SculptSearchSphereData data{};
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = square_f(ss->cache->radius * radius_scale);
    data.original = use_original;
    data.ignore_fully_ineffective = brush->sculpt_tool != SCULPT_TOOL_MASK;
    data.center = nullptr;
    nodes = blender::bke::pbvh::search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, leaf_flag);
  }
  else {
    DistRayAABB_Precalc dist_ray_to_aabb_precalc;
    dist_squared_ray_to_aabb_v3_precalc(
        &dist_ray_to_aabb_precalc, ss->cache->location, ss->cache->view_normal);
    SculptSearchCircleData data{};
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = ss->cache ? square_f(ss->cache->radius * radius_scale) :
                                      ss->cursor_radius;
    data.original = use_original;
    data.dist_ray_to_aabb_precalc = &dist_ray_to_aabb_precalc;
    data.ignore_fully_ineffective = brush->sculpt_tool != SCULPT_TOOL_MASK;
    nodes = blender::bke::pbvh::search_gather(ss->pbvh, SCULPT_search_circle_cb, &data, leaf_flag);
  }

  return nodes;
}

static Vector<PBVHNode *> sculpt_pbvh_gather_generic(
    Object *ob, Sculpt *sd, const Brush *brush, bool use_original, float radius_scale)
{
  return sculpt_pbvh_gather_generic_intern(ob, sd, brush, use_original, radius_scale, PBVH_Leaf);
}

static Vector<PBVHNode *> sculpt_pbvh_gather_texpaint(
    Object *ob, Sculpt *sd, const Brush *brush, bool use_original, float radius_scale)
{
  return sculpt_pbvh_gather_generic_intern(
      ob, sd, brush, use_original, radius_scale, PBVH_TexLeaf);
}

/* Calculate primary direction of movement for many brushes. */
static void calc_sculpt_normal(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_no[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  const SculptSession *ss = ob->sculpt;

  switch (brush->sculpt_plane) {
    case SCULPT_DISP_DIR_VIEW:
      copy_v3_v3(r_area_no, ss->cache->true_view_normal);
      break;

    case SCULPT_DISP_DIR_X:
      ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
      break;

    case SCULPT_DISP_DIR_Y:
      ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
      break;

    case SCULPT_DISP_DIR_Z:
      ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
      break;

    case SCULPT_DISP_DIR_AREA:
      SCULPT_calc_area_normal(sd, ob, nodes, r_area_no);
      break;

    default:
      break;
  }
}

static void update_sculpt_normal(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  StrokeCache *cache = ob->sculpt->cache;
  const Brush *brush = cache->brush;  // BKE_paint_brush(&sd->paint);
  int tool = SCULPT_get_tool(ob->sculpt, brush);

  /* Grab brush does not update the sculpt normal during a stroke. */
  const bool update_normal = !((brush->flag & BRUSH_ORIGINAL_NORMAL) &&
                               !(tool == SCULPT_TOOL_GRAB) &&
                               !(tool == SCULPT_TOOL_THUMB && !(brush->flag & BRUSH_ANCHORED)) &&
                               !(tool == SCULPT_TOOL_ELASTIC_DEFORM) &&
                               !(tool == SCULPT_TOOL_SNAKE_HOOK && cache->normal_weight > 0.0f)) ||
                             dot_v3v3(cache->sculpt_normal, cache->sculpt_normal) == 0.0f;

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0 &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(cache) || update_normal))
  {
    calc_sculpt_normal(sd, ob, nodes, cache->sculpt_normal);
    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(cache->sculpt_normal, cache->sculpt_normal, cache->view_normal);
      normalize_v3(cache->sculpt_normal);
    }
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
  }
  else {
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
    flip_v3(cache->sculpt_normal_symm, cache->mirror_symmetry_pass);
    mul_m4_v3(cache->symm_rot_mat, cache->sculpt_normal_symm);
  }
}

static void calc_local_y(ViewContext *vc, const float center[3], float y[3])
{
  Object *ob = vc->obact;
  float loc[3];
  const float xy_delta[2] = {0.0f, 1.0f};

  mul_v3_m4v3(loc, ob->world_to_object, center);
  const float zfac = ED_view3d_calc_zfac(vc->rv3d, loc);

  ED_view3d_win_to_delta(vc->region, xy_delta, zfac, y);
  normalize_v3(y);

  add_v3_v3(y, ob->loc);
  mul_m4_v3(ob->world_to_object, y);
}

static void calc_brush_local_mat(const float rotation,
                                 Object *ob,
                                 float local_mat[4][4],
                                 float local_mat_inv[4][4])
{
  const StrokeCache *cache = ob->sculpt->cache;
  float tmat[4][4];
  float mat[4][4];
  float scale[4][4];
  float angle, v[3];
  float up[3];

  /* Ensure `ob->world_to_object` is up to date. */
  invert_m4_m4(ob->world_to_object, ob->object_to_world);

  /* Initialize last column of matrix. */
  mat[0][3] = 0.0f;
  mat[1][3] = 0.0f;
  mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;

  /* Get view's up vector in object-space. */
  calc_local_y(cache->vc, cache->location, up);

  /* Calculate the X axis of the local matrix. */
  cross_v3_v3v3(v, up, cache->sculpt_normal);
  /* Apply rotation (user angle, rake, etc.) to X axis. */
  angle = rotation - cache->special_rotation;
  rotate_v3_v3v3fl(mat[0], v, cache->sculpt_normal, angle);

  /* Get other axes. */
  cross_v3_v3v3(mat[1], cache->sculpt_normal, mat[0]);
  copy_v3_v3(mat[2], cache->sculpt_normal);

  /* Set location. */
  copy_v3_v3(mat[3], cache->location);

  /* Scale by brush radius. */
  float radius = cache->radius;

  /* Square tips should scale by square root of 2. */
  if (BKE_brush_has_cube_tip(cache->brush, PAINT_MODE_SCULPT)) {
    radius += (radius / M_SQRT2 - radius) * cache->brush->tip_roundness;
  }
  else {
    radius /= M_SQRT2;
  }

  normalize_m4(mat);
  scale_m4_fl(scale, radius);
  mul_m4_m4m4(tmat, mat, scale);

  /* Return tmat as is (for converting from local area coords to model-space coords). */
  copy_m4_m4(local_mat_inv, tmat);
  /* Return inverse (for converting from model-space coords to local area coords). */
  invert_m4_m4(local_mat, tmat);
}

#define SCULPT_TILT_SENSITIVITY 0.7f
void SCULPT_tilt_apply_to_normal(float r_normal[3], StrokeCache *cache, const float tilt_strength)
{
  if (!U.experimental.use_sculpt_tools_tilt) {
    return;
  }
  const float rot_max = M_PI_2 * tilt_strength * SCULPT_TILT_SENSITIVITY;
  mul_v3_mat3_m4v3(r_normal, cache->vc->obact->object_to_world, r_normal);
  float normal_tilt_y[3];
  rotate_v3_v3v3fl(normal_tilt_y, r_normal, cache->vc->rv3d->viewinv[0], cache->y_tilt * rot_max);
  float normal_tilt_xy[3];
  rotate_v3_v3v3fl(
      normal_tilt_xy, normal_tilt_y, cache->vc->rv3d->viewinv[1], cache->x_tilt * rot_max);
  mul_v3_mat3_m4v3(r_normal, cache->vc->obact->world_to_object, normal_tilt_xy);
  normalize_v3(r_normal);
}

void SCULPT_tilt_effective_normal_get(const SculptSession *ss, const Brush *brush, float r_no[3])
{
  copy_v3_v3(r_no, ss->cache->sculpt_normal_symm);
  SCULPT_tilt_apply_to_normal(r_no, ss->cache, brush->tilt_strength_factor);
}

static void update_brush_local_mat(Sculpt *sd, Object *ob)
{
  StrokeCache *cache = ob->sculpt->cache;

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0) {
    const Brush *brush = BKE_paint_brush(&sd->paint);
    const MTex *mask_tex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
    calc_brush_local_mat(mask_tex->rot, ob, cache->brush_local_mat, cache->brush_local_mat_inv);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture painting
 * \{ */

static bool sculpt_needs_pbvh_pixels(PaintModeSettings *paint_mode_settings,
                                     const Brush *brush,
                                     Object *ob)
{
  if (brush->sculpt_tool == SCULPT_TOOL_PAINT && U.experimental.use_sculpt_texture_paint) {
    Image *image;
    ImageUser *image_user;
    return SCULPT_paint_image_canvas_get(paint_mode_settings, ob, &image, &image_user);
  }

  return false;
}

static void sculpt_pbvh_update_pixels(PaintModeSettings *paint_mode_settings,
                                      SculptSession *ss,
                                      Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  Mesh *mesh = (Mesh *)ob->data;

  Image *image;
  ImageUser *image_user;
  if (!SCULPT_paint_image_canvas_get(paint_mode_settings, ob, &image, &image_user)) {
    return;
  }

  BKE_pbvh_build_pixels(ss->pbvh, mesh, image, image_user);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Brush Plane & Symmetry Utilities
 * \{ */

struct SculptRaycastData {
  SculptSession *ss;
  const float *ray_start;
  const float *ray_normal;
  bool hit;
  float depth;
  bool original;

  PBVHVertRef active_vertex;
  PBVHFaceRef active_face;
  float *face_normal;

  IsectRayPrecalc isect_precalc;
};

struct SculptFindNearestToRayData {
  SculptSession *ss;
  const float *ray_start, *ray_normal;
  bool hit;
  float depth;
  float dist_sq_to_ray;
  bool original;
};

ePaintSymmetryAreas SCULPT_get_vertex_symm_area(const float co[3])
{
  ePaintSymmetryAreas symm_area = ePaintSymmetryAreas(PAINT_SYMM_AREA_DEFAULT);
  if (co[0] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_X;
  }
  if (co[1] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Y;
  }
  if (co[2] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Z;
  }
  return symm_area;
}

void SCULPT_flip_v3_by_symm_area(float v[3],
                                 const ePaintSymmetryFlags symm,
                                 const ePaintSymmetryAreas symmarea,
                                 const float pivot[3])
{
  for (int i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = ePaintSymmetryFlags(1 << i);
    if (!(symm & symm_it)) {
      continue;
    }
    if (symmarea & symm_it) {
      flip_v3(v, symm_it);
    }
    if (pivot[i] < 0.0f) {
      flip_v3(v, symm_it);
    }
  }
}

void SCULPT_flip_quat_by_symm_area(float quat[4],
                                   const ePaintSymmetryFlags symm,
                                   const ePaintSymmetryAreas symmarea,
                                   const float pivot[3])
{
  for (int i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = ePaintSymmetryFlags(1 << i);
    if (!(symm & symm_it)) {
      continue;
    }
    if (symmarea & symm_it) {
      flip_qt(quat, symm_it);
    }
    if (pivot[i] < 0.0f) {
      flip_qt(quat, symm_it);
    }
  }
}

void SCULPT_calc_brush_plane(
    Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  zero_v3(r_area_co);
  zero_v3(r_area_no);

  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache) &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) ||
       !(brush->flag & BRUSH_ORIGINAL_PLANE) || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
  {
    switch (brush->sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss->cache->true_view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
        break;

      case SCULPT_DISP_DIR_AREA:
        SCULPT_calc_area_normal_and_center(sd, ob, nodes, r_area_no, r_area_co);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss->cache->view_normal);
          normalize_v3(r_area_no);
        }
        break;

      default:
        break;
    }

    /* For flatten center. */
    /* Flatten center has not been calculated yet if we are not using the area normal. */
    if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA) {
      SCULPT_calc_area_center(sd, ob, nodes, r_area_co);
    }

    /* For area normal. */
    if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) &&
        (brush->flag & BRUSH_ORIGINAL_NORMAL))
    {
      copy_v3_v3(r_area_no, ss->cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss->cache->sculpt_normal, r_area_no);
    }

    /* For flatten center. */
    if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) &&
        (brush->flag & BRUSH_ORIGINAL_PLANE))
    {
      copy_v3_v3(r_area_co, ss->cache->last_center);
    }
    else {
      copy_v3_v3(ss->cache->last_center, r_area_co);
    }
  }
  else {
    /* For area normal. */
    copy_v3_v3(r_area_no, ss->cache->sculpt_normal);

    /* For flatten center. */
    copy_v3_v3(r_area_co, ss->cache->last_center);

    /* For area normal. */
    flip_v3(r_area_no, ss->cache->mirror_symmetry_pass);

    /* For flatten center. */
    flip_v3(r_area_co, ss->cache->mirror_symmetry_pass);

    /* For area normal. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_no);

    /* For flatten center. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_co);

    /* Shift the plane for the current tile. */
    add_v3_v3(r_area_co, ss->cache->plane_offset);
  }
}

int SCULPT_plane_trim(const StrokeCache *cache, const Brush *brush, const float val[3])
{
  return (!(brush->flag & BRUSH_PLANE_TRIM) ||
          (dot_v3v3(val, val) <= cache->radius_squared * cache->plane_trim_squared));
}

int SCULPT_plane_point_side(const float co[3], const float plane[4])
{
  float d = plane_point_side_v3(plane, co);
  return d <= 0.0f;
}

float SCULPT_brush_plane_offset_get(Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  float rv = brush->plane_offset;

  if (brush->flag & BRUSH_OFFSET_PRESSURE) {
    rv *= ss->cache->pressure;
  }

  return rv;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Gravity Brush
 * \{ */

static void do_gravity_task_cb_ex(void *__restrict userdata,
                                  const int n,
                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id,
                                                    nullptr);

    mul_v3_v3fl(proxy[vd.i], offset, fade);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_gravity(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float bstrength)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float offset[3];
  float gravity_vector[3];

  mul_v3_v3fl(gravity_vector, ss->cache->gravity_direction, -ss->cache->radius_squared);

  /* Offset with as much as possible factored in already. */
  mul_v3_v3v3(offset, gravity_vector, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.offset = offset;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), &data, do_gravity_task_cb_ex, &settings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Brush Utilities
 * \{ */

void SCULPT_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3])
{
  Mesh *me = (Mesh *)ob->data;
  float(*ofs)[3] = nullptr;
  int a;
  const int kb_act_idx = ob->shapenr - 1;

  /* For relative keys editing of base should update other keys. */
  if (BKE_keyblock_is_basis(me->key, kb_act_idx)) {
    ofs = BKE_keyblock_convert_to_vertcos(ob, kb);

    /* Calculate key coord offsets (from previous location). */
    for (a = 0; a < me->totvert; a++) {
      sub_v3_v3v3(ofs[a], vertCos[a], ofs[a]);
    }

    /* Apply offsets on other keys. */
    LISTBASE_FOREACH (KeyBlock *, currkey, &me->key->block) {
      if ((currkey != kb) && (currkey->relative == kb_act_idx)) {
        BKE_keyblock_update_from_offset(ob, currkey, ofs);
      }
    }

    MEM_freeN(ofs);
  }

  /* Modifying of basis key should update mesh. */
  if (kb == me->key->refkey) {
    BKE_mesh_vert_coords_apply(me, vertCos);
  }

  /* Apply new coords on active key block, no need to re-allocate kb->data here! */
  BKE_keyblock_update_from_vertcos(ob, kb, vertCos);
}

static void topology_undopush_cb(PBVHNode *node, void *data)
{
  SculptSearchSphereData *sdata = (SculptSearchSphereData *)data;

  SCULPT_ensure_dyntopo_node_undo(
      sdata->ob,
      node,
      SCULPT_get_tool(sdata->ob->sculpt, sdata->brush) == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                             SCULPT_UNDO_COORDS,
      0);

  BKE_pbvh_node_mark_update(node);
}

int SCULPT_get_symmetry_pass(const SculptSession *ss)
{
  int symidx = ss->cache->mirror_symmetry_pass + (ss->cache->radial_symmetry_pass * 8);

  if (symidx >= SCULPT_MAX_SYMMETRY_PASSES) {
    symidx = SCULPT_MAX_SYMMETRY_PASSES - 1;
  }

  return symidx;
}

typedef struct DynTopoAutomaskState {
  AutomaskingCache *cache;
  SculptSession *ss;
  AutomaskingCache _fixed;
  bool free_automasking;
} DynTopoAutomaskState;

static float sculpt_topology_automasking_cb(PBVHVertRef vertex, void *vdata)
{
  DynTopoAutomaskState *state = (DynTopoAutomaskState *)vdata;

  float mask = SCULPT_automasking_factor_get(state->cache, state->ss, vertex, nullptr);
  float mask2 = 1.0f - SCULPT_vertex_mask_get(state->ss, vertex);

  return mask * mask2;
}

static float sculpt_topology_automasking_mask_cb(PBVHVertRef vertex, void *vdata)
{
  DynTopoAutomaskState *state = (DynTopoAutomaskState *)vdata;
  return 1.0f - SCULPT_vertex_mask_get(state->ss, vertex);
}

static float sculpt_null_mask_cb(PBVHVertRef /*vertex*/, void * /*vdata*/)
{
  return 1.0f;
}

bool SCULPT_dyntopo_automasking_init(const SculptSession *ss,
                                     Sculpt *sd,
                                     const Brush *br,
                                     Object *ob,
                                     blender::bke::dyntopo::DyntopoMaskCB *r_mask_cb,
                                     void **r_mask_cb_data)
{
  if (!SCULPT_is_automasking_enabled(sd, ss, br)) {
    if (CustomData_has_layer(&ss->bm->vdata, CD_PAINT_MASK)) {
      DynTopoAutomaskState *state = (DynTopoAutomaskState *)MEM_callocN(
          sizeof(DynTopoAutomaskState), "DynTopoAutomaskState");

      if (!ss->cache) {
        state->cache = SCULPT_automasking_cache_init(sd, br, ob);
      }
      else {
        state->cache = ss->cache->automasking;
      }

      state->ss = (SculptSession *)ss;

      *r_mask_cb_data = (void *)state;
      *r_mask_cb = sculpt_topology_automasking_mask_cb;

      return true;
    }
    else {
      *r_mask_cb_data = nullptr;
      *r_mask_cb = sculpt_null_mask_cb;
      return false;
    }
  }

  DynTopoAutomaskState *state = (DynTopoAutomaskState *)MEM_callocN(sizeof(DynTopoAutomaskState),
                                                                    "DynTopoAutomaskState");
  if (!ss->cache) {
    state->cache = SCULPT_automasking_cache_init(sd, br, ob);
    state->free_automasking = true;
  }
  else {
    state->cache = ss->cache->automasking;
  }

  state->ss = (SculptSession *)ss;

  *r_mask_cb_data = (void *)state;
  *r_mask_cb = sculpt_topology_automasking_cb;

  return true;
}

void SCULPT_dyntopo_automasking_end(void *mask_data)
{
  MEM_SAFE_FREE(mask_data);
}

bool SCULPT_needs_area_normal(SculptSession * /*ss*/, Sculpt * /*sd*/, Brush *brush)
{
  return brush->tip_roundness != 1.0f || brush->tip_scale_x != 1.0f;
}

/* Note: we do the topology update before any brush actions to avoid
 * issues with the proxies. The size of the proxy can't change, so
 * topology must be updated first. */
static void sculpt_topology_update(Sculpt *sd,
                                   Object *ob,
                                   Brush *brush,
                                   UnifiedPaintSettings * /* ups */,
                                   PaintModeSettings * /*paint_mode_settings*/)
{
  using namespace blender::bke::dyntopo;
  SculptSession *ss = ob->sculpt;

  /* build brush radius scale */
  float radius_scale = ss->cached_dyntopo.radius_scale;

  if ((brush->dyntopo.flag & DYNTOPO_DISABLED) || !(sd->flags & SCULPT_DYNTOPO_ENABLED)) {
    return;
  }

  /* Build a list of all nodes that are potentially within the brush's area of influence. */
  const bool use_original = sculpt_tool_needs_original(SCULPT_get_tool(ss, brush)) ?
                                true :
                                !ss->cache->accum;

  /* Free index based vertex info as it will become invalid after modifying the topology during
   * the stroke. */
  MEM_SAFE_FREE(ss->vertex_info.boundary);

  PBVHTopologyUpdateMode mode = PBVHTopologyUpdateMode(0);
  float location[3];

  int dyntopo_mode = ss->cached_dyntopo.flag;
  int dyntopo_detail_mode = ss->cached_dyntopo.mode;

  if (dyntopo_detail_mode != DYNTOPO_DETAIL_MANUAL) {
    if (dyntopo_mode & DYNTOPO_SUBDIVIDE) {
      mode |= PBVH_Subdivide;
    }
    else if (dyntopo_mode & DYNTOPO_LOCAL_SUBDIVIDE) {
      mode |= PBVH_LocalSubdivide | PBVH_Subdivide;
    }

    if (dyntopo_mode & DYNTOPO_COLLAPSE) {
      mode |= PBVH_Collapse;
    }
    else if (dyntopo_mode & DYNTOPO_LOCAL_COLLAPSE) {
      mode |= PBVH_LocalCollapse | PBVH_Collapse;
    }
  }
  else {
    if (dyntopo_mode & DYNTOPO_SUBDIVIDE) {
      mode |= PBVH_Subdivide;
    }
    if (dyntopo_mode & DYNTOPO_COLLAPSE) {
      mode |= PBVH_Collapse;
    }
  }

  if (dyntopo_mode & DYNTOPO_CLEANUP) {
    mode |= PBVH_Cleanup;
  }

  /* Force both subdivide and collapse for simplify brush. */
  // XXX done with inherit flags now
  if (brush->sculpt_tool == SCULPT_TOOL_SIMPLIFY) {
    // mode |= PBVH_Collapse | PBVH_Subdivide;
  }

  int edge_multiply = 1 + int(powf(ss->cached_dyntopo.quality, 3.0f) * 50.0f);

  SculptSearchSphereData sdata{};
  sdata.ss = ss, sdata.sd = sd, sdata.ob = ob;
  sdata.radius_squared = square_f(ss->cache->radius * radius_scale * 1.25f);
  sdata.original = use_original;
  sdata.ignore_fully_ineffective = SCULPT_get_tool(ss, brush) != SCULPT_TOOL_MASK;
  sdata.center = nullptr;
  sdata.brush = brush;

  void *mask_cb_data;
  blender::bke::dyntopo::DyntopoMaskCB mask_cb;

  BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);

  SCULPT_dyntopo_automasking_init(ss, sd, brush, ob, &mask_cb, &mask_cb_data);

  int actv = BM_ID_NONE, actf = BM_ID_NONE;

  if (ss->active_vertex.i != PBVH_REF_NONE) {
    BM_idmap_check_assign(ss->bm_idmap, (BMElem *)ss->active_vertex.i);
    actv = BM_idmap_get_id(ss->bm_idmap, (BMElem *)ss->active_vertex.i);
  }

  if (ss->active_face.i != PBVH_REF_NONE) {
    BM_idmap_check_assign(ss->bm_idmap, (BMElem *)ss->active_face.i);
    actf = BM_idmap_get_id(ss->bm_idmap, (BMElem *)ss->active_face.i);
  }

  blender::bke::dyntopo::BrushSphere sphere_tester(ss->cache->location, ss->cache->radius);
  blender::bke::dyntopo::BrushTube tube_tester(
      ss->cache->location, ss->cache->view_normal, ss->cache->radius);

  /* do nodes under the brush cursor */
  blender::bke::dyntopo::remesh_topology_nodes(
      brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE ? &sphere_tester : &tube_tester,
      ob,
      ss->pbvh,
      SCULPT_search_sphere_cb,
      topology_undopush_cb,
      &sdata,
      mode,
      (brush->flag & BRUSH_FRONTFACE) != 0,
      ss->cache->view_normal,
      true,
      mask_cb,
      mask_cb_data,
      edge_multiply);

  SCULPT_dyntopo_automasking_end(mask_cb_data);

  if (actv != BM_ID_NONE) {
    BMVert *v = (BMVert *)BM_idmap_lookup(ss->bm_idmap, actv);

    if (v && v->head.htype == BM_VERT) {
      ss->active_vertex.i = (intptr_t)v;
    }
    else {
      ss->active_vertex.i = PBVH_REF_NONE;
    }
  }

  if (actf != BM_ID_NONE) {
    BMFace *f = (BMFace *)BM_idmap_lookup(ss->bm_idmap, actf);

    if (f && f->head.htype == BM_FACE) {
      ss->active_face.i = (intptr_t)f;
    }
    else {
      ss->active_face.i = PBVH_REF_NONE;
    }
  }

  /* Update average stroke position. */
  copy_v3_v3(location, ss->cache->true_location);
  mul_m4_v3(ob->object_to_world, location);

  ss->totfaces = ss->totpoly = ss->bm->totface;
  ss->totvert = ss->bm->totvert;
}

static void do_check_origco_cb(void *__restrict userdata,
                               const int n,
                               const TaskParallelTLS *__restrict /* tls */)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHVertexIter vd;

  bool modified = false;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    modified |= SCULPT_vertex_check_origdata(ss, vd.vertex);
  }
  BKE_pbvh_vertex_iter_end;

  if (modified) {
    BKE_pbvh_node_mark_original_update(data->nodes[n]);
  }
}

static void do_brush_action_task_cb(void *__restrict userdata,
                                    const int n,
                                    const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;

  bool need_coords = ss->cache->supports_gravity;

  if (data->brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) {
    BKE_pbvh_node_mark_update_face_sets(data->nodes[n]);

    /* Draw face sets in smooth mode moves the vertices. */
    if (ss->cache->alt_smooth) {
      need_coords = true;
    }
    else {
      SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_FACE_SETS);
    }
  }
  else if (data->brush->sculpt_tool == SCULPT_TOOL_MASK) {
    SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_MASK);
    BKE_pbvh_node_mark_update_mask(data->nodes[n]);
  }
  else if (SCULPT_tool_is_paint(data->brush->sculpt_tool)) {
    SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COLOR);
    BKE_pbvh_node_mark_update_color(data->nodes[n]);
  }
  else {
    need_coords = true;
  }

  if (need_coords) {
    SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void do_brush_action(Sculpt *sd,
                            Object *ob,
                            Brush *brush,
                            UnifiedPaintSettings *ups,
                            PaintModeSettings *paint_mode_settings)
{
  SculptSession *ss = ob->sculpt;
  Vector<PBVHNode *> nodes, texnodes;

  /* Check for unsupported features. */
  PBVHType type = BKE_pbvh_type(ss->pbvh);

  if (SCULPT_tool_is_paint(brush->sculpt_tool) && SCULPT_has_loop_colors(ob)) {
    if (type != PBVH_FACES) {
      return;
    }

    BKE_pbvh_ensure_node_loops(ss->pbvh);
  }

  const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) || !ss->cache->accum;
  const bool use_pixels = sculpt_needs_pbvh_pixels(paint_mode_settings, brush, ob);
  bool needs_original = use_original || SCULPT_automasking_needs_original(sd, brush);

  if (sculpt_needs_pbvh_pixels(paint_mode_settings, brush, ob)) {
    sculpt_pbvh_update_pixels(paint_mode_settings, ss, ob);

    texnodes = sculpt_pbvh_gather_texpaint(ob, sd, brush, use_original, 1.0f);

    if (texnodes.is_empty()) {
      return;
    }
  }

  /* Build a list of all nodes that are potentially within the brush's area of influence */

  if (SCULPT_tool_needs_all_pbvh_nodes(brush)) {
    /* These brushes need to update all nodes as they are not constrained by the brush radius */
    nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);
  }
  else if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    nodes = SCULPT_cloth_brush_affected_nodes_gather(ss, brush);
  }
  else {
    float radius_scale = 1.0f;

    /* Corners of square brushes can go outside the brush radius. */
    if (BKE_brush_has_cube_tip(brush, PAINT_MODE_SCULPT)) {
      radius_scale = M_SQRT2;
    }

    /* With these options enabled not all required nodes are inside the original brush radius, so
     * the brush can produce artifacts in some situations. */
    if (brush->sculpt_tool == SCULPT_TOOL_DRAW && brush->flag & BRUSH_ORIGINAL_NORMAL) {
      radius_scale = 2.0f;
    }
    nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale);
  }

  /* Draw Face Sets in draw mode makes a single undo push, in alt-smooth mode deforms the
   * vertices and uses regular coords undo. */
  /* It also assigns the paint_face_set here as it needs to be done regardless of the stroke type
   * and the number of nodes under the brush influence. */
  if (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS &&
      SCULPT_stroke_is_first_brush_step(ss->cache) && !ss->cache->alt_smooth)
  {
    if (ss->cache->invert) {
      /* When inverting the brush, pick the paint face mask ID from the mesh. */
      ss->cache->paint_face_set = SCULPT_active_face_set_get(ss);
    }
    else {
      /* By default create a new Face Sets. */
      ss->cache->paint_face_set = SCULPT_face_set_next_available_get(ss);
    }
  }

  /* For anchored brushes with spherical falloff, we start off with zero radius, thus we have no
   * PBVH nodes on the first brush step. */
  if (!nodes.is_empty() ||
      ((brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) && (brush->flag & BRUSH_ANCHORED)))
  {
    if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
      /* Initialize auto-masking cache. */
      if (SCULPT_is_automasking_enabled(sd, ss, brush)) {
        ss->cache->automasking = SCULPT_automasking_cache_init(sd, brush, ob);
        ss->last_automasking_settings_hash = SCULPT_automasking_settings_hash(
            ob, ss->cache->automasking);
      }
      /* Initialize surface smooth cache. */
      if ((brush->sculpt_tool == SCULPT_TOOL_SMOOTH) &&
          (brush->smooth_deform_type == BRUSH_SMOOTH_DEFORM_SURFACE))
      {
        SCULPT_surface_smooth_laplacian_init(ob);
      }
    }
  }

  if (!ss->cache->accum || needs_original) {
    SculptThreadedTaskData task_data{};
    task_data.sd = sd;
    task_data.ob = ob;
    task_data.brush = brush;
    task_data.nodes = nodes;

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
    BLI_task_parallel_range(0, nodes.size(), &task_data, do_check_origco_cb, &settings);

    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateOriginalBB);
  }

  /* Only act if some verts are inside the brush area. */
  if (nodes.is_empty()) {
    return;
  }
  float location[3];

  if (!use_pixels && !ss->bm) {
    SculptThreadedTaskData task_data{};
    task_data.sd = sd;
    task_data.ob = ob;
    task_data.brush = brush;
    task_data.nodes = nodes;

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
    BLI_task_parallel_range(0, nodes.size(), &task_data, do_brush_action_task_cb, &settings);
  }
  else if (ss->bm) {
    SculptUndoType undo_type;
    int extra_type = 0;

    if (SCULPT_tool_is_paint(brush->sculpt_tool)) {
      undo_type = SCULPT_UNDO_COLOR;
    }
    else if (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) {
      if (ss->cache->alt_smooth) {
        undo_type = SCULPT_UNDO_COORDS;
      }
      else {
        undo_type = SCULPT_UNDO_FACE_SETS;
      }
    }
    else if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      undo_type = SCULPT_UNDO_MASK;
    }
    else {
      undo_type = SCULPT_UNDO_COORDS;
    }

    if (ss->cache->supports_gravity && sd->gravity_factor > 0.0f &&
        undo_type != SCULPT_UNDO_COORDS) {
      extra_type = int(SCULPT_UNDO_COORDS);
    }

    for (int i : nodes.index_range()) {
      SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], undo_type, extra_type);

      switch (undo_type) {
        case SCULPT_UNDO_FACE_SETS:
          BKE_pbvh_node_mark_update_face_sets(nodes[i]);
          break;
        case SCULPT_UNDO_MASK:
          BKE_pbvh_node_mark_update_mask(nodes[i]);
          break;
        case SCULPT_UNDO_COLOR:
          BKE_pbvh_node_mark_update_color(nodes[i]);
          break;
        case SCULPT_UNDO_COORDS:
          BKE_pbvh_node_mark_update(nodes[i]);
          break;
        case SCULPT_UNDO_HIDDEN:
        case SCULPT_UNDO_DYNTOPO_BEGIN:
        case SCULPT_UNDO_DYNTOPO_END:
        case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
        case SCULPT_UNDO_GEOMETRY:
        case SCULPT_UNDO_NO_TYPE:
          break;
      }

      if (extra_type == int(SCULPT_UNDO_COORDS)) {
        BKE_pbvh_node_mark_update(nodes[i]);
      }
    }
  }

  if (sculpt_brush_needs_normal(ss, sd, brush)) {
    update_sculpt_normal(sd, ob, nodes);
  }

  update_brush_local_mat(sd, ob);

  if (brush->sculpt_tool == SCULPT_TOOL_POSE && SCULPT_stroke_is_first_brush_step(ss->cache)) {
    SCULPT_pose_brush_init(sd, ob, ss, brush);
  }

  if (brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (!ss->cache->cloth_sim) {
      ss->cache->cloth_sim = SCULPT_cloth_brush_simulation_create(
          ob, 1.0f, 0.0f, 0.0f, false, true);
      SCULPT_cloth_brush_simulation_init(ss, ss->cache->cloth_sim);
    }
    SCULPT_cloth_brush_store_simulation_state(ss, ss->cache->cloth_sim);
    SCULPT_cloth_brush_ensure_nodes_constraints(
        sd, ob, nodes, ss->cache->cloth_sim, ss->cache->location, FLT_MAX);
  }

  bool invert = ss->cache->pen_flip || ss->cache->invert;
  if (brush->flag & BRUSH_DIR_IN) {
    invert = !invert;
  }

  /* Apply one type of brush action. */
  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
      SCULPT_do_draw_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_SMOOTH:
      if (brush->smooth_deform_type == BRUSH_SMOOTH_DEFORM_LAPLACIAN) {
        SCULPT_do_smooth_brush(sd, ob, nodes);
      }
      else if (brush->smooth_deform_type == BRUSH_SMOOTH_DEFORM_SURFACE) {
        SCULPT_do_surface_smooth_brush(sd, ob, nodes);
      }
      break;
    case SCULPT_TOOL_CREASE:
      SCULPT_do_crease_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_BLOB:
      SCULPT_do_crease_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_PINCH:
      SCULPT_do_pinch_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_INFLATE:
      SCULPT_do_inflate_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_GRAB:
      SCULPT_do_grab_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_ROTATE:
      SCULPT_do_rotate_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      SCULPT_do_snake_hook_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_NUDGE:
      SCULPT_do_nudge_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_THUMB:
      SCULPT_do_thumb_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_LAYER:
      SCULPT_do_layer_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_FLATTEN:
      SCULPT_do_flatten_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_CLAY:
      SCULPT_do_clay_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_CLAY_STRIPS:
      SCULPT_do_clay_strips_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      SCULPT_do_multiplane_scrape_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_CLAY_THUMB:
      SCULPT_do_clay_thumb_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_FILL:
      if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        SCULPT_do_scrape_brush(sd, ob, nodes);
      }
      else {
        SCULPT_do_fill_brush(sd, ob, nodes);
      }
      break;
    case SCULPT_TOOL_SCRAPE:
      if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        SCULPT_do_fill_brush(sd, ob, nodes);
      }
      else {
        SCULPT_do_scrape_brush(sd, ob, nodes);
      }
      break;
    case SCULPT_TOOL_MASK:
      SCULPT_do_mask_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_POSE:
      SCULPT_do_pose_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_DRAW_SHARP:
      SCULPT_do_draw_sharp_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      SCULPT_do_elastic_deform_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      SCULPT_do_slide_relax_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_BOUNDARY:
      SCULPT_do_boundary_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_CLOTH:
      SCULPT_do_cloth_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_DRAW_FACE_SETS:
      SCULPT_do_draw_face_sets_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
      SCULPT_do_displacement_eraser_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      SCULPT_do_displacement_smear_brush(sd, ob, nodes);
      break;
    case SCULPT_TOOL_PAINT:
      SCULPT_do_paint_brush(paint_mode_settings, ss->scene, sd, ob, nodes, texnodes);
      break;
    case SCULPT_TOOL_SMEAR:
      SCULPT_do_smear_brush(sd, ob, nodes);
      break;
  }

  if (!ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK) &&
      brush->autosmooth_factor > 0)
  {
    if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
      SCULPT_smooth(sd, ob, nodes, brush->autosmooth_factor * (1.0f - ss->cache->pressure), false);
    }
    else {
      SCULPT_smooth(sd, ob, nodes, brush->autosmooth_factor, false);
    }
  }

  if (sculpt_brush_use_topology_rake(ss, brush)) {
    SCULPT_bmesh_topology_rake(sd, ob, nodes, brush->topology_rake_factor);
  }

  if (!SCULPT_tool_can_reuse_automask(brush->sculpt_tool) ||
      (ss->cache->supports_gravity && sd->gravity_factor > 0.0f))
  {
    /* Clear cavity mask cache. */
    ss->last_automasking_settings_hash = 0;
  }

  /* The cloth brush adds the gravity as a regular force and it is processed in the solver. */
  if (ss->cache->supports_gravity &&
      !ELEM(
          brush->sculpt_tool, SCULPT_TOOL_CLOTH, SCULPT_TOOL_DRAW_FACE_SETS, SCULPT_TOOL_BOUNDARY))
  {
    do_gravity(sd, ob, nodes, sd->gravity_factor);
  }

  if (brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
      SCULPT_cloth_sim_activate_nodes(ss->cache->cloth_sim, nodes);
      SCULPT_cloth_brush_do_simulation_step(sd, ob, ss->cache->cloth_sim, nodes);
    }
  }

  /* Update average stroke position. */
  copy_v3_v3(location, ss->cache->true_location);
  mul_m4_v3(ob->object_to_world, location);

  add_v3_v3(ups->average_stroke_accum, location);
  ups->average_stroke_counter++;
  /* Update last stroke position. */
  ups->last_stroke_valid = true;
}

/* Flush displacement from deformed PBVH vertex to original mesh. */
static void sculpt_flush_pbvhvert_deform(const SculptSession &ss,
                                         const PBVHVertexIter &vd,
                                         MutableSpan<float3> positions)
{
  float disp[3], newco[3];
  int index = vd.vert_indices[vd.i];

  sub_v3_v3v3(disp, vd.co, ss.deform_cos[index]);
  mul_m3_v3(ss.deform_imats[index], disp);
  add_v3_v3v3(newco, disp, ss.orig_cos[index]);

  copy_v3_v3(ss.deform_cos[index], vd.co);
  copy_v3_v3(ss.orig_cos[index], newco);

  if (!ss.shapekey_active) {
    copy_v3_v3(positions[index], newco);
  }
}

static void sculpt_combine_proxies_node(Object &object,
                                        Sculpt &sd,
                                        const bool use_orco,
                                        PBVHNode &node)
{
  SculptSession *ss = object.sculpt;

  int proxy_count;
  PBVHProxyNode *proxies;
  BKE_pbvh_node_get_proxies(&node, &proxies, &proxy_count);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  MutableSpan<float3> positions = mesh.vert_positions_for_write();

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, &node, vd, PBVH_ITER_UNIQUE) {
    float val[3];

    zero_v3(val);

    for (int p = 0; p < proxy_count; p++) {
      add_v3_v3(val, proxies[p].co[vd.i]);
    }

    bool modified = len_squared_v3(val) > 0.0f;

    if (use_orco) {
      add_v3_v3(val, SCULPT_vertex_origco_get(ss, vd.vertex));
    }
    else {
      add_v3_v3(val, vd.co);
    }

    SCULPT_clip(&sd, ss, vd.co, val);

    if (ss->deform_modifiers_active) {
      sculpt_flush_pbvhvert_deform(*ss, vd, positions);
    }

    if (modified) {
      BKE_sculpt_sharp_boundary_flag_update(ss, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_free_proxies(&node);
}

static void sculpt_combine_proxies(Sculpt *sd, Object *ob)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!ss->cache->supports_gravity && sculpt_tool_is_proxy_used(brush->sculpt_tool)) {
    /* First line is tools that don't support proxies. */
    return;
  }

  /* First line is tools that don't support proxies. */
  const bool use_orco = ELEM(brush->sculpt_tool,
                             SCULPT_TOOL_GRAB,
                             SCULPT_TOOL_ROTATE,
                             SCULPT_TOOL_THUMB,
                             SCULPT_TOOL_ELASTIC_DEFORM,
                             SCULPT_TOOL_BOUNDARY,
                             SCULPT_TOOL_POSE);

  Vector<PBVHNode *> nodes = blender::bke::pbvh::gather_proxies(ss->pbvh);

  threading::parallel_for(nodes.index_range(), 1, [&](IndexRange range) {
    for (const int i : range) {
      sculpt_combine_proxies_node(*ob, *sd, use_orco, *nodes[i]);
    }
  });
}

void SCULPT_combine_transform_proxies(Sculpt *sd, Object *ob)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;

  Vector<PBVHNode *> nodes = blender::bke::pbvh::gather_proxies(ss->pbvh);

  threading::parallel_for(nodes.index_range(), 1, [&](IndexRange range) {
    for (const int i : range) {
      sculpt_combine_proxies_node(*ob, *sd, false, *nodes[i]);
    }
  });
}

/**
 * Copy the modified vertices from the #PBVH to the active key.
 */
static void sculpt_update_keyblock(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  float(*vertCos)[3];

  /* Key-block update happens after handling deformation caused by modifiers,
   * so ss->orig_cos would be updated with new stroke. */
  if (ss->orig_cos) {
    vertCos = ss->orig_cos;
  }
  else {
    vertCos = BKE_pbvh_vert_coords_alloc(ss->pbvh);
  }

  if (!vertCos) {
    return;
  }

  SCULPT_vertcos_to_key(ob, ss->shapekey_active, vertCos);

  if (vertCos != ss->orig_cos) {
    MEM_freeN(vertCos);
  }
}

void SCULPT_flush_stroke_deform(Sculpt * /*sd*/, Object *ob, bool is_proxy_used)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;

  if (is_proxy_used && ss->deform_modifiers_active) {
    /* This brushes aren't using proxies, so sculpt_combine_proxies() wouldn't propagate needed
     * deformation to original base. */

    Mesh *me = (Mesh *)ob->data;
    Vector<PBVHNode *> nodes;
    float(*vertCos)[3] = nullptr;

    if (ss->shapekey_active) {
      vertCos = static_cast<float(*)[3]>(
          MEM_mallocN(sizeof(*vertCos) * me->totvert, "flushStrokeDeofrm keyVerts"));

      /* Mesh could have isolated verts which wouldn't be in BVH, to deal with this we copy old
       * coordinates over new ones and then update coordinates for all vertices from BVH. */
      memcpy(vertCos, ss->orig_cos, sizeof(*vertCos) * me->totvert);
    }

    nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

    MutableSpan<float3> positions = me->vert_positions_for_write();

    threading::parallel_for(nodes.index_range(), 1, [&](IndexRange range) {
      for (const int i : range) {
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[i], vd, PBVH_ITER_UNIQUE) {
          sculpt_flush_pbvhvert_deform(*ss, vd, positions);

          if (!vertCos) {
            continue;
          }

          int index = vd.vert_indices[vd.i];
          copy_v3_v3(vertCos[index], ss->orig_cos[index]);
        }
        BKE_pbvh_vertex_iter_end;
      }
    });

    if (vertCos) {
      SCULPT_vertcos_to_key(ob, ss->shapekey_active, vertCos);
      MEM_freeN(vertCos);
    }
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }
}

void SCULPT_cache_calc_brushdata_symm(StrokeCache *cache,
                                      const ePaintSymmetryFlags symm,
                                      const char axis,
                                      const float angle)
{
  flip_v3_v3(cache->location, cache->true_location, symm);
  flip_v3_v3(cache->last_location, cache->true_last_location, symm);
  flip_v3_v3(cache->grab_delta_symmetry, cache->grab_delta, symm);
  flip_v3_v3(cache->view_normal, cache->true_view_normal, symm);

  flip_v3_v3(cache->initial_location, cache->true_initial_location, symm);
  flip_v3_v3(cache->initial_normal, cache->true_initial_normal, symm);

  /* XXX This reduces the length of the grab delta if it approaches the line of symmetry
   * XXX However, a different approach appears to be needed. */
#if 0
  if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
    float frac = 1.0f / max_overlap_count(sd);
    float reduce = (feather - frac) / (1.0f - frac);

    printf("feather: %f frac: %f reduce: %f\n", feather, frac, reduce);

    if (frac < 1.0f) {
      mul_v3_fl(cache->grab_delta_symmetry, reduce);
    }
  }
#endif

  unit_m4(cache->symm_rot_mat);
  unit_m4(cache->symm_rot_mat_inv);
  zero_v3(cache->plane_offset);

  /* Expects XYZ. */
  if (axis) {
    rotate_m4(cache->symm_rot_mat, axis, angle);
    rotate_m4(cache->symm_rot_mat_inv, axis, -angle);
  }

  mul_m4_v3(cache->symm_rot_mat, cache->location);
  mul_m4_v3(cache->symm_rot_mat, cache->grab_delta_symmetry);

  if (cache->supports_gravity) {
    flip_v3_v3(cache->gravity_direction, cache->true_gravity_direction, symm);
    mul_m4_v3(cache->symm_rot_mat, cache->gravity_direction);
  }

  if (cache->is_rake_rotation_valid) {
    flip_qt_qt(cache->rake_rotation_symmetry, cache->rake_rotation, symm);
  }
}

using BrushActionFunc = void (*)(Sculpt *sd,
                                 Object *ob,
                                 Brush *brush,
                                 UnifiedPaintSettings *ups,
                                 PaintModeSettings *paint_mode_settings);

static void do_tiled(Sculpt *sd,
                     Object *ob,
                     Brush *brush,
                     UnifiedPaintSettings *ups,
                     PaintModeSettings *paint_mode_settings,
                     BrushActionFunc action)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float radius = cache->radius;
  const BoundBox *bb = BKE_object_boundbox_get(ob);
  const float *bbMin = bb->vec[0];
  const float *bbMax = bb->vec[6];
  const float *step = sd->paint.tile_offset;

  /* These are integer locations, for real location: multiply with step and add orgLoc.
   * So 0,0,0 is at orgLoc. */
  int start[3];
  int end[3];
  int cur[3];

  /* Position of the "prototype" stroke for tiling. */
  float orgLoc[3];
  float original_initial_location[3];
  copy_v3_v3(orgLoc, cache->location);
  copy_v3_v3(original_initial_location, cache->initial_location);

  for (int dim = 0; dim < 3; dim++) {
    if ((sd->paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bbMin[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bbMax[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }

  /* First do the "un-tiled" position to initialize the stroke for this location. */
  cache->tile_pass = 0;
  action(sd, ob, brush, ups, paint_mode_settings);

  /* Now do it for all the tiles. */
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          /* Skip tile at orgLoc, this was already handled before all others. */
          continue;
        }

        ++cache->tile_pass;

        for (int dim = 0; dim < 3; dim++) {
          cache->location[dim] = cur[dim] * step[dim] + orgLoc[dim];
          cache->plane_offset[dim] = cur[dim] * step[dim];
          cache->initial_location[dim] = cur[dim] * step[dim] + original_initial_location[dim];
        }
        action(sd, ob, brush, ups, paint_mode_settings);
      }
    }
  }
}

static void do_radial_symmetry(Sculpt *sd,
                               Object *ob,
                               Brush *brush,
                               UnifiedPaintSettings *ups,
                               PaintModeSettings *paint_mode_settings,
                               BrushActionFunc action,
                               const ePaintSymmetryFlags symm,
                               const int axis,
                               const float /*feather*/)
{
  SculptSession *ss = ob->sculpt;

  for (int i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd->radial_symm[axis - 'X'];
    ss->cache->radial_symmetry_pass = i;
    SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);
    do_tiled(sd, ob, brush, ups, paint_mode_settings, action);
  }
}

/**
 * Noise texture gives different values for the same input coord; this
 * can tear a multi-resolution mesh during sculpting so do a stitch in this case.
 */
static void sculpt_fix_noise_tear(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const MTex *mtex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);

  if (ss->multires.active && mtex->tex && mtex->tex->type == TEX_NOISE) {
    multires_stitch_grids(ob);
  }
}

static void do_symmetrical_brush_actions(Sculpt *sd,
                                         Object *ob,
                                         BrushActionFunc action,
                                         UnifiedPaintSettings *ups,
                                         PaintModeSettings *paint_mode_settings)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float feather = calc_symmetry_feather(sd, ss->cache);

  cache->bstrength = brush_strength(sd, cache, feather, ups, paint_mode_settings);
  cache->symmetry = symm;

  /* `symm` is a bit combination of XYZ -
   * 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (int i = 0; i <= symm; i++) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    const ePaintSymmetryFlags symm = ePaintSymmetryFlags(i);
    cache->mirror_symmetry_pass = symm;
    cache->radial_symmetry_pass = 0;

    SCULPT_cache_calc_brushdata_symm(cache, symm, 0, 0);
    do_tiled(sd, ob, brush, ups, paint_mode_settings, action);

    do_radial_symmetry(sd, ob, brush, ups, paint_mode_settings, action, symm, 'X', feather);
    do_radial_symmetry(sd, ob, brush, ups, paint_mode_settings, action, symm, 'Y', feather);
    do_radial_symmetry(sd, ob, brush, ups, paint_mode_settings, action, symm, 'Z', feather);
  }
}

bool SCULPT_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT;
}

bool SCULPT_mode_poll_view3d(bContext *C)
{
  return (SCULPT_mode_poll(C) && CTX_wm_region_view3d(C));
}

bool SCULPT_poll_view3d(bContext *C)
{
  return (SCULPT_poll(C) && CTX_wm_region_view3d(C));
}

bool SCULPT_poll(bContext *C)
{
  return SCULPT_mode_poll(C) && PAINT_brush_tool_poll(C);
}

static const char *sculpt_tool_name(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((eBrushSculptTool)brush->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
      return "Draw Brush";
    case SCULPT_TOOL_SMOOTH:
      return "Smooth Brush";
    case SCULPT_TOOL_CREASE:
      return "Crease Brush";
    case SCULPT_TOOL_BLOB:
      return "Blob Brush";
    case SCULPT_TOOL_PINCH:
      return "Pinch Brush";
    case SCULPT_TOOL_INFLATE:
      return "Inflate Brush";
    case SCULPT_TOOL_GRAB:
      return "Grab Brush";
    case SCULPT_TOOL_NUDGE:
      return "Nudge Brush";
    case SCULPT_TOOL_THUMB:
      return "Thumb Brush";
    case SCULPT_TOOL_LAYER:
      return "Layer Brush";
    case SCULPT_TOOL_FLATTEN:
      return "Flatten Brush";
    case SCULPT_TOOL_CLAY:
      return "Clay Brush";
    case SCULPT_TOOL_CLAY_STRIPS:
      return "Clay Strips Brush";
    case SCULPT_TOOL_CLAY_THUMB:
      return "Clay Thumb Brush";
    case SCULPT_TOOL_FILL:
      return "Fill Brush";
    case SCULPT_TOOL_SCRAPE:
      return "Scrape Brush";
    case SCULPT_TOOL_SNAKE_HOOK:
      return "Snake Hook Brush";
    case SCULPT_TOOL_ROTATE:
      return "Rotate Brush";
    case SCULPT_TOOL_MASK:
      return "Mask Brush";
    case SCULPT_TOOL_SIMPLIFY:
      return "Simplify Brush";
    case SCULPT_TOOL_DRAW_SHARP:
      return "Draw Sharp Brush";
    case SCULPT_TOOL_ELASTIC_DEFORM:
      return "Elastic Deform Brush";
    case SCULPT_TOOL_POSE:
      return "Pose Brush";
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      return "Multi-plane Scrape Brush";
    case SCULPT_TOOL_SLIDE_RELAX:
      return "Slide/Relax Brush";
    case SCULPT_TOOL_BOUNDARY:
      return "Boundary Brush";
    case SCULPT_TOOL_CLOTH:
      return "Cloth Brush";
    case SCULPT_TOOL_DRAW_FACE_SETS:
      return "Draw Face Sets";
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
      return "Multires Displacement Eraser";
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      return "Multires Displacement Smear";
    case SCULPT_TOOL_PAINT:
      return "Paint Brush";
    case SCULPT_TOOL_SMEAR:
      return "Smear Brush";
  }

  return "Sculpting";
}

/* Operator for applying a stroke (various attributes including mouse path)
 * using the current brush. */

void SCULPT_cache_free(SculptSession * /*ss*/, struct Object * /*ob*/, StrokeCache *cache)
{
  MEM_SAFE_FREE(cache->dial);
  MEM_SAFE_FREE(cache->prev_colors);
  MEM_SAFE_FREE(cache->prev_displacement);
  MEM_SAFE_FREE(cache->limit_surface_co);
  MEM_SAFE_FREE(cache->prev_colors_vpaint);

  if (cache->pose_ik_chain) {
    SCULPT_pose_ik_chain_free(cache->pose_ik_chain);
  }

  for (int i = 0; i < PAINT_SYMM_AREAS; i++) {
    if (cache->boundaries[i]) {
      SCULPT_boundary_data_free(cache->boundaries[i]);
    }
  }

  if (cache->cloth_sim) {
    SCULPT_cloth_simulation_free(cache->cloth_sim);
  }

  MEM_freeN(cache);
}

/* Initialize mirror modifier clipping. */
static void sculpt_init_mirror_clipping(Object *ob, SculptSession *ss)
{
  unit_m4(ss->cache->clip_mirror_mtx);

  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (!(md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime))) {
      continue;
    }
    MirrorModifierData *mmd = (MirrorModifierData *)md;

    if (!(mmd->flag & MOD_MIR_CLIPPING)) {
      continue;
    }
    /* Check each axis for mirroring. */
    for (int i = 0; i < 3; i++) {
      if (!(mmd->flag & (MOD_MIR_AXIS_X << i))) {
        continue;
      }
      /* Enable sculpt clipping. */
      ss->cache->flag |= CLIP_X << i;

      /* Update the clip tolerance. */
      if (mmd->tolerance > ss->cache->clip_tolerance[i]) {
        ss->cache->clip_tolerance[i] = mmd->tolerance;
      }

      /* Store matrix for mirror object clipping. */
      if (mmd->mirror_ob) {
        float imtx_mirror_ob[4][4];
        invert_m4_m4(imtx_mirror_ob, mmd->mirror_ob->object_to_world);
        mul_m4_m4m4(ss->cache->clip_mirror_mtx, imtx_mirror_ob, ob->object_to_world);
      }
    }
  }
}

static void smooth_brush_toggle_on(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Scene *scene = CTX_data_scene(C);
  Brush *brush = paint->brush;

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    cache->saved_mask_brush_tool = brush->mask_tool;
    brush->mask_tool = BRUSH_MASK_SMOOTH;
  }
  else if (ELEM(brush->sculpt_tool,
                SCULPT_TOOL_SLIDE_RELAX,
                SCULPT_TOOL_DRAW_FACE_SETS,
                SCULPT_TOOL_PAINT,
                SCULPT_TOOL_SMEAR))
  {
    /* Do nothing, this tool has its own smooth mode. */
  }
  else {
    int cur_brush_size = BKE_brush_size_get(scene, brush);

    STRNCPY(cache->saved_active_brush_name, brush->id.name + 2);

    /* Switch to the smooth brush. */
    brush = BKE_paint_toolslots_brush_get(paint, SCULPT_TOOL_SMOOTH);
    if (brush) {
      BKE_paint_brush_set(paint, brush);
      cache->saved_smooth_size = BKE_brush_size_get(scene, brush);
      BKE_brush_size_set(scene, brush, cur_brush_size);
      BKE_curvemapping_init(brush->curve);
    }
  }
}

static void smooth_brush_toggle_off(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Brush *brush = BKE_paint_brush(paint);

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    brush->mask_tool = cache->saved_mask_brush_tool;
  }
  else if (ELEM(brush->sculpt_tool,
                SCULPT_TOOL_SLIDE_RELAX,
                SCULPT_TOOL_DRAW_FACE_SETS,
                SCULPT_TOOL_PAINT,
                SCULPT_TOOL_SMEAR))
  {
    /* Do nothing. */
  }
  else {
    /* Try to switch back to the saved/previous brush. */
    BKE_brush_size_set(scene, brush, cache->saved_smooth_size);
    brush = (Brush *)BKE_libblock_find_name(bmain, ID_BR, cache->saved_active_brush_name);
    if (brush) {
      BKE_paint_brush_set(paint, brush);
    }
  }
}

/* Initialize the stroke cache invariants from operator properties. */
static void sculpt_update_cache_invariants(
    bContext *C, Sculpt *sd, SculptSession *ss, wmOperator *op, const float mval[2])
{
  StrokeCache *cache = static_cast<StrokeCache *>(
      MEM_callocN(sizeof(StrokeCache), "stroke cache"));
  ToolSettings *tool_settings = CTX_data_tool_settings(C);
  UnifiedPaintSettings *ups = &tool_settings->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&sd->paint);
  ViewContext *vc = paint_stroke_view_context(static_cast<PaintStroke *>(op->customdata));
  Object *ob = CTX_data_active_object(C);
  float mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  float max_scale;
  int mode;

  ss->hard_edge_mode = ups->hard_edge_mode;
  ss->smooth_boundary_flag = eSculptBoundary(ups->smooth_boundary_flag);

  Mesh *me = BKE_object_get_original_mesh(ob);
  BKE_sculptsession_reproject_smooth_set(ob, !(me->flag & ME_SCULPT_IGNORE_UVS));

  ss->cache = cache;

  /* Set scaling adjustment. */
  max_scale = 0.0f;
  for (int i = 0; i < 3; i++) {
    max_scale = max_ff(max_scale, fabsf(ob->scale[i]));
  }
  cache->scale[0] = max_scale / ob->scale[0];
  cache->scale[1] = max_scale / ob->scale[1];
  cache->scale[2] = max_scale / ob->scale[2];

  cache->plane_trim_squared = brush->plane_trim * brush->plane_trim;

  cache->flag = 0;

  sculpt_init_mirror_clipping(ob, ss);

  /* Initial mouse location. */
  if (mval) {
    copy_v2_v2(cache->initial_mouse, mval);
  }
  else {
    zero_v2(cache->initial_mouse);
  }

  copy_v3_v3(cache->initial_location, ss->cursor_location);
  copy_v3_v3(cache->true_initial_location, ss->cursor_location);

  copy_v3_v3(cache->initial_normal, ss->cursor_normal);
  copy_v3_v3(cache->true_initial_normal, ss->cursor_normal);

  mode = RNA_enum_get(op->ptr, "mode");
  cache->invert = mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
  cache->normal_weight = brush->normal_weight;

  /* Interpret invert as following normal, for grab brushes. */
  if (SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool)) {
    if (cache->invert) {
      cache->invert = false;
      cache->normal_weight = (cache->normal_weight == 0.0f);
    }
  }

  /* Not very nice, but with current events system implementation
   * we can't handle brush appearance inversion hotkey separately (sergey). */
  if (cache->invert) {
    ups->draw_inverted = true;
  }
  else {
    ups->draw_inverted = false;
  }

  /* Alt-Smooth. */
  if (cache->alt_smooth) {
    smooth_brush_toggle_on(C, &sd->paint, cache);
    /* Refresh the brush pointer in case we switched brush in the toggle function. */
    brush = BKE_paint_brush(&sd->paint);
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  copy_v2_v2(cache->mouse_event, cache->initial_mouse);
  copy_v2_v2(ups->tex_mouse, cache->initial_mouse);

  /* Truly temporary data that isn't stored in properties. */
  cache->vc = vc;
  cache->brush = brush;

  /* Cache projection matrix. */
  ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

  invert_m4_m4(ob->world_to_object, ob->object_to_world);
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->world_to_object);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(cache->true_view_normal, viewDir);

  cache->supports_gravity = (!ELEM(brush->sculpt_tool,
                                   SCULPT_TOOL_MASK,
                                   SCULPT_TOOL_SMOOTH,
                                   SCULPT_TOOL_SIMPLIFY,
                                   SCULPT_TOOL_DISPLACEMENT_SMEAR,
                                   SCULPT_TOOL_DISPLACEMENT_ERASER) &&
                             (sd->gravity_factor > 0.0f));
  /* Get gravity vector in world space. */
  if (cache->supports_gravity) {
    if (sd->gravity_object) {
      Object *gravity_object = sd->gravity_object;

      copy_v3_v3(cache->true_gravity_direction, gravity_object->object_to_world[2]);
    }
    else {
      cache->true_gravity_direction[0] = cache->true_gravity_direction[1] = 0.0f;
      cache->true_gravity_direction[2] = 1.0f;
    }

    /* Transform to sculpted object space. */
    mul_m3_v3(mat, cache->true_gravity_direction);
    normalize_v3(cache->true_gravity_direction);
  }

  cache->accum = true;

  /* Make copies of the mesh vertex locations and normals for some tools. */
  if (brush->flag & BRUSH_ANCHORED) {
    cache->accum = false;
  }

  /* Draw sharp does not need the original coordinates to produce the accumulate effect, so it
   * should work the opposite way. */
  if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
    cache->accum = false;
  }

  if (SCULPT_TOOL_HAS_ACCUMULATE(brush->sculpt_tool)) {
    if (!(brush->flag & BRUSH_ACCUMULATE)) {
      cache->accum = false;
      if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
        cache->accum = true;
      }
    }
  }

  /* Original coordinates require the sculpt undo system, which isn't used
   * for image brushes. It's also not necessary, just disable it. */
  if (brush && brush->sculpt_tool == SCULPT_TOOL_PAINT &&
      SCULPT_use_image_paint_brush(&tool_settings->paint_mode, ob))
  {
    cache->accum = true;
  }

  cache->first_time = true;

#define PIXEL_INPUT_THRESHHOLD 5
  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->dial = BLI_dial_init(cache->initial_mouse, PIXEL_INPUT_THRESHHOLD);
  }

#undef PIXEL_INPUT_THRESHHOLD

  if (ss->pbvh) {
    /* NotForPR: draw original coordinates for debugging. */
    BKE_pbvh_show_orig_set(ss->pbvh, tool_settings->show_origco);
  }

  if (SCULPT_tool_is_paint(brush->sculpt_tool)) {
    BKE_sculpt_ensure_origcolor(ob);
  }
  else if (SCULPT_tool_is_mask(brush->sculpt_tool)) {
    BKE_sculpt_ensure_origmask(ob);
  }

  SCULPT_apply_dyntopo_settings(CTX_data_scene(C), ss, sd, brush);

  BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);
}

static float sculpt_brush_dynamic_size_get(Brush *brush, StrokeCache *cache, float initial_size)
{
  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_CLAY:
      return max_ff(initial_size * 0.20f, initial_size * pow3f(cache->pressure));
    case SCULPT_TOOL_CLAY_STRIPS:
      return max_ff(initial_size * 0.30f, initial_size * powf(cache->pressure, 1.5f));
    case SCULPT_TOOL_CLAY_THUMB: {
      float clay_stabilized_pressure = SCULPT_clay_thumb_get_stabilized_pressure(cache);
      return initial_size * clay_stabilized_pressure;
    }
    default:
      return initial_size * cache->pressure;
  }
}

/* In these brushes the grab delta is calculated always from the initial stroke location, which is
 * generally used to create grab deformations. */
static bool sculpt_needs_delta_from_anchored_origin(Brush *brush)
{
  if (brush->sculpt_tool == SCULPT_TOOL_SMEAR && (brush->flag & BRUSH_ANCHORED)) {
    return true;
  }

  if (ELEM(brush->sculpt_tool,
           SCULPT_TOOL_GRAB,
           SCULPT_TOOL_POSE,
           SCULPT_TOOL_BOUNDARY,
           SCULPT_TOOL_THUMB,
           SCULPT_TOOL_ELASTIC_DEFORM))
  {
    return true;
  }
  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH &&
      brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
    return true;
  }
  return false;
}

/* In these brushes the grab delta is calculated from the previous stroke location, which is used
 * to calculate to orientate the brush tip and deformation towards the stroke direction. */
static bool sculpt_needs_delta_for_tip_orientation(Brush *brush)
{
  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    return brush->cloth_deform_type != BRUSH_CLOTH_DEFORM_GRAB;
  }
  return ELEM(brush->sculpt_tool,
              SCULPT_TOOL_CLAY_STRIPS,
              SCULPT_TOOL_PINCH,
              SCULPT_TOOL_MULTIPLANE_SCRAPE,
              SCULPT_TOOL_CLAY_THUMB,
              SCULPT_TOOL_NUDGE,
              SCULPT_TOOL_SNAKE_HOOK);
}

static void sculpt_update_brush_delta(UnifiedPaintSettings *ups, Object *ob, Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float mval[2] = {
      cache->mouse_event[0],
      cache->mouse_event[1],
  };
  int tool = brush->sculpt_tool;

  if (!ELEM(tool,
            SCULPT_TOOL_PAINT,
            SCULPT_TOOL_GRAB,
            SCULPT_TOOL_ELASTIC_DEFORM,
            SCULPT_TOOL_CLOTH,
            SCULPT_TOOL_NUDGE,
            SCULPT_TOOL_CLAY_STRIPS,
            SCULPT_TOOL_PINCH,
            SCULPT_TOOL_MULTIPLANE_SCRAPE,
            SCULPT_TOOL_CLAY_THUMB,
            SCULPT_TOOL_SNAKE_HOOK,
            SCULPT_TOOL_POSE,
            SCULPT_TOOL_BOUNDARY,
            SCULPT_TOOL_SMEAR,
            SCULPT_TOOL_THUMB) &&
      !sculpt_brush_use_topology_rake(ss, brush))
  {
    return;
  }
  float grab_location[3], imat[4][4], delta[3], loc[3];

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    if (tool == SCULPT_TOOL_GRAB && brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
      copy_v3_v3(cache->orig_grab_location,
                 SCULPT_vertex_co_for_grab_active_get(ss, SCULPT_active_vertex_get(ss)));
    }
    else {
      copy_v3_v3(cache->orig_grab_location, cache->true_location);
    }
  }
  else if (tool == SCULPT_TOOL_SNAKE_HOOK ||
           (tool == SCULPT_TOOL_CLOTH &&
            brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK))
  {
    add_v3_v3(cache->true_location, cache->grab_delta);
  }

  /* Compute 3d coordinate at same z from original location + mval. */
  mul_v3_m4v3(loc, ob->object_to_world, cache->orig_grab_location);
  ED_view3d_win_to_3d(cache->vc->v3d, cache->vc->region, loc, mval, grab_location);

  /* Compute delta to move verts by. */
  if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    if (sculpt_needs_delta_from_anchored_origin(brush)) {
      sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
      invert_m4_m4(imat, ob->object_to_world);
      mul_mat3_m4_v3(imat, delta);
      add_v3_v3(cache->grab_delta, delta);
    }
    else if (sculpt_needs_delta_for_tip_orientation(brush)) {
      if (brush->flag & BRUSH_ANCHORED) {
        float orig[3];
        mul_v3_m4v3(orig, ob->object_to_world, cache->orig_grab_location);
        sub_v3_v3v3(cache->grab_delta, grab_location, orig);
      }
      else {
        sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
      }
      invert_m4_m4(imat, ob->object_to_world);
      mul_mat3_m4_v3(imat, cache->grab_delta);
    }
    else {
      /* Use for 'Brush.topology_rake_factor'. */
      sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
    }
  }
  else {
    zero_v3(cache->grab_delta);
  }

  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_plane_v3_v3v3(cache->grab_delta, cache->grab_delta, ss->cache->true_view_normal);
  }

  copy_v3_v3(cache->old_grab_location, grab_location);

  if (tool == SCULPT_TOOL_GRAB) {
    if (brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
      copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
    }
    else {
      copy_v3_v3(cache->anchored_location, cache->true_location);
    }
  }
  else if (tool == SCULPT_TOOL_ELASTIC_DEFORM || SCULPT_is_cloth_deform_brush(brush)) {
    copy_v3_v3(cache->anchored_location, cache->true_location);
  }
  else if (tool == SCULPT_TOOL_THUMB) {
    copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
  }

  if (sculpt_needs_delta_from_anchored_origin(brush)) {
    /* Location stays the same for finding vertices in brush radius. */
    copy_v3_v3(cache->true_location, cache->orig_grab_location);

    ups->draw_anchored = true;
    copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
    ups->anchored_size = ups->pixel_radius;
  }

  /* Handle 'rake' */
  cache->is_rake_rotation_valid = false;

  invert_m4_m4(imat, ob->object_to_world);
  mul_mat3_m4_v3(imat, grab_location);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    copy_v3_v3(cache->rake_data.follow_co, grab_location);
  }

  if (!sculpt_brush_needs_rake_rotation(brush)) {
    return;
  }
  cache->rake_data.follow_dist = cache->radius * SCULPT_RAKE_BRUSH_FACTOR;

  if (!is_zero_v3(cache->grab_delta)) {
    const float eps = 0.00001f;

    float v1[3], v2[3];

    copy_v3_v3(v1, cache->rake_data.follow_co);
    copy_v3_v3(v2, cache->rake_data.follow_co);
    sub_v3_v3(v2, cache->grab_delta);

    sub_v3_v3(v1, grab_location);
    sub_v3_v3(v2, grab_location);

    if ((normalize_v3(v2) > eps) && (normalize_v3(v1) > eps) && (len_squared_v3v3(v1, v2) > eps)) {
      const float rake_dist_sq = len_squared_v3v3(cache->rake_data.follow_co, grab_location);
      const float rake_fade = (rake_dist_sq > square_f(cache->rake_data.follow_dist)) ?
                                  1.0f :
                                  sqrtf(rake_dist_sq) / cache->rake_data.follow_dist;

      float axis[3], angle;
      float tquat[4];

      rotation_between_vecs_to_quat(tquat, v1, v2);

      /* Use axis-angle to scale rotation since the factor may be above 1. */
      quat_to_axis_angle(axis, &angle, tquat);
      normalize_v3(axis);

      angle *= brush->rake_factor * rake_fade;
      axis_angle_normalized_to_quat(cache->rake_rotation, axis, angle);
      cache->is_rake_rotation_valid = true;
    }
  }
  sculpt_rake_data_update(&cache->rake_data, grab_location);
}

static void sculpt_update_cache_paint_variants(StrokeCache *cache, const Brush *brush)
{
  cache->paint_brush.hardness = brush->hardness;
  if (brush->paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE) {
    cache->paint_brush.hardness *= brush->paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE_INVERT ?
                                       1.0f - cache->pressure :
                                       cache->pressure;
  }

  cache->paint_brush.flow = brush->flow;
  if (brush->paint_flags & BRUSH_PAINT_FLOW_PRESSURE) {
    cache->paint_brush.flow *= brush->paint_flags & BRUSH_PAINT_FLOW_PRESSURE_INVERT ?
                                   1.0f - cache->pressure :
                                   cache->pressure;
  }

  cache->paint_brush.wet_mix = brush->wet_mix;
  if (brush->paint_flags & BRUSH_PAINT_WET_MIX_PRESSURE) {
    cache->paint_brush.wet_mix *= brush->paint_flags & BRUSH_PAINT_WET_MIX_PRESSURE_INVERT ?
                                      1.0f - cache->pressure :
                                      cache->pressure;

    /* This makes wet mix more sensible in higher values, which allows to create brushes that have
     * a wider pressure range were they only blend colors without applying too much of the brush
     * color. */
    cache->paint_brush.wet_mix = 1.0f - pow2f(1.0f - cache->paint_brush.wet_mix);
  }

  cache->paint_brush.wet_persistence = brush->wet_persistence;
  if (brush->paint_flags & BRUSH_PAINT_WET_PERSISTENCE_PRESSURE) {
    cache->paint_brush.wet_persistence = brush->paint_flags &
                                                 BRUSH_PAINT_WET_PERSISTENCE_PRESSURE_INVERT ?
                                             1.0f - cache->pressure :
                                             cache->pressure;
  }

  cache->paint_brush.density = brush->density;
  if (brush->paint_flags & BRUSH_PAINT_DENSITY_PRESSURE) {
    cache->paint_brush.density = brush->paint_flags & BRUSH_PAINT_DENSITY_PRESSURE_INVERT ?
                                     1.0f - cache->pressure :
                                     cache->pressure;
  }
}

/* Initialize the stroke cache variants from operator properties. */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, Object *ob, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) ||
      !((brush->flag & BRUSH_ANCHORED) || (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK) ||
        (brush->sculpt_tool == SCULPT_TOOL_ROTATE) || SCULPT_is_cloth_deform_brush(brush)))
  {
    RNA_float_get_array(ptr, "location", cache->true_location);
  }

  cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");
  RNA_float_get_array(ptr, "mouse", cache->mouse);
  RNA_float_get_array(ptr, "mouse_event", cache->mouse_event);

  /* XXX: Use pressure value from first brush step for brushes which don't support strokes (grab,
   * thumb). They depends on initial state and brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle changing
   * events. We should avoid this after events system re-design. */
  if (paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT) || cache->first_time) {
    cache->pressure = RNA_float_get(ptr, "pressure");
  }

  cache->x_tilt = RNA_float_get(ptr, "x_tilt");
  cache->y_tilt = RNA_float_get(ptr, "y_tilt");

  /* Truly temporary data that isn't stored in properties. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    cache->initial_radius = SCULPT_calc_radius(cache->vc, brush, scene, cache->true_location);

    if (!BKE_brush_use_locked_size(scene, brush)) {
      BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius);
    }
  }

  /* Clay stabilized pressure. */
  if (brush->sculpt_tool == SCULPT_TOOL_CLAY_THUMB) {
    if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
      for (int i = 0; i < SCULPT_CLAY_STABILIZER_LEN; i++) {
        ss->cache->clay_pressure_stabilizer[i] = 0.0f;
      }
      ss->cache->clay_pressure_stabilizer_index = 0;
    }
    else {
      cache->clay_pressure_stabilizer[cache->clay_pressure_stabilizer_index] = cache->pressure;
      cache->clay_pressure_stabilizer_index += 1;
      if (cache->clay_pressure_stabilizer_index >= SCULPT_CLAY_STABILIZER_LEN) {
        cache->clay_pressure_stabilizer_index = 0;
      }
    }
  }

  if (BKE_brush_use_size_pressure(brush) && paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT))
  {
    cache->radius = sculpt_brush_dynamic_size_get(brush, cache, cache->initial_radius);
    cache->dyntopo_pixel_radius = sculpt_brush_dynamic_size_get(
        brush, cache, ups->initial_pixel_radius);
  }
  else {
    cache->radius = cache->initial_radius;
    cache->dyntopo_pixel_radius = ups->initial_pixel_radius;
  }

  sculpt_update_cache_paint_variants(cache, brush);

  cache->radius_squared = cache->radius * cache->radius;

  if (brush->flag & BRUSH_ANCHORED) {
    /* True location has been calculated as part of the stroke system already here. */
    if (brush->flag & BRUSH_EDGE_TO_EDGE) {
      RNA_float_get_array(ptr, "location", cache->true_location);
    }

    cache->radius = paint_calc_object_space_radius(
        cache->vc, cache->true_location, ups->pixel_radius);
    cache->radius_squared = cache->radius * cache->radius;

    copy_v3_v3(cache->anchored_location, cache->true_location);
  }

  sculpt_update_brush_delta(ups, ob, brush);

  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->vertex_rotation = -BLI_dial_angle(cache->dial, cache->mouse) * cache->bstrength;

    ups->draw_anchored = true;
    copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
    copy_v3_v3(cache->anchored_location, cache->true_location);
    ups->anchored_size = ups->pixel_radius;
  }

  cache->special_rotation = ups->brush_rotation;

  cache->iteration_count++;
}

/* Returns true if any of the smoothing modes are active (currently
 * one of smooth brush, autosmooth, mask smooth, or shift-key
 * smooth). */
static bool sculpt_needs_connectivity_info(const Sculpt *sd,
                                           const Brush *brush,
                                           SculptSession *ss,
                                           int stroke_mode)
{
  if (!brush) {
    return true;
  }

  if (ss && ss->pbvh && SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return true;
  }
  return ((stroke_mode == BRUSH_STROKE_SMOOTH) || (ss && ss->cache && ss->cache->alt_smooth) ||
          (brush->sculpt_tool == SCULPT_TOOL_SMOOTH) || (brush->autosmooth_factor > 0) ||
          ((brush->sculpt_tool == SCULPT_TOOL_MASK) && (brush->mask_tool == BRUSH_MASK_SMOOTH)) ||
          (brush->sculpt_tool == SCULPT_TOOL_POSE) ||
          (brush->sculpt_tool == SCULPT_TOOL_BOUNDARY) ||
          (brush->sculpt_tool == SCULPT_TOOL_SLIDE_RELAX) ||
          SCULPT_tool_is_paint(brush->sculpt_tool) || (brush->sculpt_tool == SCULPT_TOOL_CLOTH) ||
          (brush->sculpt_tool == SCULPT_TOOL_SMEAR) ||
          (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) ||
          (brush->sculpt_tool == SCULPT_TOOL_DISPLACEMENT_SMEAR) ||
          (brush->sculpt_tool == SCULPT_TOOL_PAINT));
}

void SCULPT_stroke_modifiers_check(const bContext *C, Object *ob, const Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  bool need_pmap = sculpt_needs_connectivity_info(sd, brush, ss, 0);
  if (ss->shapekey_active || ss->deform_modifiers_active ||
      (!BKE_sculptsession_use_pbvh_draw(ob, rv3d) && need_pmap))
  {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    BKE_sculpt_update_object_for_edit(
        depsgraph, ob, need_pmap, false, SCULPT_tool_is_paint(brush->sculpt_tool));
  }
}

static void sculpt_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) >= *tmin) {
    return;
  }
  SculptRaycastData *srd = static_cast<SculptRaycastData *>(data_v);
  float(*origco)[3] = nullptr;
  bool use_origco = false;

  if (srd->original && srd->ss->cache) {
    if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
      use_origco = true;
    }
    else {
      /* Intersect with coordinates from before we started stroke. */
      SculptUndoNode *unode = SCULPT_undo_get_node(node, SCULPT_UNDO_COORDS);
      origco = (unode) ? unode->co : nullptr;
      use_origco = origco ? true : false;
    }
  }

  float back_depth = 0.0f;
  int hit_count = 0;

  if (BKE_pbvh_node_raycast(srd->ss,
                            srd->ss->pbvh,
                            node,
                            origco,
                            use_origco,
                            srd->ray_start,
                            srd->ray_normal,
                            &srd->isect_precalc,
                            &hit_count,
                            &srd->depth,
                            &back_depth,
                            &srd->active_vertex,
                            &srd->active_face,
                            srd->face_normal,
                            srd->ss->stroke_id))
  {
    srd->hit = true;
    *tmin = srd->depth;
  }
}

static void sculpt_find_nearest_to_ray_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) >= *tmin) {
    return;
  }
  SculptFindNearestToRayData *srd = static_cast<SculptFindNearestToRayData *>(data_v);
  float(*origco)[3] = nullptr;
  bool use_origco = false;

  if (srd->original && srd->ss->cache) {
    if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
      use_origco = true;
    }
    else {
      /* Intersect with coordinates from before we started stroke. */
      SculptUndoNode *unode = SCULPT_undo_get_node(node, SCULPT_UNDO_COORDS);
      origco = (unode) ? unode->co : nullptr;
      use_origco = origco ? true : false;
    }
  }

  if (BKE_pbvh_node_find_nearest_to_ray(srd->ss,
                                        srd->ss->pbvh,
                                        node,
                                        origco,
                                        use_origco,
                                        srd->ray_start,
                                        srd->ray_normal,
                                        &srd->depth,
                                        &srd->dist_sq_to_ray,
                                        srd->ss->stroke_id))
  {
    srd->hit = true;
    *tmin = srd->dist_sq_to_ray;
  }
}

float SCULPT_raycast_init(ViewContext *vc,
                          const float mval[2],
                          float ray_start[3],
                          float ray_end[3],
                          float ray_normal[3],
                          bool original)
{
  float obimat[4][4];
  float dist;
  Object *ob = vc->obact;
  RegionView3D *rv3d = vc->rv3d;
  View3D *v3d = vc->v3d;

  /* TODO: what if the segment is totally clipped? (return == 0). */
  ED_view3d_win_to_segment_clipped(
      vc->depsgraph, vc->region, vc->v3d, mval, ray_start, ray_end, true);

  invert_m4_m4(obimat, ob->object_to_world);
  mul_m4_v3(obimat, ray_start);
  mul_m4_v3(obimat, ray_end);

  sub_v3_v3v3(ray_normal, ray_end, ray_start);
  dist = normalize_v3(ray_normal);

  if ((rv3d->is_persp == false) &&
      /* If the ray is clipped, don't adjust its start/end. */
      !RV3D_CLIPPING_ENABLED(v3d, rv3d))
  {
    BKE_pbvh_raycast_project_ray_root(ob->sculpt->pbvh, original, ray_start, ray_end, ray_normal);

    /* rRecalculate the normal. */
    sub_v3_v3v3(ray_normal, ray_end, ray_start);
    dist = normalize_v3(ray_normal);
  }

  return dist;
}

bool SCULPT_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mval[2],
                                        bool use_sampled_normal,
                                        bool /*use_back*/)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Sculpt *sd = scene->toolsettings->sculpt;
  Object *ob;
  SculptSession *ss;
  ViewContext vc;
  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3], sampled_normal[3],
      mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  bool original = false;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;
  ss = ob->sculpt;

  if (!ss->pbvh || !vc.rv3d) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* PBVH raycast to get active vertex and face normal. */
  depth = SCULPT_raycast_init(&vc, mval, ray_start, ray_end, ray_normal, original);
  SCULPT_stroke_modifiers_check(C, ob, brush);

  SculptRaycastData srd{};
  srd.original = original;
  srd.ss = ob->sculpt;
  srd.hit = false;
  srd.ray_start = ray_start;
  srd.ray_normal = ray_normal;
  srd.depth = depth;
  srd.face_normal = face_normal;
  srd.active_face.i = PBVH_REF_NONE;
  srd.active_vertex.i = PBVH_REF_NONE;

  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);
  BKE_pbvh_raycast(
      ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original, ss->stroke_id);

  /* Cursor is not over the mesh, return default values. */
  if (!srd.hit) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* Update the active vertex of the SculptSession. */
  ss->active_vertex = srd.active_vertex;
  copy_v3_v3(out->active_vertex_co, SCULPT_active_vertex_co_get(ss));

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      ss->active_face = srd.active_face;
      ss->active_grid_index = 0;
      break;
    case PBVH_GRIDS:
      ss->active_face = srd.active_face;
      ss->active_grid_index = ss->active_face.i;
      break;
    case PBVH_BMESH:
      ss->active_face = srd.active_face;
      ss->active_grid_index = 0;
      break;
  }

  copy_v3_v3(out->location, ray_normal);
  mul_v3_fl(out->location, srd.depth);
  add_v3_v3(out->location, ray_start);

  /* Option to return the face normal directly for performance o accuracy reasons. */
  if (!use_sampled_normal) {
    copy_v3_v3(out->normal, srd.face_normal);
    return srd.hit;
  }

  /* Sampled normal calculation. */
  float radius;

  /* Update cursor data in SculptSession. */
  invert_m4_m4(ob->world_to_object, ob->object_to_world);
  copy_m3_m4(mat, vc.rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->world_to_object);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(ss->cursor_view_normal, viewDir);
  copy_v3_v3(ss->cursor_normal, srd.face_normal);
  copy_v3_v3(ss->cursor_location, out->location);
  ss->rv3d = vc.rv3d;
  ss->v3d = vc.v3d;

  if (!BKE_brush_use_locked_size(scene, brush)) {
    radius = paint_calc_object_space_radius(&vc, out->location, BKE_brush_size_get(scene, brush));
  }
  else {
    radius = BKE_brush_unprojected_radius_get(scene, brush);
  }
  ss->cursor_radius = radius;

  Vector<PBVHNode *> nodes = sculpt_pbvh_gather_cursor_update(ob, sd, original);

  /* In case there are no nodes under the cursor, return the face normal. */
  if (nodes.is_empty()) {
    copy_v3_v3(out->normal, srd.face_normal);
    return true;
  }

  /* Calculate the sampled normal. */
  if (SCULPT_pbvh_calc_area_normal(brush, ob, nodes, true, sampled_normal)) {
    copy_v3_v3(out->normal, sampled_normal);
    copy_v3_v3(ss->cursor_sampled_normal, sampled_normal);
  }
  else {
    /* Use face normal when there are no vertices to sample inside the cursor radius. */
    copy_v3_v3(out->normal, srd.face_normal);
  }
  return true;
}

bool SCULPT_stroke_get_location(bContext *C,
                                float out[3],
                                const float mval[2],
                                bool force_original)
{
  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  bool check_closest = brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE;

  return SCULPT_stroke_get_location_ex(C, out, mval, force_original, check_closest, true);
}

bool SCULPT_stroke_get_location_ex(bContext *C,
                                   float out[3],
                                   const float mval[2],
                                   bool force_original,
                                   bool check_closest,
                                   bool limit_closest_radius)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob;
  SculptSession *ss;
  StrokeCache *cache;
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3];
  bool original;
  ViewContext vc;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;

  ss = ob->sculpt;
  cache = ss->cache;
  original = force_original || ((cache) ? !cache->accum : false);

  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));

  SCULPT_stroke_modifiers_check(C, ob, brush);

  depth = SCULPT_raycast_init(&vc, mval, ray_start, ray_end, ray_normal, original);

  bool hit = false;
  {
    SculptRaycastData srd;
    srd.ss = ob->sculpt;
    srd.ray_start = ray_start;
    srd.ray_normal = ray_normal;
    srd.hit = false;
    srd.depth = depth;
    srd.original = original;
    srd.face_normal = face_normal;
    srd.active_face.i = PBVH_REF_NONE;
    srd.active_vertex.i = PBVH_REF_NONE;
    isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

    BKE_pbvh_raycast(
        ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original, ss->stroke_id);
    if (srd.hit) {
      hit = true;
      copy_v3_v3(out, ray_normal);
      mul_v3_fl(out, srd.depth);
      add_v3_v3(out, ray_start);
    }
  }

  if (hit || !check_closest) {
    return hit;
  }

  SculptFindNearestToRayData srd{};
  srd.original = original;
  srd.ss = ob->sculpt;
  srd.hit = false;
  srd.ray_start = ray_start;
  srd.ray_normal = ray_normal;
  srd.depth = FLT_MAX;
  srd.dist_sq_to_ray = FLT_MAX;

  BKE_pbvh_find_nearest_to_ray(
      ss->pbvh, sculpt_find_nearest_to_ray_cb, &srd, ray_start, ray_normal, srd.original);
  if (srd.hit && srd.dist_sq_to_ray) {
    hit = true;
    copy_v3_v3(out, ray_normal);
    mul_v3_fl(out, srd.depth);
    add_v3_v3(out, ray_start);
  }

  float closest_radius_sq = FLT_MAX;
  if (limit_closest_radius) {
    closest_radius_sq = SCULPT_calc_radius(&vc, brush, CTX_data_scene(C), out);
    closest_radius_sq *= closest_radius_sq;
  }

  return hit && srd.dist_sq_to_ray < closest_radius_sq;
}

static void sculpt_brush_init_tex(Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  const MTex *mask_tex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);

  /* Init mtex nodes. */
  if (mask_tex->tex && mask_tex->tex->nodetree) {
    /* Has internal flag to detect it only does it once. */
    ntreeTexBeginExecTree(mask_tex->tex->nodetree);
  }

  if (ss->tex_pool == nullptr) {
    ss->tex_pool = BKE_image_pool_new();
  }
}

static void sculpt_brush_stroke_init(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);
  Sculpt *sd = tool_settings->sculpt;
  SculptSession *ss = CTX_data_active_object(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  int mode = RNA_enum_get(op->ptr, "mode");
  bool need_pmap, needs_colors;
  bool need_mask = false;

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    need_mask = true;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH ||
      brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM)
  {
    need_mask = true;
  }

  view3d_operator_needs_opengl(C);
  sculpt_brush_init_tex(sd, ss);

  need_pmap = sculpt_needs_connectivity_info(sd, brush, ss, mode);
  needs_colors = SCULPT_tool_is_paint(brush->sculpt_tool) &&
                 !SCULPT_use_image_paint_brush(&tool_settings->paint_mode, ob);

  if (needs_colors) {
    BKE_sculpt_color_layer_create_if_needed(ob);
  }

  /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
   * earlier steps modifying the data. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(
      depsgraph, ob, need_pmap, need_mask, SCULPT_tool_is_paint(brush->sculpt_tool));

  ED_paint_tool_update_sticky_shading_color(C, ob);
}

static void sculpt_restore_mesh(Sculpt *sd, Object *ob)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* For the cloth brush it makes more sense to not restore the mesh state to keep running the
   * simulation from the previous state. */
  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    return;
  }

  /* Restore the mesh before continuing with anchored stroke. */
  if (is_realtime_restored(brush)) {
    paint_mesh_restore_co(sd, ob);
  }
}

void SCULPT_update_object_bounding_box(Object *ob)
{
  if (ob->runtime.bb) {
    float bb_min[3], bb_max[3];

    BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
    BKE_boundbox_init_from_minmax(ob->runtime.bb, bb_min, bb_max);
  }
}

void SCULPT_flush_update_step(bContext *C, SculptUpdateType update_flags)
{
  using namespace blender;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  MultiresModifierData *mmd = ss->multires.modifier;
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  if (rv3d) {
    /* Mark for faster 3D viewport redraws. */
    rv3d->rflag |= RV3D_PAINTING;
  }

  if (mmd != nullptr) {
    multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
  }

  if ((update_flags & SCULPT_UPDATE_IMAGE) != 0) {
    ED_region_tag_redraw(region);
    if (update_flags == SCULPT_UPDATE_IMAGE) {
      /* Early exit when only need to update the images. We don't want to tag any geometry updates
       * that would rebuilt the PBVH. */
      return;
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  /* Only current viewport matters, slower update for all viewports will
   * be done in sculpt_flush_update_done. */
  if (!BKE_sculptsession_use_pbvh_draw(ob, rv3d)) {
    /* Slow update with full dependency graph update and all that comes with it.
     * Needed when there are modifiers or full shading in the 3D viewport. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);
  }
  else {
    /* Fast path where we just update the BVH nodes that changed, and redraw
     * only the part of the 3D viewport where changes happened. */
    rcti r;

    if (update_flags & SCULPT_UPDATE_COORDS) {
      BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB);
      /* Update the object's bounding box too so that the object
       * doesn't get incorrectly clipped during drawing in
       * draw_mesh_object(). #33790. */
      SCULPT_update_object_bounding_box(ob);
    }

    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d && SCULPT_get_redraw_rect(region, rv3d, ob, &r)) {
      if (ss->cache) {
        ss->cache->current_r = r;
      }

      /* previous is not set in the current cache else
       * the partial rect will always grow */
      sculpt_extend_redraw_rect_previous(ob, &r);

      r.xmin += region->winrct.xmin - 2;
      r.xmax += region->winrct.xmin + 2;
      r.ymin += region->winrct.ymin - 2;
      r.ymax += region->winrct.ymin + 2;
      ED_region_tag_redraw_partial(region, &r, true);
    }
  }

  if (update_flags & SCULPT_UPDATE_COORDS && !ss->shapekey_active) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
      /* When sculpting and changing the positions of a mesh, tag them as changed and update. */
      BKE_mesh_tag_positions_changed(mesh);
      /* Update the mesh's bounds eagerly since the PBVH already has that information. */
      Bounds<float3> bounds;
      BKE_pbvh_bounding_box(ob->sculpt->pbvh, bounds.min, bounds.max);
      mesh->bounds_set_eager(bounds);
    }
  }
}

void SCULPT_flush_update_done(const bContext *C, Object *ob, SculptUpdateType update_flags)
{
  /* After we are done drawing the stroke, check if we need to do a more
   * expensive depsgraph tag to update geometry. */
  wmWindowManager *wm = CTX_wm_manager(C);
  RegionView3D *current_rv3d = CTX_wm_region_view3d(C);
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  /* Always needed for linked duplicates. */
  bool need_tag = (ID_REAL_USERS(&mesh->id) > 1);

  if (current_rv3d) {
    current_rv3d->rflag &= ~RV3D_PAINTING;
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype != SPACE_VIEW3D) {
        continue;
      }

      /* Tag all 3D viewports for redraw now that we are done. Others
       * viewports did not get a full redraw, and anti-aliasing for the
       * current viewport was deactivated. */
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
          if (rv3d != current_rv3d) {
            need_tag |= !BKE_sculptsession_use_pbvh_draw(ob, rv3d);
          }

          ED_region_tag_redraw(region);
        }
      }
    }

    if (update_flags & SCULPT_UPDATE_IMAGE) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        if (sl->spacetype != SPACE_IMAGE) {
          continue;
        }
        ED_area_tag_redraw_regiontype(area, RGN_TYPE_WINDOW);
      }
    }
  }

  if (update_flags & SCULPT_UPDATE_COORDS) {
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateOriginalBB);

    /* Coordinates were modified, so fake neighbors are not longer valid. */
    SCULPT_fake_neighbors_free(ob);
  }

  if (update_flags & SCULPT_UPDATE_MASK) {
    BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  }

  if (update_flags & SCULPT_UPDATE_COLOR) {
    BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateColor);
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    blender::bke::dyntopo::after_stroke(ss->pbvh, false);
  }

  BKE_sculpt_attributes_destroy_temporary_stroke(ob);

  if (update_flags & SCULPT_UPDATE_COORDS) {
    /* Optimization: if there is locked key and active modifiers present in */
    /* the stack, keyblock is updating at each step. otherwise we could update */
    /* keyblock only when stroke is finished. */
    if (ss->shapekey_active && !ss->deform_modifiers_active) {
      sculpt_update_keyblock(ob);
    }
  }

  if (need_tag) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

/* Returns whether the mouse/stylus is over the mesh (1)
 * or over the background (0). */
static bool over_mesh(bContext *C, wmOperator * /*op*/, const float mval[2])
{
  float co_dummy[3];
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  bool check_closest = brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE;

  return SCULPT_stroke_get_location_ex(C, co_dummy, mval, false, check_closest, true);
}

static void sculpt_stroke_undo_begin(const bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);

  /* Setup the correct undo system. Image painting and sculpting are mutual exclusive.
   * Color attributes are part of the sculpting undo system. */
  if (brush && brush->sculpt_tool == SCULPT_TOOL_PAINT &&
      SCULPT_use_image_paint_brush(&tool_settings->paint_mode, ob))
  {
    ED_image_undo_push_begin(op->type->name, PAINT_MODE_SCULPT);
  }
  else {
    SCULPT_undo_push_begin_ex(ob, sculpt_tool_name(sd));
  }
}

static void sculpt_stroke_undo_end(const bContext *C, Brush *brush)
{
  Object *ob = CTX_data_active_object(C);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);

  if (brush && brush->sculpt_tool == SCULPT_TOOL_PAINT &&
      SCULPT_use_image_paint_brush(&tool_settings->paint_mode, ob))
  {
    ED_image_undo_push_end();
  }
  else {
    SCULPT_undo_push_end(ob);
  }
}

bool SCULPT_handles_colors_report(SculptSession *ss, ReportList *reports)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return true;
    case PBVH_BMESH:
      return true;
    case PBVH_GRIDS:
      BKE_report(reports, RPT_ERROR, "Not supported in multiresolution mode");
      return false;
  }

  BLI_assert_msg(0, "PBVH corruption, type was invalid.");

  return false;
}

static bool sculpt_stroke_test_start(bContext *C, wmOperator *op, const float mval[2])
{
  /* Don't start the stroke until `mval` goes over the mesh.
   * NOTE: `mval` will only be null when re-executing the saved stroke.
   * We have exception for 'exec' strokes since they may not set `mval`,
   * only 'location', see: #52195. */
  if (((op->flag & OP_IS_INVOKE) == 0) || (mval == nullptr) || over_mesh(C, op, mval)) {
    Object *ob = CTX_data_active_object(C);
    SculptSession *ss = ob->sculpt;
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
    Brush *brush = BKE_paint_brush(&sd->paint);
    ToolSettings *tool_settings = CTX_data_tool_settings(C);

    /* NOTE: This should be removed when paint mode is available. Paint mode can force based on the
     * canvas it is painting on. (ref. use_sculpt_texture_paint). */
    if (brush && SCULPT_tool_is_paint(brush->sculpt_tool) &&
        !SCULPT_use_image_paint_brush(&tool_settings->paint_mode, ob))
    {
      View3D *v3d = CTX_wm_view3d(C);
      if (v3d->shading.type == OB_SOLID) {
        v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
      }
    }

    ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

    sculpt_update_cache_invariants(C, sd, ss, op, mval);

    SCULPT_stroke_id_next(ob);
    ss->cache->stroke_id = ss->stroke_id;

    SculptCursorGeometryInfo sgi;
    SCULPT_cursor_geometry_info_update(C, &sgi, mval, false, false);

    sculpt_stroke_undo_begin(C, op);

    return true;
  }
  return false;
}

static void sculpt_stroke_update_step(bContext *C,
                                      wmOperator * /*op*/,
                                      PaintStroke *stroke,
                                      PointerRNA *itemptr)
{
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);
  StrokeCache *cache = ss->cache;

  float stroke_distance = paint_stroke_distance_get(stroke);
  float stroke_delta = stroke_distance - cache->stroke_distance;
  cache->stroke_distance = stroke_distance;

  SCULPT_stroke_modifiers_check(C, ob, brush);
  sculpt_update_cache_variants(C, sd, ob, itemptr);
  sculpt_restore_mesh(sd, ob);

  if (brush->flag & BRUSH_SCENE_SPACING) {
    stroke_delta /= ss->cache->radius;
  }
  else {
    stroke_delta /= ups->pixel_radius;
  }
  cache->stroke_distance_t += stroke_delta;

  if (ELEM(ss->cached_dyntopo.mode, DYNTOPO_DETAIL_CONSTANT, DYNTOPO_DETAIL_MANUAL)) {
    float object_space_constant_detail = 1.0f / (ss->cached_dyntopo.constant_detail *
                                                 mat4_to_scale(ob->object_to_world));
    blender::bke::dyntopo::detail_size_set(
        ss->pbvh, object_space_constant_detail, ss->cached_dyntopo.detail_range);
  }
  else if (ss->cached_dyntopo.mode == DYNTOPO_DETAIL_BRUSH) {
    blender::bke::dyntopo::detail_size_set(ss->pbvh,
                                           ss->cache->radius * ss->cached_dyntopo.detail_percent /
                                               100.0f,
                                           ss->cached_dyntopo.detail_range);
  }
  else { /* Relative mode. */
    blender::bke::dyntopo::detail_size_set(ss->pbvh,
                                           (ss->cache->radius / ss->cache->dyntopo_pixel_radius) *
                                               (ss->cached_dyntopo.detail_size * U.pixelsize),
                                           ss->cached_dyntopo.detail_range);
  }

  float dyntopo_spacing = float(ss->cached_dyntopo.spacing) / 50.0f;

  bool do_dyntopo = SCULPT_stroke_is_dynamic_topology(ss, brush);

  if (dyntopo_spacing > 0.0f) {
    do_dyntopo = do_dyntopo &&
                 (ss->cache->stroke_distance_t - ss->cache->last_dyntopo_t) > dyntopo_spacing;
  }

  if (do_dyntopo) {
    ss->cache->last_dyntopo_t = ss->cache->stroke_distance_t;

    /* Note: dyntopo repeats happen after the dab. */
    do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups, &tool_settings->paint_mode);
  }

  do_symmetrical_brush_actions(sd, ob, do_brush_action, ups, &tool_settings->paint_mode);
  sculpt_combine_proxies(sd, ob);

  if (do_dyntopo && ss->cached_dyntopo.repeat) {
    float3 location = ss->cache->true_location;

    add_v3_v3(cache->true_location, cache->grab_delta);

    for (int i = 0; i < ss->cached_dyntopo.repeat; i++) {
      do_symmetrical_brush_actions(
          sd, ob, sculpt_topology_update, ups, &tool_settings->paint_mode);
    }

    copy_v3_v3(ss->cache->true_location, location);
  }

  /* Hack to fix noise texture tearing mesh. */
  sculpt_fix_noise_tear(sd, ob);

  /* TODO(sergey): This is not really needed for the solid shading,
   * which does use pBVH drawing anyway, but texture and wireframe
   * requires this.
   *
   * Could be optimized later, but currently don't think it's so
   * much common scenario.
   *
   * Same applies to the DEG_id_tag_update() invoked from
   * sculpt_flush_update_step().
   */
  if (ss->deform_modifiers_active) {
    SCULPT_flush_stroke_deform(sd, ob, sculpt_tool_is_proxy_used(brush->sculpt_tool));
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }

  ss->cache->first_time = false;
  copy_v3_v3(ss->cache->true_last_location, ss->cache->true_location);

  /* Cleanup. */
  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
  }
  else if (SCULPT_tool_is_paint(brush->sculpt_tool)) {
    if (SCULPT_use_image_paint_brush(&tool_settings->paint_mode, ob)) {
      SCULPT_flush_update_step(C, SCULPT_UPDATE_IMAGE);
    }
    else {
      SCULPT_flush_update_step(C, SCULPT_UPDATE_COLOR);
    }
  }
  else {
    SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  }
}

static void sculpt_brush_exit_tex(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  const MTex *mask_tex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);

  if (mask_tex->tex && mask_tex->tex->nodetree) {
    ntreeTexEndExecTree(mask_tex->tex->nodetree->runtime->execdata);
  }
}

static void sculpt_stroke_done(const bContext *C, PaintStroke * /*stroke*/)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  ToolSettings *tool_settings = CTX_data_tool_settings(C);

  /* Finished. */
  if (!ss->cache) {
    sculpt_brush_exit_tex(sd);
    return;
  }
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&sd->paint);
  BLI_assert(brush == ss->cache->brush); /* const, so we shouldn't change. */
  ups->draw_inverted = false;

  SCULPT_stroke_modifiers_check(C, ob, brush);

  /* Alt-Smooth. */
  if (ss->cache->alt_smooth) {
    smooth_brush_toggle_off(C, &sd->paint, ss->cache);
    /* Refresh the brush pointer in case we switched brush in the toggle function. */
    brush = BKE_paint_brush(&sd->paint);
  }

  if (SCULPT_is_automasking_enabled(sd, ss, brush)) {
    SCULPT_automasking_cache_free(ss, ob, ss->cache->automasking);
  }

  SCULPT_cache_free(ss, ob, ss->cache);
  ss->cache = nullptr;

  sculpt_stroke_undo_end(C, brush);

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  }
  else if (brush->sculpt_tool == SCULPT_TOOL_PAINT) {
    if (SCULPT_use_image_paint_brush(&tool_settings->paint_mode, ob)) {
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_IMAGE);
    }
    else {
      BKE_sculpt_attributes_destroy_temporary_stroke(ob);
      SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COLOR);
    }
  }
  else {
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  sculpt_brush_exit_tex(sd);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PaintStroke *stroke;
  int ignore_background_click;
  int retval;
  Object *ob = CTX_data_active_object(C);

  /* Test that ob is visible; otherwise we won't be able to get evaluated data
   * from the depsgraph. We do this here instead of SCULPT_mode_poll
   * to avoid falling through to the translate operator in the
   * global view3d keymap.
   *
   * NOTE: #BKE_object_is_visible_in_viewport is not working here (it returns false
   * if the object is in local view); instead, test for OB_HIDE_VIEWPORT directly.
   */

  if (ob->visibility_flag & OB_HIDE_VIEWPORT) {
    return OPERATOR_CANCELLED;
  }

  sculpt_brush_stroke_init(C, op);

  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;

  if (SCULPT_tool_is_paint(brush->sculpt_tool) &&
      !SCULPT_handles_colors_report(ob->sculpt, op->reports))
  {
    return OPERATOR_CANCELLED;
  }
  if (SCULPT_tool_is_mask(brush->sculpt_tool)) {
    MultiresModifierData *mmd = BKE_sculpt_multires_active(ss->scene, ob);
    BKE_sculpt_mask_layers_ensure(CTX_data_depsgraph_pointer(C), CTX_data_main(C), ob, mmd);
  }
  if (SCULPT_tool_is_face_sets(brush->sculpt_tool)) {
    ss->face_sets = BKE_sculpt_face_sets_ensure(ob);
  }

  stroke = paint_stroke_new(C,
                            op,
                            SCULPT_stroke_get_location,
                            sculpt_stroke_test_start,
                            sculpt_stroke_update_step,
                            nullptr,
                            sculpt_stroke_done,
                            event->type);

  op->customdata = stroke;

  /* For tablet rotation. */
  ignore_background_click = RNA_boolean_get(op->ptr, "ignore_background_click");
  const float mval[2] = {float(event->mval[0]), float(event->mval[1])};
  if (ignore_background_click && !over_mesh(C, op, mval)) {
    paint_stroke_free(C, op, static_cast<PaintStroke *>(op->customdata));
    return OPERATOR_PASS_THROUGH;
  }

  retval = op->type->modal(C, op, event);
  if (ELEM(retval, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    paint_stroke_free(C, op, static_cast<PaintStroke *>(op->customdata));
    return retval;
  }
  /* Add modal handler. */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
  sculpt_brush_stroke_init(C, op);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    sculpt_stroke_test_start,
                                    sculpt_stroke_update_step,
                                    nullptr,
                                    sculpt_stroke_done,
                                    0);

  /* Frees op->customdata. */
  paint_stroke_exec(C, op, static_cast<PaintStroke *>(op->customdata));

  return OPERATOR_FINISHED;
}

static void sculpt_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);

  /* XXX Canceling strokes that way does not work with dynamic topology,
   *     user will have to do real undo for now. See #46456. */
  if (ss->cache && !SCULPT_stroke_is_dynamic_topology(ss, brush)) {
    paint_mesh_restore_co(sd, ob);
  }

  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));

  if (ss->cache) {
    SCULPT_cache_free(ss, ob, ss->cache);
    ss->cache = nullptr;
  }

  sculpt_brush_exit_tex(sd);
}

static int sculpt_brush_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, (PaintStroke **)&op->customdata);
}

static void sculpt_redo_empty_ui(bContext * /*C*/, wmOperator * /*op*/) {}

void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sculpt";
  ot->idname = "SCULPT_OT_brush_stroke";
  ot->description = "Sculpt a stroke into the geometry";

  /* API callbacks. */
  ot->invoke = sculpt_brush_stroke_invoke;
  ot->modal = sculpt_brush_stroke_modal;
  ot->exec = sculpt_brush_stroke_exec;
  ot->poll = SCULPT_poll;
  ot->cancel = sculpt_brush_stroke_cancel;
  ot->ui = sculpt_redo_empty_ui;

  /* Flags (sculpt does own undo? (ton)). */
  ot->flag = OPTYPE_BLOCKING;

  /* Properties. */

  paint_stroke_operator_properties(ot, true);

  RNA_def_boolean(ot->srna,
                  "ignore_background_click",
                  0,
                  "Ignore Background Click",
                  "Clicks on the background do not start the stroke");
}

/* Fake Neighbors. */
/* This allows the sculpt tools to work on meshes with multiple connected components as they had
 * only one connected component. When initialized and enabled, the sculpt API will return extra
 * connectivity neighbors that are not in the real mesh. These neighbors are calculated for each
 * vertex using the minimum distance to a vertex that is in a different connected component. */

/* The fake neighbors first need to be ensured to be initialized.
 * After that tools which needs fake neighbors functionality need to
 * temporarily enable it:
 *
 *   void my_awesome_sculpt_tool() {
 *     SCULPT_fake_neighbors_ensure(sd, object, brush->disconnected_distance_max);
 *     SCULPT_fake_neighbors_enable(ob);
 *
 *     ... Logic of the tool ...
 *     SCULPT_fake_neighbors_disable(ob);
 *   }
 *
 * Such approach allows to keep all the connectivity information ready for reuse
 * (without having lag prior to every stroke), but also makes it so the affect
 * is localized to a specific brushes and tools only. */

enum {
  SCULPT_TOPOLOGY_ID_NONE,
  SCULPT_TOPOLOGY_ID_DEFAULT,
};

static void SCULPT_fake_neighbor_init(SculptSession *ss, const float max_dist)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  ss->fake_neighbors.fake_neighbor_index = static_cast<PBVHVertRef *>(
      MEM_malloc_arrayN(totvert, sizeof(int), "fake neighbor"));
  for (int i = 0; i < totvert; i++) {
    ss->fake_neighbors.fake_neighbor_index[i].i = FAKE_NEIGHBOR_NONE;
  }

  ss->fake_neighbors.current_max_distance = max_dist;
}

static void SCULPT_fake_neighbor_add(SculptSession *ss, PBVHVertRef v_a, PBVHVertRef v_b)
{
  int v_index_a = BKE_pbvh_vertex_to_index(ss->pbvh, v_a);
  int v_index_b = BKE_pbvh_vertex_to_index(ss->pbvh, v_b);

  if (ss->fake_neighbors.fake_neighbor_index[v_index_a].i == FAKE_NEIGHBOR_NONE) {
    ss->fake_neighbors.fake_neighbor_index[v_index_a].i = v_index_b;
    ss->fake_neighbors.fake_neighbor_index[v_index_b].i = v_index_a;
  }
}

static void sculpt_pose_fake_neighbors_free(SculptSession *ss)
{
  MEM_SAFE_FREE(ss->fake_neighbors.fake_neighbor_index);
}

struct NearestVertexFakeNeighborTLSData {
  PBVHVertRef nearest_vertex;
  float nearest_vertex_distance_squared;
  int current_topology_id;
};

static void do_fake_neighbor_search_task_cb(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;
  NearestVertexFakeNeighborTLSData *nvtd = static_cast<NearestVertexFakeNeighborTLSData *>(
      tls->userdata_chunk);
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    int vd_topology_id = SCULPT_vertex_island_get(ss, vd.vertex);
    if (vd_topology_id != nvtd->current_topology_id &&
        ss->fake_neighbors.fake_neighbor_index[vd.index].i == FAKE_NEIGHBOR_NONE)
    {
      float distance_squared = len_squared_v3v3(vd.co, data->nearest_vertex_search_co);
      if (distance_squared < nvtd->nearest_vertex_distance_squared &&
          distance_squared < data->max_distance_squared)
      {
        nvtd->nearest_vertex = vd.vertex;
        nvtd->nearest_vertex_distance_squared = distance_squared;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void fake_neighbor_search_reduce(const void *__restrict /*userdata*/,
                                        void *__restrict chunk_join,
                                        void *__restrict chunk)
{
  NearestVertexFakeNeighborTLSData *join = static_cast<NearestVertexFakeNeighborTLSData *>(
      chunk_join);
  NearestVertexFakeNeighborTLSData *nvtd = static_cast<NearestVertexFakeNeighborTLSData *>(chunk);
  if (join->nearest_vertex.i == PBVH_REF_NONE) {
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
  else if (nvtd->nearest_vertex_distance_squared < join->nearest_vertex_distance_squared) {
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
}

static PBVHVertRef SCULPT_fake_neighbor_search(Sculpt *sd,
                                               Object *ob,
                                               const PBVHVertRef vertex,
                                               float max_distance)
{
  SculptSession *ss = ob->sculpt;
  Vector<PBVHNode *> nodes;

  SculptSearchSphereData data{};
  data.ss = ss;
  data.sd = sd;
  data.radius_squared = max_distance * max_distance;
  data.original = false;
  data.center = SCULPT_vertex_co_get(ss, vertex);

  nodes = blender::bke::pbvh::search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data);

  if (nodes.is_empty()) {
    return BKE_pbvh_make_vref(PBVH_REF_NONE);
  }

  SculptThreadedTaskData task_data{};
  task_data.sd = sd;
  task_data.ob = ob;
  task_data.nodes = nodes;
  task_data.max_distance_squared = max_distance * max_distance;

  copy_v3_v3(task_data.nearest_vertex_search_co, SCULPT_vertex_co_get(ss, vertex));

  NearestVertexFakeNeighborTLSData nvtd;
  nvtd.nearest_vertex.i = -1;
  nvtd.nearest_vertex_distance_squared = FLT_MAX;
  nvtd.current_topology_id = SCULPT_vertex_island_get(ss, vertex);

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  settings.func_reduce = fake_neighbor_search_reduce;
  settings.userdata_chunk = &nvtd;
  settings.userdata_chunk_size = sizeof(NearestVertexFakeNeighborTLSData);
  BLI_task_parallel_range(0, nodes.size(), &task_data, do_fake_neighbor_search_task_cb, &settings);

  return nvtd.nearest_vertex;
}

struct SculptTopologyIDFloodFillData {
  int next_id;
};

void SCULPT_boundary_info_ensure(Object *object)
{
  using namespace blender;
  SculptSession *ss = object->sculpt;

  /* PBVH_BMESH now handles boundaries itself. */
  if (ss->bm || ss->vertex_info.boundary) {
    return;
  }

  Mesh *base_mesh = BKE_mesh_from_object(object);
  const blender::Span<int2> edges = base_mesh->edges();
  const OffsetIndices polys = base_mesh->polys();
  const Span<int> corner_edges = base_mesh->corner_edges();

  ss->vertex_info.boundary = BLI_BITMAP_NEW(base_mesh->totvert, "Boundary info");
  int *adjacent_faces_edge_count = static_cast<int *>(
      MEM_calloc_arrayN(base_mesh->totedge, sizeof(int), "Adjacent face edge count"));

  for (const int i : polys.index_range()) {
    for (const int edge : corner_edges.slice(polys[i])) {
      adjacent_faces_edge_count[edge]++;
    }
  }

  for (const int e : edges.index_range()) {
    if (adjacent_faces_edge_count[e] < 2) {
      const int2 &edge = edges[e];
      BLI_BITMAP_SET(ss->vertex_info.boundary, edge[0], true);
      BLI_BITMAP_SET(ss->vertex_info.boundary, edge[1], true);
    }
  }

  MEM_freeN(adjacent_faces_edge_count);
}

void SCULPT_ensure_vemap(SculptSession *ss)
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH && ss->vemap.is_empty()) {
    ss->vemap = blender::bke::mesh::build_vert_to_edge_map(
        ss->edges, ss->totvert, ss->vert_to_edge_offsets, ss->vert_to_edge_indices);
  }
}

void SCULPT_ensure_epmap(SculptSession *ss)
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH && ss->epmap.is_empty()) {
    ss->epmap = blender::bke::mesh::build_edge_to_poly_map(ss->polys,
                                                           ss->corner_edges,
                                                           ss->totedges,
                                                           ss->edge_to_poly_offsets,
                                                           ss->edge_to_poly_indices);
  }
}

void SCULPT_fake_neighbors_ensure(Sculpt *sd, Object *ob, const float max_dist)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  /* Fake neighbors were already initialized with the same distance, so no need to be
   * recalculated.
   */
  if (ss->fake_neighbors.fake_neighbor_index &&
      ss->fake_neighbors.current_max_distance == max_dist) {
    return;
  }

  SCULPT_topology_islands_ensure(ob);
  SCULPT_fake_neighbor_init(ss, max_dist);

  for (int i = 0; i < totvert; i++) {
    const PBVHVertRef from_v = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    /* This vertex does not have a fake neighbor yet, search one for it. */
    if (ss->fake_neighbors.fake_neighbor_index[i].i == FAKE_NEIGHBOR_NONE) {
      const PBVHVertRef to_v = SCULPT_fake_neighbor_search(sd, ob, from_v, max_dist);
      if (to_v.i != PBVH_REF_NONE) {
        /* Add the fake neighbor if available. */
        SCULPT_fake_neighbor_add(ss, from_v, to_v);
      }
    }
  }
}

void SCULPT_fake_neighbors_enable(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(ss->fake_neighbors.fake_neighbor_index != nullptr);
  ss->fake_neighbors.use_fake_neighbors = true;
}

void SCULPT_fake_neighbors_disable(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(ss->fake_neighbors.fake_neighbor_index != nullptr);
  ss->fake_neighbors.use_fake_neighbors = false;
}

void SCULPT_fake_neighbors_free(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  sculpt_pose_fake_neighbors_free(ss);
}

void SCULPT_automasking_node_begin(Object *ob,
                                   const SculptSession * /*ss*/,
                                   AutomaskingCache *automasking,
                                   AutomaskingNodeData *automask_data,
                                   PBVHNode *node)
{
  if (!automasking) {
    memset(automask_data, 0, sizeof(*automask_data));
    return;
  }

  automask_data->node = node;
  automask_data->have_orig_data = automasking->settings.flags &
                                  (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL);

  if (automask_data->have_orig_data) {
    SCULPT_orig_vert_data_init(&automask_data->orig_data, ob, node, SCULPT_UNDO_COORDS);
  }
  else {
    memset(&automask_data->orig_data, 0, sizeof(automask_data->orig_data));
  }
}

void SCULPT_automasking_node_update(SculptSession *ss,
                                    AutomaskingNodeData *automask_data,
                                    PBVHVertexIter *vd)
{
  if (automask_data->have_orig_data) {
    SCULPT_orig_vert_data_update(ss, &automask_data->orig_data, vd->vertex);
  }
}

bool SCULPT_vertex_is_occluded(SculptSession *ss, PBVHVertRef vertex, bool original)
{
  float ray_start[3], ray_end[3], ray_normal[3], face_normal[3];
  float co[3];

  copy_v3_v3(co, SCULPT_vertex_co_get(ss, vertex));
  float mouse[2];

  ViewContext *vc = ss->cache ? ss->cache->vc : &ss->filter_cache->vc;

  ED_view3d_project_float_v2_m4(
      vc->region, co, mouse, ss->cache ? ss->cache->projection_mat : ss->filter_cache->viewmat);

  int depth = SCULPT_raycast_init(vc, mouse, ray_end, ray_start, ray_normal, original);

  negate_v3(ray_normal);

  copy_v3_v3(ray_start, SCULPT_vertex_co_get(ss, vertex));
  madd_v3_v3fl(ray_start, ray_normal, 0.002);

  SculptRaycastData srd = {0};
  srd.original = original;
  srd.ss = ss;
  srd.hit = false;
  srd.ray_start = ray_start;
  srd.ray_normal = ray_normal;
  srd.depth = depth;
  srd.face_normal = face_normal;
  srd.active_face.i = PBVH_REF_NONE;
  srd.active_vertex.i = PBVH_REF_NONE;

  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);
  BKE_pbvh_raycast(
      ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original, ss->stroke_id);

  return srd.hit;
}

void SCULPT_stroke_id_next(Object *ob)
{
  ushort *id = reinterpret_cast<ushort *>(&ob->sculpt->stroke_id);

  /* Try to avoid offending undefined behavior sanitizers. */
  id[0] = ushort((int(id[0]) + 1) % 65535);
  id[1] = 0;
}

int SCULPT_face_set_get(const SculptSession *ss, PBVHFaceRef face)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = reinterpret_cast<BMFace *>(face.i);
      return BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
    }
    case PBVH_FACES:
    case PBVH_GRIDS:
      return ss->face_sets[face.i];
  }

  BLI_assert_unreachable();

  return 0;
}

void SCULPT_face_set_set(SculptSession *ss, PBVHFaceRef face, int fset)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = reinterpret_cast<BMFace *>(face.i);
      BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      break;
    }
    case PBVH_FACES:
    case PBVH_GRIDS:
      ss->face_sets[face.i] = fset;
  }
}

int SCULPT_vertex_island_get(SculptSession *ss, PBVHVertRef vertex)
{
  if (ss->attrs.topology_island_key) {
    return vertex_attr_get<uint8_t>(vertex, ss->attrs.topology_island_key);
  }

  return -1;
}

void SCULPT_topology_islands_invalidate(SculptSession *ss)
{
  ss->islands_valid = false;
}

void SCULPT_topology_islands_ensure(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (ss->attrs.topology_island_key && ss->islands_valid && BKE_pbvh_type(ss->pbvh) != PBVH_BMESH)
  {
    return;
  }

  SculptAttributeParams params;
  params.permanent = params.stroke_only = params.simple_array = false;

  ss->attrs.topology_island_key = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_POINT, CD_PROP_INT8, SCULPT_ATTRIBUTE_NAME(topology_island_key), &params);
  SCULPT_vertex_random_access_ensure(ss);

  int totvert = SCULPT_vertex_count_get(ss);
  Set<PBVHVertRef> visit;
  Vector<PBVHVertRef> stack;
  uint8_t island_nr = 0;

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    if (visit.contains(vertex)) {
      continue;
    }

    stack.clear();
    stack.append(vertex);
    visit.add(vertex);

    while (stack.size()) {
      PBVHVertRef vertex2 = stack.pop_last();
      SculptVertexNeighborIter ni;

      vertex_attr_set<uint8_t>(vertex2, ss->attrs.topology_island_key, island_nr);

      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex2, ni) {
        if (visit.add(ni.vertex) && SCULPT_vertex_any_face_visible_get(ss, ni.vertex)) {
          stack.append(ni.vertex);
        }
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
    }

    island_nr++;
  }

  ss->islands_valid = true;
}

void SCULPT_cube_tip_init(Sculpt * /*sd*/, Object *ob, Brush *brush, float mat[4][4])
{
  SculptSession *ss = ob->sculpt;
  float scale[4][4];
  float tmat[4][4];
  float unused[4][4];

  zero_m4(mat);
  calc_brush_local_mat(0.0, ob, unused, mat);

  /* Note: we ignore the radius scaling done inside of calc_brush_local_mat to
   * duplicate prior behavior.
   *
   * TODO: try disabling this and check that all edge cases work properly.
   */
  normalize_m4(mat);

  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  mul_v3_fl(tmat[1], brush->tip_scale_x);
  invert_m4_m4(mat, tmat);
}
/** \} */
