/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode tools.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_brush.hh"
#include "BKE_bvhutils.h"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mirror.hh"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.h"
#include "BKE_multires.hh"
#include "BKE_object.h"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_colormanagement.h"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.h"
#include "WM_types.hh"

#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_space_api.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_immediate.h"
#include "GPU_state.h"
#include "GPU_vertex_buffer.h"
#include "GPU_vertex_format.h"

#include "bmesh.h"
#include "bmesh_log.h"
#include "bmesh_tools.h"

#include "../../bmesh/intern/bmesh_idmap.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace blender::bke::paint;
using blender::float3;

/* Reset the copy of the mesh that is being sculpted on (currently just for the layer brush). */

static int sculpt_set_persistent_base_exec(bContext *C, wmOperator * /*op*/)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return OPERATOR_FINISHED;
  }
  SCULPT_vertex_random_access_ensure(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);

  SculptAttributeParams params = {0};
  params.permanent = true;

  ss->attrs.persistent_co = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_co), &params);
  ss->attrs.persistent_no = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_no), &params);
  ss->attrs.persistent_disp = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(persistent_disp), &params);

  const int totvert = SCULPT_vertex_count_get(ss);

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    vertex_attr_set<float3>(vertex, ss->attrs.persistent_co, SCULPT_vertex_co_get(ss, vertex));
    SCULPT_vertex_normal_get(ss, vertex, vertex_attr_ptr<float>(vertex, ss->attrs.persistent_no));
    vertex_attr_set<float>(vertex, ss->attrs.persistent_disp, 0.0f);
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

  SCULPT_pbvh_clear(ob);
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
    return BKE_pbvh_type(ob->sculpt->pbvh) != PBVH_GRIDS;
  }
  return false;
}

static bool sculpt_only_bmesh_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (SCULPT_mode_poll(C) && ob->sculpt && ob->sculpt->pbvh) {
    return BKE_pbvh_type(ob->sculpt->pbvh) == PBVH_BMESH;
  }
  return false;
}

static int sculpt_symmetrize_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  const Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;
  const float dist = RNA_float_get(op->ptr, "merge_tolerance");

  if (!pbvh) {
    return OPERATOR_CANCELLED;
  }

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_BMESH: {
      /* Dyntopo Symmetrize. */

      /* To simplify undo for symmetrize, all BMesh elements are logged
       * as deleted, then after symmetrize operation all BMesh elements
       * are logged as added (as opposed to attempting to store just the
       * parts that symmetrize modifies). */
      SCULPT_undo_push_begin(ob, op);
      SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_SYMMETRIZE);

      BM_mesh_toolflags_set(ss->bm, true);

      /* Symmetrize and re-triangulate. */
      BMO_op_callf(ss->bm,
                   (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                   "symmetrize input=%avef direction=%i dist=%f use_shapekey=%b",
                   sd->symmetrize_direction,
                   dist,
                   true);

      /* Bisect operator flags edges (keep tags clean for edge queue). */
      BM_mesh_elem_hflag_disable_all(ss->bm, BM_EDGE, BM_ELEM_TAG, false);

      BM_mesh_toolflags_set(ss->bm, false);

      BKE_pbvh_recalc_bmesh_boundary(ss->pbvh);

      /* De-duplicate element IDs. */
      BM_idmap_check_ids(ss->bm_idmap);

      BM_mesh_toolflags_set(ss->bm, false);

      /* Finish undo. */
      BM_log_full_mesh(ss->bm, ss->bm_log);
      SCULPT_undo_push_end(ob);

      break;
    }
    case PBVH_FACES: {
      /* Mesh Symmetrize. */
      ED_sculpt_undo_geometry_begin(ob, op);
      Mesh *mesh = static_cast<Mesh *>(ob->data);

      BKE_mesh_mirror_apply_mirror_on_axis(bmain, mesh, sd->symmetrize_direction, dist);

      ED_sculpt_undo_geometry_end(ob);
      BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);

      break;
    }
    case PBVH_GRIDS:
      return OPERATOR_CANCELLED;
  }

  SCULPT_topology_islands_invalidate(ss);

  /* Redraw. */
  SCULPT_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

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

static void sculpt_init_session(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  /* Create persistent sculpt mode data. */
  BKE_sculpt_toolsettings_data_ensure(scene);

  /* Create sculpt mode session data. */
  if (ob->sculpt != nullptr) {
    BKE_sculptsession_free(ob);
  }

  ob->sculpt = MEM_new<SculptSession>(__func__);
  ob->sculpt->mode_type = OB_MODE_SCULPT;

  ob->sculpt->active_face.i = PBVH_REF_NONE;
  ob->sculpt->active_vertex.i = PBVH_REF_NONE;

  /* Trigger evaluation of modifier stack to ensure
   * multires modifier sets .runtime.ccg in
   * the evaluated mesh.
   */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  /* This function expects a fully evaluated depsgraph. */
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);

  BKE_sculptsession_update_attr_refs(ob);

  SculptSession *ss = ob->sculpt;
  if (ss->face_sets || (ss->bm && ss->cd_faceset_offset != -1)) {
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

    SCULPT_face_random_access_ensure(ss);
    const int new_face_set = SCULPT_face_set_next_available_get(ss);

    for (int i = 0; i < ss->totfaces; i++) {
      PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);

      int fset = SCULPT_face_set_get(ss, face);
      if (fset == SCULPT_FACE_SET_NONE) {
        SCULPT_face_set_set(ss, face, new_face_set);
      }
    }
  }
}

void SCULPT_ensure_valid_pivot(const Object *ob, Scene *scene)
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  const SculptSession *ss = ob->sculpt;

  /* Account for the case where no objects are evaluated. */
  if (!ss->pbvh) {
    return;
  }

  /* No valid pivot? Use bounding box center. */
  if (ups->average_stroke_counter == 0 || !ups->last_stroke_valid) {
    float location[3], max[3];
    BKE_pbvh_bounding_box(ss->pbvh, location, max);

    interp_v3_v3v3(location, location, max, 0.5f);
    mul_m4_v3(ob->object_to_world, location);

    copy_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter = 1;

    /* Update last stroke position. */
    ups->last_stroke_valid = true;
  }
}

void ED_object_sculptmode_enter_ex(Main *bmain,
                                   Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob,
                                   const bool force_dyntopo,
                                   ReportList *reports,
                                   bool do_undo)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  /* Enter sculpt mode. */
  ob->mode |= mode_flag;

  sculpt_init_session(bmain, depsgraph, scene, ob);

  if (!(fabsf(ob->scale[0] - ob->scale[1]) < 1e-4f && fabsf(ob->scale[1] - ob->scale[2]) < 1e-4f))
  {
    BKE_report(
        reports, RPT_WARNING, "Object has non-uniform scale, sculpting may be unpredictable");
  }
  else if (is_negative_m4(ob->object_to_world)) {
    BKE_report(reports, RPT_WARNING, "Object has negative scale, sculpting may be unpredictable");
  }

  Paint *paint = BKE_paint_get_active_from_paintmode(scene, PAINT_MODE_SCULPT);
  BKE_paint_init(bmain, scene, PAINT_MODE_SCULPT, PAINT_CURSOR_SCULPT);

  ED_paint_cursor_start(paint, SCULPT_mode_poll_view3d);

  bool has_multires = false;

  /* Check dynamic-topology flag; re-enter dynamic-topology mode when changing modes,
   * As long as no data was added that is not supported. */
  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);

    const char *message_unsupported = nullptr;
    if (mmd != nullptr) {
      message_unsupported = TIP_("multi-res modifier");
      has_multires = true;
    }
    else {
      enum eDynTopoWarnFlag flag = SCULPT_dynamic_topology_check(scene, ob);
      if (flag == 0) {
        /* pass */
      }
      else if (flag & DYNTOPO_WARN_EDATA) {
        message_unsupported = TIP_("edge data");
      }
      else if (flag & DYNTOPO_WARN_MODIFIER) {
        message_unsupported = TIP_("constructive modifier");
      }
      else {
        BLI_assert(0);
      }
    }

    if (!has_multires && ((message_unsupported == nullptr) || force_dyntopo)) {
      /* Needed because we may be entering this mode before the undo system loads. */
      wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
      bool has_undo = do_undo && wm->undo_stack != nullptr;

      /* Undo push is needed to prevent memory leak. */
      if (has_undo) {
        SCULPT_undo_push_begin_ex(ob, "Dynamic topology enable");
      }

      bool need_bmlog = !ob->sculpt->bm_log;

      SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, ob);

      if (has_undo) {
        SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_BEGIN);
        SCULPT_undo_push_end(ob);
      }
      else if (need_bmlog) {
        if (ob->sculpt->bm_log) {
          BM_log_free(ob->sculpt->bm_log);
          ob->sculpt->bm_log = nullptr;
        }

        if (ob->sculpt->bm_idmap) {
          BM_idmap_destroy(ob->sculpt->bm_idmap);
          ob->sculpt->bm_idmap = nullptr;
        }

        /* Recreate idmap and log. */

        BKE_sculpt_ensure_idmap(ob);

        /* See if we can rebuild the log from the undo stack. */
        SCULPT_undo_ensure_bmlog(ob);

        /* Create an empty log if reconstruction failed. */
        if (!ob->sculpt->bm_log) {
          ob->sculpt->bm_log = BM_log_create(ob->sculpt->bm, ob->sculpt->bm_idmap);
        }
      }
    }
    else {
      BKE_reportf(
          reports, RPT_WARNING, "Dynamic Topology found: %s, disabled", message_unsupported);
      me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
    }
  }

  SCULPT_ensure_valid_pivot(ob, scene);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_sculptmode_enter(bContext *C, Depsgraph *depsgraph, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, reports, true);
}

