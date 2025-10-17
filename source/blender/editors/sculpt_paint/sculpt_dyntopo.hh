/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_enum_flags.hh"

struct bContext;
struct BMesh;
struct Brush;
struct Depsgraph;
struct Main;
struct Object;
struct Scene;
namespace blender::ed::sculpt_paint::undo {
struct StepData;
}

namespace blender::ed::sculpt_paint::dyntopo {

enum WarnFlag {
  VDATA = (1 << 0),
  EDATA = (1 << 1),
  LDATA = (1 << 2),
  MODIFIER = (1 << 3),
};
ENUM_OPERATORS(WarnFlag);

/** Enable dynamic topology; mesh will be triangulated */
void enable_ex(Main &bmain, Depsgraph &depsgraph, Object &ob);
void disable(bContext *C, undo::StepData *undo_step);
void disable_with_undo(Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob);

/**
 * Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without.
 * Same goes for alt-key smoothing.
 */
bool stroke_is_dyntopo(const Object &object, const Brush &brush);

void triangulate(BMesh *bm);

WarnFlag check_attribute_warning(Scene &scene, Object &ob);

namespace detail_size {

/**
 * Scaling factor to match the displayed size to the actual sculpted size
 */
constexpr float RELATIVE_SCALE_FACTOR = 0.4f;
/** The relative scale of the minimum and maximum edge length. */
constexpr float EDGE_LENGTH_MIN_FACTOR = 0.4f;

/**
 * Converts from Sculpt#constant_detail to the #pbvh::Tree max edge length.
 */
float constant_to_detail_size(float constant_detail, const Object &ob);

/**
 * Converts from Sculpt#detail_percent to the #pbvh::Tree max edge length.
 */
float brush_to_detail_size(float brush_percent, float brush_radius);

/**
 * Converts from Sculpt#detail_size to the #pbvh::Tree max edge length.
 */
float relative_to_detail_size(float relative_detail,
                              float brush_radius,
                              float pixel_radius,
                              float pixel_size);

/**
 * Converts from Sculpt#constant_detail to equivalent Sculpt#detail_percent value.
 *
 * Corresponds to a change from Constant & Manual Detailing to Brush Detailing.
 */
float constant_to_brush_detail(float constant_detail, float brush_radius, const Object &ob);

/**
 * Converts from Sculpt#constant_detail to equivalent Sculpt#detail_size value.
 *
 * Corresponds to a change from Constant & Manual Detailing to Relative Detailing.
 */
float constant_to_relative_detail(float constant_detail,
                                  float brush_radius,
                                  float pixel_radius,
                                  float pixel_size,
                                  const Object &ob);
}  // namespace detail_size
}  // namespace blender::ed::sculpt_paint::dyntopo
