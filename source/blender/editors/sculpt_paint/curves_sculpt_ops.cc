/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_utildefines.h"

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_paint.h"
#include "BKE_spline.hh"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "ED_curves_sculpt.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.h"

#include "BLI_index_mask_ops.hh"
#include "BLI_kdtree.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.hh"

#include "PIL_time.h"

#include "curves_sculpt_intern.h"
#include "curves_sculpt_intern.hh"
#include "paint_intern.h"

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool CURVES_SCULPT_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT_CURVES;
}

bool CURVES_SCULPT_mode_poll_view3d(bContext *C)
{
  if (!CURVES_SCULPT_mode_poll(C)) {
    return false;
  }
  if (CTX_wm_region_view3d(C) == nullptr) {
    return false;
  }
  return true;
}

/** \} */

namespace blender::ed::sculpt_paint {

using blender::bke::CurvesGeometry;
using blender::fn::CPPType;

/* -------------------------------------------------------------------- */
/** \name * SCULPT_CURVES_OT_brush_stroke
 * \{ */

/**
 * Resamples the curves to a shorter length.
 */
class ShrinkOperation : public CurvesSculptStrokeOperation {
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

        const float3 old_tip_position = positions[last_point_i];

        float2 old_tip_position_screen;
        ED_view3d_project_float_v2_m4(
            region, old_tip_position, old_tip_position_screen, projection.values);

        const float distance_screen = math::distance(old_tip_position_screen, mouse_prev);
        if (distance_screen > brush_radius) {
          continue;
        }

        const float radius_falloff = pow2f(1.0f - distance_screen / brush_radius);
        const float weight = brush_strength * radius_falloff;

        const float2 offset_tip_position_screen = old_tip_position_screen + weight * mouse_diff;
        float3 offset_tip_position;
        ED_view3d_win_to_3d(v3d,
                            region,
                            ob_mat * old_tip_position,
                            offset_tip_position_screen,
                            offset_tip_position);
        offset_tip_position = ob_imat * offset_tip_position;
        const float shrink_length = math::distance(offset_tip_position, old_tip_position);

        this->shrink_curve(positions, curve_points, shrink_length);
      }
    });

    curves.tag_positions_changed();
    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);
  }

  void shrink_curve(MutableSpan<float3> positions,
                    const IndexRange curve_points,
                    const float shrink_length) const
  {
    PolySpline spline;
    spline.resize(curve_points.size());
    MutableSpan<float3> spline_positions = spline.positions();
    spline_positions.copy_from(positions.slice(curve_points));
    spline.mark_cache_invalid();
    const float old_length = spline.length();
    const float new_length = std::max(0.0f, old_length - shrink_length);
    const float length_factor = new_length / old_length;

    Vector<float> old_point_lengths;
    old_point_lengths.append(0.0f);
    for (const int i : spline_positions.index_range().drop_back(1)) {
      const float3 &p1 = spline_positions[i];
      const float3 &p2 = spline_positions[i + 1];
      const float length = math::distance(p1, p2);
      old_point_lengths.append(old_point_lengths.last() + length);
    }

    for (const int i : spline_positions.index_range()) {
      const float eval_length = old_point_lengths[i] * length_factor;
      const Spline::LookupResult lookup = spline.lookup_evaluated_length(eval_length);
      const float index_factor = lookup.evaluated_index + lookup.factor;
      float3 p;
      spline.sample_with_index_factors<float3>(spline_positions, {&index_factor, 1}, {&p, 1});
      positions[curve_points[i]] = p;
    }
  }
};

class AddOperation : public CurvesSculptStrokeOperation {
 private:
  /** Contains the root points of the curves that existed before this operation started. */
  KDTree_3d *old_kdtree_ = nullptr;
  /** Number of points in the kdtree above. */
  int old_kdtree_size_ = 0;

  /**
   * Indicates that the corresponding curve has already been created and can't be changed by this
   * operation anymore.
   */
  static constexpr int ExistsAlreadyIndex = INT32_MAX;

  struct NewPointsData {
    Vector<float3> bary_coords;
    Vector<int> looptri_indices;
    Vector<float3> positions;
    Vector<float3> normals;
  };

