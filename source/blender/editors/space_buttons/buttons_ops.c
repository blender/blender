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
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spbuttons
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_undo.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "buttons_intern.h" /* own include */

/********************** context_menu operator *********************/

static int context_menu_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Context Menu"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  uiItemM(layout, "INFO_MT_area", NULL, ICON_NONE);
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void BUTTONS_OT_context_menu(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Context Menu";
  ot->description = "Display properties editor context_menu";
  ot->idname = "BUTTONS_OT_context_menu";

  /* api callbacks */
  ot->invoke = context_menu_invoke;
  ot->poll = ED_operator_buttons_active;
}

/********************** filebrowse operator *********************/

typedef struct FileBrowseOp {
  PointerRNA ptr;
  PropertyRNA *prop;
  bool is_undo;
} FileBrowseOp;

static int file_browse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  FileBrowseOp *fbo = op->customdata;
  ID *id;
  char *str, path[FILE_MAX];
  const char *path_prop = RNA_struct_find_property(op->ptr, "directory") ? "directory" :
                                                                           "filepath";

  if (RNA_struct_property_is_set(op->ptr, path_prop) == 0 || fbo == NULL) {
    return OPERATOR_CANCELLED;
  }

  str = RNA_string_get_alloc(op->ptr, path_prop, NULL, 0);

  /* add slash for directories, important for some properties */
  if (RNA_property_subtype(fbo->prop) == PROP_DIRPATH) {
    const bool is_relative = RNA_boolean_get(op->ptr, "relative_path");
    id = fbo->ptr.id.data;

    BLI_strncpy(path, str, FILE_MAX);
    BLI_path_abs(path, id ? ID_BLEND_PATH(bmain, id) : BKE_main_blendfile_path(bmain));

    if (BLI_is_dir(path)) {
      /* do this first so '//' isnt converted to '//\' on windows */
      BLI_add_slash(path);
      if (is_relative) {
        BLI_strncpy(path, str, FILE_MAX);
        BLI_path_rel(path, BKE_main_blendfile_path(bmain));
        str = MEM_reallocN(str, strlen(path) + 2);
        BLI_strncpy(str, path, FILE_MAX);
      }
      else {
        str = MEM_reallocN(str, strlen(str) + 2);
      }
    }
    else {
      char *const lslash = (char *)BLI_last_slash(str);
      if (lslash) {
        lslash[1] = '\0';
      }
    }
  }

  RNA_property_string_set(&fbo->ptr, fbo->prop, str);
  RNA_property_update(C, &fbo->ptr, fbo->prop);
  MEM_freeN(str);

  if (fbo->is_undo) {
    const char *undostr = RNA_property_identifier(fbo->prop);
    ED_undo_push(C, undostr);
  }

  /* special, annoying exception, filesel on redo panel [#26618] */
  {
    wmOperator *redo_op = WM_operator_last_redo(C);
    if (redo_op) {
      if (fbo->ptr.data == redo_op->ptr->data) {
        ED_undo_operator_repeat(C, redo_op);
      }
    }
  }

  MEM_freeN(op->customdata);

  return OPERATOR_FINISHED;
}

static void file_browse_cancel(bContext *UNUSED(C), wmOperator *op)
{
  MEM_freeN(op->customdata);
  op->customdata = NULL;
}

static int file_browse_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  bool is_undo;
  FileBrowseOp *fbo;
  char *str;

  if (CTX_wm_space_file(C)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot activate a file selector, one already open");
    return OPERATOR_CANCELLED;
  }

  UI_context_active_but_prop_get_filebrowser(C, &ptr, &prop, &is_undo);

  if (!prop) {
    return OPERATOR_CANCELLED;
  }

  str = RNA_property_string_get_alloc(&ptr, prop, NULL, 0, NULL);

  /* useful yet irritating feature, Shift+Click to open the file
   * Alt+Click to browse a folder in the OS's browser */
  if (event->shift || event->alt) {
    wmOperatorType *ot = WM_operatortype_find("WM_OT_path_open", true);
    PointerRNA props_ptr;

    if (event->alt) {
      char *lslash = (char *)BLI_last_slash(str);
      if (lslash) {
        *lslash = '\0';
      }
    }

    WM_operator_properties_create_ptr(&props_ptr, ot);
    RNA_string_set(&props_ptr, "filepath", str);
    WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &props_ptr);
    WM_operator_properties_free(&props_ptr);

    MEM_freeN(str);
    return OPERATOR_CANCELLED;
  }
  else {
    PropertyRNA *prop_relpath;
    const char *path_prop = RNA_struct_find_property(op->ptr, "directory") ? "directory" :
                                                                             "filepath";
    fbo = MEM_callocN(sizeof(FileBrowseOp), "FileBrowseOp");
    fbo->ptr = ptr;
    fbo->prop = prop;
    fbo->is_undo = is_undo;
    op->customdata = fbo;

    /* normally ED_fileselect_get_params would handle this but we need to because of stupid
     * user-prefs exception - campbell */
    if ((prop_relpath = RNA_struct_find_property(op->ptr, "relative_path"))) {
      if (!RNA_property_is_set(op->ptr, prop_relpath)) {
        bool is_relative = (U.flag & USER_RELPATHS) != 0;

        /* while we want to follow the defaults,
         * we better not switch existing paths relative/absolute state. */
        if (str[0]) {
          is_relative = BLI_path_is_rel(str);
        }

        if (UNLIKELY(ptr.data == &U)) {
          is_relative = false;
        }

        /* annoying exception!, if were dealing with the user prefs, default relative to be off */
        RNA_property_boolean_set(op->ptr, prop_relpath, is_relative);
      }
    }

    RNA_string_set(op->ptr, path_prop, str);
    MEM_freeN(str);

    WM_event_add_fileselect(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
}

void BUTTONS_OT_file_browse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Accept";
  ot->description =
      "Open a file browser, Hold Shift to open the file, Alt to browse containing directory";
  ot->idname = "BUTTONS_OT_file_browse";

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
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
}

/* second operator, only difference from BUTTONS_OT_file_browse is WM_FILESEL_DIRECTORY */
void BUTTONS_OT_directory_browse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Accept";
  ot->description =
      "Open a directory browser, Hold Shift to open the file, Alt to browse containing directory";
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
                                 FILE_SORT_ALPHA);
}
