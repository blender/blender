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

#include "ED_point_cloud.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"

#include "DNA_pointcloud_types.h"
#include "DNA_windowmanager_types.h"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"

#include "WM_api.hh"

namespace blender::ed::point_cloud {

static bool object_has_editable_point_cloud(const Main &bmain, const Object &object)
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

static bool point_cloud_poll_impl(bContext *C,
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

static bool editable_point_cloud_poll(bContext *C)
{
  return point_cloud_poll_impl(C, false, false);
}

bool editable_point_cloud_in_edit_mode_poll(bContext *C)
{
  return point_cloud_poll_impl(C, true, true);
}

VectorSet<PointCloud *> get_unique_editable_point_clouds(const bContext &C)
{
  VectorSet<PointCloud *> unique_points;

  const Main &bmain = *CTX_data_main(&C);

  Object *object = CTX_data_active_object(&C);
  if (object && object_has_editable_point_cloud(bmain, *object)) {
    unique_points.add_new(static_cast<PointCloud *>(object->data));
  }

  CTX_DATA_BEGIN (&C, Object *, object, selected_objects) {
    if (object_has_editable_point_cloud(bmain, *object)) {
      unique_points.add(static_cast<PointCloud *>(object->data));
    }
  }
  CTX_DATA_END;

  return unique_points;
}

static bool has_anything_selected(const Span<PointCloud *> point_cloud_ids)
{
  return std::any_of(
      point_cloud_ids.begin(), point_cloud_ids.end(), [](const PointCloud *point_cloud_id) {
        return has_anything_selected(*point_cloud_id);
      });
}

static int select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  VectorSet<PointCloud *> unique_point_cloud = get_unique_editable_point_clouds(*C);

  if (action == SEL_TOGGLE) {
    action = has_anything_selected(unique_point_cloud) ? SEL_DESELECT : SEL_SELECT;
  }

  for (PointCloud *point_cloud_id : unique_point_cloud) {
    /* (De)select all the curves. */
    select_all(*point_cloud_id, action);

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&point_cloud_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, point_cloud_id);
  }

  return OPERATOR_FINISHED;
}

static void POINT_CLOUD_OT_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->idname = "POINT_CLOUD_OT_select_all";
  ot->description = "(De)select all point cloud";

  ot->exec = select_all_exec;
  ot->poll = editable_point_cloud_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

void operatortypes_point_cloud()
{
  WM_operatortype_append(POINT_CLOUD_OT_attribute_set);
  WM_operatortype_append(POINT_CLOUD_OT_select_all);
}

void keymap_point_cloud(wmKeyConfig *keyconf)
{
  /* Only set in editmode point cloud, by space_view3d listener. */
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Point Cloud", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = editable_point_cloud_in_edit_mode_poll;
}

}  // namespace blender::ed::point_cloud
