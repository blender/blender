/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"

#include "BLT_translation.hh"

#include "DNA_userdef_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "IMB_colormanagement.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_paint.hh"

#include "mesh_brush_common.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include <cmath>
#include <cstdlib>

namespace blender::ed::sculpt_paint::color {

enum class FilterType {
  Fill = 0,
  Hue,
  Saturation,
  Value,
  Brightness,
  Contrast,
  Red,
  Green,
  Blue,
  Smooth,
};

static const float fill_filter_default_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

static EnumPropertyItem prop_color_filter_types[] = {
    {int(FilterType::Fill), "FILL", 0, "Fill", "Fill with a specific color"},
    {int(FilterType::Hue), "HUE", 0, "Hue", "Change hue"},
    {int(FilterType::Saturation), "SATURATION", 0, "Saturation", "Change saturation"},
    {int(FilterType::Value), "VALUE", 0, "Value", "Change value"},
    {int(FilterType::Brightness), "BRIGHTNESS", 0, "Brightness", "Change brightness"},
    {int(FilterType::Contrast), "CONTRAST", 0, "Contrast", "Change contrast"},
    {int(FilterType::Smooth), "SMOOTH", 0, "Smooth", "Smooth colors"},
    {int(FilterType::Red), "RED", 0, "Red", "Change red channel"},
    {int(FilterType::Green), "GREEN", 0, "Green", "Change green channel"},
    {int(FilterType::Blue), "BLUE", 0, "Blue", "Change blue channel"},
    {0, nullptr, 0, nullptr, nullptr},
};

struct LocalData {
  Vector<float> factors;
  Vector<float4> colors;
  Vector<Vector<int>> vert_neighbors;
  Vector<float4> average_colors;
  Vector<float4> new_colors;
};

BLI_NOINLINE static void clamp_factors(const MutableSpan<float> factors,
                                       const float min,
                                       const float max)
{
  for (float &factor : factors) {
    factor = std::clamp(factor, min, max);
  }
}

static void color_filter_task(Object &ob,
                              const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const GroupedSpan<int> vert_to_face_map,
                              const FilterType mode,
                              const float filter_strength,
                              const float *filter_fill_color,
                              const bke::pbvh::Node &node,
                              LocalData &tls,
                              bke::GSpanAttributeWriter &color_attribute)
{
  const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
  SculptSession &ss = *ob.sculpt;

  const Span<float4> orig_colors = orig_color_data_get_mesh(ob, node);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  auto_mask::calc_vert_factors(ob, *ss.filter_cache->automasking, node, verts, factors);
  scale_factors(factors, filter_strength);

  tls.new_colors.resize(verts.size());
  const MutableSpan<float4> new_colors = tls.new_colors;

  /* Copy alpha. */
  for (const int i : verts.index_range()) {
    new_colors[i][3] = orig_colors[i][3];
  }

  switch (mode) {
    case FilterType::Fill: {
      clamp_factors(factors, 0.0f, 1.0f);
      for (const int i : verts.index_range()) {
        float fill_color_rgba[4];
        copy_v3_v3(fill_color_rgba, filter_fill_color);
        fill_color_rgba[3] = 1.0f;
        mul_v4_fl(fill_color_rgba, factors[i]);
        blend_color_mix_float(new_colors[i], orig_colors[i], fill_color_rgba);
      }
      break;
    }
    case FilterType::Hue: {
      for (const int i : verts.index_range()) {
        float3 hsv_color;
        rgb_to_hsv_v(orig_colors[i], hsv_color);
        const float hue = hsv_color[0];
        hsv_color[0] = fmod((hsv_color[0] + fabs(factors[i])) - hue, 1);
        hsv_to_rgb_v(hsv_color, new_colors[i]);
      }
      break;
    }
    case FilterType::Saturation: {
      for (const int i : verts.index_range()) {
        float3 hsv_color;
        rgb_to_hsv_v(orig_colors[i], hsv_color);

        if (hsv_color[1] > 0.001f) {
          hsv_color[1] = std::clamp(hsv_color[1] + factors[i] * hsv_color[1], 0.0f, 1.0f);
          hsv_to_rgb_v(hsv_color, new_colors[i]);
        }
        else {
          copy_v3_v3(new_colors[i], orig_colors[i]);
        }
      }
      break;
    }
    case FilterType::Value: {
      for (const int i : verts.index_range()) {
        float3 hsv_color;
        rgb_to_hsv_v(orig_colors[i], hsv_color);
        hsv_color[2] = std::clamp(hsv_color[2] + factors[i], 0.0f, 1.0f);
        hsv_to_rgb_v(hsv_color, new_colors[i]);
      }
      break;
    }
    case FilterType::Red: {
      for (const int i : verts.index_range()) {
        copy_v3_v3(new_colors[i], orig_colors[i]);
        new_colors[i][0] = std::clamp(orig_colors[i][0] + factors[i], 0.0f, 1.0f);
      }
      break;
    }
    case FilterType::Green: {
      for (const int i : verts.index_range()) {
        copy_v3_v3(new_colors[i], orig_colors[i]);
        new_colors[i][1] = std::clamp(orig_colors[i][1] + factors[i], 0.0f, 1.0f);
      }
      break;
    }
    case FilterType::Blue: {
      for (const int i : verts.index_range()) {
        copy_v3_v3(new_colors[i], orig_colors[i]);
        new_colors[i][2] = std::clamp(orig_colors[i][2] + factors[i], 0.0f, 1.0f);
      }
      break;
    }
    case FilterType::Brightness: {
      clamp_factors(factors, -1.0f, 1.0f);
      for (const int i : verts.index_range()) {
        const float brightness = factors[i];
        const float contrast = 0;
        float delta = contrast / 2.0f;
        const float gain = 1.0f - delta * 2.0f;
        delta *= -1;
        const float offset = gain * (brightness + delta);
        for (int component = 0; component < 3; component++) {
          new_colors[i][component] = std::clamp(
              gain * orig_colors[i][component] + offset, 0.0f, 1.0f);
        }
      }
      break;
    }
    case FilterType::Contrast: {
      clamp_factors(factors, -1.0f, 1.0f);
      for (const int i : verts.index_range()) {
        const float brightness = 0;
        const float contrast = factors[i];
        float delta = contrast / 2.0f;
        float gain = 1.0f - delta * 2.0f;

        float offset;
        if (contrast > 0) {
          gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
          offset = gain * (brightness - delta);
        }
        else {
          delta *= -1;
          offset = gain * (brightness + delta);
        }
        for (int component = 0; component < 3; component++) {
          new_colors[i][component] = std::clamp(
              gain * orig_colors[i][component] + offset, 0.0f, 1.0f);
        }
      }
      break;
    }
    case FilterType::Smooth: {
      clamp_factors(factors, -1.0f, 1.0f);

      tls.colors.resize(verts.size());
      const MutableSpan<float4> colors = tls.colors;
      for (const int i : verts.index_range()) {
        colors[i] = color_vert_get(faces,
                                   corner_verts,
                                   vert_to_face_map,
                                   color_attribute.span,
                                   color_attribute.domain,
                                   verts[i]);
      }

      tls.vert_neighbors.resize(verts.size());
      calc_vert_neighbors(faces, corner_verts, vert_to_face_map, {}, verts, tls.vert_neighbors);
      const Span<Vector<int>> neighbors = tls.vert_neighbors;

      tls.average_colors.resize(verts.size());
      const MutableSpan<float4> average_colors = tls.average_colors;
      smooth::neighbor_color_average(faces,
                                     corner_verts,
                                     vert_to_face_map,
                                     color_attribute.span,
                                     color_attribute.domain,
                                     neighbors,
                                     average_colors);

      for (const int i : verts.index_range()) {
        const int vert = verts[i];

        if (factors[i] < 0.0f) {
          interp_v4_v4v4(average_colors[i], average_colors[i], colors[i], 0.5f);
        }

        bool copy_alpha = colors[i][3] == average_colors[i][3];

        if (factors[i] < 0.0f) {
          float delta_color[4];

          /* Unsharp mask. */
          copy_v4_v4(delta_color, ss.filter_cache->pre_smoothed_color[vert]);
          sub_v4_v4(delta_color, average_colors[i]);

          copy_v4_v4(new_colors[i], colors[i]);
          madd_v4_v4fl(new_colors[i], delta_color, factors[i]);
        }
        else {
          blend_color_interpolate_float(new_colors[i], colors[i], average_colors[i], factors[i]);
        }

        new_colors[i] = math::clamp(new_colors[i], 0.0f, 1.0f);

        /* Prevent accumulated numeric error from corrupting alpha. */
        if (copy_alpha) {
          new_colors[i][3] = average_colors[i][3];
        }
      }
      break;
    }
  }

  for (const int i : verts.index_range()) {
    color_vert_set(faces,
                   corner_verts,
                   vert_to_face_map,
                   color_attribute.domain,
                   verts[i],
                   new_colors[i],
                   color_attribute.span);
  }
}

static void sculpt_color_presmooth_init(const Mesh &mesh, SculptSession &ss)
{
  const Span<bke::pbvh::Node *> nodes = ss.filter_cache->nodes;
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = ss.vert_to_face_map;
  const bke::GAttributeReader color_attribute = active_color_attribute(mesh);
  const GVArraySpan colors = *color_attribute;

  if (ss.filter_cache->pre_smoothed_color.is_empty()) {
    ss.filter_cache->pre_smoothed_color = Array<float4>(SCULPT_vertex_count_get(ss));
  }
  const MutableSpan<float4> pre_smoothed_color = ss.filter_cache->pre_smoothed_color;

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);
      for (const int vert : verts) {
        pre_smoothed_color[vert] = color_vert_get(
            faces, corner_verts, vert_to_face_map, colors, color_attribute.domain, vert);
      }
    }
  });

  struct LocalData {
    Vector<Vector<int>> vert_neighbors;
    Vector<float4> averaged_colors;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  for ([[maybe_unused]] const int iteration : IndexRange(2)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);

        tls.vert_neighbors.resize(verts.size());
        calc_vert_neighbors(faces, corner_verts, vert_to_face_map, {}, verts, tls.vert_neighbors);
        const Span<Vector<int>> vert_neighbors = tls.vert_neighbors;

        tls.averaged_colors.resize(verts.size());
        const MutableSpan<float4> averaged_colors = tls.averaged_colors;
        smooth::neighbor_data_average_mesh(
            pre_smoothed_color.as_span(), vert_neighbors, averaged_colors);

        for (const int i : verts.index_range()) {
          pre_smoothed_color[verts[i]] = math::interpolate(
              pre_smoothed_color[verts[i]], averaged_colors[i], 0.5f);
        }
      }
    });
  }
}

