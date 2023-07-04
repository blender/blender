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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_task.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh_api.hh"
#include "BKE_scene.h"

#include "IMB_colormanagement.h"

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

#include "UI_interface.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

static EnumPropertyItem prop_sculpt_gradient_type[] = {
    {SCULPT_GRADIENT_LINEAR, "LINEAR", 0, "Linear", ""},
    {SCULPT_GRADIENT_SPHERICAL, "SPHERICAL", 0, "Spherical", ""},
    {SCULPT_GRADIENT_RADIAL, "RADIAL", 0, "Radial", ""},
    {SCULPT_GRADIENT_ANGLE, "ANGLE", 0, "Angle", ""},
    {SCULPT_GRADIENT_REFLECTED, "REFLECTED", 0, "Reflected", ""},
    {0, NULL, 0, NULL, NULL},
};

static void sculpt_gradient_apply_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  SculptGradientContext *gcontext = ss->filter_cache->gradient_context;

  SculptOrigVertData orig_data;
  AutomaskingNodeData automask_data;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->filter_cache->automasking, &automask_data, data->nodes[n]);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    float fade = vd.mask ? *vd.mask : 0.0f;
    fade *= SCULPT_automasking_factor_get(
        ss->filter_cache->automasking, ss, vd.vertex, &automask_data);
    if (fade == 0.0f) {
      continue;
    }

    float world_co[3];
    float projected_co[3];

    /* TODO: Implement symmetry by flipping this coordinate. */
    float symm_co[3];
    copy_v3_v3(symm_co, vd.co);

    mul_v3_m4v3(world_co, data->ob->object_to_world, symm_co);
    /* TOOD: Implement this again. */
    /* ED_view3d_project(gcontext->vc.region, world_co, projected_co); */

    float gradient_value = 0.0f;
    switch (gcontext->gradient_type) {
      case SCULPT_GRADIENT_LINEAR:

        break;
      case SCULPT_GRADIENT_SPHERICAL:

        break;
      case SCULPT_GRADIENT_RADIAL: {
        const float dist = len_v2v2(projected_co, gcontext->line_points[0]);
        gradient_value = dist / gcontext->line_length;
      } break;
      case SCULPT_GRADIENT_ANGLE:
        break;
      case SCULPT_GRADIENT_REFLECTED:

        break;
    }

    gradient_value = clamp_f(gradient_value, 0.0f, 1.0f);
    gcontext->sculpt_gradient_apply_for_element(sd, ss, &orig_data, &vd, gradient_value, fade);
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
  gcontext->sculpt_gradient_node_update(data->nodes[n]);
}

static int sculpt_gradient_update_exec(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptGradientContext *gcontext = ss->filter_cache->gradient_context;

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  gcontext->line_points[0][0] = RNA_int_get(op->ptr, "xstart");
  gcontext->line_points[0][1] = RNA_int_get(op->ptr, "ystart");
  gcontext->line_points[1][0] = RNA_int_get(op->ptr, "xend");
  gcontext->line_points[1][1] = RNA_int_get(op->ptr, "yend");
  gcontext->line_length = len_v2v2(gcontext->line_points[0], gcontext->line_points[1]);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  BKE_pbvh_parallel_range_settings(&settings, true, ss->filter_cache->totnode);
  BLI_task_parallel_range(
      0, ss->filter_cache->totnode, &data, sculpt_gradient_apply_task_cb, &settings);

  SCULPT_flush_update_step(C, ss->filter_cache->gradient_context->update_type);

  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_gradient_properties(wmOperatorType *ot)
{
  RNA_def_enum(
      ot->srna, "type", prop_sculpt_gradient_type, SCULPT_GRADIENT_LINEAR, "Gradient Type", "");
}

static void sculpt_gradient_context_init_common(bContext *C,
                                                wmOperator *op,
                                                const wmEvent *event,
                                                SculptGradientContext *gcontext)
{
  /* View Context. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ED_view3d_viewcontext_init(C, &gcontext->vc, depsgraph);

  /* Properties */
  gcontext->gradient_type = RNA_enum_get(op->ptr, "type");
  gcontext->strength = RNA_float_get(op->ptr, "strength");

  /* Symmetry. */
  Object *ob = gcontext->vc.obact;
  gcontext->symm = SCULPT_mesh_symmetry_xyz_get(ob);

  /* Depth */
  SculptCursorGeometryInfo sgi;
  float mouse[2] = {event->mval[0], event->mval[1]};
  const bool hit = SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false);
  if (hit) {
    copy_v3_v3(gcontext->depth_point, sgi.location);
  }
  else {
    zero_v3(gcontext->depth_point);
  }
}

static SculptGradientContext *sculpt_mask_gradient_context_create(Object *ob, wmOperator *op)
{
  SculptGradientContext *gradient_context = MEM_callocN(sizeof(SculptGradientContext),
                                                        "gradient context");
  gradient_context->update_type = SCULPT_UPDATE_MASK;
  return gradient_context;
}

static int sculpt_mask_gradient_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  SCULPT_vertex_random_access_ensure(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, true, false);

  // XXX get area_normal_radius argument properly
  SCULPT_filter_cache_init(C, ob, sd, SCULPT_UNDO_MASK, event->mval, 0.25f, 1.0f);

  ss->filter_cache->gradient_context = sculpt_mask_gradient_context_create(ob, op);
  sculpt_gradient_context_init_common(C, op, event, ss->filter_cache->gradient_context);

  return WM_gesture_straightline_invoke(C, op, event);
}

void SCULPT_OT_mask_gradient(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask Gradient";
  ot->idname = "SCULPT_OT_mask_gradient";
  ot->description = "Creates or modifies the mask using a gradient";

  /* api callbacks */
  /*
  ot->invoke = WM_gesture_straightline_invoke;
  ot->modal = WM_gesture_straightline_modal;
  ot->exec = sculpt_gradient_update_exec;

  ot->poll = SCULPT_mode_poll;
  */

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  sculpt_gradient_properties(ot);
}
