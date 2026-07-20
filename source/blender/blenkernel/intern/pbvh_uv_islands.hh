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

/**
 * MeshData contains input geometry data converted in a list of primitives, edges and vertices for
 * quick access for both local space and uv space.
 */
struct MeshData {
 public:
  OffsetIndices<int> faces;
  Span<int3> corner_tris;
  Span<int> corner_verts;
  Span<int> corner_edges;
  Span<int2> mesh_edges;
  Span<float2> uv_map;
  Span<float3> vert_positions;

  GroupedSpan<int> vert_to_face_map;

  Array<int> edge_to_face_offsets;
  Array<int> edge_to_face_indices;
  GroupedSpan<int> edge_to_face_map;

  /** UV island each primitive belongs to. */
  Array<int> uv_island_ids;
  /** Total number of found uv islands. */
  int64_t uv_island_len;
  /** True if a mesh edge is a UV island border. */
  Array<bool> uv_edge_is_border;

  explicit MeshData(OffsetIndices<int> faces,
                    Span<int3> corner_tris,
                    Span<int> corner_verts,
                    Span<int> corner_edges,
                    Span<int2> mesh_edges,
                    GroupedSpan<int> vert_to_face_map,
                    Span<float2> uv_map,
                    Span<float3> vert_positions);

  bool is_edge_manifold(const int edge_id) const
  {
    return edge_to_face_map[edge_id].size() == 2;
  }
};

struct UVVert {
  int vert;
  /* Position in uv space. */
  float2 uv;

  /* uv edges that share this UVVert. */
  Vector<int> uv_edges;

  struct {
    bool is_border : 1;
    bool is_extended : 1;
  } flags;

  explicit UVVert();
  explicit UVVert(const MeshData &mesh_data, int corner);
};

struct UVEdge {
  std::array<int, 2> verts;
  Vector<int, 2> uv_primitive_indices;
  bool is_border = false;

  int get_other_uv_vert(const UVIsland &island, int vert);
  bool has_same_verts(const UVIsland &island, const int2 &edge) const;

 private:
  bool has_same_verts(const UVIsland &island, int vert1, int vert2) const;
};

struct UVPrimitive {
  /**
   * Index of the primitive in the original mesh.
   */
  int primitive_i;
  std::array<int, 3> edges;

  explicit UVPrimitive(int primitive_i);

  /**
   * Get the UVVert in the order that the verts are ordered in the MeshPrimitive.
   */
  int get_uv_vert(const UVIsland &island,
                  const MeshData &mesh_data,
                  uint8_t mesh_vert_index) const;

  /**
   * Get the UVEdge that share the given uv coordinates.
   * Will assert when no UVEdge found.
   */
  int get_uv_edge(const UVIsland &island, float2 uv1, float2 uv2) const;
  int get_uv_edge(const UVIsland &island, int v1, int v2) const;

  bool contains_uv_vert(const UVIsland &island, const int uv_vert) const;
  int get_other_uv_vert(const UVIsland &island, const int v1, const int v2) const;
};

struct UVBorderEdge {
  int uv_edge_i;
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

  explicit UVBorderEdge(int uv_edge_i, int uv_primitive);

  int get_uv_vert(const UVIsland &island, int index) const;

  /**
   * Get the uv vertex from the primitive that is not part of the edge.
   */
  int get_other_uv_vert(const UVIsland &island) const;

  bool is_extendable(const UVIsland &island) const;

  float length(const UVIsland &island) const;
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
  float2 uv(const UVIsland &island, float factor, float min_uv_distance);

  /**
   * Does this corner exist as 2 connected edges of the mesh.
   *
   * During the extraction phase a connection can be made in uv-space that
   * doesn't reflect to two connected edges inside the mesh.
   */
  bool connected_in_mesh(const UVIsland &island) const;
  void print_debug(const UVIsland &island) const;
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
  float outside_angle(const UVIsland &island, const UVBorderEdge &edge) const;

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
  Vector<UVVert> uv_verts;
  Vector<UVEdge> uv_edges;
  Vector<UVPrimitive> uv_primitives;
  /** Number of primitive, before border extension. */
  int64_t num_original_primitives = 0;
  /**
   * List of borders of this island. There can be multiple borders per island as a border could
   * be completely encapsulated by another one.
   */
  Vector<UVBorder> borders;

  /**
   * Key is mesh vert index, Value is list of UVVertices that refer to the mesh vertex with that
   * index. Map is used internally to quickly lookup similar UVVertices.
   */
  Map<int, Vector<int>> uv_vert_lookup;

  int lookup(const UVVert &vert);
  int lookup_or_create(const UVVert &vert);
  int lookup(const UVEdge &edge);
  int lookup_or_create(const UVEdge &edge);

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
   * Rasterize the UV islands into the mask, using the per-island triangle grouping.
   * Tiles should be added beforehand using the 'add_tile' method.
   */
  void add(const MeshData &mesh_data, GroupedSpan<int> tris_by_island);

  void dilate(int max_iterations);
};

}  // namespace blender::bke::pbvh::uv_islands
