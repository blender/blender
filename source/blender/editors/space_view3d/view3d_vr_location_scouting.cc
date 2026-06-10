/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_camera_types.h"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"

#include "WM_api.hh"

#ifdef WITH_XR_OPENXR
#  include "wm_xr.hh"
#endif

#include "view3d_intern.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Location Scouting Capture Review Operator
 *
 *  \note This operator relies on the WM XR submodule, and is thus conditionally compiled
 *        if WITH_XR_OPENXR is defined. However, to ensure keymap files continuity, the
 *        operator modal keymap is always defined, and is conditionally assigned to the
 *        operator if WITH_XR_OPENXR is defined.
 *
 * \{ */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
  CAPTURE_REVIEW_MODAL_EXIT = 1,

  CAPTURE_REVIEW_MODAL_PREV,
  CAPTURE_REVIEW_MODAL_NEXT,

  CAPTURE_REVIEW_MODAL_ADD_CAMERA,
  CAPTURE_REVIEW_MODAL_ADD_MARKER,
};

void vr_location_scouting_capture_review_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {CAPTURE_REVIEW_MODAL_EXIT, "EXIT", 0, "Exit", ""},

      {CAPTURE_REVIEW_MODAL_PREV, "PREVIOUS", 0, "Previous Capture", "Switch to previous capture"},
      {CAPTURE_REVIEW_MODAL_NEXT, "NEXT", 0, "Next Capture", "Switch to next capture"},

      {CAPTURE_REVIEW_MODAL_ADD_CAMERA,
       "ADD_CAMERA",
       0,
       "Add Camera",
       "Add Camera from current capture"},
      {CAPTURE_REVIEW_MODAL_ADD_MARKER,
       "ADD_MARKER",
       0,
       "Add Marker",
       "Add Marker from current capture"},

      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf,
                                         "View3D VR Location Scouting Capture Review Modal");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(
      keyconf, "View3D VR Location Scouting Capture Review Modal", modal_items);

  /* Assign map to operators. */
#ifdef WITH_XR_OPENXR
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_vr_location_scouting_capture_review");
#endif
}

#ifdef WITH_XR_OPENXR

struct CaptureReviewData {
  /* Context. */
  ScrArea *area;
  RegionView3D *rv3d;
  View3D *v3d;

  /* Previous camera to restore on exit. */
  Object *prev_view3d_cam_ob;
  eRegionView3D_Persp prev_view3d_persp;

  /* Fake camera object to set the View3D. */
  Object *cam_ob;
  Camera *cam_data;
};

static void location_scouting_review_draw_status(bContext *C, wmOperator *op)
{
  WorkspaceStatus status(C);

  status.opmodal(IFACE_("Exit"), op->type, CAPTURE_REVIEW_MODAL_EXIT);

  status.opmodal("", op->type, CAPTURE_REVIEW_MODAL_PREV);
  status.item(IFACE_("Previous"), ICON_NONE);

  status.opmodal("", op->type, CAPTURE_REVIEW_MODAL_NEXT);
  status.item(IFACE_("Next"), ICON_NONE);

  status.opmodal("", op->type, CAPTURE_REVIEW_MODAL_ADD_CAMERA);
  status.item(IFACE_("Add Camera"), ICON_NONE);

  status.opmodal("", op->type, CAPTURE_REVIEW_MODAL_ADD_MARKER);
  status.item(IFACE_("Add Marker"), ICON_NONE);
}

static bool vr_location_scouting_capture_review_poll(bContext *C)
{
  return !wm_xr_location_scouting_is_captures_empty(CTX_data_scene(C)) &&
         ED_operator_region_view3d_active(C);
}

/* The capture review running property is stored on the WM, registered by the Python VR add-on. */
static bool vr_location_scouting_capture_review_get_running_state(bContext *C)
{
  PointerRNA wm_ptr = RNA_id_pointer_create(&CTX_wm_manager(C)->id);
  PropertyRNA *state_prop = RNA_struct_find_property(&wm_ptr, "vr_capture_review_running");
  BLI_assert(state_prop != nullptr);

  return RNA_property_boolean_get(&wm_ptr, state_prop);
}

static void vr_location_scouting_capture_review_set_running_state(bContext *C, const bool state)
{
  PointerRNA wm_ptr = RNA_id_pointer_create(&CTX_wm_manager(C)->id);
  PropertyRNA *state_prop = RNA_struct_find_property(&wm_ptr, "vr_capture_review_running");
  BLI_assert(state_prop != nullptr);

  RNA_property_boolean_set(&wm_ptr, state_prop, state);
}

