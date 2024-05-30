/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode tools.
 */

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mirror.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"

#include "IMB_colormanagement.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>
#include <cstring>

/* Reset the copy of the mesh that is being sculpted on (currently just for the layer brush). */

static int sculpt_set_persistent_base_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession *ss = ob.sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  /* Do not allow in DynTopo just yet. */
  if (!ss || (ss && ss->bm)) {
    return OPERATOR_FINISHED;
  }
  SCULPT_vertex_random_access_ensure(*ss);
  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  SculptAttributeParams params = {0};
  params.permanent = true;

  ss->attrs.persistent_co = BKE_sculpt_attribute_ensure(
      &ob, bke::AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_co), &params);
  ss->attrs.persistent_no = BKE_sculpt_attribute_ensure(
      &ob, bke::AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_no), &params);
  ss->attrs.persistent_disp = BKE_sculpt_attribute_ensure(
      &ob, bke::AttrDomain::Point, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(persistent_disp), &params);

  const int totvert = SCULPT_vertex_count_get(*ss);

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(*ss->pbvh, i);

    copy_v3_v3((float *)SCULPT_vertex_attr_get(vertex, ss->attrs.persistent_co),
               SCULPT_vertex_co_get(*ss, vertex));
    *(float3 *)SCULPT_vertex_attr_get(vertex, ss->attrs.persistent_no) = SCULPT_vertex_normal_get(
        *ss, vertex);
    (*(float *)SCULPT_vertex_attr_get(vertex, ss->attrs.persistent_disp)) = 0.0f;
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Persistent Base";
  ot->idname = "SCULPT_OT_set_persistent_base";
  ot->description = "Reset the copy of the mesh that is being sculpted on";

  /* API callbacks. */
  ot->exec = sculpt_set_persistent_base_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************* SCULPT_OT_optimize *************************/

static int sculpt_optimize_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);

  SCULPT_pbvh_clear(*ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

/* The BVH gets less optimal more quickly with dynamic topology than
 * regular sculpting. There is no doubt more clever stuff we can do to
 * optimize it on the fly, but for now this gives the user a nicer way
 * to recalculate it than toggling modes. */
static void SCULPT_OT_optimize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Rebuild BVH";
  ot->idname = "SCULPT_OT_optimize";
  ot->description = "Recalculate the sculpt BVH to improve performance";

  /* API callbacks. */
  ot->exec = sculpt_optimize_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* Dynamic topology symmetrize ********************/

static bool sculpt_no_multires_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (SCULPT_mode_poll(C) && ob->sculpt && ob->sculpt->pbvh) {
    return BKE_pbvh_type(*ob->sculpt->pbvh) != PBVH_GRIDS;
  }
  return false;
}

