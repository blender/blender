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
#include "BLI_sys_types.h"

#include "DNA_brush_enums.h"

struct Brush;
struct CurveMapping;
struct Depsgraph;
struct Object;
struct Sculpt;
struct SculptSession;

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

  /* Cached factor for automasking modes that are implemented to process the entire mesh. */
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

/* Returns the automasking cache depending on the active tool. Used for code that can run both for
 * brushes and filter. */
const Cache *active_cache_get(const SculptSession &ss);

/**
 * Creates and initializes an automasking cache.
 *
 * For automasking modes that cannot be calculated in real time,
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

}  // namespace blender::ed::sculpt_paint::auto_mask
