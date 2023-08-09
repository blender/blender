/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * UV Islands for PBVH Pixel extraction. When primitives share an edge they belong to the same UV
 * Island.
 *
 * \note Similar to `uvedit_islands.cc`, but optimized for PBVH painting without using BMesh for
 * performance reasons. Non-manifold meshes only (i.e. edges must have less than 3 faces).
 *
 * Polygons (face with more than 3 edges) are supported as they are split up to primitives.
 *
 * \note After the algorithm is stable the OO data structures should be converted back to use DOD
 * principles to improve reusability. Currently this is not done (yet) as during implementation it
 * was hard to follow when the algorithm evolved during several iterations. At that time we needed
 * more flexibility.
 */

#pragma once

#include <fstream>
#include <optional>

#include "BLI_array.hh"
#include "BLI_edgehash.h"
#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_vector.hh"
#include "BLI_vector_list.hh"
#include "BLI_virtual_array.hh"

#include "DNA_meshdata_types.h"

namespace blender::bke::pbvh::uv_islands {

struct MeshEdge;
struct UVBorder;
struct UVEdge;
struct UVIslands;
struct UVIslandsMask;
struct UVPrimitive;
struct MeshData;
struct UVVertex;

struct MeshEdge {
  int vert1;
  int vert2;
};

class VertToEdgeMap {
  Array<Vector<int>> edges_of_vert_;

 public:
  VertToEdgeMap() = delete;
  VertToEdgeMap(const int verts_num)
  {
    edges_of_vert_.reinitialize(verts_num);
  }

  void add(const int edge_i, const int v1, const int v2)
  {
    edges_of_vert_[v1].append(edge_i);
    edges_of_vert_[v2].append(edge_i);
  }
  Span<int> operator[](const int vert_i) const
  {
    return edges_of_vert_[vert_i];
  }
};

class EdgeToPrimitiveMap {
  Array<Vector<int>> primitives_of_edge_;

 public:
  EdgeToPrimitiveMap() = delete;
  EdgeToPrimitiveMap(const int edges_num)
  {
    primitives_of_edge_.reinitialize(edges_num);
  }

  void add(const int primitive_i, const int edge_i)
  {
    primitives_of_edge_[edge_i].append(primitive_i);
  }
  Span<int> operator[](const int edge_i) const
  {
    return primitives_of_edge_[edge_i];
  }
};

class TriangleToEdgeMap {
  Array<std::array<int, 3>> edges_of_triangle_;

 public:
  TriangleToEdgeMap() = delete;
  TriangleToEdgeMap(const int edges_num)
  {
    edges_of_triangle_.reinitialize(edges_num);
  }

  void add(const Span<int> edges, const int tri_i)
  {
    std::copy(edges.begin(), edges.end(), edges_of_triangle_[tri_i].begin());
  }
  Span<int> operator[](const int tri_i) const
  {
    return edges_of_triangle_[tri_i];
  }
};

/**
 * MeshData contains input geometry data converted in a list of primitives, edges and vertices for
 * quick access for both local space and uv space.
 */
struct MeshData {
 public:
  const Span<MLoopTri> looptris;
  const Span<int> corner_verts;
  const Span<float2> uv_map;
  const Span<float3> vert_positions;

  VertToEdgeMap vert_to_edge_map;

  Vector<MeshEdge> edges;
  EdgeToPrimitiveMap edge_to_primitive_map;

  TriangleToEdgeMap primitive_to_edge_map;

  /**
   * UV island each primitive belongs to. This is used to speed up the initial uv island
   * extraction and should not be used afterwards.
   */
  Array<int> uv_island_ids;
  /** Total number of found uv islands. */
  int64_t uv_island_len;

 public:
  explicit MeshData(Span<MLoopTri> looptris,
                    Span<int> corner_verts,
                    const Span<float2> uv_map,
                    const Span<float3> vert_positions);
};

struct UVVertex {
  int vertex;
  /* Position in uv space. */
  float2 uv;

  /* uv edges that share this UVVertex. */
  Vector<UVEdge *> uv_edges;

  struct {
    bool is_border : 1;
    bool is_extended : 1;
  } flags;

  explicit UVVertex();
  explicit UVVertex(const MeshData &mesh_data, const int loop);
};

struct UVEdge {
  std::array<UVVertex *, 2> vertices;
  Vector<UVPrimitive *, 2> uv_primitives;

  UVVertex *get_other_uv_vertex(const int vertex_index);
  bool has_shared_edge(Span<float2> uv_map, const int loop_1, const int loop_2) const;
  bool has_shared_edge(const UVEdge &other) const;
  bool has_same_vertices(const MeshEdge &edge) const;
  bool is_border_edge() const;

 private:
  bool has_shared_edge(const UVVertex &v1, const UVVertex &v2) const;
  bool has_same_vertices(const int v1, const int v2) const;
  bool has_same_uv_vertices(const UVEdge &other) const;
};

struct UVPrimitive {
  /**
   * Index of the primitive in the original mesh.
   */
  const int primitive_i;
  Vector<UVEdge *, 3> edges;

  explicit UVPrimitive(const int primitive_i);

  Vector<std::pair<UVEdge *, UVEdge *>> shared_edges(UVPrimitive &other);
  bool has_shared_edge(const UVPrimitive &other) const;
  bool has_shared_edge(const MeshData &mesh_data, int other_triangle_index) const;

  /**
   * Get the UVVertex in the order that the verts are ordered in the MeshPrimitive.
   */
  const UVVertex *get_uv_vertex(const MeshData &mesh_data, const uint8_t mesh_vert_index) const;

