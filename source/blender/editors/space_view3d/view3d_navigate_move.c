/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BKE_context.h"

#include "WM_api.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_screen.h"

#include "view3d_intern.h"
#include "view3d_navigate.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Move (Pan) Operator
 * \{ */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */

void viewmove_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_SWITCH_ZOOM, "SWITCH_TO_ZOOM", 0, "Switch to Zoom"},
      {VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "View3D Move Modal");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "View3D Move Modal", modal_items);

  /* items for modal map */

  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = MIDDLEMOUSE,
                              .value = KM_RELEASE,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEW_MODAL_CONFIRM);
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = EVT_ESCKEY,
                              .value = KM_PRESS,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEW_MODAL_CONFIRM);

  /* disabled mode switching for now, can re-implement better, later on */
#if 0
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = LEFTMOUSE,
                              .value = KM_PRESS,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = EVT_LEFTCTRLKEY,
                              .value = KM_PRESS,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = EVT_LEFTSHIFTKEY,
                              .value = KM_RELEASE,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_ROTATE);
#endif

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_move");
}

static int viewmove_modal(bContext *C, wmOperator *op, const wmEvent *event)
{

  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ZOOM:
        WM_operator_name_call(C, "VIEW3D_OT_zoom", WM_OP_INVOKE_DEFAULT, NULL, event);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ROTATE:
        WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL, event);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    viewmove_apply(vod, event->xy[0], event->xy[1]);
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
  }

  if (ret & OPERATOR_FINISHED) {
    ED_view3d_camera_lock_undo_push(op->type->name, vod->v3d, vod->rv3d, C);
    viewops_data_free(C, op->customdata);
    op->customdata = NULL;
  }

  return ret;
}

static int viewmove_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  vod = op->customdata = viewops_data_create(
      C,
      event,
      (viewops_flag_from_prefs() & ~VIEWOPS_FLAG_ORBIT_SELECT) |
          (use_cursor_init ? VIEWOPS_FLAG_USE_MOUSE_INIT : 0));
  vod = op->customdata;

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->region);

  if (event->type == MOUSEPAN) {
    /* invert it, trackpad scroll follows same principle as 2d windows this way */
    viewmove_apply(
        vod, 2 * event->xy[0] - event->prev_xy[0], 2 * event->xy[1] - event->prev_xy[1]);

    viewops_data_free(C, op->customdata);
    op->customdata = NULL;

    return OPERATOR_FINISHED;
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void viewmove_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op->customdata);
  op->customdata = NULL;
}

void VIEW3D_OT_move(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Pan View";
  ot->description = "Move the view";
  ot->idname = "VIEW3D_OT_move";

  /* api callbacks */
  ot->invoke = viewmove_invoke;
  ot->modal = viewmove_modal;
  ot->poll = view3d_location_poll;
  ot->cancel = viewmove_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */
