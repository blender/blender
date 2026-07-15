/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * UV Islands for pbvh::Tree Pixel extraction. When primitives share an edge they belong to the
 * same UV Island.
 *
 * \note Similar to `uvedit_islands.cc`, but optimized for pbvh::Tree painting without using BMesh
 * for performance reasons. Non-manifold meshes only (i.e. edges must have less than 3 faces).
 *
 * Polygons (face with more than 3 edges) are supported as they are split up to primitives.
 *
 * \note After the algorithm is stable the OO data structures should be converted back to use DOD
 * principles to improve reusability. Currently this is not done (yet) as during implementation it
 * was hard to follow when the algorithm evolved during several iterations. At that time we needed
 * more flexibility.
 */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_vector.hh"
#include "BLI_vector_list.hh"

namespace blender::bke::pbvh::uv_islands {

struct UVBorder;
struct UVEdge;
struct UVIslandsMask;
struct UVPrimitive;
struct MeshData;
struct UVIsland;
struct UVVert;

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
  OffsetIndices<int> faces;
  Span<int3> corner_tris;
  Span<int> corner_verts;
  Span<float2> uv_map;
  Span<float3> vert_positions;

  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  GroupedSpan<int> vert_to_edge_map;

  Vector<int2> edges;
  Array<int> edge_to_primitive_offsets;
  Array<int> edge_to_primitive_indices;
  GroupedSpan<int> edge_to_primitive_map;

  TriangleToEdgeMap primitive_to_edge_map;

  /**
   * UV island each primitive belongs to. This is used to speed up the initial uv island
   * extraction and should not be used afterwards.
   */
  Array<int> uv_island_ids;
  /** Total number of found uv islands. */
  int64_t uv_island_len;

  explicit MeshData(OffsetIndices<int> faces,
                    Span<int3> corner_tris,
                    Span<int> corner_verts,
                    Span<float2> uv_map,
                    Span<float3> vert_positions);

  bool is_edge_manifold(const int edge_id) const
  {
    return edge_to_primitive_map[edge_id].size() == 2;
  }
};

struct UVVert {
  int vert;
  /* Position in uv space. */
  float2 uv;

  /* uv edges that share this UVVert. */
  Vector<UVEdge *> uv_edges;

  struct {
    bool is_border : 1;
    bool is_extended : 1;
  } flags;

  explicit UVVert();
  explicit UVVert(const MeshData &mesh_data, int corner);
};

struct UVEdge {
  std::array<UVVert *, 2> verts;
  Vector<int, 2> uv_primitive_indices;

  UVVert *get_other_uv_vert(int vert);
  bool has_same_verts(const int2 &edge) const;
  bool is_border_edge() const;

 private:
  bool has_same_verts(int vert1, int vert2) const;
};

struct UVPrimitive {
  /**
   * Index of the primitive in the original mesh.
   */
  const int primitive_i;
  Vector<UVEdge *, 3> edges;

  explicit UVPrimitive(int primitive_i);

  /**
   * Get the UVVert in the order that the verts are ordered in the MeshPrimitive.
   */
  const UVVert *get_uv_vert(const MeshData &mesh_data, uint8_t mesh_vert_index) const;

  /**
   * Get the UVEdge that share the given uv coordinates.
   * Will assert when no UVEdge found.
   */
  UVEdge *get_uv_edge(float2 uv1, float2 uv2) const;
  UVEdge *get_uv_edge(int v1, int v2) const;

  bool contains_uv_vert(const UVVert *uv_vert) const;
  const UVVert *get_other_uv_vert(const UVVert *v1, const UVVert *v2) const;
};

struct UVBorderEdge {
  UVEdge *edge;
  /* Index into UVIsland::uv_primitives. */
  int uv_primitive;
  /* Should the vertices of the edge be evaluated in reverse order. */
  bool reverse_order = false;
  bool removed = false;

  /* Previous and next edge, forming a double linked list. */
  UVBorderEdge *prev = nullptr;
  UVBorderEdge *next = nullptr;
  int64_t border_index = -1;
  /* Stable ID for tie-break in the priority queue. */
  int64_t order = -1;