  /**
   * Get the UVEdge that share the given uv coordinates.
   * Will assert when no UVEdge found.
   */
  UVEdge *get_uv_edge(const float2 uv1, const float2 uv2) const;
  UVEdge *get_uv_edge(const int v1, const int v2) const;

  bool contains_uv_vertex(const UVVertex *uv_vertex) const;
  const UVVertex *get_other_uv_vertex(const UVVertex *v1, const UVVertex *v2) const;

  UVBorder extract_border() const;
};

struct UVBorderEdge {
  UVEdge *edge;
  bool tag = false;
  UVPrimitive *uv_primitive;
  /* Should the vertices of the edge be evaluated in reverse order. */
  bool reverse_order = false;

  int64_t index = -1;
  int64_t prev_index = -1;
  int64_t next_index = -1;
  int64_t border_index = -1;

  explicit UVBorderEdge(UVEdge *edge, UVPrimitive *uv_primitive);

  UVVertex *get_uv_vertex(int index);
  const UVVertex *get_uv_vertex(int index) const;

  /**
   * Get the uv vertex from the primitive that is not part of the edge.
   */
  const UVVertex *get_other_uv_vertex() const;

  float length() const;
};

struct UVBorderCorner {
  UVBorderEdge *first;
  UVBorderEdge *second;
  float angle;

  explicit UVBorderCorner(UVBorderEdge *first, UVBorderEdge *second, float angle);

  /**
   * Calculate a uv coordinate between the edges of the corner.
   *
   * 'min_uv_distance' is the minimum distance between the corner and the
   * resulting uv coordinate. The distance is in uv space.
   */
  float2 uv(float factor, float min_uv_distance);

  /**
   * Does this corner exist as 2 connected edges of the mesh.
   *
   * During the extraction phase a connection can be made in uv-space that
   * doesn't reflect to two connected edges inside the mesh.
   */
  bool connected_in_mesh() const;
  void print_debug() const;
};

struct UVBorder {
  /** Ordered list of UV Verts of the border of this island. */
  Vector<UVBorderEdge> edges;

  /**
   * Check if the border is counter clock wise from its island.
   */
  bool is_ccw() const;

  /**
   * Flip the order of the verts, changing the order between CW and CCW.
   */
  void flip_order();

  /**
   * Calculate the outside angle of the given vert.
   */
  float outside_angle(const UVBorderEdge &edge) const;

  void update_indexes(uint64_t border_index);

  static std::optional<UVBorder> extract_from_edges(Vector<UVBorderEdge> &edges);

  /** Remove edge from the border. updates the indexes. */
  void remove(int64_t index);
};

struct UVIsland {
  /**
   * Id (Index) of the UVIsland. Contains the index of this island in UVIslands.
   *
   * Useful during debugging to set a breaking condition on a specific island/vert.
   */
  int id;
  VectorList<UVVertex> uv_vertices;
  VectorList<UVEdge> uv_edges;
  VectorList<UVPrimitive> uv_primitives;
  /**
   * List of borders of this island. There can be multiple borders per island as a border could
   * be completely encapsulated by another one.
   */
  Vector<UVBorder> borders;

  /**
   * Key is mesh vert index, Value is list of UVVertices that refer to the mesh vertex with that
   * index. Map is used internally to quickly lookup similar UVVertices.
   */
  Map<int64_t, Vector<UVVertex *>> uv_vertex_lookup;

  UVVertex *lookup(const UVVertex &vertex);
  UVVertex *lookup_or_create(const UVVertex &vertex);
  UVEdge *lookup(const UVEdge &edge);
  UVEdge *lookup_or_create(const UVEdge &edge);

  /** Initialize the border attribute. */
  void extract_borders();
  /** Iterative extend border to fit the mask. */
  void extend_border(const MeshData &mesh_data,
                     const UVIslandsMask &mask,
                     const short island_index);

 private:
  void append(const UVPrimitive &primitive);

 public:
  bool has_shared_edge(const UVPrimitive &primitive) const;
  bool has_shared_edge(const MeshData &mesh_data, const int primitive_i) const;
  void extend_border(const UVPrimitive &primitive);

  /** Print a python script to the console that generates a mesh representing this UVIsland. */
  void print_debug(const MeshData &mesh_data) const;
};

struct UVIslands {
  Vector<UVIsland> islands;

  explicit UVIslands(const MeshData &mesh_data);

  void extract_borders();
  void extend_borders(const MeshData &mesh_data, const UVIslandsMask &islands_mask);
  void print_debug(const MeshData &mesh_data) const;
};

/** Mask to find the index of the UVIsland for a given UV coordinate. */
struct UVIslandsMask {

  /** Mask for each udim tile. */
  struct Tile {
    float2 udim_offset;
    ushort2 tile_resolution;
    ushort2 mask_resolution;
    Array<uint16_t> mask;

    Tile(float2 udim_offset, ushort2 tile_resolution);

    bool is_masked(const uint16_t island_index, const float2 uv) const;
    bool contains(const float2 uv) const;
    float get_pixel_size_in_uv_space() const;
  };

  Vector<Tile> tiles;

  void add_tile(float2 udim_offset, ushort2 resolution);

  /**
   * Find a tile containing the given uv coordinate.
   */
  const Tile *find_tile(const float2 uv) const;

  /**
   * Is the given uv coordinate part of the given island_index mask.
   *
   * true - part of the island mask.
   * false - not part of the island mask.
   */
  bool is_masked(const uint16_t island_index, const float2 uv) const;

  /**
   * Add the given UVIslands to the mask. Tiles should be added beforehand using the 'add_tile'
   * method.
   */
  void add(const MeshData &mesh_data, const UVIslands &islands);

  void dilate(int max_iterations);
};

}  // namespace blender::bke::pbvh::uv_islands
