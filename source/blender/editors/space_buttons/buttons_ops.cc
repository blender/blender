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
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_path_templates.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "buttons_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Start / Clear Search Filter Operators
 *
 * \note Almost a duplicate of the file browser operator #FILE_OT_start_filter.
 * \{ */

static wmOperatorStatus buttons_start_filter_exec(bContext *C, wmOperator * /*op*/)
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

static wmOperatorStatus buttons_clear_filter_exec(bContext *C, wmOperator * /*op*/)
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

static wmOperatorStatus toggle_pin_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceProperties *sbuts = CTX_wm_space_properties(C);

  sbuts->flag ^= SB_PIN_CONTEXT;

  /* Create the properties space pointer. */
  bScreen *screen = CTX_wm_screen(C);
  PointerRNA sbuts_ptr = RNA_pointer_create_discrete(&screen->id, &RNA_SpaceProperties, sbuts);

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

static wmOperatorStatus context_menu_invoke(bContext *C,
                                            wmOperator * /*op*/,
                                            const wmEvent * /*event*/)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Context Menu"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  layout->menu("INFO_MT_area", std::nullopt, ICON_NONE);
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
  PropertyRNA *prop = nullptr;
  bool is_undo = false;
  bool is_userdef = false;
};

static bool file_browse_operator_relative_paths_supported(wmOperator *op)
{
  FileBrowseOp *fbo = static_cast<FileBrowseOp *>(op->customdata);
  const PropertySubType subtype = RNA_property_subtype(fbo->prop);
  if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH)) {
    const int flag = RNA_property_flag(fbo->prop);
    if ((flag & PROP_PATH_SUPPORTS_BLEND_RELATIVE) == 0) {
      return false;
    }
  }
  return true;
}

