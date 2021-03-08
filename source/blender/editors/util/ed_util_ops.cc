/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edutil
 *
 * Utility operators for UI data or for the UI to use.
 */

#include <cstring>

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_fileops.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_icons.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLT_translation.h"

#include "ED_render.h"
#include "ED_undo.h"
#include "ED_util.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

/* -------------------------------------------------------------------- */
/** \name ID Previews
 * \{ */

static bool lib_id_preview_editing_poll(bContext *C)
{
  const PointerRNA idptr = CTX_data_pointer_get(C, "id");
  BLI_assert(!idptr.data || RNA_struct_is_ID(idptr.type));

  const ID *id = (ID *)idptr.data;
  if (!id) {
    return false;
  }
  if (ID_IS_LINKED(id)) {
    CTX_wm_operator_poll_msg_set(C, TIP_("Can't edit external library data"));
    return false;
  }
  if (ID_IS_OVERRIDE_LIBRARY(id)) {
    CTX_wm_operator_poll_msg_set(C, TIP_("Can't edit previews of overridden library data"));
    return false;
  }
  if (!BKE_previewimg_id_get_p(id)) {
    CTX_wm_operator_poll_msg_set(C, TIP_("Data-block does not support previews"));
    return false;
  }

  return true;
}

static int lib_id_load_custom_preview_exec(bContext *C, wmOperator *op)
{
  char path[FILE_MAX];

  RNA_string_get(op->ptr, "filepath", path);

  if (!BLI_is_file(path)) {
    BKE_reportf(op->reports, RPT_ERROR, "File not found '%s'", path);
    return OPERATOR_CANCELLED;
  }

  PointerRNA idptr = CTX_data_pointer_get(C, "id");
  ID *id = (ID *)idptr.data;

  BKE_previewimg_id_custom_set(id, path);

  WM_event_add_notifier(C, NC_ASSET | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_id_load_custom_preview(wmOperatorType *ot)
{
  ot->name = "Load Custom Preview";
  ot->description = "Choose an image to help identify the data-block visually";
  ot->idname = "ED_OT_lib_id_load_custom_preview";

  /* api callbacks */
  ot->poll = lib_id_preview_editing_poll;
  ot->exec = lib_id_load_custom_preview_exec;
  ot->invoke = WM_operator_filesel;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

static int lib_id_generate_preview_exec(bContext *C, wmOperator *UNUSED(op))
{
  PointerRNA idptr = CTX_data_pointer_get(C, "id");
  ID *id = (ID *)idptr.data;

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  PreviewImage *preview = BKE_previewimg_id_get(id);
  if (preview) {
    BKE_previewimg_clear(preview);
  }
  UI_icon_render_id(C, nullptr, id, ICON_SIZE_PREVIEW, true);

  WM_event_add_notifier(C, NC_ASSET | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_id_generate_preview(wmOperatorType *ot)
{
  ot->name = "Generate Preview";
  ot->description = "Create an automatic preview for the selected data-block";
  ot->idname = "ED_OT_lib_id_generate_preview";

  /* api callbacks */
  ot->poll = lib_id_preview_editing_poll;
  ot->exec = lib_id_generate_preview_exec;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic ID Operators
 * \{ */

static int lib_id_fake_user_toggle_exec(bContext *C, wmOperator *op)
{
  PropertyPointerRNA pprop;
  PointerRNA idptr = PointerRNA_NULL;

  UI_context_active_but_prop_get_templateID(C, &pprop.ptr, &pprop.prop);

  if (pprop.prop) {
    idptr = RNA_property_pointer_get(&pprop.ptr, pprop.prop);
  }

  if ((pprop.prop == nullptr) || RNA_pointer_is_null(&idptr) || !RNA_struct_is_ID(idptr.type)) {
    BKE_report(
        op->reports, RPT_ERROR, "Incorrect context for running data-block fake user toggling");
    return OPERATOR_CANCELLED;
  }

  ID *id = (ID *)idptr.data;

  if ((id->lib != nullptr) || (ELEM(GS(id->name), ID_GR, ID_SCE, ID_SCR, ID_TXT, ID_OB, ID_WS))) {
    BKE_report(op->reports, RPT_ERROR, "Data-block type does not support fake user");
    return OPERATOR_CANCELLED;
  }

  if (ID_FAKE_USERS(id)) {
    id_fake_user_clear(id);
  }
  else {
    id_fake_user_set(id);
  }

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_id_fake_user_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Fake User";
  ot->description = "Save this data-block even if it has no users";
  ot->idname = "ED_OT_lib_id_fake_user_toggle";

  /* api callbacks */
  ot->exec = lib_id_fake_user_toggle_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

static int lib_id_unlink_exec(bContext *C, wmOperator *op)
{
  PropertyPointerRNA pprop;
  PointerRNA idptr;

  UI_context_active_but_prop_get_templateID(C, &pprop.ptr, &pprop.prop);

  if (pprop.prop) {
    idptr = RNA_property_pointer_get(&pprop.ptr, pprop.prop);
  }

  if ((pprop.prop == nullptr) || RNA_pointer_is_null(&idptr) || !RNA_struct_is_ID(idptr.type)) {
    BKE_report(
        op->reports, RPT_ERROR, "Incorrect context for running data-block fake user toggling");
    return OPERATOR_CANCELLED;
  }

  memset(&idptr, 0, sizeof(idptr));
  RNA_property_pointer_set(&pprop.ptr, pprop.prop, idptr, nullptr);
  RNA_property_update(C, &pprop.ptr, pprop.prop);

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_id_unlink(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlink Data-Block";
  ot->description = "Remove a usage of a data-block, clearing the assignment";
  ot->idname = "ED_OT_lib_id_unlink";

  /* api callbacks */
  ot->exec = lib_id_unlink_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General editor utils.
 * \{ */

static int ed_flush_edits_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  ED_editors_flush_edits(bmain);
  return OPERATOR_FINISHED;
}

static void ED_OT_flush_edits(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flush Edits";
  ot->description = "Flush edit data from active editing modes";
  ot->idname = "ED_OT_flush_edits";

  /* api callbacks */
  ot->exec = ed_flush_edits_exec;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

void ED_operatortypes_edutils(void)
{
  WM_operatortype_append(ED_OT_lib_id_load_custom_preview);
  WM_operatortype_append(ED_OT_lib_id_generate_preview);

  WM_operatortype_append(ED_OT_lib_id_fake_user_toggle);
  WM_operatortype_append(ED_OT_lib_id_unlink);

  WM_operatortype_append(ED_OT_flush_edits);

  WM_operatortype_append(ED_OT_undo);
  WM_operatortype_append(ED_OT_undo_push);
  WM_operatortype_append(ED_OT_redo);
  WM_operatortype_append(ED_OT_undo_redo);
  WM_operatortype_append(ED_OT_undo_history);
}