 public:
  ~AddOperation() override
  {
    if (old_kdtree_ != nullptr) {
      BLI_kdtree_3d_free(old_kdtree_);
    }
  }

  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension) override
  {
    Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
    Scene &scene = *CTX_data_scene(C);
    Object &object = *CTX_data_active_object(C);
    ARegion *region = CTX_wm_region(C);
    View3D *v3d = CTX_wm_view3d(C);

    Curves &curves_id = *static_cast<Curves *>(object.data);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);

    if (curves_id.surface == nullptr || curves_id.surface->type != OB_MESH) {
      return;
    }

    const Object &surface_ob = *curves_id.surface;
    const Mesh &surface = *static_cast<const Mesh *>(surface_ob.data);
    const float4x4 surface_ob_mat = surface_ob.obmat;
    const float4x4 surface_ob_imat = surface_ob_mat.inverted();

    ToolSettings &tool_settings = *scene.toolsettings;
    CurvesSculpt &curves_sculpt = *tool_settings.curves_sculpt;
    Brush &brush = *BKE_paint_brush(&curves_sculpt.paint);
    const float brush_radius_screen = BKE_brush_size_get(&scene, &brush);
    const float strength = BKE_brush_alpha_get(&scene, &brush);
    const float minimum_distance = curves_sculpt.distance;

    /* This is the main ray that is used to determine the brush position in 3D space. */
    float3 ray_start, ray_end;
    ED_view3d_win_to_segment_clipped(
        &depsgraph, region, v3d, stroke_extension.mouse_position, ray_start, ray_end, true);
    ray_start = surface_ob_imat * ray_start;
    ray_end = surface_ob_imat * ray_end;
    const float3 ray_direction = math::normalize(ray_end - ray_start);

    /* This ray is used to determine the brush radius in 3d space. */
    float3 offset_ray_start, offset_ray_end;
    ED_view3d_win_to_segment_clipped(&depsgraph,
                                     region,
                                     v3d,
                                     stroke_extension.mouse_position +
                                         float2(0, brush_radius_screen),
                                     offset_ray_start,
                                     offset_ray_end,
                                     true);
    offset_ray_start = surface_ob_imat * offset_ray_start;
    offset_ray_end = surface_ob_imat * offset_ray_end;

    float4x4 ob_imat;
    invert_m4_m4(ob_imat.values, object.obmat);

    const float4x4 transform = ob_imat * surface_ob_mat;

    BVHTreeFromMesh bvhtree;
    BKE_bvhtree_from_mesh_get(&bvhtree, &surface, BVHTREE_FROM_LOOPTRI, 2);

    /* Do a raycast against the surface object to find the brush position. */
    BVHTreeRayHit ray_hit;
    ray_hit.dist = FLT_MAX;
    ray_hit.index = -1;
    BLI_bvhtree_ray_cast(bvhtree.tree,
                         ray_start,
                         ray_direction,
                         0.0f,
                         &ray_hit,
                         bvhtree.raycast_callback,
                         &bvhtree);

    if (ray_hit.index == -1) {
      /* The ray did not hit the surface. */
      free_bvhtree_from_mesh(&bvhtree);
      return;
    }
    /* Brush position in the space of the surface object. */
    const float3 brush_pos_3d_surface = ray_hit.co;
    const float brush_radius_3d_surface = dist_to_line_v3(
        brush_pos_3d_surface, offset_ray_start, offset_ray_end);

    /* Brush position in the space of the curves object. */
    const float3 brush_pos_3d_curves = transform * brush_pos_3d_surface;
    const float brush_radius_3d_curves = dist_to_line_v3(
        brush_pos_3d_curves, transform * offset_ray_start, transform * offset_ray_end);

    Vector<int> looptri_indices = this->find_looptri_indices_to_consider(
        bvhtree, brush_pos_3d_surface, brush_radius_3d_surface);

    free_bvhtree_from_mesh(&bvhtree);

    if (old_kdtree_ == nullptr && minimum_distance > 0.0f) {
      old_kdtree_ = this->kdtree_from_curve_roots_and_positions(curves, curves.curves_range(), {});
      old_kdtree_size_ = curves.curves_size();
    }

    float density;
    if (minimum_distance > 0.0f) {
      /* Estimate the sampling density based on the target minimum distance. */
      density = strength * pow2f(1.0f / minimum_distance);
    }
    else {
      /* Sample a somewhat constant amount of points based on the strength. */
      const float brush_circle_area_3d = M_PI * pow2f(brush_radius_3d_curves);
      density = strength * 100.0f / brush_circle_area_3d;
    }

    NewPointsData new_points = this->sample_new_points(density,
                                                       minimum_distance,
                                                       brush_radius_3d_curves,
                                                       brush_pos_3d_curves,
                                                       looptri_indices,
                                                       transform,
                                                       surface);
    if (minimum_distance > 0.0f) {
      this->eliminate_too_close_points(new_points, curves, minimum_distance);
    }
    this->insert_new_curves(new_points, curves);

    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(region);
  }

 private:
  Vector<int> find_looptri_indices_to_consider(BVHTreeFromMesh &bvhtree,
                                               const float3 &brush_pos,
                                               const float brush_radius_3d)
  {
    Vector<int> looptri_indices;

    struct RangeQueryUserData {
      Vector<int> &indices;
    } range_query_user_data = {looptri_indices};

    BLI_bvhtree_range_query(
        bvhtree.tree,
        brush_pos,
        brush_radius_3d,
        [](void *userdata, int index, const float co[3], float dist_sq) {
          UNUSED_VARS(co, dist_sq);
          RangeQueryUserData &data = *static_cast<RangeQueryUserData *>(userdata);
          data.indices.append(index);
        },
        &range_query_user_data);

    return looptri_indices;
  }

  KDTree_3d *kdtree_from_curve_roots_and_positions(const CurvesGeometry &curves,
                                                   const IndexRange curves_range,
                                                   Span<float3> extra_positions)
  {
    const int tot_points = curves_range.size() + extra_positions.size();
    KDTree_3d *kdtree = BLI_kdtree_3d_new(tot_points);
    for (const int curve_i : curves_range) {
      const int first_point_i = curves.offsets()[curve_i];
      const float3 root_position = curves.positions()[first_point_i];
      BLI_kdtree_3d_insert(kdtree, ExistsAlreadyIndex, root_position);
    }
    for (const int i : extra_positions.index_range()) {
      BLI_kdtree_3d_insert(kdtree, i, extra_positions[i]);
    }
    BLI_kdtree_3d_balance(kdtree);
    return kdtree;
  }

  int float_to_int_amount(float amount_f, RandomNumberGenerator &rng)
  {
    const float add_probability = fractf(amount_f);
    const bool add_point = add_probability > rng.get_float();
    return (int)amount_f + (int)add_point;
  }

  bool is_too_close_to_existing_point(const float3 position, const float minimum_distance) const
  {
    if (old_kdtree_ == nullptr) {
      return false;
    }
    KDTreeNearest_3d nearest;
    nearest.index = -1;
    BLI_kdtree_3d_find_nearest(old_kdtree_, position, &nearest);
    if (nearest.index >= 0 && nearest.dist < minimum_distance) {
      return true;
    }
    return false;
  }

  NewPointsData sample_new_points(const float density,
                                  const float minimum_distance,
                                  const float brush_radius_3d,
                                  const float3 &brush_pos,
                                  const Span<int> looptri_indices,
                                  const float4x4 &transform,
                                  const Mesh &surface)
  {
    const float brush_radius_3d_sq = brush_radius_3d * brush_radius_3d;
    const float area_threshold = M_PI * brush_radius_3d_sq;

    const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&surface),
                                  BKE_mesh_runtime_looptri_len(&surface)};

    threading::EnumerableThreadSpecific<NewPointsData> new_points_per_thread;

    const double time = PIL_check_seconds_timer();
    const uint64_t time_as_int = *reinterpret_cast<const uint64_t *>(&time);
    const uint32_t rng_base_seed = time_as_int ^ (time_as_int >> 32);

    RandomNumberGenerator rng{rng_base_seed};

    threading::parallel_for(looptri_indices.index_range(), 512, [&](const IndexRange range) {
      RandomNumberGenerator looptri_rng{rng_base_seed + (uint32_t)range.start()};

      for (const int looptri_index : looptri_indices.slice(range)) {
        const MLoopTri &looptri = looptris[looptri_index];
        const float3 &v0 = transform * float3(surface.mvert[surface.mloop[looptri.tri[0]].v].co);
        const float3 &v1 = transform * float3(surface.mvert[surface.mloop[looptri.tri[1]].v].co);
        const float3 &v2 = transform * float3(surface.mvert[surface.mloop[looptri.tri[2]].v].co);
        const float looptri_area = area_tri_v3(v0, v1, v2);

        float3 normal;
        normal_tri_v3(normal, v0, v1, v2);

        /* Use a different sampling strategy depending on whether the triangle is large or small
         * compared to the brush size. When the triangle is small, points are distributed within
         * the triangle directly. If the triangle is larger than the brush, distribute new points
         * in a circle on the triangle plane. */
        if (looptri_area < area_threshold) {
          const int amount = this->float_to_int_amount(looptri_area * density, looptri_rng);

          threading::parallel_for(IndexRange(amount), 512, [&](const IndexRange amount_range) {
            RandomNumberGenerator point_rng{rng_base_seed + looptri_index * 1000 +
                                            (uint32_t)amount_range.start()};
            NewPointsData &new_points = new_points_per_thread.local();

            for ([[maybe_unused]] const int i : amount_range) {
              const float3 bary_coord = point_rng.get_barycentric_coordinates();
              const float3 point_pos = attribute_math::mix3(bary_coord, v0, v1, v2);

              if (math::distance(point_pos, brush_pos) > brush_radius_3d) {
                continue;
              }
              if (minimum_distance > 0.0f &&
                  this->is_too_close_to_existing_point(point_pos, minimum_distance)) {
                continue;
              }

              new_points.bary_coords.append(bary_coord);
              new_points.looptri_indices.append(looptri_index);
              new_points.positions.append(point_pos);
              new_points.normals.append(normal);
            }
          });
        }
        else {
          float3 hit_pos_proj = brush_pos;
          project_v3_plane(hit_pos_proj, normal, v0);
          const float proj_distance_sq = math::distance_squared(hit_pos_proj, brush_pos);
          const float brush_radius_factor_sq = 1.0f -
                                               std::min(1.0f,
                                                        proj_distance_sq / brush_radius_3d_sq);
          const float radius_proj_sq = brush_radius_3d_sq * brush_radius_factor_sq;
          const float radius_proj = std::sqrt(radius_proj_sq);
          const float circle_area = M_PI * radius_proj_sq;

          const int amount = this->float_to_int_amount(circle_area * density, rng);

          const float3 axis_1 = math::normalize(v1 - v0) * radius_proj;
          const float3 axis_2 = math::normalize(
                                    math::cross(axis_1, math::cross(axis_1, v2 - v0))) *
                                radius_proj;

          threading::parallel_for(IndexRange(amount), 512, [&](const IndexRange amount_range) {
            RandomNumberGenerator point_rng{rng_base_seed + looptri_index * 1000 +
                                            (uint32_t)amount_range.start()};
            NewPointsData &new_points = new_points_per_thread.local();

            for ([[maybe_unused]] const int i : amount_range) {
              const float r = std::sqrt(rng.get_float());
              const float angle = rng.get_float() * 2 * M_PI;
              const float x = r * std::cos(angle);
              const float y = r * std::sin(angle);

              const float3 point_pos = hit_pos_proj + axis_1 * x + axis_2 * y;

              if (!isect_point_tri_prism_v3(point_pos, v0, v1, v2)) {
                continue;
              }
              if (minimum_distance > 0.0f &&
                  this->is_too_close_to_existing_point(point_pos, minimum_distance)) {
                continue;
              }

              float3 bary_coord;
              interp_weights_tri_v3(bary_coord, v0, v1, v2, point_pos);

              new_points.bary_coords.append(bary_coord);
              new_points.looptri_indices.append(looptri_index);
              new_points.positions.append(point_pos);
              new_points.normals.append(normal);
            }
          });
        }
      }
    });

    NewPointsData new_points;
    for (const NewPointsData &local_new_points : new_points_per_thread) {
      new_points.bary_coords.extend(local_new_points.bary_coords);
      new_points.looptri_indices.extend(local_new_points.looptri_indices);
      new_points.positions.extend(local_new_points.positions);
      new_points.normals.extend(local_new_points.normals);
    }
    return new_points;
  }

  void eliminate_too_close_points(NewPointsData &points,
                                  const CurvesGeometry &curves,
                                  const float minimum_distance)
  {
    Array<bool> elimination_mask(points.positions.size(), false);

    const int curves_added_previously = curves.curves_size() - old_kdtree_size_;
    KDTree_3d *new_points_kdtree = this->kdtree_from_curve_roots_and_positions(
        curves, IndexRange(old_kdtree_size_, curves_added_previously), points.positions);

    Array<Vector<int>> points_in_range(points.positions.size());
    threading::parallel_for(points.positions.index_range(), 256, [&](const IndexRange range) {
      for (const int point_i : range) {
        const float3 query_position = points.positions[point_i];

        struct CallbackData {
          int point_i;
          Vector<int> &found_indices;
          MutableSpan<bool> elimination_mask;
        } callback_data = {point_i, points_in_range[point_i], elimination_mask};

        BLI_kdtree_3d_range_search_cb(
            new_points_kdtree,
            query_position,
            minimum_distance,
            [](void *user_data, int index, const float *UNUSED(co), float UNUSED(dist_sq)) {
              CallbackData &data = *static_cast<CallbackData *>(user_data);
              if (index == data.point_i) {
                /* Ignore self. */
                return true;
              }
              if (index == ExistsAlreadyIndex) {
                /* An already existing point is too close, so this new point will be eliminated. */
                data.elimination_mask[data.point_i] = true;
                return false;
              }
              data.found_indices.append(index);
              return true;
            },
            &callback_data);
      }
    });

    for (const int point_i : points.positions.index_range()) {
      if (elimination_mask[point_i]) {
        /* Point is eliminated already. */
        continue;
      }

      for (const int other_point_i : points_in_range[point_i]) {
        elimination_mask[other_point_i] = true;
      }
    }

    BLI_kdtree_3d_free(new_points_kdtree);
    for (int i = points.positions.size() - 1; i >= 0; i--) {
      if (elimination_mask[i]) {
        points.positions.remove_and_reorder(i);
        points.bary_coords.remove_and_reorder(i);
        points.looptri_indices.remove_and_reorder(i);
        points.normals.remove_and_reorder(i);
      }
    }
  }

  void insert_new_curves(const NewPointsData &new_points, CurvesGeometry &curves)
  {
    const int tot_new_curves = new_points.positions.size();

    const int points_per_curve = 8;
    curves.resize(curves.points_size() + tot_new_curves * points_per_curve,
                  curves.curves_size() + tot_new_curves);

    MutableSpan<int> offsets = curves.offsets();
    MutableSpan<float3> positions = curves.positions();

    for (const int i : IndexRange(tot_new_curves)) {
      const int curve_i = curves.curves_size() - tot_new_curves + i;
      const int first_point_i = offsets[curve_i];
      offsets[curve_i + 1] = offsets[curve_i] + points_per_curve;

      const float3 root = new_points.positions[i];
      const float3 tip = root + 0.1f * new_points.normals[i];

      for (const int j : IndexRange(points_per_curve)) {
        positions[first_point_i + j] = math::interpolate(
            root, tip, j / (float)(points_per_curve - 1));
      }
    }
  }
};

