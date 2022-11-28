/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <fstream>
#include <optional>

#include "BLI_array.hh"
#include "BLI_edgehash.h"
#include "BLI_float3x3.hh"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_math_vec_types.hh"
#include "BLI_rect.h"
#include "BLI_vector.hh"
#include "BLI_vector_list.hh"

#include "DNA_meshdata_types.h"


namespace blender::bke::uv_islands {

struct MeshEdge;
struct MeshPrimitive;
struct UVBorder;
struct UVEdge;
struct UVIslands;
struct UVIslandsMask;
struct UVPrimitive;
struct UVPrimitiveEdge;
struct UVVertex;

struct MeshVertex {
  int64_t v;
  Vector<MeshEdge *> edges;
};

struct MeshUVVert {
  MeshVertex *vertex;
  float2 uv;
  int64_t loop;
};

struct MeshEdge {
  MeshVertex *vert1;
  MeshVertex *vert2;
  Vector<MeshPrimitive *> primitives;
};

/** Represents a triangle in 3d space (MLoopTri) */
struct MeshPrimitive {
  int64_t index;
  int64_t poly;
  Vector<MeshEdge *, 3> edges;
  Vector<MeshUVVert, 3> vertices;

  /**
   * UV island this primitive belongs to. This is used to speed up the initial uv island
   * extraction, but should not be used when extending uv islands.
   */
  int64_t uv_island_id;

  MeshUVVert *get_other_uv_vertex(const MeshVertex *v1, const MeshVertex *v2)
  {
    BLI_assert(vertices[0].vertex == v1 || vertices[1].vertex == v1 || vertices[2].vertex == v1);
    BLI_assert(vertices[0].vertex == v2 || vertices[1].vertex == v2 || vertices[2].vertex == v2);
    for (MeshUVVert &uv_vertex : vertices) {
      if (uv_vertex.vertex != v1 && uv_vertex.vertex != v2) {
        return &uv_vertex;
      }
    }
    return nullptr;
  }

  rctf uv_bounds() const;

  bool has_shared_uv_edge(const MeshPrimitive *other) const
  {
    int shared_uv_verts = 0;
    for (const MeshUVVert &vert : vertices) {
      for (const MeshUVVert &other_vert : other->vertices) {
        if (vert.uv == other_vert.uv) {
          shared_uv_verts += 1;
        }
      }
    }
    return shared_uv_verts >= 2;
  }
};

/**
 * MeshData contains input geometry data converted in a list of primitives, edges and vertices for
 * quick access for both local space and uv space.
 */
struct MeshData {
 public:
  const MLoopTri *looptri;
  const int64_t looptri_len;
  const int64_t vert_len;
  const MLoop *mloop;
  const MLoopUV *mloopuv;

 public:
  Vector<MeshPrimitive> primitives;
  Vector<MeshEdge> edges;
  Vector<MeshVertex> vertices;
  /** Total number of uv islands detected. */
  int64_t uv_island_len;

  explicit MeshData(const MLoopTri *looptri,
                    const int64_t looptri_len,
                    const int64_t vert_len,
                    const MLoop *mloop,
                    const MLoopUV *mloopuv)
      : looptri(looptri),
        looptri_len(looptri_len),
        vert_len(vert_len),
        mloop(mloop),
        mloopuv(mloopuv)
  {
    init_vertices();
    init_primitives();
    init_edges();
    init_primitive_uv_island_ids();
  }

  void init_vertices()
  {
    vertices.reserve(vert_len);
    for (int64_t i = 0; i < vert_len; i++) {
      MeshVertex vert;
      vert.v = i;
      vertices.append(vert);
    }
  }

  void init_primitives()
  {
    primitives.reserve(looptri_len);
    for (int64_t i = 0; i < looptri_len; i++) {
      const MLoopTri &tri = looptri[i];
      MeshPrimitive primitive;
      primitive.index = i;
      primitive.poly = tri.poly;

      for (int j = 0; j < 3; j++) {
        MeshUVVert uv_vert;
        uv_vert.loop = tri.tri[j];
        uv_vert.vertex = &vertices[mloop[uv_vert.loop].v];
        uv_vert.uv = mloopuv[uv_vert.loop].uv;
        primitive.vertices.append(uv_vert);
      }
      primitives.append(primitive);
    }
  }

