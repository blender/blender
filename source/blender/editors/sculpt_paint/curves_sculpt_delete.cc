/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_kdtree.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_rand.hh"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "DEG_depsgraph.h"

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_mesh.hh"
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
  CurvesBrush3D brush_3d_;
  /**
   * Need to store those in case the brush is evaluated more than once before the curves are
   * evaluated again. This can happen when the mouse is moved quickly and the brush spacing is
   * small.
   */
  Vector<float3> deformed_positions_;

  friend struct DeleteOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct DeleteOperationExecutor {
  DeleteOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  IndexMaskMemory selected_curve_memory_;
  IndexMask curve_selection_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;

  float2 brush_pos_re_;

  CurvesSurfaceTransforms transforms_;

  DeleteOperationExecutor(const bContext &C) : ctx_(C) {}

  void execute(DeleteOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    object_ = CTX_data_active_object(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &curves_id_->geometry.wrap();

    curve_selection_ = curves::retrieve_selected_curves(*curves_id_, selected_curve_memory_);

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);

    brush_pos_re_ = stroke_extension.mouse_position;

    transforms_ = CurvesSurfaceTransforms(*object_, curves_id_->surface);

    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    if (stroke_extension.is_first) {
      if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
        this->initialize_spherical_brush_reference_point();
      }
      const bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);
      self_->deformed_positions_ = deformation.positions;
    }

    Array<bool> curves_to_keep(curves_->curves_num(), true);
    if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      this->delete_projected_with_symmetry(curves_to_keep);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->delete_spherical_with_symmetry(curves_to_keep);
    }
    else {
      BLI_assert_unreachable();
    }

    IndexMaskMemory mask_memory;
    const IndexMask mask_to_keep = IndexMask::from_bools(curves_to_keep, mask_memory);

    /* Remove deleted curves from the stored deformed positions. */
    const OffsetIndices points_by_curve = curves_->points_by_curve();
    Vector<float3> new_deformed_positions;
    mask_to_keep.foreach_index([&](const int64_t i) {
      new_deformed_positions.extend(
          self_->deformed_positions_.as_span().slice(points_by_curve[i]));
    });
    self_->deformed_positions_ = std::move(new_deformed_positions);

    *curves_ = bke::curves_copy_curve_selection(*curves_, mask_to_keep, {});

    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void delete_projected_with_symmetry(MutableSpan<bool> curves_to_keep)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->delete_projected(brush_transform, curves_to_keep);
    }
  }

  void delete_projected(const float4x4 &brush_transform, MutableSpan<bool> curves_to_keep)
  {
    const float4x4 brush_transform_inv = math::invert(brush_transform);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.ptr());

    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);
    const OffsetIndices points_by_curve = curves_->points_by_curve();

    curve_selection_.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
      for (const int curve_i : segment) {
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() == 1) {
          const float3 pos_cu = math::transform_point(brush_transform_inv,
                                                      self_->deformed_positions_[points.first()]);
          float2 pos_re;
          ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.ptr());

          if (math::distance_squared(brush_pos_re_, pos_re) <= brush_radius_sq_re) {
            curves_to_keep[curve_i] = false;
          }
          continue;
        }

        for (const int segment_i : points.drop_back(1)) {
          const float3 pos1_cu = math::transform_point(brush_transform_inv,
                                                       self_->deformed_positions_[segment_i]);
          const float3 pos2_cu = math::transform_point(brush_transform_inv,
                                                       self_->deformed_positions_[segment_i + 1]);

          float2 pos1_re, pos2_re;
          ED_view3d_project_float_v2_m4(ctx_.region, pos1_cu, pos1_re, projection.ptr());
          ED_view3d_project_float_v2_m4(ctx_.region, pos2_cu, pos2_re, projection.ptr());

          const float dist_sq_re = dist_squared_to_line_segment_v2(
              brush_pos_re_, pos1_re, pos2_re);
          if (dist_sq_re <= brush_radius_sq_re) {
            curves_to_keep[curve_i] = false;
            break;
          }
        }
      }
    });
  }

  void delete_spherical_with_symmetry(MutableSpan<bool> curves_to_keep)
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.ptr());

    float3 brush_wo;
    ED_view3d_win_to_3d(
        ctx_.v3d,
        ctx_.region,
        math::transform_point(transforms_.curves_to_world, self_->brush_3d_.position_cu),
        brush_pos_re_,
        brush_wo);
    const float3 brush_cu = math::transform_point(transforms_.world_to_curves, brush_wo);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));

    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->delete_spherical(math::transform_point(brush_transform, brush_cu), curves_to_keep);
    }
  }

  void delete_spherical(const float3 &brush_cu, MutableSpan<bool> curves_to_keep)
  {
    const float brush_radius_cu = self_->brush_3d_.radius_cu * brush_radius_factor_;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);
    const OffsetIndices points_by_curve = curves_->points_by_curve();

    curve_selection_.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
      for (const int curve_i : segment) {
        const IndexRange points = points_by_curve[curve_i];

        if (points.size() == 1) {
          const float3 &pos_cu = self_->deformed_positions_[points.first()];
          const float distance_sq_cu = math::distance_squared(pos_cu, brush_cu);
          if (distance_sq_cu < brush_radius_sq_cu) {
            curves_to_keep[curve_i] = false;
          }
          continue;
        }

        for (const int segment_i : points.drop_back(1)) {
          const float3 &pos1_cu = self_->deformed_positions_[segment_i];
          const float3 &pos2_cu = self_->deformed_positions_[segment_i + 1];

          const float distance_sq_cu = dist_squared_to_line_segment_v3(brush_cu, pos1_cu, pos2_cu);
          if (distance_sq_cu > brush_radius_sq_cu) {
            continue;
          }
          curves_to_keep[curve_i] = false;
          break;
        }
      }
    });
  }

  void initialize_spherical_brush_reference_point()
  {
    std::optional<CurvesBrush3D> brush_3d = sample_curves_3d_brush(*ctx_.depsgraph,
                                                                   *ctx_.region,
                                                                   *ctx_.v3d,
                                                                   *ctx_.rv3d,
                                                                   *object_,
                                                                   brush_pos_re_,
                                                                   brush_radius_base_re_);
    if (brush_3d.has_value()) {
      self_->brush_3d_ = *brush_3d;
    }
  }
};

void DeleteOperation::on_stroke_extended(const bContext &C,
                                         const StrokeExtension &stroke_extension)
{
  DeleteOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_delete_operation()
{
  return std::make_unique<DeleteOperation>();
}

}  // namespace blender::ed::sculpt_paint