static int sculpt_symmetrize_exec(bContext *C, wmOperator *op)
{
  using namespace blender::ed::sculpt_paint;
  Main *bmain = CTX_data_main(C);
  Object &ob = *CTX_data_active_object(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  SculptSession &ss = *ob.sculpt;
  PBVH *pbvh = ss.pbvh.get();
  const float dist = RNA_float_get(op->ptr, "merge_tolerance");

  if (!pbvh) {
    return OPERATOR_CANCELLED;
  }

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  switch (BKE_pbvh_type(*pbvh)) {
    case PBVH_BMESH: {
      /* Dyntopo Symmetrize. */

      /* To simplify undo for symmetrize, all BMesh elements are logged
       * as deleted, then after symmetrize operation all BMesh elements
       * are logged as added (as opposed to attempting to store just the
       * parts that symmetrize modifies). */
      undo::push_begin(ob, op);
      undo::push_node(ob, nullptr, undo::Type::DyntopoSymmetrize);
      BM_log_before_all_removed(ss.bm, ss.bm_log);

      BM_mesh_toolflags_set(ss.bm, true);

      /* Symmetrize and re-triangulate. */
      BMO_op_callf(ss.bm,
                   (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                   "symmetrize input=%avef direction=%i dist=%f use_shapekey=%b",
                   sd.symmetrize_direction,
                   dist,
                   true);
      dyntopo::triangulate(ss.bm);

      /* Bisect operator flags edges (keep tags clean for edge queue). */
      BM_mesh_elem_hflag_disable_all(ss.bm, BM_EDGE, BM_ELEM_TAG, false);

      BM_mesh_toolflags_set(ss.bm, false);

      /* Finish undo. */
      BM_log_all_added(ss.bm, ss.bm_log);
      undo::push_end(ob);

      break;
    }
    case PBVH_FACES: {
      /* Mesh Symmetrize. */
      undo::geometry_begin(ob, op);
      Mesh *mesh = static_cast<Mesh *>(ob.data);

      BKE_mesh_mirror_apply_mirror_on_axis(bmain, mesh, sd.symmetrize_direction, dist);

      undo::geometry_end(ob);
      BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);

      break;
    }
    case PBVH_GRIDS:
      return OPERATOR_CANCELLED;
  }

  SCULPT_topology_islands_invalidate(ss);

  /* Redraw. */
  SCULPT_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_symmetrize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Symmetrize";
  ot->idname = "SCULPT_OT_symmetrize";
  ot->description = "Symmetrize the topology modifications";

  /* API callbacks. */
  ot->exec = sculpt_symmetrize_exec;
  ot->poll = sculpt_no_multires_poll;

  PropertyRNA *prop = RNA_def_float(ot->srna,
                                    "merge_tolerance",
                                    0.0005f,
                                    0.0f,
                                    FLT_MAX,
                                    "Merge Distance",
                                    "Distance within which symmetrical vertices are merged",
                                    0.0f,
                                    1.0f);

  RNA_def_property_ui_range(prop, 0.0, FLT_MAX, 0.001, 5);
}

/**** Toggle operator for turning sculpt mode on or off ****/

namespace blender::ed::sculpt_paint {

static void sculpt_init_session(Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob)
{
  /* Create persistent sculpt mode data. */
  BKE_sculpt_toolsettings_data_ensure(&bmain, &scene);

  /* Create sculpt mode session data. */
  if (ob.sculpt != nullptr) {
    BKE_sculptsession_free(&ob);
  }
  ob.sculpt = MEM_new<SculptSession>(__func__);
  ob.sculpt->mode_type = OB_MODE_SCULPT;

  /* Trigger evaluation of modifier stack to ensure
   * multires modifier sets .runtime.ccg in
   * the evaluated mesh.
   */
  DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);

  BKE_scene_graph_evaluated_ensure(&depsgraph, &bmain);

  /* This function expects a fully evaluated depsgraph. */
  BKE_sculpt_update_object_for_edit(&depsgraph, &ob, false);

  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  if (mesh.attributes().contains(".sculpt_face_set")) {
    /* Here we can detect geometry that was just added to Sculpt Mode as it has the
     * SCULPT_FACE_SET_NONE assigned, so we can create a new Face Set for it. */
    /* In sculpt mode all geometry that is assigned to SCULPT_FACE_SET_NONE is considered as not
     * initialized, which is used is some operators that modify the mesh topology to perform
     * certain actions in the new faces. After these operations are finished, all faces should have
     * a valid face set ID assigned (different from SCULPT_FACE_SET_NONE) to manage their
     * visibility correctly. */
    /* TODO(pablodp606): Based on this we can improve the UX in future tools for creating new
     * objects, like moving the transform pivot position to the new area or masking existing
     * geometry. */
    const int new_face_set = face_set::find_next_available_id(ob);
    face_set::initialize_none_to_id(static_cast<Mesh *>(ob.data), new_face_set);
  }
}

void ensure_valid_pivot(const Object &ob, Scene &scene)
{
  UnifiedPaintSettings &ups = scene.toolsettings->unified_paint_settings;
  const SculptSession &ss = *ob.sculpt;

  /* Account for the case where no objects are evaluated. */
  if (!ss.pbvh) {
    return;
  }

  /* No valid pivot? Use bounding box center. */
  if (ups.average_stroke_counter == 0 || !ups.last_stroke_valid) {
    const Bounds<float3> bounds = bke::pbvh::bounds_get(*ob.sculpt->pbvh);
    const float3 center = math::midpoint(bounds.min, bounds.max);
    const float3 location = math::transform_point(ob.object_to_world(), center);

    copy_v3_v3(ups.average_stroke_accum, location);
    ups.average_stroke_counter = 1;

    /* Update last stroke position. */
    ups.last_stroke_valid = true;
  }
}

}  // namespace blender::ed::sculpt_paint

void ED_object_sculptmode_enter_ex(Main &bmain,
                                   Depsgraph &depsgraph,
                                   Scene &scene,
                                   Object &ob,
                                   const bool force_dyntopo,
                                   ReportList *reports)
{
  using namespace blender::ed::sculpt_paint;
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *mesh = BKE_mesh_from_object(&ob);

  /* Enter sculpt mode. */
  ob.mode |= mode_flag;

  sculpt_init_session(bmain, depsgraph, scene, ob);

  if (!(fabsf(ob.scale[0] - ob.scale[1]) < 1e-4f && fabsf(ob.scale[1] - ob.scale[2]) < 1e-4f)) {
    BKE_report(
        reports, RPT_WARNING, "Object has non-uniform scale, sculpting may be unpredictable");
  }
  else if (is_negative_m4(ob.object_to_world().ptr())) {
    BKE_report(reports, RPT_WARNING, "Object has negative scale, sculpting may be unpredictable");
  }

  Paint *paint = BKE_paint_get_active_from_paintmode(&scene, PaintMode::Sculpt);
  BKE_paint_init(&bmain, &scene, PaintMode::Sculpt, PAINT_CURSOR_SCULPT);

  ED_paint_cursor_start(paint, SCULPT_mode_poll_view3d);

  /* Check dynamic-topology flag; re-enter dynamic-topology mode when changing modes,
   * As long as no data was added that is not supported. */
  if (mesh->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    MultiresModifierData *mmd = BKE_sculpt_multires_active(&scene, &ob);

    const char *message_unsupported = nullptr;
    if (mesh->corners_num != mesh->faces_num * 3) {
      message_unsupported = RPT_("non-triangle face");
    }
    else if (mmd != nullptr) {
      message_unsupported = RPT_("multi-res modifier");
    }
    else {
      dyntopo::WarnFlag flag = dyntopo::check_attribute_warning(scene, ob);
      if (flag == 0) {
        /* pass */
      }
      else if (flag & dyntopo::VDATA) {
        message_unsupported = RPT_("vertex data");
      }
      else if (flag & dyntopo::EDATA) {
        message_unsupported = RPT_("edge data");
      }
      else if (flag & dyntopo::LDATA) {
        message_unsupported = RPT_("face data");
      }
      else if (flag & dyntopo::MODIFIER) {
        message_unsupported = RPT_("constructive modifier");
      }
      else {
        BLI_assert(0);
      }
    }

    if ((message_unsupported == nullptr) || force_dyntopo) {
      /* Needed because we may be entering this mode before the undo system loads. */
      wmWindowManager *wm = static_cast<wmWindowManager *>(bmain.wm.first);
      bool has_undo = wm->undo_stack != nullptr;
      /* Undo push is needed to prevent memory leak. */
      if (has_undo) {
        undo::push_begin_ex(ob, "Dynamic topology enable");
      }
      dyntopo::enable_ex(bmain, depsgraph, ob);
      if (has_undo) {
        undo::push_node(ob, nullptr, undo::Type::DyntopoBegin);
        undo::push_end(ob);
      }
    }
    else {
      BKE_reportf(
          reports, RPT_WARNING, "Dynamic Topology found: %s, disabled", message_unsupported);
      mesh->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
    }
  }

  ensure_valid_pivot(ob, scene);

  /* Flush object mode. */
  DEG_id_tag_update(&ob.id, ID_RECALC_SYNC_TO_EVAL);
}

void ED_object_sculptmode_enter(bContext *C, Depsgraph &depsgraph, ReportList *reports)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);
  ViewLayer &view_layer = *CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(&scene, &view_layer);
  Object &ob = *BKE_view_layer_active_object_get(&view_layer);
  ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, reports);
}

void ED_object_sculptmode_exit_ex(Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob)
{
  using namespace blender::ed::sculpt_paint;
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *mesh = BKE_mesh_from_object(&ob);

  multires_flush_sculpt_updates(&ob);

  /* Not needed for now. */
#if 0
  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
  const int flush_recalc = ed_object_sculptmode_flush_recalc_flag(scene, ob, mmd);
#endif

  /* Always for now, so leaving sculpt mode always ensures scene is in
   * a consistent state. */
  if (true || /* flush_recalc || */ (ob.sculpt && ob.sculpt->bm)) {
    DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
  }

  if (mesh->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    /* Dynamic topology must be disabled before exiting sculpt
     * mode to ensure the undo stack stays in a consistent
     * state. */
    dyntopo::disable_with_undo(bmain, depsgraph, scene, ob);

    /* Store so we know to re-enable when entering sculpt mode. */
    mesh->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
  }

  /* Leave sculpt mode. */
  ob.mode &= ~mode_flag;

  BKE_sculptsession_free(&ob);

  paint_cursor_delete_textures();

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(&ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob.id, ID_RECALC_SYNC_TO_EVAL);
}

void ED_object_sculptmode_exit(bContext *C, Depsgraph &depsgraph)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);
  ViewLayer &view_layer = *CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(&scene, &view_layer);
  Object &ob = *BKE_view_layer_active_object_get(&view_layer);
  ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
}

namespace blender::ed::sculpt_paint {

static int sculpt_mode_toggle_exec(bContext *C, wmOperator *op)
{
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main &bmain = *CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
  Scene &scene = *CTX_data_scene(C);
  ToolSettings &ts = *scene.toolsettings;
  ViewLayer &view_layer = *CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(&scene, &view_layer);
  Object &ob = *BKE_view_layer_active_object_get(&view_layer);
  const int mode_flag = OB_MODE_SCULPT;
  const bool is_mode_set = (ob.mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!object::mode_compat_set(C, &ob, eObjectMode(mode_flag), op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    ED_object_sculptmode_exit_ex(bmain, *depsgraph, scene, ob);
  }
  else {
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_sculptmode_enter_ex(bmain, *depsgraph, scene, ob, false, op->reports);
    BKE_paint_brush_validate(&bmain, &ts.sculpt->paint);

    if (ob.mode & mode_flag) {
      Mesh *mesh = static_cast<Mesh *>(ob.data);
      /* Dyntopo adds its own undo step. */
      if ((mesh->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) == 0) {
        /* Without this the memfile undo step is used,
         * while it works it causes lag when undoing the first undo step, see #71564. */
        wmWindowManager *wm = CTX_wm_manager(C);
        if (wm->op_undo_depth <= 1) {
          undo::push_begin(ob, op);
          undo::push_end(ob);
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, &scene);

  WM_msg_publish_rna_prop(mbus, &ob.id, &ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sculpt Mode";
  ot->idname = "SCULPT_OT_sculptmode_toggle";
  ot->description = "Toggle sculpt mode in 3D view";

  /* API callbacks. */
  ot->exec = sculpt_mode_toggle_exec;
  ot->poll = ED_operator_object_active_editable_mesh;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::sculpt_paint

void SCULPT_geometry_preview_lines_update(bContext *C, SculptSession &ss, float radius)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);

  ss.preview_vert_count = 0;
  int totpoints = 0;

  /* This function is called from the cursor drawing code, so the PBVH may not be build yet. */
  if (!ss.pbvh) {
    return;
  }

  if (!ss.deform_modifiers_active) {
    return;
  }

  if (BKE_pbvh_type(*ss.pbvh) == PBVH_GRIDS) {
    return;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  float brush_co[3];
  copy_v3_v3(brush_co, SCULPT_active_vertex_co_get(ss));

  blender::BitVector<> visited_verts(SCULPT_vertex_count_get(ss));

  /* Assuming an average of 6 edges per vertex in a triangulated mesh. */
  const int max_preview_verts = SCULPT_vertex_count_get(ss) * 3 * 2;

  if (ss.preview_vert_list == nullptr) {
    ss.preview_vert_list = MEM_cnew_array<PBVHVertRef>(max_preview_verts, __func__);
  }

  std::queue<PBVHVertRef> non_visited_verts;
  non_visited_verts.push(SCULPT_active_vertex_get(ss));

  while (!non_visited_verts.empty()) {
    PBVHVertRef from_v = non_visited_verts.front();
    non_visited_verts.pop();

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, from_v, ni) {
      if (totpoints + (ni.neighbors.size() * 2) < max_preview_verts) {
        PBVHVertRef to_v = ni.vertex;
        int to_v_i = ni.index;
        ss.preview_vert_list[totpoints] = from_v;
        totpoints++;
        ss.preview_vert_list[totpoints] = to_v;
        totpoints++;
        if (visited_verts[to_v_i]) {
          continue;
        }
        visited_verts[to_v_i].set();
        const float *co = SCULPT_vertex_co_for_grab_active_get(ss, to_v);
        if (len_squared_v3v3(brush_co, co) < radius * radius) {
          non_visited_verts.push(to_v);
        }
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  ss.preview_vert_count = totpoints;
}

static int sculpt_sample_color_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  Brush &brush = *BKE_paint_brush(&sd.paint);
  SculptSession &ss = *ob.sculpt;
  PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);
  blender::float4 active_vertex_color;

  if (!SCULPT_handles_colors_report(ss, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(CTX_data_depsgraph_pointer(C), &ob, false);

  /* No color attribute? Set color to white. */
  if (!SCULPT_has_colors(ss)) {
    copy_v4_fl(active_vertex_color, 1.0f);
  }
  else {
    active_vertex_color = SCULPT_vertex_color_get(ss, active_vertex);
  }

  float color_srgb[3];
  IMB_colormanagement_scene_linear_to_srgb_v3(color_srgb, active_vertex_color);
  BKE_brush_color_set(&scene, &brush, color_srgb);

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, &brush);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_sample_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Color";
  ot->idname = "SCULPT_OT_sample_color";
  ot->description = "Sample the vertex color of the active vertex";

  /* api callbacks */
  ot->invoke = sculpt_sample_color_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;
}

namespace blender::ed::sculpt_paint::mask {

/**
 * #sculpt_mask_by_color_delta_get returns values in the (0,1) range that are used to generate the
 * mask based on the difference between two colors (the active color and the color of any other
 * vertex). Ideally, a threshold of 0 should mask only the colors that are equal to the active
 * color and threshold of 1 should mask all colors. In order to avoid artifacts and produce softer
 * falloffs in the mask, the MASK_BY_COLOR_SLOPE defines the size of the transition values between
 * masked and unmasked vertices. The smaller this value is, the sharper the generated mask is going
 * to be.
 */
#define MASK_BY_COLOR_SLOPE 0.25f

static float sculpt_mask_by_color_delta_get(const float *color_a,
                                            const float *color_b,
                                            const float threshold,
                                            const bool invert)
{
  float len = len_v3v3(color_a, color_b);
  /* Normalize len to the (0, 1) range. */
  len = len / M_SQRT3;

  if (len < threshold - MASK_BY_COLOR_SLOPE) {
    len = 1.0f;
  }
  else if (len >= threshold) {
    len = 0.0f;
  }
  else {
    len = (-len + threshold) / MASK_BY_COLOR_SLOPE;
  }

  if (invert) {
    return 1.0f - len;
  }
  return len;
}

static float sculpt_mask_by_color_final_mask_get(const float current_mask,
                                                 const float new_mask,
                                                 const bool invert,
                                                 const bool preserve_mask)
{
  if (preserve_mask) {
    if (invert) {
      return min_ff(current_mask, new_mask);
    }
    return max_ff(current_mask, new_mask);
  }
  return new_mask;
}

struct MaskByColorContiguousFloodFillData {
  float threshold;
  bool invert;
  float *new_mask;
  float initial_color[3];
};

static void do_mask_by_color_contiguous_update_node(Object &ob,
                                                    const float *mask_by_color_floodfill,
                                                    const bool invert,
                                                    const bool preserve_mask,
                                                    const SculptMaskWriteInfo mask_write,
                                                    PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  undo::push_node(ob, node, undo::Type::Mask);
  bool update_node = false;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    const float current_mask = vd.mask;
    const float new_mask = mask_by_color_floodfill[vd.index];
    const float mask = sculpt_mask_by_color_final_mask_get(
        current_mask, new_mask, invert, preserve_mask);
    if (current_mask == mask) {
      continue;
    }

    SCULPT_mask_vert_set(BKE_pbvh_type(*ss.pbvh), mask_write, mask, vd);

    update_node = true;
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static bool sculpt_mask_by_color_contiguous_floodfill(SculptSession &ss,
                                                      PBVHVertRef from_v,
                                                      PBVHVertRef to_v,
                                                      bool is_duplicate,
                                                      MaskByColorContiguousFloodFillData *data)
{
  int from_v_i = BKE_pbvh_vertex_to_index(*ss.pbvh, from_v);
  int to_v_i = BKE_pbvh_vertex_to_index(*ss.pbvh, to_v);

  float4 current_color = SCULPT_vertex_color_get(ss, to_v);

  float new_vertex_mask = sculpt_mask_by_color_delta_get(
      current_color, data->initial_color, data->threshold, data->invert);
  data->new_mask[to_v_i] = new_vertex_mask;

  if (is_duplicate) {
    data->new_mask[to_v_i] = data->new_mask[from_v_i];
  }

  float len = len_v3v3(current_color, data->initial_color);
  len = len / M_SQRT3;
  return len <= data->threshold;
}

static void sculpt_mask_by_color_contiguous(Object &object,
                                            const PBVHVertRef vertex,
                                            const float threshold,
                                            const bool invert,
                                            const bool preserve_mask)
{
  using namespace blender;
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *object.sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *new_mask = MEM_cnew_array<float>(totvert, __func__);

  if (invert) {
    for (int i = 0; i < totvert; i++) {
      new_mask[i] = 1.0f;
    }
  }

  flood_fill::FillData flood = flood_fill::init_fill(ss);
  flood_fill::add_initial(flood, vertex);

  MaskByColorContiguousFloodFillData ffd;
  ffd.threshold = threshold;
  ffd.invert = invert;
  ffd.new_mask = new_mask;

  float4 color = SCULPT_vertex_color_get(ss, vertex);

  copy_v3_v3(ffd.initial_color, color);

  flood_fill::execute(ss, flood, [&](PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate) {
    return sculpt_mask_by_color_contiguous_floodfill(ss, from_v, to_v, is_duplicate, &ffd);
  });

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(*ss.pbvh, {});
  const SculptMaskWriteInfo mask_write = SCULPT_mask_get_for_write(ss);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_mask_by_color_contiguous_update_node(
          object, new_mask, invert, preserve_mask, mask_write, nodes[i]);
    }
  });

  MEM_freeN(new_mask);
}

static void do_mask_by_color_task(Object &ob,
                                  const float threshold,
                                  const bool invert,
                                  const bool preserve_mask,
                                  const PBVHVertRef mask_by_color_vertex,
                                  const SculptMaskWriteInfo mask_write,
                                  PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  undo::push_node(ob, node, undo::Type::Mask);
  bool update_node = false;

  float4 active_color = SCULPT_vertex_color_get(ss, mask_by_color_vertex);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    float4 col = SCULPT_vertex_color_get(ss, vd.vertex);

    const float current_mask = vd.mask;
    const float new_mask = sculpt_mask_by_color_delta_get(active_color, col, threshold, invert);
    const float mask = sculpt_mask_by_color_final_mask_get(
        current_mask, new_mask, invert, preserve_mask);
    if (current_mask == mask) {
      continue;
    }

    SCULPT_mask_vert_set(BKE_pbvh_type(*ss.pbvh), mask_write, mask, vd);

    update_node = true;
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static void sculpt_mask_by_color_full_mesh(Object &object,
                                           const PBVHVertRef vertex,
                                           const float threshold,
                                           const bool invert,
                                           const bool preserve_mask)
{
  using namespace blender;
  SculptSession &ss = *object.sculpt;

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(*ss.pbvh, {});
  const SculptMaskWriteInfo mask_write = SCULPT_mask_get_for_write(ss);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_mask_by_color_task(
          object, threshold, invert, preserve_mask, vertex, mask_write, nodes[i]);
    }
  });
}

static int sculpt_mask_by_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  using namespace blender::ed::sculpt_paint;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  View3D *v3d = CTX_wm_view3d(C);

  {
    if (v3d && v3d->shading.type == OB_SOLID) {
      v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
    }
  }

  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  /* Color data is not available in multi-resolution or dynamic topology. */
  if (!SCULPT_handles_colors_report(ss, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  MultiresModifierData *mmd = BKE_sculpt_multires_active(CTX_data_scene(C), &ob);
  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), &ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);
  SCULPT_vertex_random_access_ensure(ss);

  /* Tools that are not brushes do not have the brush gizmo to update the vertex as the mouse move,
   * so it needs to be updated here. */
  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);

  undo::push_begin(ob, op);
  BKE_sculpt_color_layer_create_if_needed(&ob);

  const PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool invert = RNA_boolean_get(op->ptr, "invert");
  const bool preserve_mask = RNA_boolean_get(op->ptr, "preserve_previous_mask");

  if (SCULPT_has_loop_colors(ob)) {
    BKE_pbvh_ensure_node_loops(*ss.pbvh);
  }

  if (RNA_boolean_get(op->ptr, "contiguous")) {
    sculpt_mask_by_color_contiguous(ob, active_vertex, threshold, invert, preserve_mask);
  }
  else {
    sculpt_mask_by_color_full_mesh(ob, active_vertex, threshold, invert, preserve_mask);
  }

  bke::pbvh::update_mask(*ss.pbvh);
  undo::push_end(ob);

  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_mask_by_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask by Color";
  ot->idname = "SCULPT_OT_mask_by_color";
  ot->description = "Creates a mask based on the active color attribute";

  /* api callbacks */
  ot->invoke = sculpt_mask_by_color_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  ot->prop = RNA_def_boolean(
      ot->srna, "contiguous", false, "Contiguous", "Mask only contiguous color areas");

  ot->prop = RNA_def_boolean(ot->srna, "invert", false, "Invert", "Invert the generated mask");
  ot->prop = RNA_def_boolean(
      ot->srna,
      "preserve_previous_mask",
      false,
      "Preserve Previous Mask",
      "Preserve the previous mask and add or subtract the new one generated by the colors");

  RNA_def_float(ot->srna,
                "threshold",
                0.35f,
                0.0f,
                1.0f,
                "Threshold",
                "How much changes in color affect the mask generation",
                0.0f,
                1.0f);
}

}  // namespace blender::ed::sculpt_paint::mask

enum CavityBakeMixMode {
  AUTOMASK_BAKE_MIX,
  AUTOMASK_BAKE_MULTIPLY,
  AUTOMASK_BAKE_DIVIDE,
  AUTOMASK_BAKE_ADD,
  AUTOMASK_BAKE_SUBTRACT,
};

enum CavityBakeSettingsSource {
  AUTOMASK_SETTINGS_OPERATOR,
  AUTOMASK_SETTINGS_SCENE,
  AUTOMASK_SETTINGS_BRUSH
};

static void sculpt_bake_cavity_exec_task(Object &ob,
                                         blender::ed::sculpt_paint::auto_mask::Cache &automasking,
                                         const CavityBakeMixMode mode,
                                         const float factor,
                                         const SculptMaskWriteInfo mask_write,
                                         PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  PBVHVertexIter vd;

  undo::push_node(ob, node, undo::Type::Mask);

  auto_mask::NodeData automask_data = auto_mask::node_begin(ob, &automasking, *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    auto_mask::node_update(automask_data, vd);

    float automask = auto_mask::factor_get(&automasking, ss, vd.vertex, &automask_data);
    float mask;

    switch (mode) {
      case AUTOMASK_BAKE_MIX:
        mask = automask;
        break;
      case AUTOMASK_BAKE_MULTIPLY:
        mask = vd.mask * automask;
        break;
        break;
      case AUTOMASK_BAKE_DIVIDE:
        mask = automask > 0.00001f ? vd.mask / automask : 0.0f;
        break;
        break;
      case AUTOMASK_BAKE_ADD:
        mask = vd.mask + automask;
        break;
      case AUTOMASK_BAKE_SUBTRACT:
        mask = vd.mask - automask;
        break;
    }

    mask = vd.mask + (mask - vd.mask) * factor;
    CLAMP(mask, 0.0f, 1.0f);

    SCULPT_mask_vert_set(BKE_pbvh_type(*ss.pbvh), mask_write, mask, vd);
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update_mask(node);
}

static int sculpt_bake_cavity_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  using namespace blender::ed::sculpt_paint;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  MultiresModifierData *mmd = BKE_sculpt_multires_active(CTX_data_scene(C), &ob);
  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), &ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);
  SCULPT_vertex_random_access_ensure(ss);

  undo::push_begin(ob, op);

  CavityBakeMixMode mode = CavityBakeMixMode(RNA_enum_get(op->ptr, "mix_mode"));
  float factor = RNA_float_get(op->ptr, "mix_factor");

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(*ss.pbvh, {});

  /* Set up automasking settings. */
  Sculpt sd2 = sd;

  CavityBakeSettingsSource src = (CavityBakeSettingsSource)RNA_enum_get(op->ptr,
                                                                        "settings_source");
  switch (src) {
    case AUTOMASK_SETTINGS_OPERATOR:
      if (RNA_boolean_get(op->ptr, "invert")) {
        sd2.automasking_flags = BRUSH_AUTOMASKING_CAVITY_INVERTED;
      }
      else {
        sd2.automasking_flags = BRUSH_AUTOMASKING_CAVITY_NORMAL;
      }

      if (RNA_boolean_get(op->ptr, "use_curve")) {
        sd2.automasking_flags |= BRUSH_AUTOMASKING_CAVITY_USE_CURVE;
      }

      sd2.automasking_cavity_blur_steps = RNA_int_get(op->ptr, "blur_steps");
      sd2.automasking_cavity_factor = RNA_float_get(op->ptr, "factor");

      sd2.automasking_cavity_curve = sd.automasking_cavity_curve_op;
      break;
    case AUTOMASK_SETTINGS_BRUSH:
      if (brush) {
        sd2.automasking_flags = brush->automasking_flags;
        sd2.automasking_cavity_factor = brush->automasking_cavity_factor;
        sd2.automasking_cavity_curve = brush->automasking_cavity_curve;
        sd2.automasking_cavity_blur_steps = brush->automasking_cavity_blur_steps;

        /* Ensure only cavity masking is enabled. */
        sd2.automasking_flags &= BRUSH_AUTOMASKING_CAVITY_ALL | BRUSH_AUTOMASKING_CAVITY_USE_CURVE;
      }
      else {
        sd2.automasking_flags = 0;
        BKE_report(op->reports, RPT_WARNING, "No active brush");

        return OPERATOR_CANCELLED;
      }

      break;
    case AUTOMASK_SETTINGS_SCENE:
      /* Ensure only cavity masking is enabled. */
      sd2.automasking_flags &= BRUSH_AUTOMASKING_CAVITY_ALL | BRUSH_AUTOMASKING_CAVITY_USE_CURVE;
      break;
  }

  /* Ensure cavity mask is actually enabled. */
  if (!(sd2.automasking_flags & BRUSH_AUTOMASKING_CAVITY_ALL)) {
    sd2.automasking_flags |= BRUSH_AUTOMASKING_CAVITY_NORMAL;
  }

  /* Create copy of brush with cleared automasking settings. */
  Brush brush2 = blender::dna::shallow_copy(*brush);
  brush2.automasking_flags = 0;
  brush2.automasking_boundary_edges_propagation_steps = 1;
  brush2.automasking_cavity_curve = sd2.automasking_cavity_curve;

  SCULPT_stroke_id_next(ob);

  std::unique_ptr<auto_mask::Cache> automasking = auto_mask::cache_init(sd2, &brush2, ob);
  const SculptMaskWriteInfo mask_write = SCULPT_mask_get_for_write(ss);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      sculpt_bake_cavity_exec_task(ob, *automasking, mode, factor, mask_write, nodes[i]);
    }
  });

  bke::pbvh::update_mask(*ss.pbvh);
  undo::push_end(ob);

  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

