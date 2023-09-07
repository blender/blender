/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "buttons_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Start / Clear Search Filter Operators
 *
 * \note Almost a duplicate of the file browser operator #FILE_OT_start_filter.
 * \{ */

static int buttons_start_filter_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceProperties *space = CTX_wm_space_properties(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_HEADER);

  UI_textbutton_activate_rna(C, region, space, "search_filter");

  return OPERATOR_FINISHED;
}

void BUTTONS_OT_start_filter(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Filter";
  ot->description = "Start entering filter text";
  ot->idname = "BUTTONS_OT_start_filter";

  /* Callbacks. */
  ot->exec = buttons_start_filter_exec;
  ot->poll = ED_operator_buttons_active;
}

static int buttons_clear_filter_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceProperties *space = CTX_wm_space_properties(C);

  space->runtime->search_string[0] = '\0';

  ScrArea *area = CTX_wm_area(C);
  ED_region_search_filter_update(area, CTX_wm_region(C));
  ED_area_tag_redraw(area);

  return OPERATOR_FINISHED;
}

void BUTTONS_OT_clear_filter(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Clear Filter";
  ot->description = "Clear the search filter";
  ot->idname = "BUTTONS_OT_clear_filter";

  /* Callbacks. */
  ot->exec = buttons_clear_filter_exec;
  ot->poll = ED_operator_buttons_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pin ID Operator
 * \{ */

static int toggle_pin_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceProperties *sbuts = CTX_wm_space_properties(C);

  sbuts->flag ^= SB_PIN_CONTEXT;

  /* Create the properties space pointer. */
  bScreen *screen = CTX_wm_screen(C);
  PointerRNA sbuts_ptr = RNA_pointer_create(&screen->id, &RNA_SpaceProperties, sbuts);

  /* Create the new ID pointer and set the pin ID with RNA
   * so we can use the property's RNA update functionality. */
  ID *new_id = (sbuts->flag & SB_PIN_CONTEXT) ? buttons_context_id_path(C) : nullptr;
  PointerRNA new_id_ptr = RNA_id_pointer_create(new_id);
  RNA_pointer_set(&sbuts_ptr, "pin_id", new_id_ptr);

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void BUTTONS_OT_toggle_pin(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Toggle Pin ID";
  ot->description = "Keep the current data-block displayed";
  ot->idname = "BUTTONS_OT_toggle_pin";

  /* Callbacks. */
  ot->exec = toggle_pin_exec;
  ot->poll = ED_operator_buttons_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Menu Operator
 * \{ */

static int context_menu_invoke(bContext *C, wmOperator * /*op*/, const wmEvent * /*event*/)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Context Menu"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  uiItemM(layout, "INFO_MT_area", nullptr, ICON_NONE);
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void BUTTONS_OT_context_menu(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Context Menu";
  ot->description = "Display properties editor context_menu";
  ot->idname = "BUTTONS_OT_context_menu";

  /* Callbacks. */
  ot->invoke = context_menu_invoke;
  ot->poll = ED_operator_buttons_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Browse Operator
 * \{ */

struct FileBrowseOp {
  PointerRNA ptr;
  PropertyRNA *prop;
  bool is_undo;
  bool is_userdef;
};

static int file_browse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  FileBrowseOp *fbo = static_cast<FileBrowseOp *>(op->customdata);
  ID *id;
  char *path;
  int path_len;
  const char *path_prop = RNA_struct_find_property(op->ptr, "directory") ? "directory" :
                                                                           "filepath";

  if (RNA_struct_property_is_set(op->ptr, path_prop) == 0 || fbo == nullptr) {
    return OPERATOR_CANCELLED;
  }

  path = RNA_string_get_alloc(op->ptr, path_prop, nullptr, 0, &path_len);

  /* Add slash for directories, important for some properties. */
  if (RNA_property_subtype(fbo->prop) == PROP_DIRPATH) {
    char path_buf[FILE_MAX];
    const bool is_relative = RNA_boolean_get(op->ptr, "relative_path");
    id = fbo->ptr.owner_id;

    STRNCPY(path_buf, path);
    BLI_path_abs(path_buf, id ? ID_BLEND_PATH(bmain, id) : BKE_main_blendfile_path(bmain));

    if (BLI_is_dir(path_buf)) {
      /* Do this first so '//' isn't converted to '//\' on windows. */
      BLI_path_slash_ensure(path_buf, sizeof(path_buf));
      if (is_relative) {
        BLI_path_rel(path_buf, BKE_main_blendfile_path(bmain));
        path_len = strlen(path_buf);
        path = static_cast<char *>(MEM_reallocN(path, path_len + 1));
        memcpy(path, path_buf, path_len + 1);
      }
      else {
        path = static_cast<char *>(MEM_reallocN(path, path_len + 1));
      }
    }
    else {
      char *const lslash = (char *)BLI_path_slash_rfind(path);
      if (lslash) {
        lslash[1] = '\0';
      }
    }
  }

  RNA_property_string_set(&fbo->ptr, fbo->prop, path);
  RNA_property_update(C, &fbo->ptr, fbo->prop);
  MEM_freeN(path);

  if (fbo->is_undo) {
    const char *undostr = RNA_property_identifier(fbo->prop);
    ED_undo_push(C, undostr);
  }

  /* Special annoying exception, filesel on redo panel #26618. */
  {
    wmOperator *redo_op = WM_operator_last_redo(C);
    if (redo_op) {
      if (fbo->ptr.data == redo_op->ptr->data) {
        ED_undo_operator_repeat(C, redo_op);
      }
    }
  }

  /* Tag user preferences as dirty. */
  if (fbo->is_userdef) {
    U.runtime.is_dirty = true;
  }

  MEM_freeN(op->customdata);

  return OPERATOR_FINISHED;
}

static void file_browse_cancel(bContext * /*C*/, wmOperator *op)
{
  MEM_freeN(op->customdata);
  op->customdata = nullptr;
}

static int file_browse_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  bool is_undo;
  bool is_userdef;
  FileBrowseOp *fbo;
  char *path;

  const SpaceFile *sfile = CTX_wm_space_file(C);
  if (sfile && sfile->op) {
    BKE_report(op->reports, RPT_ERROR, "Cannot activate a file selector dialog, one already open");
    return OPERATOR_CANCELLED;
  }

  UI_context_active_but_prop_get_filebrowser(C, &ptr, &prop, &is_undo, &is_userdef);

  if (!prop) {
    return OPERATOR_CANCELLED;
  }

  path = RNA_property_string_get_alloc(&ptr, prop, nullptr, 0, nullptr);

  /* Useful yet irritating feature, Shift+Click to open the file
   * Alt+Click to browse a folder in the OS's browser. */
  if (event->modifier & (KM_SHIFT | KM_ALT)) {
    wmOperatorType *ot = WM_operatortype_find("WM_OT_path_open", true);
    PointerRNA props_ptr;

    if (event->modifier & KM_ALT) {
      char *lslash = (char *)BLI_path_slash_rfind(path);
      if (lslash) {
        *lslash = '\0';
      }
    }

    WM_operator_properties_create_ptr(&props_ptr, ot);
    RNA_string_set(&props_ptr, "filepath", path);
    WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &props_ptr, nullptr);
    WM_operator_properties_free(&props_ptr);

    MEM_freeN(path);
    return OPERATOR_CANCELLED;
  }

  PropertyRNA *prop_relpath;
  const char *path_prop = RNA_struct_find_property(op->ptr, "directory") ? "directory" :
                                                                           "filepath";
  fbo = static_cast<FileBrowseOp *>(MEM_callocN(sizeof(FileBrowseOp), "FileBrowseOp"));
  fbo->ptr = ptr;
  fbo->prop = prop;
  fbo->is_undo = is_undo;
  fbo->is_userdef = is_userdef;
  op->customdata = fbo;

  /* Normally ED_fileselect_get_params would handle this but we need to because of stupid
   * user-prefs exception. - campbell */
  if ((prop_relpath = RNA_struct_find_property(op->ptr, "relative_path"))) {
    if (!RNA_property_is_set(op->ptr, prop_relpath)) {
      bool is_relative = (U.flag & USER_RELPATHS) != 0;

      /* While we want to follow the defaults,
       * we better not switch existing paths relative/absolute state. */
      if (path[0]) {
        is_relative = BLI_path_is_rel(path);
      }

      if (UNLIKELY(ptr.data == &U || is_userdef)) {
        is_relative = false;
      }

      /* Annoying exception!, if we're dealing with the user preferences,
       * default relative to be off. */
      RNA_property_boolean_set(op->ptr, prop_relpath, is_relative);
    }
  }

  RNA_string_set(op->ptr, path_prop, path);
  MEM_freeN(path);

  PropertyRNA *prop_check_existing = RNA_struct_find_property(op->ptr, "check_existing");
  if (!RNA_property_is_set(op->ptr, prop_check_existing)) {
    const bool is_output_path = (RNA_property_flag(prop) & PROP_PATH_OUTPUT) != 0;
    RNA_property_boolean_set(op->ptr, prop_check_existing, is_output_path);
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void BUTTONS_OT_file_browse(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Accept";
  ot->description =
      "Open a file browser, hold Shift to open the file, Alt to browse containing directory";
  ot->idname = "BUTTONS_OT_file_browse";

  /* Callbacks. */
  ot->invoke = file_browse_invoke;
  ot->exec = file_browse_exec;
  ot->cancel = file_browse_cancel;

  /* Conditional undo based on button flag. */
  ot->flag = 0;

  /* Properties. */
  WM_operator_properties_filesel(ot,
                                 0,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

void BUTTONS_OT_directory_browse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Accept";
  ot->description =
      "Open a directory browser, hold Shift to open the file, Alt to browse containing directory";
  ot->idname = "BUTTONS_OT_directory_browse";

  /* api callbacks */
  ot->invoke = file_browse_invoke;
  ot->exec = file_browse_exec;
  ot->cancel = file_browse_cancel;

  /* conditional undo based on button flag */
  ot->flag = 0;

  /* properties */
  WM_operator_properties_filesel(ot,
                                 0,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */
