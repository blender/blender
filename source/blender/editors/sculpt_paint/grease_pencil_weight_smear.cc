/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "grease_pencil_weight_paint.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class SmearWeightPaintOperation : public WeightPaintOperation {
  /* Brush direction (angle) during a stroke movement. */
  float2 brush_direction;
  bool brush_direction_is_set;

  /** Get the direction of the brush while the mouse is moving. The direction is given as a
   * normalized XY vector. */
  bool get_brush_direction()
  {
    this->brush_direction = this->mouse_position - this->mouse_position_previous;

    /* Skip tiny changes in direction, we want the bigger movements only. */
    if (math::length_squared(this->brush_direction) < 9.0f) {
      return this->brush_direction_is_set;
    }

    this->brush_direction = math::normalize(this->brush_direction);
    this->brush_direction_is_set = true;
    this->mouse_position_previous = this->mouse_position;

    return true;
  }

  /** Apply the Smear tool to a point under the brush. */
  void apply_smear_tool(const BrushPoint &point,
                        DrawingWeightData &drawing_weight,
                        PointsTouchedByBrush &touched_points)
  {
    /* Find the nearest neighbors of the to-be-smeared point. */
    KDTreeNearest_2d nearest_points[SMEAR_NEIGHBOUR_NUM];
    const int point_num = BLI_kdtree_2d_find_nearest_n(
        touched_points.kdtree,
        drawing_weight.point_positions[point.drawing_point_index],
        nearest_points,
        SMEAR_NEIGHBOUR_NUM);

    /* For smearing a weight to point A, we look for a point B in the trail of the mouse
     * movement, matching the last known brush angle best and with the shortest distance to A. */
    float point_dot_product[SMEAR_NEIGHBOUR_NUM];
    float min_distance = FLT_MAX, max_distance = -FLT_MAX;
    int smear_point_num = 0;
    for (const int i : IndexRange(point_num)) {
      /* Skip the point we are about to smear. */
      if (nearest_points[i].dist < FIND_NEAREST_POINT_EPSILON) {
        continue;
      }
      const float2 direction_nearest_to_point = math::normalize(
          drawing_weight.point_positions[point.drawing_point_index] -
          float2(nearest_points[i].co));

      /* Match point direction with brush direction. */
      point_dot_product[i] = math::dot(direction_nearest_to_point, this->brush_direction);
      if (point_dot_product[i] <= 0.0f) {
        continue;
      }
      smear_point_num++;
      min_distance = math::min(min_distance, nearest_points[i].dist);
      max_distance = math::max(max_distance, nearest_points[i].dist);
    }
    if (smear_point_num == 0) {
      return;
    }

    /* Find best match in angle and distance. */
    int best_match = -1;
    float max_score = 0.0f;
    const float distance_normalizer = (min_distance == max_distance) ?
                                          1.0f :
                                          (0.95f / (max_distance - min_distance));
    for (const int i : IndexRange(point_num)) {
      if (point_dot_product[i] <= 0.0f) {
        continue;
      }
      const float score = point_dot_product[i] *
                          (1.0f - (nearest_points[i].dist - min_distance) * distance_normalizer);
      if (score > max_score) {
        max_score = score;
        best_match = i;
      }
    }
    if (best_match == -1) {
      return;
    }
    const float smear_weight = touched_points.weights[nearest_points[best_match].index];

    apply_weight_to_point(point, smear_weight, drawing_weight);
  }

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override
  {
    using namespace blender::ed::greasepencil;

    this->get_brush_settings(C, start_sample);
    this->ensure_active_vertex_group_in_object();
    this->get_locked_and_bone_deformed_vertex_groups();

    /* Get editable drawings grouped per frame number. When multi-frame editing is disabled, this
     * is just one group for the current frame. When multi-frame editing is enabled, the selected
     * keyframes are grouped per frame number. This way we can use Smear on multiple layers
     * together instead of on every layer individually. */
    const Scene *scene = CTX_data_scene(&C);
    Array<Vector<MutableDrawingInfo>> drawings_per_frame =
        retrieve_editable_drawings_grouped_per_frame(*scene, *this->grease_pencil);

    this->drawing_weight_data = Array<Array<DrawingWeightData>>(drawings_per_frame.size());

    /* Get weight data for all drawings in this frame group. */
    for (const int frame_group : drawings_per_frame.index_range()) {
      const Vector<MutableDrawingInfo> &drawings = drawings_per_frame[frame_group];
      this->init_weight_data_for_drawings(C, drawings, frame_group);
    }
  }

  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override
  {
    using namespace blender::ed::greasepencil;

    this->get_mouse_input_sample(extension_sample);

    /* For the Smear tool, we use the direction of the brush during the stroke movement. The
     * direction is derived from the current and previous mouse position. */
    if (!this->get_brush_direction()) {
      /* Abort when no direction is established yet. */
      return;
    }

    /* Iterate over the drawings grouped per frame number. Collect all stroke points under the
     * brush and smear them. */
    std::atomic<bool> changed = false;
    threading::parallel_for_each(
        this->drawing_weight_data.index_range(), [&](const int frame_group) {
          Array<DrawingWeightData> &drawing_weights = this->drawing_weight_data[frame_group];

          /* For all layers at this key frame, collect the stroke points under the brush in a
           * buffer. */
          threading::parallel_for_each(drawing_weights, [&](DrawingWeightData &drawing_weight) {
            for (const int point_index : drawing_weight.point_positions.index_range()) {
              const float2 &co = drawing_weight.point_positions[point_index];

              /* When the point is under the brush, add it to the brush point buffer. */
              this->add_point_under_brush_to_brush_buffer(co, drawing_weight, point_index);
            }
          });

          /* Create a KDTree with all stroke points touched by the brush during the weight paint
           * operation. */
          PointsTouchedByBrush touched_points = this->create_affected_points_kdtree(
              drawing_weights);

          /* Apply the Smear tool to all points in the brush buffer. */
          threading::parallel_for_each(drawing_weights, [&](DrawingWeightData &drawing_weight) {
            for (const BrushPoint &point : drawing_weight.points_in_brush) {
              this->apply_smear_tool(point, drawing_weight, touched_points);

              /* Normalize weights of bone-deformed vertex groups to 1.0f. */
              if (this->auto_normalize) {
                normalize_vertex_weights(drawing_weight.deform_verts[point.drawing_point_index],
                                         drawing_weight.active_vertex_group,
                                         drawing_weight.locked_vgroups,
                                         drawing_weight.bone_deformed_vgroups);
              }
            }

            if (!drawing_weight.points_in_brush.is_empty()) {
              changed = true;
              drawing_weight.points_in_brush.clear();
            }
          });

          BLI_kdtree_2d_free(touched_points.kdtree);
        });

    if (changed) {
      DEG_id_tag_update(&this->grease_pencil->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
    }
  }

  void on_stroke_done(const bContext & /*C*/) override {}
};

std::unique_ptr<GreasePencilStrokeOperation> new_weight_paint_smear_operation()
{
  return std::make_unique<SmearWeightPaintOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
