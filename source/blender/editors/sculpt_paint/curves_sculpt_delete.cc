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

class DeleteOperation : public CurvesSculptStrokeOperation {
 private:
  float2 last_mouse_position_;

 public:
  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension) override
  {
    Scene &scene = *CTX_data_scene(C);
    Object &object = *CTX_data_active_object(C);
    ARegion *region = CTX_wm_region(C);
    RegionView3D *rv3d = CTX_wm_region_view3d(C);

    CurvesSculpt &curves_sculpt = *scene.toolsettings->curves_sculpt;
    Brush &brush = *BKE_paint_brush(&curves_sculpt.paint);
    const float brush_radius = BKE_brush_size_get(&scene, &brush);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d, &object, projection.values);

    Curves &curves_id = *static_cast<Curves *>(object.data);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
    MutableSpan<float3> positions = curves.positions();

    const float2 mouse_start = stroke_extension.is_first ? stroke_extension.mouse_position :
                                                           last_mouse_position_;
    const float2 mouse_end = stroke_extension.mouse_position;

    /* Find indices of curves that have to be removed. */
    Vector<int64_t> indices;
    const IndexMask curves_to_remove = index_mask_ops::find_indices_based_on_predicate(
        curves.curves_range(), 512, indices, [&](const int curve_i) {
          const IndexRange point_range = curves.range_for_curve(curve_i);
          for (const int segment_i : IndexRange(point_range.size() - 1)) {
            const float3 pos1 = positions[point_range[segment_i]];
            const float3 pos2 = positions[point_range[segment_i + 1]];

            float2 pos1_proj, pos2_proj;
            ED_view3d_project_float_v2_m4(region, pos1, pos1_proj, projection.values);
            ED_view3d_project_float_v2_m4(region, pos2, pos2_proj, projection.values);

            const float dist = dist_seg_seg_v2(pos1_proj, pos2_proj, mouse_start, mouse_end);
            if (dist <= brush_radius) {
              return true;
            }
          }
          return false;
        });

    curves.remove_curves(curves_to_remove);

    curves.tag_positions_changed();
    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);

    last_mouse_position_ = stroke_extension.mouse_position;
  }
};

std::unique_ptr<CurvesSculptStrokeOperation> new_delete_operation()
{
  return std::make_unique<DeleteOperation>();
}

}  // namespace blender::ed::sculpt_paint
