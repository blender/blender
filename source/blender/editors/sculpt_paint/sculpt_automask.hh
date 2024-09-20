/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#pragma once

#include <memory>

#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"
#include "BLI_sys_types.h"

#include "DNA_brush_enums.h"

struct BMVert;
struct Brush;
struct CurveMapping;
struct Depsgraph;
struct Object;
struct Sculpt;
struct SculptSession;
namespace blender::bke::pbvh {
struct MeshNode;
struct GridsNode;
struct BMeshNode;
}  // namespace blender::bke::pbvh

namespace blender::ed::sculpt_paint::auto_mask {

struct Settings {
  /* eAutomasking_flag. */
  int flags;
  int initial_face_set;
  int initial_island_nr;

  float cavity_factor;
  int cavity_blur_steps;
  CurveMapping *cavity_curve;

  float start_normal_limit, start_normal_falloff;
  float view_normal_limit, view_normal_falloff;

  bool topology_use_brush_limit;
};

struct Cache {
  Settings settings;

  /** Cached factor for auto-masking modes that are implemented to process the entire mesh. */
  Array<float> factor;

  enum class OcclusionValue : int8_t {
    Unknown = 0,
    Visible = 1,
    Occluded = 2,
  };
  /**
   * Cached occlusion values for each vertex. Since calculating the occlusion is so expensive,
   * it's only calculated at the beginning of a stroke and stored for later.
   *
   * \todo Ideally the "unknown" state would be stored per node rather than per vertex, with a
   * lock-protected `Map<const bke::pbvh::Node , BitVector<>>` for example. Currently complications
   * with face domain auto-masking prevent this though. This array can't be a bitmap because it's
   * written to from multiple threads at the same time.
   */
  Array<OcclusionValue> occlusion;

  /**
   * Cached cavity factor values for each vertex.
   *
   * \note -1 means the vertex value still needs to be calculated.
   */
  Array<float> cavity_factor;

  bool can_reuse_mask;
  uchar current_stroke_id;
};

/**
 * Returns the auto-masking cache depending on the active tool. Used for code that can run both for
 * brushes and filter.
 */
const Cache *active_cache_get(const SculptSession &ss);

/**
 * Creates and initializes an auto-masking cache.
 *
 * For auto-masking modes that cannot be calculated in real time,
 * data is also stored at the vertex level prior to the stroke starting.
 */
std::unique_ptr<Cache> cache_init(const Depsgraph &depsgraph, const Sculpt &sd, Object &ob);
std::unique_ptr<Cache> cache_init(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  const Brush *brush,
                                  Object &ob);

bool mode_enabled(const Sculpt &sd, const Brush *br, eAutomasking_flag mode);
bool is_enabled(const Sculpt &sd, const Object &object, const Brush *br);

bool needs_normal(const SculptSession &ss, const Sculpt &sd, const Brush *brush);

bool brush_type_can_reuse_automask(int sculpt_brush_type);

/**
 * Calculate all auto-masking influence on each vertex.
 */
void calc_vert_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const Cache &cache,
                       const bke::pbvh::MeshNode &node,
                       Span<int> verts,
                       MutableSpan<float> factors);
inline void calc_vert_factors(const Depsgraph &depsgraph,
                              const Object &object,
                              const Cache *cache,
                              const bke::pbvh::MeshNode &node,
                              Span<int> verts,
                              MutableSpan<float> factors)
{
  if (cache) {
    calc_vert_factors(depsgraph, object, *cache, node, verts, factors);
  }
}
void calc_grids_factors(const Depsgraph &depsgraph,
                        const Object &object,
                        const Cache &cache,
                        const bke::pbvh::GridsNode &node,
                        Span<int> grids,
                        MutableSpan<float> factors);
inline void calc_grids_factors(const Depsgraph &depsgraph,
                               const Object &object,
                               const Cache *cache,
                               const bke::pbvh::GridsNode &node,
                               Span<int> grids,
                               MutableSpan<float> factors)
{
  if (cache) {
    calc_grids_factors(depsgraph, object, *cache, node, grids, factors);
  }
}
void calc_vert_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const Cache &cache,
                       const bke::pbvh::BMeshNode &node,
                       const Set<BMVert *, 0> &verts,
                       MutableSpan<float> factors);
inline void calc_vert_factors(const Depsgraph &depsgraph,
                              const Object &object,
                              const Cache *cache,
                              const bke::pbvh::BMeshNode &node,
                              const Set<BMVert *, 0> &verts,
                              MutableSpan<float> factors)
{
  if (cache) {
    calc_vert_factors(depsgraph, object, *cache, node, verts, factors);
  }
}

/**
 * Calculate all auto-masking influence on each face.
 */
void calc_face_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       OffsetIndices<int> faces,
                       Span<int> corner_verts,
                       const Cache &cache,
                       const bke::pbvh::MeshNode &node,
                       Span<int> face_indices,
                       MutableSpan<float> factors);

}  // namespace blender::ed::sculpt_paint::auto_mask
