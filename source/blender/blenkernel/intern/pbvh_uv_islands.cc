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
#include "BLI_ordered_edge.hh"
#include "BLI_task.hh"

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

static void uv_edge_append_to_uv_verts(UVEdge &uv_edge)
{
  for (UVVert *vert : uv_edge.verts) {
    vert->uv_edges.append_non_duplicates(&uv_edge);
  }
}

static void uv_primitive_append_to_uv_edges(UVPrimitive &uv_primitive)
{
  for (UVEdge *uv_edge : uv_primitive.edges) {
    uv_edge->uv_primitive_indices.append_non_duplicates(uv_primitive.primitive_i);
  }
}

static void uv_primitive_append_to_uv_verts(UVPrimitive &uv_primitive)
{
  for (UVEdge *uv_edge : uv_primitive.edges) {
    uv_edge_append_to_uv_verts(*uv_edge);
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

static bool primitive_has_shared_uv_edge(const Span<float2> uv_map,
                                         const int3 &tri,
                                         const int3 &tri_other)
{
  int shared_uv_verts = 0;
  for (const int corner : {tri[0], tri[1], tri[2]}) {
    for (const int other_corner : {tri_other[0], tri_other[1], tri_other[2]}) {
      if (uv_map[corner] == uv_map[other_corner]) {
        shared_uv_verts += 1;
      }
    }
  }
  return shared_uv_verts >= 2;
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

static GroupedSpan<int> build_edge_to_primitive_map(const TriangleToEdgeMap &prim_to_edge,
                                                    const int edges_num,
                                                    const int prims_num,
                                                    Array<int> &r_offsets,
                                                    Array<int> &r_indices)
{
  r_offsets = Array<int>(edges_num + 1, 0);
  for (const int prim_i : IndexRange(prims_num)) {
    for (const int edge_i : prim_to_edge[prim_i]) {
      r_offsets[edge_i]++;
    }
  }
  const OffsetIndices<int> offsets = offset_indices::accumulate_counts_to_offsets(r_offsets);
  r_indices.reinitialize(offsets.total_size());
  Array<int> pos(r_offsets.as_span().drop_back(1));
  for (const int prim_i : IndexRange(prims_num)) {
    for (const int edge_i : prim_to_edge[prim_i]) {
      r_indices[pos[edge_i]++] = prim_i;
    }
  }
  return {offsets, r_indices};
}

static void mesh_data_init_edges(MeshData &mesh_data)
{
  mesh_data.edges.reserve(mesh_data.corner_tris.size() * 2);
  Map<OrderedEdge, int> eh;
  eh.reserve(mesh_data.corner_tris.size() * 3);
  for (int64_t tri_index = 0; tri_index < mesh_data.corner_tris.size(); tri_index++) {
    const int3 &tri = mesh_data.corner_tris[tri_index];
    Vector<int, 3> tri_edges;
    for (int j = 0; j < 3; j++) {
      int v1 = mesh_data.corner_verts[tri[j]];
      int v2 = mesh_data.corner_verts[tri[(j + 1) % 3]];

      int64_t edge_index = mesh_data.edges.size();
      const bool is_new_edge = eh.add({v1, v2}, edge_index);
      if (is_new_edge) {
        mesh_data.edges.append({v1, v2});
      }
      else {
        edge_index = eh.lookup({v1, v2});
      }

      tri_edges.append(edge_index);
    }
    mesh_data.primitive_to_edge_map.add(tri_edges, tri_index);
  }
  mesh_data.vert_to_edge_map = mesh::build_vert_to_edge_map(mesh_data.edges,
                                                            mesh_data.vert_positions.size(),
                                                            mesh_data.vert_to_edge_offsets,
                                                            mesh_data.vert_to_edge_indices);
  mesh_data.edge_to_primitive_map = build_edge_to_primitive_map(
      mesh_data.primitive_to_edge_map,
      mesh_data.edges.size(),
      mesh_data.corner_tris.size(),
      mesh_data.edge_to_primitive_offsets,
      mesh_data.edge_to_primitive_indices);
}

static int mesh_data_init_primitive_uv_island_ids(MeshData &mesh_data)
{
  /* Group primitives into UV islands, connected through shared UV edges. */
  const int64_t primitives_num = mesh_data.corner_tris.size();
  mesh_data.uv_island_ids.reinitialize(primitives_num);

  AtomicDisjointSet disjoint_set(primitives_num);
  threading::parallel_for(IndexRange(primitives_num), 1024, [&](const IndexRange range) {
    for (const int primitive_i : range) {
      for (const int edge : mesh_data.primitive_to_edge_map[primitive_i]) {
        for (const int other_primitive_i : mesh_data.edge_to_primitive_map[edge]) {
          /* Join each pair once. */
          if (other_primitive_i <= primitive_i) {
            continue;
          }
          if (primitive_has_shared_uv_edge(mesh_data.uv_map,
                                           mesh_data.corner_tris[primitive_i],
                                           mesh_data.corner_tris[other_primitive_i]))
          {
            disjoint_set.join(primitive_i, other_primitive_i);
          }
        }
      }
    }
  });

  return disjoint_set.calc_reduced_ids(mesh_data.uv_island_ids);
}

static void mesh_data_init(MeshData &mesh_data)
{
  PRF_scope(ProfileCategory::Editor);
  mesh_data_init_edges(mesh_data);
  mesh_data.uv_island_len = mesh_data_init_primitive_uv_island_ids(mesh_data);
}

MeshData::MeshData(const OffsetIndices<int> faces,
                   const Span<int3> corner_tris,
                   const Span<int> corner_verts,
                   const Span<float2> uv_map,
                   const Span<float3> vert_positions)
    : faces(faces),
      corner_tris(corner_tris),
      corner_verts(corner_verts),
      uv_map(uv_map),
      vert_positions(vert_positions),
      primitive_to_edge_map(corner_tris.size())
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

bool UVEdge::has_same_verts(const int vert1, const int vert2) const
{
  return (verts[0]->vert == vert1 && verts[1]->vert == vert2) ||
         (verts[0]->vert == vert2 && verts[1]->vert == vert1);
}

bool UVEdge::has_same_verts(const int2 &edge) const
{
  return has_same_verts(edge[0], edge[1]);
}

bool UVEdge::is_border_edge() const
{
  return uv_primitive_indices.size() == 1;
}

UVVert *UVEdge::get_other_uv_vert(const int vert)
{
  if (verts[0]->vert == vert) {
    return verts[1];
  }
  return verts[0];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVIsland
 * \{ */
UVVert *UVIsland::lookup(const UVVert &vert)
{
  const int vert_index = vert.vert;
  const Vector<UVVert *> &verts = uv_vert_lookup.lookup_or_add_default(vert_index);
  for (UVVert *v : verts) {
    if (v->uv == vert.uv) {
      return v;
    }
  }
  return nullptr;
}

UVVert *UVIsland::lookup_or_create(const UVVert &vert)
{
  UVVert *found_vert = lookup(vert);
  if (found_vert != nullptr) {
    return found_vert;
  }

  uv_verts.append(vert);
  UVVert *result = &uv_verts.last();
  result->uv_edges.clear();
  /* v is already a key. Ensured by UVIsland::lookup in this method. */
  uv_vert_lookup.lookup(vert.vert).append(result);
  return result;
}

UVEdge *UVIsland::lookup(const UVEdge &edge)
{
  UVVert *found_vert = lookup(*edge.verts[0]);
  if (found_vert == nullptr) {
    return nullptr;
  }
  for (UVEdge *e : found_vert->uv_edges) {
    UVVert *other_vert = e->get_other_uv_vert(found_vert->vert);
    if (other_vert->vert == edge.verts[1]->vert && other_vert->uv == edge.verts[1]->uv) {
      return e;
    }
  }
  return nullptr;
}

UVEdge *UVIsland::lookup_or_create(const UVEdge &edge)
{
  UVEdge *found_edge = lookup(edge);
  if (found_edge != nullptr) {
    return found_edge;
  }

  uv_edges.append(edge);
  UVEdge *result = &uv_edges.last();
  result->uv_primitive_indices.clear();
  return result;
}

static UVPrimitive *add_primitive(const MeshData &mesh_data,
                                  UVIsland &uv_island,
                                  const int primitive_i)
{
  UVPrimitive uv_primitive(primitive_i);
  const int3 &tri = mesh_data.corner_tris[primitive_i];
  uv_island.uv_primitives.append(uv_primitive);
  UVPrimitive *uv_primitive_ptr = &uv_island.uv_primitives.last();
  for (const int edge_i : mesh_data.primitive_to_edge_map[primitive_i]) {
    const int2 &edge = mesh_data.edges[edge_i];
    const int corner_1 = get_uv_corner(mesh_data, tri, edge[0]);
    const int corner_2 = get_uv_corner(mesh_data, tri, edge[1]);
    UVEdge uv_edge_template;
    uv_edge_template.verts[0] = uv_island.lookup_or_create(UVVert(mesh_data, corner_1));
    uv_edge_template.verts[1] = uv_island.lookup_or_create(UVVert(mesh_data, corner_2));
    UVEdge *uv_edge = uv_island.lookup_or_create(uv_edge_template);
    uv_primitive_ptr->edges.append(uv_edge);
    uv_edge_append_to_uv_verts(*uv_edge);
    uv_edge->uv_primitive_indices.append(uv_primitive_ptr->primitive_i);
  }
  return uv_primitive_ptr;
}

std::optional<UVBorder> extract_border_from_edges(MutableSpan<UVBorderEdge> edges,
                                                  MutableBoundedBitSpan borders_used);

void UVIsland::extract_borders()
{
  PRF_scope(ProfileCategory::Editor);
  /* Lookup all borders of the island. */
  Vector<UVBorderEdge> edges;
  for (const int uv_prim_i : uv_primitives.index_range()) {
    const UVPrimitive &prim = uv_primitives[uv_prim_i];
    for (UVEdge *edge : prim.edges) {
      if (edge->is_border_edge()) {
        edges.append(UVBorderEdge(edge, uv_prim_i));
      }
    }
  }

  BitVector<128> borders_used(edges.size(), false);
  while (true) {
    std::optional<UVBorder> border = extract_border_from_edges(edges, borders_used);
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

  Fan(const MeshData &mesh_data, const int vert)
  {
    flags.is_manifold = true;
    int current_edge = mesh_data.vert_to_edge_map[vert].first();
    const int stop_primitive = mesh_data.edge_to_primitive_map[current_edge].first();
    int previous_primitive = stop_primitive;
    while (true) {
      bool stop = false;
      if (!mesh_data.is_edge_manifold(current_edge)) {
        flags.is_manifold = false;
        break;
      }
      for (const int other_primitive_i : mesh_data.edge_to_primitive_map[current_edge]) {
        if (stop) {
          break;
        }
        if (other_primitive_i == previous_primitive) {
          continue;
        }

        const int3 &other_tri = mesh_data.corner_tris[other_primitive_i];

        for (const int edge_i : mesh_data.primitive_to_edge_map[other_primitive_i]) {
          const int2 &edge = mesh_data.edges[edge_i];
          if (edge_i == current_edge || (edge[0] != vert && edge[1] != vert)) {
            continue;
          }
          segments.append(FanSegment(mesh_data, other_primitive_i, other_tri, vert));
          current_edge = edge_i;
          previous_primitive = other_primitive_i;
          stop = true;
          break;
        }
      }
      if (stop == false) {
        flags.is_manifold = false;
        break;
      }
      if (stop_primitive == previous_primitive) {
        break;
      }
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

  void mark_already_added_segments(const UVVert &uv_vert)
  {
    /* Go over all fan edges to find if they can be found as primitive around the uv vertex. */
    for (FanSegment &fan_edge : segments) {
      fan_edge.flags.found = false;
      for (const UVEdge *uv_edge : uv_vert.uv_edges) {
        if (uv_edge->uv_primitive_indices.contains(fan_edge.primitive_index)) {
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
                                            UVVert *connected_vert_1,
                                            UVVert *connected_vert_2,
                                            float2 uv_unconnected,
                                            const int mesh_primitive_i)
{
  UVPrimitive prim1(mesh_primitive_i);
  const int3 &tri = mesh_data.corner_tris[mesh_primitive_i];

  const int other_vert_i = primitive_get_other_uv_vert(
      mesh_data, tri, connected_vert_1->vert, connected_vert_2->vert);
  UVVert vert_template;
  vert_template.uv = uv_unconnected;
  vert_template.vert = other_vert_i;
  UVVert *vert_ptr = island.lookup_or_create(vert_template);

  const int corner_1 = get_uv_corner(mesh_data, tri, connected_vert_1->vert);
  vert_template.uv = connected_vert_1->uv;
  vert_template.vert = mesh_data.corner_verts[corner_1];
  UVVert *vert_1_ptr = island.lookup_or_create(vert_template);

  const int corner_2 = get_uv_corner(mesh_data, tri, connected_vert_2->vert);
  vert_template.uv = connected_vert_2->uv;
  vert_template.vert = mesh_data.corner_verts[corner_2];
  UVVert *vert_2_ptr = island.lookup_or_create(vert_template);

  UVEdge edge_template;
  edge_template.verts[0] = vert_1_ptr;
  edge_template.verts[1] = vert_2_ptr;
  prim1.edges.append(island.lookup_or_create(edge_template));
  edge_template.verts[0] = vert_2_ptr;
  edge_template.verts[1] = vert_ptr;
  prim1.edges.append(island.lookup_or_create(edge_template));
  edge_template.verts[0] = vert_ptr;
  edge_template.verts[1] = vert_1_ptr;
  prim1.edges.append(island.lookup_or_create(edge_template));
  uv_primitive_append_to_uv_edges(prim1);
  uv_primitive_append_to_uv_verts(prim1);
  island.uv_primitives.append(prim1);
}
/**
 * Find a primitive that can be used to fill give corner.
 * Will return -1 when no primitive can be found.
 */
static int find_fill_primitive(const MeshData &mesh_data, UVBorderCorner &corner)
{
  if (corner.first->get_uv_vert(1) != corner.second->get_uv_vert(0)) {
    return -1;
  }
  if (corner.first->get_uv_vert(0) == corner.second->get_uv_vert(1)) {
    return -1;
  }
  const UVVert *shared_vert = corner.second->get_uv_vert(0);
  for (const int edge_i : mesh_data.vert_to_edge_map[shared_vert->vert]) {
    const int2 &edge = mesh_data.edges[edge_i];
    if (corner.first->edge->has_same_verts(edge)) {
      for (const int primitive_i : mesh_data.edge_to_primitive_map[edge_i]) {
        const int3 &tri = mesh_data.corner_tris[primitive_i];
        const int other_vert = primitive_get_other_uv_vert(mesh_data, tri, edge[0], edge[1]);
        if (other_vert == corner.second->get_uv_vert(1)->vert) {
          return primitive_i;
        }
      }
    }
  }
  return -1;
}

static void add_uv_primitive_fill(UVIsland &island,
                                  UVVert &uv_vert1,
                                  UVVert &uv_vert2,
                                  UVVert &uv_vert3,
                                  const int fill_primitive_i)
{
  UVPrimitive uv_primitive(fill_primitive_i);
  UVEdge edge_template;
  edge_template.verts[0] = &uv_vert1;
  edge_template.verts[1] = &uv_vert2;
  uv_primitive.edges.append(island.lookup_or_create(edge_template));
  edge_template.verts[0] = &uv_vert2;
  edge_template.verts[1] = &uv_vert3;
  uv_primitive.edges.append(island.lookup_or_create(edge_template));
  edge_template.verts[0] = &uv_vert3;
  edge_template.verts[1] = &uv_vert1;
  uv_primitive.edges.append(island.lookup_or_create(edge_template));
  uv_primitive_append_to_uv_edges(uv_primitive);
  uv_primitive_append_to_uv_verts(uv_primitive);
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
  if (!corner.connected_in_mesh()) {
    return;
  }

  UVBorderEdge *after_edge = corner.second->next;

  UVVert *uv_vert = corner.second->get_uv_vert(0);
  Fan fan(mesh_data, uv_vert->vert);
  if (!fan.flags.is_manifold) {
    return;
  }
  fan.mark_already_added_segments(*uv_vert);
  int num_to_add = fan.count_edges_not_added();

  /* In 3d space everything can connected, but in uv space it may not.
   * in this case in the space between we should extract the primitives to be added
   * from the fan. */
  Vector<FanSegment *> winding_solution = fan.best_path_between(
      mesh_data, corner.first->get_uv_vert(0)->vert, corner.second->get_uv_vert(1)->vert);

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
                                     find_fill_primitive(mesh_data, corner);

    if (fill_primitive_i != -1) {
      fill_primitive_1_i = fill_primitive_i;
      fill_primitive_2_i = fill_primitive_i;
    }

    float2 center_uv = corner.uv(0.5f, min_uv_distance);
    add_uv_primitive_shared_uv_edge(mesh_data,
                                    island,
                                    corner.first->get_uv_vert(1),
                                    corner.first->get_uv_vert(0),
                                    center_uv,
                                    fill_primitive_1_i);
    const int new_prim_1_i = island.uv_primitives.size() - 1;
    add_uv_primitive_shared_uv_edge(mesh_data,
                                    island,
                                    corner.second->get_uv_vert(0),
                                    corner.second->get_uv_vert(1),
                                    center_uv,
                                    fill_primitive_2_i);
    const int new_prim_2_i = island.uv_primitives.size() - 1;

    /* Update border after adding the new geometry. */
    {
      UVBorderEdge *border_edge = corner.first;
      border_edge->uv_primitive = new_prim_1_i;
      border_edge->edge = island.uv_primitives[border_edge->uv_primitive].get_uv_edge(
          corner.first->get_uv_vert(0)->uv, center_uv);
      border_edge->reverse_order = border_edge->edge->verts[0]->uv == center_uv;
    }
    {
      UVBorderEdge *border_edge = corner.second;
      border_edge->uv_primitive = new_prim_2_i;
      border_edge->edge = island.uv_primitives[border_edge->uv_primitive].get_uv_edge(
          corner.second->get_uv_vert(1)->uv, center_uv);
      border_edge->reverse_order = border_edge->edge->verts[1]->uv == center_uv;
    }

    r_modified.append(corner.first);
    r_modified.append(after_edge);
  }
  else {
    UVEdge *current_edge = corner.first->edge;
    Vector<UVBorderEdge> new_border_edges;

    num_to_add = winding_solution.size();
    for (int64_t segment_index : winding_solution.index_range()) {

      float2 old_uv = current_edge->get_other_uv_vert(uv_vert->vert)->uv;
      int shared_edge_vert = current_edge->get_other_uv_vert(uv_vert->vert)->vert;

      float factor = (segment_index + 1.0f) / num_to_add;
      float2 new_uv = corner.uv(factor, min_uv_distance);

      FanSegment &segment = *winding_solution[segment_index];

      const int fill_primitive_i = segment.primitive_index;
      const int3 &tri_fill = mesh_data.corner_tris[fill_primitive_i];
      const int other_prim_vert = primitive_get_other_uv_vert(
          mesh_data, tri_fill, uv_vert->vert, shared_edge_vert);

      UVVert uv_vert_template;
      uv_vert_template.vert = uv_vert->vert;
      uv_vert_template.uv = uv_vert->uv;
      UVVert *vert_1_ptr = island.lookup_or_create(uv_vert_template);
      uv_vert_template.vert = shared_edge_vert;
      uv_vert_template.uv = old_uv;
      UVVert *vert_2_ptr = island.lookup_or_create(uv_vert_template);
      uv_vert_template.vert = other_prim_vert;
      uv_vert_template.uv = new_uv;
      UVVert *vert_3_ptr = island.lookup_or_create(uv_vert_template);

      add_uv_primitive_fill(island, *vert_1_ptr, *vert_2_ptr, *vert_3_ptr, fill_primitive_i);

      const int new_prim_i = island.uv_primitives.size() - 1;
      current_edge = island.uv_primitives[new_prim_i].get_uv_edge(uv_vert->vert, other_prim_vert);
      UVBorderEdge new_border(
          island.uv_primitives[new_prim_i].get_uv_edge(shared_edge_vert, other_prim_vert),
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
      border_edge.edge->verts[0]->flags.is_border = true;
      border_edge.edge->verts[1]->flags.is_border = true;
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
      queue.push({border.outside_angle(border_edge), &border_edge});
    }
  }

  /* Process border edges in order. */
  Vector<UVBorderEdge *> modified_edges;
  while (!queue.empty()) {
    const HeapEntry entry = queue.top();
    queue.pop();
    UVBorderEdge *border_edge = entry.edge;
    if (!border_edge->is_extendable()) {
      continue;
    }

    UVVert *uv_vert = border_edge->get_uv_vert(0);
    UVBorder &border = borders[border_edge->border_index];

    /* If the angle changed, re-queue with new angle. */
    const float angle = border.outside_angle(*border_edge);
    if (angle != entry.angle) {
      queue.push({angle, border_edge});
      continue;
    }

    UVBorderCorner extension_corner(border_edge->prev, border_edge, angle);

    /* Found corner is outside the mask, the corner should not be considered for extension. */
    const UVIslandsMask::Tile *tile = mask.find_tile(uv_vert->uv);
    if (tile && tile->is_masked(island_index, uv_vert->uv)) {
      modified_edges.clear();
      extend_at_vert(mesh_data,
                     *this,
                     extension_corner,
                     tile->get_pixel_size_in_uv_space() * 2.0f,
                     order_counter,
                     modified_edges);

      /* Queue modified and newly generated edges. */
      for (UVBorderEdge *modified_edge : modified_edges) {
        if (modified_edge->is_extendable()) {
          UVBorder &modified_border = borders[modified_edge->border_index];
          queue.push({modified_border.outside_angle(*modified_edge), modified_edge});
        }
      }
    }
    /* Mark that the vert is extended. */
    uv_vert->flags.is_extended = true;
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
    ss << "  [" << uvprimitive.edges[0]->verts[0]->vert << ", "
       << uvprimitive.edges[0]->verts[1]->vert << ", "
       << uvprimitive
              .get_other_uv_vert(uvprimitive.edges[0]->verts[0], uvprimitive.edges[0]->verts[1])
              ->vert
       << "],\n";
  }
  ss << "]\n";

  ss << "uvisland_uvs = [\n";
  for (const UVPrimitive &uvprimitive : uv_primitives) {
    float2 uv = uvprimitive.edges[0]->verts[0]->uv;
    ss << "  " << uv.x << ", " << uv.y << ",\n";
    uv = uvprimitive.edges[0]->verts[1]->uv;
    ss << "  " << uv.x << ", " << uv.y << ",\n";
    uv = uvprimitive
             .get_other_uv_vert(uvprimitive.edges[0]->verts[0], uvprimitive.edges[0]->verts[1])
             ->uv;
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

std::optional<UVBorder> extract_border_from_edges(MutableSpan<UVBorderEdge> edges,
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

  float2 first_uv = start_edge.get_uv_vert(0)->uv;
  float2 current_uv = start_edge.get_uv_vert(1)->uv;
  while (current_uv != first_uv) {
    bool edge_added = false;
    for (const int edge_i : edges.index_range()) {
      if (borders_used[edge_i].test()) {
        continue;
      }
      UVBorderEdge &border_edge = edges[edge_i];
      int i;
      for (i = 0; i < 2; i++) {
        if (border_edge.edge->verts[i]->uv == current_uv) {
          border_edge.reverse_order = i == 1;
          borders_used[edge_i].set();
          current_uv = border_edge.get_uv_vert(1)->uv;
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
  const UVVert *uv_vert1 = edge.get_uv_vert(0);
  const UVVert *uv_vert2 = edge.get_uv_vert(1);
  const UVVert *uv_vert3 = edge.get_other_uv_vert(island);
  float poly[3][2];
  copy_v2_v2(poly[0], uv_vert1->uv);
  copy_v2_v2(poly[1], uv_vert2->uv);
  copy_v2_v2(poly[2], uv_vert3->uv);
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

float UVBorder::outside_angle(const UVBorderEdge &edge) const
{
  const UVBorderEdge &prev = *edge.prev;
  return M_PI - angle_signed_v2v2(prev.get_uv_vert(1)->uv - prev.get_uv_vert(0)->uv,
                                  edge.get_uv_vert(1)->uv - edge.get_uv_vert(0)->uv);
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

float2 UVBorderCorner::uv(float factor, float min_uv_distance)
{
  using namespace blender::math;
  float2 origin = first->get_uv_vert(1)->uv;
  float angle_between = angle * factor;
  float desired_len = max_ff(second->length() * factor + first->length() * (1.0 - factor),
                             min_uv_distance);
  float2 v = normalize(first->get_uv_vert(0)->uv - origin);

  float2x2 rot_mat = from_rotation<float2x2>(AngleRadian(angle_between));
  float2 rotated = rot_mat * v;
  float2 result = rotated * desired_len + first->get_uv_vert(1)->uv;
  return result;
}

bool UVBorderCorner::connected_in_mesh() const
{
  return first->get_uv_vert(1)->vert == second->get_uv_vert(0)->vert;
}

void UVBorderCorner::print_debug() const
{
  std::stringstream ss;
  ss << "# ";
  if (connected_in_mesh()) {
    ss << first->get_uv_vert(0)->vert << "-";
    ss << first->get_uv_vert(1)->vert << "-";
    ss << second->get_uv_vert(1)->vert << "\n";
  }
  else {
    ss << first->get_uv_vert(0)->vert << "-";
    ss << first->get_uv_vert(1)->vert << ", ";
    ss << second->get_uv_vert(0)->vert << "-";
    ss << second->get_uv_vert(1)->vert << "\n";
  }
  std::cout << ss.str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVPrimitive
 * \{ */

UVPrimitive::UVPrimitive(const int primitive_i) : primitive_i(primitive_i) {}

const UVVert *UVPrimitive::get_uv_vert(const MeshData &mesh_data,
                                       const uint8_t mesh_vert_index) const
{
  const int3 &tri = mesh_data.corner_tris[this->primitive_i];
  const int mesh_vert = mesh_data.corner_verts[tri[mesh_vert_index]];
  for (const UVEdge *uv_edge : edges) {
    for (const UVVert *uv_vert : uv_edge->verts) {
      if (uv_vert->vert == mesh_vert) {
        return uv_vert;
      }
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

UVEdge *UVPrimitive::get_uv_edge(const float2 uv1, const float2 uv2) const
{
  for (UVEdge *uv_edge : edges) {
    const float2 &e1 = uv_edge->verts[0]->uv;
    const float2 &e2 = uv_edge->verts[1]->uv;
    if ((e1 == uv1 && e2 == uv2) || (e1 == uv2 && e2 == uv1)) {
      return uv_edge;
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

UVEdge *UVPrimitive::get_uv_edge(const int v1, const int v2) const
{
  for (UVEdge *uv_edge : edges) {
    const int e1 = uv_edge->verts[0]->vert;
    const int e2 = uv_edge->verts[1]->vert;
    if ((e1 == v1 && e2 == v2) || (e1 == v2 && e2 == v1)) {
      return uv_edge;
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

bool UVPrimitive::contains_uv_vert(const UVVert *uv_vert) const
{
  for (UVEdge *edge : edges) {
    if (std::find(edge->verts.begin(), edge->verts.end(), uv_vert) != edge->verts.end()) {
      return true;
    }
  }
  return false;
}

const UVVert *UVPrimitive::get_other_uv_vert(const UVVert *v1, const UVVert *v2) const
{
  BLI_assert(contains_uv_vert(v1));
  BLI_assert(contains_uv_vert(v2));

  for (const UVEdge *edge : edges) {
    for (const UVVert *uv_vert : edge->verts) {
      if (!ELEM(uv_vert, v1, v2)) {
        return uv_vert;
      }
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVBorderEdge
 * \{ */
UVBorderEdge::UVBorderEdge(UVEdge *edge, int uv_primitive) : edge(edge), uv_primitive(uv_primitive)
{
}

UVVert *UVBorderEdge::get_uv_vert(int index)
{
  int actual_index = reverse_order ? 1 - index : index;
  return edge->verts[actual_index];
}

const UVVert *UVBorderEdge::get_uv_vert(int index) const
{
  int actual_index = reverse_order ? 1 - index : index;
  return edge->verts[actual_index];
}

const UVVert *UVBorderEdge::get_other_uv_vert(const UVIsland &island) const
{
  return island.uv_primitives[uv_primitive].get_other_uv_vert(edge->verts[0], edge->verts[1]);
}

bool UVBorderEdge::is_extendable() const
{
  if (removed) {
    return false;
  }
  const UVVert *uv_vert = get_uv_vert(0);
  return uv_vert->flags.is_border && !uv_vert->flags.is_extended;
}

float UVBorderEdge::length() const
{
  return len_v2v2(edge->verts[0]->uv, edge->verts[1]->uv);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV islands
 * \{ */

Array<UVIsland> build_uv_islands(const MeshData &mesh_data,
                                 const GroupedSpan<int> tris_by_island,
                                 const UVIslandsMask &uv_masks)
{
  PRF_scope(ProfileCategory::Editor);

  Array<UVIsland> islands(tris_by_island.size());

  /* Add primitive to island. */
  threading::parallel_for(islands.index_range(), 1, [&](const IndexRange range) {
    for (const int64_t uv_island_id : range) {
      UVIsland &uv_island = islands[uv_island_id];
      uv_island.id = uv_island_id;
      for (const int primitive_i : tris_by_island[uv_island_id]) {
        add_primitive(mesh_data, uv_island, primitive_i);
      }
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
  for (Tile &tile : tiles) {
    dilate_tile(tile, max_iterations);
  }
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
