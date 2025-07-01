/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edpointcloud
 * Implements the Point Cloud operators.
 */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"

#include "ED_pointcloud.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"

#include "DNA_pointcloud_types.h"
#include "DNA_windowmanager_types.h"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "WM_api.hh"

namespace blender::ed::pointcloud {

static bool object_has_editable_pointcloud(const Main &bmain, const Object &object)
{
  if (object.type != OB_POINTCLOUD) {
    return false;
  }
  if (object.mode != OB_MODE_EDIT) {
    return false;
  }
  if (!BKE_id_is_editable(&bmain, static_cast<const ID *>(object.data))) {
    return false;
  }
  return true;
}

static bool pointcloud_poll_impl(bContext *C,
                                 const bool check_editable,
                                 const bool check_edit_mode)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_POINTCLOUD) {
    return false;
  }
  if (check_editable) {
    if (!ED_operator_object_active_editable_ex(C, object)) {
      return false;
    }
  }
  if (check_edit_mode) {
    if ((object->mode & OB_MODE_EDIT) == 0) {
      return false;
    }
  }
  return true;
}

static bool editable_pointcloud_poll(bContext *C)
{
  return pointcloud_poll_impl(C, false, false);
}

bool editable_pointcloud_in_edit_mode_poll(bContext *C)
{
  return pointcloud_poll_impl(C, true, true);
}

VectorSet<PointCloud *> get_unique_editable_pointclouds(const bContext &C)
{
  VectorSet<PointCloud *> unique_points;

  const Main &bmain = *CTX_data_main(&C);

  Object *object = CTX_data_active_object(&C);
  if (object && object_has_editable_pointcloud(bmain, *object)) {
    unique_points.add_new(static_cast<PointCloud *>(object->data));
  }

  CTX_DATA_BEGIN (&C, Object *, object, selected_objects) {
    if (object_has_editable_pointcloud(bmain, *object)) {
      unique_points.add(static_cast<PointCloud *>(object->data));
    }
  }
  CTX_DATA_END;

  return unique_points;
}

static bool has_anything_selected(const Span<PointCloud *> pointclouds)
{
  return std::any_of(pointclouds.begin(), pointclouds.end(), [](const PointCloud *pointcloud) {
    return has_anything_selected(*pointcloud);
  });
}

static wmOperatorStatus select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  VectorSet<PointCloud *> unique_pointcloud = get_unique_editable_pointclouds(*C);

  if (action == SEL_TOGGLE) {
    action = has_anything_selected(unique_pointcloud) ? SEL_DESELECT : SEL_SELECT;
  }

  for (PointCloud *pointcloud : unique_pointcloud) {
    /* (De)select all the curves. */
    select_all(*pointcloud, action);

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&pointcloud->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, pointcloud);
  }

  return OPERATOR_FINISHED;
}

static void POINTCLOUD_OT_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->idname = "POINTCLOUD_OT_select_all";
  ot->description = "(De)select all point cloud";

  ot->exec = select_all_exec;
  ot->poll = editable_pointcloud_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static wmOperatorStatus select_random_exec(bContext *C, wmOperator *op)
{
  const int seed = RNA_int_get(op->ptr, "seed");
  const float probability = RNA_float_get(op->ptr, "probability");

  for (PointCloud *pointcloud : get_unique_editable_pointclouds(*C)) {
    IndexMaskMemory memory;
    const IndexMask inv_random_elements = random_mask(
                                              pointcloud->totpoint, seed, probability, memory)
                                              .complement(IndexRange(pointcloud->totpoint),
                                                          memory);
    const bool was_anything_selected = has_anything_selected(*pointcloud);
    bke::GSpanAttributeWriter selection = ensure_selection_attribute(*pointcloud,
                                                                     bke::AttrType::Bool);
    if (!was_anything_selected) {
      pointcloud::fill_selection_true(selection.span);
    }

    pointcloud::fill_selection_false(selection.span, inv_random_elements);
    selection.finish();

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&pointcloud->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, pointcloud);
  }
  return OPERATOR_FINISHED;
}

static void select_random_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  layout->prop(op->ptr, "seed", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "probability", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
}

static void POINTCLOUD_OT_select_random(wmOperatorType *ot)
{
  ot->name = "Select Random";
  ot->idname = __func__;
  ot->description = "Randomizes existing selection or create new random selection";

  ot->exec = select_random_exec;
  ot->poll = editable_pointcloud_poll;
  ot->ui = select_random_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "seed",
              0,
              INT32_MIN,
              INT32_MAX,
              "Seed",
              "Source of randomness",
              INT32_MIN,
              INT32_MAX);
  RNA_def_float(ot->srna,
                "probability",
                0.5f,
                0.0f,
                1.0f,
                "Probability",
                "Chance of every point being included in the selection",
                0.0f,
                1.0f);
}

namespace pointcloud_delete {

static wmOperatorStatus delete_exec(bContext *C, wmOperator * /*op*/)
{
  for (PointCloud *pointcloud : get_unique_editable_pointclouds(*C)) {
    if (remove_selection(*pointcloud)) {
      DEG_id_tag_update(&pointcloud->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, &pointcloud);
    }
  }

  return OPERATOR_FINISHED;
}

}  // namespace pointcloud_delete

static void POINTCLOUD_OT_delete(wmOperatorType *ot)
{
  ot->name = "Delete";
  ot->idname = __func__;
  ot->description = "Remove selected points";

  ot->exec = pointcloud_delete::delete_exec;
  ot->poll = editable_pointcloud_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void operatortypes_pointcloud()
{
  WM_operatortype_append(POINTCLOUD_OT_attribute_set);
  WM_operatortype_append(POINTCLOUD_OT_delete);
  WM_operatortype_append(POINTCLOUD_OT_duplicate);
  WM_operatortype_append(POINTCLOUD_OT_select_all);
  WM_operatortype_append(POINTCLOUD_OT_select_random);
  WM_operatortype_append(POINTCLOUD_OT_separate);
}

void operatormacros_pointcloud()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("POINTCLOUD_OT_duplicate_move",
                                    "Duplicate",
                                    "Make copies of selected elements and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "POINTCLOUD_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

void keymap_pointcloud(wmKeyConfig *keyconf)
{
  /* Only set in editmode point cloud, by space_view3d listener. */
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Point Cloud", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = editable_pointcloud_in_edit_mode_poll;
}

}  // namespace blender::ed::pointcloud
