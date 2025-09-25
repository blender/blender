/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil_vertex_groups.hh"
#include "BKE_modifier.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "DNA_brush_types.h"
#include "DNA_meshdata_types.h"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

static constexpr float FIND_NEAREST_POINT_EPSILON = 1e-6f;
static constexpr int BLUR_NEIGHBOUR_NUM = 5;
static constexpr int SMEAR_NEIGHBOUR_NUM = 8;

class WeightPaintOperation : public GreasePencilStrokeOperation {
 public:
  struct BrushPoint {
    float influence;
    int drawing_point_index;
  };

  struct DrawingWeightData {
    int active_vertex_group;
    MutableSpan<MDeformVert> deform_verts;
    VMutableArray<float> deform_weights;
    float multi_frame_falloff;

    Vector<bool> locked_vgroups;
    Vector<bool> bone_deformed_vgroups;

    Array<float2> point_positions;

    /* A stroke point can be read-only in case of material locking. Read-only means that the
     * vertex weight can't be changed, but the weight does count for average, blur and smear. */
    Array<bool> point_is_read_only;

    /* Flag for all stroke points in a drawing: true when the point was touched by the brush during
     * a #GreasePencilStrokeOperation. */
    Array<bool> points_touched_by_brush;
    int points_touched_by_brush_num;

    /* Collected points under the brush in one #on_stroke_extended action. */
    Vector<BrushPoint> points_in_brush;
  };

  struct PointsTouchedByBrush {
    KDTree_2d *kdtree;
    Array<float> weights;
  };

  Object *object;
  GreasePencil *grease_pencil;
  Brush *brush;
  float initial_brush_radius;
  float brush_radius;
  float brush_radius_wide;
  float initial_brush_strength;
  float brush_strength;
  float brush_weight;
  float2 mouse_position;
  float2 mouse_position_previous;
  rctf brush_bbox;

  /* Flag for Auto-normalize weights of bone deformed vertex groups. */
  bool auto_normalize;
  /* Brush mode: normal, invert or smooth. */
  BrushStrokeMode stroke_mode;
  /* Add or subtract weight? */
  bool invert_brush_weight;
  /* Active vertex group in GP object. */
  bDeformGroup *object_defgroup;

  /* Weight paint data per editable drawing. Stored per frame group. */
  Array<Array<DrawingWeightData>> drawing_weight_data;

  /* Set of bone-deformed vertex groups (object level). */
  Set<std::string> object_bone_deformed_defgroups;
  /* Set of locked vertex groups (object level). */
  Set<std::string> object_locked_defgroups;

  ~WeightPaintOperation() override = default;

  /* Apply a weight to a point under the brush. */
  void apply_weight_to_point(const BrushPoint &point,
                             const float target_weight,
                             DrawingWeightData &drawing_weight)
  {
    /* Blend the current point weight with the target weight. */
    const float old_weight = drawing_weight.deform_weights[point.drawing_point_index];
    const float weight_delta = (this->invert_brush_weight ? (1.0f - target_weight) :
                                                            target_weight) -
                               old_weight;
    drawing_weight.deform_weights.set(
        point.drawing_point_index,
        math::clamp(
            old_weight + math::interpolate(0.0f, weight_delta, point.influence), 0.0f, 1.0f));
  }

  /* Get brush settings (radius, strength etc.) */
  void get_brush_settings(const bContext &C, const InputSample &start_sample)
  {
    using namespace blender::ed::greasepencil;

    this->object = CTX_data_active_object(&C);
    this->grease_pencil = static_cast<GreasePencil *>(this->object->data);
    Paint *paint = BKE_paint_get_active_from_context(&C);
    Brush *brush = BKE_paint_brush(paint);

    this->brush = brush;
    this->initial_brush_radius = BKE_brush_radius_get(paint, brush);
    this->initial_brush_strength = BKE_brush_alpha_get(paint, brush);
    this->brush_weight = BKE_brush_weight_get(paint, brush);
    this->mouse_position_previous = start_sample.mouse_position;
    this->invert_brush_weight = false;

    BKE_curvemapping_init(brush->curve_distance_falloff);

    /* Auto-normalize weights is only applied when the object is deformed by an armature. */
    const ToolSettings *ts = CTX_data_tool_settings(&C);
    this->auto_normalize = ts->auto_normalize &&
                           (BKE_modifiers_is_deformed_by_armature(this->object) != nullptr);
  }

