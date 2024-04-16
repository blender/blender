/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_linklist_stack.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_ccg.h"
#include "BKE_context.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "bmesh.hh"

#define SCULPT_GEODESIC_VERTEX_NONE -1

namespace blender::ed::sculpt_paint::geodesic {

/* Propagate distance from v1 and v2 to v0. */
static bool sculpt_geodesic_mesh_test_dist_add(Span<float3> vert_positions,
                                               const int v0,
                                               const int v1,
                                               const int v2,
                                               MutableSpan<float> dists,
                                               const Set<int> &initial_verts)
{
  if (initial_verts.contains(v0)) {
    return false;
  }

  BLI_assert(dists[v1] != FLT_MAX);
  if (dists[v0] <= dists[v1]) {
    return false;
  }

  float dist0;
  if (v2 != SCULPT_GEODESIC_VERTEX_NONE) {
    BLI_assert(dists[v2] != FLT_MAX);
    if (dists[v0] <= dists[v2]) {
      return false;
    }
    dist0 = geodesic_distance_propagate_across_triangle(
        vert_positions[v0], vert_positions[v1], vert_positions[v2], dists[v1], dists[v2]);
  }
  else {
    float vec[3];
    sub_v3_v3v3(vec, vert_positions[v1], vert_positions[v0]);
    dist0 = dists[v1] + len_v3(vec);
  }

  if (dist0 < dists[v0]) {
    dists[v0] = dist0;
    return true;
  }

  return false;
}

static Array<float> geodesic_mesh_create(Object *ob,
                                         const Set<int> &initial_verts,
                                         const float limit_radius)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  const int totvert = mesh->verts_num;
  const int totedge = mesh->edges_num;

  const float limit_radius_sq = limit_radius * limit_radius;

  const Span<float3> vert_positions = SCULPT_mesh_deformed_positions_get(ss);
  const Span<int2> edges = mesh->edges();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int> corner_edges = mesh->corner_edges();
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  Array<float> dists(totvert);
  BitVector<> edge_tag(totedge);

  if (ss->edge_to_face_map.is_empty()) {
    ss->edge_to_face_map = bke::mesh::build_edge_to_face_map(
        faces, corner_edges, edges.size(), ss->edge_to_face_offsets, ss->edge_to_face_indices);
  }
  if (ss->vert_to_edge_map.is_empty()) {
    ss->vert_to_edge_map = bke::mesh::build_vert_to_edge_map(
        edges, mesh->verts_num, ss->vert_to_edge_offsets, ss->vert_to_edge_indices);
  }

  /* Both contain edge indices encoded as *void. */
  BLI_LINKSTACK_DECLARE(queue, void *);
  BLI_LINKSTACK_DECLARE(queue_next, void *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totvert; i++) {
    if (initial_verts.contains(i)) {
      dists[i] = 0.0f;
    }
    else {
      dists[i] = FLT_MAX;
    }
  }

  /* Masks vertices that are further than limit radius from an initial vertex. As there is no need
   * to define a distance to them the algorithm can stop earlier by skipping them. */
  BitVector<> affected_vert(totvert);

  if (limit_radius == FLT_MAX) {
    /* In this case, no need to loop through all initial vertices to check distances as they are
     * all going to be affected. */
    affected_vert.fill(true);
  }
  else {
    /* This is an O(n^2) loop used to limit the geodesic distance calculation to a radius. When
     * this optimization is needed, it is expected for the tool to request the distance to a low
     * number of vertices (usually just 1 or 2). */
    for (const int v : initial_verts) {
      const float *v_co = vert_positions[v];
      for (int i = 0; i < totvert; i++) {
        if (len_squared_v3v3(v_co, vert_positions[i]) <= limit_radius_sq) {
          affected_vert[i].set();
        }
      }
    }
  }

  /* Add edges adjacent to an initial vertex to the queue. */
  for (int i = 0; i < totedge; i++) {
    const int v1 = edges[i][0];
    const int v2 = edges[i][1];
    if (!affected_vert[v1] && !affected_vert[v2]) {
      continue;
    }
    if (dists[v1] != FLT_MAX || dists[v2] != FLT_MAX) {
      BLI_LINKSTACK_PUSH(queue, POINTER_FROM_INT(i));
    }
  }

