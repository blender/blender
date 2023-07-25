/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array.hh"
#include "BLI_function_ref.hh"
#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_vector.hh"

#include "BKE_attribute_math.hh"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.hh"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"

#include "paint_intern.hh" /* own include */
#include "sculpt_intern.hh"

using blender::Array;
using blender::ColorGeometry4f;
using blender::FunctionRef;
using blender::GMutableSpan;
using blender::IndexMask;
using blender::IndexMaskMemory;
using blender::IndexRange;
using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Internal Utility Functions
 * \{ */

static bool vertex_weight_paint_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *me = BKE_mesh_from_object(ob);
  return (ob && ELEM(ob->mode, OB_MODE_VERTEX_PAINT, OB_MODE_WEIGHT_PAINT)) &&
         (me && me->faces_num && !me->deform_verts().is_empty());
}

static void tag_object_after_update(Object *object)
{
  BLI_assert(object->type == OB_MESH);
  Mesh *mesh = static_cast<Mesh *>(object->data);
  DEG_id_tag_update(&mesh->id, ID_RECALC_COPY_ON_WRITE);
  /* NOTE: Original mesh is used for display, so tag it directly here. */
  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Color from Weight Operator
 * \{ */

static bool vertex_paint_from_weight(Object *ob)
{
  using namespace blender;

  Mesh *me;
  if ((me = BKE_mesh_from_object(ob)) == nullptr || ED_mesh_color_ensure(me, nullptr) == false) {
    return false;
  }

  if (!me->attributes().contains(me->active_color_attribute)) {
    BLI_assert_unreachable();
    return false;
  }

  const int active_vertex_group_index = me->vertex_group_active_index - 1;
  const bDeformGroup *deform_group = static_cast<const bDeformGroup *>(
      BLI_findlink(&me->vertex_group_names, active_vertex_group_index));
  if (deform_group == nullptr) {
    BLI_assert_unreachable();
    return false;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();

  bke::GAttributeWriter color_attribute = attributes.lookup_for_write(me->active_color_attribute);
  if (!color_attribute) {
    BLI_assert_unreachable();
    return false;
  }

  /* Retrieve the vertex group with the domain and type of the existing color
   * attribute, in order to let the attribute API handle both conversions. */
  const GVArray vertex_group = *attributes.lookup(
      deform_group->name,
      ATTR_DOMAIN_POINT,
      bke::cpp_type_to_custom_data_type(color_attribute.varray.type()));
  if (!vertex_group) {
    BLI_assert_unreachable();
    return false;
  }

  GVArraySpan interpolated{
      attributes.adapt_domain(vertex_group, ATTR_DOMAIN_POINT, color_attribute.domain)};

  color_attribute.varray.set_all(interpolated.data());
  color_attribute.finish();
  tag_object_after_update(ob);

  return true;
}

static int vertex_paint_from_weight_exec(bContext *C, wmOperator * /*op*/)
{
  Object *obact = CTX_data_active_object(C);
  if (vertex_paint_from_weight(obact)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_from_weight(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Color from Weight";
  ot->idname = "PAINT_OT_vertex_color_from_weight";
  ot->description = "Convert active weight into gray scale vertex colors";

  /* api callback */
  ot->exec = vertex_paint_from_weight_exec;
  ot->poll = vertex_weight_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* TODO: invert, alpha */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Vertex Colors Operator
 * \{ */

static IndexMask get_selected_indices(const Mesh &mesh,
                                      const eAttrDomain domain,
                                      IndexMaskMemory &memory)
{
  using namespace blender;
  const bke::AttributeAccessor attributes = mesh.attributes();

  if (mesh.editflag & ME_EDIT_PAINT_FACE_SEL) {
    const VArray<bool> selection = *attributes.lookup_or_default<bool>(
        ".select_poly", domain, false);
    return IndexMask::from_bools(selection, memory);
  }
  if (mesh.editflag & ME_EDIT_PAINT_VERT_SEL) {
    const VArray<bool> selection = *attributes.lookup_or_default<bool>(
        ".select_vert", domain, false);
    return IndexMask::from_bools(selection, memory);
  }
  return IndexMask(attributes.domain_size(domain));
}

static void face_corner_color_equalize_verts(Mesh &mesh, const IndexMask selection)
{
  using namespace blender;
  const StringRef name = mesh.active_color_attribute;
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(name);
  if (!attribute) {
    BLI_assert_unreachable();
    return;
  }
  if (attribute.domain == ATTR_DOMAIN_POINT) {
    return;
  }

  GVArray color_attribute_point = *attributes.lookup(name, ATTR_DOMAIN_POINT);
  GVArray color_attribute_corner = attributes.adapt_domain(
      color_attribute_point, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER);
  color_attribute_corner.materialize(selection, attribute.span.data());
  attribute.finish();
}

static bool vertex_color_smooth(Object *ob)
{
  Mesh *me;
  if (((me = BKE_mesh_from_object(ob)) == nullptr) || (ED_mesh_color_ensure(me, nullptr) == false))
  {
    return false;
  }

  IndexMaskMemory memory;
  const IndexMask selection = get_selected_indices(*me, ATTR_DOMAIN_CORNER, memory);

  face_corner_color_equalize_verts(*me, selection);

  tag_object_after_update(ob);

  return true;
}

static int vertex_color_smooth_exec(bContext *C, wmOperator * /*op*/)
{
  Object *obact = CTX_data_active_object(C);
  if (vertex_color_smooth(obact)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Vertex Colors";
  ot->idname = "PAINT_OT_vertex_color_smooth";
  ot->description = "Smooth colors across vertices";

  /* api callbacks */
  ot->exec = vertex_color_smooth_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Color Transformation Operators
 * \{ */

static void transform_active_color_data(
    Mesh &mesh, const FunctionRef<void(ColorGeometry4f &color)> transform_fn)
{
  using namespace blender;
  const StringRef name = mesh.active_color_attribute;
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (!attributes.contains(name)) {
    BLI_assert_unreachable();
    return;
  }

  bke::GAttributeWriter color_attribute = attributes.lookup_for_write(name);
  if (!color_attribute) {
    BLI_assert_unreachable();
    return;
  }

  IndexMaskMemory memory;
  const IndexMask selection = get_selected_indices(mesh, color_attribute.domain, memory);

  selection.foreach_segment(GrainSize(1024), [&](const IndexMaskSegment segment) {
    color_attribute.varray.type().to_static_type_tag<ColorGeometry4f, ColorGeometry4b>(
        [&](auto type_tag) {
          using namespace blender;
          using T = typename decltype(type_tag)::type;
          for ([[maybe_unused]] const int i : segment) {
            if constexpr (std::is_void_v<T>) {
              BLI_assert_unreachable();
            }
            else if constexpr (std::is_same_v<T, ColorGeometry4f>) {
              ColorGeometry4f color = color_attribute.varray.get<ColorGeometry4f>(i);
              transform_fn(color);
              color_attribute.varray.set_by_copy(i, &color);
            }
            else if constexpr (std::is_same_v<T, ColorGeometry4b>) {
              ColorGeometry4f color = color_attribute.varray.get<ColorGeometry4b>(i).decode();
              transform_fn(color);
              ColorGeometry4b color_encoded = color.encode();
              color_attribute.varray.set_by_copy(i, &color_encoded);
            }
          }
        });
  });

  color_attribute.finish();

  DEG_id_tag_update(&mesh.id, 0);
}

static void transform_active_color(bContext *C,
                                   wmOperator *op,
                                   const FunctionRef<void(ColorGeometry4f &color)> transform_fn)
{
  Object *obact = CTX_data_active_object(C);

  /* Ensure valid sculpt state. */
  BKE_sculpt_update_object_for_edit(
      CTX_data_ensure_evaluated_depsgraph(C), obact, true, false, true);

  SCULPT_undo_push_begin(obact, op);

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(
      obact->sculpt->pbvh, nullptr, nullptr);
  for (PBVHNode *node : nodes) {
    SCULPT_undo_push_node(obact, node, SCULPT_UNDO_COLOR);
  }

  transform_active_color_data(*BKE_mesh_from_object(obact), transform_fn);

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_color(node);
  }

  SCULPT_undo_push_end(obact);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
}

static int vertex_color_brightness_contrast_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);

  float gain, offset;
  {
    float brightness = RNA_float_get(op->ptr, "brightness");
    float contrast = RNA_float_get(op->ptr, "contrast");
    brightness /= 100.0f;
    float delta = contrast / 200.0f;
    /*
     * The algorithm is by Werner D. Streidt
     * (http://visca.com/ffactory/archives/5-99/msg00021.html)
     * Extracted of OpenCV `demhist.c`.
     */
    if (contrast > 0) {
      gain = 1.0f - delta * 2.0f;
      gain = 1.0f / max_ff(gain, FLT_EPSILON);
      offset = gain * (brightness - delta);
    }
    else {
      delta *= -1;
      gain = max_ff(1.0f - delta * 2.0f, 0.0f);
      offset = gain * brightness + delta;
    }
  }

  Mesh *me;
  if (((me = BKE_mesh_from_object(obact)) == nullptr) ||
      (ED_mesh_color_ensure(me, nullptr) == false))
  {
    return OPERATOR_CANCELLED;
  }

  transform_active_color(C, op, [&](ColorGeometry4f &color) {
    for (int i = 0; i < 3; i++) {
      color[i] = gain * color[i] + offset;
    }
  });

  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_color_brightness_contrast(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Vertex Paint Brightness/Contrast";
  ot->idname = "PAINT_OT_vertex_color_brightness_contrast";
  ot->description = "Adjust vertex color brightness/contrast";

  /* api callbacks */
  ot->exec = vertex_color_brightness_contrast_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  const float min = -100, max = +100;
  prop = RNA_def_float(ot->srna, "brightness", 0.0f, min, max, "Brightness", "", min, max);
  prop = RNA_def_float(ot->srna, "contrast", 0.0f, min, max, "Contrast", "", min, max);
  RNA_def_property_ui_range(prop, min, max, 1, 1);
}

static int vertex_color_hsv_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);

  const float hue = RNA_float_get(op->ptr, "h");
  const float sat = RNA_float_get(op->ptr, "s");
  const float val = RNA_float_get(op->ptr, "v");

  Mesh *me;
  if (((me = BKE_mesh_from_object(obact)) == nullptr) ||
      (ED_mesh_color_ensure(me, nullptr) == false))
  {
    return OPERATOR_CANCELLED;
  }

  transform_active_color(C, op, [&](ColorGeometry4f &color) {
    float hsv[3];
    rgb_to_hsv_v(color, hsv);

    hsv[0] += (hue - 0.5f);
    if (hsv[0] > 1.0f) {
      hsv[0] -= 1.0f;
    }
    else if (hsv[0] < 0.0f) {
      hsv[0] += 1.0f;
    }
    hsv[1] *= sat;
    hsv[2] *= val;

    hsv_to_rgb_v(hsv, color);
  });

  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_color_hsv(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Hue/Saturation/Value";
  ot->idname = "PAINT_OT_vertex_color_hsv";
  ot->description = "Adjust vertex color Hue/Saturation/Value";

  /* api callbacks */
  ot->exec = vertex_color_hsv_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  RNA_def_float(ot->srna, "h", 0.5f, 0.0f, 1.0f, "Hue", "", 0.0f, 1.0f);
  RNA_def_float(ot->srna, "s", 1.0f, 0.0f, 2.0f, "Saturation", "", 0.0f, 2.0f);
  RNA_def_float(ot->srna, "v", 1.0f, 0.0f, 2.0f, "Value", "", 0.0f, 2.0f);
}

static int vertex_color_invert_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);

  Mesh *me;
  if (((me = BKE_mesh_from_object(obact)) == nullptr) ||
      (ED_mesh_color_ensure(me, nullptr) == false))
  {
    return OPERATOR_CANCELLED;
  }

  transform_active_color(C, op, [&](ColorGeometry4f &color) {
    for (int i = 0; i < 3; i++) {
      color[i] = 1.0f - color[i];
    }
  });

  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_color_invert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Invert";
  ot->idname = "PAINT_OT_vertex_color_invert";
  ot->description = "Invert RGB values";

  /* api callbacks */
  ot->exec = vertex_color_invert_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_color_levels_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);

  const float gain = RNA_float_get(op->ptr, "gain");
  const float offset = RNA_float_get(op->ptr, "offset");

  Mesh *me;
  if (((me = BKE_mesh_from_object(obact)) == nullptr) ||
      (ED_mesh_color_ensure(me, nullptr) == false))
  {
    return OPERATOR_CANCELLED;
  }

  transform_active_color(C, op, [&](ColorGeometry4f &color) {
    for (int i = 0; i < 3; i++) {
      color[i] = gain * (color[i] + offset);
    }
  });

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);

  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_color_levels(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Levels";
  ot->idname = "PAINT_OT_vertex_color_levels";
  ot->description = "Adjust levels of vertex colors";

  /* api callbacks */
  ot->exec = vertex_color_levels_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  RNA_def_float(
      ot->srna, "offset", 0.0f, -1.0f, 1.0f, "Offset", "Value to add to colors", -1.0f, 1.0f);
  RNA_def_float(
      ot->srna, "gain", 1.0f, 0.0f, FLT_MAX, "Gain", "Value to multiply colors by", 0.0f, 10.0f);
}

/** \} */
