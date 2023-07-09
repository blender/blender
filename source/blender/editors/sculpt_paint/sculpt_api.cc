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
#include "BKE_idprop.h"
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
#include "BKE_pbvh_api.hh"
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

using blender::float2;
using blender::float3;
using blender::IndexRange;
using blender::OffsetIndices;

using namespace blender::bke::paint;

static bool sculpt_check_boundary_vertex_in_base_mesh(const SculptSession *ss, int index)
{
  BLI_assert(ss->vertex_info.boundary);

  return BLI_BITMAP_TEST(ss->vertex_info.boundary, index);
}

eSculptBoundary SCULPT_edge_is_boundary(const SculptSession *ss,
                                        const PBVHEdgeRef edge,
                                        eSculptBoundary boundary_types)
{
  int oldflag = blender::bke::paint::edge_attr_get<int>(edge, ss->attrs.edge_boundary_flags);
  bool update = oldflag & (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV |
                           SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE);

  if (update) {
    switch (BKE_pbvh_type(ss->pbvh)) {
      case PBVH_BMESH: {
        BMEdge *e = reinterpret_cast<BMEdge *>(edge.i);
        blender::bke::pbvh::update_edge_boundary_bmesh(
            e,
            ss->attrs.face_set ? ss->attrs.face_set->bmesh_cd_offset : -1,
            ss->attrs.edge_boundary_flags->bmesh_cd_offset,
            ss->attrs.flags->bmesh_cd_offset,
            ss->attrs.valence->bmesh_cd_offset,
            &ss->bm->ldata,
            ss->sharp_angle_limit);
        break;
      }
      case PBVH_FACES: {
        Span<float3> pos = {reinterpret_cast<const float3 *>(ss->vert_positions), ss->totvert};
        Span<float3> nor = {reinterpret_cast<const float3 *>(ss->vert_normals), ss->totvert};

        blender::bke::pbvh::update_edge_boundary_faces(
            edge.i,
            pos,
            nor,
            ss->edges,
            ss->polys,
            ss->poly_normals,
            (int *)ss->attrs.edge_boundary_flags->data,
            (int *)ss->attrs.boundary_flags->data,
            ss->attrs.face_set ? (int *)ss->attrs.face_set->data : nullptr,
            ss->sharp_edge,
            ss->seam_edge,
            ss->pmap,
            ss->epmap,
            ss->ldata,
            ss->sharp_angle_limit,
            ss->corner_verts,
            ss->corner_edges);
        break;
      }
      case PBVH_GRIDS: {
        const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);

        blender::bke::pbvh::update_edge_boundary_grids(
            edge.i,
            ss->edges,
            ss->polys,
            (int *)ss->attrs.edge_boundary_flags->data,
            (int *)ss->attrs.boundary_flags->data,
            ss->attrs.face_set ? (int *)ss->attrs.face_set->data : nullptr,
            ss->sharp_edge,
            ss->seam_edge,
            ss->pmap,
            ss->epmap,
            ss->ldata,
            ss->subdiv_ccg,
            key,
            ss->sharp_angle_limit,
            ss->corner_verts,
            ss->corner_edges);

        break;
      }
    }
  }

  return boundary_types & eSculptBoundary(blender::bke::paint::edge_attr_get<int>(
                              edge, ss->attrs.edge_boundary_flags));
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
  blender::bke::pbvh::update_vert_boundary_grids(ss->pbvh, vertex.i);
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

    Span<int> pmap = ss->pmap[vertex.i];

    if (pmap.size() < 4) {
      bool ok = true;

      for (int poly : pmap) {
        const IndexRange mp = ss->polys[poly];
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
  bool needs_update = flag & (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV);

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = reinterpret_cast<BMVert *>(vertex.i);

      if (needs_update) {
        blender::bke::pbvh::update_vert_boundary_bmesh(ss->cd_faceset_offset,
                                                       ss->cd_vert_node_offset,
                                                       ss->cd_face_node_offset,
                                                       ss->cd_vcol_offset,
                                                       ss->attrs.boundary_flags->bmesh_cd_offset,
                                                       ss->attrs.flags->bmesh_cd_offset,
                                                       ss->attrs.valence->bmesh_cd_offset,
                                                       (BMVert *)vertex.i,
                                                       &ss->bm->ldata,
                                                       ss->sharp_angle_limit);
      }
      else if ((mask & (SCULPT_BOUNDARY_SHARP_ANGLE | SCULPT_CORNER_SHARP_ANGLE)) &&
               flag & SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE)
      {
        blender::bke::pbvh::update_sharp_vertex_bmesh(
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

static int sculpt_calc_valence(const struct SculptSession *ss, PBVHVertRef vertex)
{

  int tot = 0;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = reinterpret_cast<BMVert *>(vertex.i);
      tot = BM_vert_edge_count(v);
      break;
    }
    case PBVH_FACES: {
      Vector<int, 32> edges;
      for (int edge : ss->pmap[vertex.i]) {
        if (!edges.contains(edge)) {
          edges.append(edge);
        }
      }

      tot = edges.size();
      break;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int vertex_index = vertex.i - grid_index * key->grid_area;

      SubdivCCGCoord coord{};
      coord.grid_index = grid_index;
      coord.x = vertex_index % key->grid_size;
      coord.y = vertex_index / key->grid_size;

      SubdivCCGNeighbors neighbors;
      BKE_subdiv_ccg_neighbor_coords_get(ss->subdiv_ccg, &coord, true, &neighbors);

      tot = neighbors.size;
      break;
    }
  }

  return tot;
}

int SCULPT_vertex_valence_get(const struct SculptSession *ss, PBVHVertRef vertex)
{
  uint8_t flag = vertex_attr_get<uint8_t>(vertex, ss->attrs.flags);

  if (flag & SCULPTFLAG_NEED_VALENCE) {
    int tot = sculpt_calc_valence(ss, vertex);

    vertex_attr_set<uint>(vertex, ss->attrs.valence, tot);
    *vertex_attr_ptr<uint8_t>(vertex, ss->attrs.flags) &= ~SCULPTFLAG_NEED_VALENCE;

    return tot;
  }

#if 0
  int tot = vertex_attr_get<uint>(vertex, ss->attrs.valence);
  int real_tot = sculpt_calc_valence(ss, vertex);

  if (tot != real_tot) {
    printf("%s: invalid valence! got %d expected %d\n", tot, real_tot);
  }
#endif

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

void SCULPT_apply_dyntopo_settings(Scene *scene, SculptSession *ss, Sculpt *sculpt, Brush *brush)
{
  using namespace blender::bke::dyntopo;

  if (!brush) {
    ss->cached_dyntopo = sculpt->dyntopo;
    return;
  }

  DynTopoSettings *ds1 = &brush->dyntopo;
  DynTopoSettings *ds2 = &sculpt->dyntopo;

  DynTopoSettings *ds_final = &ss->cached_dyntopo;

  ds_final->inherit = BKE_brush_dyntopo_inherit_flags(brush);
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
  ds_final->detail_size = ds_final->inherit & DYNTOPO_INHERIT_DETAIL_SIZE ? ds2->detail_size :
                                                                            ds1->detail_size;
  ds_final->mode = ds_final->inherit & DYNTOPO_INHERIT_MODE ? ds2->mode : ds1->mode;
  ds_final->radius_scale = ds_final->inherit & DYNTOPO_INHERIT_RADIUS_SCALE ? ds2->radius_scale :
                                                                              ds1->radius_scale;
  ds_final->spacing = ds_final->inherit & DYNTOPO_INHERIT_SPACING ? ds2->spacing : ds1->spacing;
  ds_final->repeat = ds_final->inherit & DYNTOPO_INHERIT_REPEAT ? ds2->repeat : ds1->repeat;
  ds_final->quality = ds_final->inherit & DYNTOPO_INHERIT_QUALITY ? ds2->quality : ds1->quality;
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
