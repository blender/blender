/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 * Implements the Sculpt Mode tools
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"
#include "atomic_ops.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_brush.h"
#include "BKE_brush_engine.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_kelvinlet.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_mirror.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_colormanagement.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Reset the copy of the mesh that is being sculpted on (currently just for the layer brush). */

static int sculpt_set_persistent_base_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return OPERATOR_FINISHED;
  }
  SCULPT_vertex_random_access_ensure(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);

  SculptCustomLayer *scl_co, *scl_no, *scl_disp;

  SCULPT_ensure_persistent_layers(ss, ob);

  scl_co = ss->custom_layers[SCULPT_SCL_PERS_CO];
  scl_no = ss->custom_layers[SCULPT_SCL_PERS_NO];
  scl_disp = ss->custom_layers[SCULPT_SCL_PERS_DISP];

  const int totvert = SCULPT_vertex_count_get(ss);

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    float *co = SCULPT_temp_cdata_get(vertex, scl_co);
    float *no = SCULPT_temp_cdata_get(vertex, scl_no);
    float *disp = SCULPT_temp_cdata_get(vertex, scl_disp);

    copy_v3_v3(co, SCULPT_vertex_co_get(ss, vertex));
    SCULPT_vertex_normal_get(ss, vertex, no);
    *disp = 0.0f;
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

static int sculpt_optimize_exec(bContext *C, wmOperator *UNUSED(op))
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

static int sculpt_spatial_sort_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;

  if (!pbvh) {
    return OPERATOR_CANCELLED;
  }

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_BMESH:
      SCULPT_undo_push_begin(ob, "Dynamic topology symmetrize");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_GEOMETRY);

      BKE_pbvh_reorder_bmesh(ss->pbvh);

      BKE_pbvh_bmesh_on_mesh_change(ss->pbvh);
      BM_log_full_mesh(ss->bm, ss->bm_log);

      ss->active_vertex_index.i = 0;
      ss->active_face_index.i = 0;

      BKE_pbvh_free(ss->pbvh);
      ss->pbvh = NULL;

      /* Finish undo. */
      SCULPT_undo_push_end(ob);

      break;
    case PBVH_FACES:
      return OPERATOR_CANCELLED;
    case PBVH_GRIDS:
      return OPERATOR_CANCELLED;
  }

  /* Redraw. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, ND_DATA | NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}
static void SCULPT_OT_spatial_sort_mesh(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Spatially Sort Mesh";
  ot->idname = "SCULPT_OT_spatial_sort_mesh";
  ot->description = "Spatially sort mesh to improve memory coherency";

  /* API callbacks. */
  ot->exec = sculpt_spatial_sort_exec;
  ot->poll = sculpt_only_bmesh_poll;
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
    case PBVH_BMESH:
      /* Dyntopo Symmetrize. */

      /* To simplify undo for symmetrize, all BMesh elements are logged
       * as deleted, then after symmetrize operation all BMesh elements
       * are logged as added (as opposed to attempting to store just the
       * parts that symmetrize modifies). */
      SCULPT_undo_push_begin(ob, "Dynamic topology symmetrize");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_SYMMETRIZE);

      BM_mesh_toolflags_set(ss->bm, true);

      /* Symmetrize and re-triangulate. */
      BMO_op_callf(ss->bm,
                   (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                   "symmetrize input=%avef direction=%i dist=%f use_shapekey=%b",
                   sd->symmetrize_direction,
                   dist,
                   true);
#ifndef DYNTOPO_DYNAMIC_TESS
      SCULPT_dynamic_topology_triangulate(ss, ss->bm);
#endif
      /* Bisect operator flags edges (keep tags clean for edge queue). */
      BM_mesh_elem_hflag_disable_all(ss->bm, BM_EDGE, BM_ELEM_TAG, false);

      BM_mesh_toolflags_set(ss->bm, false);

      BKE_pbvh_recalc_bmesh_boundary(ss->pbvh);
      SCULT_dyntopo_flag_all_disk_sort(ss);

      // symmetrize is messing up ids, regenerate them from scratch
      BM_reassign_ids(ss->bm);
      BM_mesh_toolflags_set(ss->bm, false);
      BM_log_full_mesh(ss->bm, ss->bm_log);

      /* Finish undo. */
      SCULPT_undo_push_end(ob);

      break;
    case PBVH_FACES:
      /* Mesh Symmetrize. */
      ED_sculpt_undo_geometry_begin(ob, "mesh symmetrize");
      Mesh *mesh = ob->data;

      BKE_mesh_mirror_apply_mirror_on_axis(bmain, mesh, sd->symmetrize_direction, dist);

      ED_sculpt_undo_geometry_end(ob);
      BKE_mesh_calc_normals(ob->data);
      BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

      break;
    case PBVH_GRIDS:
      return OPERATOR_CANCELLED;
  }

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

  RNA_def_float(ot->srna,
                "merge_tolerance",
                0.0002f,
                0.0f,
                FLT_MAX,
                "Merge Distance",
                "Distance within which symmetrical vertices are merged",
                0.0f,
                1.0f);
}

