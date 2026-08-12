/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "RNA_types.hh"

#include "BKE_paint.hh"

#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::vwpaint {
bool test_brush_angle_falloff(const Brush &brush, float angle_cos);
bool use_normal(const VPaint &vp);

bool brush_use_accumulate_ex(const Brush &brush, eObjectMode ob_mode);
bool brush_use_accumulate(const VPaint &vp);

void get_brush_alpha_data(const SculptSession &ss,
                          const Paint &paint,
                          const Brush &brush,
                          float *r_brush_size_pressure,
                          float *r_brush_alpha_value,
                          float *r_brush_alpha_pressure);

void init_stroke(
    const wmOperator &op, Main &bmain, Paint &paint, Depsgraph &depsgraph, Object &ob);
StrokeToggleSettings create_toggle_settings(const wmOperator &op, Main &bmain, Paint &paint);

IndexMask pbvh_gather_generic(const Depsgraph &depsgraph,
                              const Object &ob,
                              const VPaint &wp,
                              const Brush &brush,
                              IndexMaskMemory &memory);

bool mode_toggle_poll_test(bContext *C);

void smooth_brush_toggle_off(Paint *paint, StrokeCache *cache);
void smooth_brush_toggle_on(Main *bmain, Paint *paint, StrokeToggleSettings &toggle_settings);

void create_stroke_cache();
/** Initialize the stroke cache variants from operator properties. */
void update_cache_variants(
    Depsgraph &depsgraph, ViewContext &vc, VPaint &vp, Object &ob, Base &base, PointerRNA *ptr);
/** Initialize the stroke cache invariants from operator properties. */
void update_cache_invariants(VPaint &vp, SculptSession &ss, wmOperator *op, const float mval[2]);
void last_stroke_update(const float location[3], Paint &paint);
}  // namespace blender::ed::sculpt_paint::vwpaint