static std::unique_ptr<CurvesSculptStrokeOperation> start_brush_operation(bContext *C,
                                                                          wmOperator *UNUSED(op))
{
  Scene &scene = *CTX_data_scene(C);
  CurvesSculpt &curves_sculpt = *scene.toolsettings->curves_sculpt;
  Brush &brush = *BKE_paint_brush(&curves_sculpt.paint);
  switch (brush.curves_sculpt_tool) {
    case CURVES_SCULPT_TOOL_COMB:
      return new_comb_operation();
    case CURVES_SCULPT_TOOL_DELETE:
      return new_delete_operation();
    case CURVES_SCULPT_TOOL_SNAKE_HOOK:
      return new_snake_hook_operation();
    case CURVES_SCULPT_TOOL_TEST1:
      return std::make_unique<AddOperation>();
    case CURVES_SCULPT_TOOL_TEST2:
      return std::make_unique<ShrinkOperation>();
  }
  BLI_assert_unreachable();
  return {};
}

struct SculptCurvesBrushStrokeData {
  std::unique_ptr<CurvesSculptStrokeOperation> operation;
  PaintStroke *stroke;
};

static bool stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
  out[0] = mouse[0];
  out[1] = mouse[1];
  out[2] = 0;
  UNUSED_VARS(C);
  return true;
}