static int sculpt_mode_toggle_exec(bContext *C, wmOperator *op)
{
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  const int mode_flag = OB_MODE_SCULPT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
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
      Mesh *me = ob->data;
      /* Dyntopo adds its own undo step. */
      if ((me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) == 0) {
        /* Without this the memfile undo step is used,
         * while it works it causes lag when undoing the first undo step, see T71564. */
        wmWindowManager *wm = CTX_wm_manager(C);
        if (wm->op_undo_depth <= 1) {
          SCULPT_undo_push_begin(ob, op->type->name);
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

static int vertex_to_loop_colors_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  ID *data;
  data = ob->data;
  if (data && ID_IS_LINKED(data)) {
    return OPERATOR_CANCELLED;
  }

  if (ob->type != OB_MESH) {
    return OPERATOR_CANCELLED;
  }

  Mesh *mesh = ob->data;

  const int mloopcol_layer_n = CustomData_get_active_layer(&mesh->ldata, CD_MLOOPCOL);
  if (mloopcol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MLoopCol *loopcols = CustomData_get_layer_n(&mesh->ldata, CD_MLOOPCOL, mloopcol_layer_n);

  const int MPropCol_layer_n = CustomData_get_active_layer(&mesh->vdata, CD_PROP_COLOR);
  if (MPropCol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MPropCol *vertcols = CustomData_get_layer_n(&mesh->vdata, CD_PROP_COLOR, MPropCol_layer_n);

  MLoop *loops = CustomData_get_layer(&mesh->ldata, CD_MLOOP);
  MPoly *polys = CustomData_get_layer(&mesh->pdata, CD_MPOLY);

  for (int i = 0; i < mesh->totpoly; i++) {
    MPoly *c_poly = &polys[i];
    for (int j = 0; j < c_poly->totloop; j++) {
      int loop_index = c_poly->loopstart + j;
      MLoop *c_loop = &loops[c_poly->loopstart + j];
      loopcols[loop_index].r = (char)(vertcols[c_loop->v].color[0] * 255);
      loopcols[loop_index].g = (char)(vertcols[c_loop->v].color[1] * 255);
      loopcols[loop_index].b = (char)(vertcols[c_loop->v].color[2] * 255);
      loopcols[loop_index].a = (char)(vertcols[c_loop->v].color[3] * 255);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

bool SCULPT_convert_colors_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  bool ok = ob && ob->data && ob->type == OB_MESH;
  ok = ok && ELEM(ob->mode, OB_MODE_SCULPT, OB_MODE_OBJECT);

  return ok;
}

static void SCULPT_OT_vertex_to_loop_colors(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sculpt Vertex Color to Vertex Color";
  ot->description = "Copy to active face corner color attribute";
  ot->idname = "SCULPT_OT_vertex_to_loop_colors";

  /* api callbacks */
  ot->poll = SCULPT_convert_colors_poll;
  ot->exec = vertex_to_loop_colors_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int loop_to_vertex_colors_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  ID *data;
  data = ob->data;
  if (data && ID_IS_LINKED(data)) {
    return OPERATOR_CANCELLED;
  }

  if (ob->type != OB_MESH) {
    return OPERATOR_CANCELLED;
  }

  Mesh *mesh = ob->data;

  const int mloopcol_layer_n = CustomData_get_active_layer(&mesh->ldata, CD_MLOOPCOL);
  if (mloopcol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MLoopCol *loopcols = CustomData_get_layer_n(&mesh->ldata, CD_MLOOPCOL, mloopcol_layer_n);

  const int MPropCol_layer_n = CustomData_get_active_layer(&mesh->vdata, CD_PROP_COLOR);
  if (MPropCol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MPropCol *vertcols = CustomData_get_layer_n(&mesh->vdata, CD_PROP_COLOR, MPropCol_layer_n);

  MLoop *loops = CustomData_get_layer(&mesh->ldata, CD_MLOOP);
  MPoly *polys = CustomData_get_layer(&mesh->pdata, CD_MPOLY);

  for (int i = 0; i < mesh->totpoly; i++) {
    MPoly *c_poly = &polys[i];
    for (int j = 0; j < c_poly->totloop; j++) {
      int loop_index = c_poly->loopstart + j;
      MLoop *c_loop = &loops[c_poly->loopstart + j];
      vertcols[c_loop->v].color[0] = (loopcols[loop_index].r / 255.0f);
      vertcols[c_loop->v].color[1] = (loopcols[loop_index].g / 255.0f);
      vertcols[c_loop->v].color[2] = (loopcols[loop_index].b / 255.0f);
      vertcols[c_loop->v].color[3] = (loopcols[loop_index].a / 255.0f);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_loop_to_vertex_colors(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Color to Sculpt Vertex Color";
  ot->description = "Load from active face corner color attribute";
  ot->idname = "SCULPT_OT_loop_to_vertex_colors";

  /* api callbacks */
  ot->poll = SCULPT_convert_colors_poll;
  ot->exec = loop_to_vertex_colors_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
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

static void sculpt_sample_color_draw(const bContext *UNUSED(C), ARegion *UNUSED(ar), void *arg)
{
  SampleColorCustomData *sccd = (SampleColorCustomData *)arg;
  GPU_line_width(2.0f);
  GPU_line_smooth(true);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

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
  if (base_sample == NULL) {
    return false;
  }

  Object *object_sample = base_sample->object;
  if (object_sample->type != OB_MESH) {
    return false;
  }

  Object *ob_eval = DEG_get_evaluated_object(depsgraph, object_sample);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
  MPropCol *vcol = CustomData_get_layer(&me_eval->vdata, CD_PROP_COLOR);

  if (!vcol) {
    return false;
  }

  ARegion *region = CTX_wm_region(C);
  float global_loc[3];
  if (!ED_view3d_autodist_simple(region, event->mval, global_loc, 0, NULL)) {
    return false;
  }

  float object_loc[3];
  mul_v3_m4v3(object_loc, ob_eval->imat, global_loc);

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
  IMB_colormanagement_scene_linear_to_srgb_v3(sccd->sampled_color);
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

  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  bool use_channels = mode == PAINT_MODE_SCULPT;

  /* Finish operation on release. */
  if (event->val == KM_RELEASE) {
    float color_srgb[3];
    copy_v3_v3(color_srgb, sccd->sampled_color);
    BKE_brush_color_set(scene, brush, sccd->sampled_color, use_channels);
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
    SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);
    SCULPT_vertex_color_get(ss, active_vertex, sccd->sampled_color);
    IMB_colormanagement_scene_linear_to_srgb_v3(sccd->sampled_color);
  }
  else {
    sculpt_sample_color_update_from_base(C, event, sccd);
  }

  ss->draw_faded_cursor = true;
  ED_region_tag_redraw(region);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_sample_color_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(e))
{
  ARegion *region = CTX_wm_region(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);
  float active_vertex_color[4];

  if (!SCULPT_vertex_color_get(ss, active_vertex, active_vertex_color)) {
    return OPERATOR_CANCELLED;
  }
  else {
    const SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);
    float active_vertex_color[4];

    SCULPT_vertex_color_get(ss, active_vertex, active_vertex_color);

    if (!active_vertex_color) {
      return OPERATOR_CANCELLED;
    }

    SampleColorCustomData *sccd = MEM_callocN(sizeof(SampleColorCustomData),
                                              "Sample Color Custom Data");
    copy_v4_v4(sccd->sampled_color, active_vertex_color);
    copy_v4_v4(sccd->initial_color, BKE_brush_color_get(scene, brush));

    sccd->draw_handle = ED_region_draw_cb_activate(
        region->type, sculpt_sample_color_draw, sccd, REGION_DRAW_POST_PIXEL);

    op->customdata = sccd;

    WM_event_add_modal_handler(C, op);
    ED_region_tag_redraw(region);

    return OPERATOR_RUNNING_MODAL;
  }
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
  ot->poll = SCULPT_vertex_colors_poll;

  ot->flag = OPTYPE_REGISTER;
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

typedef struct MaskByColorContiguousFloodFillData {
  float threshold;
  bool invert;
  float *new_mask;
  float initial_color[4];
} MaskByColorContiguousFloodFillData;

static void do_mask_by_color_contiguous_update_nodes_cb(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
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
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_redraw(data->nodes[n]);
  }
}

static bool sculpt_mask_by_color_contiguous_floodfill_cb(
    SculptSession *ss, SculptVertRef from_v, SculptVertRef to_v, bool is_duplicate, void *userdata)
{
  MaskByColorContiguousFloodFillData *data = userdata;
  int to_v_i = BKE_pbvh_vertex_index_to_table(ss->pbvh, to_v);
  int from_v_i = BKE_pbvh_vertex_index_to_table(ss->pbvh, from_v);

  const float current_color[4];

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
                                            const SculptVertRef vertex,
                                            const float threshold,
                                            const bool invert,
                                            const bool preserve_mask)
{
  SculptSession *ss = object->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *new_mask = MEM_calloc_arrayN(totvert, sizeof(float), "new mask");

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
  SCULPT_vertex_color_get(ss, vertex, ffd.initial_color);

  SCULPT_floodfill_execute(ss, &flood, sculpt_mask_by_color_contiguous_floodfill_cb, &ffd);
  SCULPT_floodfill_free(&flood);

  int totnode;
  PBVHNode **nodes;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .ob = object,
      .nodes = nodes,
      .mask_by_color_floodfill = new_mask,
      .mask_by_color_vertex = vertex,
      .mask_by_color_threshold = threshold,
      .mask_by_color_invert = invert,
      .mask_by_color_preserve_mask = preserve_mask,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(
      0, totnode, &data, do_mask_by_color_contiguous_update_nodes_cb, &settings);

  MEM_SAFE_FREE(nodes);

  MEM_freeN(new_mask);
}

static void do_mask_by_color_task_cb(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
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
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_redraw(data->nodes[n]);
  }
}

static void sculpt_mask_by_color_full_mesh(Object *object,
                                           const SculptVertRef vertex,
                                           const float threshold,
                                           const bool invert,
                                           const bool preserve_mask)
{
  SculptSession *ss = object->sculpt;

  int totnode;
  PBVHNode **nodes;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .ob = object,
      .nodes = nodes,
      .mask_by_color_vertex = vertex,
      .mask_by_color_threshold = threshold,
      .mask_by_color_invert = invert,
      .mask_by_color_preserve_mask = preserve_mask,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_mask_by_color_task_cb, &settings);

  MEM_SAFE_FREE(nodes);
}

static int sculpt_mask_by_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  if (!SCULPT_has_colors(ss)) {
    return OPERATOR_CANCELLED;
  }

  /* Color data is not available in Multires. */
  if (!ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_BMESH)) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_vertex_random_access_ensure(ss);

  /* Tools that are not brushes do not have the brush gizmo to update the vertex as the mouse
   * move, so it needs to be updated here. */
  SculptCursorGeometryInfo sgi;
  float mouse[2];
  mouse[0] = event->mval[0];
  mouse[1] = event->mval[1];
  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false);

  SCULPT_undo_push_begin(ob, "Mask by color");

  const SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool invert = RNA_boolean_get(op->ptr, "invert");
  const bool preserve_mask = RNA_boolean_get(op->ptr, "preserve_previous_mask");

  if (RNA_boolean_get(op->ptr, "contiguous")) {
    sculpt_mask_by_color_contiguous(ob, active_vertex, threshold, invert, preserve_mask);
  }
  else {
    sculpt_mask_by_color_full_mesh(ob, active_vertex, threshold, invert, preserve_mask);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  SCULPT_undo_push_end(ob);

  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_mask_by_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask by Color";
  ot->idname = "SCULPT_OT_mask_by_color";
  ot->description = "Creates a mask based on the sculpt vertex colors";

  /* api callbacks */
  ot->invoke = sculpt_mask_by_color_invoke;
  ot->poll = SCULPT_vertex_colors_poll;

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

static int sculpt_set_limit_surface_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return OPERATOR_FINISHED;
  }

  SCULPT_vertex_random_access_ensure(ss);

  if (ss->limit_surface) {
    SCULPT_temp_customlayer_release(ss, ob, ss->limit_surface);
  }

  MEM_SAFE_FREE(ss->limit_surface);

  ss->limit_surface = MEM_callocN(sizeof(SculptCustomLayer), "ss->limit_surface");
  SculptLayerParams params = {.permanent = false, .simple_array = false};

  SCULPT_temp_customlayer_ensure(
      ss, ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "_sculpt_limit_surface", &params);
  SCULPT_temp_customlayer_get(ss,
                              ob,
                              ATTR_DOMAIN_POINT,
                              CD_PROP_FLOAT3,
                              "_sculpt_limit_surface",
                              ss->limit_surface,
                              &params);

  const int totvert = SCULPT_vertex_count_get(ss);
  const bool weighted = false;
  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
    float *f = SCULPT_temp_cdata_get(vertex, ss->limit_surface);

    SCULPT_neighbor_coords_average(ss, f, vertex, 0.0, true, weighted);
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

static int sculpt_regularize_rake_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  Object *sculpt_get_vis_object(bContext * C, SculptSession * ss, char *name);
  void sculpt_end_vis_object(bContext * C, SculptSession * ss, Object * ob, BMesh * bm);

  Object *visob = sculpt_get_vis_object(C, ss, "rakevis");
  BMesh *visbm = BM_mesh_create(
      &bm_mesh_allocsize_default,
      &((struct BMeshCreateParams){.create_unique_ids = false, .use_toolflags = false}));

  if (!ss) {
    printf("mising sculpt session\n");
    return OPERATOR_CANCELLED;
  }
  if (!ss->bm) {
    printf("bmesh only!\n");
    return OPERATOR_CANCELLED;
  }

  BMesh *bm = ss->bm;
  BMIter iter;
  BMVert *v;

  PBVHNode **nodes = NULL;
  int totnode;

  int idx = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_COLOR, "_rake_temp");
  if (idx < 0) {
    printf("no rake temp\n");
    return OPERATOR_CANCELLED;
  }

  int cd_vcol = bm->vdata.layers[idx].offset;
  int cd_vcol_vis = -1;

  idx = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_COLOR, "_rake_vis");
  if (idx >= 0) {
    cd_vcol_vis = bm->vdata.layers[idx].offset;
  }

  for (int step = 0; step < 33; step++) {
    BLI_mempool *nodepool = BLI_mempool_create(sizeof(BMLinkItem), bm->totvert, 4196, 0);

    BKE_pbvh_get_nodes(ss->pbvh, PBVH_Leaf, &nodes, &totnode);

    SCULPT_undo_push_begin(ob, "Regularized Rake Directions");
    for (int i = 0; i < totnode; i++) {
      SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COLOR, -1);
      BKE_pbvh_node_mark_update_color(nodes[i]);
    }
    SCULPT_undo_push_end(ob);

    MEM_SAFE_FREE(nodes);

    BMVert **stack = NULL;
    BLI_array_declare(stack);

    bm->elem_index_dirty |= BM_VERT;
    BM_mesh_elem_index_ensure(bm, BM_VERT);

    BLI_bitmap *visit = BLI_BITMAP_NEW(bm->totvert, "regularize rake visit bitmap");

    BMVert **verts = MEM_malloc_arrayN(bm->totvert, sizeof(*verts), "verts");
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      verts[v->head.index] = v;
      v->head.hflag &= ~BM_ELEM_SELECT;
    }

    RNG *rng = BLI_rng_new((uint)BLI_thread_rand(0));
    BLI_rng_shuffle_array(rng, verts, sizeof(void *), bm->totvert);

    for (int i = 0; i < bm->totvert; i++) {
      BMVert *v = verts[i];

      MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);
      if (mv->flag & (SCULPTVERT_CORNER | SCULPTVERT_FSET_CORNER | SCULPTVERT_SHARP_CORNER |
                      SCULPTVERT_SEAM_CORNER)) {
        continue;
      }

      if (BLI_BITMAP_TEST(visit, v->head.index)) {
        continue;
      }

      // v->head.hflag |= BM_ELEM_SELECT;

      float *dir = BM_ELEM_CD_GET_VOID_P(v, cd_vcol);
      normalize_v3(dir);

      BMLinkItem *node = BLI_mempool_alloc(nodepool);
      node->next = node->prev = NULL;
      node->item = v;
      node->depth = 0;

      BLI_BITMAP_SET(visit, v->head.index, true);

      ListBase queue = {node, node};
      const int boundflag = SCULPTVERT_BOUNDARY | SCULPTVERT_FSET_BOUNDARY |
                            SCULPTVERT_SEAM_BOUNDARY | SCULPTVERT_SHARP_BOUNDARY;
      while (queue.first) {
        BMLinkItem *node2 = BLI_poptail(&queue);
        BMVert *v2 = node2->item;

        float *dir2 = BM_ELEM_CD_GET_VOID_P(v2, cd_vcol);

        if (cd_vcol_vis >= 0) {
          float *color = BM_ELEM_CD_GET_VOID_P(v2, cd_vcol_vis);
          color[0] = color[1] = color[2] = (float)(node2->depth % 5) / 5.0f;
          color[3] = 1.0f;
        }

        if (step % 5 != 0 && node2->depth > 15) {
          // break;
        }
        // dir2[0] = dir2[1] = dir2[2] = (float)(node2->depth % 5) / 5.0f;
        // dir2[3] = 1.0f;

        BMIter viter;
        BMEdge *e;

        int closest_vec_to_perp(
            float dir[3], float r_dir2[3], float no[3], float *buckets, float w);

        // float buckets[8] = {0};
        float tmp[3];
        float dir32[3];
        float avg[3] = {0.0f};
        float tot = 0.0f;

        // angle_on_axis_v3v3v3_v3
        float tco[3];
        zero_v3(tco);
        add_v3_fl(tco, 1000.0f);
        // madd_v3_v3fl(tco, v2->no, -dot_v3v3(v2->no, tco));

        float tanco[3];
        add_v3_v3v3(tanco, v2->co, dir2);

        SCULPT_dyntopo_check_disk_sort(ss, (SculptVertRef){.i = (intptr_t)v2});

        float lastdir3[3];
        float firstdir3[3];
        bool first = true;
        float thsum = 0.0f;

        // don't propegate across singularities

        BM_ITER_ELEM (e, &viter, v2, BM_EDGES_OF_VERT) {
          // e = l->e;
          BMVert *v3 = BM_edge_other_vert(e, v2);
          float *dir3 = BM_ELEM_CD_GET_VOID_P(v3, cd_vcol);
          float dir32[3];

          copy_v3_v3(dir32, dir3);

          if (first) {
            first = false;
            copy_v3_v3(firstdir3, dir32);
          }
          else {
            float th = saacos(dot_v3v3(dir32, lastdir3));
            thsum += th;
          }

          copy_v3_v3(lastdir3, dir32);

          add_v3_v3(avg, dir32);
          tot += 1.0f;
        }

        thsum += saacos(dot_v3v3(lastdir3, firstdir3));
        bool sing = thsum >= M_PI * 0.5f;

        // still apply smoothing even with singularity?
        if (tot > 0.0f && !(mv->flag & boundflag)) {
          mul_v3_fl(avg, 1.0 / tot);
          interp_v3_v3v3(dir2, dir2, avg, sing ? 0.15 : 0.25);
          normalize_v3(dir2);
        }

        if (sing) {
          v2->head.hflag |= BM_ELEM_SELECT;

          if (node2->depth == 0) {
            continue;
          }
        }

        BM_ITER_ELEM (e, &viter, v2, BM_EDGES_OF_VERT) {
          BMVert *v3 = BM_edge_other_vert(e, v2);
          float *dir3 = BM_ELEM_CD_GET_VOID_P(v3, cd_vcol);

          if (BLI_BITMAP_TEST(visit, v3->head.index)) {
            continue;
          }

          copy_v3_v3(dir32, dir3);
          madd_v3_v3fl(dir32, v2->no, -dot_v3v3(dir3, v2->no));
          normalize_v3(dir32);

          if (dot_v3v3(dir32, dir2) < 0) {
            negate_v3(dir32);
          }

          cross_v3_v3v3(tmp, dir32, v2->no);
          normalize_v3(tmp);

          if (dot_v3v3(tmp, dir2) < 0) {
            negate_v3(tmp);
          }

          float th1 = fabsf(saacos(dot_v3v3(dir2, dir32)));
          float th2 = fabsf(saacos(dot_v3v3(dir2, tmp)));

          if (th2 < th1) {
            copy_v3_v3(dir32, tmp);
          }

          madd_v3_v3fl(dir32, v3->no, -dot_v3v3(dir32, v3->no));
          normalize_v3(dir32);
          copy_v3_v3(dir3, dir32);

          // int bits = closest_vec_to_perp(dir2, dir32, v2->no, buckets, 1.0f);

          MSculptVert *mv3 = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v3);
          if (mv3->flag & boundflag) {
            // continue;
          }

          BLI_BITMAP_SET(visit, v3->head.index, true);

          BMLinkItem *node3 = BLI_mempool_alloc(nodepool);
          node3->next = node3->prev = NULL;
          node3->item = v3;
          node3->depth = node2->depth + 1;

          BLI_addhead(&queue, node3);
        }

        BLI_mempool_free(nodepool, node2);
      }
    }

    MEM_SAFE_FREE(verts);
    BLI_array_free(stack);
    BLI_mempool_destroy(nodepool);
    MEM_SAFE_FREE(visit);
  }

  BMVert *v3;
  BM_ITER_MESH (v3, &iter, bm, BM_VERTS_OF_MESH) {
    float visco[3];
    float *dir3 = BM_ELEM_CD_GET_VOID_P(v3, cd_vcol);

    madd_v3_v3v3fl(visco, v3->co, v3->no, 0.001);
    BMVert *vis1 = BM_vert_create(visbm, visco, NULL, BM_CREATE_NOP);

    madd_v3_v3v3fl(visco, visco, dir3, 0.003);
    BMVert *vis2 = BM_vert_create(visbm, visco, NULL, BM_CREATE_NOP);
    BM_edge_create(visbm, vis1, vis2, NULL, BM_CREATE_NOP);

    float tan[3];
    cross_v3_v3v3(tan, dir3, v3->no);
    madd_v3_v3fl(visco, tan, 0.001);

    vis1 = BM_vert_create(visbm, visco, NULL, BM_CREATE_NOP);
    BM_edge_create(visbm, vis1, vis2, NULL, BM_CREATE_NOP);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  sculpt_end_vis_object(C, ss, visob, visbm);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_regularize_rake_directions(wmOperatorType *ot)
{
  ot->name = "Regularize Rake Directions";
  ot->idname = "SCULPT_OT_regularize_rake_directions";
  ot->description = "Development operator";

  /* API callbacks. */
  ot->poll = SCULPT_mode_poll;
  ot->exec = sculpt_regularize_rake_exec;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ED_operatortypes_sculpt(void)
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
  WM_operatortype_append(SCULPT_OT_dirty_mask);
  WM_operatortype_append(SCULPT_OT_mask_expand);
  WM_operatortype_append(SCULPT_OT_set_pivot_position);
  WM_operatortype_append(SCULPT_OT_face_sets_create);
  WM_operatortype_append(SCULPT_OT_face_sets_change_visibility);
  WM_operatortype_append(SCULPT_OT_face_sets_randomize_colors);
  WM_operatortype_append(SCULPT_OT_cloth_filter);
  WM_operatortype_append(SCULPT_OT_face_sets_edit);
  WM_operatortype_append(SCULPT_OT_face_set_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_face_set_box_gesture);
  WM_operatortype_append(SCULPT_OT_trim_box_gesture);
  WM_operatortype_append(SCULPT_OT_trim_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_project_line_gesture);
  WM_operatortype_append(SCULPT_OT_project_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_project_box_gesture);

  WM_operatortype_append(SCULPT_OT_sample_color);
  WM_operatortype_append(SCULPT_OT_loop_to_vertex_colors);
  WM_operatortype_append(SCULPT_OT_vertex_to_loop_colors);
  WM_operatortype_append(SCULPT_OT_color_filter);
  WM_operatortype_append(SCULPT_OT_mask_by_color);
  WM_operatortype_append(SCULPT_OT_dyntopo_detail_size_edit);
  WM_operatortype_append(SCULPT_OT_mask_init);

  WM_operatortype_append(SCULPT_OT_face_sets_init);
  WM_operatortype_append(SCULPT_OT_reset_brushes);
  WM_operatortype_append(SCULPT_OT_ipmask_filter);
  WM_operatortype_append(SCULPT_OT_face_set_by_topology);

  WM_operatortype_append(SCULPT_OT_spatial_sort_mesh);

  WM_operatortype_append(SCULPT_OT_expand);
  WM_operatortype_append(SCULPT_OT_regularize_rake_directions);
}