void ED_object_sculptmode_exit_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  multires_flush_sculpt_updates(ob);

  /* Not needed for now. */
#if 0
  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
  const int flush_recalc = ed_object_sculptmode_flush_recalc_flag(scene, ob, mmd);
#endif

  /* Always for now, so leaving sculpt mode always ensures scene is in
   * a consistent state. */
  if (true || /* flush_recalc || */ (ob->sculpt && ob->sculpt->bm)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  /* Leave sculpt mode. We do this here to prevent
   * the depsgraph spawning a PBVH_FACES after disabling
   * dynamic topology below. */
  ob->mode &= ~mode_flag;

  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    /* Dynamic topology must be disabled before exiting sculpt
     * mode to ensure the undo stack stays in a consistent
     * state. */
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);

    /* Store so we know to re-enable when entering sculpt mode. */
    me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
  }

  BKE_sculptsession_free(ob);

  paint_cursor_delete_textures();

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_sculptmode_exit(bContext *C, Depsgraph *depsgraph)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
}

static int sculpt_mode_toggle_exec(bContext *C, wmOperator *op)
{
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  const int mode_flag = OB_MODE_SCULPT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, eObjectMode(mode_flag), op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
  }
  else {
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, op->reports, true);
    BKE_paint_toolslots_brush_validate(bmain, &ts->sculpt->paint);

    if (ob->mode & mode_flag) {
      Mesh *me = static_cast<Mesh *>(ob->data);
      /* Dyntopo adds its own undo step. */
      if ((me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) == 0) {
        /* Without this the memfile undo step is used,
         * while it works it causes lag when undoing the first undo step, see #71564. */
        wmWindowManager *wm = CTX_wm_manager(C);
        if (wm->op_undo_depth <= 1) {
          SCULPT_undo_push_begin(ob, op);
          SCULPT_undo_push_end(ob);
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

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

void SCULPT_geometry_preview_lines_update(bContext *C, SculptSession *ss, float radius)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);

  ss->preview_vert_count = 0;
  int totpoints = 0;

  /* This function is called from the cursor drawing code, so the PBVH may not be build yet. */
  if (!ss->pbvh) {
    return;
  }

  if (!ss->deform_modifiers_active) {
    return;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    return;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  float brush_co[3];
  copy_v3_v3(brush_co, SCULPT_active_vertex_co_get(ss));

  BLI_bitmap *visited_verts = BLI_BITMAP_NEW(SCULPT_vertex_count_get(ss), "visited_verts");

  /* Assuming an average of 6 edges per vertex in a triangulated mesh. */
  const int max_preview_verts = SCULPT_vertex_count_get(ss) * 3 * 2;

  if (ss->preview_vert_list == nullptr) {
    ss->preview_vert_list = MEM_cnew_array<PBVHVertRef>(max_preview_verts, __func__);
  }

  GSQueue *non_visited_verts = BLI_gsqueue_new(sizeof(PBVHVertRef));
  PBVHVertRef active_v = SCULPT_active_vertex_get(ss);
  BLI_gsqueue_push(non_visited_verts, &active_v);

  while (!BLI_gsqueue_is_empty(non_visited_verts)) {
    PBVHVertRef from_v;

    BLI_gsqueue_pop(non_visited_verts, &from_v);
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, from_v, ni) {
      if (totpoints + (ni.size * 2) < max_preview_verts) {
        PBVHVertRef to_v = ni.vertex;
        int to_v_i = ni.index;

        ss->preview_vert_list[totpoints] = from_v;
        totpoints++;
        ss->preview_vert_list[totpoints] = to_v;
        totpoints++;
        if (BLI_BITMAP_TEST(visited_verts, to_v_i)) {
          continue;
        }
        BLI_BITMAP_ENABLE(visited_verts, to_v_i);
        const float *co = SCULPT_vertex_co_for_grab_active_get(ss, to_v);
        if (len_squared_v3v3(brush_co, co) < radius * radius) {
          BLI_gsqueue_push(non_visited_verts, &to_v);
        }
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  BLI_gsqueue_free(non_visited_verts);

  MEM_freeN(visited_verts);

  ss->preview_vert_count = totpoints;
}

#define SAMPLE_COLOR_PREVIEW_SIZE 60
#define SAMPLE_COLOR_OFFSET_X -15
#define SAMPLE_COLOR_OFFSET_Y -15
typedef struct SampleColorCustomData {
  void *draw_handle;
  Object *active_object;

  float mval[2];

  float initial_color[4];
  float sampled_color[4];
} SampleColorCustomData;

static void sculpt_sample_color_draw(const bContext * /* C */, ARegion * /* ar */, void *arg)
{
  SampleColorCustomData *sccd = (SampleColorCustomData *)arg;
  GPU_line_width(2.0f);
  GPU_line_smooth(true);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

  immUniform1f("size", 16.0f);

  const float origin_x = sccd->mval[0] + SAMPLE_COLOR_OFFSET_X;
  const float origin_y = sccd->mval[1] + SAMPLE_COLOR_OFFSET_Y;

  immUniformColor3fvAlpha(sccd->sampled_color, 1.0f);
  immRectf(pos,
           origin_x,
           origin_y,
           origin_x - SAMPLE_COLOR_PREVIEW_SIZE,
           origin_y - SAMPLE_COLOR_PREVIEW_SIZE);

  immUniformColor3fvAlpha(sccd->initial_color, 1.0f);
  immRectf(pos,
           origin_x - SAMPLE_COLOR_PREVIEW_SIZE,
           origin_y,
           origin_x - 2.0f * SAMPLE_COLOR_PREVIEW_SIZE,
           origin_y - SAMPLE_COLOR_PREVIEW_SIZE);

  immUnbindProgram();
  GPU_line_smooth(false);
}

static bool sculpt_sample_color_update_from_base(bContext *C,
                                                 const wmEvent *event,
                                                 SampleColorCustomData *sccd)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Base *base_sample = ED_view3d_give_base_under_cursor(C, event->mval);
  if (base_sample == nullptr) {
    return false;
  }

  Object *object_sample = base_sample->object;
  if (object_sample->type != OB_MESH) {
    return false;
  }

  Object *ob_eval = DEG_get_evaluated_object(depsgraph, object_sample);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
  MPropCol *vcol = static_cast<MPropCol *>(
      CustomData_get_layer_for_write(&me_eval->vert_data, CD_PROP_COLOR, me_eval->totvert));

  if (!vcol) {
    return false;
  }

  ARegion *region = CTX_wm_region(C);
  float global_loc[3];
  if (!ED_view3d_autodist_simple(region, event->mval, global_loc, 0, nullptr)) {
    return false;
  }

  float object_loc[3];
  mul_v3_m4v3(object_loc, ob_eval->world_to_object, global_loc);

  BVHTreeFromMesh bvh;
  BKE_bvhtree_from_mesh_get(&bvh, me_eval, BVHTREE_FROM_VERTS, 2);
  BVHTreeNearest nearest;
  nearest.index = -1;
  nearest.dist_sq = FLT_MAX;
  BLI_bvhtree_find_nearest(bvh.tree, object_loc, &nearest, bvh.nearest_callback, &bvh);
  if (nearest.index == -1) {
    return false;
  }
  free_bvhtree_from_mesh(&bvh);

  copy_v4_v4(sccd->sampled_color, vcol[nearest.index].color);
  IMB_colormanagement_scene_linear_to_srgb_v3(sccd->sampled_color, sccd->sampled_color);
  return true;
}

static int sculpt_sample_color_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Scene *scene = CTX_data_scene(C);
  Brush *brush = BKE_paint_brush(&sd->paint);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  SampleColorCustomData *sccd = (SampleColorCustomData *)op->customdata;

  /* Finish operation on release. */
  if (event->val == KM_RELEASE) {
    float color_srgb[3];
    copy_v3_v3(color_srgb, sccd->sampled_color);
    BKE_brush_color_set(scene, brush, sccd->sampled_color);
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
    ED_region_draw_cb_exit(region->type, sccd->draw_handle);
    ED_region_tag_redraw(region);
    MEM_freeN(sccd);
    ss->draw_faded_cursor = false;
    return OPERATOR_FINISHED;
  }

  SculptCursorGeometryInfo sgi;
  sccd->mval[0] = event->mval[0];
  sccd->mval[1] = event->mval[1];

  const bool over_mesh = SCULPT_cursor_geometry_info_update(C, &sgi, sccd->mval, false, false);
  if (over_mesh) {
    PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);
    SCULPT_vertex_color_get(ss, active_vertex, sccd->sampled_color);
    IMB_colormanagement_scene_linear_to_srgb_v3(sccd->sampled_color, sccd->sampled_color);
  }
  else {
    sculpt_sample_color_update_from_base(C, event, sccd);
  }

  ss->draw_faded_cursor = true;
  ED_region_tag_redraw(region);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_sample_color_invoke(bContext *C, wmOperator *op, const wmEvent * /* event */)
{
  ARegion *region = CTX_wm_region(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;

  if (!SCULPT_handles_colors_report(ss, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(CTX_data_depsgraph_pointer(C), ob, true, false, false);

  if (!SCULPT_has_colors(ss)) {
    BKE_report(op->reports, RPT_ERROR, "Mesh has no color attributes.");
    return OPERATOR_CANCELLED;
  }

  const PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);
  float active_vertex_color[4];

  SCULPT_vertex_color_get(ss, active_vertex, active_vertex_color);

  float color_srgb[3];
  IMB_colormanagement_scene_linear_to_srgb_v3(color_srgb, active_vertex_color);
  BKE_brush_color_set(scene, brush, color_srgb);

  SampleColorCustomData *sccd = MEM_cnew<SampleColorCustomData>("Sample Color Custom Data");
  copy_v4_v4(sccd->sampled_color, active_vertex_color);
  copy_v4_v4(sccd->initial_color, BKE_brush_color_get(scene, brush));

  sccd->draw_handle = ED_region_draw_cb_activate(
      region->type, sculpt_sample_color_draw, sccd, REGION_DRAW_POST_PIXEL);

  op->customdata = sccd;

  WM_event_add_modal_handler(C, op);
  ED_region_tag_redraw(region);

  return OPERATOR_RUNNING_MODAL;
}

static void SCULPT_OT_sample_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Color";
  ot->idname = "SCULPT_OT_sample_color";
  ot->description = "Sample the vertex color of the active vertex";

  /* api callbacks */
  ot->invoke = sculpt_sample_color_invoke;
  ot->modal = sculpt_sample_color_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;
}

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
  float initial_color[4];
};

static void do_mask_by_color_contiguous_update_nodes_cb(void *__restrict userdata,
                                                        const int n,
                                                        const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;

  SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_MASK);
  bool update_node = false;

  const bool invert = data->mask_by_color_invert;
  const bool preserve_mask = data->mask_by_color_preserve_mask;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    const float current_mask = *vd.mask;
    const float new_mask = data->mask_by_color_floodfill[vd.index];
    *vd.mask = sculpt_mask_by_color_final_mask_get(current_mask, new_mask, invert, preserve_mask);
    if (current_mask == *vd.mask) {
      continue;
    }
    update_node = true;
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_update_mask(data->nodes[n]);
  }
}

