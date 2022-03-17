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
 * Moves individual points under the brush and does a length preservation step afterwards.
 */
class CombOperation : public CurvesSculptStrokeOperation {
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
    const float mouse_diff_len = math::length(mouse_diff);

    threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
      for (const int curve_i : curves_range) {
        const IndexRange curve_points = curves.range_for_curve(curve_i);
        /* Compute lengths of the segments. Those are used to make sure that the lengths don't
         * change. */
        Vector<float, 16> segment_lengths(curve_points.size() - 1);
        for (const int segment_i : IndexRange(curve_points.size() - 1)) {
          const float3 &p1 = positions[curve_points[segment_i]];
          const float3 &p2 = positions[curve_points[segment_i] + 1];
          const float length = math::distance(p1, p2);
          segment_lengths[segment_i] = length;
        }
        bool curve_changed = false;
        for (const int point_i : curve_points.drop_front(1)) {
          const float3 old_position = positions[point_i];

          /* Find the position of the point in screen space. */
          float2 old_position_screen;
          ED_view3d_project_float_v2_m4(
              region, old_position, old_position_screen, projection.values);

          /* Project the point onto the line drawn by the mouse. Note, it's projected on the
           * infinite line, not only on the line segment. */
          float2 old_position_screen_proj;
          /* t is 0 when the point is closest to the previous mouse position and 1 when it's
           * closest to the current mouse position. */
          const float t = closest_to_line_v2(
              old_position_screen_proj, old_position_screen, mouse_prev, mouse_cur);

          /* Compute the distance to the mouse line segment. */
          const float2 old_position_screen_proj_segment = mouse_prev +
                                                          std::clamp(t, 0.0f, 1.0f) * mouse_diff;
          const float distance_screen = math::distance(old_position_screen,
                                                       old_position_screen_proj_segment);
          if (distance_screen > brush_radius) {
            /* Ignore the point because it's too far away. */
            continue;
          }
          /* Compute a falloff that is based on how far along the point along the last stroke
           * segment is. */
          const float t_overshoot = brush_radius / mouse_diff_len;
          const float t_falloff = 1.0f - std::max(t, 0.0f) / (1.0f + t_overshoot);
          /* A falloff that is based on how far away the point is from the stroke. */
          const float radius_falloff = pow2f(1.0f - distance_screen / brush_radius);
          /* Combine the different falloffs and brush strength. */
          const float weight = brush_strength * t_falloff * radius_falloff;

          /* Offset the old point position in screen space and transform it back into 3D space. */
          const float2 new_position_screen = old_position_screen + mouse_diff * weight;
          float3 new_position;
          ED_view3d_win_to_3d(
              v3d, region, ob_mat * old_position, new_position_screen, new_position);
          new_position = ob_imat * new_position;
          positions[point_i] = new_position;

          curve_changed = true;
        }
        if (!curve_changed) {
          continue;
        }
        /* Ensure that the length of each segment stays the same. */
        for (const int segment_i : IndexRange(curve_points.size() - 1)) {
          const float3 &p1 = positions[curve_points[segment_i]];
          float3 &p2 = positions[curve_points[segment_i] + 1];
          const float3 direction = math::normalize(p2 - p1);
          const float desired_length = segment_lengths[segment_i];
          p2 = p1 + direction * desired_length;
        }
      }
    });

    curves.tag_positions_changed();
    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);
  }
};

std::unique_ptr<CurvesSculptStrokeOperation> new_comb_operation()
{
  return std::make_unique<CombOperation>();
}

}  // namespace blender::ed::sculpt_paint