static void sculpt_color_filter_apply(bContext *C, wmOperator *op, Object &ob)
{
  SculptSession &ss = *ob.sculpt;

  const FilterType mode = FilterType(RNA_enum_get(op->ptr, "type"));
  float filter_strength = RNA_float_get(op->ptr, "strength");
  float fill_color[3];

  RNA_float_get_array(op->ptr, "fill_color", fill_color);
  IMB_colormanagement_srgb_to_scene_linear_v3(fill_color, fill_color);

  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  if (filter_strength < 0.0 && ss.filter_cache->pre_smoothed_color.is_empty()) {
    sculpt_color_presmooth_init(mesh, ss);
  }

  const Span<bke::pbvh::Node *> nodes = ss.filter_cache->nodes;

  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = ss.vert_to_face_map;
  bke::GSpanAttributeWriter color_attribute = active_color_attribute_for_write(mesh);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    LocalData &tls = all_tls.local();
    for (const int i : range) {
      color_filter_task(ob,
                        faces,
                        corner_verts,
                        vert_to_face_map,
                        mode,
                        filter_strength,
                        fill_color,
                        *nodes[i],
                        tls,
                        color_attribute);
      BKE_pbvh_node_mark_update_color(nodes[i]);
    }
  });
  color_attribute.finish();
  flush_update_step(C, UpdateType::Color);
}