static bool sculpt_mask_by_color_contiguous_floodfill_cb(
    SculptSession *ss, PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate, void *userdata)
{
  MaskByColorContiguousFloodFillData *data = static_cast<MaskByColorContiguousFloodFillData *>(
      userdata);
  int to_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, to_v);
  int from_v_i = BKE_pbvh_vertex_to_index(ss->pbvh, from_v);

  float current_color[4];

  SCULPT_vertex_color_get(ss, to_v, current_color);

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

static void sculpt_mask_by_color_contiguous(Object *object,
                                            const PBVHVertRef vertex,
                                            const float threshold,
                                            const bool invert,
                                            const bool preserve_mask)
{
  SculptSession *ss = object->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *new_mask = MEM_cnew_array<float>(totvert, __func__);

  if (invert) {
    for (int i = 0; i < totvert; i++) {
      new_mask[i] = 1.0f;
    }
  }

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_initial(&flood, vertex);

  MaskByColorContiguousFloodFillData ffd;
  ffd.threshold = threshold;
  ffd.invert = invert;
  ffd.new_mask = new_mask;

  float color[4];
  SCULPT_vertex_color_get(ss, vertex, color);

  copy_v3_v3(ffd.initial_color, color);

  SCULPT_floodfill_execute(ss, &flood, sculpt_mask_by_color_contiguous_floodfill_cb, &ffd);
  SCULPT_floodfill_free(&flood);

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  SculptThreadedTaskData data{};
  data.ob = object;
  data.nodes = nodes;
  data.mask_by_color_floodfill = new_mask;
  data.mask_by_color_vertex = vertex;
  data.mask_by_color_threshold = threshold;
  data.mask_by_color_invert = invert;
  data.mask_by_color_preserve_mask = preserve_mask;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(
      0, nodes.size(), &data, do_mask_by_color_contiguous_update_nodes_cb, &settings);

  MEM_freeN(new_mask);
}