  /* Get or create active vertex group in GP object. */
  void ensure_active_vertex_group_in_object()
  {
    int object_defgroup_nr = BKE_object_defgroup_active_index_get(this->object) - 1;
    if (object_defgroup_nr == -1) {
      const ListBase *defbase = BKE_object_defgroup_list(this->object);
      if (const Object *modob = BKE_modifiers_is_deformed_by_armature(this->object)) {
        /* This happens on a Bone select, when no vgroup existed yet. */
        const Bone *actbone = static_cast<bArmature *>(modob->data)->act_bone;
        if (actbone) {
          const bPoseChannel *pchan = BKE_pose_channel_find_name(modob->pose, actbone->name);

          if (pchan) {
            bDeformGroup *dg = BKE_object_defgroup_find_name(this->object, pchan->name);
            if (dg == nullptr) {
              dg = BKE_object_defgroup_add_name(this->object, pchan->name);
              object_defgroup_nr = BLI_findindex(defbase, dg);
            }
            else {
              const int actdef = BLI_findindex(defbase, dg);
              BLI_assert(actdef >= 0);
              this->grease_pencil->vertex_group_active_index = actdef + 1;
              object_defgroup_nr = actdef;
            }
          }
        }
      }
      if (BLI_listbase_is_empty(defbase)) {
        BKE_object_defgroup_add(this->object);
        object_defgroup_nr = 0;
      }
    }
    this->object_defgroup = static_cast<bDeformGroup *>(
        BLI_findlink(BKE_object_defgroup_list(this->object), object_defgroup_nr));
  }

  /* Get locked and bone-deformed vertex groups in GP object. */
  void get_locked_and_bone_deformed_vertex_groups()
  {
    const ListBase *defgroups = BKE_object_defgroup_list(this->object);
    LISTBASE_FOREACH (bDeformGroup *, dg, defgroups) {
      if ((dg->flag & DG_LOCK_WEIGHT) != 0) {
        this->object_locked_defgroups.add(dg->name);
      }
    }
    this->object_bone_deformed_defgroups = ed::greasepencil::get_bone_deformed_vertex_group_names(
        *this->object);
  }

