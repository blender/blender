/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_float4x4.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_kdtree.h"
#include "BLI_rand.hh"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "DEG_depsgraph.h"

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_paint.h"
#include "BKE_spline.hh"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

namespace blender::ed::sculpt_paint {

using blender::bke::CurvesGeometry;

/**
 * Drags the tip point of each curve and resamples the rest of the curve.
 */
class SnakeHookOperation : public CurvesSculptStrokeOperation {
 private:
  float2 last_mouse_position_;

 public:
  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension) override
  {
    BLI_SCOPED_DEFER([&]() { last_mouse_position_ = stroke_extension.mouse_position; });

    if (stroke_extension.is_first) {
      return;
    }

    Scene &scene = *CTX_data_scene(C);
    Object &object = *CTX_data_active_object(C);
    ARegion *region = CTX_wm_region(C);
    View3D *v3d = CTX_wm_view3d(C);
    RegionView3D *rv3d = CTX_wm_region_view3d(C);

    CurvesSculpt &curves_sculpt = *scene.toolsettings->curves_sculpt;
    Brush &brush = *BKE_paint_brush(&curves_sculpt.paint);
    const float brush_radius = BKE_brush_size_get(&scene, &brush);
    const float brush_strength = BKE_brush_alpha_get(&scene, &brush);

    const float4x4 ob_mat = object.obmat;
    const float4x4 ob_imat = ob_mat.inverted();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d, &object, projection.values);

    Curves &curves_id = *static_cast<Curves *>(object.data);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
    MutableSpan<float3> positions = curves.positions();

    const float2 mouse_prev = last_mouse_position_;
    const float2 mouse_cur = stroke_extension.mouse_position;
    const float2 mouse_diff = mouse_cur - mouse_prev;

    threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
      for (const int curve_i : curves_range) {
        const IndexRange curve_points = curves.range_for_curve(curve_i);
        const int last_point_i = curve_points.last();

        const float3 old_position = positions[last_point_i];

        float2 old_position_screen;
        ED_view3d_project_float_v2_m4(
            region, old_position, old_position_screen, projection.values);

        const float distance_screen = math::distance(old_position_screen, mouse_prev);
        if (distance_screen > brush_radius) {
          continue;
        }

        const float radius_falloff = pow2f(1.0f - distance_screen / brush_radius);
        const float weight = brush_strength * radius_falloff;

        const float2 new_position_screen = old_position_screen + mouse_diff * weight;
        float3 new_position;
        ED_view3d_win_to_3d(v3d, region, ob_mat * old_position, new_position_screen, new_position);
        new_position = ob_imat * new_position;

        this->move_last_point_and_resample(positions, curve_points, new_position);
      }
    });

    curves.tag_positions_changed();
    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);
  }

  void move_last_point_and_resample(MutableSpan<float3> positions,
                                    const IndexRange curve_points,
                                    const float3 &new_last_point_position) const
  {
    Vector<float> old_lengths;
    old_lengths.append(0.0f);
    /* Used to (1) normalize the segment sizes over time and (2) support making zero-length
     * segments */
    const float extra_length = 0.001f;
    for (const int segment_i : IndexRange(curve_points.size() - 1)) {
      const float3 &p1 = positions[curve_points[segment_i]];
      const float3 &p2 = positions[curve_points[segment_i] + 1];
      const float length = math::distance(p1, p2);
      old_lengths.append(old_lengths.last() + length + extra_length);
    }
    Vector<float> point_factors;
    for (float &old_length : old_lengths) {
      point_factors.append(old_length / old_lengths.last());
    }

    PolySpline new_spline;
    new_spline.resize(curve_points.size());
    MutableSpan<float3> new_spline_positions = new_spline.positions();
    for (const int i : IndexRange(curve_points.size() - 1)) {
      new_spline_positions[i] = positions[curve_points[i]];
    }
    new_spline_positions.last() = new_last_point_position;
    new_spline.mark_cache_invalid();

    for (const int i : IndexRange(curve_points.size())) {
      const float factor = point_factors[i];
      const Spline::LookupResult lookup = new_spline.lookup_evaluated_factor(factor);
      const float index_factor = lookup.evaluated_index + lookup.factor;
      float3 p;
      new_spline.sample_with_index_factors<float3>(
          new_spline_positions, {&index_factor, 1}, {&p, 1});
      positions[curve_points[i]] = p;
    }
  }
};

std::unique_ptr<CurvesSculptStrokeOperation> new_snake_hook_operation()
{
  return std::make_unique<SnakeHookOperation>();
}

}  // namespace blender::ed::sculpt_paint
