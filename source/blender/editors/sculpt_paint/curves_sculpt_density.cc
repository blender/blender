/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <numeric>

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BLI_index_mask_ops.hh"
#include "BLI_kdtree.h"
#include "BLI_rand.hh"
#include "BLI_task.hh"

#include "PIL_time.h"

#include "GEO_add_curves_on_mesh.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "WM_api.h"

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

class DensityAddOperation : public CurvesSculptStrokeOperation {
 private:
  /** Used when some data should be interpolated from existing curves. */
  KDTree_3d *original_curve_roots_kdtree_ = nullptr;
  /** Contains curve roots of all curves that existed before the brush started. */
  KDTree_3d *deformed_curve_roots_kdtree_ = nullptr;
  /** Root positions of curves that have been added in the current brush stroke. */
  Vector<float3> new_deformed_root_positions_;
  int original_curve_num_ = 0;

  friend struct DensityAddOperationExecutor;

 public:
  ~DensityAddOperation() override
  {
    if (original_curve_roots_kdtree_ != nullptr) {
      BLI_kdtree_3d_free(original_curve_roots_kdtree_);
    }
    if (deformed_curve_roots_kdtree_ != nullptr) {
      BLI_kdtree_3d_free(deformed_curve_roots_kdtree_);
    }
  }

  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct DensityAddOperationExecutor {
  DensityAddOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *curves_ob_orig_ = nullptr;
  Curves *curves_id_orig_ = nullptr;
  CurvesGeometry *curves_orig_ = nullptr;

  Object *surface_ob_orig_ = nullptr;
  Mesh *surface_orig_ = nullptr;

  Object *surface_ob_eval_ = nullptr;
  Mesh *surface_eval_ = nullptr;
  Span<MLoopTri> surface_looptris_eval_;
  VArraySpan<float2> surface_uv_map_eval_;
  BVHTreeFromMesh surface_bvh_eval_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  const BrushCurvesSculptSettings *brush_settings_ = nullptr;

  float brush_strength_;
  float brush_radius_re_;
  float2 brush_pos_re_;

  CurvesSurfaceTransforms transforms_;

  DensityAddOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(DensityAddOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    curves_ob_orig_ = CTX_data_active_object(&C);
    curves_id_orig_ = static_cast<Curves *>(curves_ob_orig_->data);
    curves_orig_ = &CurvesGeometry::wrap(curves_id_orig_->geometry);

    if (stroke_extension.is_first) {
      self_->original_curve_num_ = curves_orig_->curves_num();
    }

    if (curves_id_orig_->surface == nullptr || curves_id_orig_->surface->type != OB_MESH) {
      report_missing_surface(stroke_extension.reports);
      return;
    }

    surface_ob_orig_ = curves_id_orig_->surface;
    surface_orig_ = static_cast<Mesh *>(surface_ob_orig_->data);
    if (surface_orig_->totpoly == 0) {
      report_empty_original_surface(stroke_extension.reports);
      return;
    }

    surface_ob_eval_ = DEG_get_evaluated_object(ctx_.depsgraph, surface_ob_orig_);
    if (surface_ob_eval_ == nullptr) {
      return;
    }
    surface_eval_ = BKE_object_get_evaluated_mesh(surface_ob_eval_);
    if (surface_eval_->totpoly == 0) {
      report_empty_evaluated_surface(stroke_extension.reports);
      return;
    }

    BKE_bvhtree_from_mesh_get(&surface_bvh_eval_, surface_eval_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_eval_); });
    surface_looptris_eval_ = surface_eval_->looptris();
    /* Find UV map. */
    VArraySpan<float2> surface_uv_map;
    if (curves_id_orig_->surface_uv_map != nullptr) {
      surface_uv_map = surface_orig_->attributes().lookup<float2>(curves_id_orig_->surface_uv_map,
                                                                  ATTR_DOMAIN_CORNER);
      surface_uv_map_eval_ = surface_eval_->attributes().lookup<float2>(
          curves_id_orig_->surface_uv_map, ATTR_DOMAIN_CORNER);
    }
    if (surface_uv_map.is_empty()) {
      report_missing_uv_map_on_original_surface(stroke_extension.reports);
      return;
    }
    if (surface_uv_map_eval_.is_empty()) {
      report_missing_uv_map_on_evaluated_surface(stroke_extension.reports);
      return;
    }

