/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"

#include "BKE_appdir.hh"
#include "BKE_blender_copybuffer.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_report.hh"

#include "BLO_readfile.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"

#include "view3d_intern.hh"
#include "view3d_navigate.hh"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void view3d_copybuffer_filepath_get(char filepath[FILE_MAX], size_t filepath_maxncpy)
{
  BLI_path_join(filepath, filepath_maxncpy, BKE_tempdir_base(), "copybuffer.blend");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport Copy Operator
 * \{ */

static wmOperatorStatus view3d_copybuffer_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::blendfile;

  Main *bmain = CTX_data_main(C);
  PartialWriteContext copybuffer{*bmain};

  Object *obact = CTX_data_active_object(C);
  Object *obact_copy = nullptr;

  /* context, selection, could be generalized */
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    ID *ob_id_copy = copybuffer.id_add(
        &ob->id,
        PartialWriteContext::IDAddOptions{
            (PartialWriteContext::IDAddOperations::SET_FAKE_USER |
             PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK |
             PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES)},
        nullptr);

    if (obact && (obact == ob)) {
      obact_copy = reinterpret_cast<Object *>(ob_id_copy);
    }
  }
  CTX_DATA_END;

  /* Explicitly adding an object to the copy/paste buffer _may_ add others as dependencies (e.g. a
   * parent object). So count to total amount of objects added, to get a matching number with the
   * one reported by the "paste" operation. */
  int num_copied = 0;

  /* Count & mark the active as done (when set). */
  LISTBASE_FOREACH (Object *, ob, &copybuffer.bmain.objects) {
    ob->flag &= ~OB_FLAG_ACTIVE_CLIPBOARD;
    num_copied += 1;
  }

  if (num_copied == 0) {
    BKE_report(op->reports, RPT_INFO, "No objects selected to copy");
    return OPERATOR_CANCELLED;
  }

  if (obact_copy) {
    obact_copy->flag |= OB_FLAG_ACTIVE_CLIPBOARD;
  }

  char filepath[FILE_MAX];
  view3d_copybuffer_filepath_get(filepath, sizeof(filepath));
  copybuffer.write(filepath, *op->reports);

  BKE_reportf(op->reports, RPT_INFO, "Copied %d selected object(s)", num_copied);

  return OPERATOR_FINISHED;
}

static void VIEW3D_OT_copybuffer(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Objects";
  ot->idname = "VIEW3D_OT_copybuffer";
  ot->description = "Copy the selected objects to the internal clipboard";

  /* API callbacks. */
  ot->exec = view3d_copybuffer_exec;
  ot->poll = ED_operator_scene;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport Paste Operator
 * \{ */

static wmOperatorStatus view3d_pastebuffer_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  int flag = 0;

  if (RNA_boolean_get(op->ptr, "autoselect")) {
    flag |= FILE_AUTOSELECT | BLO_LIBLINK_APPEND_SET_OB_ACTIVE_CLIPBOARD;
  }
  if (RNA_boolean_get(op->ptr, "active_collection")) {
    flag |= FILE_ACTIVE_COLLECTION;
  }

  view3d_copybuffer_filepath_get(filepath, sizeof(filepath));

  const int num_pasted = BKE_copybuffer_paste(C, filepath, flag, op->reports, FILTER_ID_OB);
  if (num_pasted == 0) {
    BKE_report(op->reports, RPT_INFO, "No objects to paste");
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
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

  /* API callbacks. */
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void view3d_operatortypes()
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
  WM_operatortype_append(VIEW3D_OT_camera_background_image_add);
  WM_operatortype_append(VIEW3D_OT_camera_background_image_remove);
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

  blender::ed::transform::transform_operatortypes();
}

void view3d_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "3D View Generic", SPACE_VIEW3D, RGN_TYPE_WINDOW);

  /* only for region 3D window */
  WM_keymap_ensure(keyconf, "3D View", SPACE_VIEW3D, RGN_TYPE_WINDOW);

  fly_modal_keymap(keyconf);
  walk_modal_keymap(keyconf);
  viewrotate_modal_keymap(keyconf);
  viewmove_modal_keymap(keyconf);
  viewzoom_modal_keymap(keyconf);
  viewdolly_modal_keymap(keyconf);
  viewplace_modal_keymap(keyconf);
}

/** \} */