  explicit UVBorderEdge(UVEdge *edge, int uv_primitive);

  UVVert *get_uv_vert(int index);
  const UVVert *get_uv_vert(int index) const;

  /**
   * Get the uv vertex from the primitive that is not part of the edge.
   */
  const UVVert *get_other_uv_vert(const UVIsland &island) const;

  bool is_extendable() const;

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
  /** Ordered list of UV Verts of the (original) border of this island. */
  Vector<UVBorderEdge> edges;
  /* Edges added by border extension. */
  VectorList<UVBorderEdge> edges_extend_border;

  /**
   * Check if the border is counter clock wise from its island.
   */
  bool is_ccw(const UVIsland &island) const;

  /**
   * Flip the order of the verts, changing the order between CW and CCW.
   */
  void flip_order();

  /**
   * Calculate the outside angle of the given vert.
   */
  float outside_angle(const UVBorderEdge &edge) const;

  /** Setup prev and next pointers to turn edges into a linked list. */
  void setup_links(int64_t border_index);
};

struct UVIsland {
  /**
   * Id (Index) of the UVIsland. Contains the index of this island in the islands array.
   *
   * Useful during debugging to set a breaking condition on a specific island/vert.
   */
  int id;
  VectorList<UVVert> uv_verts;
  VectorList<UVEdge> uv_edges;
  Vector<UVPrimitive> uv_primitives;
  /**
   * List of borders of this island. There can be multiple borders per island as a border could
   * be completely encapsulated by another one.
   */
  Vector<UVBorder> borders;

  /**
   * Key is mesh vert index, Value is list of UVVertices that refer to the mesh vertex with that
   * index. Map is used internally to quickly lookup similar UVVertices.
   */
  Map<int64_t, Vector<UVVert *>> uv_vert_lookup;

  UVVert *lookup(const UVVert &vert);
  UVVert *lookup_or_create(const UVVert &vert);
  UVEdge *lookup(const UVEdge &edge);
  UVEdge *lookup_or_create(const UVEdge &edge);

  /** Initialize the border attribute. */
  void extract_borders();
  /** Iterative extend border to fit the mask. */
  void extend_border(const MeshData &mesh_data, const UVIslandsMask &mask, short island_index);

  /** Print a python script to the console that generates a mesh representing this UVIsland. */
  void print_debug(const MeshData &mesh_data) const;
};

Array<UVIsland> build_uv_islands(const MeshData &mesh_data,
                                 GroupedSpan<int> tris_by_island,
                                 const UVIslandsMask &uv_masks);

/** Mask to find the index of the UVIsland for a given UV coordinate. */
struct UVIslandsMask {

  /** Mask for each udim tile. */
  struct Tile {
    float2 udim_offset;
    ushort2 tile_resolution;
    ushort2 mask_resolution;
    Array<uint16_t> mask;

    Tile(float2 udim_offset, ushort2 tile_resolution);

    /** Lookup by UV. */
    bool is_masked(uint16_t island_index, float2 uv) const;
    /** Lookup by resolved mask pixel coordinate. */
    bool is_masked(const uint16_t island_index, const int mask_x, const int mask_y) const
    {
      return mask[int64_t(mask_y) * mask_resolution.x + mask_x] == island_index;
    }
    bool contains(float2 uv) const;
    float get_pixel_size_in_uv_space() const;
  };

  Vector<Tile> tiles;

  void add_tile(float2 udim_offset, ushort2 resolution);

  /**
   * Find a tile containing the given uv coordinate.
   */
  const Tile *find_tile(float2 uv) const;

  /**
   * Is the given uv coordinate part of the given island_index mask.
   *
   * true - part of the island mask.
   * false - not part of the island mask.
   */
  bool is_masked(uint16_t island_index, float2 uv) const;

  /**
   * Add the given UV islands to the mask. Tiles should be added beforehand using the 'add_tile'
   * method.
   */
  void add(const MeshData &mesh_data, GroupedSpan<int> tris_by_island);

  void dilate(int max_iterations);
};

}  // namespace blender::bke::pbvh::uv_islands
