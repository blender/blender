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
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_link_utils.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_memarena.h"
#include "BLI_offset_indices.hh"
#include "BLI_rand.h"
#include "BLI_set.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node_runtime.hh"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "NOD_texture.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_colormanagement.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "paint_intern.h"
#include "sculpt_intern.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "atomic_ops.h"

#include "bmesh.h"
#include "bmesh_log.h"
#include "bmesh_tools.h"

#include "UI_resources.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

using blender::IndexRange;
using blender::OffsetIndices;

using namespace blender::bke::paint;

/**
 * Checks if the face sets of the adjacent faces to the edge between \a v1 and \a v2
 * in the base mesh are equal.
 */
static bool sculpt_check_unique_face_set_for_edge_in_base_mesh(const SculptSession *ss,
                                                               int v1,
                                                               int v2)
{
  if (!ss->face_sets) {
    return true;
  }

  const MeshElemMap *vert_map = &ss->pmap[v1];
  int p1 = -1, p2 = -1;
  for (int i = 0; i < vert_map->count; i++) {
    const IndexRange p = ss->polys[vert_map->indices[i]];

    for (int l = 0; l < p.size(); l++) {
      const int *corner_verts = &ss->corner_verts[p.start() + l];
      if (*corner_verts == v2) {
        if (p1 == -1) {
          p1 = vert_map->indices[i];
          break;
        }

        if (p2 == -1) {
          p2 = vert_map->indices[i];
          break;
        }
      }
    }
  }

  if (p1 != -1 && p2 != -1) {
    return abs(ss->face_sets[p1]) == (ss->face_sets[p2]);
  }
  return true;
}

static bool sculpt_check_boundary_vertex_in_base_mesh(const SculptSession *ss, int index)
{
  BLI_assert(ss->vertex_info.boundary);

  return BLI_BITMAP_TEST(ss->vertex_info.boundary, index);
}

static bool sculpt_check_unique_face_set_in_base_mesh(const SculptSession *ss,
                                                      int vertex,
                                                      bool *r_corner)
{
  if (!ss->attrs.face_set) {
    return true;
  }
  const MeshElemMap *vert_map = &ss->pmap[vertex];
  int fset1 = -1, fset2 = -1, fset3 = -1;

  for (int i : IndexRange(vert_map->count)) {
    PBVHFaceRef face = {vert_map->indices[i]};
    int fset = face_attr_get<int>(face, ss->attrs.face_set);

    if (fset1 == -1) {
      fset1 = fset;
    }
    else if (fset2 == -1 && fset != fset1) {
      fset2 = fset;
    }
    else if (fset3 == -1 && fset != fset1 && fset != fset2) {
      fset3 = fset;
    }
  }

  *r_corner = fset3 != -1;
  return fset2 == -1;
}

