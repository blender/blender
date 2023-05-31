/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include <math.h>
#include <stdlib.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_blender_copybuffer.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "view3d_intern.h"
#include "view3d_navigate.h"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

/* ************************** copy paste ***************************** */

static int view3d_copybuffer_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  char filepath[FILE_MAX];
  int num_copied = 0;

  BKE_copybuffer_copy_begin(bmain);

  /* context, selection, could be generalized */
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if ((ob->id.tag & LIB_TAG_DOIT) == 0) {
      BKE_copybuffer_copy_tag_ID(&ob->id);
      num_copied++;
    }
  }
  CTX_DATA_END;

  BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_base(), "copybuffer.blend");
  BKE_copybuffer_copy_end(bmain, filepath, op->reports);

  BKE_reportf(op->reports, RPT_INFO, "Copied %d selected object(s)", num_copied);

  return OPERATOR_FINISHED;
}

static void VIEW3D_OT_copybuffer(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Objects";
  ot->idname = "VIEW3D_OT_copybuffer";
  ot->description = "Copy the selected objects to the internal clipboard";

  /* api callbacks */
  ot->exec = view3d_copybuffer_exec;
  ot->poll = ED_operator_scene;
}

static int view3d_pastebuffer_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  short flag = 0;

  if (RNA_boolean_get(op->ptr, "autoselect")) {
    flag |= FILE_AUTOSELECT;
  }
  if (RNA_boolean_get(op->ptr, "active_collection")) {
    flag |= FILE_ACTIVE_COLLECTION;
  }

  BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_base(), "copybuffer.blend");

  const int num_pasted = BKE_copybuffer_paste(C, filepath, flag, op->reports, FILTER_ID_OB);
  if (num_pasted == 0) {
    BKE_report(op->reports, RPT_INFO, "No objects to paste");
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_WINDOW, NULL);
  ED_outliner_select_sync_from_object_tag(C);

  BKE_reportf(op->reports, RPT_INFO, "%d object(s) pasted", num_pasted);

  return OPERATOR_FINISHED;
}

static void VIEW3D_OT_pastebuffer(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Paste Objects";
  ot->idname = "VIEW3D_OT_pastebuffer";
  ot->description = "Paste objects from the internal clipboard";

  /* api callbacks */
  ot->exec = view3d_pastebuffer_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "autoselect", true, "Select", "Select pasted objects");
  RNA_def_boolean(ot->srna,
                  "active_collection",
                  true,
                  "Active Collection",
                  "Put pasted objects in the active collection");
}

/* ************************** registration **********************************/

