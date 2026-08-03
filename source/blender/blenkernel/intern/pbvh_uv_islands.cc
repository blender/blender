/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_atomic_disjoint_set.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_bit_vector.hh"
#include "BLI_map.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "PRF_profile.hh"

#include "pbvh_pixels_rasterize.hh"
#include "pbvh_uv_islands.hh"

#include <atomic>
#include <iostream>
#include <optional>
#include <queue>
#include <sstream>

namespace blender::bke::pbvh::uv_islands {

static void uv_edge_append_to_uv_verts(UVIsland &island, const int uv_edge_i)
{
  for (const int uv_vert_i : island.uv_edges[uv_edge_i].verts) {
    island.uv_verts[uv_vert_i].uv_edges.append_non_duplicates(uv_edge_i);
  }
}

static void uv_primitive_append_to_uv_edges(UVIsland &island, UVPrimitive &uv_primitive)
{
  for (const int uv_edge_i : uv_primitive.edges) {
    island.uv_edges[uv_edge_i].uv_primitive_indices.append_non_duplicates(
        uv_primitive.primitive_i);
  }
}

static void uv_primitive_append_to_uv_verts(UVIsland &island, UVPrimitive &uv_primitive)
{
  for (const int uv_edge_i : uv_primitive.edges) {
    uv_edge_append_to_uv_verts(island, uv_edge_i);
  }
}

/* -------------------------------------------------------------------- */
/** \name Mesh Primitives
 * \{ */

static int primitive_get_other_uv_vert(const MeshData &mesh_data,
                                       const int3 &tri,
                                       const int v1,
                                       const int v2)
{
  const Span<int> corner_verts = mesh_data.corner_verts;
  BLI_assert(ELEM(v1, corner_verts[tri[0]], corner_verts[tri[1]], corner_verts[tri[2]]));
  BLI_assert(ELEM(v2, corner_verts[tri[0]], corner_verts[tri[1]], corner_verts[tri[2]]));
  for (const int corner : {tri[0], tri[1], tri[2]}) {
    const int vert = corner_verts[corner];
    if (!ELEM(vert, v1, v2)) {
      return vert;
    }
  }
  return -1;
}

static int get_uv_corner(const MeshData &mesh_data, const int3 &tri, const int vert)
{
  for (const int corner : {tri[0], tri[1], tri[2]}) {
    if (mesh_data.corner_verts[corner] == vert) {
      return corner;
    }
  }
  BLI_assert_unreachable();
  return tri[0];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MeshData
 * \{ */

/** Test if triangle tri_i has (v0, v1) as an edge. */
static bool tri_contains_verts(const MeshData &mesh_data,
                               const int tri_i,
                               const int v0,
                               const int v1)
{
  const int3 &tri = mesh_data.corner_tris[tri_i];
  bool has_0 = false;
  bool has_1 = false;
  for (int k = 0; k < 3; k++) {
    const int v = mesh_data.corner_verts[tri[k]];
    has_0 |= v == v0;
    has_1 |= v == v1;
  }
  return has_0 && has_1;
}

/** The corner in face whose outgoing edge is edge_i. */
static int face_corner_of_edge(const MeshData &mesh_data, const int face, const int edge_i)
{
  for (const int corner : mesh_data.faces[face]) {
    if (mesh_data.corner_edges[corner] == edge_i) {
      return corner;
    }
  }
  return -1;
}

static bool faces_have_shared_uv_edge(const MeshData &mesh_data,
                                      const int face_0,
                                      const int face_1,
                                      const int edge_i)
{
  const int c0 = face_corner_of_edge(mesh_data, face_0, edge_i);
  const int c1 = face_corner_of_edge(mesh_data, face_1, edge_i);
  if (c0 == -1 || c1 == -1) {
    return false;
  }
  const int c0_next = mesh::face_corner_next(mesh_data.faces[face_0], c0);
  const int c1_next = mesh::face_corner_next(mesh_data.faces[face_1], c1);

  const Span<float2> uv_map = mesh_data.uv_map;
  if (mesh_data.corner_verts[c0] == mesh_data.corner_verts[c1]) {
    return uv_map[c0] == uv_map[c1] && uv_map[c0_next] == uv_map[c1_next];
  }
  return uv_map[c0] == uv_map[c1_next] && uv_map[c0_next] == uv_map[c1];
}

static void mesh_data_init_topology(MeshData &mesh_data)
{
  mesh_data.edge_to_face_map = mesh::build_edge_to_face_map(mesh_data.faces,
                                                            mesh_data.corner_edges,
                                                            mesh_data.mesh_edges.size(),
                                                            mesh_data.edge_to_face_offsets,
                                                            mesh_data.edge_to_face_indices);
}

static int mesh_data_init_primitive_uv_island_ids(MeshData &mesh_data)
{
  /* Group primitives into UV islands, connected through shared UV edges. */
  const int64_t primitives_num = mesh_data.corner_tris.size();
  mesh_data.uv_island_ids.reinitialize(primitives_num);
  mesh_data.uv_edge_is_border = Array<bool>(mesh_data.mesh_edges.size(), false);

  AtomicDisjointSet disjoint_set(primitives_num);

  /* Initialize with one island per face. */
  threading::parallel_for(mesh_data.faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face : range) {
      const IndexRange tris = mesh::face_triangles_range(mesh_data.faces, int(face));
      for (const int tri : tris.drop_front(1)) {
        disjoint_set.join(tris.first(), tri);
      }
    }
  });

  /* Merge the islands of two faces when their UVs match along an edge,
   * and store which edges are on the border. */
  threading::parallel_for(mesh_data.mesh_edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int edge_i : range) {
      const Span<int> edge_faces = mesh_data.edge_to_face_map[edge_i];
      if (edge_faces.size() == 2 &&
          faces_have_shared_uv_edge(mesh_data, edge_faces[0], edge_faces[1], edge_i))
      {
        disjoint_set.join(mesh::face_triangles_range(mesh_data.faces, edge_faces[0]).first(),
                          mesh::face_triangles_range(mesh_data.faces, edge_faces[1]).first());
      }
      else {
        const bool is_loose_edge = edge_faces.is_empty();
        mesh_data.uv_edge_is_border[edge_i] = !is_loose_edge;
      }
    }
  });

  return disjoint_set.calc_reduced_ids(mesh_data.uv_island_ids);
}

static void mesh_data_init(MeshData &mesh_data)
{
  PRF_scope(ProfileCategory::Editor);
  mesh_data_init_topology(mesh_data);
  mesh_data.uv_island_len = mesh_data_init_primitive_uv_island_ids(mesh_data);
}