eSculptBoundary SCULPT_edge_is_boundary(const SculptSession *ss,
                                        const PBVHEdgeRef edge,
                                        eSculptBoundary typemask)
{

  int ret = 0;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMEdge *e = (BMEdge *)edge.i;

      if (e->l && e->l != e->l->radial_next && typemask & SCULPT_BOUNDARY_SHARP_ANGLE) {
        if (BKE_pbvh_test_sharp_faces_bmesh(e->l->f, e->l->radial_next->f, ss->sharp_angle_limit))
        {
          ret |= SCULPT_BOUNDARY_SHARP_ANGLE;
        }
      }

      if (typemask & SCULPT_BOUNDARY_MESH) {
        ret |= (!e->l || e->l == e->l->radial_next) ? SCULPT_BOUNDARY_MESH : 0;
      }

      if ((typemask & SCULPT_BOUNDARY_FACE_SET) && e->l && e->l != e->l->radial_next &&
          ss->cd_faceset_offset != -1)
      {
        if (ss->boundary_symmetry) {
          // TODO: calc and cache this properly

          int boundflag1 = BM_ELEM_CD_GET_INT(e->v1, ss->attrs.boundary_flags->bmesh_cd_offset);
          int boundflag2 = BM_ELEM_CD_GET_INT(e->v2, ss->attrs.boundary_flags->bmesh_cd_offset);

          ret |= (boundflag1 | boundflag2) & SCULPT_BOUNDARY_FACE_SET;
        }
        else {
          int fset1 = BM_ELEM_CD_GET_INT(e->l->f, ss->cd_faceset_offset);
          int fset2 = BM_ELEM_CD_GET_INT(e->l->radial_next->f, ss->cd_faceset_offset);

          ret |= fset1 != fset2 ? SCULPT_BOUNDARY_FACE_SET : 0;
        }
      }

      if (typemask & SCULPT_BOUNDARY_UV) {
        int boundflag1 = BM_ELEM_CD_GET_INT(e->v1, ss->attrs.boundary_flags->bmesh_cd_offset);
        int boundflag2 = BM_ELEM_CD_GET_INT(e->v2, ss->attrs.boundary_flags->bmesh_cd_offset);

        ret |= (boundflag1 | boundflag2) & SCULPT_BOUNDARY_UV;
      }

      if (typemask & SCULPT_BOUNDARY_SHARP_MARK) {
        ret |= !BM_elem_flag_test(e, BM_ELEM_SMOOTH) ? SCULPT_BOUNDARY_SHARP_MARK : 0;
      }

      if (typemask & SCULPT_BOUNDARY_SEAM) {
        ret |= BM_elem_flag_test(e, BM_ELEM_SEAM) ? SCULPT_BOUNDARY_SEAM : 0;
      }

      break;
    }
    case PBVH_FACES: {
      eSculptBoundary epmap_mask = typemask & (SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_FACE_SET);

      /* Some boundary types require an edge->poly map to be fully accurate. */
      if (epmap_mask && ss->epmap) {
        if (ss->face_sets && epmap_mask & SCULPT_BOUNDARY_FACE_SET) {
          MeshElemMap *polys = &ss->epmap[edge.i];
          int fset = -1;

          for (int i : IndexRange(polys->count)) {
            if (fset == -1) {
              fset = ss->face_sets[polys->indices[i]];
            }
            else if (ss->face_sets[polys->indices[i]] != fset) {
              ret |= SCULPT_BOUNDARY_FACE_SET;
              break;
            }
          }
        }
        if (epmap_mask & SCULPT_BOUNDARY_MESH) {
          ret |= ss->epmap[edge.i].count == 1 ? SCULPT_BOUNDARY_MESH : 0;
        }
      }
      else if (epmap_mask)
      { /* No edge->poly map; approximate from vertices (will give artifacts on corners). */
        PBVHVertRef v1, v2;

        SCULPT_edge_get_verts(ss, edge, &v1, &v2);
        eSculptBoundary a = SCULPT_vertex_is_boundary(ss, v1, epmap_mask);
        eSculptBoundary b = SCULPT_vertex_is_boundary(ss, v2, epmap_mask);

        ret |= a & b;
      }

      if (typemask & SCULPT_BOUNDARY_SHARP_MARK) {
        ret |= (ss->sharp_edge && ss->sharp_edge[edge.i]) ? SCULPT_BOUNDARY_SHARP_MARK : 0;
      }

      if (typemask & SCULPT_BOUNDARY_SEAM) {
        ret |= (ss->seam_edge && ss->seam_edge[edge.i]) ? SCULPT_BOUNDARY_SEAM : 0;
      }

      break;
    }
    case PBVH_GRIDS: {
      // not implemented
      break;
    }
  }

  return (eSculptBoundary)ret;
}

void SCULPT_edge_get_verts(const SculptSession *ss,
                           const PBVHEdgeRef edge,
                           PBVHVertRef *r_v1,
                           PBVHVertRef *r_v2)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMEdge *e = (BMEdge *)edge.i;
      r_v1->i = (intptr_t)e->v1;
      r_v2->i = (intptr_t)e->v2;
      break;
    }

    case PBVH_FACES: {
      r_v1->i = (intptr_t)ss->edges[edge.i][0];
      r_v2->i = (intptr_t)ss->edges[edge.i][1];
      break;
    }
    case PBVH_GRIDS:
      // not supported yet
      r_v1->i = r_v2->i = PBVH_REF_NONE;
      break;
  }
}