    transforms_ = CurvesSurfaceTransforms(*curves_ob_orig_, curves_id_orig_->surface);

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_settings_ = brush_->curves_sculpt_settings;
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);
    brush_radius_re_ = brush_radius_get(*ctx_.scene, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    Vector<float3> new_positions_cu;
    Vector<float2> new_uvs;
    const double time = PIL_check_seconds_timer() * 1000000.0;
    RandomNumberGenerator rng{*(uint32_t *)(&time)};

    /* Find potential new curve root points. */
    if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      this->sample_projected_with_symmetry(rng, new_uvs, new_positions_cu);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->sample_spherical_with_symmetry(rng, new_uvs, new_positions_cu);
    }
    else {
      BLI_assert_unreachable();
    }
    for (float3 &pos : new_positions_cu) {
      pos = transforms_.surface_to_curves * pos;
    }

    if (stroke_extension.is_first) {
      this->prepare_curve_roots_kdtrees();
    }

    const int already_added_curves = self_->new_deformed_root_positions_.size();
    KDTree_3d *new_roots_kdtree = BLI_kdtree_3d_new(already_added_curves +
                                                    new_positions_cu.size());
    BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(new_roots_kdtree); });

    /* Used to tag all curves that are too close to existing curves or too close to other new
     * curves. */
    Array<bool> new_curve_skipped(new_positions_cu.size(), false);
    threading::parallel_invoke(
        512 < already_added_curves + new_positions_cu.size(),
        /* Build kdtree from root points created by the current stroke. */
        [&]() {
          for (const int i : IndexRange(already_added_curves)) {
            BLI_kdtree_3d_insert(new_roots_kdtree, -1, self_->new_deformed_root_positions_[i]);
          }
          for (const int new_i : new_positions_cu.index_range()) {
            const float3 &root_pos_cu = new_positions_cu[new_i];
            BLI_kdtree_3d_insert(new_roots_kdtree, new_i, root_pos_cu);
          }
          BLI_kdtree_3d_balance(new_roots_kdtree);
        },
        /* Check which new root points are close to roots that existed before the current stroke
         * started. */
        [&]() {
          threading::parallel_for(
              new_positions_cu.index_range(), 128, [&](const IndexRange range) {
                for (const int new_i : range) {
                  const float3 &new_root_pos_cu = new_positions_cu[new_i];
                  KDTreeNearest_3d nearest;
                  nearest.dist = FLT_MAX;
                  BLI_kdtree_3d_find_nearest(
                      self_->deformed_curve_roots_kdtree_, new_root_pos_cu, &nearest);
                  if (nearest.dist < brush_settings_->minimum_distance) {
                    new_curve_skipped[new_i] = true;
                  }
                }
              });
        });

    /* Find new points that are too close too other new points. */
    for (const int new_i : new_positions_cu.index_range()) {
      if (new_curve_skipped[new_i]) {
        continue;
      }
      const float3 &root_pos_cu = new_positions_cu[new_i];
      BLI_kdtree_3d_range_search_cb_cpp(
          new_roots_kdtree,
          root_pos_cu,
          brush_settings_->minimum_distance,
          [&](const int other_new_i, const float * /*co*/, float /*dist_sq*/) {
            if (other_new_i == -1) {
              new_curve_skipped[new_i] = true;
              return false;
            }
            if (new_i == other_new_i) {
              return true;
            }
            new_curve_skipped[other_new_i] = true;
            return true;
          });
    }

    /* Remove points that are too close to others. */
    for (int64_t i = new_positions_cu.size() - 1; i >= 0; i--) {
      if (new_curve_skipped[i]) {
        new_positions_cu.remove_and_reorder(i);
        new_uvs.remove_and_reorder(i);
      }
    }
    self_->new_deformed_root_positions_.extend(new_positions_cu);

    /* Find normals. */
    if (!CustomData_has_layer(&surface_orig_->ldata, CD_NORMAL)) {
      BKE_mesh_calc_normals_split(surface_orig_);
    }
    const Span<float3> corner_normals_su = {
        reinterpret_cast<const float3 *>(CustomData_get_layer(&surface_orig_->ldata, CD_NORMAL)),
        surface_orig_->totloop};

    const Span<MLoopTri> surface_looptris_orig = surface_orig_->looptris();
    const geometry::ReverseUVSampler reverse_uv_sampler{surface_uv_map, surface_looptris_orig};

    geometry::AddCurvesOnMeshInputs add_inputs;
    add_inputs.uvs = new_uvs;
    add_inputs.interpolate_length = brush_settings_->flag &
                                    BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_LENGTH;
    add_inputs.interpolate_shape = brush_settings_->flag &
                                   BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_SHAPE;
    add_inputs.interpolate_point_count = brush_settings_->flag &
                                         BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_POINT_COUNT;
    add_inputs.fallback_curve_length = brush_settings_->curve_length;
    add_inputs.fallback_point_count = std::max(2, brush_settings_->points_per_curve);
    add_inputs.transforms = &transforms_;
    add_inputs.surface = surface_orig_;
    add_inputs.corner_normals_su = corner_normals_su;
    add_inputs.surface_looptris = surface_looptris_orig;
    add_inputs.reverse_uv_sampler = &reverse_uv_sampler;
    add_inputs.old_roots_kdtree = self_->original_curve_roots_kdtree_;

    const geometry::AddCurvesOnMeshOutputs add_outputs = geometry::add_curves_on_mesh(
        *curves_orig_, add_inputs);
    bke::MutableAttributeAccessor attributes = curves_orig_->attributes_for_write();
    if (bke::GSpanAttributeWriter selection = attributes.lookup_for_write_span(".selection")) {
      curves::fill_selection_true(selection.span.slice(selection.domain == ATTR_DOMAIN_POINT ?
                                                           add_outputs.new_points_range :
                                                           add_outputs.new_curves_range));
      selection.finish();
    }

    if (add_outputs.uv_error) {
      report_invalid_uv_map(stroke_extension.reports);
    }

    DEG_id_tag_update(&curves_id_orig_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_orig_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void prepare_curve_roots_kdtrees()
  {
    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *curves_ob_orig_);
    const Span<int> curve_offsets = curves_orig_->offsets();
    const Span<float3> original_positions = curves_orig_->positions();
    const Span<float3> deformed_positions = deformation.positions;
    BLI_assert(original_positions.size() == deformed_positions.size());

    auto roots_kdtree_from_positions = [&](const Span<float3> positions) {
      KDTree_3d *kdtree = BLI_kdtree_3d_new(curves_orig_->curves_num());
      for (const int curve_i : curves_orig_->curves_range()) {
        const int root_point_i = curve_offsets[curve_i];
        BLI_kdtree_3d_insert(kdtree, curve_i, positions[root_point_i]);
      }
      BLI_kdtree_3d_balance(kdtree);
      return kdtree;
    };

    threading::parallel_invoke(
        1024 < original_positions.size() + deformed_positions.size(),
        [&]() {
          self_->original_curve_roots_kdtree_ = roots_kdtree_from_positions(original_positions);
        },
        [&]() {
          self_->deformed_curve_roots_kdtree_ = roots_kdtree_from_positions(deformed_positions);
        });
  }

  void sample_projected_with_symmetry(RandomNumberGenerator &rng,
                                      Vector<float2> &r_uvs,
                                      Vector<float3> &r_positions_su)
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, curves_ob_orig_, projection.values);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_orig_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      const float4x4 brush_transform_inv = brush_transform.inverted();
      const float4x4 transform = transforms_.curves_to_surface * brush_transform *
                                 transforms_.world_to_curves;
      Vector<float3> positions_su;
      Vector<float3> bary_coords;
      Vector<int> looptri_indices;
      const int new_points = bke::mesh_surface_sample::sample_surface_points_projected(
          rng,
          *surface_eval_,
          surface_bvh_eval_,
          brush_pos_re_,
          brush_radius_re_,
          [&](const float2 &pos_re, float3 &r_start_su, float3 &r_end_su) {
            float3 start_wo, end_wo;
            ED_view3d_win_to_segment_clipped(
                ctx_.depsgraph, ctx_.region, ctx_.v3d, pos_re, start_wo, end_wo, true);
            r_start_su = transform * start_wo;
            r_end_su = transform * end_wo;
          },
          true,
          brush_settings_->density_add_attempts,
          brush_settings_->density_add_attempts,
          bary_coords,
          looptri_indices,
          positions_su);

      /* Remove some sampled points randomly based on the brush falloff and strength. */
      for (int i = new_points - 1; i >= 0; i--) {
        const float3 pos_su = positions_su[i];
        const float3 pos_cu = brush_transform_inv * transforms_.surface_to_curves * pos_su;
        float2 pos_re;
        ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
        const float dist_to_brush_re = math::distance(brush_pos_re_, pos_re);
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, dist_to_brush_re, brush_radius_re_);
        const float weight = brush_strength_ * radius_falloff;
        if (rng.get_float() > weight) {
          bary_coords.remove_and_reorder(i);
          looptri_indices.remove_and_reorder(i);
          positions_su.remove_and_reorder(i);
        }
      }

      for (const int i : bary_coords.index_range()) {
        const float2 uv = bke::mesh_surface_sample::sample_corner_attrribute_with_bary_coords(
            bary_coords[i], surface_looptris_eval_[looptri_indices[i]], surface_uv_map_eval_);
        r_uvs.append(uv);
      }
      r_positions_su.extend(positions_su);
    }
  }

  void sample_spherical_with_symmetry(RandomNumberGenerator &rng,
                                      Vector<float2> &r_uvs,
                                      Vector<float3> &r_positions_su)
  {
    const std::optional<CurvesBrush3D> brush_3d = sample_curves_surface_3d_brush(*ctx_.depsgraph,
                                                                                 *ctx_.region,
                                                                                 *ctx_.v3d,
                                                                                 transforms_,
                                                                                 surface_bvh_eval_,
                                                                                 brush_pos_re_,
                                                                                 brush_radius_re_);
    if (!brush_3d.has_value()) {
      return;
    }

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_orig_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      const float3 brush_pos_cu = brush_transform * brush_3d->position_cu;
      const float3 brush_pos_su = transforms_.curves_to_surface * brush_pos_cu;
      const float brush_radius_su = transform_brush_radius(
          transforms_.curves_to_surface, brush_pos_cu, brush_3d->radius_cu);
      const float brush_radius_sq_su = pow2f(brush_radius_su);

      Vector<int> selected_looptri_indices;
      BLI_bvhtree_range_query_cpp(
          *surface_bvh_eval_.tree,
          brush_pos_su,
          brush_radius_su,
          [&](const int index, const float3 & /*co*/, const float /*dist_sq*/) {
            selected_looptri_indices.append(index);
          });

      const float brush_plane_area_su = M_PI * brush_radius_sq_su;
      const float approximate_density_su = brush_settings_->density_add_attempts /
                                           brush_plane_area_su;

      Vector<float3> positions_su;
      Vector<float3> bary_coords;
      Vector<int> looptri_indices;
      const int new_points = bke::mesh_surface_sample::sample_surface_points_spherical(
          rng,
          *surface_eval_,
          selected_looptri_indices,
          brush_pos_su,
          brush_radius_su,
          approximate_density_su,
          bary_coords,
          looptri_indices,
          positions_su);

      /* Remove some sampled points randomly based on the brush falloff and strength. */
      for (int i = new_points - 1; i >= 0; i--) {
        const float3 pos_su = positions_su[i];
        const float3 pos_cu = transforms_.surface_to_curves * pos_su;
        const float dist_to_brush_cu = math::distance(pos_cu, brush_pos_cu);
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, dist_to_brush_cu, brush_3d->radius_cu);
        const float weight = brush_strength_ * radius_falloff;
        if (rng.get_float() > weight) {
          bary_coords.remove_and_reorder(i);
          looptri_indices.remove_and_reorder(i);
          positions_su.remove_and_reorder(i);
        }
      }

      for (const int i : bary_coords.index_range()) {
        const float2 uv = bke::mesh_surface_sample::sample_corner_attrribute_with_bary_coords(
            bary_coords[i], surface_looptris_eval_[looptri_indices[i]], surface_uv_map_eval_);
        r_uvs.append(uv);
      }
      r_positions_su.extend(positions_su);
    }
  }
};

