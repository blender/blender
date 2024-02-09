/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_main.hh"
#include "BKE_preview_image.hh"
#include "BKE_report.h"

#include "BLT_translation.hh"

#include "ED_asset.hh"
#include "ED_render.hh"
#include "ED_undo.hh"
#include "ED_util.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"

#include "WM_api.hh"
#include "WM_types.hh"

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
    CTX_wm_operator_poll_msg_set(C, "Can't edit external library data");
    return false;
  }
  if (ID_IS_OVERRIDE_LIBRARY(id)) {
    CTX_wm_operator_poll_msg_set(C, "Can't edit previews of overridden library data");
    return false;
  }
  if (!BKE_previewimg_id_get_p(id)) {
    CTX_wm_operator_poll_msg_set(C, "Data-block does not support previews");
    return false;
  }

  return true;
}

static ID *lib_id_load_custom_preview_id_get(bContext *C, const wmOperator *op)
{
  /* #invoke() gets the ID from context and saves it in the custom data. */
  if (op->customdata) {
    return static_cast<ID *>(op->customdata);
  }

  PointerRNA idptr = CTX_data_pointer_get(C, "id");
  return static_cast<ID *>(idptr.data);
}

static int lib_id_load_custom_preview_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];

  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_is_file(filepath)) {
    BKE_reportf(op->reports, RPT_ERROR, "File not found '%s'", filepath);
    return OPERATOR_CANCELLED;
  }

  ID *id = lib_id_load_custom_preview_id_get(C, op);
  if (!id) {
    BKE_report(
        op->reports, RPT_ERROR, "Failed to set preview: no ID in context (incorrect context?)");
    return OPERATOR_CANCELLED;
  }

  BKE_previewimg_id_custom_set(id, filepath);

  WM_event_add_notifier(C, NC_ASSET | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

/**
 * Obtain the ID from context, and spawn a File Browser to select the preview image. The
 * File Browser may re-use the Asset Browser under the cursor, and clear the file-list on
 * confirmation, leading to failure to obtain the ID at that point. So get it before spawning the
 * File Browser (store it in the operator custom data).
 */
static int lib_id_load_custom_preview_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  op->customdata = lib_id_load_custom_preview_id_get(C, op);
  return WM_operator_filesel(C, op, event);
}