PBVHVertRef SCULPT_edge_other_vertex(const SculptSession *ss,
                                     const PBVHEdgeRef edge,
                                     const PBVHVertRef vertex)
{
  PBVHVertRef v1, v2;

  SCULPT_edge_get_verts(ss, edge, &v1, &v2);

  return v1.i == vertex.i ? v2 : v1;
}

static void grids_update_boundary_flags(const SculptSession *ss, PBVHVertRef vertex)
{
  int *flag = vertex_attr_ptr<int>(vertex, ss->attrs.boundary_flags);

  *flag = 0;

  int index = (int)vertex.i;
  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
  const int grid_index = index / key->grid_area;
  const int vertex_index = index - grid_index * key->grid_area;
  SubdivCCGCoord coord{};

  coord.grid_index = grid_index;
  coord.x = vertex_index % key->grid_size;
  coord.y = vertex_index / key->grid_size;

  int v1, v2;
  const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
      ss->subdiv_ccg, &coord, ss->corner_verts, ss->polys, &v1, &v2);

  bool fset_corner = false;
  switch (adjacency) {
    case SUBDIV_CCG_ADJACENT_VERTEX:
      if (!sculpt_check_unique_face_set_in_base_mesh(ss, v1, &fset_corner)) {
        *flag |= SCULPT_BOUNDARY_FACE_SET;
      }
      if (sculpt_check_boundary_vertex_in_base_mesh(ss, v1)) {
        *flag |= SCULPT_BOUNDARY_MESH;
      }
      break;
    case SUBDIV_CCG_ADJACENT_EDGE: {
      if (!sculpt_check_unique_face_set_for_edge_in_base_mesh(ss, v1, v2)) {
        *flag |= SCULPT_BOUNDARY_FACE_SET;
      }

      if (sculpt_check_boundary_vertex_in_base_mesh(ss, v1) &&
          sculpt_check_boundary_vertex_in_base_mesh(ss, v2))
      {
        *flag |= SCULPT_BOUNDARY_MESH;
      }
      break;
    }
    case SUBDIV_CCG_ADJACENT_NONE:
      break;
  }

  if (fset_corner) {
    *flag |= SCULPT_CORNER_FACE_SET | SCULPT_BOUNDARY_FACE_SET;
  }
}

static void faces_update_boundary_flags(const SculptSession *ss, const PBVHVertRef vertex)
{
  blender::bke::pbvh::update_vert_boundary_faces((int *)ss->attrs.boundary_flags->data,
                                                 ss->face_sets,
                                                 ss->hide_poly,
                                                 ss->vert_positions,
                                                 ss->edges.data(),
                                                 ss->corner_verts.data(),
                                                 ss->corner_edges.data(),
                                                 ss->polys,
                                                 ss->totfaces,
                                                 ss->pmap,
                                                 vertex,
                                                 ss->sharp_edge,
                                                 ss->seam_edge,
                                                 static_cast<uint8_t *>(ss->attrs.flags->data),
                                                 static_cast<int *>(ss->attrs.valence->data));

  /* We have to handle boundary here seperately. */

  int *flag = vertex_attr_ptr<int>(vertex, ss->attrs.boundary_flags);
  *flag &= ~(SCULPT_CORNER_MESH | SCULPT_BOUNDARY_MESH);

  if (sculpt_check_boundary_vertex_in_base_mesh(ss, vertex.i)) {
    *flag |= SCULPT_BOUNDARY_MESH;

    if (ss->pmap[vertex.i].count < 4) {
      bool ok = true;

      for (int i = 0; i < ss->pmap[vertex.i].count; i++) {
        const IndexRange mp = ss->polys[ss->pmap[vertex.i].indices[i]];
        if (mp.size() < 4) {
          ok = false;
        }
      }
      if (ok) {
        *flag |= SCULPT_CORNER_MESH;
      }
      else {
        *flag &= ~SCULPT_CORNER_MESH;
      }
    }
  }
}