void DensityAddOperation::on_stroke_extended(const bContext &C,
                                             const StrokeExtension &stroke_extension)
{
  DensityAddOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

class DensitySubtractOperation : public CurvesSculptStrokeOperation {
 private:
  friend struct DensitySubtractOperationExecutor;

  /**
   * Deformed root positions of curves that still exist. This has to be stored in case the brush is
   * executed more than once before the curves are evaluated again. This can happen when the mouse
   * is moved quickly and the brush spacing is small.
   */
  Vector<float3> deformed_root_positions_;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct DensitySubtractOperationExecutor {
  DensitySubtractOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  Object *surface_ob_orig_ = nullptr;
  Mesh *surface_orig_ = nullptr;

  Object *surface_ob_eval_ = nullptr;
  Mesh *surface_eval_ = nullptr;
  BVHTreeFromMesh surface_bvh_eval_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;
  float2 brush_pos_re_;

  float minimum_distance_;

  CurvesSurfaceTransforms transforms_;

  KDTree_3d *root_points_kdtree_;

  DensitySubtractOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(DensitySubtractOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    self_ = &self;

    object_ = CTX_data_active_object(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }

    surface_ob_orig_ = curves_id_->surface;
    if (surface_ob_orig_ == nullptr) {
      return;
    }
    surface_orig_ = static_cast<Mesh *>(surface_ob_orig_->data);

    surface_ob_eval_ = DEG_get_evaluated_object(ctx_.depsgraph, surface_ob_orig_);
    if (surface_ob_eval_ == nullptr) {
      return;
    }
    surface_eval_ = BKE_object_get_evaluated_mesh(surface_ob_eval_);

    BKE_bvhtree_from_mesh_get(&surface_bvh_eval_, surface_eval_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_eval_); });

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    minimum_distance_ = brush_->curves_sculpt_settings->minimum_distance;

    curve_selection_ = curves::retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    transforms_ = CurvesSurfaceTransforms(*object_, curves_id_->surface);
    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    if (stroke_extension.is_first) {
      const bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);
      for (const int curve_i : curves_->curves_range()) {
        const int first_point_i = curves_->offsets()[curve_i];
        self_->deformed_root_positions_.append(deformation.positions[first_point_i]);
      }
    }

    root_points_kdtree_ = BLI_kdtree_3d_new(curve_selection_.size());
    BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(root_points_kdtree_); });
    for (const int curve_i : curve_selection_) {
      const float3 &pos_cu = self_->deformed_root_positions_[curve_i];
      BLI_kdtree_3d_insert(root_points_kdtree_, curve_i, pos_cu);
    }
    BLI_kdtree_3d_balance(root_points_kdtree_);

    /* Find all curves that should be deleted. */
    Array<bool> curves_to_delete(curves_->curves_num(), false);
    if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      this->reduce_density_projected_with_symmetry(curves_to_delete);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->reduce_density_spherical_with_symmetry(curves_to_delete);
    }
    else {
      BLI_assert_unreachable();
    }

    Vector<int64_t> indices;
    const IndexMask mask_to_delete = index_mask_ops::find_indices_based_on_predicate(
        curves_->curves_range(), 4096, indices, [&](const int curve_i) {
          return curves_to_delete[curve_i];
        });

    /* Remove deleted curves from the stored deformed root positions. */
    const Vector<IndexRange> ranges_to_keep = mask_to_delete.extract_ranges_invert(
        curves_->curves_range());
    BLI_assert(curves_->curves_num() == self_->deformed_root_positions_.size());
    Vector<float3> new_deformed_positions;
    for (const IndexRange range : ranges_to_keep) {
      new_deformed_positions.extend(self_->deformed_root_positions_.as_span().slice(range));
    }
    self_->deformed_root_positions_ = std::move(new_deformed_positions);

    curves_->remove_curves(mask_to_delete);
    BLI_assert(curves_->curves_num() == self_->deformed_root_positions_.size());

    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void reduce_density_projected_with_symmetry(MutableSpan<bool> curves_to_delete)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->reduce_density_projected(brush_transform, curves_to_delete);
    }
  }

  void reduce_density_projected(const float4x4 &brush_transform,
                                MutableSpan<bool> curves_to_delete)
  {
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    /* Randomly select the curves that are allowed to be removed, based on the brush radius and
     * strength. */
    Array<bool> allow_remove_curve(curves_->curves_num(), false);
    threading::parallel_for(curves_->curves_range(), 512, [&](const IndexRange range) {
      RandomNumberGenerator rng(int(PIL_check_seconds_timer() * 1000000.0));

      for (const int curve_i : range) {
        if (curves_to_delete[curve_i]) {
          allow_remove_curve[curve_i] = true;
          continue;
        }
        const float3 pos_cu = brush_transform * self_->deformed_root_positions_[curve_i];

        float2 pos_re;
        ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
        const float dist_to_brush_sq_re = math::distance_squared(brush_pos_re_, pos_re);
        if (dist_to_brush_sq_re > brush_radius_sq_re) {
          continue;
        }
        const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, dist_to_brush_re, brush_radius_re);
        const float weight = brush_strength_ * radius_falloff;
        if (rng.get_float() < weight) {
          allow_remove_curve[curve_i] = true;
        }
      }
    });

    /* Detect curves that are too close to other existing curves. */
    for (const int curve_i : curve_selection_) {
      if (curves_to_delete[curve_i]) {
        continue;
      }
      if (!allow_remove_curve[curve_i]) {
        continue;
      }
      const float3 orig_pos_cu = self_->deformed_root_positions_[curve_i];
      const float3 pos_cu = brush_transform * orig_pos_cu;
      float2 pos_re;
      ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
      const float dist_to_brush_sq_re = math::distance_squared(brush_pos_re_, pos_re);
      if (dist_to_brush_sq_re > brush_radius_sq_re) {
        continue;
      }
      BLI_kdtree_3d_range_search_cb_cpp(
          root_points_kdtree_,
          orig_pos_cu,
          minimum_distance_,
          [&](const int other_curve_i, const float * /*co*/, float /*dist_sq*/) {
            if (other_curve_i == curve_i) {
              return true;
            }
            if (allow_remove_curve[other_curve_i]) {
              curves_to_delete[other_curve_i] = true;
            }
            return true;
          });
    }
  }

  void reduce_density_spherical_with_symmetry(MutableSpan<bool> curves_to_delete)
  {
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const std::optional<CurvesBrush3D> brush_3d = sample_curves_surface_3d_brush(*ctx_.depsgraph,
                                                                                 *ctx_.region,
                                                                                 *ctx_.v3d,
                                                                                 transforms_,
                                                                                 surface_bvh_eval_,
                                                                                 brush_pos_re_,
                                                                                 brush_radius_re);
    if (!brush_3d.has_value()) {
      return;
    }

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      const float3 brush_pos_cu = brush_transform * brush_3d->position_cu;
      this->reduce_density_spherical(brush_pos_cu, brush_3d->radius_cu, curves_to_delete);
    }
  }

  void reduce_density_spherical(const float3 &brush_pos_cu,
                                const float brush_radius_cu,
                                MutableSpan<bool> curves_to_delete)
  {
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    /* Randomly select the curves that are allowed to be removed, based on the brush radius and
     * strength. */
    Array<bool> allow_remove_curve(curves_->curves_num(), false);
    threading::parallel_for(curves_->curves_range(), 512, [&](const IndexRange range) {
      RandomNumberGenerator rng(int(PIL_check_seconds_timer() * 1000000.0));

      for (const int curve_i : range) {
        if (curves_to_delete[curve_i]) {
          allow_remove_curve[curve_i] = true;
          continue;
        }
        const float3 pos_cu = self_->deformed_root_positions_[curve_i];

        const float dist_to_brush_sq_cu = math::distance_squared(brush_pos_cu, pos_cu);
        if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
          continue;
        }
        const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_cu);
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, dist_to_brush_cu, brush_radius_cu);
        const float weight = brush_strength_ * radius_falloff;
        if (rng.get_float() < weight) {
          allow_remove_curve[curve_i] = true;
        }
      }
    });

    /* Detect curves that are too close to other existing curves. */
    for (const int curve_i : curve_selection_) {
      if (curves_to_delete[curve_i]) {
        continue;
      }
      if (!allow_remove_curve[curve_i]) {
        continue;
      }
      const float3 &pos_cu = self_->deformed_root_positions_[curve_i];
      const float dist_to_brush_sq_cu = math::distance_squared(pos_cu, brush_pos_cu);
      if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
        continue;
      }

      BLI_kdtree_3d_range_search_cb_cpp(
          root_points_kdtree_,
          pos_cu,
          minimum_distance_,
          [&](const int other_curve_i, const float * /*co*/, float /*dist_sq*/) {
            if (other_curve_i == curve_i) {
              return true;
            }
            if (allow_remove_curve[other_curve_i]) {
              curves_to_delete[other_curve_i] = true;
            }
            return true;
          });
    }
  }
};