static bool stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
  UNUSED_VARS(C, op, mouse);
  return true;
}

static void stroke_update_step(bContext *C,
                               wmOperator *op,
                               PaintStroke *UNUSED(stroke),
                               PointerRNA *stroke_element)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);

  StrokeExtension stroke_extension;
  RNA_float_get_array(stroke_element, "mouse", stroke_extension.mouse_position);

  if (!op_data->operation) {
    stroke_extension.is_first = true;
    op_data->operation = start_brush_operation(C, op);
  }
  else {
    stroke_extension.is_first = false;
  }

  op_data->operation->on_stroke_extended(C, stroke_extension);
}

static void stroke_done(const bContext *C, PaintStroke *stroke)
{
  UNUSED_VARS(C, stroke);
}

static int sculpt_curves_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SculptCurvesBrushStrokeData *op_data = MEM_new<SculptCurvesBrushStrokeData>(__func__);
  op_data->stroke = paint_stroke_new(C,
                                     op,
                                     stroke_get_location,
                                     stroke_test_start,
                                     stroke_update_step,
                                     nullptr,
                                     stroke_done,
                                     event->type);
  op->customdata = op_data;

  int return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, op_data->stroke);
    MEM_delete(op_data);
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_curves_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);
  int return_value = paint_stroke_modal(C, op, event, op_data->stroke);
  if (ELEM(return_value, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    MEM_delete(op_data);
  }
  return return_value;
}