static bool sculpt_vertex_ensure_boundary(const SculptSession *ss,
                                          const PBVHVertRef vertex,
                                          int mask)
{
  eSculptBoundary flag = *vertex_attr_ptr<eSculptBoundary>(vertex, ss->attrs.boundary_flags);
  bool needs_update = flag & SCULPT_BOUNDARY_NEEDS_UPDATE;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = reinterpret_cast<BMVert *>(vertex.i);

      if (needs_update) {
        BKE_pbvh_update_vert_boundary(ss->cd_faceset_offset,
                                      ss->cd_vert_node_offset,
                                      ss->cd_face_node_offset,
                                      ss->cd_vcol_offset,
                                      ss->attrs.boundary_flags->bmesh_cd_offset,
                                      ss->attrs.flags->bmesh_cd_offset,
                                      ss->attrs.valence->bmesh_cd_offset,
                                      (BMVert *)vertex.i,
                                      ss->boundary_symmetry,
                                      &ss->bm->ldata,
                                      ss->totuv,
                                      !ss->ignore_uvs,
                                      ss->sharp_angle_limit);
      }
      else if ((mask & (SCULPT_BOUNDARY_SHARP_ANGLE | SCULPT_CORNER_SHARP_ANGLE)) &&
               flag & SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE)
      {
        blender::bke::pbvh::update_sharp_boundary_bmesh(
            v, ss->attrs.boundary_flags->bmesh_cd_offset, ss->sharp_angle_limit);
      }

      break;
    }
    case PBVH_FACES: {
      if (needs_update) {
        faces_update_boundary_flags(ss, vertex);
      }
      break;
    }

    case PBVH_GRIDS: {
      if (needs_update) {
        grids_update_boundary_flags(ss, vertex);
        needs_update = false;
      }
      break;
    }
  }

  return needs_update;
}

eSculptCorner SCULPT_vertex_is_corner(const SculptSession *ss,
                                      const PBVHVertRef vertex,
                                      eSculptCorner cornertype)
{
  sculpt_vertex_ensure_boundary(ss, vertex, int(cornertype));
  eSculptCorner flag = eSculptCorner(vertex_attr_get<int>(vertex, ss->attrs.boundary_flags));

  return flag & cornertype;
}

eSculptBoundary SCULPT_vertex_is_boundary(const SculptSession *ss,
                                          const PBVHVertRef vertex,
                                          eSculptBoundary boundary_types)
{
  eSculptBoundary flag = vertex_attr_get<eSculptBoundary>(vertex, ss->attrs.boundary_flags);

  sculpt_vertex_ensure_boundary(ss, vertex, int(boundary_types));

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS && boundary_types & SCULPT_BOUNDARY_MESH) {
    /* TODO: BKE_pbvh_update_vert_boundary_grids does not yet support mesh boundaries for
     * PBVH_GRIDS.*/
    const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
    const int grid_index = vertex.i / key->grid_area;
    const int vertex_index = vertex.i - grid_index * key->grid_area;
    SubdivCCGCoord coord{};
    coord.grid_index = grid_index;
    coord.x = vertex_index % key->grid_size;
    coord.y = vertex_index / key->grid_size;
    int v1, v2;
    const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
        ss->subdiv_ccg, &coord, ss->corner_verts, ss->polys, &v1, &v2);

    switch (adjacency) {
      case SUBDIV_CCG_ADJACENT_VERTEX:
        flag |= sculpt_check_boundary_vertex_in_base_mesh(ss, v1) ? SCULPT_BOUNDARY_MESH :
                                                                    (eSculptBoundary)0;
      case SUBDIV_CCG_ADJACENT_EDGE:
        if (sculpt_check_boundary_vertex_in_base_mesh(ss, v1) &&
            sculpt_check_boundary_vertex_in_base_mesh(ss, v2))
        {
          flag |= SCULPT_BOUNDARY_MESH;
        }
      case SUBDIV_CCG_ADJACENT_NONE:
        break;
    }
  }

  return flag & boundary_types;
}

bool SCULPT_vertex_check_origdata(SculptSession *ss, PBVHVertRef vertex)
{
  return blender::bke::paint::get_original_vertex(ss, vertex, nullptr, nullptr, nullptr, nullptr);
}