static void do_mask_by_color_task_cb(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;

  SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_MASK);
  bool update_node = false;

  const float threshold = data->mask_by_color_threshold;
  const bool invert = data->mask_by_color_invert;
  const bool preserve_mask = data->mask_by_color_preserve_mask;
  float active_color[4];

  SCULPT_vertex_color_get(ss, data->mask_by_color_vertex, active_color);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    const float current_mask = *vd.mask;
    float vcolor[4];

    SCULPT_vertex_color_get(ss, vd.vertex, vcolor);

    const float new_mask = sculpt_mask_by_color_delta_get(active_color, vcolor, threshold, invert);
    *vd.mask = sculpt_mask_by_color_final_mask_get(current_mask, new_mask, invert, preserve_mask);

    if (current_mask == *vd.mask) {
      continue;
    }
    update_node = true;
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_update_mask(data->nodes[n]);
  }
}

static void sculpt_mask_by_color_full_mesh(Object *object,
                                           const PBVHVertRef vertex,
                                           const float threshold,
                                           const bool invert,
                                           const bool preserve_mask)
{
  SculptSession *ss = object->sculpt;

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  SculptThreadedTaskData data{};
  data.ob = object;
  data.nodes = nodes;
  data.mask_by_color_vertex = vertex;
  data.mask_by_color_threshold = threshold;
  data.mask_by_color_invert = invert;
  data.mask_by_color_preserve_mask = preserve_mask;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), &data, do_mask_by_color_task_cb, &settings);
}

