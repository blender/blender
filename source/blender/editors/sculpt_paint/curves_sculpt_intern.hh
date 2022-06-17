/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "curves_sculpt_intern.h"
#include "paint_intern.h"

#include "BLI_math_vector.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute.h"
#include "BKE_curves.hh"

struct ARegion;
struct RegionView3D;
struct Depsgraph;
struct View3D;
struct Object;
struct Brush;
struct Scene;

namespace blender::ed::sculpt_paint {

using bke::CurvesGeometry;

struct StrokeExtension {
  bool is_first;
  float2 mouse_position;
  float pressure;
};

float brush_radius_factor(const Brush &brush, const StrokeExtension &stroke_extension);
float brush_radius_get(const Scene &scene,
                       const Brush &brush,
                       const StrokeExtension &stroke_extension);

float brush_strength_factor(const Brush &brush, const StrokeExtension &stroke_extension);
float brush_strength_get(const Scene &scene,
                         const Brush &brush,
                         const StrokeExtension &stroke_extension);

/**
 * Base class for stroke based operations in curves sculpt mode.
 */
class CurvesSculptStrokeOperation {
 public:
  virtual ~CurvesSculptStrokeOperation() = default;
  virtual void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) = 0;
};

std::unique_ptr<CurvesSculptStrokeOperation> new_add_operation(const bContext &C,
                                                               ReportList *reports);
std::unique_ptr<CurvesSculptStrokeOperation> new_comb_operation();
std::unique_ptr<CurvesSculptStrokeOperation> new_delete_operation();
std::unique_ptr<CurvesSculptStrokeOperation> new_snake_hook_operation();
std::unique_ptr<CurvesSculptStrokeOperation> new_grow_shrink_operation(
    const BrushStrokeMode brush_mode, const bContext &C);
std::unique_ptr<CurvesSculptStrokeOperation> new_selection_paint_operation(
    const BrushStrokeMode brush_mode, const bContext &C);

struct CurvesBrush3D {
  float3 position_cu;
  float radius_cu;
};

/**
 * Find 3d brush position based on cursor position for curves sculpting.
 */
std::optional<CurvesBrush3D> sample_curves_3d_brush(const Depsgraph &depsgraph,
                                                    const ARegion &region,
                                                    const View3D &v3d,
                                                    const RegionView3D &rv3d,
                                                    const Object &curves_object,
                                                    const float2 &brush_pos_re,
                                                    const float brush_radius_re);

Vector<float4x4> get_symmetry_brush_transforms(eCurvesSymmetryType symmetry);

/**
 * Get the floating point selection on the curve domain, averaged from points if necessary.
 */
VArray<float> get_curves_selection(const Curves &curves_id);

/**
 * Get the floating point selection on the curve domain, copied from curves if necessary.
 */
VArray<float> get_point_selection(const Curves &curves_id);

/**
 * Find curves that have any point selected (a selection factor greater than zero),
 * or curves that have their own selection factor greater than zero.
 */
IndexMask retrieve_selected_curves(const Curves &curves_id, Vector<int64_t> &r_indices);

void move_last_point_and_resample(MutableSpan<float3> positions, const float3 &new_last_position);

class CurvesSculptCommonContext {
 public:
  const Depsgraph *depsgraph = nullptr;
  const Scene *scene = nullptr;
  ARegion *region = nullptr;
  const View3D *v3d = nullptr;
  const RegionView3D *rv3d = nullptr;

  CurvesSculptCommonContext(const bContext &C);
};

struct CurvesSculptTransforms {
  float4x4 curves_to_world;
  float4x4 curves_to_surface;
  float4x4 world_to_curves;
  float4x4 world_to_surface;
  float4x4 surface_to_world;
  float4x4 surface_to_curves;
  float4x4 surface_to_curves_normal;

  CurvesSculptTransforms() = default;
  CurvesSculptTransforms(const Object &curves_ob, const Object *surface_ob);
};

}  // namespace blender::ed::sculpt_paint
