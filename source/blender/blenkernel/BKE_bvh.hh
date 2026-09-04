/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_function_ref.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_vector_types.hh"

#include <limits>
#include <optional>

struct RTCDeviceTy;
struct RTCSceneTy;

namespace blender {

struct Mesh;

namespace bke::bvh {

struct Ray {
  float3 origin;
  /* The ray direction does not have to be normalized. */
  float3 direction;
  /* Scale factor for the ray direction (the ray length if the direction is normalized). */
  float dist_max = std::numeric_limits<float>::max();

  Ray() = default;
  Ray(const float3 &origin, const float3 &direction) : origin(origin), direction(direction) {}
  Ray(const float3 &origin, const float3 &direction, const float radius)
      : origin(origin), direction(direction), dist_max(radius)
  {
  }
};

struct RayHit {
  /* Ng. Not normalized. */
  float3 normal;
  /** Triangle barycentric coordinates of the hit. */
  float3 bary_coord;
  /** Index of the hit element (i.e. triangle). */
  int index;
  float distance;
  float3 position(const Ray &ray) const
  {
    return ray.origin + ray.direction * distance;
  }
};

struct ClosestPointResult {
  /** Location of the closest point. */
  float3 position;
  /** Triangle barycentric coordinates of the closest point. Only used for triangle trees. */
  float3 bary_coord;
  /** Index of the closest element. */
  uint32_t index;
  /* Currently unused. */
  uint32_t geomID;
};

/**
 * A wrapper around Embree's BVH tree and #BLI_kdopbvh.hh. Besides a simpler and more friendly API
 * compared to Embree, this provides storage for index mapping for trees created from subsets of a
 * geometry.
 */
class Tree {
 public:
  /* Type erased non Embree implementation to avoid extra includes. */
  struct FallbackTree {
    virtual ~FallbackTree() = default;
  };

  /** The type of element the tree is built from, which query results refer to. */
  enum class ElemType : int8_t {
    Tris,
    Points,
    Edges,
  };

  /**
   * Geometry data referenced by the Embree user geometry used for #ElemType::Points and
   * #ElemType::Edges. The data is not copied, so the geometry it comes from must outlive the tree.
   * That is already required elsewhere, since the tree is invalidated whenever positions change.
   */
  struct UserGeometryData {
    Span<float3> positions;
    /** Only used for #ElemType::Edges. */
    Span<int2> edges;
    /** Maps primitives to elements. Empty when the tree contains every element. */
    Span<int> index_map;

    /** The index of the element referenced by an Embree primitive. */
    int element(const uint32_t prim_id) const
    {
      return index_map.is_empty() ? int(prim_id) : index_map[prim_id];
    }
  };

 private:
  /**
   * Geometry type for the tree.
   * \note Eventually the tree should be able to contain more than one geometry, in which case this
   * would be stored as an array. For now the tree only handles one geometry, so a single value is
   * enough and simplifies code switching behavior based on the tree type.
   */
  ElemType elem_type_ = ElemType::Tris;
  /** Embree device and scene. Unused when Embree is not available. */
  [[maybe_unused]] RTCDeviceTy *rtc_device_ = nullptr;
  [[maybe_unused]] RTCSceneTy *rtc_scene_ = nullptr;
  /**
   * Map indices from each geometry in the Embree scene to another set of indices. Used e.g. when
   * the tree references a subset of a mesh but must keep track of the original global indices.
   */
  Vector<Array<int, 0>, 1> index_map_by_geom_;
  /**
   * Data referenced by custom geometry in the Embree scene. Embree stores a pointer to each of
   * these structs, so they are allocated separately for pointer stability across tree moves.
   */
  Vector<std::unique_ptr<UserGeometryData>, 1> user_data_by_geom_;

  /** Used when Embree is not available. */
  std::unique_ptr<FallbackTree> fallback_tree_;

 public:
  Tree();
  Tree(const Tree &) = delete;
  Tree &operator=(const Tree &) = delete;
  Tree(Tree &&);
  Tree &operator=(Tree &&);
  ~Tree();

  /**
   * Create a BVH tree from a subset of the mesh faces. #from_single_mesh should be used when all
   * faces are contained in the mask.
   * \param map_global_indices: Record and later return the indices from the full mesh rather than
   * the index in the masked faces.
   */
  static Tree from_tris(const Mesh &mesh, const IndexMask &mask, bool map_global_indices);
  /** Create a BVH tree from the entire mesh. */
  static Tree from_single_mesh(const Mesh &mesh);
  /**
   * Create a BVH tree from a subset of the given points. Query results reference indices in the
   * full #positions array rather than indices in the mask.
   * \note Memory referenced by the span must live at least as long as the tree.`
   */
  static Tree from_points(Span<float3> positions, const IndexMask &mask);
  /**
   * Create a BVH tree from a subset of the given edges. Query results reference indices in the
   * full #edges array rather than indices in the mask.
   * \note Memory referenced by the span must live at least as long as the tree.`
   */
  static Tree from_edges(Span<float3> positions, Span<int2> edges, const IndexMask &mask);

  /** Intersect a single ray against the tree. Only supported for #ElemType::Tris. */
  std::optional<RayHit> ray_intersect(const Ray &ray) const;

  /** Call a callback for every ray intersection. Only supported for #ElemType::Tris. */
  void ray_intersect_all(const Ray &ray, FunctionRef<void(const RayHit &)> fn) const;

  /**
   * Find the closest surface point to a given position.
   * \param radius: Elements at or farther than this distance are ignored.
   */
  std::optional<ClosestPointResult> closest_point(
      const float3 &point, float radius = std::numeric_limits<float>::max()) const;

  /** Call a callback for every element within a given radius. */
  void range_query(const float3 &point, const float radius, FunctionRef<bool(int)> fn) const;
};

}  // namespace bke::bvh
}  // namespace blender
