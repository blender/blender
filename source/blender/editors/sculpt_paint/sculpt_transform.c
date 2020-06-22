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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

void ED_sculpt_init_transform(struct bContext *C)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  copy_v3_v3(ss->init_pivot_pos, ss->pivot_pos);
  copy_v4_v4(ss->init_pivot_rot, ss->pivot_rot);

  SCULPT_undo_push_begin("Transform");
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);

  ss->pivot_rot[3] = 1.0f;

  SCULPT_vertex_random_access_init(ss);
  SCULPT_filter_cache_init(ob, sd, SCULPT_UNDO_COORDS);
}

static void sculpt_transform_task_cb(void *__restrict userdata,
                                     const int i,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{

  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[i]);

  PBVHVertexIter vd;

  SCULPT_undo_push_node(data->ob, node, SCULPT_UNDO_COORDS);
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    float transformed_co[3], orig_co[3], disp[3];
    float fade = vd.mask ? *vd.mask : 0.0f;
    copy_v3_v3(orig_co, orig_data.co);
    char symm_area = SCULPT_get_vertex_symm_area(orig_co);

    copy_v3_v3(transformed_co, orig_co);
    mul_m4_v3(data->transform_mats[(int)symm_area], transformed_co);
    sub_v3_v3v3(disp, transformed_co, orig_co);
    mul_v3_fl(disp, 1.0f - fade);

    add_v3_v3v3(vd.co, orig_co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(node);
}

void ED_sculpt_update_modal_transform(struct bContext *C)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  SCULPT_vertex_random_access_init(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
  };

  float final_pivot_pos[3], d_t[3], d_r[4];
  float t_mat[4][4], r_mat[4][4], s_mat[4][4], pivot_mat[4][4], pivot_imat[4][4],
      transform_mat[4][4];

  copy_v3_v3(final_pivot_pos, ss->pivot_pos);
  for (int i = 0; i < PAINT_SYMM_AREAS; i++) {
    ePaintSymmetryAreas v_symm = i;

    copy_v3_v3(final_pivot_pos, ss->pivot_pos);

    unit_m4(pivot_mat);

    unit_m4(t_mat);
    unit_m4(r_mat);
    unit_m4(s_mat);

    /* Translation matrix. */
    sub_v3_v3v3(d_t, ss->pivot_pos, ss->init_pivot_pos);
    SCULPT_flip_v3_by_symm_area(d_t, symm, v_symm, ss->init_pivot_pos);
    translate_m4(t_mat, d_t[0], d_t[1], d_t[2]);

    /* Rotation matrix. */
    sub_qt_qtqt(d_r, ss->pivot_rot, ss->init_pivot_rot);
    normalize_qt(d_r);
    SCULPT_flip_quat_by_symm_area(d_r, symm, v_symm, ss->init_pivot_pos);
    quat_to_mat4(r_mat, d_r);

    /* Scale matrix. */
    size_to_mat4(s_mat, ss->pivot_scale);

    /* Pivot matrix. */
    SCULPT_flip_v3_by_symm_area(final_pivot_pos, symm, v_symm, ss->init_pivot_pos);
    translate_m4(pivot_mat, final_pivot_pos[0], final_pivot_pos[1], final_pivot_pos[2]);
    invert_m4_m4(pivot_imat, pivot_mat);

    /* Final transform matrix. */
    mul_m4_m4m4(transform_mat, r_mat, t_mat);
    mul_m4_m4m4(transform_mat, transform_mat, s_mat);
    mul_m4_m4m4(data.transform_mats[i], transform_mat, pivot_imat);
    mul_m4_m4m4(data.transform_mats[i], pivot_mat, data.transform_mats[i]);
  }

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BLI_task_parallel_range(
      0, ss->filter_cache->totnode, &data, sculpt_transform_task_cb, &settings);

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
}

void ED_sculpt_end_transform(struct bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  if (ss->filter_cache) {
    SCULPT_filter_cache_free(ss);
  }
  /* Force undo push to happen even inside transform operator, since the sculpt
   * undo system works separate from regular undo and this is require to properly
   * finish an undo step also when canceling. */
  const bool use_nested_undo = true;
  SCULPT_undo_push_end_ex(use_nested_undo);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
}

typedef enum eSculptPivotPositionModes {
  SCULPT_PIVOT_POSITION_ORIGIN = 0,
  SCULPT_PIVOT_POSITION_UNMASKED = 1,
  SCULPT_PIVOT_POSITION_MASK_BORDER = 2,
  SCULPT_PIVOT_POSITION_ACTIVE_VERTEX = 3,
  SCULPT_PIVOT_POSITION_CURSOR_SURFACE = 4,
} eSculptPivotPositionModes;

