/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_meshdata_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "IMB_colormanagement.h"

#include "DEG_depsgraph.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_paint.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include <cmath>
#include <cstdlib>

enum eSculptColorFilterTypes {
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
};

static const float fill_filter_default_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

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
    {0, nullptr, 0, nullptr, nullptr},
};

static void color_filter_task_cb(void *__restrict userdata,
                                 const int n,
                                 const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;

  const int mode = data->filter_type;

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COLOR);

  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->filter_cache->automasking, &automask_data, data->nodes[n]);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(ss, &orig_data, vd.vertex);
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    float orig_color[3], final_color[4], hsv_color[3];
    int hue;
    float brightness, contrast, gain, delta, offset;
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= data->filter_strength;
    fade *= SCULPT_automasking_factor_get(
        ss->filter_cache->automasking, ss, vd.vertex, &automask_data);
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
        SCULPT_neighbor_color_average(ss, smooth_color, vd.vertex);

        float col[4];
        SCULPT_vertex_color_get(ss, vd.vertex, col);

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

    SCULPT_vertex_color_set(ss, vd.vertex, final_color);
  }
  BKE_pbvh_vertex_iter_end;
  BKE_pbvh_node_mark_update_color(data->nodes[n]);
}

static void sculpt_color_presmooth_init(SculptSession *ss)
{
  int totvert = SCULPT_vertex_count_get(ss);

  if (!ss->filter_cache->pre_smoothed_color) {
    ss->filter_cache->pre_smoothed_color = static_cast<float(*)[4]>(
        MEM_malloc_arrayN(totvert, sizeof(float[4]), __func__));
  }

  for (int i = 0; i < totvert; i++) {
    SCULPT_vertex_color_get(
        ss, BKE_pbvh_index_to_vertex(ss->pbvh, i), ss->filter_cache->pre_smoothed_color[i]);
  }

  for (int iteration = 0; iteration < 2; iteration++) {
    for (int i = 0; i < totvert; i++) {
      float avg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      int total = 0;

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, BKE_pbvh_index_to_vertex(ss->pbvh, i), ni) {
        float col[4] = {0};

        copy_v4_v4(col, ss->filter_cache->pre_smoothed_color[ni.index]);

        add_v4_v4(avg, col);
        total++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (total > 0) {
        mul_v4_fl(avg, 1.0f / float(total));
        interp_v4_v4v4(ss->filter_cache->pre_smoothed_color[i],
                       ss->filter_cache->pre_smoothed_color[i],
                       avg,
                       0.5f);
      }
    }
  }
}

static void sculpt_color_filter_apply(bContext *C, wmOperator *op, Object *ob)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;

  const int mode = RNA_enum_get(op->ptr, "type");
  float filter_strength = RNA_float_get(op->ptr, "strength");
  float fill_color[3];

  RNA_float_get_array(op->ptr, "fill_color", fill_color);
  IMB_colormanagement_srgb_to_scene_linear_v3(fill_color, fill_color);

  if (filter_strength < 0.0 && !ss->filter_cache->pre_smoothed_color) {
    sculpt_color_presmooth_init(ss);
  }

  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.nodes = ss->filter_cache->nodes;
  data.filter_type = mode;
  data.filter_strength = filter_strength;
  data.filter_fill_color = fill_color;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  BKE_pbvh_parallel_range_settings(&settings, true, ss->filter_cache->nodes.size());
  BLI_task_parallel_range(
      0, ss->filter_cache->nodes.size(), &data, color_filter_task_cb, &settings);

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COLOR);
}

static void sculpt_color_filter_end(bContext *C, Object *ob)
{
  SculptSession *ss = ob->sculpt;

  SCULPT_undo_push_end(ob);
  SCULPT_filter_cache_free(ss, ob);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COLOR);
}

static int sculpt_color_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    sculpt_color_filter_end(C, ob);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = (event->prev_press_xy[0] - event->xy[0]) * 0.001f;
  float filter_strength = ss->filter_cache->start_filter_strength * -len;
  RNA_float_set(op->ptr, "strength", filter_strength);

  sculpt_color_filter_apply(C, op, ob);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_color_filter_init(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  View3D *v3d = CTX_wm_view3d(C);

  int mval[2];
  RNA_int_get_array(op->ptr, "start_mouse", mval);
  float mval_fl[2] = {float(mval[0]), float(mval[1])};

  SCULPT_stroke_id_next(ob);

  const bool use_automasking = SCULPT_is_automasking_enabled(sd, ss, nullptr);
  if (use_automasking) {
    if (v3d) {
      /* Update the active face set manually as the paint cursor is not enabled when using the Mesh
       * Filter Tool. */
      SculptCursorGeometryInfo sgi;
      SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false, false);
    }
  }

  /* Disable for multires and dyntopo for now */
  if (!ss->pbvh || !SCULPT_handles_colors_report(ss, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, op);
  BKE_sculpt_color_layer_create_if_needed(ob);

  BKE_sculpt_ensure_origcolor(ob);

  /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
   * earlier steps modifying the data. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, true);

  SCULPT_filter_cache_init(C,
                           ob,
                           sd,
                           SCULPT_UNDO_COLOR,
                           mval_fl,
                           RNA_float_get(op->ptr, "area_normal_radius"),
                           RNA_float_get(op->ptr, "strength"));
  FilterCache *filter_cache = ss->filter_cache;
  filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  filter_cache->automasking = SCULPT_automasking_cache_init(sd, nullptr, ob);

  return OPERATOR_PASS_THROUGH;
}

static int sculpt_color_filter_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);

  if (sculpt_color_filter_init(C, op) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  sculpt_color_filter_apply(C, op, ob);
  sculpt_color_filter_end(C, ob);

  return OPERATOR_FINISHED;
}

static int sculpt_color_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d && v3d->shading.type == OB_SOLID) {
    v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
  }

  RNA_int_set_array(op->ptr, "start_mouse", event->mval);

  if (sculpt_color_filter_init(C, op) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  ED_paint_tool_update_sticky_shading_color(C, ob);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static std::string sculpt_color_filter_get_name(wmOperatorType * /*ot*/, PointerRNA *ptr)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, "type");
  const int value = RNA_property_enum_get(ptr, prop);
  const char *ui_name = nullptr;

  RNA_property_enum_name_gettexted(nullptr, ptr, prop, value, &ui_name);
  return ui_name;
}

static void sculpt_color_filter_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  uiItemR(layout, op->ptr, "strength", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (RNA_enum_get(op->ptr, "type") == COLOR_FILTER_FILL) {
    uiItemR(layout, op->ptr, "fill_color", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
}

void SCULPT_OT_color_filter(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Filter Color";
  ot->idname = "SCULPT_OT_color_filter";
  ot->description = "Applies a filter to modify the active color attribute";

  /* api callbacks */
  ot->invoke = sculpt_color_filter_invoke;
  ot->exec = sculpt_color_filter_exec;
  ot->modal = sculpt_color_filter_modal;
  ot->poll = SCULPT_mode_poll;
  ot->ui = sculpt_color_filter_ui;
  ot->get_name = sculpt_color_filter_get_name;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  SCULPT_mesh_filter_properties(ot);

  RNA_def_enum(ot->srna, "type", prop_color_filter_types, COLOR_FILTER_FILL, "Filter Type", "");

  PropertyRNA *prop = RNA_def_float_color(ot->srna,
                                          "fill_color",
                                          3,
                                          fill_filter_default_color,
                                          0.0f,
                                          FLT_MAX,
                                          "Fill Color",
                                          "",
                                          0.0f,
                                          1.0f);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MESH);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
}