int SCULPT_vertex_valence_get(const struct SculptSession *ss, PBVHVertRef vertex)
{
  SculptVertexNeighborIter ni;
  uint8_t flag = vertex_attr_get<uint8_t>(vertex, ss->attrs.flags);

  if (flag & SCULPTFLAG_NEED_VALENCE) {
    vertex_attr_set<uint8_t>(vertex, ss->attrs.flags, flag & ~SCULPTFLAG_NEED_VALENCE);

    int tot = 0;

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      tot++;
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    vertex_attr_set<uint>(vertex, ss->attrs.valence, tot);
    return tot;
  }

  return vertex_attr_get<uint>(vertex, ss->attrs.valence);
}

/* See SCULPT_stroke_id_test. */
void SCULPT_stroke_id_ensure(Object *ob)
{
  BKE_sculpt_ensure_sculpt_layers(ob);
}

int SCULPT_get_tool(const SculptSession *ss, const Brush *br)
{
  if (ss->cache && ss->cache->tool_override) {
    return ss->cache->tool_override;
  }

  return br->sculpt_tool;
}

void SCULPT_ensure_persistent_layers(SculptSession *ss, Object *ob)
{
  SculptAttributeParams params = {};
  params.permanent = true;

  if (!ss->attrs.persistent_co) {
    ss->attrs.persistent_co = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_co), &params);
    ss->attrs.persistent_no = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_no), &params);
    ss->attrs.persistent_disp = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(persistent_disp), &params);
  }
}

void SCULPT_apply_dyntopo_settings(SculptSession *ss, Sculpt *sculpt, Brush *brush)
{
  if (!brush) {
    ss->cached_dyntopo = sculpt->dyntopo;
    return;
  }

  DynTopoSettings *ds1 = &brush->dyntopo;
  DynTopoSettings *ds2 = &sculpt->dyntopo;

  DynTopoSettings *ds_final = &ss->cached_dyntopo;

  ds_final->inherit = ds1->inherit;
  ds_final->flag = 0;

  for (int i = 0; i < DYNTOPO_MAX_FLAGS; i++) {
    if (ds_final->inherit & (1 << i)) {
      ds_final->flag |= ds2->flag & (1 << i);
    }
    else {
      ds_final->flag |= ds1->flag & (1 << i);
    }
  }

  ds_final->constant_detail = ds_final->inherit & DYNTOPO_INHERIT_CONSTANT_DETAIL ?
                                  ds2->constant_detail :
                                  ds1->constant_detail;
  ds_final->detail_percent = ds_final->inherit & DYNTOPO_INHERIT_DETAIL_PERCENT ?
                                 ds2->detail_percent :
                                 ds1->detail_percent;
  ds_final->detail_range = ds_final->inherit & DYNTOPO_INHERIT_DETAIL_RANGE ? ds2->detail_range :
                                                                              ds1->detail_range;
  ds_final->detail_size = ds_final->inherit & DYNTOPO_INHERIT_DETAIL_SIZE ? ds2->detail_size :
                                                                            ds1->detail_size;
  ds_final->mode = ds_final->inherit & DYNTOPO_INHERIT_MODE ? ds2->mode : ds1->mode;
  ds_final->radius_scale = ds_final->inherit & DYNTOPO_INHERIT_RADIUS_SCALE ? ds2->radius_scale :
                                                                              ds1->radius_scale;
  ds_final->spacing = ds_final->inherit & DYNTOPO_INHERIT_SPACING ? ds2->spacing : ds1->spacing;
  ds_final->repeat = ds_final->inherit & DYNTOPO_INHERIT_REPEAT ? ds2->repeat : ds1->repeat;
}

bool SCULPT_face_is_hidden(const SculptSession *ss, PBVHFaceRef face)
{
  if (ss->bm) {
    BMFace *f = reinterpret_cast<BMFace *>(face.i);
    return BM_elem_flag_test(f, BM_ELEM_HIDDEN);
  }
  else {
    return ss->hide_poly ? ss->hide_poly[face.i] : false;
  }
}
