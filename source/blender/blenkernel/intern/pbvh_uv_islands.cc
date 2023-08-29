/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "pbvh_uv_islands.hh"

#include <iostream>
#include <optional>
#include <sstream>

namespace blender::bke::pbvh::uv_islands {

static void uv_edge_append_to_uv_vertices(UVEdge &uv_edge)
{
  for (UVVertex *vertex : uv_edge.vertices) {
    vertex->uv_edges.append_non_duplicates(&uv_edge);
  }
}

static void uv_primitive_append_to_uv_edges(UVPrimitive &uv_primitive)
{
  for (UVEdge *uv_edge : uv_primitive.edges) {
    uv_edge->uv_primitives.append_non_duplicates(&uv_primitive);
  }
}

static void uv_primitive_append_to_uv_vertices(UVPrimitive &uv_primitive)
{
  for (UVEdge *uv_edge : uv_primitive.edges) {
    uv_edge_append_to_uv_vertices(*uv_edge);
  }
}

/* -------------------------------------------------------------------- */
/** \name Mesh Primitives
 * \{ */

static int primitive_get_other_uv_vertex(const MeshData &mesh_data,
                                         const MLoopTri &looptri,
                                         const int v1,
                                         const int v2)
{
  const Span<int> corner_verts = mesh_data.corner_verts;
  BLI_assert(ELEM(v1,
                  corner_verts[looptri.tri[0]],
                  corner_verts[looptri.tri[1]],
                  corner_verts[looptri.tri[2]]));
  BLI_assert(ELEM(v2,
                  corner_verts[looptri.tri[0]],
                  corner_verts[looptri.tri[1]],
                  corner_verts[looptri.tri[2]]));
  for (const int loop : looptri.tri) {
    const int vert = corner_verts[loop];
    if (!ELEM(vert, v1, v2)) {
      return vert;
    }
  }
  return -1;
}

static bool primitive_has_shared_uv_edge(const Span<float2> uv_map,
                                         const MLoopTri &looptri,
                                         const MLoopTri &other)
{
  int shared_uv_verts = 0;
  for (const int loop : looptri.tri) {
    for (const int other_loop : other.tri) {
      if (uv_map[loop] == uv_map[other_loop]) {
        shared_uv_verts += 1;
      }
    }
  }
  return shared_uv_verts >= 2;
}

static int get_uv_loop(const MeshData &mesh_data, const MLoopTri &looptri, const int vert)
{
  for (const int loop : looptri.tri) {
    if (mesh_data.corner_verts[loop] == vert) {
      return loop;
    }
  }
  BLI_assert_unreachable();
  return looptri.tri[0];
}

static rctf primitive_uv_bounds(const MLoopTri &looptri, const Span<float2> uv_map)
{
  rctf result;
  BLI_rctf_init_minmax(&result);
  for (const int loop : looptri.tri) {
    BLI_rctf_do_minmax_v(&result, uv_map[loop]);
  }
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MeshData
 * \{ */

static void mesh_data_init_edges(MeshData &mesh_data)
{
  mesh_data.edges.reserve(mesh_data.looptris.size() * 2);
  EdgeHash *eh = BLI_edgehash_new_ex(__func__, mesh_data.looptris.size() * 3);
  for (int64_t i = 0; i < mesh_data.looptris.size(); i++) {
    const MLoopTri &tri = mesh_data.looptris[i];
    Vector<int, 3> edges;
    for (int j = 0; j < 3; j++) {
      int v1 = mesh_data.corner_verts[tri.tri[j]];
      int v2 = mesh_data.corner_verts[tri.tri[(j + 1) % 3]];

      void **edge_index_ptr;
      int64_t edge_index;
      if (BLI_edgehash_ensure_p(eh, v1, v2, &edge_index_ptr)) {
        edge_index = POINTER_AS_INT(*edge_index_ptr) - 1;
        *edge_index_ptr = POINTER_FROM_INT(edge_index);
      }
      else {
        edge_index = mesh_data.edges.size();
        *edge_index_ptr = POINTER_FROM_INT(edge_index + 1);
        MeshEdge edge;
        edge.vert1 = v1;
        edge.vert2 = v2;
        mesh_data.edges.append(edge);
        mesh_data.vert_to_edge_map.add(edge_index, v1, v2);
      }

      edges.append(edge_index);
    }
    mesh_data.primitive_to_edge_map.add(edges, i);
  }
  /* Build edge to neighboring triangle map. */
  mesh_data.edge_to_primitive_map = EdgeToPrimitiveMap(mesh_data.edges.size());
  for (const int prim_i : mesh_data.looptris.index_range()) {
    for (const int edge_i : mesh_data.primitive_to_edge_map[prim_i]) {
      mesh_data.edge_to_primitive_map.add(prim_i, edge_i);
    }
  }

  BLI_edgehash_free(eh, nullptr);
}
static constexpr int INVALID_UV_ISLAND_ID = -1;
/**
 * NOTE: doesn't support weird topology where unconnected mesh primitives share the same uv
 * island. For a accurate implementation we should use implement an uv_prim_lookup.
 */
static void extract_uv_neighbors(const MeshData &mesh_data,
                                 const Span<int> uv_island_ids,
                                 const int primitive_i,
                                 Vector<int> &prims_to_add)
{
  for (const int edge : mesh_data.primitive_to_edge_map[primitive_i]) {
    for (const int other_primitive_i : mesh_data.edge_to_primitive_map[edge]) {
      if (primitive_i == other_primitive_i) {
        continue;
      }
      if (uv_island_ids[other_primitive_i] != INVALID_UV_ISLAND_ID) {
        continue;
      }

      if (primitive_has_shared_uv_edge(mesh_data.uv_map,
                                       mesh_data.looptris[primitive_i],
                                       mesh_data.looptris[other_primitive_i]))
      {
        prims_to_add.append(other_primitive_i);
      }
    }
  }
}

static int mesh_data_init_primitive_uv_island_ids(MeshData &mesh_data)
{
  mesh_data.uv_island_ids.reinitialize(mesh_data.looptris.size());
  mesh_data.uv_island_ids.fill(INVALID_UV_ISLAND_ID);

  int uv_island_id = 0;
  Vector<int> prims_to_add;
  for (const int primitive_i : mesh_data.looptris.index_range()) {
    /* Early exit when uv island id is already extracted during uv neighbor extractions. */
    if (mesh_data.uv_island_ids[primitive_i] != INVALID_UV_ISLAND_ID) {
      continue;
    }

    prims_to_add.append(primitive_i);
    while (!prims_to_add.is_empty()) {
      const int other_primitive_i = prims_to_add.pop_last();
      mesh_data.uv_island_ids[other_primitive_i] = uv_island_id;
      extract_uv_neighbors(mesh_data, mesh_data.uv_island_ids, other_primitive_i, prims_to_add);
    }
    uv_island_id++;
  }

  return uv_island_id;
}

static void mesh_data_init(MeshData &mesh_data)
{
  mesh_data_init_edges(mesh_data);
  mesh_data.uv_island_len = mesh_data_init_primitive_uv_island_ids(mesh_data);
}

MeshData::MeshData(const Span<MLoopTri> looptris,
                   const Span<int> corner_verts,
                   const Span<float2> uv_map,
                   const Span<float3> vert_positions)
    : looptris(looptris),
      corner_verts(corner_verts),
      uv_map(uv_map),
      vert_positions(vert_positions),
      vert_to_edge_map(vert_positions.size()),
      edge_to_primitive_map(0),
      primitive_to_edge_map(looptris.size())
{
  mesh_data_init(*this);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVVertex
 * \{ */

static void uv_vertex_init_flags(UVVertex &uv_vertex)
{
  uv_vertex.flags.is_border = false;
  uv_vertex.flags.is_extended = false;
}

UVVertex::UVVertex()
{
  uv_vertex_init_flags(*this);
}

UVVertex::UVVertex(const MeshData &mesh_data, const int loop)
    : vertex(mesh_data.corner_verts[loop]), uv(mesh_data.uv_map[loop])
{
  uv_vertex_init_flags(*this);
}

/**
 * Get a list containing the indices of mesh primitives (primitive of the input mesh), that
 * surround the given uv_vertex in uv-space.
 */
static Vector<int> connecting_mesh_primitive_indices(const UVVertex &uv_vertex)
{
  Vector<int> primitives_around_uv_vertex;
  for (const UVEdge *uv_edge : uv_vertex.uv_edges) {
    for (const UVPrimitive *uv_primitive : uv_edge->uv_primitives) {
      primitives_around_uv_vertex.append_non_duplicates(uv_primitive->primitive_i);
    }
  }
  return primitives_around_uv_vertex;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVEdge
 * \{ */

bool UVEdge::has_shared_edge(const Span<float2> uv_map, const int loop_1, const int loop_2) const
{
  return (vertices[0]->uv == uv_map[loop_1] && vertices[1]->uv == uv_map[loop_2]) ||
         (vertices[0]->uv == uv_map[loop_2] && vertices[1]->uv == uv_map[loop_1]);
}

bool UVEdge::has_shared_edge(const UVVertex &v1, const UVVertex &v2) const
{
  return (vertices[0]->uv == v1.uv && vertices[1]->uv == v2.uv) ||
         (vertices[0]->uv == v2.uv && vertices[1]->uv == v1.uv);
}

bool UVEdge::has_shared_edge(const UVEdge &other) const
{
  return has_shared_edge(*other.vertices[0], *other.vertices[1]);
}

bool UVEdge::has_same_vertices(const int vert1, const int vert2) const
{
  return (vertices[0]->vertex == vert1 && vertices[1]->vertex == vert2) ||
         (vertices[0]->vertex == vert2 && vertices[1]->vertex == vert1);
}

bool UVEdge::has_same_uv_vertices(const UVEdge &other) const
{
  return has_shared_edge(other) &&
         has_same_vertices(other.vertices[0]->vertex, other.vertices[1]->vertex);
}

bool UVEdge::has_same_vertices(const MeshEdge &edge) const
{
  return has_same_vertices(edge.vert1, edge.vert2);
}

bool UVEdge::is_border_edge() const
{
  return uv_primitives.size() == 1;
}

UVVertex *UVEdge::get_other_uv_vertex(const int vertex)
{
  if (vertices[0]->vertex == vertex) {
    return vertices[1];
  }
  return vertices[0];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVIsland
 * \{ */
UVVertex *UVIsland::lookup(const UVVertex &vertex)
{
  const int vert_index = vertex.vertex;
  Vector<UVVertex *> &vertices = uv_vertex_lookup.lookup_or_add_default(vert_index);
  for (UVVertex *v : vertices) {
    if (v->uv == vertex.uv) {
      return v;
    }
  }
  return nullptr;
}

UVVertex *UVIsland::lookup_or_create(const UVVertex &vertex)
{
  UVVertex *found_vertex = lookup(vertex);
  if (found_vertex != nullptr) {
    return found_vertex;
  }

  uv_vertices.append(vertex);
  UVVertex *result = &uv_vertices.last();
  result->uv_edges.clear();
  /* v is already a key. Ensured by UVIsland::lookup in this method. */
  uv_vertex_lookup.lookup(vertex.vertex).append(result);
  return result;
}

UVEdge *UVIsland::lookup(const UVEdge &edge)
{
  UVVertex *found_vertex = lookup(*edge.vertices[0]);
  if (found_vertex == nullptr) {
    return nullptr;
  }
  for (UVEdge *e : found_vertex->uv_edges) {
    UVVertex *other_vertex = e->get_other_uv_vertex(found_vertex->vertex);
    if (other_vertex->vertex == edge.vertices[1]->vertex &&
        other_vertex->uv == edge.vertices[1]->uv) {
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
  result->uv_primitives.clear();
  return result;
}

void UVIsland::append(const UVPrimitive &primitive)
{
  uv_primitives.append(primitive);
  UVPrimitive *new_prim_ptr = &uv_primitives.last();
  for (int i = 0; i < 3; i++) {
    UVEdge *other_edge = primitive.edges[i];
    UVEdge uv_edge_template;
    uv_edge_template.vertices[0] = lookup_or_create(*other_edge->vertices[0]);
    uv_edge_template.vertices[1] = lookup_or_create(*other_edge->vertices[1]);
    new_prim_ptr->edges[i] = lookup_or_create(uv_edge_template);
    uv_edge_append_to_uv_vertices(*new_prim_ptr->edges[i]);
    new_prim_ptr->edges[i]->uv_primitives.append(new_prim_ptr);
  }
}

bool UVIsland::has_shared_edge(const UVPrimitive &primitive) const
{
  for (const VectorList<UVPrimitive>::UsedVector &prims : uv_primitives) {
    for (const UVPrimitive &prim : prims) {
      if (prim.has_shared_edge(primitive)) {
        return true;
      }
    }
  }
  return false;
}

bool UVIsland::has_shared_edge(const MeshData &mesh_data, const int primitive_i) const
{
  for (const VectorList<UVPrimitive>::UsedVector &primitives : uv_primitives) {
    for (const UVPrimitive &prim : primitives) {
      if (prim.has_shared_edge(mesh_data, primitive_i)) {
        return true;
      }
    }
  }
  return false;
}

void UVIsland::extend_border(const UVPrimitive &primitive)
{
  for (const VectorList<UVPrimitive>::UsedVector &primitives : uv_primitives) {
    for (const UVPrimitive &prim : primitives) {
      if (prim.has_shared_edge(primitive)) {
        this->append(primitive);
      }
    }
  }
}

static UVPrimitive *add_primitive(const MeshData &mesh_data,
                                  UVIsland &uv_island,
                                  const int primitive_i)
{
  UVPrimitive uv_primitive(primitive_i);
  const MLoopTri &primitive = mesh_data.looptris[primitive_i];
  uv_island.uv_primitives.append(uv_primitive);
  UVPrimitive *uv_primitive_ptr = &uv_island.uv_primitives.last();
  for (const int edge_i : mesh_data.primitive_to_edge_map[primitive_i]) {
    const MeshEdge &edge = mesh_data.edges[edge_i];
    const int loop_1 = get_uv_loop(mesh_data, primitive, edge.vert1);
    const int loop_2 = get_uv_loop(mesh_data, primitive, edge.vert2);
    UVEdge uv_edge_template;
    uv_edge_template.vertices[0] = uv_island.lookup_or_create(UVVertex(mesh_data, loop_1));
    uv_edge_template.vertices[1] = uv_island.lookup_or_create(UVVertex(mesh_data, loop_2));
    UVEdge *uv_edge = uv_island.lookup_or_create(uv_edge_template);
    uv_primitive_ptr->edges.append(uv_edge);
    uv_edge_append_to_uv_vertices(*uv_edge);
    uv_edge->uv_primitives.append(uv_primitive_ptr);
  }
  return uv_primitive_ptr;
}

void UVIsland::extract_borders()
{
  /* Lookup all borders of the island. */
  Vector<UVBorderEdge> edges;
  for (VectorList<UVPrimitive>::UsedVector &prims : uv_primitives) {
    for (UVPrimitive &prim : prims) {
      for (UVEdge *edge : prim.edges) {
        if (edge->is_border_edge()) {
          edges.append(UVBorderEdge(edge, &prim));
        }
      }
    }
  }

  while (true) {
    std::optional<UVBorder> border = UVBorder::extract_from_edges(edges);
    if (!border.has_value()) {
      break;
    }
    if (!border->is_ccw()) {
      border->flip_order();
    }
    borders.append(*border);
  }
}

static std::optional<UVBorderCorner> sharpest_border_corner(UVBorder &border, float *r_angle)
{
  *r_angle = std::numeric_limits<float>::max();
  std::optional<UVBorderCorner> result;
  for (UVBorderEdge &edge : border.edges) {
    const UVVertex *uv_vertex = edge.get_uv_vertex(0);
    /* Only allow extending from tagged border vertices that have not been extended yet. During
     * extending new borders are created, those are ignored as their is_border is set to false. */
    if (!uv_vertex->flags.is_border || uv_vertex->flags.is_extended) {
      continue;
    }
    float new_angle = border.outside_angle(edge);
    if (new_angle < *r_angle) {
      *r_angle = new_angle;
      result = UVBorderCorner(&border.edges[edge.prev_index], &edge, new_angle);
    }
  }
  return result;
}

static std::optional<UVBorderCorner> sharpest_border_corner(UVIsland &island)
{
  std::optional<UVBorderCorner> result;
  float sharpest_angle = std::numeric_limits<float>::max();
  for (UVBorder &border : island.borders) {
    float new_angle;
    std::optional<UVBorderCorner> new_result = sharpest_border_corner(border, &new_angle);
    if (new_angle < sharpest_angle) {
      sharpest_angle = new_angle;
      result = new_result;
    }
  }
  return result;
}

/** The inner edge of a fan. */
struct FanSegment {
  const int primitive_index;
  const MLoopTri *primitive;
  /* UVs order are already applied. So `uvs[0]` matches `primitive->vertices[vert_order[0]]`. */
  float2 uvs[3];
  int vert_order[3];

  struct {
    bool found : 1;
  } flags;

  FanSegment(const MeshData &mesh_data,
             const int primitive_index,
             const MLoopTri *primitive,
             int vertex)
      : primitive_index(primitive_index), primitive(primitive)
  {
    flags.found = false;

    /* Reorder so the first edge starts with the given vertex. */
    if (mesh_data.corner_verts[primitive->tri[1]] == vertex) {
      vert_order[0] = 1;
      vert_order[1] = 2;
      vert_order[2] = 0;
    }
    else if (mesh_data.corner_verts[primitive->tri[2]] == vertex) {
      vert_order[0] = 2;
      vert_order[1] = 0;
      vert_order[2] = 1;
    }
    else {
      BLI_assert(mesh_data.corner_verts[primitive->tri[0]] == vertex);
      vert_order[0] = 0;
      vert_order[1] = 1;
      vert_order[2] = 2;
    }
  }

  void print_debug(const MeshData &mesh_data) const
  {
    std::stringstream ss;
    ss << " v1:" << mesh_data.corner_verts[primitive->tri[vert_order[0]]];
    ss << " v2:" << mesh_data.corner_verts[primitive->tri[vert_order[1]]];
    ss << " v3:" << mesh_data.corner_verts[primitive->tri[vert_order[2]]];
    ss << " uv1:" << uvs[0];
    ss << " uv2:" << uvs[1];
    ss << " uv3:" << uvs[2];
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

  Fan(const MeshData &mesh_data, const int vertex)
  {
    flags.is_manifold = true;
    int current_edge = mesh_data.vert_to_edge_map[vertex].first();
    const int stop_primitive = mesh_data.edge_to_primitive_map[current_edge].first();
    int previous_primitive = stop_primitive;
    while (true) {
      bool stop = false;
      for (const int other_primitive_i : mesh_data.edge_to_primitive_map[current_edge]) {
        if (stop) {
          break;
        }
        if (other_primitive_i == previous_primitive) {
          continue;
        }

        const MLoopTri &other_looptri = mesh_data.looptris[other_primitive_i];

        for (const int edge_i : mesh_data.primitive_to_edge_map[other_primitive_i]) {
          const MeshEdge &edge = mesh_data.edges[edge_i];
          if (edge_i == current_edge || (edge.vert1 != vertex && edge.vert2 != vertex)) {
            continue;
          }
          segments.append(FanSegment(mesh_data, other_primitive_i, &other_looptri, vertex));
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

  void mark_already_added_segments(const UVVertex &uv_vertex)
  {
    Vector<int> mesh_primitive_indices = connecting_mesh_primitive_indices(uv_vertex);

    /* Go over all fan edges to find if they can be found as primitive around the uv vertex. */
    for (FanSegment &fan_edge : segments) {
      fan_edge.flags.found = mesh_primitive_indices.contains(fan_edge.primitive_index);
    }
  }

  void init_uv_coordinates(const MeshData &mesh_data, UVVertex &uv_vertex)
  {
    for (FanSegment &fan_edge : segments) {
      int other_v = mesh_data.corner_verts[fan_edge.primitive->tri[fan_edge.vert_order[0]]];
      if (other_v == uv_vertex.vertex) {
        other_v = mesh_data.corner_verts[fan_edge.primitive->tri[fan_edge.vert_order[1]]];
      }

      for (UVEdge *edge : uv_vertex.uv_edges) {
        const UVVertex *other_uv_vertex = edge->get_other_uv_vertex(uv_vertex.vertex);
        int64_t other_edge_v = other_uv_vertex->vertex;
        if (other_v == other_edge_v) {
          fan_edge.uvs[0] = uv_vertex.uv;
          fan_edge.uvs[1] = other_uv_vertex->uv;
          break;
        }
      }
    }

    segments.last().uvs[2] = segments.first().uvs[1];
    for (int i = 0; i < segments.size() - 1; i++) {
      segments[i].uvs[2] = segments[i + 1].uvs[1];
    }
  }

#ifndef NDEBUG
  /**
   * Check if the given vertex is part of the outside of the fan.
   * Return true if the given vertex is found on the outside of the fan, otherwise returns false.
   */
  bool contains_vertex_on_outside(const MeshData &mesh_data, const int vertex_index) const
  {
    for (const FanSegment &segment : segments) {
      int v2 = mesh_data.corner_verts[segment.primitive->tri[segment.vert_order[1]]];
      if (vertex_index == v2) {
        return true;
      }
    }
    return false;
  }

#endif

  static bool is_path_valid(const Span<FanSegment *> &path,
                            const MeshData &mesh_data,
                            const int from_vertex,
                            const int to_vertex)
  {
    int current_vert = from_vertex;
    for (FanSegment *segment : path) {
      int v1 = mesh_data.corner_verts[segment->primitive->tri[segment->vert_order[1]]];
      int v2 = mesh_data.corner_verts[segment->primitive->tri[segment->vert_order[2]]];
      if (!ELEM(current_vert, v1, v2)) {
        return false;
      }
      current_vert = v1 == current_vert ? v2 : v1;
    }
    return current_vert == to_vertex;
  }

  /**
   * Find the closest path over the fan between `from_vertex` and `to_vertex`. The result contains
   * exclude the starting and final edge.
   *
   * Algorithm only uses the winding order of the given fan segments.
   */
  static Vector<FanSegment *> path_between(const Span<FanSegment *> edge_order,
                                           const MeshData &mesh_data,
                                           const int from_vertex,
                                           const int to_vertex,
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
      int v2 =
          mesh_data.corner_verts[segment->primitive->tri[segment->vert_order[from_vert_order]]];
      if (v2 == from_vertex) {
        break;
      }
      index = (index + index_increment + edge_order.size()) % edge_order.size();
    }

    while (true) {
      FanSegment *segment = edge_order[index];
      result.append(segment);

      int v3 = mesh_data.corner_verts[segment->primitive->tri[segment->vert_order[to_vert_order]]];
      if (v3 == to_vertex) {
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
                                         const int from_vertex,
                                         const int to_vertex)
  {
    BLI_assert_msg(contains_vertex_on_outside(mesh_data, from_vertex),
                   "Inconsistency detected, `from_vertex` isn't part of the outside of the fan.");
    BLI_assert_msg(contains_vertex_on_outside(mesh_data, to_vertex),
                   "Inconsistency detected, `to_vertex` isn't part of the outside of the fan.");
    if (to_vertex == from_vertex) {
      return Vector<FanSegment *>();
    }

    Array<FanSegment *> edges(segments.size());
    for (int64_t index : segments.index_range()) {
      edges[index] = &segments[index];
    }

    Vector<FanSegment *> winding_1 = path_between(edges, mesh_data, from_vertex, to_vertex, false);
    Vector<FanSegment *> winding_2 = path_between(edges, mesh_data, from_vertex, to_vertex, true);

    bool winding_1_valid = is_path_valid(winding_1, mesh_data, from_vertex, to_vertex);
    bool winding_2_valid = is_path_valid(winding_2, mesh_data, from_vertex, to_vertex);

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
                                            UVVertex *connected_vert_1,
                                            UVVertex *connected_vert_2,
                                            float2 uv_unconnected,
                                            const int mesh_primitive_i)
{
  UVPrimitive prim1(mesh_primitive_i);
  const MLoopTri &looptri = mesh_data.looptris[mesh_primitive_i];

  const int other_vert_i = primitive_get_other_uv_vertex(
      mesh_data, looptri, connected_vert_1->vertex, connected_vert_2->vertex);
  UVVertex vert_template;
  vert_template.uv = uv_unconnected;
  vert_template.vertex = other_vert_i;
  UVVertex *vert_ptr = island.lookup_or_create(vert_template);

  const int loop_1 = get_uv_loop(mesh_data, looptri, connected_vert_1->vertex);
  vert_template.uv = connected_vert_1->uv;
  vert_template.vertex = mesh_data.corner_verts[loop_1];
  UVVertex *vert_1_ptr = island.lookup_or_create(vert_template);

  const int loop_2 = get_uv_loop(mesh_data, looptri, connected_vert_2->vertex);
  vert_template.uv = connected_vert_2->uv;
  vert_template.vertex = mesh_data.corner_verts[loop_2];
  UVVertex *vert_2_ptr = island.lookup_or_create(vert_template);

  UVEdge edge_template;
  edge_template.vertices[0] = vert_1_ptr;
  edge_template.vertices[1] = vert_2_ptr;
  prim1.edges.append(island.lookup_or_create(edge_template));
  edge_template.vertices[0] = vert_2_ptr;
  edge_template.vertices[1] = vert_ptr;
  prim1.edges.append(island.lookup_or_create(edge_template));
  edge_template.vertices[0] = vert_ptr;
  edge_template.vertices[1] = vert_1_ptr;
  prim1.edges.append(island.lookup_or_create(edge_template));
  uv_primitive_append_to_uv_edges(prim1);
  uv_primitive_append_to_uv_vertices(prim1);
  island.uv_primitives.append(prim1);
}
/**
 * Find a primitive that can be used to fill give corner.
 * Will return -1 when no primitive can be found.
 */
static int find_fill_primitive(const MeshData &mesh_data, UVBorderCorner &corner)
{
  if (corner.first->get_uv_vertex(1) != corner.second->get_uv_vertex(0)) {
    return -1;
  }
  if (corner.first->get_uv_vertex(0) == corner.second->get_uv_vertex(1)) {
    return -1;
  }
  UVVertex *shared_vert = corner.second->get_uv_vertex(0);
  for (const int edge_i : mesh_data.vert_to_edge_map[shared_vert->vertex]) {
    const MeshEdge &edge = mesh_data.edges[edge_i];
    if (corner.first->edge->has_same_vertices(edge)) {
      for (const int primitive_i : mesh_data.edge_to_primitive_map[edge_i]) {
        const MLoopTri &looptri = mesh_data.looptris[primitive_i];
        const int other_vert = primitive_get_other_uv_vertex(
            mesh_data, looptri, edge.vert1, edge.vert2);
        if (other_vert == corner.second->get_uv_vertex(1)->vertex) {
          return primitive_i;
        }
      }
    }
  }
  return -1;
}

static void add_uv_primitive_fill(UVIsland &island,
                                  UVVertex &uv_vertex1,
                                  UVVertex &uv_vertex2,
                                  UVVertex &uv_vertex3,
                                  const int fill_primitive_i)
{
  UVPrimitive uv_primitive(fill_primitive_i);
  UVEdge edge_template;
  edge_template.vertices[0] = &uv_vertex1;
  edge_template.vertices[1] = &uv_vertex2;
  uv_primitive.edges.append(island.lookup_or_create(edge_template));
  edge_template.vertices[0] = &uv_vertex2;
  edge_template.vertices[1] = &uv_vertex3;
  uv_primitive.edges.append(island.lookup_or_create(edge_template));
  edge_template.vertices[0] = &uv_vertex3;
  edge_template.vertices[1] = &uv_vertex1;
  uv_primitive.edges.append(island.lookup_or_create(edge_template));
  uv_primitive_append_to_uv_edges(uv_primitive);
  uv_primitive_append_to_uv_vertices(uv_primitive);
  island.uv_primitives.append(uv_primitive);
}

static void extend_at_vert(const MeshData &mesh_data,
                           UVIsland &island,
                           UVBorderCorner &corner,
                           float min_uv_distance)
{

  int border_index = corner.first->border_index;
  UVBorder &border = island.borders[border_index];
  if (!corner.connected_in_mesh()) {
    return;
  }

  UVVertex *uv_vertex = corner.second->get_uv_vertex(0);
  Fan fan(mesh_data, uv_vertex->vertex);
  if (!fan.flags.is_manifold) {
    return;
  }
  fan.init_uv_coordinates(mesh_data, *uv_vertex);
  fan.mark_already_added_segments(*uv_vertex);
  int num_to_add = fan.count_edges_not_added();

  /* In 3d space everything can connected, but in uv space it may not.
   * in this case in the space between we should extract the primitives to be added
   * from the fan. */
  Vector<FanSegment *> winding_solution = fan.best_path_between(
      mesh_data, corner.first->get_uv_vertex(0)->vertex, corner.second->get_uv_vertex(1)->vertex);

  /*
   * When all edges are already added and its winding solution contains one segment to be added,
   * the segment should be split into two segments in order one for both sides.
   *
   * Although the fill_primitive can fill the missing segment it could lead to a squashed
   * triangle when the corner angle is near 180 degrees. In order to fix this we will
   * always add two segments both using the same fill primitive.
   */
  if (winding_solution.size() < 2 && (num_to_add == 0 || corner.angle > 2.0f)) {
    int fill_primitive_1_i = corner.second->uv_primitive->primitive_i;
    int fill_primitive_2_i = corner.first->uv_primitive->primitive_i;

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
                                    corner.first->get_uv_vertex(1),
                                    corner.first->get_uv_vertex(0),
                                    center_uv,
                                    fill_primitive_1_i);
    UVPrimitive &new_prim_1 = island.uv_primitives.last();
    add_uv_primitive_shared_uv_edge(mesh_data,
                                    island,
                                    corner.second->get_uv_vertex(0),
                                    corner.second->get_uv_vertex(1),
                                    center_uv,
                                    fill_primitive_2_i);
    UVPrimitive &new_prim_2 = island.uv_primitives.last();

    /* Update border after adding the new geometry. */
    {
      UVBorderEdge *border_edge = corner.first;
      border_edge->uv_primitive = &new_prim_1;
      border_edge->edge = border_edge->uv_primitive->get_uv_edge(
          corner.first->get_uv_vertex(0)->uv, center_uv);
      border_edge->reverse_order = border_edge->edge->vertices[0]->uv == center_uv;
    }
    {
      UVBorderEdge *border_edge = corner.second;
      border_edge->uv_primitive = &new_prim_2;
      border_edge->edge = border_edge->uv_primitive->get_uv_edge(
          corner.second->get_uv_vertex(1)->uv, center_uv);
      border_edge->reverse_order = border_edge->edge->vertices[1]->uv == center_uv;
    }
  }
  else {
    UVEdge *current_edge = corner.first->edge;
    Vector<UVBorderEdge> new_border_edges;

    num_to_add = winding_solution.size();
    for (int64_t segment_index : winding_solution.index_range()) {

      float2 old_uv = current_edge->get_other_uv_vertex(uv_vertex->vertex)->uv;
      int shared_edge_vertex = current_edge->get_other_uv_vertex(uv_vertex->vertex)->vertex;

      float factor = (segment_index + 1.0f) / num_to_add;
      float2 new_uv = corner.uv(factor, min_uv_distance);

      FanSegment &segment = *winding_solution[segment_index];

      const int fill_primitive_i = segment.primitive_index;
      const MLoopTri &fill_primitive = mesh_data.looptris[fill_primitive_i];
      const int other_prim_vertex = primitive_get_other_uv_vertex(
          mesh_data, fill_primitive, uv_vertex->vertex, shared_edge_vertex);

      UVVertex uv_vertex_template;
      uv_vertex_template.vertex = uv_vertex->vertex;
      uv_vertex_template.uv = uv_vertex->uv;
      UVVertex *vertex_1_ptr = island.lookup_or_create(uv_vertex_template);
      uv_vertex_template.vertex = shared_edge_vertex;
      uv_vertex_template.uv = old_uv;
      UVVertex *vertex_2_ptr = island.lookup_or_create(uv_vertex_template);
      uv_vertex_template.vertex = other_prim_vertex;
      uv_vertex_template.uv = new_uv;
      UVVertex *vertex_3_ptr = island.lookup_or_create(uv_vertex_template);

      add_uv_primitive_fill(island, *vertex_1_ptr, *vertex_2_ptr, *vertex_3_ptr, fill_primitive_i);

      UVPrimitive &new_prim = island.uv_primitives.last();
      current_edge = new_prim.get_uv_edge(uv_vertex->vertex, other_prim_vertex);
      UVBorderEdge new_border(new_prim.get_uv_edge(shared_edge_vertex, other_prim_vertex),
                              &new_prim);
      new_border_edges.append(new_border);
    }

    int border_insert = corner.first->index;
    border.remove(border_insert);

    int border_next = corner.second->index;
    if (border_next < border_insert) {
      border_insert--;
    }
    else {
      border_next--;
    }
    border.remove(border_next);
    border.edges.insert(border_insert, new_border_edges);

    border.update_indexes(border_index);
  }
}

/* Marks vertices that can be extended. Only vertices that are part of a border can be extended. */
static void reset_extendability_flags(UVIsland &island)
{
  for (VectorList<UVVertex>::UsedVector &uv_vertices : island.uv_vertices) {
    for (UVVertex &uv_vertex : uv_vertices) {
      uv_vertex.flags.is_border = false;
      uv_vertex.flags.is_extended = false;
    }
  }

  for (UVBorder border : island.borders) {
    for (UVBorderEdge &border_edge : border.edges) {
      border_edge.edge->vertices[0]->flags.is_border = true;
      border_edge.edge->vertices[1]->flags.is_border = true;
    }
  }
}

void UVIsland::extend_border(const MeshData &mesh_data,
                             const UVIslandsMask &mask,
                             const short island_index)
{
  reset_extendability_flags(*this);

  int64_t border_index = 0;
  for (UVBorder &border : borders) {
    border.update_indexes(border_index++);
  }
  while (true) {
    std::optional<UVBorderCorner> extension_corner = sharpest_border_corner(*this);
    if (!extension_corner.has_value()) {
      break;
    }

    UVVertex *uv_vertex = extension_corner->second->get_uv_vertex(0);

    /* Found corner is outside the mask, the corner should not be considered for extension. */
    const UVIslandsMask::Tile *tile = mask.find_tile(uv_vertex->uv);
    if (tile && tile->is_masked(island_index, uv_vertex->uv)) {
      extend_at_vert(
          mesh_data, *this, *extension_corner, tile->get_pixel_size_in_uv_space() * 2.0f);
    }
    /* Mark that the vert is extended. */
    uv_vertex->flags.is_extended = true;
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
  for (const float3 &vertex_position : mesh_data.vert_positions) {
    ss << "  mathutils.Vector((" << vertex_position.x << ", " << vertex_position.y << ", "
       << vertex_position.z << ")),\n";
  }
  ss << "]\n";

  ss << "uvisland_edges = []\n";

  ss << "uvisland_faces = [\n";
  for (const VectorList<UVPrimitive>::UsedVector &uvprimitives : uv_primitives) {
    for (const UVPrimitive &uvprimitive : uvprimitives) {
      ss << "  [" << uvprimitive.edges[0]->vertices[0]->vertex << ", "
         << uvprimitive.edges[0]->vertices[1]->vertex << ", "
         << uvprimitive
                .get_other_uv_vertex(uvprimitive.edges[0]->vertices[0],
                                     uvprimitive.edges[0]->vertices[1])
                ->vertex
         << "],\n";
    }
  }
  ss << "]\n";

  ss << "uvisland_uvs = [\n";
  for (const VectorList<UVPrimitive>::UsedVector &uvprimitives : uv_primitives) {
    for (const UVPrimitive &uvprimitive : uvprimitives) {
      float2 uv = uvprimitive.edges[0]->vertices[0]->uv;
      ss << "  " << uv.x << ", " << uv.y << ",\n";
      uv = uvprimitive.edges[0]->vertices[1]->uv;
      ss << "  " << uv.x << ", " << uv.y << ",\n";
      uv = uvprimitive
               .get_other_uv_vertex(uvprimitive.edges[0]->vertices[0],
                                    uvprimitive.edges[0]->vertices[1])
               ->uv;
      ss << "  " << uv.x << ", " << uv.y << ",\n";
    }
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

std::optional<UVBorder> UVBorder::extract_from_edges(Vector<UVBorderEdge> &edges)
{
  /* Find a part of the border that haven't been extracted yet. */
  UVBorderEdge *starting_border_edge = nullptr;
  for (UVBorderEdge &edge : edges) {
    if (edge.tag == false) {
      starting_border_edge = &edge;
      break;
    }
  }
  if (starting_border_edge == nullptr) {
    return std::nullopt;
  }
  UVBorder border;
  border.edges.append(*starting_border_edge);
  starting_border_edge->tag = true;

  float2 first_uv = starting_border_edge->get_uv_vertex(0)->uv;
  float2 current_uv = starting_border_edge->get_uv_vertex(1)->uv;
  while (current_uv != first_uv) {
    for (UVBorderEdge &border_edge : edges) {
      if (border_edge.tag == true) {
        continue;
      }
      int i;
      for (i = 0; i < 2; i++) {
        if (border_edge.edge->vertices[i]->uv == current_uv) {
          border_edge.reverse_order = i == 1;
          border_edge.tag = true;
          current_uv = border_edge.get_uv_vertex(1)->uv;
          border.edges.append(border_edge);
          break;
        }
      }
      if (i != 2) {
        break;
      }
    }
  }
  return border;
}

bool UVBorder::is_ccw() const
{
  const UVBorderEdge &edge = edges.first();
  const UVVertex *uv_vertex1 = edge.get_uv_vertex(0);
  const UVVertex *uv_vertex2 = edge.get_uv_vertex(1);
  const UVVertex *uv_vertex3 = edge.get_other_uv_vertex();
  float poly[3][2];
  copy_v2_v2(poly[0], uv_vertex1->uv);
  copy_v2_v2(poly[1], uv_vertex2->uv);
  copy_v2_v2(poly[2], uv_vertex3->uv);
  const bool ccw = cross_poly_v2(poly, 3) < 0.0;
  return ccw;
}

void UVBorder::flip_order()
{
  uint64_t border_index = edges.first().border_index;
  for (UVBorderEdge &edge : edges) {
    edge.reverse_order = !edge.reverse_order;
  }
  std::reverse(edges.begin(), edges.end());
  update_indexes(border_index);
}

float UVBorder::outside_angle(const UVBorderEdge &edge) const
{
  const UVBorderEdge &prev = edges[edge.prev_index];
  return M_PI - angle_signed_v2v2(prev.get_uv_vertex(1)->uv - prev.get_uv_vertex(0)->uv,
                                  edge.get_uv_vertex(1)->uv - edge.get_uv_vertex(0)->uv);
}

void UVBorder::update_indexes(uint64_t border_index)
{
  for (int64_t i = 0; i < edges.size(); i++) {
    int64_t prev = (i - 1 + edges.size()) % edges.size();
    int64_t next = (i + 1) % edges.size();
    edges[i].prev_index = prev;
    edges[i].index = i;
    edges[i].next_index = next;
    edges[i].border_index = border_index;
  }
}

void UVBorder::remove(int64_t index)
{
  /* Could read the border_index from any border edge as they are consistent. */
  uint64_t border_index = edges[0].border_index;
  edges.remove(index);
  update_indexes(border_index);
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
  float2 origin = first->get_uv_vertex(1)->uv;
  float angle_between = angle * factor;
  float desired_len = max_ff(second->length() * factor + first->length() * (1.0 - factor),
                             min_uv_distance);
  float2 v = normalize(first->get_uv_vertex(0)->uv - origin);

  float2x2 rot_mat = from_rotation<float2x2>(AngleRadian(angle_between));
  float2 rotated = rot_mat * v;
  float2 result = rotated * desired_len + first->get_uv_vertex(1)->uv;
  return result;
}

bool UVBorderCorner::connected_in_mesh() const
{
  return first->get_uv_vertex(1) == second->get_uv_vertex(0);
}

void UVBorderCorner::print_debug() const
{
  std::stringstream ss;
  ss << "# ";
  if (connected_in_mesh()) {
    ss << first->get_uv_vertex(0)->vertex << "-";
    ss << first->get_uv_vertex(1)->vertex << "-";
    ss << second->get_uv_vertex(1)->vertex << "\n";
  }
  else {
    ss << first->get_uv_vertex(0)->vertex << "-";
    ss << first->get_uv_vertex(1)->vertex << ", ";
    ss << second->get_uv_vertex(0)->vertex << "-";
    ss << second->get_uv_vertex(1)->vertex << "\n";
  }
  std::cout << ss.str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVPrimitive
 * \{ */

UVPrimitive::UVPrimitive(const int primitive_i) : primitive_i(primitive_i) {}

Vector<std::pair<UVEdge *, UVEdge *>> UVPrimitive::shared_edges(UVPrimitive &other)
{
  Vector<std::pair<UVEdge *, UVEdge *>> result;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (edges[i]->has_shared_edge(*other.edges[j])) {
        result.append(std::pair<UVEdge *, UVEdge *>(edges[i], other.edges[j]));
      }
    }
  }
  return result;
}

bool UVPrimitive::has_shared_edge(const UVPrimitive &other) const
{
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (edges[i]->has_shared_edge(*other.edges[j])) {
        return true;
      }
    }
  }
  return false;
}

bool UVPrimitive::has_shared_edge(const MeshData &mesh_data, const int primitive_i) const
{
  for (const UVEdge *uv_edge : edges) {
    const MLoopTri &primitive = mesh_data.looptris[primitive_i];
    int loop_1 = primitive.tri[2];
    for (int i = 0; i < 3; i++) {
      int loop_2 = primitive.tri[i];
      if (uv_edge->has_shared_edge(mesh_data.uv_map, loop_1, loop_2)) {
        return true;
      }
      loop_1 = loop_2;
    }
  }
  return false;
}

const UVVertex *UVPrimitive::get_uv_vertex(const MeshData &mesh_data,
                                           const uint8_t mesh_vert_index) const
{
  const MLoopTri &looptri = mesh_data.looptris[this->primitive_i];
  const int mesh_vertex = mesh_data.corner_verts[looptri.tri[mesh_vert_index]];
  for (const UVEdge *uv_edge : edges) {
    for (const UVVertex *uv_vert : uv_edge->vertices) {
      if (uv_vert->vertex == mesh_vertex) {
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
    const float2 &e1 = uv_edge->vertices[0]->uv;
    const float2 &e2 = uv_edge->vertices[1]->uv;
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
    const int e1 = uv_edge->vertices[0]->vertex;
    const int e2 = uv_edge->vertices[1]->vertex;
    if ((e1 == v1 && e2 == v2) || (e1 == v2 && e2 == v1)) {
      return uv_edge;
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

bool UVPrimitive::contains_uv_vertex(const UVVertex *uv_vertex) const
{
  for (UVEdge *edge : edges) {
    if (std::find(edge->vertices.begin(), edge->vertices.end(), uv_vertex) != edge->vertices.end())
    {
      return true;
    }
  }
  return false;
}

const UVVertex *UVPrimitive::get_other_uv_vertex(const UVVertex *v1, const UVVertex *v2) const
{
  BLI_assert(contains_uv_vertex(v1));
  BLI_assert(contains_uv_vertex(v2));

  for (const UVEdge *edge : edges) {
    for (const UVVertex *uv_vertex : edge->vertices) {
      if (!ELEM(uv_vertex, v1, v2)) {
        return uv_vertex;
      }
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

UVBorder UVPrimitive::extract_border() const
{
  Vector<UVBorderEdge> border_edges;
  for (UVEdge *edge : edges) {
    /* TODO remove const cast. only needed for debugging ATM. */
    UVBorderEdge border_edge(edge, const_cast<UVPrimitive *>(this));
    border_edges.append(border_edge);
  }
  return *UVBorder::extract_from_edges(border_edges);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVBorderEdge
 * \{ */
UVBorderEdge::UVBorderEdge(UVEdge *edge, UVPrimitive *uv_primitive)
    : edge(edge), uv_primitive(uv_primitive)
{
}

UVVertex *UVBorderEdge::get_uv_vertex(int index)
{
  int actual_index = reverse_order ? 1 - index : index;
  return edge->vertices[actual_index];
}

const UVVertex *UVBorderEdge::get_uv_vertex(int index) const
{
  int actual_index = reverse_order ? 1 - index : index;
  return edge->vertices[actual_index];
}

const UVVertex *UVBorderEdge::get_other_uv_vertex() const
{
  return uv_primitive->get_other_uv_vertex(edge->vertices[0], edge->vertices[1]);
}

float UVBorderEdge::length() const
{
  return len_v2v2(edge->vertices[0]->uv, edge->vertices[1]->uv);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVIslands
 * \{ */

UVIslands::UVIslands(const MeshData &mesh_data)
{
  islands.reserve(mesh_data.uv_island_len);

  for (const int64_t uv_island_id : IndexRange(mesh_data.uv_island_len)) {
    islands.append_as(UVIsland());
    UVIsland *uv_island = &islands.last();
    uv_island->id = uv_island_id;
    for (const int primitive_i : mesh_data.looptris.index_range()) {
      if (mesh_data.uv_island_ids[primitive_i] == uv_island_id) {
        add_primitive(mesh_data, *uv_island, primitive_i);
      }
    }
  }
}

void UVIslands::extract_borders()
{
  for (UVIsland &island : islands) {
    island.extract_borders();
  }
}

void UVIslands::extend_borders(const MeshData &mesh_data, const UVIslandsMask &islands_mask)
{
  ushort index = 0;
  for (UVIsland &island : islands) {
    island.extend_border(mesh_data, islands_mask, index++);
  }
}

void UVIslands::print_debug(const MeshData &mesh_data) const
{
  for (const UVIsland &island : islands) {
    island.print_debug(mesh_data);
  }
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
  return IN_RANGE(tile_uv.x, 0.0, 1.0f) && IN_RANGE(tile_uv.y, 0.0f, 1.0f);
}

float UVIslandsMask::Tile::get_pixel_size_in_uv_space() const
{
  return min_ff(1.0f / tile_resolution.x, 1.0f / tile_resolution.y);
}

static void add_uv_island(const MeshData &mesh_data,
                          UVIslandsMask::Tile &tile,
                          const UVIsland &uv_island,
                          int16_t island_index)
{
  for (const VectorList<UVPrimitive>::UsedVector &uv_primitives : uv_island.uv_primitives) {
    for (const UVPrimitive &uv_primitive : uv_primitives) {
      const MLoopTri &looptri = mesh_data.looptris[uv_primitive.primitive_i];

      rctf uv_bounds = primitive_uv_bounds(looptri, mesh_data.uv_map);
      rcti buffer_bounds;
      buffer_bounds.xmin = max_ii(
          floor((uv_bounds.xmin - tile.udim_offset.x) * tile.mask_resolution.x), 0);
      buffer_bounds.xmax = min_ii(
          ceil((uv_bounds.xmax - tile.udim_offset.x) * tile.mask_resolution.x),
          tile.mask_resolution.x - 1);
      buffer_bounds.ymin = max_ii(
          floor((uv_bounds.ymin - tile.udim_offset.y) * tile.mask_resolution.y), 0);
      buffer_bounds.ymax = min_ii(
          ceil((uv_bounds.ymax - tile.udim_offset.y) * tile.mask_resolution.y),
          tile.mask_resolution.y - 1);

      for (int y = buffer_bounds.ymin; y < buffer_bounds.ymax + 1; y++) {
        for (int x = buffer_bounds.xmin; x < buffer_bounds.xmax + 1; x++) {
          float2 uv(float(x) / tile.mask_resolution.x, float(y) / tile.mask_resolution.y);
          float3 weights;
          barycentric_weights_v2(mesh_data.uv_map[looptri.tri[0]],
                                 mesh_data.uv_map[looptri.tri[1]],
                                 mesh_data.uv_map[looptri.tri[2]],
                                 uv + tile.udim_offset,
                                 weights);
          if (!barycentric_inside_triangle_v2(weights)) {
            continue;
          }

          uint64_t offset = tile.mask_resolution.x * y + x;
          tile.mask[offset] = island_index;
        }
      }
    }
  }
}

void UVIslandsMask::add(const MeshData &mesh_data, const UVIslands &uv_islands)
{
  for (Tile &tile : tiles) {
    for (const int i : uv_islands.islands.index_range()) {
      add_uv_island(mesh_data, tile, uv_islands.islands[i], i);
    }
  }
}

void UVIslandsMask::add_tile(const float2 udim_offset, ushort2 resolution)
{
  tiles.append_as(Tile(udim_offset, resolution));
}

static bool dilate_x(UVIslandsMask::Tile &islands_mask)
{
  bool changed = false;
  const Array<uint16_t> prev_mask = islands_mask.mask;
  for (int y = 0; y < islands_mask.mask_resolution.y; y++) {
    for (int x = 0; x < islands_mask.mask_resolution.x; x++) {
      uint64_t offset = y * islands_mask.mask_resolution.x + x;
      if (prev_mask[offset] != 0xffff) {
        continue;
      }
      if (x != 0 && prev_mask[offset - 1] != 0xffff) {
        islands_mask.mask[offset] = prev_mask[offset - 1];
        changed = true;
      }
      else if (x < islands_mask.mask_resolution.x - 1 && prev_mask[offset + 1] != 0xffff) {
        islands_mask.mask[offset] = prev_mask[offset + 1];
        changed = true;
      }
    }
  }
  return changed;
}

static bool dilate_y(UVIslandsMask::Tile &islands_mask)
{
  bool changed = false;
  const Array<uint16_t> prev_mask = islands_mask.mask;
  for (int y = 0; y < islands_mask.mask_resolution.y; y++) {
    for (int x = 0; x < islands_mask.mask_resolution.x; x++) {
      uint64_t offset = y * islands_mask.mask_resolution.x + x;
      if (prev_mask[offset] != 0xffff) {
        continue;
      }
      if (y != 0 && prev_mask[offset - islands_mask.mask_resolution.x] != 0xffff) {
        islands_mask.mask[offset] = prev_mask[offset - islands_mask.mask_resolution.x];
        changed = true;
      }
      else if (y < islands_mask.mask_resolution.y - 1 &&
               prev_mask[offset + islands_mask.mask_resolution.x] != 0xffff)
      {
        islands_mask.mask[offset] = prev_mask[offset + islands_mask.mask_resolution.x];
        changed = true;
      }
    }
  }
  return changed;
}

static void dilate_tile(UVIslandsMask::Tile &tile, int max_iterations)
{
  int index = 0;
  while (index < max_iterations) {
    bool changed = dilate_x(tile);
    changed |= dilate_y(tile);
    if (!changed) {
      break;
    }
    index++;
  }
}

void UVIslandsMask::dilate(int max_iterations)
{
  for (Tile &tile : tiles) {
    dilate_tile(tile, max_iterations);
  }
}

bool UVIslandsMask::Tile::is_masked(const uint16_t island_index, const float2 uv) const
{
  float2 local_uv = uv - udim_offset;
  if (local_uv.x < 0.0f || local_uv.y < 0.0f || local_uv.x >= 1.0f || local_uv.y >= 1.0f) {
    return false;
  }
  float2 pixel_pos_f = local_uv * float2(mask_resolution.x, mask_resolution.y);
  ushort2 pixel_pos = ushort2(pixel_pos_f.x, pixel_pos_f.y);
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