static void sculpt_curves_stroke_cancel(bContext *C, wmOperator *op)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);
  paint_stroke_cancel(C, op, op_data->stroke);
  MEM_delete(op_data);
}

static void SCULPT_CURVES_OT_brush_stroke(struct wmOperatorType *ot)
{
  ot->name = "Stroke Curves Sculpt";
  ot->idname = "SCULPT_CURVES_OT_brush_stroke";
  ot->description = "Sculpt curves using a brush";

  ot->invoke = sculpt_curves_stroke_invoke;
  ot->modal = sculpt_curves_stroke_modal;
  ot->cancel = sculpt_curves_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name * CURVES_OT_sculptmode_toggle
 * \{ */

static bool curves_sculptmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  if (ob->type != OB_CURVES) {
    return false;
  }
  return true;
}

static void curves_sculptmode_enter(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->curves_sculpt);
  CurvesSculpt *curves_sculpt = scene->toolsettings->curves_sculpt;

  ob->mode = OB_MODE_SCULPT_CURVES;

  paint_cursor_start(&curves_sculpt->paint, CURVES_SCULPT_mode_poll_view3d);
}

static void curves_sculptmode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ob->mode = OB_MODE_OBJECT;
}

static int curves_sculptmode_toggle_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  const bool is_mode_set = ob->mode == OB_MODE_SCULPT_CURVES;

  if (is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, OB_MODE_SCULPT_CURVES, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    curves_sculptmode_exit(C);
  }
  else {
    curves_sculptmode_enter(C);
  }

  WM_toolsystem_update_from_context_view3d(C);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
  return OPERATOR_CANCELLED;
}

static void CURVES_OT_sculptmode_toggle(wmOperatorType *ot)
{
  ot->name = "Curve Sculpt Mode Toggle";
  ot->idname = "CURVES_OT_sculptmode_toggle";
  ot->description = "Enter/Exit sculpt mode for curves";

  ot->exec = curves_sculptmode_toggle_exec;
  ot->poll = curves_sculptmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

/** \} */

}  // namespace blender::ed::sculpt_paint

/* -------------------------------------------------------------------- */
/** \name * Registration
 * \{ */

void ED_operatortypes_sculpt_curves()
{
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(SCULPT_CURVES_OT_brush_stroke);
  WM_operatortype_append(CURVES_OT_sculptmode_toggle);
}

/** \} */