void view3d_operatortypes(void)
{
  WM_operatortype_append(VIEW3D_OT_rotate);
  WM_operatortype_append(VIEW3D_OT_move);
  WM_operatortype_append(VIEW3D_OT_zoom);
  WM_operatortype_append(VIEW3D_OT_zoom_camera_1_to_1);
  WM_operatortype_append(VIEW3D_OT_dolly);
#ifdef WITH_INPUT_NDOF
  WM_operatortype_append(VIEW3D_OT_ndof_orbit_zoom);
  WM_operatortype_append(VIEW3D_OT_ndof_orbit);
  WM_operatortype_append(VIEW3D_OT_ndof_pan);
  WM_operatortype_append(VIEW3D_OT_ndof_all);
#endif /* WITH_INPUT_NDOF */
  WM_operatortype_append(VIEW3D_OT_view_all);
  WM_operatortype_append(VIEW3D_OT_view_axis);
  WM_operatortype_append(VIEW3D_OT_view_camera);
  WM_operatortype_append(VIEW3D_OT_view_orbit);
  WM_operatortype_append(VIEW3D_OT_view_roll);
  WM_operatortype_append(VIEW3D_OT_view_pan);
  WM_operatortype_append(VIEW3D_OT_view_persportho);
  WM_operatortype_append(VIEW3D_OT_background_image_add);
  WM_operatortype_append(VIEW3D_OT_background_image_remove);
  WM_operatortype_append(VIEW3D_OT_drop_world);
  WM_operatortype_append(VIEW3D_OT_view_selected);
  WM_operatortype_append(VIEW3D_OT_view_lock_clear);
  WM_operatortype_append(VIEW3D_OT_view_lock_to_active);
  WM_operatortype_append(VIEW3D_OT_view_center_cursor);
  WM_operatortype_append(VIEW3D_OT_view_center_pick);
  WM_operatortype_append(VIEW3D_OT_view_center_camera);
  WM_operatortype_append(VIEW3D_OT_view_center_lock);
  WM_operatortype_append(VIEW3D_OT_select);
  WM_operatortype_append(VIEW3D_OT_select_box);
  WM_operatortype_append(VIEW3D_OT_clip_border);
  WM_operatortype_append(VIEW3D_OT_select_circle);
  WM_operatortype_append(VIEW3D_OT_smoothview);
  WM_operatortype_append(VIEW3D_OT_render_border);
  WM_operatortype_append(VIEW3D_OT_clear_render_border);
  WM_operatortype_append(VIEW3D_OT_zoom_border);
  WM_operatortype_append(VIEW3D_OT_cursor3d);
  WM_operatortype_append(VIEW3D_OT_select_lasso);
  WM_operatortype_append(VIEW3D_OT_select_menu);
  WM_operatortype_append(VIEW3D_OT_bone_select_menu);
  WM_operatortype_append(VIEW3D_OT_camera_to_view);
  WM_operatortype_append(VIEW3D_OT_camera_to_view_selected);
  WM_operatortype_append(VIEW3D_OT_object_as_camera);
  WM_operatortype_append(VIEW3D_OT_localview);
  WM_operatortype_append(VIEW3D_OT_localview_remove_from);
  WM_operatortype_append(VIEW3D_OT_fly);
  WM_operatortype_append(VIEW3D_OT_walk);
  WM_operatortype_append(VIEW3D_OT_navigate);
  WM_operatortype_append(VIEW3D_OT_copybuffer);
  WM_operatortype_append(VIEW3D_OT_pastebuffer);

  WM_operatortype_append(VIEW3D_OT_object_mode_pie_or_toggle);

  WM_operatortype_append(VIEW3D_OT_snap_selected_to_grid);
  WM_operatortype_append(VIEW3D_OT_snap_selected_to_cursor);
  WM_operatortype_append(VIEW3D_OT_snap_selected_to_active);
  WM_operatortype_append(VIEW3D_OT_snap_cursor_to_grid);
  WM_operatortype_append(VIEW3D_OT_snap_cursor_to_center);
  WM_operatortype_append(VIEW3D_OT_snap_cursor_to_selected);
  WM_operatortype_append(VIEW3D_OT_snap_cursor_to_active);

  WM_operatortype_append(VIEW3D_OT_interactive_add);

  WM_operatortype_append(VIEW3D_OT_toggle_shading);
  WM_operatortype_append(VIEW3D_OT_toggle_xray);
  WM_operatortype_append(VIEW3D_OT_toggle_matcap_flip);

  WM_operatortype_append(VIEW3D_OT_ruler_add);
  WM_operatortype_append(VIEW3D_OT_ruler_remove);

  transform_operatortypes();
}

void view3d_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "3D View Generic", SPACE_VIEW3D, 0);

  /* only for region 3D window */
  WM_keymap_ensure(keyconf, "3D View", SPACE_VIEW3D, 0);

  fly_modal_keymap(keyconf);
  walk_modal_keymap(keyconf);
  viewrotate_modal_keymap(keyconf);
  viewmove_modal_keymap(keyconf);
  viewzoom_modal_keymap(keyconf);
  viewdolly_modal_keymap(keyconf);
  viewplace_modal_keymap(keyconf);
}
