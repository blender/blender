/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "grease_pencil_weight_paint.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class BlurWeightPaintOperation : public WeightPaintOperation {
  /* Apply the Blur tool to a point under the brush. */
  void apply_blur_tool(const BrushPoint &point,
                       DrawingWeightData &drawing_weight,
                       PointsTouchedByBrush &touched_points)
  {
    /* Find the nearest neighbours of the to-be-blurred point. The point itself is included. */
    KDTreeNearest_2d nearest_points[BLUR_NEIGHBOUR_NUM];
    const int point_num = BLI_kdtree_2d_find_nearest_n(
        touched_points.kdtree,
        drawing_weight.point_positions[point.drawing_point_index],
        nearest_points,
        BLUR_NEIGHBOUR_NUM);

    if (point_num <= 1) {
      return;
    }

    /* Calculate the blurred weight for the point (A). For this we use a weighted average of the
     * point weights, based on the distance of the neighbour point to A. So points closer to A
     * contribute more to the average than points farther away from A. */
    float distance_sum = 0.0f;
    for (const int i : IndexRange(point_num)) {
      distance_sum += nearest_points[i].dist;
    }
    if (distance_sum == 0.0f) {
      return;
    }
    float blur_weight_sum = 0.0f;
    for (const int i : IndexRange(point_num)) {
      blur_weight_sum += (1.0f - nearest_points[i].dist / distance_sum) *
                         touched_points.weights[nearest_points[i].index];
    }
    const float blur_weight = blur_weight_sum / (point_num - 1);

    apply_weight_to_point(point, blur_weight, drawing_weight);
  }

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override
  {
    using namespace blender::ed::greasepencil;

    this->get_brush_settings(C, start_sample);
    this->ensure_active_vertex_group_in_object();
    this->get_locked_and_bone_deformed_vertex_groups();

    /* Get editable drawings grouped per frame number. When multiframe editing is disabled, this
     * is just one group for the current frame. When multiframe editing is enabled, the selected
     * keyframes are grouped per frame number. This way we can use Blur on multiple layers
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

    this->get_mouse_input_sample(extension_sample, 1.3f);

    /* Iterate over the drawings grouped per frame number. Collect all stroke points under the
     * brush and blur them. */
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

          /* Apply the Blur tool to all points in the brush buffer. */
          threading::parallel_for_each(drawing_weights, [&](DrawingWeightData &drawing_weight) {
            for (const BrushPoint &point : drawing_weight.points_in_brush) {
              this->apply_blur_tool(point, drawing_weight, touched_points);

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

std::unique_ptr<GreasePencilStrokeOperation> new_weight_paint_blur_operation()
{
  return std::make_unique<BlurWeightPaintOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