  do {
    while (BLI_LINKSTACK_SIZE(queue)) {
      const int e = POINTER_AS_INT(BLI_LINKSTACK_POP(queue));
      int v1 = edges[e][0];
      int v2 = edges[e][1];

      if (dists[v1] == FLT_MAX || dists[v2] == FLT_MAX) {
        if (dists[v1] > dists[v2]) {
          std::swap(v1, v2);
        }
        sculpt_geodesic_mesh_test_dist_add(
            vert_positions, v2, v1, SCULPT_GEODESIC_VERTEX_NONE, dists, initial_verts);
      }

      for (const int face : ss->edge_to_face_map[e]) {
        if (!hide_poly.is_empty() && hide_poly[face]) {
          continue;
        }
        for (const int v_other : corner_verts.slice(faces[face])) {
          if (ELEM(v_other, v1, v2)) {
            continue;
          }
          if (sculpt_geodesic_mesh_test_dist_add(
                  vert_positions, v_other, v1, v2, dists, initial_verts))
          {
            for (const int e_other : ss->vert_to_edge_map[v_other]) {
              int ev_other;
              if (edges[e_other][0] == v_other) {
                ev_other = edges[e_other][1];
              }
              else {
                ev_other = edges[e_other][0];
              }

              if (e_other != e && !edge_tag[e_other] &&
                  (ss->edge_to_face_map[e_other].is_empty() || dists[ev_other] != FLT_MAX))
              {
                if (affected_vert[v_other] || affected_vert[ev_other]) {
                  edge_tag[e_other].set();
                  BLI_LINKSTACK_PUSH(queue_next, POINTER_FROM_INT(e_other));
                }
              }
            }
          }
        }
      }
    }

    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      const int e = POINTER_AS_INT(lnk->link);
      edge_tag[e].reset();
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);

  return dists;
}

/* For sculpt mesh data that does not support a geodesic distances algorithm, fallback to the
 * distance to each vertex. In this case, only one of the initial vertices will be used to
 * calculate the distance. */
static Array<float> geodesic_fallback_create(Object *ob, const Set<int> &initial_verts)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  const int totvert = mesh->verts_num;
  Array<float> dists(totvert, 0.0f);
  const int first_affected = *initial_verts.begin();
  if (first_affected == SCULPT_GEODESIC_VERTEX_NONE) {
    dists.fill(FLT_MAX);
    return dists;
  }

  const float *first_affected_co = SCULPT_vertex_co_get(
      ss, BKE_pbvh_index_to_vertex(ss->pbvh, first_affected));
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    dists[i] = len_v3v3(first_affected_co, SCULPT_vertex_co_get(ss, vertex));
  }

  return dists;
}

Array<float> distances_create(Object *ob, const Set<int> &initial_verts, const float limit_radius)
{
  SculptSession *ss = ob->sculpt;
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return geodesic_mesh_create(ob, initial_verts, limit_radius);
    case PBVH_BMESH:
    case PBVH_GRIDS:
      return geodesic_fallback_create(ob, initial_verts);
  }
  BLI_assert_unreachable();
  return {};
}

Array<float> distances_create_from_vert_and_symm(Object *ob,
                                                 const PBVHVertRef vertex,
                                                 const float limit_radius)
{
  SculptSession *ss = ob->sculpt;
  Set<int> initial_verts;

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (SCULPT_is_symmetry_iteration_valid(i, symm)) {
      PBVHVertRef v = {PBVH_REF_NONE};

      if (i == 0) {
        v = vertex;
      }
      else {
        float location[3];
        flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), ePaintSymmetryFlags(i));
        v = SCULPT_nearest_vertex_get(ob, location, FLT_MAX, false);
      }
      if (v.i != PBVH_REF_NONE) {
        initial_verts.add(BKE_pbvh_vertex_to_index(ss->pbvh, v));
      }
    }
  }

  return distances_create(ob, initial_verts, limit_radius);
}

}  // namespace blender::ed::sculpt_paint::geodesic
