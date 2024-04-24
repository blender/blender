/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::project {

struct ProjectOperation {
  gesture::Operation operation;
};

static void gesture_begin(bContext &C, gesture::GestureData &gesture_data)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  BKE_sculpt_update_object_for_edit(depsgraph, gesture_data.vc.obact, false);
}

static void apply_projection(gesture::GestureData &gesture_data, PBVHNode *node)
{
  PBVHVertexIter vd;
  bool any_updated = false;

  undo::push_node(*gesture_data.vc.obact, node, undo::Type::Position);

  BKE_pbvh_vertex_iter_begin (gesture_data.ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    float vertex_normal[3];
    const float *co = SCULPT_vertex_co_get(gesture_data.ss, vd.vertex);
    SCULPT_vertex_normal_get(gesture_data.ss, vd.vertex, vertex_normal);

    if (!gesture::is_affected(gesture_data, co, vertex_normal)) {
      continue;
    }

    float projected_pos[3];
    closest_to_plane_v3(projected_pos, gesture_data.line.plane, vd.co);

    float disp[3];
    sub_v3_v3v3(disp, projected_pos, vd.co);
    const float mask = vd.mask;
    mul_v3_fl(disp, 1.0f - mask);
    if (is_zero_v3(disp)) {
      continue;
    }
    add_v3_v3(vd.co, disp);
    any_updated = true;
  }
  BKE_pbvh_vertex_iter_end;

  if (any_updated) {
    BKE_pbvh_node_mark_update(node);
  }
}

static void gesture_apply_for_symmetry_pass(bContext & /*C*/, gesture::GestureData &gesture_data)
{
  switch (gesture_data.shape_type) {
    case gesture::ShapeType::Line:
      threading::parallel_for(gesture_data.nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          apply_projection(gesture_data, gesture_data.nodes[i]);
        }
      });
      break;
    case gesture::ShapeType::Lasso:
    case gesture::ShapeType::Box:
      /* Gesture shape projection not implemented yet. */
      BLI_assert(false);
      break;
  }
}

static void gesture_end(bContext &C, gesture::GestureData &gesture_data)
{
  SculptSession *ss = gesture_data.ss;
  Sculpt *sd = CTX_data_tool_settings(&C)->sculpt;
  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, gesture_data.vc.obact, true);
  }

  SCULPT_flush_update_step(&C, SCULPT_UPDATE_COORDS);
  SCULPT_flush_update_done(&C, gesture_data.vc.obact, SCULPT_UPDATE_COORDS);
}

static void init_operation(gesture::GestureData &gesture_data, wmOperator & /*op*/)
{
  gesture_data.operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<ProjectOperation>(__func__));

  ProjectOperation *project_operation = (ProjectOperation *)gesture_data.operation;

  project_operation->operation.begin = gesture_begin;
  project_operation->operation.apply_for_symmetry_pass = gesture_apply_for_symmetry_pass;
  project_operation->operation.end = gesture_end;
}

static int gesture_line_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  return WM_gesture_straightline_active_side_invoke(C, op, event);
}

static int gesture_line_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_line(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

void SCULPT_OT_project_line_gesture(wmOperatorType *ot)
{
  ot->name = "Project Line Gesture";
  ot->idname = "SCULPT_OT_project_line_gesture";
  ot->description = "Project the geometry onto a plane defined by a line";

  ot->invoke = gesture_line_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = gesture_line_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  gesture::operator_properties(ot, gesture::ShapeType::Line);
}

}  // namespace blender::ed::sculpt_paint::project