static void cavity_bake_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  Scene *scene = CTX_data_scene(C);
  Sculpt *sd = scene->toolsettings ? scene->toolsettings->sculpt : nullptr;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  CavityBakeSettingsSource source = (CavityBakeSettingsSource)RNA_enum_get(op->ptr,
                                                                           "settings_source");

  if (!sd) {
    source = AUTOMASK_SETTINGS_OPERATOR;
  }

  switch (source) {
    case AUTOMASK_SETTINGS_OPERATOR: {
      uiItemR(layout, op->ptr, "mix_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "mix_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "settings_source", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "factor", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "blur_steps", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "invert", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "use_curve", UI_ITEM_NONE, nullptr, ICON_NONE);

      if (sd && RNA_boolean_get(op->ptr, "use_curve")) {
        PointerRNA sculpt_ptr = RNA_pointer_create(&scene->id, &RNA_Sculpt, sd);
        uiTemplateCurveMapping(
            layout, &sculpt_ptr, "automasking_cavity_curve_op", 'v', false, false, false, false);
      }
      break;
    }
    case AUTOMASK_SETTINGS_BRUSH:
    case AUTOMASK_SETTINGS_SCENE:
      uiItemR(layout, op->ptr, "mix_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "mix_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, op->ptr, "settings_source", UI_ITEM_NONE, nullptr, ICON_NONE);

      break;
  }
}