static int sculpt_mask_by_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  {
    View3D *v3d = CTX_wm_view3d(C);
    if (v3d && v3d->shading.type == OB_SOLID) {
      v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
    }
  }

  /* Color data is not available in multi-resolution or dynamic topology. */
  if (!SCULPT_handles_colors_report(ss, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  MultiresModifierData *mmd = BKE_sculpt_multires_active(CTX_data_scene(C), ob);
  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  SCULPT_vertex_random_access_ensure(ss);

  /* Tools that are not brushes do not have the brush gizmo to update the vertex as the mouse
   * move, so it needs to be updated here. */
  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false, false);

  SCULPT_undo_push_begin(ob, op);
  BKE_sculpt_color_layer_create_if_needed(ob);

  const PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool invert = RNA_boolean_get(op->ptr, "invert");
  const bool preserve_mask = RNA_boolean_get(op->ptr, "preserve_previous_mask");

  if (SCULPT_has_loop_colors(ob)) {
    BKE_pbvh_ensure_node_loops(ss->pbvh);
  }

  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), ob, ss->multires.modifier);

  if (RNA_boolean_get(op->ptr, "contiguous")) {
    sculpt_mask_by_color_contiguous(ob, active_vertex, threshold, invert, preserve_mask);
  }
  else {
    sculpt_mask_by_color_full_mesh(ob, active_vertex, threshold, invert, preserve_mask);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  SCULPT_undo_push_end(ob);

  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

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

static int sculpt_reset_brushes_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
    if (br->ob_mode != OB_MODE_SCULPT) {
      continue;
    }
    BKE_brush_sculpt_reset(br);
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, br);
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_reset_brushes(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Reset Sculpt Brushes";
  ot->idname = "SCULPT_OT_reset_brushes";
  ot->description = "Resets all sculpt brushes to their default value";

  /* API callbacks. */
  ot->exec = sculpt_reset_brushes_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;
}