  /* For each drawing, retrieve pointers to the vertex weight data of the active vertex group,
   * so that we can read and write to them later. And create buffers for points under the brush
   * during one #on_stroke_extended action. */
  void init_weight_data_for_drawings(const bContext &C,
                                     const Span<ed::greasepencil::MutableDrawingInfo> &drawings,
                                     const int frame_group)
  {
    const Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
    const Object *ob_eval = DEG_get_evaluated(depsgraph, this->object);
    const RegionView3D *rv3d = CTX_wm_region_view3d(&C);
    const ARegion *region = CTX_wm_region(&C);

    this->drawing_weight_data[frame_group].reinitialize(drawings.size());

    threading::parallel_for(drawings.index_range(), 1, [&](const IndexRange range) {
      for (const int drawing_index : range) {
        const ed::greasepencil::MutableDrawingInfo &drawing_info = drawings[drawing_index];
        bke::CurvesGeometry &curves = drawing_info.drawing.strokes_for_write();

        /* Find or create the active vertex group in the drawing. */
        DrawingWeightData &drawing_weight_data =
            this->drawing_weight_data[frame_group][drawing_index];
        drawing_weight_data.active_vertex_group = bke::greasepencil::ensure_vertex_group(
            this->object_defgroup->name, curves.vertex_group_names);

        drawing_weight_data.multi_frame_falloff = drawing_info.multi_frame_falloff;
        drawing_weight_data.deform_verts = curves.deform_verts_for_write();
        drawing_weight_data.deform_weights = bke::varray_for_mutable_deform_verts(
            drawing_weight_data.deform_verts, drawing_weight_data.active_vertex_group);

        /* Create boolean arrays indicating whether a vertex group is locked/bone deformed
         * or not. */
        if (this->auto_normalize) {
          LISTBASE_FOREACH (bDeformGroup *, dg, &curves.vertex_group_names) {
            drawing_weight_data.locked_vgroups.append(
                this->object_locked_defgroups.contains(dg->name));
            drawing_weight_data.bone_deformed_vgroups.append(
                this->object_bone_deformed_defgroups.contains(dg->name));
          }
        }

        /* Convert stroke points to screen space positions. */
        const bke::greasepencil::Layer &layer = this->grease_pencil->layer(
            drawing_info.layer_index);
        const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
        const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);

        bke::crazyspace::GeometryDeformation deformation =
            bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
                ob_eval, *this->object, drawing_info.drawing);
        drawing_weight_data.point_positions.reinitialize(deformation.positions.size());
        threading::parallel_for(curves.points_range(), 1024, [&](const IndexRange point_range) {
          for (const int point : point_range) {
            drawing_weight_data.point_positions[point] = ED_view3d_project_float_v2_m4(
                region, deformation.positions[point], projection);
          }
        });

        /* Get the read-only state of stroke points (can be true in case of material locking). */
        drawing_weight_data.point_is_read_only.reinitialize(deformation.positions.size());
        drawing_weight_data.point_is_read_only.fill(true);
        IndexMaskMemory memory;
        const IndexMask editable_points = ed::greasepencil::retrieve_editable_points(
            *this->object, drawing_info.drawing, drawing_info.layer_index, memory);
        editable_points.foreach_index(GrainSize(1024), [&](const int64_t index) {
          drawing_weight_data.point_is_read_only[index] = false;
        });

        /* Initialize the flag for stroke points being touched by the brush. */
        drawing_weight_data.points_touched_by_brush_num = 0;
        drawing_weight_data.points_touched_by_brush = Array<bool>(deformation.positions.size(),
                                                                  false);
      }
    });
  }

  /* Get mouse position and pressure. */
  void get_mouse_input_sample(const InputSample &input_sample,
                              const float brush_widen_factor = 1.0f)
  {
    this->mouse_position = input_sample.mouse_position;
    this->brush_radius = this->initial_brush_radius;
    if (BKE_brush_use_size_pressure(this->brush)) {
      this->brush_radius *= input_sample.pressure;
    }
    this->brush_strength = this->initial_brush_strength;
    if (BKE_brush_use_alpha_pressure(this->brush)) {
      this->brush_strength *= input_sample.pressure;
    }
    this->brush_radius_wide = this->brush_radius * brush_widen_factor;

    BLI_rctf_init(&this->brush_bbox,
                  this->mouse_position.x - this->brush_radius_wide,
                  this->mouse_position.x + this->brush_radius_wide,
                  this->mouse_position.y - this->brush_radius_wide,
                  this->mouse_position.y + this->brush_radius_wide);
  }

  /* Add a point to the brush buffer when it is within the brush radius. */
  void add_point_under_brush_to_brush_buffer(const float2 point_position,
                                             DrawingWeightData &drawing_weight,
                                             const int point_index)
  {
    if (!BLI_rctf_isect_pt_v(&this->brush_bbox, point_position)) {
      return;
    }
    const float dist_point_to_brush_center = math::distance(point_position, this->mouse_position);
    if (dist_point_to_brush_center > this->brush_radius_wide) {
      return;
    }

    /* Point is touched by the (wide) brush, set flag for that. */
    if (!drawing_weight.points_touched_by_brush[point_index]) {
      drawing_weight.points_touched_by_brush_num++;
    }
    drawing_weight.points_touched_by_brush[point_index] = true;

    if (dist_point_to_brush_center > this->brush_radius) {
      return;
    }

    /* When the point is under the brush, add it to the brush buffer. */
    const float influence = drawing_weight.multi_frame_falloff * this->brush_strength *
                            BKE_brush_curve_strength(
                                this->brush, dist_point_to_brush_center, this->brush_radius);
    if (influence != 0.0f) {
      drawing_weight.points_in_brush.append({influence, point_index});
    }
  }

  /* Create KDTree for all stroke points touched by the brush during a weight paint operation. */
  PointsTouchedByBrush create_affected_points_kdtree(const Span<DrawingWeightData> drawing_weights)
  {
    /* Get number of stroke points touched by the brush. */
    int point_num = 0;
    for (const DrawingWeightData &drawing_weight : drawing_weights) {
      point_num += drawing_weight.points_touched_by_brush_num;
    }

    /* Create KDTree of stroke points touched by the brush. */
    KDTree_2d *touched_points = BLI_kdtree_2d_new(point_num);
    Array<float> touched_points_weights(point_num);
    int kdtree_index = 0;
    for (const DrawingWeightData &drawing_weight : drawing_weights) {
      for (const int point_index : drawing_weight.point_positions.index_range()) {
        if (drawing_weight.points_touched_by_brush[point_index]) {
          BLI_kdtree_2d_insert(
              touched_points, kdtree_index, drawing_weight.point_positions[point_index]);
          touched_points_weights[kdtree_index] = drawing_weight.deform_weights[point_index];
          kdtree_index++;
        }
      }
    }
    BLI_kdtree_2d_balance(touched_points);

    return {touched_points, touched_points_weights};
  }
};

}  // namespace blender::ed::sculpt_paint::greasepencil