static wmOperatorStatus file_browse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  FileBrowseOp *fbo = static_cast<FileBrowseOp *>(op->customdata);
  char *path;
  const char *path_prop = RNA_struct_find_property(op->ptr, "directory") ? "directory" :
                                                                           "filepath";

  if (fbo == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (RNA_struct_property_is_set(op->ptr, path_prop) == 0) {
    MEM_delete(fbo);
    return OPERATOR_CANCELLED;
  }

  path = RNA_string_get_alloc(op->ptr, path_prop, nullptr, 0, nullptr);

  if (path[0]) {
    /* Check relative paths are supported here as this option will be hidden
     * when it's not supported. In this case the value may have been enabled
     * by default or from the last-used setting.
     * Either way, don't use the blend-file relative prefix when it's not supported. */
    const PropertySubType prop_subtype = RNA_property_subtype(fbo->prop);
    const bool is_relative = BLI_path_is_rel(path);
    const bool make_relative = RNA_boolean_get(op->ptr, "relative_path") &&
                               file_browse_operator_relative_paths_supported(op);

    /* Add slash for directories, important for some properties. */
    if ((prop_subtype == PROP_DIRPATH) || (is_relative || make_relative)) {
      char path_buf[FILE_MAX];
      ID *id = fbo->ptr.owner_id;

      STRNCPY(path_buf, path);
      MEM_freeN(path);

      if (is_relative) {
        BLI_path_abs(path_buf, id ? ID_BLEND_PATH(bmain, id) : BKE_main_blendfile_path(bmain));
      }

      if (prop_subtype == PROP_DIRPATH) {
        BLI_path_slash_ensure(path_buf, sizeof(path_buf));
      }

      if (make_relative) {
        BLI_path_rel(path_buf, BKE_main_blendfile_path(bmain));
      }
      path = BLI_strdup(path_buf);
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

  BLI_assert(fbo == op->customdata);
  MEM_delete(fbo);

  return OPERATOR_FINISHED;
}

static void file_browse_cancel(bContext * /*C*/, wmOperator *op)
{
  FileBrowseOp *fbo = static_cast<FileBrowseOp *>(op->customdata);
  MEM_delete(fbo);
  op->customdata = nullptr;
}

static wmOperatorStatus file_browse_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  bool is_undo;
  bool is_userdef;
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

  if ((RNA_property_flag(prop) & PROP_PATH_SUPPORTS_TEMPLATES) != 0) {
    const std::optional<blender::bke::path_templates::VariableMap> variables =
        BKE_build_template_variables_for_prop(C, &ptr, prop);
    BLI_assert(variables.has_value());

    const blender::Vector<blender::bke::path_templates::Error> errors = BKE_path_apply_template(
        path, FILE_MAX, *variables);
    if (!errors.is_empty()) {
      BKE_report_path_template_errors(op->reports, RPT_ERROR, path, errors);
      return OPERATOR_CANCELLED;
    }
  }

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
    WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::ExecDefault, &props_ptr, nullptr);
    WM_operator_properties_free(&props_ptr);

    MEM_freeN(path);
    return OPERATOR_CANCELLED;
  }

  {
    const char *info;
    if (!RNA_property_editable_info(&ptr, prop, &info)) {
      if (info[0]) {
        BKE_reportf(op->reports, RPT_ERROR, "Property is not editable: %s", info);
      }
      else {
        BKE_report(op->reports, RPT_ERROR, "Property is not editable");
      }
      MEM_freeN(path);
      return OPERATOR_CANCELLED;
    }
  }

  PropertyRNA *prop_relpath;
  const char *path_prop = RNA_struct_find_property(op->ptr, "directory") ? "directory" :
                                                                           "filepath";
  FileBrowseOp *fbo = MEM_new<FileBrowseOp>(__func__);
  fbo->ptr = ptr;
  fbo->prop = prop;
  fbo->is_undo = is_undo;
  fbo->is_userdef = is_userdef;

  op->customdata = fbo;

  /* NOTE(@ideasman42): Normally #ED_fileselect_get_params would handle this
   * but we need to because of stupid user-preferences exception. */
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

  const char *prop_id = RNA_property_identifier(prop);

  /* NOTE: relying on built-in names isn't useful for add-on authors.
   * The property itself should support this kind of meta-data. */
  if (STR_ELEM(prop_id, "font_path_ui", "font_path_ui_mono", "font_directory")) {
    RNA_boolean_set(op->ptr, "filter_font", true);
    RNA_boolean_set(op->ptr, "filter_folder", true);
    RNA_enum_set(op->ptr, "display_type", FILE_IMGDISPLAY);
    RNA_enum_set(op->ptr, "sort_method", FILE_SORT_ALPHA);
    if (!path[0]) {
      char fonts_path[FILE_MAX] = {0};
      if (U.fontdir[0]) {
        STRNCPY(fonts_path, U.fontdir);
        /* The file selector will expand the blend-file relative prefix. */
      }
      else if (!BKE_appdir_font_folder_default(fonts_path, ARRAY_SIZE(fonts_path))) {
        STRNCPY(fonts_path, BKE_appdir_folder_default_or_root());
      }
      BLI_path_slash_ensure(fonts_path, ARRAY_SIZE(fonts_path));
      MEM_freeN(path);
      path = BLI_strdup(fonts_path);
    }
  }

  if (!path[0]) {
    /* Find a reasonable folder to start in if none found. */
    char default_path[FILE_MAX] = {0};
    STRNCPY(default_path, BKE_appdir_folder_default_or_root());
    BLI_path_slash_ensure(default_path, ARRAY_SIZE(default_path));
    MEM_freeN(path);
    path = BLI_strdup(default_path);
  }

  RNA_string_set(op->ptr, path_prop, path);
  MEM_freeN(path);

  PropertyRNA *prop_check_existing = RNA_struct_find_property(op->ptr, "check_existing");
  if (!RNA_property_is_set(op->ptr, prop_check_existing)) {
    const bool is_output_path = (RNA_property_flag(prop) & PROP_PATH_OUTPUT) != 0;
    RNA_property_boolean_set(op->ptr, prop_check_existing, is_output_path);
  }
  if (std::optional<std::string> filter = RNA_property_string_path_filter(C, &ptr, prop)) {
    RNA_string_set(op->ptr, "filter_glob", filter->c_str());
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static bool file_browse_poll_property(const bContext * /*C*/,
                                      wmOperator *op,
                                      const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  if (STREQ(prop_id, "relative_path")) {
    if (!file_browse_operator_relative_paths_supported(op)) {
      return false;
    }
  }
  return true;
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
  ot->poll_property = file_browse_poll_property;

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

  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "filter_glob", nullptr, 0, "Glob Filter", "Custom filter");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void BUTTONS_OT_directory_browse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Accept";
  ot->description =
      "Open a directory browser, hold Shift to open the file, Alt to browse containing directory";
  ot->idname = "BUTTONS_OT_directory_browse";

  /* API callbacks. */
  ot->invoke = file_browse_invoke;
  ot->exec = file_browse_exec;
  ot->cancel = file_browse_cancel;
  ot->poll_property = file_browse_poll_property;

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
