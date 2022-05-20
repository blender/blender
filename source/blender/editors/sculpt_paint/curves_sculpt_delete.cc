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

#include "WM_api.h"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * cu: Local space of the curves object that is being edited.
 * wo: World space.
 * re: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

using blender::bke::CurvesGeometry;

class DeleteOperation : public CurvesSculptStrokeOperation {
 private:
  float2 brush_pos_prev_re_;

  CurvesBrush3D brush_3d_;

  friend struct DeleteOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct DeleteOperationExecutor {
  DeleteOperation *self_ = nullptr;
  const Depsgraph *depsgraph_ = nullptr;
  const Scene *scene_ = nullptr;
  ARegion *region_ = nullptr;
  const View3D *v3d_ = nullptr;
  const RegionView3D *rv3d_ = nullptr;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_re_;

  float2 brush_pos_re_;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  void execute(DeleteOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {

    self_ = &self;
    depsgraph_ = CTX_data_depsgraph_pointer(&C);
    scene_ = CTX_data_scene(&C);
    object_ = CTX_data_active_object(&C);
    region_ = CTX_wm_region(&C);
    v3d_ = CTX_wm_view3d(&C);
    rv3d_ = CTX_wm_region_view3d(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);

    curves_sculpt_ = scene_->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_re_ = brush_radius_get(*scene_, *brush_, stroke_extension);

    brush_pos_re_ = stroke_extension.mouse_position;

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    if (stroke_extension.is_first) {
      if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
        this->initialize_spherical_brush_reference_point();
      }
    }

    Array<bool> curves_to_delete(curves_->curves_num(), false);
    if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      this->delete_projected_with_symmetry(curves_to_delete);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->delete_spherical_with_symmetry(curves_to_delete);
    }
    else {
      BLI_assert_unreachable();
    }

    Vector<int64_t> indices;
    const IndexMask mask = index_mask_ops::find_indices_based_on_predicate(
        curves_->curves_range(), 4096, indices, [&](const int curve_i) {
          return curves_to_delete[curve_i];
        });

    curves_->remove_curves(mask);

    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(region_);
  }

  void delete_projected_with_symmetry(MutableSpan<bool> curves_to_delete)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->delete_projected(brush_transform, curves_to_delete);
    }
  }

  void delete_projected(const float4x4 &brush_transform, MutableSpan<bool> curves_to_delete)
  {
    const float4x4 brush_transform_inv = brush_transform.inverted();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d_, object_, projection.values);

    Span<float3> positions_cu = curves_->positions();

    const float brush_radius_sq_re = pow2f(brush_radius_re_);

    threading::parallel_for(curves_->curves_range(), 512, [&](IndexRange curve_range) {
      for (const int curve_i : curve_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int segment_i : points.drop_back(1)) {
          const float3 pos1_cu = brush_transform_inv * positions_cu[segment_i];
          const float3 pos2_cu = brush_transform_inv * positions_cu[segment_i + 1];

          float2 pos1_re, pos2_re;
          ED_view3d_project_float_v2_m4(region_, pos1_cu, pos1_re, projection.values);
          ED_view3d_project_float_v2_m4(region_, pos2_cu, pos2_re, projection.values);

          const float dist_sq_re = dist_squared_to_line_segment_v2(
              brush_pos_re_, pos1_re, pos2_re);
          if (dist_sq_re <= brush_radius_sq_re) {
            curves_to_delete[curve_i] = true;
            break;
          }
        }
      }
    });
  }

  void delete_spherical_with_symmetry(MutableSpan<bool> curves_to_delete)
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d_, object_, projection.values);

    float3 brush_wo;
    ED_view3d_win_to_3d(v3d_,
                        region_,
                        curves_to_world_mat_ * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_wo);
    const float3 brush_cu = world_to_curves_mat_ * brush_wo;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));

    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->delete_spherical(brush_transform * brush_cu, curves_to_delete);
    }
  }

  void delete_spherical(const float3 &brush_cu, MutableSpan<bool> curves_to_delete)
  {
    Span<float3> positions_cu = curves_->positions();

    const float brush_radius_cu = self_->brush_3d_.radius_cu;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    threading::parallel_for(curves_->curves_range(), 512, [&](IndexRange curve_range) {
      for (const int curve_i : curve_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int segment_i : points.drop_back(1)) {
          const float3 &pos1_cu = positions_cu[segment_i];
          const float3 &pos2_cu = positions_cu[segment_i + 1];

          const float distance_sq_cu = dist_squared_to_line_segment_v3(brush_cu, pos1_cu, pos2_cu);
          if (distance_sq_cu > brush_radius_sq_cu) {
            continue;
          }
          curves_to_delete[curve_i] = true;
          break;
        }
      }
    });
  }

  void initialize_spherical_brush_reference_point()
  {
    std::optional<CurvesBrush3D> brush_3d = sample_curves_3d_brush(
        *depsgraph_, *region_, *v3d_, *rv3d_, *object_, brush_pos_re_, brush_radius_re_);
    if (brush_3d.has_value()) {
      self_->brush_3d_ = *brush_3d;
    }
  }
};

void DeleteOperation::on_stroke_extended(const bContext &C,
                                         const StrokeExtension &stroke_extension)
{
  DeleteOperationExecutor executor;
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_delete_operation()
{
  return std::make_unique<DeleteOperation>();
}

}  // namespace blender::ed::sculpt_paint
