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
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_paint.h"
#include "BKE_report.h"
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

#include "WM_api.h"

/**
 * The code below uses a prefix naming convention to indicate the coordinate space:
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

static void initialize_straight_curve_positions(const float3 &p1,
                                                const float3 &p2,
                                                MutableSpan<float3> r_positions)
{
  const float step = 1.0f / (float)(r_positions.size() - 1);
  for (const int i : r_positions.index_range()) {
    r_positions[i] = math::interpolate(p1, p2, i * step);
  }
}

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct AddOperationExecutor {
  AddOperation *self_ = nullptr;
  const Depsgraph *depsgraph_ = nullptr;
  const Scene *scene_ = nullptr;
  ARegion *region_ = nullptr;
  const View3D *v3d_ = nullptr;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  Object *surface_ob_ = nullptr;
  Mesh *surface_ = nullptr;
  Span<MLoopTri> surface_looptris_;
  Span<float3> corner_normals_su_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  const BrushCurvesSculptSettings *brush_settings_ = nullptr;

  float brush_radius_re_;
  float2 brush_pos_re_;

  bool use_front_face_;
  bool interpolate_length_;
  bool interpolate_shape_;
  bool interpolate_point_count_;
  bool use_interpolation_;
  float new_curve_length_;
  int add_amount_;
  int constant_points_per_curve_;

  /** Various matrices to convert between coordinate spaces. */
  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;
  float4x4 world_to_surface_mat_;
  float4x4 surface_to_world_mat_;
  float4x4 surface_to_curves_mat_;
  float4x4 surface_to_curves_normal_mat_;

  BVHTreeFromMesh surface_bvh_;

  int tot_old_curves_;
  int tot_old_points_;

  struct AddedPoints {
    Vector<float3> positions_cu;
    Vector<float3> bary_coords;
    Vector<int> looptri_indices;
  };

  struct NeighborInfo {
    /* Curve index of the neighbor. */
    int index;
    /* The weights of all neighbors of a new curve add up to 1. */
    float weight;
  };
  static constexpr int max_neighbors = 5;
  using NeighborsVector = Vector<NeighborInfo, max_neighbors>;

  void execute(AddOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    depsgraph_ = CTX_data_depsgraph_pointer(&C);
    scene_ = CTX_data_scene(&C);
    object_ = CTX_data_active_object(&C);
    region_ = CTX_wm_region(&C);
    v3d_ = CTX_wm_view3d(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);

    if (curves_id_->surface == nullptr || curves_id_->surface->type != OB_MESH) {
      return;
    }

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    surface_ob_ = curves_id_->surface;
    surface_ = static_cast<Mesh *>(surface_ob_->data);
    surface_to_world_mat_ = surface_ob_->obmat;
    world_to_surface_mat_ = surface_to_world_mat_.inverted();
    surface_to_curves_mat_ = world_to_curves_mat_ * surface_to_world_mat_;
    surface_to_curves_normal_mat_ = surface_to_curves_mat_.inverted().transposed();

    if (!CustomData_has_layer(&surface_->ldata, CD_NORMAL)) {
      BKE_mesh_calc_normals_split(surface_);
    }
    corner_normals_su_ = {
        reinterpret_cast<const float3 *>(CustomData_get_layer(&surface_->ldata, CD_NORMAL)),
        surface_->totloop};

    curves_sculpt_ = scene_->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_settings_ = brush_->curves_sculpt_settings;
    brush_radius_re_ = brush_radius_get(*scene_, *brush_, stroke_extension);
    brush_pos_re_ = stroke_extension.mouse_position;

    use_front_face_ = brush_->flag & BRUSH_FRONTFACE;
    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);
    add_amount_ = std::max(0, brush_settings_->add_amount);
    constant_points_per_curve_ = std::max(2, brush_settings_->points_per_curve);
    interpolate_length_ = brush_settings_->flag & BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_LENGTH;
    interpolate_shape_ = brush_settings_->flag & BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_SHAPE;
    interpolate_point_count_ = brush_settings_->flag &
                               BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_POINT_COUNT;
    use_interpolation_ = interpolate_length_ || interpolate_shape_ || interpolate_point_count_;
    new_curve_length_ = brush_settings_->curve_length;

    tot_old_curves_ = curves_->curves_num();
    tot_old_points_ = curves_->points_num();

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

    Array<NeighborsVector> neighbors_per_curve;
    if (use_interpolation_) {
      this->ensure_curve_roots_kdtree();
      neighbors_per_curve = this->find_curve_neighbors(added_points);
    }

    /* Resize to add the new curves, building the offsets in the array owned by the curves. */
    const int tot_added_curves = added_points.bary_coords.size();
    curves_->resize(curves_->points_num(), curves_->curves_num() + tot_added_curves);
    if (interpolate_point_count_) {
      this->initialize_curve_offsets_with_interpolation(neighbors_per_curve);
    }
    else {
      this->initialize_curve_offsets_without_interpolation(constant_points_per_curve_);
    }

    /* Resize to add the correct point count calculated as part of building the offsets. */
    curves_->resize(curves_->offsets().last(), curves_->curves_num());

    this->initialize_attributes(added_points, neighbors_per_curve);

    curves_->update_curve_types();

    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(region_);
  }

  float3 get_bary_coords(const Mesh &mesh, const MLoopTri &looptri, const float3 position) const
  {
    const float3 &v0 = mesh.mvert[mesh.mloop[looptri.tri[0]].v].co;
    const float3 &v1 = mesh.mvert[mesh.mloop[looptri.tri[1]].v].co;
    const float3 &v2 = mesh.mvert[mesh.mloop[looptri.tri[2]].v].co;
    float3 bary_coords;
    interp_weights_tri_v3(bary_coords, v0, v1, v2, position);
    return bary_coords;
  }

  /**
   * Sample a single point exactly at the mouse position.
   */
  void sample_in_center_with_symmetry(AddedPoints &r_added_points)
  {
    float3 ray_start_wo, ray_end_wo;
    ED_view3d_win_to_segment_clipped(
        depsgraph_, region_, v3d_, brush_pos_re_, ray_start_wo, ray_end_wo, true);
    const float3 ray_start_su = world_to_surface_mat_ * ray_start_wo;
    const float3 ray_end_su = world_to_surface_mat_ * ray_end_wo;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));

    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->sample_in_center(
          r_added_points, brush_transform * ray_start_su, brush_transform * ray_end_su);
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
    const float3 bary_coords = this->get_bary_coords(
        *surface_, surface_looptris_[looptri_index], brush_pos_su);

    const float3 brush_pos_cu = surface_to_curves_mat_ * brush_pos_su;

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
    const int max_iterations = std::max(100'000, add_amount_ * 10);
    int current_iteration = 0;
    while (r_added_points.bary_coords.size() < old_amount + add_amount_) {
      if (current_iteration++ >= max_iterations) {
        break;
      }

      const float r = brush_radius_re_ * std::sqrt(rng.get_float());
      const float angle = rng.get_float() * 2.0f * M_PI;
      const float2 pos_re = brush_pos_re_ + r * float2(std::cos(angle), std::sin(angle));

      float3 ray_start_wo, ray_end_wo;
      ED_view3d_win_to_segment_clipped(
          depsgraph_, region_, v3d_, pos_re, ray_start_wo, ray_end_wo, true);
      const float3 ray_start_su = brush_transform * (world_to_surface_mat_ * ray_start_wo);
      const float3 ray_end_su = brush_transform * (world_to_surface_mat_ * ray_end_wo);
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
        continue;
      }

      if (use_front_face_) {
        const float3 normal_su = ray_hit.no;
        if (math::dot(ray_direction_su, normal_su) >= 0.0f) {
          continue;
        }
      }

      const int looptri_index = ray_hit.index;
      const float3 pos_su = ray_hit.co;

      const float3 bary_coords = this->get_bary_coords(
          *surface_, surface_looptris_[looptri_index], pos_su);

      const float3 pos_cu = surface_to_curves_mat_ * pos_su;

      r_added_points.positions_cu.append(pos_cu);
      r_added_points.bary_coords.append(bary_coords);
      r_added_points.looptri_indices.append(looptri_index);
    }
  }

  /**
   * Sample points in a 3D sphere around the surface position that the mouse hovers over.
   */
  void sample_spherical_with_symmetry(RandomNumberGenerator &rng, AddedPoints &r_added_points)
  {
    /* Find ray that starts in the center of the brush. */
    float3 brush_ray_start_wo, brush_ray_end_wo;
    ED_view3d_win_to_segment_clipped(
        depsgraph_, region_, v3d_, brush_pos_re_, brush_ray_start_wo, brush_ray_end_wo, true);
    const float3 brush_ray_start_su = world_to_surface_mat_ * brush_ray_start_wo;
    const float3 brush_ray_end_su = world_to_surface_mat_ * brush_ray_end_wo;

    /* Find ray that starts on the boundary of the brush. That is used to compute the brush radius
     * in 3D. */
    float3 brush_radius_ray_start_wo, brush_radius_ray_end_wo;
    ED_view3d_win_to_segment_clipped(depsgraph_,
                                     region_,
                                     v3d_,
                                     brush_pos_re_ + float2(brush_radius_re_, 0),
                                     brush_radius_ray_start_wo,
                                     brush_radius_ray_end_wo,
                                     true);
    const float3 brush_radius_ray_start_su = world_to_surface_mat_ * brush_radius_ray_start_wo;
    const float3 brush_radius_ray_end_su = world_to_surface_mat_ * brush_radius_ray_end_wo;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->sample_spherical(rng,
                             r_added_points,
                             brush_transform * brush_ray_start_su,
                             brush_transform * brush_ray_end_su,
                             brush_transform * brush_radius_ray_start_su,
                             brush_transform * brush_radius_ray_end_su);
    }
  }

  void sample_spherical(RandomNumberGenerator &rng,
                        AddedPoints &r_added_points,
                        const float3 &brush_ray_start_su,
                        const float3 &brush_ray_end_su,
                        const float3 &brush_radius_ray_start_su,
                        const float3 &brush_radius_ray_end_su)
  {
    const float3 brush_ray_direction_su = math::normalize(brush_ray_end_su - brush_ray_start_su);

    BVHTreeRayHit ray_hit;
    ray_hit.dist = FLT_MAX;
    ray_hit.index = -1;
    BLI_bvhtree_ray_cast(surface_bvh_.tree,
                         brush_ray_start_su,
                         brush_ray_direction_su,
                         0.0f,
                         &ray_hit,
                         surface_bvh_.raycast_callback,
                         &surface_bvh_);

    if (ray_hit.index == -1) {
      return;
    }

    /* Compute brush radius. */
    const float3 brush_pos_su = ray_hit.co;
    const float brush_radius_su = dist_to_line_v3(
        brush_pos_su, brush_radius_ray_start_su, brush_radius_ray_end_su);
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
            if (math::dot(normal_su, brush_ray_direction_su) >= 0.0f) {
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

    /* Used for switching between two triangle sampling strategies. */
    const float area_threshold = brush_plane_area_su;

    /* Usually one or two iterations should be enough. */
    const int max_iterations = 5;
    int current_iteration = 0;

    const int old_amount = r_added_points.bary_coords.size();
    while (r_added_points.bary_coords.size() < old_amount + add_amount_) {
      if (current_iteration++ >= max_iterations) {
        break;
      }

      for (const int looptri_index : looptri_indices) {
        const MLoopTri &looptri = surface_looptris_[looptri_index];

        const float3 v0_su = surface_->mvert[surface_->mloop[looptri.tri[0]].v].co;
        const float3 v1_su = surface_->mvert[surface_->mloop[looptri.tri[1]].v].co;
        const float3 v2_su = surface_->mvert[surface_->mloop[looptri.tri[2]].v].co;

        const float looptri_area_su = area_tri_v3(v0_su, v1_su, v2_su);

        if (looptri_area_su < area_threshold) {
          /* The triangle is small compared to the brush radius. Sample by generating random
           * barycentric coordinates. */
          const int amount = rng.round_probabilistic(approximate_density_su * looptri_area_su);
          for ([[maybe_unused]] const int i : IndexRange(amount)) {
            const float3 bary_coord = rng.get_barycentric_coordinates();
            const float3 point_pos_su = attribute_math::mix3(bary_coord, v0_su, v1_su, v2_su);
            const float distance_to_brush_sq_su = math::distance_squared(point_pos_su,
                                                                         brush_pos_su);
            if (distance_to_brush_sq_su > brush_radius_sq_su) {
              continue;
            }

            r_added_points.bary_coords.append(bary_coord);
            r_added_points.looptri_indices.append(looptri_index);
            r_added_points.positions_cu.append(surface_to_curves_mat_ * point_pos_su);
          }
        }
        else {
          /* The triangle is large compared to the brush radius. Sample by generating random points
           * on the triangle plane within the brush radius. */
          float3 normal_su;
          normal_tri_v3(normal_su, v0_su, v1_su, v2_su);

          float3 brush_pos_proj_su = brush_pos_su;
          project_v3_plane(brush_pos_proj_su, normal_su, v0_su);

          const float proj_distance_sq_su = math::distance_squared(brush_pos_proj_su,
                                                                   brush_pos_su);
          const float brush_radius_factor_sq = 1.0f -
                                               std::min(1.0f,
                                                        proj_distance_sq_su / brush_radius_sq_su);
          const float radius_proj_sq_su = brush_radius_sq_su * brush_radius_factor_sq;
          const float radius_proj_su = std::sqrt(radius_proj_sq_su);
          const float circle_area_su = M_PI * radius_proj_su;

          const int amount = rng.round_probabilistic(approximate_density_su * circle_area_su);

          const float3 axis_1_su = math::normalize(v1_su - v0_su) * radius_proj_su;
          const float3 axis_2_su = math::normalize(math::cross(
                                       axis_1_su, math::cross(axis_1_su, v2_su - v0_su))) *
                                   radius_proj_su;

          for ([[maybe_unused]] const int i : IndexRange(amount)) {
            const float r = std::sqrt(rng.get_float());
            const float angle = rng.get_float() * 2.0f * M_PI;
            const float x = r * std::cos(angle);
            const float y = r * std::sin(angle);
            const float3 point_pos_su = brush_pos_proj_su + axis_1_su * x + axis_2_su * y;
            if (!isect_point_tri_prism_v3(point_pos_su, v0_su, v1_su, v2_su)) {
              /* Sampled point is not in the triangle. */
              continue;
            }

            float3 bary_coord;
            interp_weights_tri_v3(bary_coord, v0_su, v1_su, v2_su, point_pos_su);

            r_added_points.bary_coords.append(bary_coord);
            r_added_points.looptri_indices.append(looptri_index);
            r_added_points.positions_cu.append(surface_to_curves_mat_ * point_pos_su);
          }
        }
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

  void initialize_curve_offsets_with_interpolation(const Span<NeighborsVector> neighbors_per_curve)
  {
    MutableSpan<int> new_offsets = curves_->offsets_for_write().drop_front(tot_old_curves_);

    attribute_math::DefaultMixer<int> mixer{new_offsets};
    threading::parallel_for(neighbors_per_curve.index_range(), 1024, [&](IndexRange curves_range) {
      for (const int i : curves_range) {
        if (neighbors_per_curve[i].is_empty()) {
          mixer.mix_in(i, constant_points_per_curve_, 1.0f);
        }
        else {
          for (const NeighborInfo &neighbor : neighbors_per_curve[i]) {
            const int neighbor_points_num = curves_->points_for_curve(neighbor.index).size();
            mixer.mix_in(i, neighbor_points_num, neighbor.weight);
          }
        }
      }
    });
    mixer.finalize();

    bke::curves::accumulate_counts_to_offsets(new_offsets, tot_old_points_);
  }

  void initialize_curve_offsets_without_interpolation(const int points_per_curve)
  {
    MutableSpan<int> new_offsets = curves_->offsets_for_write().drop_front(tot_old_curves_);
    int offset = tot_old_points_;
    for (const int i : new_offsets.index_range()) {
      new_offsets[i] = offset;
      offset += points_per_curve;
    }
  }

  void initialize_attributes(const AddedPoints &added_points,
                             const Span<NeighborsVector> neighbors_per_curve)
  {
    Array<float> new_lengths_cu(added_points.bary_coords.size());
    if (interpolate_length_) {
      this->interpolate_lengths(neighbors_per_curve, new_lengths_cu);
    }
    else {
      new_lengths_cu.fill(new_curve_length_);
    }

    Array<float3> new_normals_su = this->compute_normals_for_added_curves_su(added_points);
    this->initialize_surface_attachment(added_points);

    if (interpolate_shape_) {
      this->initialize_position_with_interpolation(
          added_points, neighbors_per_curve, new_normals_su, new_lengths_cu);
    }
    else {
      this->initialize_position_without_interpolation(
          added_points, new_lengths_cu, new_normals_su);
    }
  }

  Array<NeighborsVector> find_curve_neighbors(const AddedPoints &added_points)
  {
    const int tot_added_curves = added_points.bary_coords.size();
    Array<NeighborsVector> neighbors_per_curve(tot_added_curves);
    threading::parallel_for(IndexRange(tot_added_curves), 128, [&](const IndexRange range) {
      for (const int i : range) {
        const float3 root_cu = added_points.positions_cu[i];
        std::array<KDTreeNearest_3d, max_neighbors> nearest_n;
        const int found_neighbors = BLI_kdtree_3d_find_nearest_n(
            self_->curve_roots_kdtree_, root_cu, nearest_n.data(), max_neighbors);
        float tot_weight = 0.0f;
        for (const int neighbor_i : IndexRange(found_neighbors)) {
          KDTreeNearest_3d &nearest = nearest_n[neighbor_i];
          const float weight = 1.0f / std::max(nearest.dist, 0.00001f);
          tot_weight += weight;
          neighbors_per_curve[i].append({nearest.index, weight});
        }
        /* Normalize weights. */
        for (NeighborInfo &neighbor : neighbors_per_curve[i]) {
          neighbor.weight /= tot_weight;
        }
      }
    });
    return neighbors_per_curve;
  }

  void interpolate_lengths(const Span<NeighborsVector> neighbors_per_curve,
                           MutableSpan<float> r_lengths)
  {
    const Span<float3> positions_cu = curves_->positions();

    threading::parallel_for(r_lengths.index_range(), 128, [&](const IndexRange range) {
      for (const int added_curve_i : range) {
        const Span<NeighborInfo> neighbors = neighbors_per_curve[added_curve_i];
        float length_sum = 0.0f;
        for (const NeighborInfo &neighbor : neighbors) {
          const IndexRange neighbor_points = curves_->points_for_curve(neighbor.index);
          float neighbor_length = 0.0f;
          for (const int segment_i : neighbor_points.drop_back(1)) {
            const float3 &p1 = positions_cu[segment_i];
            const float3 &p2 = positions_cu[segment_i + 1];
            neighbor_length += math::distance(p1, p2);
          }
          length_sum += neighbor.weight * neighbor_length;
        }
        const float length = neighbors.is_empty() ? new_curve_length_ : length_sum;
        r_lengths[added_curve_i] = length;
      }
    });
  }

  float3 compute_point_normal_su(const int looptri_index, const float3 &bary_coord)
  {
    const MLoopTri &looptri = surface_looptris_[looptri_index];
    const int l0 = looptri.tri[0];
    const int l1 = looptri.tri[1];
    const int l2 = looptri.tri[2];

    const float3 &l0_normal_su = corner_normals_su_[l0];
    const float3 &l1_normal_su = corner_normals_su_[l1];
    const float3 &l2_normal_su = corner_normals_su_[l2];

    const float3 normal_su = math::normalize(
        attribute_math::mix3(bary_coord, l0_normal_su, l1_normal_su, l2_normal_su));
    return normal_su;
  }

  Array<float3> compute_normals_for_added_curves_su(const AddedPoints &added_points)
  {
    Array<float3> normals_su(added_points.bary_coords.size());
    threading::parallel_for(normals_su.index_range(), 256, [&](const IndexRange range) {
      for (const int i : range) {
        const int looptri_index = added_points.looptri_indices[i];
        const float3 &bary_coord = added_points.bary_coords[i];
        normals_su[i] = this->compute_point_normal_su(looptri_index, bary_coord);
      }
    });
    return normals_su;
  }

  void initialize_surface_attachment(const AddedPoints &added_points)
  {
    MutableSpan<int> surface_triangle_indices = curves_->surface_triangle_indices_for_write();
    MutableSpan<float2> surface_triangle_coords = curves_->surface_triangle_coords_for_write();
    threading::parallel_for(
        added_points.bary_coords.index_range(), 1024, [&](const IndexRange range) {
          for (const int i : range) {
            const int curve_i = tot_old_curves_ + i;
            surface_triangle_indices[curve_i] = added_points.looptri_indices[i];
            surface_triangle_coords[curve_i] = float2(added_points.bary_coords[i]);
          }
        });
  }

  /**
   * Initialize new curves so that they are just a straight line in the normal direction.
   */
  void initialize_position_without_interpolation(const AddedPoints &added_points,
                                                 const Span<float> lengths_cu,
                                                 const MutableSpan<float3> normals_su)
  {
    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    threading::parallel_for(
        added_points.bary_coords.index_range(), 256, [&](const IndexRange range) {
          for (const int i : range) {
            const IndexRange points = curves_->points_for_curve(tot_old_curves_ + i);
            const float3 &root_cu = added_points.positions_cu[i];
            const float length = lengths_cu[i];
            const float3 &normal_su = normals_su[i];
            const float3 normal_cu = math::normalize(surface_to_curves_normal_mat_ * normal_su);
            const float3 tip_cu = root_cu + length * normal_cu;

            initialize_straight_curve_positions(root_cu, tip_cu, positions_cu.slice(points));
          }
        });
  }

  /**
   * Use neighboring curves to determine the shape.
   */
  void initialize_position_with_interpolation(const AddedPoints &added_points,
                                              const Span<NeighborsVector> neighbors_per_curve,
                                              const Span<float3> new_normals_su,
                                              const Span<float> new_lengths_cu)
  {
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const VArray_Span<int> surface_triangle_indices{curves_->surface_triangle_indices()};
    const Span<float2> surface_triangle_coords = curves_->surface_triangle_coords();

    threading::parallel_for(
        added_points.bary_coords.index_range(), 256, [&](const IndexRange range) {
          for (const int i : range) {
            const Span<NeighborInfo> neighbors = neighbors_per_curve[i];
            const IndexRange points = curves_->points_for_curve(tot_old_curves_ + i);

            const float length_cu = new_lengths_cu[i];
            const float3 &normal_su = new_normals_su[i];
            const float3 normal_cu = math::normalize(surface_to_curves_normal_mat_ * normal_su);

            const float3 &root_cu = added_points.positions_cu[i];

            if (neighbors.is_empty()) {
              /* If there are no neighbors, just make a straight line. */
              const float3 tip_cu = root_cu + length_cu * normal_cu;
              initialize_straight_curve_positions(root_cu, tip_cu, positions_cu.slice(points));
              continue;
            }

            positions_cu.slice(points).fill(root_cu);

            for (const NeighborInfo &neighbor : neighbors) {
              const int neighbor_curve_i = neighbor.index;
              const int neighbor_looptri_index = surface_triangle_indices[neighbor_curve_i];

              float3 neighbor_bary_coord{surface_triangle_coords[neighbor_curve_i]};
              neighbor_bary_coord.z = 1.0f - neighbor_bary_coord.x - neighbor_bary_coord.y;

              const float3 neighbor_normal_su = this->compute_point_normal_su(
                  neighbor_looptri_index, neighbor_bary_coord);
              const float3 neighbor_normal_cu = math::normalize(surface_to_curves_normal_mat_ *
                                                                neighbor_normal_su);

              /* The rotation matrix used to transform relative coordinates of the neighbor curve
               * to the new curve. */
              float normal_rotation_cu[3][3];
              rotation_between_vecs_to_mat3(normal_rotation_cu, neighbor_normal_cu, normal_cu);

              const IndexRange neighbor_points = curves_->points_for_curve(neighbor_curve_i);
              const float3 &neighbor_root_cu = positions_cu[neighbor_points[0]];

              /* Use a temporary #PolySpline, because that's the easiest way to resample an
               * existing curve right now. Resampling is necessary if the length of the new curve
               * does not match the length of the neighbors or the number of handle points is
               * different. */
              PolySpline neighbor_spline;
              neighbor_spline.resize(neighbor_points.size());
              neighbor_spline.positions().copy_from(positions_cu.slice(neighbor_points));
              neighbor_spline.mark_cache_invalid();

              const float neighbor_length_cu = neighbor_spline.length();
              const float length_factor = std::min(1.0f, length_cu / neighbor_length_cu);

              const float resample_factor = (1.0f / (points.size() - 1.0f)) * length_factor;
              for (const int j : IndexRange(points.size())) {
                const Spline::LookupResult lookup = neighbor_spline.lookup_evaluated_factor(
                    j * resample_factor);
                const float index_factor = lookup.evaluated_index + lookup.factor;
                float3 p;
                neighbor_spline.sample_with_index_factors<float3>(
                    neighbor_spline.positions(), {&index_factor, 1}, {&p, 1});
                const float3 relative_coord = p - neighbor_root_cu;
                float3 rotated_relative_coord = relative_coord;
                mul_m3_v3(normal_rotation_cu, rotated_relative_coord);
                positions_cu[points[j]] += neighbor.weight * rotated_relative_coord;
              }
            }
          }
        });
  }
};

void AddOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  AddOperationExecutor executor;
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