static void SCULPT_OT_mask_from_cavity(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask From Cavity";
  ot->idname = "SCULPT_OT_mask_from_cavity";
  ot->description = "Creates a mask based on the curvature of the surface";
  ot->ui = cavity_bake_ui;

  static EnumPropertyItem mix_modes[] = {
      {AUTOMASK_BAKE_MIX, "MIX", ICON_NONE, "Mix", ""},
      {AUTOMASK_BAKE_MULTIPLY, "MULTIPLY", ICON_NONE, "Multiply", ""},
      {AUTOMASK_BAKE_DIVIDE, "DIVIDE", ICON_NONE, "Divide", ""},
      {AUTOMASK_BAKE_ADD, "ADD", ICON_NONE, "Add", ""},
      {AUTOMASK_BAKE_SUBTRACT, "SUBTRACT", ICON_NONE, "Subtract", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* api callbacks */
  ot->exec = sculpt_bake_cavity_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "mix_mode", mix_modes, AUTOMASK_BAKE_MIX, "Mode", "Mix mode");
  RNA_def_float(ot->srna, "mix_factor", 1.0f, 0.0f, 5.0f, "Mix Factor", "", 0.0f, 1.0f);

  static EnumPropertyItem settings_sources[] = {
      {AUTOMASK_SETTINGS_OPERATOR,
       "OPERATOR",
       ICON_NONE,
       "Operator",
       "Use settings from operator properties"},
      {AUTOMASK_SETTINGS_BRUSH, "BRUSH", ICON_NONE, "Brush", "Use settings from brush"},
      {AUTOMASK_SETTINGS_SCENE, "SCENE", ICON_NONE, "Scene", "Use settings from scene"},
      {0, nullptr, 0, nullptr, nullptr}};

  RNA_def_enum(ot->srna,
               "settings_source",
               settings_sources,
               AUTOMASK_SETTINGS_OPERATOR,
               "Settings",
               "Use settings from here");

  RNA_def_float(ot->srna,
                "factor",
                0.5f,
                0.0f,
                5.0f,
                "Factor",
                "The contrast of the cavity mask",
                0.0f,
                1.0f);
  RNA_def_int(ot->srna,
              "blur_steps",
              2,
              0,
              25,
              "Blur",
              "The number of times the cavity mask is blurred",
              0,
              25);
  RNA_def_boolean(ot->srna, "use_curve", false, "Custom Curve", "");

  RNA_def_boolean(ot->srna, "invert", false, "Cavity (Inverted)", "");
}

void ED_operatortypes_sculpt()
{
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(SCULPT_OT_brush_stroke);
  WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
  WM_operatortype_append(SCULPT_OT_set_persistent_base);
  WM_operatortype_append(dyntopo::SCULPT_OT_dynamic_topology_toggle);
  WM_operatortype_append(SCULPT_OT_optimize);
  WM_operatortype_append(SCULPT_OT_symmetrize);
  WM_operatortype_append(dyntopo::SCULPT_OT_detail_flood_fill);
  WM_operatortype_append(dyntopo::SCULPT_OT_sample_detail_size);
  WM_operatortype_append(filter::SCULPT_OT_mesh_filter);
  WM_operatortype_append(mask::SCULPT_OT_mask_filter);
  WM_operatortype_append(SCULPT_OT_set_pivot_position);
  WM_operatortype_append(face_set::SCULPT_OT_face_sets_create);
  WM_operatortype_append(face_set::SCULPT_OT_face_set_change_visibility);
  WM_operatortype_append(face_set::SCULPT_OT_face_sets_randomize_colors);
  WM_operatortype_append(face_set::SCULPT_OT_face_sets_init);
  WM_operatortype_append(face_set::SCULPT_OT_face_sets_edit);
  WM_operatortype_append(cloth::SCULPT_OT_cloth_filter);
  WM_operatortype_append(face_set::SCULPT_OT_face_set_lasso_gesture);
  WM_operatortype_append(face_set::SCULPT_OT_face_set_box_gesture);
  WM_operatortype_append(face_set::SCULPT_OT_face_set_line_gesture);
  WM_operatortype_append(face_set::SCULPT_OT_face_set_polyline_gesture);
  WM_operatortype_append(trim::SCULPT_OT_trim_box_gesture);
  WM_operatortype_append(trim::SCULPT_OT_trim_lasso_gesture);
  WM_operatortype_append(trim::SCULPT_OT_trim_line_gesture);
  WM_operatortype_append(trim::SCULPT_OT_trim_polyline_gesture);
  WM_operatortype_append(project::SCULPT_OT_project_line_gesture);

  WM_operatortype_append(SCULPT_OT_sample_color);
  WM_operatortype_append(color::SCULPT_OT_color_filter);
  WM_operatortype_append(mask::SCULPT_OT_mask_by_color);
  WM_operatortype_append(dyntopo::SCULPT_OT_dyntopo_detail_size_edit);
  WM_operatortype_append(mask::SCULPT_OT_mask_init);

  WM_operatortype_append(expand::SCULPT_OT_expand);
  WM_operatortype_append(SCULPT_OT_mask_from_cavity);
}

void ED_keymap_sculpt(wmKeyConfig *keyconf)
{
  using namespace blender::ed::sculpt_paint;
  filter::modal_keymap(keyconf);
}