static void sculpt_color_filter_end(bContext *C, Object &ob)
{
  SculptSession &ss = *ob.sculpt;

  undo::push_end(ob);
  MEM_delete(ss.filter_cache);
  ss.filter_cache = nullptr;
  flush_update_done(C, ob, UpdateType::Color);
}

static int sculpt_color_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    sculpt_color_filter_end(C, ob);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = (event->prev_press_xy[0] - event->xy[0]) * 0.001f;
  float filter_strength = ss.filter_cache->start_filter_strength * -len;
  RNA_float_set(op->ptr, "strength", filter_strength);

  sculpt_color_filter_apply(C, op, ob);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_color_filter_init(bContext *C, wmOperator *op)
{
  Object &ob = *CTX_data_active_object(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  SculptSession &ss = *ob.sculpt;
  View3D *v3d = CTX_wm_view3d(C);

  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  int mval[2];
  RNA_int_get_array(op->ptr, "start_mouse", mval);
  float mval_fl[2] = {float(mval[0]), float(mval[1])};

  const bool use_automasking = auto_mask::is_enabled(sd, &ss, nullptr);
  if (use_automasking) {
    /* Increment stroke id for auto-masking system. */
    SCULPT_stroke_id_next(ob);

    if (v3d) {
      /* Update the active face set manually as the paint cursor is not enabled when using the Mesh
       * Filter Tool. */
      SculptCursorGeometryInfo sgi;
      SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);
    }
  }

  /* Disable for multires and dyntopo for now */
  if (!ss.pbvh || !SCULPT_handles_colors_report(ss, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  undo::push_begin(ob, op);
  BKE_sculpt_color_layer_create_if_needed(&ob);

  /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
   * earlier steps modifying the data. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(depsgraph, &ob, true);

  filter::cache_init(C,
                     ob,
                     sd,
                     undo::Type::Color,
                     mval_fl,
                     RNA_float_get(op->ptr, "area_normal_radius"),
                     RNA_float_get(op->ptr, "strength"));
  filter::Cache *filter_cache = ss.filter_cache;
  filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  filter_cache->automasking = auto_mask::cache_init(sd, ob);

  return OPERATOR_PASS_THROUGH;
}

static int sculpt_color_filter_exec(bContext *C, wmOperator *op)
{
  Object &ob = *CTX_data_active_object(C);

  if (sculpt_color_filter_init(C, op) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  sculpt_color_filter_apply(C, op, ob);
  sculpt_color_filter_end(C, ob);

  return OPERATOR_FINISHED;
}

static int sculpt_color_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d && v3d->shading.type == OB_SOLID) {
    v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
  }

  RNA_int_set_array(op->ptr, "start_mouse", event->mval);

  if (sculpt_color_filter_init(C, op) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  ED_paint_tool_update_sticky_shading_color(C, &ob);

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

  if (FilterType(RNA_enum_get(op->ptr, "type")) == FilterType::Fill) {
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
  filter::register_operator_props(ot);

  RNA_def_enum(
      ot->srna, "type", prop_color_filter_types, int(FilterType::Fill), "Filter Type", "");

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

}  // namespace blender::ed::sculpt_paint::color