void DensitySubtractOperation::on_stroke_extended(const bContext &C,
                                                  const StrokeExtension &stroke_extension)
{
  DensitySubtractOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

/**
 * Detects whether the brush should be in Add or Subtract mode.
 */
static bool use_add_density_mode(const BrushStrokeMode brush_mode,
                                 const bContext &C,
                                 const StrokeExtension &stroke_start)
{
  const Scene &scene = *CTX_data_scene(&C);
  const Brush &brush = *BKE_paint_brush_for_read(&scene.toolsettings->curves_sculpt->paint);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_on_load(&C);
  const ARegion &region = *CTX_wm_region(&C);
  const View3D &v3d = *CTX_wm_view3d(&C);

  const eBrushCurvesSculptDensityMode density_mode = static_cast<eBrushCurvesSculptDensityMode>(
      brush.curves_sculpt_settings->density_mode);
  const bool use_invert = brush_mode == BRUSH_STROKE_INVERT;

  if (density_mode == BRUSH_CURVES_SCULPT_DENSITY_MODE_ADD) {
    return !use_invert;
  }
  if (density_mode == BRUSH_CURVES_SCULPT_DENSITY_MODE_REMOVE) {
    return use_invert;
  }

  const Object &curves_ob_orig = *CTX_data_active_object(&C);
  const Curves &curves_id_orig = *static_cast<Curves *>(curves_ob_orig.data);
  Object *surface_ob_orig = curves_id_orig.surface;
  if (surface_ob_orig == nullptr) {
    return true;
  }
  Object *surface_ob_eval = DEG_get_evaluated_object(&depsgraph, surface_ob_orig);
  if (surface_ob_eval == nullptr) {
    return true;
  }
  const CurvesGeometry &curves = CurvesGeometry::wrap(curves_id_orig.geometry);
  if (curves.curves_num() <= 1) {
    return true;
  }
  const Mesh *surface_mesh_eval = BKE_object_get_evaluated_mesh(surface_ob_eval);
  if (surface_mesh_eval == nullptr) {
    return true;
  }

  const CurvesSurfaceTransforms transforms(curves_ob_orig, curves_id_orig.surface);
  BVHTreeFromMesh surface_bvh_eval;
  BKE_bvhtree_from_mesh_get(&surface_bvh_eval, surface_mesh_eval, BVHTREE_FROM_LOOPTRI, 2);
  BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_eval); });

  const float2 brush_pos_re = stroke_start.mouse_position;
  /* Reduce radius so that only an inner circle is used to determine the existing density. */
  const float brush_radius_re = BKE_brush_size_get(&scene, &brush) * 0.5f;

  /* Find the surface point under the brush. */
  const std::optional<CurvesBrush3D> brush_3d = sample_curves_surface_3d_brush(
      depsgraph, region, v3d, transforms, surface_bvh_eval, brush_pos_re, brush_radius_re);
  if (!brush_3d.has_value()) {
    return true;
  }

  const float3 brush_pos_cu = brush_3d->position_cu;
  const float brush_radius_cu = brush_3d->radius_cu;
  const float brush_radius_sq_cu = pow2f(brush_radius_cu);

  const bke::crazyspace::GeometryDeformation deformation =
      bke::crazyspace::get_evaluated_curves_deformation(depsgraph, curves_ob_orig);
  const Span<int> offsets = curves.offsets();

  /* Compute distance from brush to curve roots. */
  Array<std::pair<float, int>> distances_sq_to_brush(curves.curves_num());
  threading::EnumerableThreadSpecific<int> valid_curve_count_by_thread;
  threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange range) {
    int &valid_curve_count = valid_curve_count_by_thread.local();
    for (const int curve_i : range) {
      const int root_point_i = offsets[curve_i];
      const float3 &root_pos_cu = deformation.positions[root_point_i];
      const float dist_sq_cu = math::distance_squared(root_pos_cu, brush_pos_cu);
      if (dist_sq_cu < brush_radius_sq_cu) {
        distances_sq_to_brush[curve_i] = {math::distance_squared(root_pos_cu, brush_pos_cu),
                                          curve_i};
        valid_curve_count++;
      }
      else {
        distances_sq_to_brush[curve_i] = {FLT_MAX, -1};
      }
    }
  });
  const int valid_curve_count = std::accumulate(
      valid_curve_count_by_thread.begin(), valid_curve_count_by_thread.end(), 0);

  /* Find a couple of curves that are closest to the brush center. */
  const int check_curve_count = std::min<int>(8, valid_curve_count);
  std::partial_sort(distances_sq_to_brush.begin(),
                    distances_sq_to_brush.begin() + check_curve_count,
                    distances_sq_to_brush.end());

  /* Compute the minimum pair-wise distance between the curve roots that are close to the brush
   * center. */
  float min_dist_sq_cu = FLT_MAX;
  for (const int i : IndexRange(check_curve_count)) {
    const float3 &pos_i = deformation.positions[offsets[distances_sq_to_brush[i].second]];
    for (int j = i + 1; j < check_curve_count; j++) {
      const float3 &pos_j = deformation.positions[offsets[distances_sq_to_brush[j].second]];
      const float dist_sq_cu = math::distance_squared(pos_i, pos_j);
      math::min_inplace(min_dist_sq_cu, dist_sq_cu);
    }
  }

  const float min_dist_cu = std::sqrt(min_dist_sq_cu);
  if (min_dist_cu > brush.curves_sculpt_settings->minimum_distance) {
    return true;
  }

  return false;
}

std::unique_ptr<CurvesSculptStrokeOperation> new_density_operation(
    const BrushStrokeMode brush_mode, const bContext &C, const StrokeExtension &stroke_start)
{
  if (use_add_density_mode(brush_mode, C, stroke_start)) {
    return std::make_unique<DensityAddOperation>();
  }
  return std::make_unique<DensitySubtractOperation>();
}

}  // namespace blender::ed::sculpt_paint