static EnumPropertyItem prop_sculpt_pivot_position_types[] = {
    {SCULPT_PIVOT_POSITION_ORIGIN,
     "ORIGIN",
     0,
     "Origin",
     "Sets the pivot to the origin of the sculpt"},
    {SCULPT_PIVOT_POSITION_UNMASKED,
     "UNMASKED",
     0,
     "Unmasked",
     "Sets the pivot position to the average position of the unmasked vertices"},
    {SCULPT_PIVOT_POSITION_MASK_BORDER,
     "BORDER",
     0,
     "Mask border",
     "Sets the pivot position to the center of the border of the mask"},
    {SCULPT_PIVOT_POSITION_ACTIVE_VERTEX,
     "ACTIVE",
     0,
     "Active vertex",
     "Sets the pivot position to the active vertex position"},
    {SCULPT_PIVOT_POSITION_CURSOR_SURFACE,
     "SURFACE",
     0,
     "Surface",
     "Sets the pivot position to the surface under the cursor"},
    {0, NULL, 0, NULL, NULL},
};

static int sculpt_set_pivot_position_exec(bContext *C, wmOperator *op)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  int mode = RNA_enum_get(op->ptr, "mode");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, true, false);

  /* Pivot to center. */
  if (mode == SCULPT_PIVOT_POSITION_ORIGIN) {
    zero_v3(ss->pivot_pos);
  }
  /* Pivot to active vertex. */
  else if (mode == SCULPT_PIVOT_POSITION_ACTIVE_VERTEX) {
    copy_v3_v3(ss->pivot_pos, SCULPT_active_vertex_co_get(ss));
  }
  /* Pivot to raycast surface. */
  else if (mode == SCULPT_PIVOT_POSITION_CURSOR_SURFACE) {
    float stroke_location[3];
    float mouse[2];
    mouse[0] = RNA_float_get(op->ptr, "mouse_x");
    mouse[1] = RNA_float_get(op->ptr, "mouse_y");
    if (SCULPT_stroke_get_location(C, stroke_location, mouse)) {
      copy_v3_v3(ss->pivot_pos, stroke_location);
    }
  }
  else {
    PBVHNode **nodes;
    int totnode;
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

    float avg[3];
    int total = 0;
    zero_v3(avg);

    /* Pivot to unmasked. */
    if (mode == SCULPT_PIVOT_POSITION_UNMASKED) {
      for (int n = 0; n < totnode; n++) {
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
        {
          const float mask = (vd.mask) ? *vd.mask : 0.0f;
          if (mask < 1.0f) {
            if (SCULPT_check_vertex_pivot_symmetry(vd.co, ss->pivot_pos, symm)) {
              add_v3_v3(avg, vd.co);
              total++;
            }
          }
        }
        BKE_pbvh_vertex_iter_end;
      }
    }
    /* Pivot to mask border. */
    else if (mode == SCULPT_PIVOT_POSITION_MASK_BORDER) {
      const float threshold = 0.2f;

      for (int n = 0; n < totnode; n++) {
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE)
        {
          const float mask = (vd.mask) ? *vd.mask : 0.0f;
          if (mask < (0.5f + threshold) && mask > (0.5f - threshold)) {
            if (SCULPT_check_vertex_pivot_symmetry(vd.co, ss->pivot_pos, symm)) {
              add_v3_v3(avg, vd.co);
              total++;
            }
          }
        }
        BKE_pbvh_vertex_iter_end;
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      copy_v3_v3(ss->pivot_pos, avg);
    }

    MEM_SAFE_FREE(nodes);
  }

  ED_region_tag_redraw(region);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);

  return OPERATOR_FINISHED;
}

static int sculpt_set_pivot_position_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_float_set(op->ptr, "mouse_x", event->mval[0]);
  RNA_float_set(op->ptr, "mouse_y", event->mval[1]);
  return sculpt_set_pivot_position_exec(C, op);
}

void SCULPT_OT_set_pivot_position(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Pivot Position";
  ot->idname = "SCULPT_OT_set_pivot_position";
  ot->description = "Sets the sculpt transform pivot position";

  /* API callbacks. */
  ot->invoke = sculpt_set_pivot_position_invoke;
  ot->exec = sculpt_set_pivot_position_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_pivot_position_types,
               SCULPT_PIVOT_POSITION_UNMASKED,
               "Mode",
               "");

  RNA_def_float(ot->srna,
                "mouse_x",
                0.0f,
                0.0f,
                FLT_MAX,
                "Mouse Position X",
                "Position of the mouse used for \"Surface\" mode",
                0.0f,
                10000.0f);
  RNA_def_float(ot->srna,
                "mouse_y",
                0.0f,
                0.0f,
                FLT_MAX,
                "Mouse Position Y",
                "Position of the mouse used for \"Surface\" mode",
                0.0f,
                10000.0f);
}
