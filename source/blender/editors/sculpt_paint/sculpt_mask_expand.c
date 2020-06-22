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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

static void sculpt_mask_expand_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const bool create_face_set = RNA_boolean_get(op->ptr, "create_face_set");

  MEM_freeN(op->customdata);

  for (int n = 0; n < ss->filter_cache->totnode; n++) {
    PBVHNode *node = ss->filter_cache->nodes[n];
    if (create_face_set) {
      for (int i = 0; i < ss->totfaces; i++) {
        ss->face_sets[i] = ss->filter_cache->prev_face_set[i];
      }
    }
    else {
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
      {
        *vd.mask = ss->filter_cache->prev_mask[vd.index];
      }
      BKE_pbvh_vertex_iter_end;
    }

    BKE_pbvh_node_mark_redraw(node);
  }

  if (!create_face_set) {
    SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
  }
  SCULPT_filter_cache_free(ss);
  SCULPT_undo_push_end();
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  ED_workspace_status_text(C, NULL);
}

static void sculpt_expand_task_cb(void *__restrict userdata,
                                  const int i,
                                  const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  PBVHVertexIter vd;
  int update_it = data->mask_expand_update_it;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL)
  {
    int vi = vd.index;
    float final_mask = *vd.mask;
    if (data->mask_expand_use_normals) {
      if (ss->filter_cache->normal_factor[SCULPT_active_vertex_get(ss)] <
          ss->filter_cache->normal_factor[vd.index]) {
        final_mask = 1.0f;
      }
      else {
        final_mask = 0.0f;
      }
    }
    else {
      if (ss->filter_cache->mask_update_it[vi] <= update_it &&
          ss->filter_cache->mask_update_it[vi] != 0) {
        final_mask = 1.0f;
      }
      else {
        final_mask = 0.0f;
      }
    }

    if (data->mask_expand_create_face_set) {
      if (final_mask == 1.0f) {
        SCULPT_vertex_face_set_set(ss, vd.index, ss->filter_cache->new_face_set);
      }
      BKE_pbvh_node_mark_redraw(node);
    }
    else {

      if (data->mask_expand_keep_prev_mask) {
        final_mask = MAX2(ss->filter_cache->prev_mask[vd.index], final_mask);
      }

      if (data->mask_expand_invert_mask) {
        final_mask = 1.0f - final_mask;
      }

      if (*vd.mask != final_mask) {
        if (vd.mvert) {
          vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
        }
        *vd.mask = final_mask;
        BKE_pbvh_node_mark_update_mask(node);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static int sculpt_mask_expand_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  ARegion *region = CTX_wm_region(C);
  float prevclick_f[2];
  copy_v2_v2(prevclick_f, op->customdata);
  int prevclick[2] = {(int)prevclick_f[0], (int)prevclick_f[1]};
  int len = (int)len_v2v2_int(prevclick, event->mval);
  len = abs(len);
  int mask_speed = RNA_int_get(op->ptr, "mask_speed");
  int mask_expand_update_it = len / mask_speed;
  mask_expand_update_it = mask_expand_update_it + 1;

  const bool create_face_set = RNA_boolean_get(op->ptr, "create_face_set");

  if (RNA_boolean_get(op->ptr, "use_cursor")) {
    SculptCursorGeometryInfo sgi;
    float mouse[2];
    mouse[0] = event->mval[0];
    mouse[1] = event->mval[1];
    SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
    mask_expand_update_it = ss->filter_cache->mask_update_it[(int)SCULPT_active_vertex_get(ss)];
  }

  if ((event->type == EVT_ESCKEY && event->val == KM_PRESS) ||
      (event->type == RIGHTMOUSE && event->val == KM_PRESS)) {
    /* Returning OPERATOR_CANCELLED will leak memory due to not finishing
     * undo. Better solution could be to make paint_mesh_restore_co work
     * for this case. */
    sculpt_mask_expand_cancel(C, op);
    return OPERATOR_FINISHED;
  }

  if ((event->type == LEFTMOUSE && event->val == KM_RELEASE) ||
      (event->type == EVT_RETKEY && event->val == KM_PRESS) ||
      (event->type == EVT_PADENTER && event->val == KM_PRESS)) {

    /* Smooth iterations. */
    BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);
    const int smooth_iterations = RNA_int_get(op->ptr, "smooth_iterations");
    SCULPT_mask_filter_smooth_apply(
        sd, ob, ss->filter_cache->nodes, ss->filter_cache->totnode, smooth_iterations);

    /* Pivot position. */
    if (RNA_boolean_get(op->ptr, "update_pivot")) {
      const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
      const float threshold = 0.2f;
      float avg[3];
      int total = 0;
      zero_v3(avg);

      for (int n = 0; n < ss->filter_cache->totnode; n++) {
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin(ss->pbvh, ss->filter_cache->nodes[n], vd, PBVH_ITER_UNIQUE)
        {
          const float mask = (vd.mask) ? *vd.mask : 0.0f;
          if (mask < (0.5f + threshold) && mask > (0.5f - threshold)) {
            if (SCULPT_check_vertex_pivot_symmetry(
                    vd.co, ss->filter_cache->mask_expand_initial_co, symm)) {
              add_v3_v3(avg, vd.co);
              total++;
            }
          }
        }
        BKE_pbvh_vertex_iter_end;
      }

      if (total > 0) {
        mul_v3_fl(avg, 1.0f / total);
        copy_v3_v3(ss->pivot_pos, avg);
      }
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
    }

    MEM_freeN(op->customdata);

    for (int i = 0; i < ss->filter_cache->totnode; i++) {
      BKE_pbvh_node_mark_redraw(ss->filter_cache->nodes[i]);
    }

    SCULPT_filter_cache_free(ss);

    SCULPT_undo_push_end();
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
    ED_workspace_status_text(C, NULL);
    return OPERATOR_FINISHED;
  }

  /* When pressing Ctrl, expand directly to the max number of iterations. This allows to flood fill
   * mask and face sets by connectivity directly. */
  if (event->ctrl) {
    mask_expand_update_it = ss->filter_cache->mask_update_last_it - 1;
  }

  if (!ELEM(event->type, MOUSEMOVE, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY)) {
    return OPERATOR_RUNNING_MODAL;
  }

  if (mask_expand_update_it == ss->filter_cache->mask_update_current_it) {
    ED_region_tag_redraw(region);
    return OPERATOR_RUNNING_MODAL;
  }

  if (mask_expand_update_it < ss->filter_cache->mask_update_last_it) {

    if (create_face_set) {
      for (int i = 0; i < ss->totfaces; i++) {
        ss->face_sets[i] = ss->filter_cache->prev_face_set[i];
      }
    }
    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .nodes = ss->filter_cache->nodes,
        .mask_expand_update_it = mask_expand_update_it,
        .mask_expand_use_normals = RNA_boolean_get(op->ptr, "use_normals"),
        .mask_expand_invert_mask = RNA_boolean_get(op->ptr, "invert"),
        .mask_expand_keep_prev_mask = RNA_boolean_get(op->ptr, "keep_previous_mask"),
        .mask_expand_create_face_set = RNA_boolean_get(op->ptr, "create_face_set"),
    };
    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(
        &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
    BLI_task_parallel_range(0, ss->filter_cache->totnode, &data, sculpt_expand_task_cb, &settings);
    ss->filter_cache->mask_update_current_it = mask_expand_update_it;
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);

  return OPERATOR_RUNNING_MODAL;
}

typedef struct MaskExpandFloodFillData {
  float original_normal[3];
  float edge_sensitivity;
  bool use_normals;
} MaskExpandFloodFillData;

static bool mask_expand_floodfill_cb(
    SculptSession *ss, int from_v, int to_v, bool is_duplicate, void *userdata)
{
  MaskExpandFloodFillData *data = userdata;

  if (!is_duplicate) {
    int to_it = ss->filter_cache->mask_update_it[from_v] + 1;
    ss->filter_cache->mask_update_it[to_v] = to_it;
    if (to_it > ss->filter_cache->mask_update_last_it) {
      ss->filter_cache->mask_update_last_it = to_it;
    }

    if (data->use_normals) {
      float current_normal[3], prev_normal[3];
      SCULPT_vertex_normal_get(ss, to_v, current_normal);
      SCULPT_vertex_normal_get(ss, from_v, prev_normal);
      const float from_edge_factor = ss->filter_cache->edge_factor[from_v];
      ss->filter_cache->edge_factor[to_v] = dot_v3v3(current_normal, prev_normal) *
                                            from_edge_factor;
      ss->filter_cache->normal_factor[to_v] = dot_v3v3(data->original_normal, current_normal) *
                                              powf(from_edge_factor, data->edge_sensitivity);
      CLAMP(ss->filter_cache->normal_factor[to_v], 0.0f, 1.0f);
    }
  }
  else {
    /* PBVH_GRIDS duplicate handling. */
    ss->filter_cache->mask_update_it[to_v] = ss->filter_cache->mask_update_it[from_v];
    if (data->use_normals) {
      ss->filter_cache->edge_factor[to_v] = ss->filter_cache->edge_factor[from_v];
      ss->filter_cache->normal_factor[to_v] = ss->filter_cache->normal_factor[from_v];
    }
  }

  return true;
}

static int sculpt_mask_expand_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  const bool use_normals = RNA_boolean_get(op->ptr, "use_normals");
  const bool create_face_set = RNA_boolean_get(op->ptr, "create_face_set");

  SculptCursorGeometryInfo sgi;
  float mouse[2];
  mouse[0] = event->mval[0];
  mouse[1] = event->mval[1];

  SCULPT_vertex_random_access_init(ss);

  op->customdata = MEM_mallocN(2 * sizeof(float), "initial mouse position");
  copy_v2_v2(op->customdata, mouse);

  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  int vertex_count = SCULPT_vertex_count_get(ss);

  ss->filter_cache = MEM_callocN(sizeof(FilterCache), "filter cache");

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &ss->filter_cache->nodes, &ss->filter_cache->totnode);

  SCULPT_undo_push_begin("Mask Expand");

  if (create_face_set) {
    SCULPT_undo_push_node(ob, ss->filter_cache->nodes[0], SCULPT_UNDO_FACE_SETS);
    for (int i = 0; i < ss->filter_cache->totnode; i++) {
      BKE_pbvh_node_mark_redraw(ss->filter_cache->nodes[i]);
    }
  }
  else {
    for (int i = 0; i < ss->filter_cache->totnode; i++) {
      SCULPT_undo_push_node(ob, ss->filter_cache->nodes[i], SCULPT_UNDO_MASK);
      BKE_pbvh_node_mark_redraw(ss->filter_cache->nodes[i]);
    }
  }

  ss->filter_cache->mask_update_it = MEM_callocN(sizeof(int) * vertex_count,
                                                 "mask update iteration");
  if (use_normals) {
    ss->filter_cache->normal_factor = MEM_callocN(sizeof(float) * vertex_count,
                                                  "mask update normal factor");
    ss->filter_cache->edge_factor = MEM_callocN(sizeof(float) * vertex_count,
                                                "mask update normal factor");
    for (int i = 0; i < vertex_count; i++) {
      ss->filter_cache->edge_factor[i] = 1.0f;
    }
  }

  if (create_face_set) {
    ss->filter_cache->prev_face_set = MEM_callocN(sizeof(float) * ss->totfaces, "prev face mask");
    for (int i = 0; i < ss->totfaces; i++) {
      ss->filter_cache->prev_face_set[i] = ss->face_sets[i];
    }
    ss->filter_cache->new_face_set = SCULPT_face_set_next_available_get(ss);
  }
  else {
    ss->filter_cache->prev_mask = MEM_callocN(sizeof(float) * vertex_count, "prev mask");
    for (int i = 0; i < vertex_count; i++) {
      ss->filter_cache->prev_mask[i] = SCULPT_vertex_mask_get(ss, i);
    }
  }

  ss->filter_cache->mask_update_last_it = 1;
  ss->filter_cache->mask_update_current_it = 1;
  ss->filter_cache->mask_update_it[SCULPT_active_vertex_get(ss)] = 0;

  copy_v3_v3(ss->filter_cache->mask_expand_initial_co, SCULPT_active_vertex_co_get(ss));

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_active(sd, ob, ss, &flood, FLT_MAX);

  MaskExpandFloodFillData fdata = {
      .use_normals = use_normals,
      .edge_sensitivity = RNA_int_get(op->ptr, "edge_sensitivity"),
  };
  SCULPT_active_vertex_normal_get(ss, fdata.original_normal);
  SCULPT_floodfill_execute(ss, &flood, mask_expand_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  if (use_normals) {
    for (int repeat = 0; repeat < 2; repeat++) {
      for (int i = 0; i < vertex_count; i++) {
        float avg = 0.0f;
        SculptVertexNeighborIter ni;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, i, ni) {
          avg += ss->filter_cache->normal_factor[ni.index];
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
        ss->filter_cache->normal_factor[i] = avg / ni.size;
      }
    }

    MEM_SAFE_FREE(ss->filter_cache->edge_factor);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .mask_expand_update_it = 0,
      .mask_expand_use_normals = RNA_boolean_get(op->ptr, "use_normals"),
      .mask_expand_invert_mask = RNA_boolean_get(op->ptr, "invert"),
      .mask_expand_keep_prev_mask = RNA_boolean_get(op->ptr, "keep_previous_mask"),
      .mask_expand_create_face_set = RNA_boolean_get(op->ptr, "create_face_set"),
  };
  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BLI_task_parallel_range(0, ss->filter_cache->totnode, &data, sculpt_expand_task_cb, &settings);

  const char *status_str = TIP_(
      "Move the mouse to expand the mask from the active vertex. LMB: confirm mask, ESC/RMB: "
      "cancel");
  ED_workspace_status_text(C, status_str);

  SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_mask_expand(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mask Expand";
  ot->idname = "SCULPT_OT_mask_expand";
  ot->description = "Expands a mask from the initial active vertex under the cursor";

  /* API callbacks. */
  ot->invoke = sculpt_mask_expand_invoke;
  ot->modal = sculpt_mask_expand_modal;
  ot->cancel = sculpt_mask_expand_cancel;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  ot->prop = RNA_def_boolean(ot->srna, "invert", true, "Invert", "Invert the new mask");
  ot->prop = RNA_def_boolean(
      ot->srna, "use_cursor", true, "Use Cursor", "Expand the mask to the cursor position");
  ot->prop = RNA_def_boolean(ot->srna,
                             "update_pivot",
                             true,
                             "Update Pivot Position",
                             "Set the pivot position to the mask border after creating the mask");
  ot->prop = RNA_def_int(ot->srna, "smooth_iterations", 2, 0, 10, "Smooth iterations", "", 0, 10);
  ot->prop = RNA_def_int(ot->srna, "mask_speed", 5, 1, 10, "Mask speed", "", 1, 10);

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_normals",
                             true,
                             "Use Normals",
                             "Generate the mask using the normals and curvature of the model");
  ot->prop = RNA_def_boolean(ot->srna,
                             "keep_previous_mask",
                             false,
                             "Keep Previous Mask",
                             "Generate the new mask on top of the current one");
  ot->prop = RNA_def_int(ot->srna,
                         "edge_sensitivity",
                         300,
                         0,
                         2000,
                         "Edge Detection Sensitivity",
                         "Sensitivity for expanding the mask across sculpted sharp edges when "
                         "using normals to generate the mask",
                         0,
                         2000);
  ot->prop = RNA_def_boolean(ot->srna,
                             "create_face_set",
                             false,
                             "Expand Face Mask",
                             "Expand a new Face Mask instead of the sculpt mask");
}
