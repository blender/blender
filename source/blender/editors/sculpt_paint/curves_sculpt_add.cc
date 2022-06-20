/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_float4x4.hh"
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
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"
#include "BKE_paint.h"
#include "BKE_report.h"

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

#include "GEO_add_curves_on_mesh.hh"

#include "WM_api.h"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * cu: Local space of the curves object that is being edited.
 * su: Local space of the surface object.
 * wo: World space.
 * re: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

using bke::CurvesGeometry;

class AddOperation : public CurvesSculptStrokeOperation {
 private:
  /** Used when some data should be interpolated from existing curves. */
  KDTree_3d *curve_roots_kdtree_ = nullptr;

  friend struct AddOperationExecutor;

 public:
  ~AddOperation() override
  {
    if (curve_roots_kdtree_ != nullptr) {
      BLI_kdtree_3d_free(curve_roots_kdtree_);
    }
  }

  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct AddOperationExecutor {
  AddOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  Object *surface_ob_ = nullptr;
  Mesh *surface_ = nullptr;
  Span<MLoopTri> surface_looptris_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  const BrushCurvesSculptSettings *brush_settings_ = nullptr;
  int add_amount_;
  bool use_front_face_;

  float brush_radius_re_;
  float2 brush_pos_re_;

  CurvesSculptTransforms transforms_;

  BVHTreeFromMesh surface_bvh_;

  struct AddedPoints {
    Vector<float3> positions_cu;
    Vector<float3> bary_coords;
    Vector<int> looptri_indices;
  };

  AddOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(AddOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    object_ = CTX_data_active_object(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);

    if (curves_id_->surface == nullptr || curves_id_->surface->type != OB_MESH) {
      return;
    }

    transforms_ = CurvesSculptTransforms(*object_, curves_id_->surface);

    surface_ob_ = curves_id_->surface;
    surface_ = static_cast<Mesh *>(surface_ob_->data);

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_settings_ = brush_->curves_sculpt_settings;
    brush_radius_re_ = brush_radius_get(*ctx_.scene, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    use_front_face_ = brush_->flag & BRUSH_FRONTFACE;
    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);
    add_amount_ = std::max(0, brush_settings_->add_amount);

    if (add_amount_ == 0) {
      return;
    }

    const double time = PIL_check_seconds_timer() * 1000000.0;
    /* Use a pointer cast to avoid overflow warnings. */
    RandomNumberGenerator rng{*(uint32_t *)(&time)};

    BKE_bvhtree_from_mesh_get(&surface_bvh_, surface_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_); });

    surface_looptris_ = {BKE_mesh_runtime_looptri_ensure(surface_),
                         BKE_mesh_runtime_looptri_len(surface_)};

    /* Sample points on the surface using one of multiple strategies. */
    AddedPoints added_points;
    if (add_amount_ == 1) {
      this->sample_in_center_with_symmetry(added_points);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      this->sample_projected_with_symmetry(rng, added_points);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->sample_spherical_with_symmetry(rng, added_points);
    }
    else {
      BLI_assert_unreachable();
    }

    if (added_points.bary_coords.is_empty()) {
      /* No new points have been added. */
      return;
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
    add_inputs.root_positions_cu = added_points.positions_cu;
    add_inputs.bary_coords = added_points.bary_coords;
    add_inputs.looptri_indices = added_points.looptri_indices;
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

    if (add_inputs.interpolate_length || add_inputs.interpolate_shape ||
        add_inputs.interpolate_point_count) {
      this->ensure_curve_roots_kdtree();
      add_inputs.old_roots_kdtree = self_->curve_roots_kdtree_;
    }

    geometry::add_curves_on_mesh(*curves_, add_inputs);

    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  /**
   * Sample a single point exactly at the mouse position.
   */
  void sample_in_center_with_symmetry(AddedPoints &r_added_points)
  {
    float3 ray_start_wo, ray_end_wo;
    ED_view3d_win_to_segment_clipped(
        ctx_.depsgraph, ctx_.region, ctx_.v3d, brush_pos_re_, ray_start_wo, ray_end_wo, true);
    const float3 ray_start_cu = transforms_.world_to_curves * ray_start_wo;
    const float3 ray_end_cu = transforms_.world_to_curves * ray_end_wo;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));

    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      const float4x4 transform = transforms_.curves_to_surface * brush_transform;
      this->sample_in_center(r_added_points, transform * ray_start_cu, transform * ray_end_cu);
    }
  }

  void sample_in_center(AddedPoints &r_added_points,
                        const float3 &ray_start_su,
                        const float3 &ray_end_su)
  {
    const float3 ray_direction_su = math::normalize(ray_end_su - ray_start_su);

    BVHTreeRayHit ray_hit;
    ray_hit.dist = FLT_MAX;
    ray_hit.index = -1;
    BLI_bvhtree_ray_cast(surface_bvh_.tree,
                         ray_start_su,
                         ray_direction_su,
                         0.0f,
                         &ray_hit,
                         surface_bvh_.raycast_callback,
                         &surface_bvh_);

    if (ray_hit.index == -1) {
      return;
    }

    const int looptri_index = ray_hit.index;
    const float3 brush_pos_su = ray_hit.co;
    const float3 bary_coords = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
        *surface_, surface_looptris_[looptri_index], brush_pos_su);

    const float3 brush_pos_cu = transforms_.surface_to_curves * brush_pos_su;

    r_added_points.positions_cu.append(brush_pos_cu);
    r_added_points.bary_coords.append(bary_coords);
    r_added_points.looptri_indices.append(looptri_index);
  }

  /**
   * Sample points by shooting rays within the brush radius in the 3D view.
   */
  void sample_projected_with_symmetry(RandomNumberGenerator &rng, AddedPoints &r_added_points)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->sample_projected(rng, r_added_points, brush_transform);
    }
  }

  void sample_projected(RandomNumberGenerator &rng,
                        AddedPoints &r_added_points,
                        const float4x4 &brush_transform)
  {
    const int old_amount = r_added_points.bary_coords.size();
    const int max_iterations = 100;
    int current_iteration = 0;
    while (r_added_points.bary_coords.size() < old_amount + add_amount_) {
      if (current_iteration++ >= max_iterations) {
        break;
      }
      const int missing_amount = add_amount_ + old_amount - r_added_points.bary_coords.size();
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
            const float3 start_cu = brush_transform * (transforms_.world_to_curves * start_wo);
            const float3 end_cu = brush_transform * (transforms_.world_to_curves * end_wo);
            r_start_su = transforms_.curves_to_surface * start_cu;
            r_end_su = transforms_.curves_to_surface * end_cu;
          },
          use_front_face_,
          add_amount_,
          missing_amount,
          r_added_points.bary_coords,
          r_added_points.looptri_indices,
          r_added_points.positions_cu);
      for (float3 &pos : r_added_points.positions_cu.as_mutable_span().take_back(new_points)) {
        pos = transforms_.surface_to_curves * pos;
      }
    }
  }

  /**
   * Sample points in a 3D sphere around the surface position that the mouse hovers over.
   */
  void sample_spherical_with_symmetry(RandomNumberGenerator &rng, AddedPoints &r_added_points)
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

    float3 view_ray_start_wo, view_ray_end_wo;
    ED_view3d_win_to_segment_clipped(ctx_.depsgraph,
                                     ctx_.region,
                                     ctx_.v3d,
                                     brush_pos_re_,
                                     view_ray_start_wo,
                                     view_ray_end_wo,
                                     true);
    const float3 view_direction_su = math::normalize(
        transforms_.world_to_surface * view_ray_end_wo -
        transforms_.world_to_surface * view_ray_start_wo);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      const float4x4 transform = transforms_.curves_to_surface * brush_transform;
      const float3 brush_pos_su = transform * brush_3d->position_cu;
      const float brush_radius_su = transform_brush_radius(
          transform, brush_3d->position_cu, brush_3d->radius_cu);
      this->sample_spherical(
          rng, r_added_points, brush_pos_su, brush_radius_su, view_direction_su);
    }
  }

  void sample_spherical(RandomNumberGenerator &rng,
                        AddedPoints &r_added_points,
                        const float3 &brush_pos_su,
                        const float brush_radius_su,
                        const float3 &view_direction_su)
  {
    const float brush_radius_sq_su = pow2f(brush_radius_su);

    /* Find surface triangles within brush radius. */
    Vector<int> looptri_indices;
    if (use_front_face_) {
      BLI_bvhtree_range_query_cpp(
          *surface_bvh_.tree,
          brush_pos_su,
          brush_radius_su,
          [&](const int index, const float3 &UNUSED(co), const float UNUSED(dist_sq)) {
            const MLoopTri &looptri = surface_looptris_[index];
            const float3 v0_su = surface_->mvert[surface_->mloop[looptri.tri[0]].v].co;
            const float3 v1_su = surface_->mvert[surface_->mloop[looptri.tri[1]].v].co;
            const float3 v2_su = surface_->mvert[surface_->mloop[looptri.tri[2]].v].co;
            float3 normal_su;
            normal_tri_v3(normal_su, v0_su, v1_su, v2_su);
            if (math::dot(normal_su, view_direction_su) >= 0.0f) {
              return;
            }
            looptri_indices.append(index);
          });
    }
    else {
      BLI_bvhtree_range_query_cpp(
          *surface_bvh_.tree,
          brush_pos_su,
          brush_radius_su,
          [&](const int index, const float3 &UNUSED(co), const float UNUSED(dist_sq)) {
            looptri_indices.append(index);
          });
    }

    /* Density used for sampling points. This does not have to be exact, because the loop below
     * automatically runs until enough samples have been found. If too many samples are found, some
     * will be discarded afterwards. */
    const float brush_plane_area_su = M_PI * brush_radius_sq_su;
    const float approximate_density_su = add_amount_ / brush_plane_area_su;

    /* Usually one or two iterations should be enough. */
    const int max_iterations = 5;
    int current_iteration = 0;

    const int old_amount = r_added_points.bary_coords.size();
    while (r_added_points.bary_coords.size() < old_amount + add_amount_) {
      if (current_iteration++ >= max_iterations) {
        break;
      }
      const int new_points = bke::mesh_surface_sample::sample_surface_points_spherical(
          rng,
          *surface_,
          looptri_indices,
          brush_pos_su,
          brush_radius_su,
          approximate_density_su,
          r_added_points.bary_coords,
          r_added_points.looptri_indices,
          r_added_points.positions_cu);
      for (float3 &pos : r_added_points.positions_cu.as_mutable_span().take_back(new_points)) {
        pos = transforms_.surface_to_curves * pos;
      }
    }

    /* Remove samples when there are too many. */
    while (r_added_points.bary_coords.size() > old_amount + add_amount_) {
      const int index_to_remove = rng.get_int32(add_amount_) + old_amount;
      r_added_points.bary_coords.remove_and_reorder(index_to_remove);
      r_added_points.looptri_indices.remove_and_reorder(index_to_remove);
      r_added_points.positions_cu.remove_and_reorder(index_to_remove);
    }
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
};

void AddOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  AddOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_add_operation(const bContext &C,
                                                               ReportList *reports)
{
  const Object &ob_active = *CTX_data_active_object(&C);
  BLI_assert(ob_active.type == OB_CURVES);
  const Curves &curves_id = *static_cast<Curves *>(ob_active.data);
  if (curves_id.surface == nullptr || curves_id.surface->type != OB_MESH) {
    BKE_report(reports, RPT_WARNING, "Can not use Add brush when there is no surface mesh");
    return {};
  }

  return std::make_unique<AddOperation>();
}

}  // namespace blender::ed::sculpt_paint