  void init_edges()
  {
    edges.reserve(looptri_len * 2);
    EdgeHash *eh = BLI_edgehash_new_ex(__func__, looptri_len * 3);
    for (int64_t i = 0; i < looptri_len; i++) {
      const MLoopTri &tri = looptri[i];
      MeshPrimitive &primitive = primitives[i];
      for (int j = 0; j < 3; j++) {
        int v1 = mloop[tri.tri[j]].v;
        int v2 = mloop[tri.tri[(j + 1) % 3]].v;
        /* TODO: Use lookup_ptr to be able to store edge 0. */
        void *v = BLI_edgehash_lookup(eh, v1, v2);
        int64_t edge_index;
        if (v == nullptr) {
          edge_index = edges.size();
          BLI_edgehash_insert(eh, v1, v2, POINTER_FROM_INT(edge_index + 1));
          MeshEdge edge;
          edge.vert1 = &vertices[v1];
          edge.vert2 = &vertices[v2];
          edges.append(edge);
          MeshEdge *edge_ptr = &edges.last();
          vertices[v1].edges.append(edge_ptr);
          vertices[v2].edges.append(edge_ptr);
        }
        else {
          edge_index = POINTER_AS_INT(v) - 1;
        }

        MeshEdge *edge = &edges[edge_index];
        edge->primitives.append(&primitive);
        primitive.edges.append(edge);
      }
    }
    BLI_edgehash_free(eh, nullptr);
  }

  static const int64_t INVALID_UV_ISLAND_ID = -1;
  /**
   * NOTE: doesn't support weird topology where unconnected mesh primitives share the same uv
   * island. For a accurate implementation we should use implement an uv_prim_lookup.
   */
  static void extract_uv_neighbors(Vector<MeshPrimitive *> &prims_to_add, MeshPrimitive *primitive)
  {
    for (MeshEdge *edge : primitive->edges) {
      for (MeshPrimitive *other_primitive : edge->primitives) {
        if (primitive == other_primitive) {
          continue;
        }
        if (other_primitive->uv_island_id != MeshData::INVALID_UV_ISLAND_ID) {
          continue;
        }

        if (primitive->has_shared_uv_edge(other_primitive)) {
          prims_to_add.append(other_primitive);
        }
      }
    }
  }

  void init_primitive_uv_island_ids()
  {
    for (MeshPrimitive &primitive : primitives) {
      primitive.uv_island_id = INVALID_UV_ISLAND_ID;
    }

    int64_t uv_island_id = 0;
    Vector<MeshPrimitive *> prims_to_add;
    for (MeshPrimitive &primitive : primitives) {
      /* Early exit when uv island id is already extracted during uv neighbor extractions. */
      if (primitive.uv_island_id != INVALID_UV_ISLAND_ID) {
        continue;
      }

      prims_to_add.append(&primitive);
      while (!prims_to_add.is_empty()) {
        MeshPrimitive *primitive = prims_to_add.pop_last();
        primitive->uv_island_id = uv_island_id;
        extract_uv_neighbors(prims_to_add, primitive);
      }
      uv_island_id++;
    }
    uv_island_len = uv_island_id;
  }
};

struct UVVertex {
  MeshVertex *vertex;
  /* Position in uv space. */
  float2 uv;

  /* uv edges that share this UVVertex. */
  Vector<UVEdge *> uv_edges;

  struct {
    bool is_border : 1;
    bool is_extended : 1;
  } flags;

  explicit UVVertex()
  {
    flags.is_border = false;
    flags.is_extended = false;
  }

  explicit UVVertex(const MeshUVVert &vert) : vertex(vert.vertex), uv(vert.uv)
  {
    flags.is_border = false;
    flags.is_extended = false;
  }
};

struct UVEdge {
  std::array<UVVertex *, 2> vertices;
  Vector<UVPrimitive *, 2> uv_primitives;

  bool has_shared_edge(const MeshUVVert &v1, const MeshUVVert &v2) const
  {
    return (vertices[0]->uv == v1.uv && vertices[1]->uv == v2.uv) ||
           (vertices[0]->uv == v2.uv && vertices[1]->uv == v1.uv);
  }

  bool has_shared_edge(const UVVertex &v1, const UVVertex &v2) const
  {
    return (vertices[0]->uv == v1.uv && vertices[1]->uv == v2.uv) ||
           (vertices[0]->uv == v2.uv && vertices[1]->uv == v1.uv);
  }

  bool has_shared_edge(const UVEdge &other) const
  {
    return has_shared_edge(*other.vertices[0], *other.vertices[1]);
  }

  bool has_same_vertices(const MeshVertex &vert1, const MeshVertex &vert2) const
  {
    return (vertices[0]->vertex == &vert1 && vertices[1]->vertex == &vert2) ||
           (vertices[0]->vertex == &vert2 && vertices[1]->vertex == &vert1);
  }

