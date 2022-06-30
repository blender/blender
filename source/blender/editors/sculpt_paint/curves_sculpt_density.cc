/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <numeric>

#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

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
  KDTree_3d *curve_roots_kdtree_ = nullptr;
  int original_curve_num_ = 0;

  friend struct DensityAddOperationExecutor;

 public:
  ~DensityAddOperation() override
  {
    if (curve_roots_kdtree_ != nullptr) {
      BLI_kdtree_3d_free(curve_roots_kdtree_);
    }
  }

  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct DensityAddOperationExecutor {
  DensityAddOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  Object *surface_ob_ = nullptr;
  Mesh *surface_ = nullptr;
  Span<MLoopTri> surface_looptris_;
  Span<float3> corner_normals_su_;
  VArray_Span<float2> surface_uv_map_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  const BrushCurvesSculptSettings *brush_settings_ = nullptr;

  float brush_strength_;
  float brush_radius_re_;
  float2 brush_pos_re_;

  CurvesSculptTransforms transforms_;

  BVHTreeFromMesh surface_bvh_;

  DensityAddOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(DensityAddOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    object_ = CTX_data_active_object(&C);
    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);

    if (stroke_extension.is_first) {
      self_->original_curve_num_ = curves_->curves_num();
    }

    if (curves_id_->surface == nullptr || curves_id_->surface->type != OB_MESH) {
      return;
    }

    surface_ob_ = curves_id_->surface;
    surface_ = static_cast<Mesh *>(surface_ob_->data);

    surface_looptris_ = {BKE_mesh_runtime_looptri_ensure(surface_),
                         BKE_mesh_runtime_looptri_len(surface_)};

    transforms_ = CurvesSculptTransforms(*object_, curves_id_->surface);

    if (!CustomData_has_layer(&surface_->ldata, CD_NORMAL)) {
      BKE_mesh_calc_normals_split(surface_);
    }
    corner_normals_su_ = {
        reinterpret_cast<const float3 *>(CustomData_get_layer(&surface_->ldata, CD_NORMAL)),
        surface_->totloop};

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_settings_ = brush_->curves_sculpt_settings;
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);
    brush_radius_re_ = brush_radius_get(*ctx_.scene, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    BKE_bvhtree_from_mesh_get(&surface_bvh_, surface_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_); });

    Vector<float3> new_bary_coords;
    Vector<int> new_looptri_indices;
    Vector<float3> new_positions_cu;
    const double time = PIL_check_seconds_timer() * 1000000.0;
    RandomNumberGenerator rng{*(uint32_t *)(&time)};

    /* Find potential new curve root points. */
    if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      this->sample_projected_with_symmetry(
          rng, new_bary_coords, new_looptri_indices, new_positions_cu);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->sample_spherical_with_symmetry(
          rng, new_bary_coords, new_looptri_indices, new_positions_cu);
    }
    else {
      BLI_assert_unreachable();
    }
    for (float3 &pos : new_positions_cu) {
      pos = transforms_.surface_to_curves * pos;
    }

    this->ensure_curve_roots_kdtree();

    const int already_added_curves = curves_->curves_num() - self_->original_curve_num_;
    KDTree_3d *new_roots_kdtree = BLI_kdtree_3d_new(already_added_curves +
                                                    new_positions_cu.size());
    BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(new_roots_kdtree); });

    /* Used to tag all curves that are too close to existing curves or too close to other new
     * curves. */
    Array<bool> new_curve_skipped(new_positions_cu.size(), false);
    threading::parallel_invoke(
        /* Build kdtree from root points created by the current stroke. */
        [&]() {
          const Span<float3> positions_cu = curves_->positions();
          for (const int curve_i : curves_->curves_range().take_back(already_added_curves)) {
            const float3 &root_pos_cu = positions_cu[curves_->offsets()[curve_i]];
            BLI_kdtree_3d_insert(new_roots_kdtree, curve_i, root_pos_cu);
          }
          for (const int new_i : new_positions_cu.index_range()) {
            const int index_in_kdtree = curves_->curves_num() + new_i;
            const float3 &root_pos_cu = new_positions_cu[new_i];
            BLI_kdtree_3d_insert(new_roots_kdtree, index_in_kdtree, root_pos_cu);
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
                      self_->curve_roots_kdtree_, new_root_pos_cu, &nearest);
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
          [&](const int other_i, const float *UNUSED(co), float UNUSED(dist_sq)) {
            if (other_i < curves_->curves_num()) {
              new_curve_skipped[new_i] = true;
              return false;
            }
            const int other_new_i = other_i - curves_->curves_num();
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
        new_bary_coords.remove_and_reorder(i);
        new_looptri_indices.remove_and_reorder(i);
      }
    }

    /* Find UV map. */
    VArray_Span<float2> surface_uv_map;
    if (curves_id_->surface_uv_map != nullptr) {
      MeshComponent surface_component;
      surface_component.replace(surface_, GeometryOwnershipType::ReadOnly);
      surface_uv_map = surface_component
                           .attribute_try_get_for_read(curves_id_->surface_uv_map,
                                                       ATTR_DOMAIN_CORNER)
                           .typed<float2>();
    }

    /* Find normals. */
    if (!CustomData_has_layer(&surface_->ldata, CD_NORMAL)) {
      BKE_mesh_calc_normals_split(surface_);
    }
    const Span<float3> corner_normals_su = {
        reinterpret_cast<const float3 *>(CustomData_get_layer(&surface_->ldata, CD_NORMAL)),
        surface_->totloop};

    geometry::AddCurvesOnMeshInputs add_inputs;
    add_inputs.root_positions_cu = new_positions_cu;
    add_inputs.bary_coords = new_bary_coords;
    add_inputs.looptri_indices = new_looptri_indices;
    add_inputs.interpolate_length = brush_settings_->flag &
                                    BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_LENGTH;
    add_inputs.interpolate_shape = brush_settings_->flag &
                                   BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_SHAPE;
    add_inputs.interpolate_point_count = brush_settings_->flag &
                                         BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_POINT_COUNT;
    add_inputs.fallback_curve_length = brush_settings_->curve_length;
    add_inputs.fallback_point_count = std::max(2, brush_settings_->points_per_curve);
    add_inputs.surface = surface_;
    add_inputs.surface_bvh = &surface_bvh_;
    add_inputs.surface_looptris = surface_looptris_;
    add_inputs.surface_uv_map = surface_uv_map;
    add_inputs.corner_normals_su = corner_normals_su;
    add_inputs.curves_to_surface_mat = transforms_.curves_to_surface;
    add_inputs.surface_to_curves_normal_mat = transforms_.surface_to_curves_normal;
    add_inputs.old_roots_kdtree = self_->curve_roots_kdtree_;

    geometry::add_curves_on_mesh(*curves_, add_inputs);

    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void ensure_curve_roots_kdtree()
  {
    if (self_->curve_roots_kdtree_ == nullptr) {
      self_->curve_roots_kdtree_ = BLI_kdtree_3d_new(curves_->curves_num());
      for (const int curve_i : curves_->curves_range()) {
        const int root_point_i = curves_->offsets()[curve_i];
        const float3 &root_pos_cu = curves_->positions()[root_point_i];
        BLI_kdtree_3d_insert(self_->curve_roots_kdtree_, curve_i, root_pos_cu);
      }
      BLI_kdtree_3d_balance(self_->curve_roots_kdtree_);
    }
  }

  void sample_projected_with_symmetry(RandomNumberGenerator &rng,
                                      Vector<float3> &r_bary_coords,
                                      Vector<int> &r_looptri_indices,
                                      Vector<float3> &r_positions_su)
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      const float4x4 brush_transform_inv = brush_transform.inverted();
      const float4x4 transform = transforms_.curves_to_surface * brush_transform *
                                 transforms_.world_to_curves;
      const int new_points = bke::mesh_surface_sample::sample_surface_points_projected(
          rng,
          *surface_,
          surface_bvh_,
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
          r_bary_coords,
          r_looptri_indices,
          r_positions_su);

      /* Remove some sampled points randomly based on the brush falloff and strength. */
      const int old_points = r_bary_coords.size() - new_points;
      for (int i = r_bary_coords.size() - 1; i >= old_points; i--) {
        const float3 pos_su = r_positions_su[i];
        const float3 pos_cu = brush_transform_inv * transforms_.surface_to_curves * pos_su;
        float2 pos_re;
        ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);
        const float dist_to_brush_re = math::distance(brush_pos_re_, pos_re);
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, dist_to_brush_re, brush_radius_re_);
        const float weight = brush_strength_ * radius_falloff;
        if (rng.get_float() > weight) {
          r_bary_coords.remove_and_reorder(i);
          r_looptri_indices.remove_and_reorder(i);
          r_positions_su.remove_and_reorder(i);
        }
      }
    }
  }

  void sample_spherical_with_symmetry(RandomNumberGenerator &rng,
                                      Vector<float3> &r_bary_coords,
                                      Vector<int> &r_looptri_indices,
                                      Vector<float3> &r_positions_su)
  {
    const std::optional<CurvesBrush3D> brush_3d = sample_curves_surface_3d_brush(*ctx_.depsgraph,
                                                                                 *ctx_.region,
                                                                                 *ctx_.v3d,
                                                                                 transforms_,
                                                                                 surface_bvh_,
                                                                                 brush_pos_re_,
                                                                                 brush_radius_re_);
    if (!brush_3d.has_value()) {
      return;
    }

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      const float3 brush_pos_cu = brush_transform * brush_3d->position_cu;
      const float3 brush_pos_su = transforms_.curves_to_surface * brush_pos_cu;
      const float brush_radius_su = transform_brush_radius(
          transforms_.curves_to_surface, brush_pos_cu, brush_3d->radius_cu);
      const float brush_radius_sq_su = pow2f(brush_radius_su);

      Vector<int> looptri_indices;
      BLI_bvhtree_range_query_cpp(
          *surface_bvh_.tree,
          brush_pos_su,
          brush_radius_su,
          [&](const int index, const float3 &UNUSED(co), const float UNUSED(dist_sq)) {
            looptri_indices.append(index);
          });

      const float brush_plane_area_su = M_PI * brush_radius_sq_su;
      const float approximate_density_su = brush_settings_->density_add_attempts /
                                           brush_plane_area_su;

      const int new_points = bke::mesh_surface_sample::sample_surface_points_spherical(
          rng,
          *surface_,
          looptri_indices,
          brush_pos_su,
          brush_radius_su,
          approximate_density_su,
          r_bary_coords,
          r_looptri_indices,
          r_positions_su);

      /* Remove some sampled points randomly based on the brush falloff and strength. */
      const int old_points = r_bary_coords.size() - new_points;
      for (int i = r_bary_coords.size() - 1; i >= old_points; i--) {
        const float3 pos_su = r_positions_su[i];
        const float3 pos_cu = transforms_.surface_to_curves * pos_su;
        const float dist_to_brush_cu = math::distance(pos_cu, brush_pos_cu);
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, dist_to_brush_cu, brush_3d->radius_cu);
        const float weight = brush_strength_ * radius_falloff;
        if (rng.get_float() > weight) {
          r_bary_coords.remove_and_reorder(i);
          r_looptri_indices.remove_and_reorder(i);
          r_positions_su.remove_and_reorder(i);
        }
      }
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
  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  friend struct DensitySubtractOperationExecutor;

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

  Object *surface_ob_ = nullptr;
  Mesh *surface_ = nullptr;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;
  float2 brush_pos_re_;

  float minimum_distance_;

  CurvesSculptTransforms transforms_;
  BVHTreeFromMesh surface_bvh_;

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

    surface_ob_ = curves_id_->surface;
    if (surface_ob_ == nullptr) {
      return;
    }
    surface_ = static_cast<Mesh *>(surface_ob_->data);

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    minimum_distance_ = brush_->curves_sculpt_settings->minimum_distance;

    curve_selection_ = retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    transforms_ = CurvesSculptTransforms(*object_, curves_id_->surface);
    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);
    BKE_bvhtree_from_mesh_get(&surface_bvh_, surface_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_); });

    const Span<float3> positions_cu = curves_->positions();

    root_points_kdtree_ = BLI_kdtree_3d_new(curve_selection_.size());
    BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(root_points_kdtree_); });
    for (const int curve_i : curve_selection_) {
      const int first_point_i = curves_->offsets()[curve_i];
      const float3 &pos_cu = positions_cu[first_point_i];
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
    const IndexMask mask = index_mask_ops::find_indices_based_on_predicate(
        curves_->curves_range(), 4096, indices, [&](const int curve_i) {
          return curves_to_delete[curve_i];
        });

    curves_->remove_curves(mask);

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
    const Span<float3> positions_cu = curves_->positions();
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    const Span<int> offsets = curves_->offsets();

    /* Randomly select the curves that are allowed to be removed, based on the brush radius and
     * strength. */
    Array<bool> allow_remove_curve(curves_->curves_num(), false);
    threading::parallel_for(curves_->curves_range(), 512, [&](const IndexRange range) {
      RandomNumberGenerator rng((int)(PIL_check_seconds_timer() * 1000000.0));

      for (const int curve_i : range) {
        if (curves_to_delete[curve_i]) {
          allow_remove_curve[curve_i] = true;
          continue;
        }
        const int first_point_i = offsets[curve_i];
        const float3 pos_cu = brush_transform * positions_cu[first_point_i];

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
      const int first_point_i = offsets[curve_i];
      const float3 orig_pos_cu = positions_cu[first_point_i];
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
          [&](const int other_curve_i, const float *UNUSED(co), float UNUSED(dist_sq)) {
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
                                                                                 surface_bvh_,
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
    const Span<float3> positions_cu = curves_->positions();
    const Span<int> offsets = curves_->offsets();

    /* Randomly select the curves that are allowed to be removed, based on the brush radius and
     * strength. */
    Array<bool> allow_remove_curve(curves_->curves_num(), false);
    threading::parallel_for(curves_->curves_range(), 512, [&](const IndexRange range) {
      RandomNumberGenerator rng((int)(PIL_check_seconds_timer() * 1000000.0));

      for (const int curve_i : range) {
        if (curves_to_delete[curve_i]) {
          allow_remove_curve[curve_i] = true;
          continue;
        }
        const int first_point_i = offsets[curve_i];
        const float3 pos_cu = positions_cu[first_point_i];

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
      const int first_point_i = offsets[curve_i];
      const float3 &pos_cu = positions_cu[first_point_i];
      const float dist_to_brush_sq_cu = math::distance_squared(pos_cu, brush_pos_cu);
      if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
        continue;
      }

      BLI_kdtree_3d_range_search_cb_cpp(
          root_points_kdtree_,
          pos_cu,
          minimum_distance_,
          [&](const int other_curve_i, const float *UNUSED(co), float UNUSED(dist_sq)) {
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
  const eBrushCurvesSculptDensityMode density_mode = static_cast<eBrushCurvesSculptDensityMode>(
      brush.curves_sculpt_settings->density_mode);
  const bool use_invert = brush_mode == BRUSH_STROKE_INVERT;

  if (density_mode == BRUSH_CURVES_SCULPT_DENSITY_MODE_ADD) {
    return !use_invert;
  }
  if (density_mode == BRUSH_CURVES_SCULPT_DENSITY_MODE_REMOVE) {
    return use_invert;
  }

  const Object &curves_ob = *CTX_data_active_object(&C);
  const Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
  const CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
  if (curves_id.surface == nullptr) {
    /* The brush won't do anything in this case anyway. */
    return true;
  }
  if (curves.curves_num() <= 1) {
    return true;
  }

  const CurvesSculptTransforms transforms(curves_ob, curves_id.surface);
  BVHTreeFromMesh surface_bvh;
  BKE_bvhtree_from_mesh_get(
      &surface_bvh, static_cast<const Mesh *>(curves_id.surface->data), BVHTREE_FROM_LOOPTRI, 2);
  BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  const ARegion &region = *CTX_wm_region(&C);
  const View3D &v3d = *CTX_wm_view3d(&C);

  const float2 brush_pos_re = stroke_start.mouse_position;
  /* Reduce radius so that only an inner circle is used to determine the existing density. */
  const float brush_radius_re = BKE_brush_size_get(&scene, &brush) * 0.5f;

  /* Find the surface point under the brush. */
  const std::optional<CurvesBrush3D> brush_3d = sample_curves_surface_3d_brush(
      depsgraph, region, v3d, transforms, surface_bvh, brush_pos_re, brush_radius_re);
  if (!brush_3d.has_value()) {
    return true;
  }

  const float3 brush_pos_cu = brush_3d->position_cu;
  const float brush_radius_cu = brush_3d->radius_cu;
  const float brush_radius_sq_cu = pow2f(brush_radius_cu);

  const Span<int> offsets = curves.offsets();
  const Span<float3> positions_cu = curves.positions();

  /* Compute distance from brush to curve roots. */
  Array<std::pair<float, int>> distances_sq_to_brush(curves.curves_num());
  threading::EnumerableThreadSpecific<int> valid_curve_count_by_thread;
  threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange range) {
    int &valid_curve_count = valid_curve_count_by_thread.local();
    for (const int curve_i : range) {
      const int root_point_i = offsets[curve_i];
      const float3 &root_pos_cu = positions_cu[root_point_i];
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
    const float3 &pos_i = positions_cu[offsets[distances_sq_to_brush[i].second]];
    for (int j = i + 1; j < check_curve_count; j++) {
      const float3 &pos_j = positions_cu[offsets[distances_sq_to_brush[j].second]];
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
