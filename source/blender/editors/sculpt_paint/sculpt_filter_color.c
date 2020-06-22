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

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
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

#include "UI_interface.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

typedef enum eSculptColorFilterTypes {
  COLOR_FILTER_HUE,
  COLOR_FILTER_SATURATION,
  COLOR_FILTER_VALUE,
  COLOR_FILTER_BRIGHTNESS,
  COLOR_FILTER_CONTRAST,
  COLOR_FILTER_RED,
  COLOR_FILTER_GREEN,
  COLOR_FILTER_BLUE,
  COLOR_FILTER_SMOOTH,
} eSculptColorFilterTypes;

EnumPropertyItem prop_color_filter_types[] = {
    {COLOR_FILTER_HUE, "HUE", 0, "Hue", "Change hue"},
    {COLOR_FILTER_SATURATION, "SATURATION", 0, "Saturation", "Change saturation"},
    {COLOR_FILTER_VALUE, "VALUE", 0, "Value", "Change value"},

    {COLOR_FILTER_BRIGHTNESS, "BRIGTHNESS", 0, "Brightness", "Change brightness"},
    {COLOR_FILTER_CONTRAST, "CONTRAST", 0, "Contrast", "Change contrast"},

    {COLOR_FILTER_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth colors"},

    {COLOR_FILTER_RED, "RED", 0, "Red", "Change red channel"},
    {COLOR_FILTER_GREEN, "GREEN", 0, "Green", "Change green channel"},
    {COLOR_FILTER_BLUE, "BLUE", 0, "Blue", "Change blue channel"},
    {0, NULL, 0, NULL, NULL},
};

static void color_filter_task_cb(void *__restrict userdata,
                                 const int n,
                                 const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  const int mode = data->filter_type;

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    float orig_color[3], final_color[4], hsv_color[3];
    int hue;
    float brightness, contrast, gain, delta, offset;
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= data->filter_strength;

    copy_v3_v3(orig_color, orig_data.col);

    switch (mode) {
      case COLOR_FILTER_HUE:
        rgb_to_hsv_v(orig_color, hsv_color);
        hue = hsv_color[0] + fade;
        hsv_color[0] = fabs((hsv_color[0] + fade) - hue);
        hsv_to_rgb_v(hsv_color, final_color);
        break;
      case COLOR_FILTER_SATURATION:
        rgb_to_hsv_v(orig_color, hsv_color);
        hsv_color[1] = hsv_color[1] + fade;
        CLAMP(hsv_color[1], 0.0f, 1.0f);
        hsv_to_rgb_v(hsv_color, final_color);
        break;
      case COLOR_FILTER_VALUE:
        rgb_to_hsv_v(orig_color, hsv_color);
        hsv_color[2] = hsv_color[2] + fade;
        CLAMP(hsv_color[2], 0.0f, 1.0f);
        hsv_to_rgb_v(hsv_color, final_color);
        break;
      case COLOR_FILTER_RED:
        orig_color[0] = orig_color[0] + fade;
        CLAMP(orig_color[0], 0.0f, 1.0f);
        copy_v3_v3(final_color, orig_color);
        break;
      case COLOR_FILTER_GREEN:
        orig_color[1] = orig_color[1] + fade;
        CLAMP(orig_color[1], 0.0f, 1.0f);
        copy_v3_v3(final_color, orig_color);
        break;
      case COLOR_FILTER_BLUE:
        orig_color[2] = orig_color[2] + fade;
        CLAMP(orig_color[2], 0.0f, 1.0f);
        copy_v3_v3(final_color, orig_color);
        break;
      case COLOR_FILTER_BRIGHTNESS:
        CLAMP(fade, -1.0f, 1.0f);
        brightness = fade;
        contrast = 0;
        delta = contrast / 2.0f;
        gain = 1.0f - delta * 2.0f;
        delta *= -1;
        offset = gain * (brightness + delta);
        for (int i = 0; i < 3; i++) {
          final_color[i] = gain * orig_color[i] + offset;
          CLAMP(final_color[i], 0.0f, 1.0f);
        }
        break;
      case COLOR_FILTER_CONTRAST:
        CLAMP(fade, -1.0f, 1.0f);
        brightness = 0;
        contrast = fade;
        delta = contrast / 2.0f;
        gain = 1.0f - delta * 2.0f;
        if (contrast > 0) {
          gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
          offset = gain * (brightness - delta);
        }
        else {
          delta *= -1;
          offset = gain * (brightness + delta);
        }
        for (int i = 0; i < 3; i++) {
          final_color[i] = gain * orig_color[i] + offset;
          CLAMP(final_color[i], 0.0f, 1.0f);
        }
        break;
      case COLOR_FILTER_SMOOTH: {
        CLAMP(fade, -1.0f, 1.0f);
        float smooth_color[4];
        SCULPT_neighbor_color_average(ss, smooth_color, vd.index);
        blend_color_interpolate_float(final_color, vd.col, smooth_color, fade);
        break;
      }
    }

    copy_v3_v3(vd.col, final_color);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  BKE_pbvh_node_mark_update_color(data->nodes[n]);
}

static int sculpt_color_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  const int mode = RNA_enum_get(op->ptr, "type");
  float filter_strength = RNA_float_get(op->ptr, "strength");

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    SCULPT_undo_push_end();
    SCULPT_filter_cache_free(ss);
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COLOR);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  float len = event->prevclickx - event->mval[0];
  filter_strength = filter_strength * -len * 0.001f;

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .filter_type = mode,
      .filter_strength = filter_strength,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BLI_task_parallel_range(0, ss->filter_cache->totnode, &data, color_filter_task_cb, &settings);

  ED_region_tag_redraw(ar);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_color_filter_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  int mode = RNA_enum_get(op->ptr, "type");
  PBVH *pbvh = ob->sculpt->pbvh;

  /* Disable for multires and dyntopo for now */
  if (!ss->pbvh) {
    return OPERATOR_CANCELLED;
  }
  if (BKE_pbvh_type(pbvh) != PBVH_FACES) {
    return OPERATOR_CANCELLED;
  }

  if (!ss->vcol) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin("color filter");

  bool needs_pmap = mode == COLOR_FILTER_SMOOTH;
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_pmap, false, true);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES && needs_pmap && !ob->sculpt->pmap) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_filter_cache_init(ob, sd, SCULPT_UNDO_COLOR);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_color_filter(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Filter color";
  ot->idname = "SCULPT_OT_color_filter";
  ot->description = "Applies a filter to modify the current sculpt vertex colors";

  /* api callbacks */
  ot->invoke = sculpt_color_filter_invoke;
  ot->modal = sculpt_color_filter_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  RNA_def_enum(ot->srna, "type", prop_color_filter_types, COLOR_FILTER_HUE, "Filter type", "");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter Strength", -10.0f, 10.0f);
}