static wmOperatorStatus vr_location_scouting_capture_review_invoke(bContext *C,
                                                                   wmOperator *op,
                                                                   const wmEvent * /*event*/)
{
  /* Operator invoked while already running, toggle off. */
  if (vr_location_scouting_capture_review_get_running_state(C)) {
    /* Set the running state (stored on the WM) to false, cached by the running operator modal. */
    vr_location_scouting_capture_review_set_running_state(C, false);
    return OPERATOR_CANCELLED;
  }

  View3D *v3d;
  ARegion *region;
  ED_view3d_context_user_region(C, &v3d, &region);

  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  ScrArea *area = CTX_wm_area(C);

  if (RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM) {
    return OPERATOR_CANCELLED;
  }

  CaptureReviewData *op_data = MEM_new_zeroed<CaptureReviewData>("View3DReviewCaptureData");
  op_data->area = area;
  op_data->rv3d = rv3d;
  op_data->v3d = v3d;

  op_data->prev_view3d_cam_ob = v3d->camera;
  ED_view3d_lastview_store(rv3d);
  /* Store previous persp separately from #ED_view3d_lastview_store as setting rv3d->lpersp to
   * CAMOB is unexpected by navigation logic. */
  op_data->prev_view3d_persp = rv3d->persp;

  /* Build a fake Camera object to set on the View3D. */
  op_data->cam_ob = BKE_id_new_nomain<Object>("ReviewCaptureCamera");
  op_data->cam_ob->type = OB_CAMERA;

  /* Lock rotation to prevent user from exiting the review camera view.
   * Re-using quad-view flags. */
  rv3d->viewlock |= RV3D_LOCK_ROTATION;

  op_data->cam_data = BKE_id_new_nomain<Camera>("ReviewCaptureCameraData");
  op_data->cam_ob->data = id_cast<ID *>(op_data->cam_data);

  op->customdata = op_data;

  /* Set running state. */
  vr_location_scouting_capture_review_set_running_state(C, true);

  WM_event_add_modal_handler(C, op);
  location_scouting_review_draw_status(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void vr_location_scouting_capture_review_exit(bContext *C, wmOperator *op)
{
  CaptureReviewData *op_data = static_cast<CaptureReviewData *>(op->customdata);
  RegionView3D *rv3d = op_data->rv3d;

  /* Restore viewport, last view stored by #ED_view3d_lastview_store */
  copy_qt_qt(rv3d->viewquat, rv3d->lviewquat);
  rv3d->view = rv3d->lview;
  rv3d->view_axis_roll = rv3d->lview_axis_roll;
  rv3d->persp = op_data->prev_view3d_persp;
  rv3d->viewlock &= ~RV3D_LOCK_ROTATION;

  op_data->v3d->camera = op_data->prev_view3d_cam_ob;

  vr_location_scouting_capture_review_set_running_state(C, false);

  /* Redraw entire area (both the viewport and sidebar regions), clear status text. */
  ED_area_tag_redraw(CTX_wm_area(C));
  ED_workspace_status_text(C, nullptr);

  /* Free data. */
  BKE_id_free(nullptr, id_cast<ID *>(op_data->cam_ob));
  BKE_id_free(nullptr, id_cast<ID *>(op_data->cam_data));

  MEM_delete(op_data);
}

static void vr_location_scouting_capture_review_cancel(bContext *C, wmOperator *op)
{
  vr_location_scouting_capture_review_exit(C, op);
}

static bool vr_location_scouting_capture_review_event(bContext *C, const wmEvent *event)
{
  /* Return true if event is handled, false otherwise. */
  if (event->type != EVT_MODAL_MAP) {
    return false;
  }

  switch (event->val) {
    case CAPTURE_REVIEW_MODAL_EXIT:
      vr_location_scouting_capture_review_set_running_state(C, false);
      break;
    case CAPTURE_REVIEW_MODAL_PREV:
    case CAPTURE_REVIEW_MODAL_NEXT: {
      wmOperatorType *ot = WM_operatortype_find("VIEW3D_OT_vr_location_scouting_browse_captures",
                                                false);

      PointerRNA props_ptr = WM_operator_properties_create_ptr(ot);
      RNA_boolean_set(&props_ptr, "backward", event->val == CAPTURE_REVIEW_MODAL_PREV);

      WM_operator_name_call_ptr(C, ot, wm::OpCallContext::ExecDefault, &props_ptr, nullptr);
      WM_operator_properties_free(&props_ptr);

      break;
    }
    case CAPTURE_REVIEW_MODAL_ADD_CAMERA:
      WM_operator_name_call(C,
                            "VIEW3D_OT_vr_location_scouting_add_camera_from_capture",
                            wm::OpCallContext::ExecDefault,
                            nullptr,
                            nullptr);
      break;
    case CAPTURE_REVIEW_MODAL_ADD_MARKER:
      WM_operator_name_call(C,
                            "VIEW3D_OT_vr_location_scouting_add_marker_from_capture",
                            wm::OpCallContext::ExecDefault,
                            nullptr,
                            nullptr);
      break;
  }

  return true;
}

static wmOperatorStatus vr_location_scouting_capture_review_modal(bContext *C,
                                                                  wmOperator *op,
                                                                  const wmEvent *event)
{
  CaptureReviewData *op_data = static_cast<CaptureReviewData *>(op->customdata);

  /* Exit review if the mouse leaves the viewport area, or if the active area editor changes.
   * This prevents undefined behavior caused by the operator running without an active View3D
   * area while still allowing the operator to be non-blocking for interactive sidebar UI. */
  if ((ED_area_find_under_cursor(C, SPACE_TYPE_ANY, event->xy) != op_data->area) ||
      (op_data->area->spacetype != SPACE_VIEW3D))
  {
    vr_location_scouting_capture_review_exit(C, op);
    return OPERATOR_FINISHED;
  }

  /* Get the current capture. */
  Scene *scene = CTX_data_scene(C);
  auto capture = wm_xr_location_scouting_get_active_capture(scene);

  if (!capture.has_value()) {
    BKE_report(op->reports, RPT_INFO, "No VR captures to display, exiting capture review...");
    vr_location_scouting_capture_review_exit(C, op);
    return OPERATOR_FINISHED;
  }

  const bool event_handled = vr_location_scouting_capture_review_event(C, event);

  /* Exit requested, state set by the operator invoke from UI, or event function from keymap. */
  if (!vr_location_scouting_capture_review_get_running_state(C)) {
    vr_location_scouting_capture_review_exit(C, op);
    return OPERATOR_FINISHED;
  }

  /* Force perspective to camera. */
  op_data->rv3d->persp = RV3D_CAMOB;

  /* Set Camera data from capture. */
  op_data->cam_data->lens = capture->lens_focal;
  SET_FLAG_FROM_TEST(op_data->cam_data->dof.flag, capture->dof_enabled, CAM_DOF_ENABLED);
  op_data->cam_data->dof.aperture_fstop = capture->dof_fstop;
  op_data->cam_data->dof.focus_distance = capture->dof_distance;

  /* Camera viewport display settings, set passepartout to emphasize that the modal is enabled. */
  op_data->cam_data->flag |= CAM_SHOWPASSEPARTOUT;
  op_data->cam_data->passepartalpha = 0.99f; /* *Almost* completely opaque. */

  float capture_cam_mat[4][4];
  quat_to_mat4(capture_cam_mat, capture->orientation_quat);
  copy_v3_v3(capture_cam_mat[3], capture->position);

  BKE_object_apply_mat4(op_data->cam_ob, capture_cam_mat, false, false);
  /* Minimum eval without going through the depsgraph. */
  BKE_object_to_mat4(op_data->cam_ob, op_data->cam_ob->runtime->object_to_world.ptr());

  /* Set fake Camera object as the View3D camera. */
  op_data->v3d->camera = op_data->cam_ob;

  ED_area_tag_redraw(CTX_wm_area(C));
  location_scouting_review_draw_status(C, op);

  if (event_handled) {
    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_vr_location_scouting_capture_review(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Location Scouting Capture Review";
  ot->description = "Interactively review Location Scouting VR Captures";
  ot->idname = "VIEW3D_OT_vr_location_scouting_capture_review";

  /* Callbacks. */
  ot->invoke = vr_location_scouting_capture_review_invoke;
  ot->cancel = vr_location_scouting_capture_review_cancel;
  ot->modal = vr_location_scouting_capture_review_modal;
  ot->poll = vr_location_scouting_capture_review_poll;
}

#endif /* WITH_XR_OPENXR */

/** \} */

}  // namespace blender
