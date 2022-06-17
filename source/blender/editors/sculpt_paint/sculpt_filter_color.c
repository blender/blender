/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

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
#include "BKE_report.h"
#include "BKE_scene.h"

#include "IMB_colormanagement.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_paint.h"
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
  COLOR_FILTER_FILL,
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

static EnumPropertyItem prop_color_filter_types[] = {
    {COLOR_FILTER_FILL, "FILL", 0, "Fill", "Fill with a specific color"},
    {COLOR_FILTER_HUE, "HUE", 0, "Hue", "Change hue"},
    {COLOR_FILTER_SATURATION, "SATURATION", 0, "Saturation", "Change saturation"},
    {COLOR_FILTER_VALUE, "VALUE", 0, "Value", "Change value"},

    {COLOR_FILTER_BRIGHTNESS, "BRIGHTNESS", 0, "Brightness", "Change brightness"},
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
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    float orig_color[3], final_color[4], hsv_color[3];
    int hue;
    float brightness, contrast, gain, delta, offset;
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= data->filter_strength;
    fade *= SCULPT_automasking_factor_get(ss->filter_cache->automasking, ss, vd.index);
    if (fade == 0.0f) {
      continue;
    }

    copy_v3_v3(orig_color, orig_data.col);
    final_color[3] = orig_data.col[3]; /* Copy alpha */

    switch (mode) {
      case COLOR_FILTER_FILL: {
        float fill_color_rgba[4];
        copy_v3_v3(fill_color_rgba, data->filter_fill_color);
        fill_color_rgba[3] = 1.0f;
        fade = clamp_f(fade, 0.0f, 1.0f);
        mul_v4_fl(fill_color_rgba, fade);
        blend_color_mix_float(final_color, orig_data.col, fill_color_rgba);
        break;
      }
      case COLOR_FILTER_HUE:
        rgb_to_hsv_v(orig_color, hsv_color);
        hue = hsv_color[0];
        hsv_color[0] = fmod((hsv_color[0] + fabs(fade)) - hue, 1);
        hsv_to_rgb_v(hsv_color, final_color);
        break;
      case COLOR_FILTER_SATURATION:
        rgb_to_hsv_v(orig_color, hsv_color);

        if (hsv_color[1] > 0.001f) {
          hsv_color[1] = clamp_f(hsv_color[1] + fade * hsv_color[1], 0.0f, 1.0f);
          hsv_to_rgb_v(hsv_color, final_color);
        }
        else {
          copy_v3_v3(final_color, orig_color);
        }
        break;
      case COLOR_FILTER_VALUE:
        rgb_to_hsv_v(orig_color, hsv_color);
        hsv_color[2] = clamp_f(hsv_color[2] + fade, 0.0f, 1.0f);
        hsv_to_rgb_v(hsv_color, final_color);
        break;
      case COLOR_FILTER_RED:
        orig_color[0] = clamp_f(orig_color[0] + fade, 0.0f, 1.0f);
        copy_v3_v3(final_color, orig_color);
        break;
      case COLOR_FILTER_GREEN:
        orig_color[1] = clamp_f(orig_color[1] + fade, 0.0f, 1.0f);
        copy_v3_v3(final_color, orig_color);
        break;
      case COLOR_FILTER_BLUE:
        orig_color[2] = clamp_f(orig_color[2] + fade, 0.0f, 1.0f);
        copy_v3_v3(final_color, orig_color);
        break;
      case COLOR_FILTER_BRIGHTNESS:
        fade = clamp_f(fade, -1.0f, 1.0f);
        brightness = fade;
        contrast = 0;
        delta = contrast / 2.0f;
        gain = 1.0f - delta * 2.0f;
        delta *= -1;
        offset = gain * (brightness + delta);
        for (int i = 0; i < 3; i++) {
          final_color[i] = clamp_f(gain * orig_color[i] + offset, 0.0f, 1.0f);
        }
        break;
      case COLOR_FILTER_CONTRAST:
        fade = clamp_f(fade, -1.0f, 1.0f);
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
          final_color[i] = clamp_f(gain * orig_color[i] + offset, 0.0f, 1.0f);
        }
        break;
      case COLOR_FILTER_SMOOTH: {
        fade = clamp_f(fade, -1.0f, 1.0f);
        float smooth_color[4];
        SCULPT_neighbor_color_average(ss, smooth_color, vd.index);

        float col[4];
        SCULPT_vertex_color_get(ss, vd.index, col);

        if (fade < 0.0f) {
          interp_v4_v4v4(smooth_color, smooth_color, col, 0.5f);
        }

        bool copy_alpha = col[3] == smooth_color[3];

        if (fade < 0.0f) {
          float delta_color[4];

          /* Unsharp mask. */
          copy_v4_v4(delta_color, ss->filter_cache->pre_smoothed_color[vd.index]);
          sub_v4_v4(delta_color, smooth_color);

          copy_v4_v4(final_color, col);
          madd_v4_v4fl(final_color, delta_color, fade);
        }
        else {
          blend_color_interpolate_float(final_color, col, smooth_color, fade);
        }

        CLAMP4(final_color, 0.0f, 1.0f);

        /* Prevent accumulated numeric error from corrupting alpha. */
        if (copy_alpha) {
          final_color[3] = smooth_color[3];
        }
        break;
      }
    }

    SCULPT_vertex_color_set(ss, vd.index, final_color);

    if (vd.mvert) {
      BKE_pbvh_vert_mark_update(ss->pbvh, vd.index);
    }
  }
  BKE_pbvh_vertex_iter_end;
  BKE_pbvh_node_mark_update_color(data->nodes[n]);
}

static void sculpt_color_presmooth_init(SculptSession *ss)
{
  int totvert = SCULPT_vertex_count_get(ss);

  if (!ss->filter_cache->pre_smoothed_color) {
    ss->filter_cache->pre_smoothed_color = MEM_malloc_arrayN(
        totvert, sizeof(float) * 4, "ss->filter_cache->pre_smoothed_color");
  }

  for (int i = 0; i < totvert; i++) {
    SCULPT_vertex_color_get(ss, i, ss->filter_cache->pre_smoothed_color[i]);
  }

  for (int iteration = 0; iteration < 2; iteration++) {
    for (int i = 0; i < totvert; i++) {
      float avg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      int total = 0;

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, i, ni) {
        float col[4] = {0};

        copy_v4_v4(col, ss->filter_cache->pre_smoothed_color[ni.index]);

        add_v4_v4(avg, col);
        total++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (total > 0) {
        mul_v4_fl(avg, 1.0f / (float)total);
        interp_v4_v4v4(ss->filter_cache->pre_smoothed_color[i],
                       ss->filter_cache->pre_smoothed_color[i],
                       avg,
                       0.5f);
      }
    }
  }
}

static int sculpt_color_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  const int mode = RNA_enum_get(op->ptr, "type");
  float filter_strength = RNA_float_get(op->ptr, "strength");

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    SCULPT_undo_push_end(ob);
    SCULPT_filter_cache_free(ss);
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COLOR);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = event->prev_press_xy[0] - event->xy[0];
  filter_strength = filter_strength * -len * 0.001f;

  float fill_color[3];
  RNA_float_get_array(op->ptr, "fill_color", fill_color);
  IMB_colormanagement_srgb_to_scene_linear_v3(fill_color, fill_color);

  if (filter_strength < 0.0 && !ss->filter_cache->pre_smoothed_color) {
    sculpt_color_presmooth_init(ss);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .filter_type = mode,
      .filter_strength = filter_strength,
      .filter_fill_color = fill_color,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  BKE_pbvh_parallel_range_settings(&settings, true, ss->filter_cache->totnode);
  BLI_task_parallel_range(0, ss->filter_cache->totnode, &data, color_filter_task_cb, &settings);

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COLOR);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_color_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  View3D *v3d = CTX_wm_view3d(C);
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;
  if (v3d->shading.type == OB_SOLID) {
    v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
  }

  const bool use_automasking = SCULPT_is_automasking_enabled(sd, ss, NULL);
  if (use_automasking) {
    /* Update the active face set manually as the paint cursor is not enabled when using the Mesh
     * Filter Tool. */
    float mval_fl[2] = {UNPACK2(event->mval)};
    SculptCursorGeometryInfo sgi;
    SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);
  }

  /* Disable for multires and dyntopo for now */
  if (!ss->pbvh || !SCULPT_handles_colors_report(ss, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, "color filter");
  BKE_sculpt_color_layer_create_if_needed(ob);

  /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
   * earlier steps modifying the data. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, true);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES && !ob->sculpt->pmap) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_filter_cache_init(C, ob, sd, SCULPT_UNDO_COLOR);
  FilterCache *filter_cache = ss->filter_cache;
  filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  filter_cache->automasking = SCULPT_automasking_cache_init(sd, NULL, ob);
  ED_paint_tool_update_sticky_shading_color(C, ob);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_color_filter(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Filter Color";
  ot->idname = "SCULPT_OT_color_filter";
  ot->description = "Applies a filter to modify the active color attribute";

  /* api callbacks */
  ot->invoke = sculpt_color_filter_invoke;
  ot->modal = sculpt_color_filter_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  RNA_def_enum(ot->srna, "type", prop_color_filter_types, COLOR_FILTER_HUE, "Filter Type", "");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter strength", -10.0f, 10.0f);

  PropertyRNA *prop = RNA_def_float_color(
      ot->srna, "fill_color", 3, NULL, 0.0f, FLT_MAX, "Fill Color", "", 0.0f, 1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
}