  bool has_same_uv_vertices(const UVEdge &other) const
  {
    return has_shared_edge(other) &&
           has_same_vertices(*other.vertices[0]->vertex, *other.vertices[1]->vertex);
    ;
  }

  bool has_same_vertices(const MeshEdge &edge) const
  {
    return has_same_vertices(*edge.vert1, *edge.vert2);
  }

  bool is_border_edge() const
  {
    return uv_primitives.size() == 1;
  }

  void append_to_uv_vertices()
  {
    for (UVVertex *vertex : vertices) {
      vertex->uv_edges.append_non_duplicates(this);
    }
  }

  UVVertex *get_other_uv_vertex(const MeshVertex *vertex)
  {
    if (vertices[0]->vertex == vertex) {
      return vertices[1];
    }
    return vertices[0];
  }
};

struct UVPrimitive {
  /**
   * Index of the primitive in the original mesh.
   */
  MeshPrimitive *primitive;
  Vector<UVEdge *, 3> edges;

  explicit UVPrimitive(MeshPrimitive *primitive) : primitive(primitive)
  {
  }

  void append_to_uv_edges()
  {
    for (UVEdge *uv_edge : edges) {
      uv_edge->uv_primitives.append_non_duplicates(this);
    }
  }
  void append_to_uv_vertices()
  {
    for (UVEdge *uv_edge : edges) {
      uv_edge->append_to_uv_vertices();
    }
  }

  Vector<std::pair<UVEdge *, UVEdge *>> shared_edges(UVPrimitive &other)
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

  bool has_shared_edge(const UVPrimitive &other) const
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

  bool has_shared_edge(const MeshPrimitive &primitive) const
  {
    for (const UVEdge *uv_edge : edges) {
      const MeshUVVert *v1 = &primitive.vertices.last();
      for (int i = 0; i < primitive.vertices.size(); i++) {
        const MeshUVVert *v2 = &primitive.vertices[i];
        if (uv_edge->has_shared_edge(*v1, *v2)) {
          return true;
        }
        v1 = v2;
      }
    }
    return false;
  }

  /**
   * Get the UVVertex in the order that the verts are ordered in the MeshPrimitive.
   */
  const UVVertex *get_uv_vertex(const uint8_t mesh_vert_index) const
  {
    const MeshVertex *mesh_vertex = primitive->vertices[mesh_vert_index].vertex;
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

  /**
   * Get the UVEdge that share the given uv coordinates.
   * Will assert when no UVEdge found.
   */
  UVEdge *get_uv_edge(const float2 uv1, const float2 uv2) const
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

  UVEdge *get_uv_edge(const MeshVertex *v1, const MeshVertex *v2) const
  {
    for (UVEdge *uv_edge : edges) {
      const MeshVertex *e1 = uv_edge->vertices[0]->vertex;
      const MeshVertex *e2 = uv_edge->vertices[1]->vertex;
      if ((e1 == v1 && e2 == v2) || (e1 == v2 && e2 == v1)) {
        return uv_edge;
      }
    }
    BLI_assert_unreachable();
    return nullptr;
  }

  const bool contains_uv_vertex(const UVVertex *uv_vertex) const
  {
    for (UVEdge *edge : edges) {
      if (std::find(edge->vertices.begin(), edge->vertices.end(), uv_vertex) !=
          edge->vertices.end()) {
        return true;
      }
    }
    return false;
  }

  const UVVertex *get_other_uv_vertex(const UVVertex *v1, const UVVertex *v2) const
  {
    BLI_assert(contains_uv_vertex(v1));
    BLI_assert(contains_uv_vertex(v2));

    for (const UVEdge *edge : edges) {
      for (const UVVertex *uv_vertex : edge->vertices) {
        if (uv_vertex != v1 && uv_vertex != v2) {
          return uv_vertex;
        }
      }
    }
    BLI_assert_unreachable();
    return nullptr;
  }

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

  explicit UVBorderEdge(UVEdge *edge, UVPrimitive *uv_primitive)
      : edge(edge), uv_primitive(uv_primitive)
  {
  }

  UVVertex *get_uv_vertex(int index)
  {
    int actual_index = reverse_order ? 1 - index : index;
    return edge->vertices[actual_index];
  }

  const UVVertex *get_uv_vertex(int index) const
  {
    int actual_index = reverse_order ? 1 - index : index;
    return edge->vertices[actual_index];
  }

  /**
   * Get the uv vertex from the primitive that is not part of the edge.
   */
  const UVVertex *get_other_uv_vertex() const
  {
    return uv_primitive->get_other_uv_vertex(edge->vertices[0], edge->vertices[1]);
  }

  float length() const
  {
    return len_v2v2(edge->vertices[0]->uv, edge->vertices[1]->uv);
  }
};

struct UVBorderCorner {
  UVBorderEdge *first;
  UVBorderEdge *second;
  float angle;

  UVBorderCorner(UVBorderEdge *first, UVBorderEdge *second, float angle)
      : first(first), second(second), angle(angle)
  {
  }

  /**
   * Calculate a uv coordinate between the edges of the corner.
   *
   * 'min_uv_distance' is the minimum distance between the corner and the
   * resulting uv coordinate. The distance is in uv space.
   */
  float2 uv(float factor, float min_uv_distance);
};

struct UVBorder {
  /** Ordered list of UV Verts of the border of this island. */
  // TODO: support multiple rings + order (CW, CCW)
  Vector<UVBorderEdge> edges;

  /**
   * Check if the border is counter clock wise from its island.
   */
  bool is_ccw() const;

  /**
   * Flip the order of the verts, changing the order between CW and CCW.
   */
  void flip();

  /**
   * Calculate the outside angle of the given vert.
   */
  float outside_angle(const UVBorderEdge &vert) const;

  void update_indexes(uint64_t border_index);

  static std::optional<UVBorder> extract_from_edges(Vector<UVBorderEdge> &edges);

  /** Remove edge from the border. updates the indexes. */
  void remove(int64_t index)
  {
    /* Could read the border_index from any border edge as they are consistent. */
    uint64_t border_index = edges[0].border_index;
    edges.remove(index);
    update_indexes(border_index);
  }
};

struct UVIsland {
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

  UVVertex *lookup(const UVVertex &vertex)
  {
    int64_t vert_index = vertex.vertex->v;
    Vector<UVVertex *> &vertices = uv_vertex_lookup.lookup_or_add_default(vert_index);
    for (UVVertex *v : vertices) {
      if (v->uv == vertex.uv) {
        return v;
      }
    }
    return nullptr;
  }

  UVVertex *lookup_or_create(const UVVertex &vertex)
  {
    UVVertex *found_vertex = lookup(vertex);
    if (found_vertex != nullptr) {
      return found_vertex;
    }

    uv_vertices.append(vertex);
    UVVertex *result = &uv_vertices.last();
    result->uv_edges.clear();
    /* v is already a key. Ensured by UVIsland::lookup in this method. */
    uv_vertex_lookup.lookup(vertex.vertex->v).append(result);
    return result;
  }

  UVEdge *lookup(const UVEdge &edge)
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

  UVEdge *lookup_or_create(const UVEdge &edge)
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

  /** Initialize the border attribute. */
  void extract_borders();
  /** Iterative extend border to fit the mask. */
  void extend_border(const UVIslandsMask &mask, const short island_index);

 private:
  void append(const UVPrimitive &primitive)
  {
    uv_primitives.append(primitive);
    UVPrimitive *new_prim_ptr = &uv_primitives.last();
    for (int i = 0; i < 3; i++) {
      UVEdge *other_edge = primitive.edges[i];
      UVEdge uv_edge_template;
      uv_edge_template.vertices[0] = lookup_or_create(*other_edge->vertices[0]);
      uv_edge_template.vertices[1] = lookup_or_create(*other_edge->vertices[1]);
      new_prim_ptr->edges[i] = lookup_or_create(uv_edge_template);
      new_prim_ptr->edges[i]->append_to_uv_vertices();
      new_prim_ptr->edges[i]->uv_primitives.append(new_prim_ptr);
    }
  }

 public:
  bool has_shared_edge(const UVPrimitive &primitive) const
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

  bool has_shared_edge(const MeshPrimitive &primitive) const
  {
    for (const VectorList<UVPrimitive>::UsedVector &primitives : uv_primitives) {
      for (const UVPrimitive &prim : primitives) {
        if (prim.has_shared_edge(primitive)) {
          return true;
        }
      }
    }
    return false;
  }

  const void extend_border(const UVPrimitive &primitive)
  {
    for (const VectorList<UVPrimitive>::UsedVector &primitives : uv_primitives) {
      for (const UVPrimitive &prim : primitives) {
        if (prim.has_shared_edge(primitive)) {
          append(primitive);
        }
      }
    }
  }
};

struct UVIslands {
  Vector<UVIsland> islands;

  explicit UVIslands(MeshData &mesh_data);

  void extract_borders();
  void extend_borders(const UVIslandsMask &islands_mask);
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
  void add(const UVIslands &islands);

  void dilate(int max_iterations);
};

}  // namespace blender::bke::uv_islands