static void ED_OT_lib_id_load_custom_preview(wmOperatorType *ot)
{
  ot->name = "Load Custom Preview";
  ot->description = "Choose an image to help identify the data-block visually";
  ot->idname = "ED_OT_lib_id_load_custom_preview";

  /* api callbacks */
  ot->poll = lib_id_preview_editing_poll;
  ot->exec = lib_id_load_custom_preview_exec;
  ot->invoke = lib_id_load_custom_preview_invoke;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

static bool lib_id_generate_preview_poll(bContext *C)
{
  if (!lib_id_preview_editing_poll(C)) {
    return false;
  }

  const PointerRNA idptr = CTX_data_pointer_get(C, "id");
  const ID *id = (ID *)idptr.data;
  if (GS(id->name) == ID_NT) {
    CTX_wm_operator_poll_msg_set(C, "Can't generate automatic preview for node group");
    return false;
  }

  return true;
}

static int lib_id_generate_preview_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender::ed;
  PointerRNA idptr = CTX_data_pointer_get(C, "id");
  ID *id = (ID *)idptr.data;

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  PreviewImage *preview = BKE_previewimg_id_get(id);
  if (preview) {
    BKE_previewimg_clear(preview);
  }

  UI_icon_render_id(C, nullptr, id, ICON_SIZE_PREVIEW, true);

  WM_event_add_notifier(C, NC_ASSET | NA_EDITED, nullptr);
  asset::list::storage_tag_main_data_dirty();

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_id_generate_preview(wmOperatorType *ot)
{
  ot->name = "Generate Preview";
  ot->description = "Create an automatic preview for the selected data-block";
  ot->idname = "ED_OT_lib_id_generate_preview";

  /* api callbacks */
  ot->poll = lib_id_generate_preview_poll;
  ot->exec = lib_id_generate_preview_exec;

  /* flags */
  ot->flag = OPTYPE_INTERNAL | OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool lib_id_generate_preview_from_object_poll(bContext *C)
{
  if (!lib_id_preview_editing_poll(C)) {
    return false;
  }
  if (CTX_data_active_object(C) == nullptr) {
    return false;
  }
  return true;
}

static int lib_id_generate_preview_from_object_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender::ed;
  PointerRNA idptr = CTX_data_pointer_get(C, "id");
  ID *id = (ID *)idptr.data;

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  Object *object_to_render = CTX_data_active_object(C);

  BKE_previewimg_id_free(id);
  PreviewImage *preview_image = BKE_previewimg_id_ensure(id);
  UI_icon_render_id_ex(C, nullptr, &object_to_render->id, ICON_SIZE_PREVIEW, true, preview_image);

  WM_event_add_notifier(C, NC_ASSET | NA_EDITED, nullptr);
  asset::list::storage_tag_main_data_dirty();

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_id_generate_preview_from_object(wmOperatorType *ot)
{
  ot->name = "Generate Preview from Object";
  ot->description = "Create a preview for this asset by rendering the active object";
  ot->idname = "ED_OT_lib_id_generate_preview_from_object";

  /* api callbacks */
  ot->poll = lib_id_generate_preview_from_object_poll;
  ot->exec = lib_id_generate_preview_from_object_exec;

  /* flags */
  ot->flag = OPTYPE_INTERNAL | OPTYPE_REGISTER | OPTYPE_UNDO;
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

  if (!BKE_id_is_editable(CTX_data_main(C), id) ||
      ELEM(GS(id->name), ID_GR, ID_SCE, ID_SCR, ID_TXT, ID_OB, ID_WS))
  {
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

static bool lib_id_override_editable_toggle_poll(bContext *C)
{
  const PointerRNA id_ptr = CTX_data_pointer_get_type(C, "id", &RNA_ID);
  const ID *id = static_cast<ID *>(id_ptr.data);

  return id && ID_IS_OVERRIDE_LIBRARY_REAL(id) && !ID_IS_LINKED(id);
}

static int lib_id_override_editable_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  const PointerRNA id_ptr = CTX_data_pointer_get_type(C, "id", &RNA_ID);
  ID *id = static_cast<ID *>(id_ptr.data);

  const bool is_system_override = BKE_lib_override_library_is_system_defined(bmain, id);
  if (is_system_override) {
    /* A system override is not editable. Make it an editable (non-system-defined) one. */
    id->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
  }
  else {
    /* Reset override, which makes it non-editable (i.e. a system define override). */
    BKE_lib_override_library_id_reset(bmain, id, true);

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
    WM_event_add_notifier(C, NC_WINDOW, nullptr);
  }

  WM_main_add_notifier(NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_id_override_editable_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Library Override Editable";
  ot->description = "Set if this library override data-block can be edited";
  ot->idname = "ED_OT_lib_id_override_editable_toggle";

  /* api callbacks */
  ot->poll = lib_id_override_editable_toggle_poll;
  ot->exec = lib_id_override_editable_toggle_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General editor utils.
 * \{ */

static int ed_flush_edits_exec(bContext *C, wmOperator * /*op*/)
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

void ED_operatortypes_edutils()
{
  WM_operatortype_append(ED_OT_lib_id_load_custom_preview);
  WM_operatortype_append(ED_OT_lib_id_generate_preview);
  WM_operatortype_append(ED_OT_lib_id_generate_preview_from_object);

  WM_operatortype_append(ED_OT_lib_id_fake_user_toggle);
  WM_operatortype_append(ED_OT_lib_id_unlink);
  WM_operatortype_append(ED_OT_lib_id_override_editable_toggle);

  WM_operatortype_append(ED_OT_flush_edits);

  WM_operatortype_append(ED_OT_undo);
  WM_operatortype_append(ED_OT_undo_push);
  WM_operatortype_append(ED_OT_redo);
  WM_operatortype_append(ED_OT_undo_redo);
  WM_operatortype_append(ED_OT_undo_history);
}
