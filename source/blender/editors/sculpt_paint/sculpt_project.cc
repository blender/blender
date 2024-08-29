/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_subdiv_ccg.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "mesh_brush_common.hh"
#include "sculpt_gesture.hh"
#include "sculpt_intern.hh"
#include "sculpt_undo.hh"

namespace blender::ed::sculpt_paint::project {

struct ProjectOperation {
  gesture::Operation operation;
};

static void gesture_begin(bContext &C, wmOperator &op, gesture::GestureData &gesture_data)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  BKE_sculpt_update_object_for_edit(depsgraph, gesture_data.vc.obact, false);
  undo::push_begin(*gesture_data.vc.obact, &op);
}

struct LocalData {
  Vector<float3> positions;
  Vector<float3> normals;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

static void apply_projection_mesh(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  const gesture::GestureData &gesture_data,
                                  const Span<float3> positions_eval,
                                  const Span<float3> vert_normals,
                                  const bke::pbvh::MeshNode &node,
                                  Object &object,
                                  LocalData &tls,
                                  const MutableSpan<float3> positions_orig)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);
  const MutableSpan positions = gather_data_mesh(positions_eval, verts, tls.positions);
  const MutableSpan normals = gather_data_mesh(vert_normals, verts, tls.normals);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);

  gesture::filter_factors(gesture_data, positions, normals, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, gesture_data.line.plane, translations);
  scale_translations(translations, factors);

  write_translations(depsgraph, sd, object, positions_eval, verts, translations, positions_orig);
}

static void apply_projection_grids(const Sculpt &sd,
                                   const gesture::GestureData &gesture_data,
                                   const bke::pbvh::GridsNode &node,
                                   Object &object,
                                   LocalData &tls)
{
  SculptSession &ss = *object.sculpt;

  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.normals.resize(positions.size());
  const MutableSpan<float3> normals = tls.normals;
  gather_grids_normals(subdiv_ccg, grids, normals);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);

  gesture::filter_factors(gesture_data, positions, normals, factors);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, gesture_data.line.plane, translations);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void apply_projection_bmesh(const Sculpt &sd,
                                   const gesture::GestureData &gesture_data,
                                   bke::pbvh::BMeshNode &node,
                                   Object &object,
                                   LocalData &tls)
{
  const SculptSession &ss = *object.sculpt;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.normals.resize(verts.size());
  const MutableSpan<float3> normals = tls.normals;
  gather_bmesh_normals(verts, normals);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);

  gesture::filter_factors(gesture_data, positions, normals, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, gesture_data.line.plane, translations);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

static void gesture_apply_for_symmetry_pass(bContext &C, gesture::GestureData &gesture_data)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  Object &object = *gesture_data.vc.obact;
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *ss.pbvh;
  const Sculpt &sd = *CTX_data_tool_settings(&C)->sculpt;
  const IndexMask &node_mask = gesture_data.node_mask;

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (gesture_data.shape_type) {
    case gesture::ShapeType::Line:
      switch (pbvh.type()) {
        case bke::pbvh::Type::Mesh: {
          MutableSpan<bke::pbvh::MeshNode> nodes = ss.pbvh->nodes<bke::pbvh::MeshNode>();
          Mesh &mesh = *static_cast<Mesh *>(object.data);
          const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
          const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
          MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();
          undo::push_nodes(depsgraph, object, node_mask, undo::Type::Position);
          threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
            LocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              apply_projection_mesh(depsgraph,
                                    sd,
                                    gesture_data,
                                    positions_eval,
                                    vert_normals,
                                    nodes[i],
                                    object,
                                    tls,
                                    positions_orig);
              BKE_pbvh_node_mark_positions_update(nodes[i]);
            });
          });
          break;
        }
        case bke::pbvh::Type::BMesh: {
          MutableSpan<bke::pbvh::GridsNode> nodes = ss.pbvh->nodes<bke::pbvh::GridsNode>();
          undo::push_nodes(depsgraph, object, node_mask, undo::Type::Position);
          threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
            LocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              apply_projection_grids(sd, gesture_data, nodes[i], object, tls);
              BKE_pbvh_node_mark_positions_update(nodes[i]);
            });
          });
          break;
        }
        case bke::pbvh::Type::Grids: {
          MutableSpan<bke::pbvh::BMeshNode> nodes = ss.pbvh->nodes<bke::pbvh::BMeshNode>();
          undo::push_nodes(depsgraph, object, node_mask, undo::Type::Position);
          threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
            LocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              apply_projection_bmesh(sd, gesture_data, nodes[i], object, tls);
              BKE_pbvh_node_mark_positions_update(nodes[i]);
            });
          });
          break;
        }
      }
      break;
    case gesture::ShapeType::Lasso:
    case gesture::ShapeType::Box:
      /* Gesture shape projection not implemented yet. */
      BLI_assert_unreachable();
      break;
  }
}

static void gesture_end(bContext &C, gesture::GestureData &gesture_data)
{
  flush_update_step(&C, UpdateType::Position);
  flush_update_done(&C, *gesture_data.vc.obact, UpdateType::Position);
  undo::push_end(*gesture_data.vc.obact);
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
