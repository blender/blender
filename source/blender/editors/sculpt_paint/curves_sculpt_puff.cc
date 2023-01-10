/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_crazyspace.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "WM_api.h"

#include "BLI_length_parameterize.hh"

#include "GEO_add_curves_on_mesh.hh"

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

class PuffOperation : public CurvesSculptStrokeOperation {
 private:
  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  /** Length of each segment indexed by the index of the first point in the segment. */
  Array<float> segment_lengths_cu_;

  friend struct PuffOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct PuffOperationExecutor {
  PuffOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  VArray<float> point_factors_;
  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;
  float2 brush_pos_re_;

  eBrushFalloffShape falloff_shape_;

  CurvesSurfaceTransforms transforms_;

  Object *surface_ob_ = nullptr;
  Mesh *surface_ = nullptr;
  Span<float3> surface_positions_;
  Span<MLoop> surface_loops_;
  Span<MLoopTri> surface_looptris_;
  Span<float3> corner_normals_su_;
  BVHTreeFromMesh surface_bvh_;

  PuffOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(PuffOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(C, stroke_extension);
    self_ = &self;

    object_ = CTX_data_active_object(&C);
    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }
    if (curves_id_->surface == nullptr || curves_id_->surface->type != OB_MESH) {
      return;
    }

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    point_factors_ = curves_->attributes().lookup_or_default<float>(
        ".selection", ATTR_DOMAIN_POINT, 1.0f);
    curve_selection_ = curves::retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    falloff_shape_ = static_cast<eBrushFalloffShape>(brush_->falloff_shape);

    surface_ob_ = curves_id_->surface;
    surface_ = static_cast<Mesh *>(surface_ob_->data);

    transforms_ = CurvesSurfaceTransforms(*object_, surface_ob_);

    if (!CustomData_has_layer(&surface_->ldata, CD_NORMAL)) {
      BKE_mesh_calc_normals_split(surface_);
    }
    corner_normals_su_ = {
        reinterpret_cast<const float3 *>(CustomData_get_layer(&surface_->ldata, CD_NORMAL)),
        surface_->totloop};

    surface_positions_ = surface_->vert_positions();
    surface_loops_ = surface_->loops();
    surface_looptris_ = surface_->looptris();
    BKE_bvhtree_from_mesh_get(&surface_bvh_, surface_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_); });

    if (stroke_extension.is_first) {
      this->initialize_segment_lengths();
      if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
        self.brush_3d_ = *sample_curves_3d_brush(*ctx_.depsgraph,
                                                 *ctx_.region,
                                                 *ctx_.v3d,
                                                 *ctx_.rv3d,
                                                 *object_,
                                                 brush_pos_re_,
                                                 brush_radius_base_re_);
      }
    }

    Array<float> curve_weights(curve_selection_.size(), 0.0f);

    if (falloff_shape_ == PAINT_FALLOFF_SHAPE_TUBE) {
      this->find_curve_weights_projected_with_symmetry(curve_weights);
    }
    else if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->find_curves_weights_spherical_with_symmetry(curve_weights);
    }
    else {
      BLI_assert_unreachable();
    }

    this->puff(curve_weights);
    this->restore_segment_lengths();

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void find_curve_weights_projected_with_symmetry(MutableSpan<float> r_curve_weights)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->find_curve_weights_projected(brush_transform, r_curve_weights);
    }
  }

  void find_curve_weights_projected(const float4x4 &brush_transform,
                                    MutableSpan<float> r_curve_weights)
  {
    const float4x4 brush_transform_inv = brush_transform.inverted();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_selection_i : range) {
        const int curve_i = curve_selection_[curve_selection_i];
        const IndexRange points = curves_->points_for_curve(curve_i);
        const float3 first_pos_cu = brush_transform_inv * deformation.positions[points[0]];
        float2 prev_pos_re;
        ED_view3d_project_float_v2_m4(ctx_.region, first_pos_cu, prev_pos_re, projection.values);
        for (const int point_i : points.drop_front(1)) {
          const float3 pos_cu = brush_transform_inv * deformation.positions[point_i];
          float2 pos_re;
          ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
          BLI_SCOPED_DEFER([&]() { prev_pos_re = pos_re; });

          const float dist_to_brush_sq_re = dist_squared_to_line_segment_v2(
              brush_pos_re_, prev_pos_re, pos_re);
          if (dist_to_brush_sq_re > brush_radius_sq_re) {
            continue;
          }

          const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, dist_to_brush_re, brush_radius_re);
          const float weight = radius_falloff;
          math::max_inplace(r_curve_weights[curve_selection_i], weight);
        }
      }
    });
  }

  void find_curves_weights_spherical_with_symmetry(MutableSpan<float> r_curve_weights)
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    float3 brush_pos_wo;
    ED_view3d_win_to_3d(ctx_.v3d,
                        ctx_.region,
                        transforms_.curves_to_world * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_pos_wo);
    const float3 brush_pos_cu = transforms_.world_to_curves * brush_pos_wo;
    const float brush_radius_cu = self_->brush_3d_.radius_cu * brush_radius_factor_;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->find_curves_weights_spherical(
          brush_transform * brush_pos_cu, brush_radius_cu, r_curve_weights);
    }
  }

  void find_curves_weights_spherical(const float3 &brush_pos_cu,
                                     const float brush_radius_cu,
                                     MutableSpan<float> r_curve_weights)
  {
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_selection_i : range) {
        const int curve_i = curve_selection_[curve_selection_i];
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int point_i : points.drop_front(1)) {
          const float3 &prev_pos_cu = deformation.positions[point_i - 1];
          const float3 &pos_cu = deformation.positions[point_i];
          const float dist_to_brush_sq_cu = dist_squared_to_line_segment_v3(
              brush_pos_cu, prev_pos_cu, pos_cu);
          if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
            continue;
          }

          const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_cu);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, dist_to_brush_cu, brush_radius_cu);
          const float weight = radius_falloff;
          math::max_inplace(r_curve_weights[curve_selection_i], weight);
        }
      }
    });
  }

  void puff(const Span<float> curve_weights)
  {
    BLI_assert(curve_weights.size() == curve_selection_.size());
    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      Vector<float> accumulated_lengths_cu;
      for (const int curve_selection_i : range) {
        const int curve_i = curve_selection_[curve_selection_i];
        const IndexRange points = curves_->points_for_curve(curve_i);
        const int first_point_i = points[0];
        const float3 first_pos_cu = positions_cu[first_point_i];
        const float3 first_pos_su = transforms_.curves_to_surface * first_pos_cu;

        /* Find the nearest position on the surface. The curve will be aligned to the normal of
         * that point. */
        BVHTreeNearest nearest;
        nearest.dist_sq = FLT_MAX;
        BLI_bvhtree_find_nearest(surface_bvh_.tree,
                                 first_pos_su,
                                 &nearest,
                                 surface_bvh_.nearest_callback,
                                 &surface_bvh_);

        const MLoopTri &looptri = surface_looptris_[nearest.index];
        const float3 closest_pos_su = nearest.co;
        const float3 &v0_su = surface_positions_[surface_loops_[looptri.tri[0]].v];
        const float3 &v1_su = surface_positions_[surface_loops_[looptri.tri[1]].v];
        const float3 &v2_su = surface_positions_[surface_loops_[looptri.tri[2]].v];
        float3 bary_coords;
        interp_weights_tri_v3(bary_coords, v0_su, v1_su, v2_su, closest_pos_su);
        const float3 normal_su = geometry::compute_surface_point_normal(
            looptri, bary_coords, corner_normals_su_);
        const float3 normal_cu = math::normalize(transforms_.surface_to_curves_normal * normal_su);

        accumulated_lengths_cu.reinitialize(points.size() - 1);
        length_parameterize::accumulate_lengths<float3>(
            positions_cu.slice(points), false, accumulated_lengths_cu);

        /* Align curve to the surface normal while making sure that the curve does not fold up much
         * in the process (e.g. when the curve was pointing in the opposite direction before). */
        for (const int i : IndexRange(points.size()).drop_front(1)) {
          const int point_i = points[i];
          const float3 old_pos_cu = positions_cu[point_i];

          /* Compute final position of the point. */
          const float length_param_cu = accumulated_lengths_cu[i - 1];
          const float3 goal_pos_cu = first_pos_cu + length_param_cu * normal_cu;

          const float weight = 0.01f * brush_strength_ * point_factors_[point_i] *
                               curve_weights[curve_selection_i];
          float3 new_pos_cu = math::interpolate(old_pos_cu, goal_pos_cu, weight);

          /* Make sure the point does not move closer to the root point than it was initially. This
           * makes the curve kind of "rotate up". */
          const float old_dist_to_root_cu = math::distance(old_pos_cu, first_pos_cu);
          const float new_dist_to_root_cu = math::distance(new_pos_cu, first_pos_cu);
          if (new_dist_to_root_cu < old_dist_to_root_cu) {
            const float3 offset = math::normalize(new_pos_cu - first_pos_cu);
            new_pos_cu += (old_dist_to_root_cu - new_dist_to_root_cu) * offset;
          }

          positions_cu[point_i] = new_pos_cu;
        }
      }
    });
  }

  void initialize_segment_lengths()
  {
    const Span<float3> positions_cu = curves_->positions();
    self_->segment_lengths_cu_.reinitialize(curves_->points_num());
    threading::parallel_for(curves_->curves_range(), 128, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int point_i : points.drop_back(1)) {
          const float3 &p1_cu = positions_cu[point_i];
          const float3 &p2_cu = positions_cu[point_i + 1];
          const float length_cu = math::distance(p1_cu, p2_cu);
          self_->segment_lengths_cu_[point_i] = length_cu;
        }
      }
    });
  }

  void restore_segment_lengths()
  {
    const Span<float> expected_lengths_cu = self_->segment_lengths_cu_;
    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int segment_i : points.drop_back(1)) {
          const float3 &p1_cu = positions_cu[segment_i];
          float3 &p2_cu = positions_cu[segment_i + 1];
          const float3 direction = math::normalize(p2_cu - p1_cu);
          const float expected_length_cu = expected_lengths_cu[segment_i];
          p2_cu = p1_cu + direction * expected_length_cu;
        }
      }
    });
  }
};

void PuffOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  PuffOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_puff_operation()
{
  return std::make_unique<PuffOperation>();
}

}  // namespace blender::ed::sculpt_paint
