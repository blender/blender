/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#pragma once

#include <memory>

#include "BLI_array.hh"
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
int settings_hash(const Object &ob, const Cache &automasking);

bool brush_type_can_reuse_automask(int sculpt_brush_type);

}  // namespace blender::ed::sculpt_paint::auto_mask