static int sculpt_set_limit_surface_exec(bContext *C, wmOperator * /* op */)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return OPERATOR_FINISHED;
  }

  SCULPT_vertex_random_access_ensure(ss);

  SculptAttributeParams params = {};

  if (!ss->attrs.limit_surface) {
    ss->attrs.limit_surface = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(limit_surface), &params);
  }

  const SculptAttribute *scl = ss->attrs.limit_surface;

  const int totvert = SCULPT_vertex_count_get(ss);
  const bool weighted = false;
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
    float *f = vertex_attr_ptr<float>(vertex, scl);

    SCULPT_neighbor_coords_average(ss, f, vertex, 0.0, 0.0f, weighted);
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_limit_surface(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Limit Surface";
  ot->idname = "SCULPT_OT_set_limit_surface";
  ot->description = "Calculates and stores a limit surface from the current mesh";

  /* API callbacks. */
  ot->exec = sculpt_set_limit_surface_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

typedef struct BMLinkItem {
  struct BMLinkItem *next, *prev;
  BMVert *item;
  int depth;
} BMLinkItem;

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

struct AutomaskBakeTaskData {
  SculptSession *ss;
  AutomaskingCache *automasking;
  Span<PBVHNode *> nodes;
  CavityBakeMixMode mode;
  float factor;
  Object *ob;
};

static void sculpt_bake_cavity_exec_task_cb(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict /*tls*/)
{
  AutomaskBakeTaskData *tdata = static_cast<AutomaskBakeTaskData *>(userdata);
  SculptSession *ss = tdata->ss;
  PBVHNode *node = tdata->nodes[n];
  PBVHVertexIter vd;
  const CavityBakeMixMode mode = tdata->mode;
  const float factor = tdata->factor;

  SCULPT_undo_push_node(tdata->ob, node, SCULPT_UNDO_MASK);

  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(tdata->ob, ss, tdata->automasking, &automask_data, node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    float automask = SCULPT_automasking_factor_get(
        tdata->automasking, ss, vd.vertex, &automask_data);
    float mask;

    switch (mode) {
      case AUTOMASK_BAKE_MIX:
        mask = automask;
        break;
      case AUTOMASK_BAKE_MULTIPLY:
        mask = *vd.mask * automask;
        break;
      case AUTOMASK_BAKE_DIVIDE:
        mask = automask > 0.00001f ? *vd.mask / automask : 0.0f;
        break;
      case AUTOMASK_BAKE_ADD:
        mask = *vd.mask + automask;
        break;
      case AUTOMASK_BAKE_SUBTRACT:
        mask = *vd.mask - automask;
        break;
    }

    mask = *vd.mask + (mask - *vd.mask) * factor;
    CLAMP(mask, 0.0f, 1.0f);

    *vd.mask = mask;
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update_mask(node);
}

static int sculpt_bake_cavity_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);

  MultiresModifierData *mmd = BKE_sculpt_multires_active(CTX_data_scene(C), ob);
  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  SCULPT_vertex_random_access_ensure(ss);

  SCULPT_undo_push_begin(ob, op);

  CavityBakeMixMode mode = CavityBakeMixMode(RNA_enum_get(op->ptr, "mix_mode"));
  float factor = RNA_float_get(op->ptr, "mix_factor");

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  AutomaskBakeTaskData tdata;

  /* Set up automasking settings.
   */
  Sculpt sd2 = *sd;

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

      sd2.automasking_cavity_curve = sd->automasking_cavity_curve_op;
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

  tdata.ob = ob;
  tdata.mode = mode;
  tdata.factor = factor;
  tdata.ss = ss;
  tdata.nodes = nodes;
  tdata.automasking = SCULPT_automasking_cache_init(&sd2, &brush2, ob);

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), &tdata, sculpt_bake_cavity_exec_task_cb, &settings);

  SCULPT_automasking_cache_free(ss, ob, tdata.automasking);

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  SCULPT_undo_push_end(ob);

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
        PointerRNA sculpt_ptr;

        RNA_pointer_create(&scene->id, &RNA_Sculpt, sd, &sculpt_ptr);
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

