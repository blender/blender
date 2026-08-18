/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  /** Triangle barycentric coordinates of the closest point. */
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

 private:
  /** Embree device and scene. */
  RTCDeviceTy *rtc_device_ = nullptr;
  RTCSceneTy *rtc_scene_ = nullptr;
  /**
   * Map indices from each geometry in the Embree scene to another set of indices. Used e.g. when
   * the tree references a subset of a mesh but must keep track of the original global indices.
   */
  Vector<Array<int, 0>> index_map_by_geom_;

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

  /** Intersect a single ray against the tree. */
  std::optional<RayHit> ray_intersect(const Ray &ray) const;

  /** Call a callback for every ray intersection. */
  void ray_intersect_all(const Ray &ray, FunctionRef<void(const RayHit &)> fn) const;

  /** Find the closest surface point to a given position. */
  std::optional<ClosestPointResult> closest_point(
      const float3 &point, float radius = std::numeric_limits<float>::max()) const;

  /** Call a callback for every element within a given radius. */
  void range_query(const float3 &point, const float radius, FunctionRef<bool(int)> fn) const;
};

}  // namespace bke::bvh
}  // namespace blender
