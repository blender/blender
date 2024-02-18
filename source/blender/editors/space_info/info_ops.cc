/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spinfo
 */

#include <cstdio>
#include <cstring>

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_bpath.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.h"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.h"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "info_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Pack Blend File Libraries Operator
 * \{ */

static int pack_libraries_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  BKE_packedfile_pack_all_libraries(bmain, op->reports);

  return OPERATOR_FINISHED;
}

void FILE_OT_pack_libraries(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pack Linked Libraries";
  ot->idname = "FILE_OT_pack_libraries";
  ot->description =
      "Store all data-blocks linked from other .blend files in the current .blend file. "
      "Library references are preserved so the linked data-blocks can be unpacked again";

  /* api callbacks */
  ot->exec = pack_libraries_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int unpack_libraries_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  WM_cursor_wait(true);
  BKE_packedfile_unpack_all_libraries(bmain, op->reports);
  WM_cursor_wait(false);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unpack Blend File Libraries Operator
 * \{ */

static int unpack_libraries_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_confirm_message(
      C, op, "Unpack Linked Libraries - creates directories, all new paths should work");
}

void FILE_OT_unpack_libraries(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unpack Linked Libraries";
  ot->idname = "FILE_OT_unpack_libraries";
  ot->description = "Restore all packed linked data-blocks to their original locations";

  /* api callbacks */
  ot->invoke = unpack_libraries_invoke;
  ot->exec = unpack_libraries_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Auto-Pack Operator
 * \{ */

static int autopack_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  if (G.fileflags & G_FILE_AUTOPACK) {
    G.fileflags &= ~G_FILE_AUTOPACK;
  }
  else {
    BKE_packedfile_pack_all(bmain, op->reports, true);
    G.fileflags |= G_FILE_AUTOPACK;
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_autopack_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Automatically Pack Resources";
  ot->idname = "FILE_OT_autopack_toggle";
  ot->description = "Automatically pack all external files into the .blend file";

  /* api callbacks */
  ot->exec = autopack_toggle_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack All Operator
 * \{ */

static int pack_all_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  BKE_packedfile_pack_all(bmain, op->reports, true);

  return OPERATOR_FINISHED;
}

static int pack_all_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Main *bmain = CTX_data_main(C);
  Image *ima;

  /* First check for dirty images. */
  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    if (BKE_image_is_dirty(ima)) {
      break;
    }
  }

  if (ima) {
    return WM_operator_confirm_message(
        C, op, "Some images are painted on. These changes will be lost. Continue?");
  }

  return pack_all_exec(C, op);
}

void FILE_OT_pack_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pack Resources";
  ot->idname = "FILE_OT_pack_all";
  ot->description = "Pack all used external files into this .blend";

  /* api callbacks */
  ot->exec = pack_all_exec;
  ot->invoke = pack_all_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unpack All Operator
 * \{ */

static const EnumPropertyItem unpack_all_method_items[] = {
    {PF_USE_LOCAL, "USE_LOCAL", 0, "Use files in current directory (create when necessary)", ""},
    {PF_WRITE_LOCAL,
     "WRITE_LOCAL",
     0,
     "Write files to current directory (overwrite existing files)",
     ""},
    {PF_USE_ORIGINAL,
     "USE_ORIGINAL",
     0,
     "Use files in original location (create when necessary)",
     ""},
    {PF_WRITE_ORIGINAL,
     "WRITE_ORIGINAL",
     0,
     "Write files to original location (overwrite existing files)",
     ""},
    {PF_KEEP, "KEEP", 0, "Disable auto-pack, keep all packed files", ""},
    {PF_REMOVE, "REMOVE", 0, "Remove Pack", ""},
    /* {PF_ASK, "ASK", 0, "Ask for each file", ""}, */
    {0, nullptr, 0, nullptr, nullptr},
};

static int unpack_all_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ePF_FileStatus method = ePF_FileStatus(RNA_enum_get(op->ptr, "method"));

  if (method != PF_KEEP) {
    WM_cursor_wait(true);
    BKE_packedfile_unpack_all(bmain, op->reports, method); /* XXX PF_ASK can't work here */
    WM_cursor_wait(false);
  }
  G.fileflags &= ~G_FILE_AUTOPACK;

  return OPERATOR_FINISHED;
}

static int unpack_all_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Main *bmain = CTX_data_main(C);
  uiPopupMenu *pup;
  uiLayout *layout;
  char title[64];
  int count = 0;

  count = BKE_packedfile_count_all(bmain);

  if (!count) {
    BKE_report(op->reports, RPT_WARNING, "No packed files to unpack");
    G.fileflags &= ~G_FILE_AUTOPACK;
    return OPERATOR_CANCELLED;
  }

  if (count == 1) {
    STRNCPY_UTF8(title, IFACE_("Unpack 1 File"));
  }
  else {
    SNPRINTF(title, IFACE_("Unpack %d Files"), count);
  }

  pup = UI_popup_menu_begin(C, title, ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
  uiItemsEnumO(layout, "FILE_OT_unpack_all", "method");

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void FILE_OT_unpack_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unpack Resources";
  ot->idname = "FILE_OT_unpack_all";
  ot->description = "Unpack all files packed into this .blend to external ones";

  /* api callbacks */
  ot->exec = unpack_all_exec;
  ot->invoke = unpack_all_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(
      ot->srna, "method", unpack_all_method_items, PF_USE_LOCAL, "Method", "How to unpack");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unpack Single Item Operator
 * \{ */

static const EnumPropertyItem unpack_item_method_items[] = {
    {PF_USE_LOCAL, "USE_LOCAL", 0, "Use file from current directory (create when necessary)", ""},
    {PF_WRITE_LOCAL,
     "WRITE_LOCAL",
     0,
     "Write file to current directory (overwrite existing file)",
     ""},
    {PF_USE_ORIGINAL,
     "USE_ORIGINAL",
     0,
     "Use file in original location (create when necessary)",
     ""},
    {PF_WRITE_ORIGINAL,
     "WRITE_ORIGINAL",
     0,
     "Write file to original location (overwrite existing file)",
     ""},
    /* {PF_ASK, "ASK", 0, "Ask for each file", ""}, */
    {0, nullptr, 0, nullptr, nullptr},
};

static int unpack_item_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ID *id;
  char idname[MAX_ID_NAME - 2];
  int type = RNA_int_get(op->ptr, "id_type");
  ePF_FileStatus method = ePF_FileStatus(RNA_enum_get(op->ptr, "method"));

  RNA_string_get(op->ptr, "id_name", idname);
  id = BKE_libblock_find_name(bmain, type, idname);

  if (id == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "No packed file");
    return OPERATOR_CANCELLED;
  }

  if (method != PF_KEEP) {
    WM_cursor_wait(true);
    BKE_packedfile_id_unpack(bmain, id, op->reports, method); /* XXX PF_ASK can't work here */
    WM_cursor_wait(false);
  }

  G.fileflags &= ~G_FILE_AUTOPACK;

  return OPERATOR_FINISHED;
}

static int unpack_item_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  uiPopupMenu *pup;
  uiLayout *layout;

  pup = UI_popup_menu_begin(C, IFACE_("Unpack"), ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
  uiItemsFullEnumO(layout,
                   op->type->idname,
                   "method",
                   static_cast<IDProperty *>(op->ptr->data),
                   WM_OP_EXEC_REGION_WIN,
                   UI_ITEM_NONE);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void FILE_OT_unpack_item(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unpack Item";
  ot->idname = "FILE_OT_unpack_item";
  ot->description = "Unpack this file to an external file";

  /* api callbacks */
  ot->exec = unpack_item_exec;
  ot->invoke = unpack_item_invoke;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(
      ot->srna, "method", unpack_item_method_items, PF_USE_LOCAL, "Method", "How to unpack");
  RNA_def_string(
      ot->srna, "id_name", nullptr, BKE_ST_MAXNAME, "ID Name", "Name of ID block to unpack");
  RNA_def_int(ot->srna,
              "id_type",
              ID_IM,
              0,
              INT_MAX,
              "ID Type",
              "Identifier type of ID block",
              0,
              INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Paths Relative Operator
 * \{ */

static int make_paths_relative_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);

  if (blendfile_path[0] == '\0') {
    BKE_report(op->reports, RPT_WARNING, "Cannot set relative paths with an unsaved blend file");
    return OPERATOR_CANCELLED;
  }

  BKE_bpath_relative_convert(bmain, blendfile_path, op->reports);

  /* redraw everything so any changed paths register */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

void FILE_OT_make_paths_relative(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Paths Relative";
  ot->idname = "FILE_OT_make_paths_relative";
  ot->description = "Make all paths to external files relative to current .blend";

  /* api callbacks */
  ot->exec = make_paths_relative_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Paths Absolute Operator
 * \{ */

static int make_paths_absolute_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);

  if (blendfile_path[0] == '\0') {
    BKE_report(op->reports, RPT_WARNING, "Cannot set absolute paths with an unsaved blend file");
    return OPERATOR_CANCELLED;
  }

  BKE_bpath_absolute_convert(bmain, blendfile_path, op->reports);

  /* redraw everything so any changed paths register */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

void FILE_OT_make_paths_absolute(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Paths Absolute";
  ot->idname = "FILE_OT_make_paths_absolute";
  ot->description = "Make all paths to external files absolute";

  /* api callbacks */
  ot->exec = make_paths_absolute_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Report Missing Files Operator
 * \{ */

static int report_missing_files_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  /* run the missing file check */
  BKE_bpath_missing_files_check(bmain, op->reports);

  return OPERATOR_FINISHED;
}

void FILE_OT_report_missing_files(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Report Missing Files";
  ot->idname = "FILE_OT_report_missing_files";
  ot->description = "Report all missing external files";

  /* api callbacks */
  ot->exec = report_missing_files_exec;

  /* flags */
  ot->flag = 0; /* only reports so no need to undo/register */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Missing Files Operator
 * \{ */

static int find_missing_files_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const char *searchpath = RNA_string_get_alloc(op->ptr, "directory", nullptr, 0, nullptr);
  const bool find_all = RNA_boolean_get(op->ptr, "find_all");

  BKE_bpath_missing_files_find(bmain, searchpath, op->reports, find_all);
  MEM_freeN((void *)searchpath);

  return OPERATOR_FINISHED;
}

static int find_missing_files_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* XXX file open button text "Find Missing Files" */
  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void FILE_OT_find_missing_files(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Find Missing Files";
  ot->idname = "FILE_OT_find_missing_files";
  ot->description = "Try to find missing external files";

  /* api callbacks */
  ot->exec = find_missing_files_exec;
  ot->invoke = find_missing_files_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "find_all",
                  false,
                  "Find All",
                  "Find all files in the search path (not just missing)");

  WM_operator_properties_filesel(ot,
                                 0,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Report Box Operator
 * \{ */

/* NOTE(@broken): Hard to decide whether to keep this as an operator,
 * or turn it into a hard_coded UI control feature,
 * handling TIMER events for all regions in `interface_handlers.cc`.
 * Not sure how good that is to be accessing UI data from
 * inactive regions, so use this for now. */

#define INFO_TIMEOUT 5.0f
#define ERROR_TIMEOUT 10.0f
#define FLASH_TIMEOUT 1.0f
#define COLLAPSE_TIMEOUT 0.25f

static int update_reports_display_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  ReportList *reports = CTX_wm_reports(C);
  Report *report;

  /* escape if not our timer */
  if ((reports->reporttimer == nullptr) || (reports->reporttimer != event->customdata) ||
      ((report = BKE_reports_last_displayable(reports)) == nullptr))
  {
    /* May have been deleted. */
    return OPERATOR_PASS_THROUGH;
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  ReportTimerInfo *rti = (ReportTimerInfo *)reports->reporttimer->customdata;
  const float flash_timeout = FLASH_TIMEOUT;
  bool send_notifier = false;

  const float timeout = (report->type & RPT_ERROR_ALL) ? ERROR_TIMEOUT : INFO_TIMEOUT;
  const float time_duration = float(reports->reporttimer->time_duration);

  /* clear the report display after timeout */
  if (time_duration > timeout) {
    WM_event_timer_remove(wm, nullptr, reports->reporttimer);
    reports->reporttimer = nullptr;

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, nullptr);

    return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
  }

  if (rti->widthfac == 0.0f) {
    rti->widthfac = 1.0f;
  }

  const float progress = powf(time_duration / timeout, 2.0f);
  const float flash_progress = powf(time_duration / flash_timeout, 2.0);

  /* save us from too many draws */
  if (flash_progress <= 1.0f) {
    /* Flash report briefly according to progress through fade-out duration. */
    send_notifier = true;
  }
  rti->flash_progress = flash_progress;

  /* collapse report at end of timeout */
  if (progress * timeout > timeout - COLLAPSE_TIMEOUT) {
    rti->widthfac = (progress * timeout - (timeout - COLLAPSE_TIMEOUT)) / COLLAPSE_TIMEOUT;
    rti->widthfac = 1.0f - rti->widthfac;
    send_notifier = true;
  }

  if (send_notifier) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, nullptr);
  }

  return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
}

void INFO_OT_reports_display_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Reports Display";
  ot->idname = "INFO_OT_reports_display_update";
  ot->description = "Update the display of reports in Blender UI (internal use)";

  /* api callbacks */
  ot->invoke = update_reports_display_invoke;

  /* flags */
  ot->flag = 0;

  /* properties */
}

/* report operators */

/** \} */