static int sculpt_reveal_all_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  Mesh *mesh = BKE_object_get_original_mesh(ob);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  if (!ss->pbvh) {
    return OPERATOR_CANCELLED;
  }

  bool with_bmesh = BKE_pbvh_type(ss->pbvh) == PBVH_BMESH;

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Propagate face hide state to verts for undo. */
  SCULPT_visibility_sync_all_from_faces(ob);

  SCULPT_undo_push_begin(ob, op);

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_visibility(node);

    if (!with_bmesh) {
      SCULPT_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);
    }
  }

  SCULPT_topology_islands_invalidate(ss);

  if (!with_bmesh) {
    /* As an optimization, free the hide attribute when making all geometry visible. This allows
     * reduced memory usage without manually clearing it later, and allows sculpt operations to
     * avoid checking element's hide status. */
    CustomData_free_layer_named(&mesh->face_data, ".hide_poly", mesh->faces_num);
    ss->hide_poly = nullptr;
  }
  else {
    SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_HIDDEN);

    BMIter iter;
    BMFace *f;
    BMVert *v;

    BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
      BM_log_vert_before_modified(ss->bm, ss->bm_log, v);
    }
    BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
      BM_log_face_modified(ss->bm, ss->bm_log, f);
    }

    SCULPT_face_visibility_all_set(ob, true);
  }

  SCULPT_visibility_sync_all_from_faces(ob);

  /* NOTE: #SCULPT_visibility_sync_all_from_faces may have deleted
   * `pbvh->hide_vert` if hide_poly did not exist, which is why
   * we call #BKE_pbvh_update_hide_attributes_from_mesh here instead of
   * after #CustomData_free_layer_named above. */
  if (!with_bmesh) {
    BKE_pbvh_update_hide_attributes_from_mesh(ss->pbvh);
  }

  BKE_pbvh_update_visibility(ss->pbvh);

  SCULPT_undo_push_end(ob);

  SCULPT_tag_update_overlays(C);
  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_reveal_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Reveal All";
  ot->idname = "SCULPT_OT_reveal_all";
  ot->description = "Unhide all geometry";

  /* Api callbacks. */
  ot->exec = sculpt_reveal_all_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ED_operatortypes_sculpt()
{
  WM_operatortype_append(SCULPT_OT_brush_stroke);
  WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
  WM_operatortype_append(SCULPT_OT_set_persistent_base);
  WM_operatortype_append(SCULPT_OT_set_limit_surface);
  WM_operatortype_append(SCULPT_OT_dynamic_topology_toggle);
  WM_operatortype_append(SCULPT_OT_optimize);
  WM_operatortype_append(SCULPT_OT_symmetrize);
  WM_operatortype_append(SCULPT_OT_detail_flood_fill);
  WM_operatortype_append(SCULPT_OT_sample_detail_size);
  WM_operatortype_append(SCULPT_OT_set_detail_size);
  WM_operatortype_append(SCULPT_OT_mesh_filter);
  WM_operatortype_append(SCULPT_OT_mask_filter);
  WM_operatortype_append(SCULPT_OT_mask_expand);
  WM_operatortype_append(SCULPT_OT_set_pivot_position);
  WM_operatortype_append(SCULPT_OT_face_sets_create);
  WM_operatortype_append(SCULPT_OT_face_set_change_visibility);
  WM_operatortype_append(SCULPT_OT_face_sets_invert_visibility);
  WM_operatortype_append(SCULPT_OT_face_sets_randomize_colors);
  WM_operatortype_append(SCULPT_OT_cloth_filter);
  WM_operatortype_append(SCULPT_OT_face_sets_edit);
  WM_operatortype_append(SCULPT_OT_face_set_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_face_set_box_gesture);
  WM_operatortype_append(SCULPT_OT_trim_box_gesture);
  WM_operatortype_append(SCULPT_OT_trim_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_project_line_gesture);

  WM_operatortype_append(SCULPT_OT_sample_color);
  WM_operatortype_append(SCULPT_OT_color_filter);
  WM_operatortype_append(SCULPT_OT_mask_by_color);
  WM_operatortype_append(SCULPT_OT_dyntopo_detail_size_edit);
  WM_operatortype_append(SCULPT_OT_mask_init);

  WM_operatortype_append(SCULPT_OT_face_sets_init);
  WM_operatortype_append(SCULPT_OT_reset_brushes);
  WM_operatortype_append(SCULPT_OT_ipmask_filter);

  WM_operatortype_append(SCULPT_OT_expand);
  WM_operatortype_append(SCULPT_OT_mask_from_cavity);
  WM_operatortype_append(SCULPT_OT_reveal_all);
}

void ED_keymap_sculpt(wmKeyConfig *keyconf)
{
  filter_mesh_modal_keymap(keyconf);
}