MeshData::MeshData(const OffsetIndices<int> faces,
                   const Span<int3> corner_tris,
                   const Span<int> corner_verts,
                   const Span<int> corner_edges,
                   const Span<int2> mesh_edges,
                   const GroupedSpan<int> vert_to_face_map,
                   const Span<float2> uv_map,
                   const Span<float3> vert_positions)
    : faces(faces),
      corner_tris(corner_tris),
      corner_verts(corner_verts),
      corner_edges(corner_edges),
      mesh_edges(mesh_edges),
      uv_map(uv_map),
      vert_positions(vert_positions),
      vert_to_face_map(vert_to_face_map)
{
  mesh_data_init(*this);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVVert
 * \{ */

static void uv_vert_init_flags(UVVert &uv_vert)
{
  uv_vert.flags.is_border = false;
  uv_vert.flags.is_extended = false;
}

UVVert::UVVert()
{
  uv_vert_init_flags(*this);
}

UVVert::UVVert(const MeshData &mesh_data, const int corner)
    : vert(mesh_data.corner_verts[corner]), uv(mesh_data.uv_map[corner])
{
  uv_vert_init_flags(*this);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVEdge
 * \{ */

bool UVEdge::has_same_verts(const UVIsland &island, const int vert1, const int vert2) const
{
  return (island.uv_verts[verts[0]].vert == vert1 && island.uv_verts[verts[1]].vert == vert2) ||
         (island.uv_verts[verts[0]].vert == vert2 && island.uv_verts[verts[1]].vert == vert1);
}

bool UVEdge::has_same_verts(const UVIsland &island, const int2 &edge) const
{
  return has_same_verts(island, edge[0], edge[1]);
}

int UVEdge::get_other_uv_vert(const UVIsland &island, const int vert)
{
  if (island.uv_verts[verts[0]].vert == vert) {
    return verts[1];
  }
  return verts[0];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVIsland
 * \{ */
int UVIsland::lookup(const UVVert &vert)
{
  const int vert_index = vert.vert;
  const Vector<int> &verts = uv_vert_lookup.lookup_or_add_default(vert_index);
  for (const int uv_vert_i : verts) {
    if (uv_verts[uv_vert_i].uv == vert.uv) {
      return uv_vert_i;
    }
  }
  return -1;
}

int UVIsland::lookup_or_create(const UVVert &vert)
{
  const int found_vert = lookup(vert);
  if (found_vert != -1) {
    return found_vert;
  }

  uv_verts.append(vert);
  uv_verts.last().uv_edges.clear();
  /* v is already a key. Ensured by UVIsland::lookup in this method. */
  uv_vert_lookup.lookup(vert.vert).append(uv_verts.size() - 1);
  return uv_verts.size() - 1;
}

int UVIsland::lookup(const UVEdge &edge)
{
  const UVVert &edge_vert_0 = this->uv_verts[edge.verts[0]];
  const UVVert &edge_vert_1 = this->uv_verts[edge.verts[1]];
  const int found_vert_i = lookup(edge_vert_0);
  if (found_vert_i == -1) {
    return -1;
  }
  const UVVert &found_vert = this->uv_verts[found_vert_i];
  for (const int uv_edge_i : found_vert.uv_edges) {
    UVEdge *e = &this->uv_edges[uv_edge_i];
    const int other_vert_i = e->get_other_uv_vert(*this, found_vert.vert);
    const UVVert &other_vert = this->uv_verts[other_vert_i];
    if (other_vert.vert == edge_vert_1.vert && other_vert.uv == edge_vert_1.uv) {
      return uv_edge_i;
    }
  }
  return -1;
}

int UVIsland::lookup_or_create(const UVEdge &edge)
{
  int found_edge = lookup(edge);
  if (found_edge != -1) {
    return found_edge;
  }

  uv_edges.append(edge);
  UVEdge *result = &uv_edges.last();
  result->uv_primitive_indices.clear();
  return uv_edges.size() - 1;
}

static UVPrimitive *add_primitive(const MeshData &mesh_data,
                                  UVIsland &uv_island,
                                  const int primitive_i,
                                  const Span<bool> tri_corner_near_border)
{
  UVPrimitive uv_primitive(primitive_i);
  const int3 &tri = mesh_data.corner_tris[primitive_i];
  uv_island.uv_primitives.append(uv_primitive);
  UVPrimitive *uv_primitive_ptr = &uv_island.uv_primitives.last();
  for (const int i : IndexRange(3)) {
    const int corner_1 = tri[i];
    const int corner_2 = tri[(i + 1) % 3];
    UVEdge uv_edge_template;
    uv_edge_template.is_border = tri_corner_near_border[int64_t(primitive_i) * 3 + i];
    uv_edge_template.verts[0] = uv_island.lookup_or_create(UVVert(mesh_data, corner_1));
    uv_edge_template.verts[1] = uv_island.lookup_or_create(UVVert(mesh_data, corner_2));
    const int uv_edge_i = uv_island.lookup_or_create(uv_edge_template);
    uv_primitive_ptr->edges[i] = uv_edge_i;
    uv_edge_append_to_uv_verts(uv_island, uv_edge_i);
    uv_island.uv_edges[uv_edge_i].uv_primitive_indices.append(uv_primitive_ptr->primitive_i);
  }
  return uv_primitive_ptr;
}

std::optional<UVBorder> extract_border_from_edges(const UVIsland &island,
                                                  MutableSpan<UVBorderEdge> edges,
                                                  MutableBoundedBitSpan borders_used);

void UVIsland::extract_borders()
{
  PRF_scope(ProfileCategory::Editor);
  /* Lookup all borders of the island. */
  Vector<UVBorderEdge> edges;
  for (const int uv_prim_i : uv_primitives.index_range()) {
    const UVPrimitive &prim = uv_primitives[uv_prim_i];
    for (const int uv_edge_i : prim.edges) {
      const UVEdge &edge = this->uv_edges[uv_edge_i];
      if (edge.is_border) {
        edges.append(UVBorderEdge(uv_edge_i, uv_prim_i));
      }
    }
  }

  BitVector<128> borders_used(edges.size(), false);
  while (true) {
    std::optional<UVBorder> border = extract_border_from_edges(*this, edges, borders_used);
    if (!border.has_value()) {
      break;
    }
    if (!border->is_ccw(*this)) {
      border->flip_order();
    }
    borders.append(std::move(*border));
  }
}

/** The inner edge of a fan. */
struct FanSegment {
  int primitive_index;
  int3 tri;
  int vert_order[3];

  struct {
    bool found : 1;
  } flags;

  FanSegment(const MeshData &mesh_data, const int primitive_index, const int3 tri, int vert)
      : primitive_index(primitive_index), tri(tri)
  {
    flags.found = false;

    /* Reorder so the first edge starts with the given vert. */
    if (mesh_data.corner_verts[tri[1]] == vert) {
      vert_order[0] = 1;
      vert_order[1] = 2;
      vert_order[2] = 0;
    }
    else if (mesh_data.corner_verts[tri[2]] == vert) {
      vert_order[0] = 2;
      vert_order[1] = 0;
      vert_order[2] = 1;
    }
    else {
      BLI_assert(mesh_data.corner_verts[tri[0]] == vert);
      vert_order[0] = 0;
      vert_order[1] = 1;
      vert_order[2] = 2;
    }
  }

  void print_debug(const MeshData &mesh_data) const
  {
    std::stringstream ss;
    ss << " v1:" << mesh_data.corner_verts[tri[vert_order[0]]];
    ss << " v2:" << mesh_data.corner_verts[tri[vert_order[1]]];
    ss << " v3:" << mesh_data.corner_verts[tri[vert_order[2]]];
    if (flags.found) {
      ss << " *found";
    }
    ss << "\n";
    std::cout << ss.str();
  }
};

struct Fan {
  /* Blades of the fan. */
  Vector<FanSegment> segments;

  struct {
    /**
     * Do all segments of the fan make a full fan, or are there parts missing. Non manifold meshes
     * can have missing parts.
     */
    bool is_manifold : 1;

  } flags;

  /* Other vertices in the fan triangle. */
  static int2 other_fan_edge_verts(const MeshData &mesh_data, const int vert, const int tri_i)
  {
    const int3 &tri = mesh_data.corner_tris[tri_i];
    int2 verts;
    int n = 0;
    for (int k = 0; k < 3; k++) {
      const int v = mesh_data.corner_verts[tri[k]];
      if (v != vert) {
        verts[n++] = v;
      }
    }
    return verts;
  }

  /* Adjacent triangle in the fan, sharing (vert, other_vert) edge. Returns -1 if there is none. */
  static int adjacent_tri(const MeshData &mesh_data,
                          const Span<int> tris,
                          const int vert,
                          const int other_vert,
                          const int tri_i)
  {
    int result = -1;
    for (const int t : tris) {
      if (t != tri_i && tri_contains_verts(mesh_data, t, vert, other_vert)) {
        if (result != -1) {
          return -1;
        }
        result = t;
      }
    }
    return result;
  }

  Fan(const MeshData &mesh_data, const int vert)
  {
    flags.is_manifold = true;

    /* Gather triangles adjacent to the vertex. */
    Vector<int, 16> tris;
    for (const int face : mesh_data.vert_to_face_map[vert]) {
      for (const int t : mesh::face_triangles_range(mesh_data.faces, face)) {
        const int3 &tri = mesh_data.corner_tris[t];
        if (ELEM(vert,
                 mesh_data.corner_verts[tri[0]],
                 mesh_data.corner_verts[tri[1]],
                 mesh_data.corner_verts[tri[2]]))
        {
          tris.append(t);
        }
      }
    }
    if (tris.is_empty()) {
      flags.is_manifold = false;
      return;
    }

    /* Walk the fan around the vertex. */
    const int start_tri = tris.first();
    int current_tri = start_tri;
    int incoming_vert = other_fan_edge_verts(mesh_data, vert, start_tri)[0];
    segments.append(FanSegment(mesh_data, start_tri, mesh_data.corner_tris[start_tri], vert));

    while (true) {
      const int2 verts = other_fan_edge_verts(mesh_data, vert, current_tri);
      const int outgoing_vert = incoming_vert == verts[0] ? verts[1] : verts[0];
      const int next_tri = adjacent_tri(mesh_data, tris, vert, outgoing_vert, current_tri);
      if (next_tri == -1) {
        /* Reached a mesh boundary or non-manifold edge. */
        flags.is_manifold = false;
        break;
      }
      if (next_tri == start_tri) {
        break;
      }
      segments.append(FanSegment(mesh_data, next_tri, mesh_data.corner_tris[next_tri], vert));
      incoming_vert = outgoing_vert;
      current_tri = next_tri;
    }
  }

  int count_edges_not_added() const
  {
    int result = 0;
    for (const FanSegment &fan_edge : segments) {
      if (!fan_edge.flags.found) {
        result++;
      }
    }
    return result;
  }

  void mark_already_added_segments(const UVIsland &uv_island, const UVVert &uv_vert)
  {
    /* Go over all fan edges to find if they can be found as primitive around the uv vertex. */
    for (FanSegment &fan_edge : segments) {
      fan_edge.flags.found = false;
      for (const int uv_edge_i : uv_vert.uv_edges) {
        const UVEdge &uv_edge = uv_island.uv_edges[uv_edge_i];
        if (uv_edge.uv_primitive_indices.contains(fan_edge.primitive_index)) {
          fan_edge.flags.found = true;
          break;
        }
      }
    }
  }

#ifndef NDEBUG
  /**
   * Check if the given vertex is part of the outside of the fan.
   * Return true if the given vertex is found on the outside of the fan, otherwise returns false.
   */
  bool contains_vert_on_outside(const MeshData &mesh_data, const int vert) const
  {
    for (const FanSegment &segment : segments) {
      int v2 = mesh_data.corner_verts[segment.tri[segment.vert_order[1]]];
      if (vert == v2) {
        return true;
      }
    }
    return false;
  }

#endif

  static bool is_path_valid(const Span<FanSegment *> path,
                            const MeshData &mesh_data,
                            const int from_vert,
                            const int to_vert)
  {
    int current_vert = from_vert;
    for (FanSegment *segment : path) {
      int v1 = mesh_data.corner_verts[segment->tri[segment->vert_order[1]]];
      int v2 = mesh_data.corner_verts[segment->tri[segment->vert_order[2]]];
      if (!ELEM(current_vert, v1, v2)) {
        return false;
      }
      current_vert = v1 == current_vert ? v2 : v1;
    }
    return current_vert == to_vert;
  }

  /**
   * Find the closest path over the fan between `from_vert` and `to_vert`. The result contains
   * exclude the starting and final edge.
   *
   * Algorithm only uses the winding order of the given fan segments.
   */
  static Vector<FanSegment *> path_between(const Span<FanSegment *> edge_order,
                                           const MeshData &mesh_data,
                                           const int from_vert,
                                           const int to_vert,
                                           const bool reversed)
  {
    const int from_vert_order = 1;
    const int to_vert_order = 2;
    const int index_increment = reversed ? -1 : 1;

    Vector<FanSegment *> result;
    result.reserve(edge_order.size());
    int index = 0;
    while (true) {
      FanSegment *segment = edge_order[index];
      int v2 = mesh_data.corner_verts[segment->tri[segment->vert_order[from_vert_order]]];
      if (v2 == from_vert) {
        break;
      }
      index = (index + index_increment + edge_order.size()) % edge_order.size();
    }

    while (true) {
      FanSegment *segment = edge_order[index];
      result.append(segment);

      int v3 = mesh_data.corner_verts[segment->tri[segment->vert_order[to_vert_order]]];
      if (v3 == to_vert) {
        break;
      }

      index = (index + index_increment + edge_order.size()) % edge_order.size();
    }

    return result;
  }

  /**
   * Score the given solution to be the best. Best solution would have the lowest score.
   *
   * Score is determined by counting the number of steps and subtracting that with steps that have
   * not yet been visited.
   */
  static int64_t score(const Span<FanSegment *> solution)
  {
    int64_t not_visited_steps = 0;
    for (FanSegment *segment : solution) {
      if (!segment->flags.found) {
        not_visited_steps++;
      }
    }
    return solution.size() - not_visited_steps;
  }

  Vector<FanSegment *> best_path_between(const MeshData &mesh_data,
                                         const int from_vert,
                                         const int to_vert)
  {
    BLI_assert_msg(contains_vert_on_outside(mesh_data, from_vert),
                   "Inconsistency detected, `from_vert` isn't part of the outside of the fan.");
    BLI_assert_msg(contains_vert_on_outside(mesh_data, to_vert),
                   "Inconsistency detected, `to_vert` isn't part of the outside of the fan.");
    if (to_vert == from_vert) {
      return Vector<FanSegment *>();
    }

    Array<FanSegment *> edges(segments.size());
    for (int64_t index : segments.index_range()) {
      edges[index] = &segments[index];
    }

    Vector<FanSegment *> winding_1 = path_between(edges, mesh_data, from_vert, to_vert, false);
    Vector<FanSegment *> winding_2 = path_between(edges, mesh_data, from_vert, to_vert, true);

    bool winding_1_valid = is_path_valid(winding_1, mesh_data, from_vert, to_vert);
    bool winding_2_valid = is_path_valid(winding_2, mesh_data, from_vert, to_vert);

    if (winding_1_valid && !winding_2_valid) {
      return winding_1;
    }
    if (!winding_1_valid && winding_2_valid) {
      return winding_2;
    }
    if (!winding_1_valid && !winding_2_valid) {
      BLI_assert_msg(false, "Both solutions aren't valid.");
      return Vector<FanSegment *>();
    }
    if (score(winding_1) < score(winding_2)) {
      return winding_1;
    }
    return winding_2;
  }

  void print_debug(const MeshData &mesh_data) const
  {
    for (const FanSegment &segment : segments) {
      segment.print_debug(mesh_data);
    }
    std::cout << "\n";
  }
};

static void add_uv_primitive_shared_uv_edge(const MeshData &mesh_data,
                                            UVIsland &island,
                                            const UVVert &connected_vert_1,
                                            const UVVert &connected_vert_2,
                                            float2 uv_unconnected,
                                            const int mesh_primitive_i)
{
  UVPrimitive prim1(mesh_primitive_i);
  const int3 &tri = mesh_data.corner_tris[mesh_primitive_i];
  /* Extract data before vector reallocation invalidates existing UVVert references. */
  const int mesh_vert_1 = connected_vert_1.vert;
  const int mesh_vert_2 = connected_vert_2.vert;
  const float2 uv_1 = connected_vert_1.uv;
  const float2 uv_2 = connected_vert_2.uv;

  const int other_vert_i = primitive_get_other_uv_vert(mesh_data, tri, mesh_vert_1, mesh_vert_2);
  UVVert vert_template;
  vert_template.uv = uv_unconnected;
  vert_template.vert = other_vert_i;
  const int uv_vert_i = island.lookup_or_create(vert_template);

  const int corner_1 = get_uv_corner(mesh_data, tri, mesh_vert_1);
  vert_template.uv = uv_1;
  vert_template.vert = mesh_data.corner_verts[corner_1];
  const int uv_vert_1_i = island.lookup_or_create(vert_template);

  const int corner_2 = get_uv_corner(mesh_data, tri, mesh_vert_2);
  vert_template.uv = uv_2;
  vert_template.vert = mesh_data.corner_verts[corner_2];
  const int uv_vert_2_i = island.lookup_or_create(vert_template);

  UVEdge edge_template;
  edge_template.verts[0] = uv_vert_1_i;
  edge_template.verts[1] = uv_vert_2_i;
  prim1.edges[0] = island.lookup_or_create(edge_template);
  edge_template.verts[0] = uv_vert_2_i;
  edge_template.verts[1] = uv_vert_i;
  prim1.edges[1] = island.lookup_or_create(edge_template);
  edge_template.verts[0] = uv_vert_i;
  edge_template.verts[1] = uv_vert_1_i;
  prim1.edges[2] = island.lookup_or_create(edge_template);
  uv_primitive_append_to_uv_edges(island, prim1);
  uv_primitive_append_to_uv_verts(island, prim1);
  island.uv_primitives.append(prim1);
}
/**
 * Find a primitive that can be used to fill give corner.
 * Will return -1 when no primitive can be found.
 */
static int find_fill_primitive(const MeshData &mesh_data,
                               const UVIsland &island,
                               UVBorderCorner &corner)
{
  if (corner.first->get_uv_vert(island, 1) != corner.second->get_uv_vert(island, 0)) {
    return -1;
  }
  if (corner.first->get_uv_vert(island, 0) == corner.second->get_uv_vert(island, 1)) {
    return -1;
  }
  const int shared_vert = island.uv_verts[corner.second->get_uv_vert(island, 0)].vert;
  const UVEdge &corner_edge = island.uv_edges[corner.first->uv_edge_i];
  const int2 edge(island.uv_verts[corner_edge.verts[0]].vert,
                  island.uv_verts[corner_edge.verts[1]].vert);
  const int target_vert = island.uv_verts[corner.second->get_uv_vert(island, 1)].vert;

  /* Find the triangle around the shared vertex whose edge is `corner.first` and
   * remaining vertex is the corner's outer vertex. */
  for (const int face : mesh_data.vert_to_face_map[shared_vert]) {
    for (const int primitive_i : mesh::face_triangles_range(mesh_data.faces, int(face))) {
      if (!tri_contains_verts(mesh_data, primitive_i, edge[0], edge[1])) {
        continue;
      }
      const int3 &tri = mesh_data.corner_tris[primitive_i];
      const int other_vert = primitive_get_other_uv_vert(mesh_data, tri, edge[0], edge[1]);
      if (other_vert == target_vert) {
        return primitive_i;
      }
    }
  }
  return -1;
}

static void add_uv_primitive_fill(UVIsland &island,
                                  const int uv_vert1_i,
                                  const int uv_vert2_i,
                                  const int uv_vert3_i,
                                  const int fill_primitive_i)
{
  UVPrimitive uv_primitive(fill_primitive_i);
  UVEdge edge_template;
  edge_template.verts[0] = uv_vert1_i;
  edge_template.verts[1] = uv_vert2_i;
  uv_primitive.edges[0] = island.lookup_or_create(edge_template);
  edge_template.verts[0] = uv_vert2_i;
  edge_template.verts[1] = uv_vert3_i;
  uv_primitive.edges[1] = island.lookup_or_create(edge_template);
  edge_template.verts[0] = uv_vert3_i;
  edge_template.verts[1] = uv_vert1_i;
  uv_primitive.edges[2] = island.lookup_or_create(edge_template);
  uv_primitive_append_to_uv_edges(island, uv_primitive);
  uv_primitive_append_to_uv_verts(island, uv_primitive);
  island.uv_primitives.append(uv_primitive);
}

static void extend_at_vert(const MeshData &mesh_data,
                           UVIsland &island,
                           UVBorderCorner &corner,
                           float min_uv_distance,
                           int64_t &order_counter,
                           Vector<UVBorderEdge *> &r_modified)
{

  int border_index = corner.first->border_index;
  UVBorder &border = island.borders[border_index];
  if (!corner.connected_in_mesh(island)) {
    return;
  }

  UVBorderEdge *after_edge = corner.second->next;

  const int uv_vert_i = corner.second->get_uv_vert(island, 0);
  const UVVert uv_vert = island.uv_verts[uv_vert_i];
  Fan fan(mesh_data, uv_vert.vert);
  if (!fan.flags.is_manifold) {
    return;
  }
  fan.mark_already_added_segments(island, uv_vert);
  int num_to_add = fan.count_edges_not_added();

  /* In 3d space everything can connected, but in uv space it may not.
   * in this case in the space between we should extract the primitives to be added
   * from the fan. */
  Vector<FanSegment *> winding_solution = fan.best_path_between(
      mesh_data,
      island.uv_verts[corner.first->get_uv_vert(island, 0)].vert,
      island.uv_verts[corner.second->get_uv_vert(island, 1)].vert);

  /*
   * When all edges are already added and its winding solution contains one segment to be added,
   * the segment should be split into two segments in order one for both sides.
   *
   * Although the tri_fill can fill the missing segment it could lead to a squashed
   * triangle when the corner angle is near 180 degrees. In order to fix this we will
   * always add two segments both using the same fill primitive.
   */
  if (winding_solution.size() < 2 && (num_to_add == 0 || corner.angle > 2.0f)) {
    int fill_primitive_1_i = island.uv_primitives[corner.second->uv_primitive].primitive_i;
    int fill_primitive_2_i = island.uv_primitives[corner.first->uv_primitive].primitive_i;

    const int fill_primitive_i = winding_solution.size() == 1 ?
                                     winding_solution[0]->primitive_index :
                                     find_fill_primitive(mesh_data, island, corner);

    if (fill_primitive_i != -1) {
      fill_primitive_1_i = fill_primitive_i;
      fill_primitive_2_i = fill_primitive_i;
    }

    float2 center_uv = corner.uv(island, 0.5f, min_uv_distance);
    add_uv_primitive_shared_uv_edge(mesh_data,
                                    island,
                                    island.uv_verts[corner.first->get_uv_vert(island, 1)],
                                    island.uv_verts[corner.first->get_uv_vert(island, 0)],
                                    center_uv,
                                    fill_primitive_1_i);
    const int new_prim_1_i = island.uv_primitives.size() - 1;
    add_uv_primitive_shared_uv_edge(mesh_data,
                                    island,
                                    island.uv_verts[corner.second->get_uv_vert(island, 0)],
                                    island.uv_verts[corner.second->get_uv_vert(island, 1)],
                                    center_uv,
                                    fill_primitive_2_i);
    const int new_prim_2_i = island.uv_primitives.size() - 1;

    /* Update border after adding the new geometry. */
    {
      UVBorderEdge *border_edge = corner.first;
      border_edge->uv_primitive = new_prim_1_i;
      border_edge->uv_edge_i = island.uv_primitives[border_edge->uv_primitive].get_uv_edge(
          island, island.uv_verts[corner.first->get_uv_vert(island, 0)].uv, center_uv);
      border_edge->reverse_order =
          island.uv_verts[island.uv_edges[border_edge->uv_edge_i].verts[0]].uv == center_uv;
    }
    {
      UVBorderEdge *border_edge = corner.second;
      border_edge->uv_primitive = new_prim_2_i;
      border_edge->uv_edge_i = island.uv_primitives[border_edge->uv_primitive].get_uv_edge(
          island, island.uv_verts[corner.second->get_uv_vert(island, 1)].uv, center_uv);
      border_edge->reverse_order =
          island.uv_verts[island.uv_edges[border_edge->uv_edge_i].verts[1]].uv == center_uv;
    }

    r_modified.append(corner.first);
    r_modified.append(after_edge);
  }
  else {
    int current_edge = corner.first->uv_edge_i;
    Vector<UVBorderEdge> new_border_edges;

    num_to_add = winding_solution.size();
    for (int64_t segment_index : winding_solution.index_range()) {

      float2 old_uv =
          island.uv_verts[island.uv_edges[current_edge].get_other_uv_vert(island, uv_vert.vert)]
              .uv;
      int shared_edge_vert =
          island.uv_verts[island.uv_edges[current_edge].get_other_uv_vert(island, uv_vert.vert)]
              .vert;

      float factor = (segment_index + 1.0f) / num_to_add;
      float2 new_uv = corner.uv(island, factor, min_uv_distance);

      FanSegment &segment = *winding_solution[segment_index];

      const int fill_primitive_i = segment.primitive_index;
      const int3 &tri_fill = mesh_data.corner_tris[fill_primitive_i];
      const int other_prim_vert = primitive_get_other_uv_vert(
          mesh_data, tri_fill, uv_vert.vert, shared_edge_vert);

      UVVert uv_vert_template;
      uv_vert_template.vert = uv_vert.vert;
      uv_vert_template.uv = uv_vert.uv;
      const int vert_1_i = island.lookup_or_create(uv_vert_template);
      uv_vert_template.vert = shared_edge_vert;
      uv_vert_template.uv = old_uv;
      const int vert_2_i = island.lookup_or_create(uv_vert_template);
      uv_vert_template.vert = other_prim_vert;
      uv_vert_template.uv = new_uv;
      const int vert_3_i = island.lookup_or_create(uv_vert_template);

      add_uv_primitive_fill(island, vert_1_i, vert_2_i, vert_3_i, fill_primitive_i);

      const int new_prim_i = island.uv_primitives.size() - 1;
      current_edge = island.uv_primitives[new_prim_i].get_uv_edge(
          island, uv_vert.vert, other_prim_vert);
      UVBorderEdge new_border(
          island.uv_primitives[new_prim_i].get_uv_edge(island, shared_edge_vert, other_prim_vert),
          new_prim_i);
      new_border_edges.append(new_border);
    }

    /* Replace two border edges with new_border_edges in the linked list. */
    corner.first->removed = true;
    corner.second->removed = true;
    UVBorderEdge *prev_edge = corner.first->prev;
    for (UVBorderEdge &new_border : new_border_edges) {
      border.edges_extend_border.append(new_border);
      UVBorderEdge *new_edge = &border.edges_extend_border.last();
      new_edge->border_index = border_index;
      new_edge->order = order_counter++;
      new_edge->removed = false;
      new_edge->prev = prev_edge;
      prev_edge->next = new_edge;
      prev_edge = new_edge;
      r_modified.append(new_edge);
    }
    prev_edge->next = after_edge;
    after_edge->prev = prev_edge;
    r_modified.append(after_edge);
  }
}

/* Marks vertices that can be extended. Only vertices that are part of a border can be extended. */
static void reset_extendability_flags(UVIsland &island)
{
  for (UVVert &uv_vert : island.uv_verts) {
    uv_vert.flags.is_border = false;
    uv_vert.flags.is_extended = false;
  }
  for (const UVBorder &border : island.borders) {
    for (const UVBorderEdge &border_edge : border.edges) {
      const UVEdge &uv_edge = island.uv_edges[border_edge.uv_edge_i];
      island.uv_verts[uv_edge.verts[0]].flags.is_border = true;
      island.uv_verts[uv_edge.verts[1]].flags.is_border = true;
    }
  }
}

void UVIsland::extend_border(const MeshData &mesh_data,
                             const UVIslandsMask &mask,
                             const short island_index)
{
  PRF_scope(ProfileCategory::Editor);
  reset_extendability_flags(*this);

  /* Set up double linked list and stable order ID. */
  int64_t order_counter = 0;
  for (const int64_t border_index : borders.index_range()) {
    borders[border_index].setup_links(border_index);
    for (UVBorderEdge &border_edge : borders[border_index].edges) {
      border_edge.order = order_counter++;
    }
  }

  /* Process the border ordered from smallest to largest angle. On high poly meshes this
   * border can be very long, so use priority queue to avoid quadratic time complexity. */
  struct HeapEntry {
    float angle;
    UVBorderEdge *edge;

    bool operator<(const HeapEntry &other) const
    {
      if (angle != other.angle) {
        return angle > other.angle;
      }
      return edge->order > other.edge->order;
    }
  };
  std::priority_queue<HeapEntry> queue;

  /* Queue initial border edges. */
  for (UVBorder &border : borders) {
    for (UVBorderEdge &border_edge : border.edges) {
      queue.push({border.outside_angle(*this, border_edge), &border_edge});
    }
  }

  /* Process border edges in order. */
  Vector<UVBorderEdge *> modified_edges;
  while (!queue.empty()) {
    const HeapEntry entry = queue.top();
    queue.pop();
    UVBorderEdge *border_edge = entry.edge;
    if (!border_edge->is_extendable(*this)) {
      continue;
    }

    const int uv_vert_i = border_edge->get_uv_vert(*this, 0);
    float2 uv_vert_uv = uv_verts[uv_vert_i].uv;
    UVBorder &border = borders[border_edge->border_index];

    /* If the angle changed, re-queue with new angle. */
    const float angle = border.outside_angle(*this, *border_edge);
    if (angle != entry.angle) {
      queue.push({angle, border_edge});
      continue;
    }

    UVBorderCorner extension_corner(border_edge->prev, border_edge, angle);

    /* Found corner is outside the mask, the corner should not be considered for extension. */
    const UVIslandsMask::Tile *tile = mask.find_tile(uv_vert_uv);
    if (tile && tile->is_masked(island_index, uv_vert_uv)) {
      modified_edges.clear();
      extend_at_vert(mesh_data,
                     *this,
                     extension_corner,
                     tile->get_pixel_size_in_uv_space() * 2.0f,
                     order_counter,
                     modified_edges);

      /* Queue modified and newly generated edges. */
      for (UVBorderEdge *modified_edge : modified_edges) {
        if (modified_edge->is_extendable(*this)) {
          UVBorder &modified_border = borders[modified_edge->border_index];
          queue.push({modified_border.outside_angle(*this, *modified_edge), modified_edge});
        }
      }
    }
    /* Mark that the vert is extended. */
    this->uv_verts[uv_vert_i].flags.is_extended = true;
  }
}

void UVIsland::print_debug(const MeshData &mesh_data) const
{
  std::stringstream ss;
  ss << "#### Start UVIsland ####\n";
  ss << "import bpy\n";
  ss << "import bpy_extras.object_utils\n";
  ss << "import mathutils\n";

  ss << "uvisland_vertices = [\n";
  for (const float3 &vert_position : mesh_data.vert_positions) {
    ss << "  mathutils.Vector((" << vert_position.x << ", " << vert_position.y << ", "
       << vert_position.z << ")),\n";
  }
  ss << "]\n";

  ss << "uvisland_edges = []\n";

  ss << "uvisland_faces = [\n";
  for (const UVPrimitive &uvprimitive : uv_primitives) {
    ss << "  [" << this->uv_verts[this->uv_edges[uvprimitive.edges[0]].verts[0]].vert << ", "
       << this->uv_verts[this->uv_edges[uvprimitive.edges[0]].verts[1]].vert << ", "
       << this->uv_verts[uvprimitive.get_other_uv_vert(
                             *this,
                             this->uv_edges[uvprimitive.edges[0]].verts[0],
                             this->uv_edges[uvprimitive.edges[0]].verts[1])]
              .vert
       << "],\n";
  }
  ss << "]\n";

  ss << "uvisland_uvs = [\n";
  for (const UVPrimitive &uvprimitive : uv_primitives) {
    float2 uv = this->uv_verts[this->uv_edges[uvprimitive.edges[0]].verts[0]].uv;
    ss << "  " << uv.x << ", " << uv.y << ",\n";
    uv = this->uv_verts[this->uv_edges[uvprimitive.edges[0]].verts[1]].uv;
    ss << "  " << uv.x << ", " << uv.y << ",\n";
    uv = this->uv_verts[uvprimitive.get_other_uv_vert(
                            *this,
                            this->uv_edges[uvprimitive.edges[0]].verts[0],
                            this->uv_edges[uvprimitive.edges[0]].verts[1])]
             .uv;
    ss << "  " << uv.x << ", " << uv.y << ",\n";
  }
  ss << "]\n";

  ss << "uvisland_mesh = bpy.data.meshes.new(name='UVIsland')\n";
  ss << "uvisland_mesh.from_pydata(uvisland_vertices, uvisland_edges, uvisland_faces)\n";
  ss << "uv_map = uvisland_mesh.attributes.new('UVMap', 'FLOAT2', 'CORNER')\n";
  ss << "uv_map.data.foreach_set('vector', uvisland_uvs)\n";
  ss << "bpy_extras.object_utils.object_data_add(bpy.context, uvisland_mesh)\n";
  ss << "#### End UVIsland ####\n\n\n";

  std::cout << ss.str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVBorder
 * \{ */

std::optional<UVBorder> extract_border_from_edges(const UVIsland &island,
                                                  MutableSpan<UVBorderEdge> edges,
                                                  MutableBoundedBitSpan borders_used)
{
  /* Find a part of the border that haven't been extracted yet. */
  const std::optional<int64_t> start_index = bits::find_first_0_index(borders_used);
  if (!start_index) {
    return std::nullopt;
  }
  UVBorderEdge &start_edge = edges[*start_index];
  UVBorder border;
  border.edges.append(start_edge);
  borders_used[*start_index].set();

  float2 first_uv = island.uv_verts[start_edge.get_uv_vert(island, 0)].uv;
  float2 current_uv = island.uv_verts[start_edge.get_uv_vert(island, 1)].uv;
  while (current_uv != first_uv) {
    bool edge_added = false;
    for (const int edge_i : edges.index_range()) {
      if (borders_used[edge_i].test()) {
        continue;
      }
      UVBorderEdge &border_edge = edges[edge_i];
      int i;
      for (i = 0; i < 2; i++) {
        if (island.uv_verts[island.uv_edges[border_edge.uv_edge_i].verts[i]].uv == current_uv) {
          border_edge.reverse_order = i == 1;
          borders_used[edge_i].set();
          current_uv = island.uv_verts[border_edge.get_uv_vert(island, 1)].uv;
          border.edges.append(border_edge);
          edge_added = true;
          break;
        }
      }
      if (i != 2) {
        break;
      }
    }
    if (!edge_added) {
      /* TODO Add a user-facing warning to notify users that the model's UVs are invalid for
       * texture painting and should be fixed for optimal results. */
      break;
    }
  }
  return border;
}

bool UVBorder::is_ccw(const UVIsland &island) const
{
  const UVBorderEdge &edge = edges.first();
  const int uv_vert1_i = edge.get_uv_vert(island, 0);
  const int uv_vert2_i = edge.get_uv_vert(island, 1);
  const int uv_vert3_i = edge.get_other_uv_vert(island);
  float poly[3][2];
  copy_v2_v2(poly[0], island.uv_verts[uv_vert1_i].uv);
  copy_v2_v2(poly[1], island.uv_verts[uv_vert2_i].uv);
  copy_v2_v2(poly[2], island.uv_verts[uv_vert3_i].uv);
  const bool ccw = cross_poly_v2(poly, 3) > 0.0;
  return ccw;
}

void UVBorder::flip_order()
{
  for (UVBorderEdge &edge : edges) {
    edge.reverse_order = !edge.reverse_order;
  }
  std::reverse(edges.begin(), edges.end());
}

float UVBorder::outside_angle(const UVIsland &island, const UVBorderEdge &edge) const
{
  const UVBorderEdge &prev = *edge.prev;
  return M_PI - angle_signed_v2v2(island.uv_verts[prev.get_uv_vert(island, 1)].uv -
                                      island.uv_verts[prev.get_uv_vert(island, 0)].uv,
                                  island.uv_verts[edge.get_uv_vert(island, 1)].uv -
                                      island.uv_verts[edge.get_uv_vert(island, 0)].uv);
}

void UVBorder::setup_links(const int64_t border_index)
{
  const int64_t n = edges.size();
  for (int64_t i = 0; i < n; i++) {
    edges[i].prev = &edges[(i - 1 + n) % n];
    edges[i].next = &edges[(i + 1) % n];
    edges[i].border_index = border_index;
    edges[i].removed = false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVBorderCorner
 * \{ */

UVBorderCorner::UVBorderCorner(UVBorderEdge *first, UVBorderEdge *second, float angle)
    : first(first), second(second), angle(angle)
{
}

float2 UVBorderCorner::uv(const UVIsland &island, float factor, float min_uv_distance)
{
  using namespace blender::math;
  float2 origin = island.uv_verts[first->get_uv_vert(island, 1)].uv;
  float angle_between = angle * factor;
  float desired_len = max_ff(
      second->length(island) * factor + first->length(island) * (1.0 - factor), min_uv_distance);
  float2 v = normalize(island.uv_verts[first->get_uv_vert(island, 0)].uv - origin);

  float2x2 rot_mat = from_rotation<float2x2>(AngleRadian(angle_between));
  float2 rotated = rot_mat * v;
  float2 result = rotated * desired_len + island.uv_verts[first->get_uv_vert(island, 1)].uv;
  return result;
}

bool UVBorderCorner::connected_in_mesh(const UVIsland &island) const
{
  return island.uv_verts[first->get_uv_vert(island, 1)].vert ==
         island.uv_verts[second->get_uv_vert(island, 0)].vert;
}

void UVBorderCorner::print_debug(const UVIsland &island) const
{
  std::stringstream ss;
  ss << "# ";
  if (connected_in_mesh(island)) {
    ss << island.uv_verts[first->get_uv_vert(island, 0)].vert << "-";
    ss << island.uv_verts[first->get_uv_vert(island, 1)].vert << "-";
    ss << island.uv_verts[second->get_uv_vert(island, 1)].vert << "\n";
  }
  else {
    ss << island.uv_verts[first->get_uv_vert(island, 0)].vert << "-";
    ss << island.uv_verts[first->get_uv_vert(island, 1)].vert << ", ";
    ss << island.uv_verts[second->get_uv_vert(island, 0)].vert << "-";
    ss << island.uv_verts[second->get_uv_vert(island, 1)].vert << "\n";
  }
  std::cout << ss.str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVPrimitive
 * \{ */

UVPrimitive::UVPrimitive(const int primitive_i) : primitive_i(primitive_i) {}

int UVPrimitive::get_uv_vert(const UVIsland &island,
                             const MeshData &mesh_data,
                             const uint8_t mesh_vert_index) const
{
  const int3 &tri = mesh_data.corner_tris[this->primitive_i];
  const int mesh_vert = mesh_data.corner_verts[tri[mesh_vert_index]];
  for (const int uv_edge_i : edges) {
    for (const int uv_vert_i : island.uv_edges[uv_edge_i].verts) {
      const UVVert &uv_vert = island.uv_verts[uv_vert_i];
      if (uv_vert.vert == mesh_vert) {
        return uv_vert_i;
      }
    }
  }
  BLI_assert_unreachable();
  return -1;
}

int UVPrimitive::get_uv_edge(const UVIsland &island, const float2 uv1, const float2 uv2) const
{
  for (const int uv_edge_i : edges) {
    const float2 &e1 = island.uv_verts[island.uv_edges[uv_edge_i].verts[0]].uv;
    const float2 &e2 = island.uv_verts[island.uv_edges[uv_edge_i].verts[1]].uv;
    if ((e1 == uv1 && e2 == uv2) || (e1 == uv2 && e2 == uv1)) {
      return uv_edge_i;
    }
  }
  BLI_assert_unreachable();
  return -1;
}

int UVPrimitive::get_uv_edge(const UVIsland &island, const int v1, const int v2) const
{
  for (int uv_edge_i : edges) {
    const int e1 = island.uv_verts[island.uv_edges[uv_edge_i].verts[0]].vert;
    const int e2 = island.uv_verts[island.uv_edges[uv_edge_i].verts[1]].vert;
    if ((e1 == v1 && e2 == v2) || (e1 == v2 && e2 == v1)) {
      return uv_edge_i;
    }
  }
  BLI_assert_unreachable();
  return -1;
}

bool UVPrimitive::contains_uv_vert(const UVIsland &island, const int uv_vert_i) const
{
  for (int uv_edge_i : edges) {
    const UVEdge &edge = island.uv_edges[uv_edge_i];
    if (std::find(edge.verts.begin(), edge.verts.end(), uv_vert_i) != edge.verts.end()) {
      return true;
    }
  }
  return false;
}

int UVPrimitive::get_other_uv_vert(const UVIsland &island, const int v1, const int v2) const
{
  BLI_assert(contains_uv_vert(island, v1));
  BLI_assert(contains_uv_vert(island, v2));

  for (int uv_edge_i : edges) {
    const UVEdge &edge = island.uv_edges[uv_edge_i];
    for (const int uv_vert_i : edge.verts) {
      if (!ELEM(uv_vert_i, v1, v2)) {
        return uv_vert_i;
      }
    }
  }
  BLI_assert_unreachable();
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVBorderEdge
 * \{ */
UVBorderEdge::UVBorderEdge(const int uv_edge_i, int uv_primitive)
    : uv_edge_i(uv_edge_i), uv_primitive(uv_primitive)
{
}

int UVBorderEdge::get_uv_vert(const UVIsland &island, int index) const
{
  const UVEdge &edge = island.uv_edges[uv_edge_i];
  int actual_index = reverse_order ? 1 - index : index;
  return edge.verts[actual_index];
}

int UVBorderEdge::get_other_uv_vert(const UVIsland &island) const
{
  const UVEdge &edge = island.uv_edges[uv_edge_i];
  return island.uv_primitives[uv_primitive].get_other_uv_vert(
      island, edge.verts[0], edge.verts[1]);
}

bool UVBorderEdge::is_extendable(const UVIsland &island) const
{
  if (removed) {
    return false;
  }
  const int uv_vert_i = get_uv_vert(island, 0);
  const UVVert &uv_vert = island.uv_verts[uv_vert_i];
  return uv_vert.flags.is_border && !uv_vert.flags.is_extended;
}

float UVBorderEdge::length(const UVIsland &island) const
{
  const UVEdge &edge = island.uv_edges[uv_edge_i];
  return len_v2v2(island.uv_verts[edge.verts[0]].uv, island.uv_verts[edge.verts[1]].uv);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV islands
 * \{ */

/** Find which triangles are near a UV island border, to only build island data
 * structures for those. This includes triangles that have either an edge or a
 * a vertex on the border. */
static void find_triangles_near_border(const MeshData &mesh_data,
                                       MutableSpan<bool> r_tri_corner_near_border,
                                       MutableSpan<bool> r_tri_near_border)
{
  const Span<int3> corner_tris = mesh_data.corner_tris;
  const Span<bool> edge_is_border = mesh_data.uv_edge_is_border;
  const int64_t tris_num = corner_tris.size();

  Array<bool> vert_is_border(mesh_data.vert_positions.size(), false);

  /* Mark all triangle corners and vertices on the border. */
  threading::parallel_for(IndexRange(tris_num), 2048, [&](const IndexRange range) {
    for (const int64_t tri_index : range) {
      const int3 real_edges = mesh::corner_tri_get_real_edges(mesh_data.mesh_edges,
                                                              mesh_data.corner_verts,
                                                              mesh_data.corner_edges,
                                                              corner_tris[tri_index]);
      for (int j = 0; j < 3; j++) {
        const int edge_i = real_edges[j];
        if (edge_i != -1 && edge_is_border[edge_i]) {
          const int2 edge = mesh_data.mesh_edges[edge_i];
          vert_is_border[edge[0]] = true;
          vert_is_border[edge[1]] = true;
          r_tri_corner_near_border[tri_index * 3 + j] = true;
        }
      }
    }
  });

  /* Mark triangles near the border. */
  threading::parallel_for(IndexRange(tris_num), 4096, [&](const IndexRange range) {
    for (const int64_t tri_index : range) {
      const int3 &tri = corner_tris[tri_index];
      r_tri_near_border[tri_index] = vert_is_border[mesh_data.corner_verts[tri[0]]] ||
                                     vert_is_border[mesh_data.corner_verts[tri[1]]] ||
                                     vert_is_border[mesh_data.corner_verts[tri[2]]];
    }
  });
}

Array<UVIsland> build_uv_islands(const MeshData &mesh_data,
                                 const GroupedSpan<int> tris_by_island,
                                 const UVIslandsMask &uv_masks)
{
  PRF_scope(ProfileCategory::Editor);

  const int64_t tris_num = mesh_data.corner_tris.size();
  Array<bool> tri_corner_near_border(tris_num * 3, false);
  Array<bool> tri_near_border(tris_num);
  find_triangles_near_border(mesh_data, tri_corner_near_border, tri_near_border);

  Array<UVIsland> islands(tris_by_island.size());

  /* Add primitives near the island border to each island. Interior primitives are not
   * stored, their UVs are read directly from the mesh where needed. */
  threading::parallel_for(islands.index_range(), 1, [&](const IndexRange range) {
    for (const int64_t uv_island_id : range) {
      UVIsland &uv_island = islands[uv_island_id];
      uv_island.id = uv_island_id;
      for (const int primitive_i : tris_by_island[uv_island_id]) {
        if (tri_near_border[primitive_i]) {
          add_primitive(mesh_data, uv_island, primitive_i, tri_corner_near_border);
        }
      }
      uv_island.num_original_primitives = uv_island.uv_primitives.size();
      uv_island.extract_borders();
      uv_island.extend_border(mesh_data, uv_masks, short(uv_island_id));
    }
  });

  return islands;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVIslandsMask
 * \{ */

static ushort2 mask_resolution_from_tile_resolution(ushort2 tile_resolution)
{
  return ushort2(max_ii(tile_resolution.x >> 2, 256), max_ii(tile_resolution.y >> 2, 256));
}

UVIslandsMask::Tile::Tile(float2 udim_offset, ushort2 tile_resolution)
    : udim_offset(udim_offset),
      tile_resolution(tile_resolution),
      mask_resolution(mask_resolution_from_tile_resolution(tile_resolution)),
      mask(mask_resolution.x * mask_resolution.y)
{
  mask.fill(0xffff);
}

bool UVIslandsMask::Tile::contains(const float2 uv) const
{
  const float2 tile_uv = uv - udim_offset;
  return IN_RANGE_INCL(tile_uv.x, 0.0f, 1.0f) && IN_RANGE_INCL(tile_uv.y, 0.0f, 1.0f);
}

float UVIslandsMask::Tile::get_pixel_size_in_uv_space() const
{
  return min_ff(1.0f / tile_resolution.x, 1.0f / tile_resolution.y);
}

static void add_uv_island(const MeshData &mesh_data,
                          UVIslandsMask::Tile &tile,
                          const Span<int> tris,
                          int16_t island_index)
{
  PRF_scope(ProfileCategory::Editor);
  const float resolution_x = float(tile.mask_resolution.x);
  const float resolution_y = float(tile.mask_resolution.y);
  for (const int tri_i : tris) {
    const int3 &tri = mesh_data.corner_tris[tri_i];

    /* Transform UV coordinates to pixel space in the tile. */
    const float2 uv0 = mesh_data.uv_map[tri[0]];
    const float2 uv1 = mesh_data.uv_map[tri[1]];
    const float2 uv2 = mesh_data.uv_map[tri[2]];
    const float2 resolution = {resolution_x, resolution_y};
    const float2 p0 = (uv0 - tile.udim_offset) * resolution;
    const float2 p1 = (uv1 - tile.udim_offset) * resolution;
    const float2 p2 = (uv2 - tile.udim_offset) * resolution;

    /* Compute bounds within tile. */
    const float2 pmin = math::min(math::min(p0, p1), p2);
    const float2 pmax = math::max(math::max(p0, p1), p2);
    const int xmin = max_ii(int(floorf(pmin.x)), 0);
    const int xmax = min_ii(int(ceilf(pmax.x)), tile.mask_resolution.x - 1);
    const int ymin = max_ii(int(floorf(pmin.y)), 0);
    const int ymax = min_ii(int(ceilf(pmax.y)), tile.mask_resolution.y - 1);
    if (xmin > xmax || ymin > ymax) {
      continue;
    }

    /* Rasterize. */
    const TriRasterizer rasterizer(p0, p1, p2);
    for (int y = ymin; y <= ymax; y++) {
      for (int x = xmin; x <= xmax; x++) {
        if (rasterizer.inside(x, y)) {
          tile.mask[int64_t(tile.mask_resolution.x) * y + x] = island_index;
        }
      }
    }
  }
}

void UVIslandsMask::add(const MeshData &mesh_data, const GroupedSpan<int> tris_by_island)
{
  PRF_scope(ProfileCategory::Editor);

  threading::parallel_for(IndexRange(tiles.size()), 1, [&](const IndexRange range) {
    for (const int tile_index : range) {
      for (const int i : tris_by_island.index_range()) {
        add_uv_island(mesh_data, tiles[tile_index], tris_by_island[i], i);
      }
    }
  });
}

void UVIslandsMask::add_tile(const float2 udim_offset, ushort2 resolution)
{
  tiles.append_as(Tile(udim_offset, resolution));
}

static bool dilate_x(const Span<uint16_t> prev_mask,
                     MutableSpan<uint16_t> mask,
                     const ushort2 resolution)
{
  std::atomic<bool> changed = false;
  threading::parallel_for(IndexRange(resolution.y), 32, [&](const IndexRange y_range) {
    bool local_changed = false;
    for (const int y : y_range) {
      const int row = y * resolution.x;
      for (int x = 0; x < resolution.x; x++) {
        const int offset = row + x;
        uint16_t value = prev_mask[offset];
        if (value == 0xffff) {
          if (x != 0 && prev_mask[offset - 1] != 0xffff) {
            value = prev_mask[offset - 1];
            local_changed = true;
          }
          else if (x < resolution.x - 1 && prev_mask[offset + 1] != 0xffff) {
            value = prev_mask[offset + 1];
            local_changed = true;
          }
        }
        mask[offset] = value;
      }
    }
    if (local_changed) {
      changed = true;
    }
  });
  return changed;
}

static bool dilate_y(const Span<uint16_t> prev_mask,
                     MutableSpan<uint16_t> mask,
                     const ushort2 resolution)
{
  std::atomic<bool> changed = false;
  threading::parallel_for(IndexRange(resolution.y), 32, [&](const IndexRange y_range) {
    bool local_changed = false;
    for (const int y : y_range) {
      const int row = y * resolution.x;
      for (int x = 0; x < resolution.x; x++) {
        const int offset = row + x;
        uint16_t value = prev_mask[offset];
        if (value == 0xffff) {
          if (y != 0 && prev_mask[offset - resolution.x] != 0xffff) {
            value = prev_mask[offset - resolution.x];
            local_changed = true;
          }
          else if (y < resolution.y - 1 && prev_mask[offset + resolution.x] != 0xffff) {
            value = prev_mask[offset + resolution.x];
            local_changed = true;
          }
        }
        mask[offset] = value;
      }
    }
    if (local_changed) {
      changed = true;
    }
  });
  return changed;
}

static void dilate_tile(UVIslandsMask::Tile &tile, int max_iterations)
{
  PRF_scope(ProfileCategory::Editor);

  /* Ping-pong between the mask and a scratch buffer for multithreading. */
  Array<uint16_t> scratch_buffer(tile.mask.size());
  MutableSpan<uint16_t> mask = tile.mask;
  MutableSpan<uint16_t> scratch_mask = scratch_buffer;
  for (int index = 0; index < max_iterations; index++) {
    bool changed = dilate_x(mask, scratch_mask, tile.mask_resolution);
    changed |= dilate_y(scratch_mask, mask, tile.mask_resolution);
    if (!changed) {
      break;
    }
  }
}

void UVIslandsMask::dilate(int max_iterations)
{
  PRF_scope(ProfileCategory::Editor);
  threading::parallel_for(tiles.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      dilate_tile(tiles[i], max_iterations);
    }
  });
}

bool UVIslandsMask::Tile::is_masked(const uint16_t island_index, const float2 uv) const
{
  float2 local_uv = uv - udim_offset;
  if (local_uv.x < 0.0f || local_uv.y < 0.0f || local_uv.x > 1.0f || local_uv.y > 1.0f) {
    return false;
  }
  float2 pixel_pos_f = local_uv * float2(mask_resolution.x, mask_resolution.y);
  ushort2 pixel_pos = ushort2(clamp_i(pixel_pos_f.x, 0, mask_resolution.x - 1),
                              clamp_i(pixel_pos_f.y, 0, mask_resolution.y - 1));
  uint64_t offset = pixel_pos.y * mask_resolution.x + pixel_pos.x;
  return mask[offset] == island_index;
}

const UVIslandsMask::Tile *UVIslandsMask::find_tile(const float2 uv) const
{
  for (const Tile &tile : tiles) {
    if (tile.contains(uv)) {
      return &tile;
    }
  }
  return nullptr;
}

bool UVIslandsMask::is_masked(const uint16_t island_index, const float2 uv) const
{
  const Tile *tile = find_tile(uv);
  if (tile == nullptr) {
    return false;
  }
  return tile->is_masked(island_index, uv);
}

/** \} */

}  // namespace blender::bke::pbvh::uv_islands
