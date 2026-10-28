/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "MEM_guardedalloc.h"

#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_string_ref.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

namespace blender::ui {

struct TemplatePathData {
  PointerRNA ptr;
  PropertyRNA *path_prop;

  PropertySubType path_prop_subtype;

  std::optional<std::string> filter_glob;
};

namespace {

/** Owned by a text field editing either the parent directory or the name. */
struct PathSplitEdit {
  enum class Part {
    Directory,
    Name,
  };

  PointerRNA ptr;
  PropertyRNA *prop;
  Part part;

  /** Edited by the button. */
  char text[FILE_MAX];
};

}  // namespace

/* Split the path property into a parent directory and name.
 * Returns true if the path has a trailing slash. */
static bool path_split_get(PointerRNA &ptr,
                           PropertyRNA *prop,
                           char (&r_dir)[FILE_MAX],
                           char (&r_name)[FILE_MAXFILE])
{
  /* String properties may be longer than #FILE_MAX, truncate them. */
  char path[FILE_MAX];
  STRNCPY(path, RNA_property_string_get(&ptr, prop).c_str());

  const size_t path_len = strlen(path);
  const bool has_trailing_slash = path_len > 0 &&
                                  BLI_path_slash_is_native_compat(path[path_len - 1]);

  BLI_path_slash_rstrip(path);

  /* If we stripped away all slashes from a path like "//", "/" or "C:\",
   * restore the original one and give an empty name. */
  if (has_trailing_slash && (path[0] == '\0' || BLI_path_is_win32_drive_only(path))) {
    STRNCPY(r_dir, RNA_property_string_get(&ptr, prop).c_str());
    r_name[0] = '\0';
    return true;
  }

  BLI_path_split_dir_file(path, r_dir, sizeof(r_dir), r_name, sizeof(r_name));
  return has_trailing_slash;
}

static void path_split_edit_apply(bContext *C, void *argN, void * /*arg2*/)
{
  PathSplitEdit *edit = static_cast<PathSplitEdit *>(argN);

  /* An empty name would turn the path into its parent directory. */
  if (edit->part == PathSplitEdit::Part::Name &&
      (edit->text[0] == '\0' || BLI_path_slash_find(edit->text)))
  {
    return;
  }

  char dir[FILE_MAX];
  char name[FILE_MAXFILE];
  const bool has_trailing_slash = path_split_get(edit->ptr, edit->prop, dir, name);

  char new_path[FILE_MAX];
  if (edit->part == PathSplitEdit::Part::Name) {
    BLI_path_join(new_path, sizeof(new_path), dir, edit->text);
  }
  else {
    BLI_path_join(new_path, sizeof(new_path), edit->text, name);
  }

  /* Preserve trailing slash, as used for directory paths. */
  if (has_trailing_slash) {
    BLI_path_slash_ensure(new_path, sizeof(new_path));
  }

  RNA_property_string_set(&edit->ptr, edit->prop, new_path);
  RNA_property_update(C, &edit->ptr, edit->prop);
}

static void path_split_edit_free(void *argN)
{
  MEM_delete(static_cast<PathSplitEdit *>(argN));
}

static void *path_split_edit_copy(const void *argN)
{
  return MEM_new<PathSplitEdit>(__func__, *static_cast<const PathSplitEdit *>(argN));
}

static bool path_split_edit_identity_cmp(const Button *a, const Button *b)
{
  const PathSplitEdit *edit_a = static_cast<const PathSplitEdit *>(a->func_argN);
  const PathSplitEdit *edit_b = static_cast<const PathSplitEdit *>(b->func_argN);
  return edit_a->ptr.data == edit_b->ptr.data && edit_a->prop == edit_b->prop &&
         edit_a->part == edit_b->part;
}

/* Parent directory or name button for split paths. */
static Button *template_filepath_split_button(Layout &layout,
                                              const PointerRNA &ptr,
                                              PropertyRNA *prop,
                                              const PathSplitEdit::Part part)
{
  PathSplitEdit *edit = MEM_new<PathSplitEdit>(__func__);
  edit->ptr = ptr;
  edit->prop = prop;
  edit->part = part;

  char dir[FILE_MAX];
  char name[FILE_MAXFILE];
  path_split_get(edit->ptr, prop, dir, name);
  if (part == PathSplitEdit::Part::Name) {
    STRNCPY(edit->text, name);
  }
  else {
    /* Without trailing slash (unless that leaves nothing). */
    STRNCPY(edit->text, dir);
    BLI_path_slash_rstrip(edit->text);
    if (edit->text[0] == '\0') {
      STRNCPY(edit->text, dir);
    }
  }

  Block *block = layout.block();
  block_layout_set_current(block, &layout);
  Button *but = uiDefBut(block,
                         ButtonType::Text,
                         "",
                         0,
                         0,
                         UI_UNIT_X * 5,
                         UI_UNIT_Y,
                         edit->text,
                         0,
                         (part == PathSplitEdit::Part::Name) ? FILE_MAXFILE : FILE_MAX,
                         RNA_property_ui_description(prop));
  if (!RNA_property_editable(&edit->ptr, prop)) {
    button_flag_enable(but, BUT_DISABLED);
  }
  if (!RNA_property_string_is_utf8(prop)) {
    button_flag_enable(but, BUT_NO_UTF8);
  }
  if (!(edit->ptr.owner_id && ID_CHECK_UNDO(edit->ptr.owner_id) &&
        RNA_property_undo_check(prop, edit->ptr.type)))
  {
    button_flag_disable(but, BUT_UNDO);
  }

  button_funcN_set(
      but, path_split_edit_apply, edit, nullptr, path_split_edit_free, path_split_edit_copy);
  button_func_identity_compare_set(but, path_split_edit_identity_cmp);

  return but;
}

/* Parent directory and name text separated by a splash. */
static void template_filepath_split_name_buttons(Layout &row,
                                                 TemplatePathData &path_data,
                                                 const std::optional<StringRef> text,
                                                 const std::optional<StringRef> placeholder)
{
  const StringRef label = text ? *text : RNA_property_ui_name(path_data.path_prop);
  if (!label.is_empty()) {
    row.label(label, ICON_NONE);
  }

  Layout &parts_row = row.row(true);

  /* Parent directory field. */
  Button *dir_but = template_filepath_split_button(
      parts_row, path_data.ptr, path_data.path_prop, PathSplitEdit::Part::Directory);
  if (placeholder && RNA_property_string_length(&path_data.ptr, path_data.path_prop) == 0) {
    button_placeholder_set(dir_but, *placeholder);
  }

  /* Slash label. */
  Layout &separator = parts_row.row(true);
  separator.ui_units_x_set(0.5f);
  separator.fixed_size_set(true);
  separator.label_multiline(SEP_STR, ICON_NONE, UI_STYLE_TEXT_CENTER);

  /* Name field. */
  Layout &name_row = parts_row.row(true);
  name_row.ui_units_x_set(7.0f);
  name_row.fixed_size_set(true);
  template_filepath_split_button(
      name_row, path_data.ptr, path_data.path_prop, PathSplitEdit::Part::Name);

  /* Hidden button for the browser operator to work. */
  block_layout_set_current(parts_row.block(), &parts_row);
  Button *but = uiDefAutoButR(parts_row.block(),
                              &path_data.ptr,
                              path_data.path_prop,
                              -1,
                              "",
                              ICON_NONE,
                              0,
                              0,
                              0,
                              UI_UNIT_Y);
  but->flag |= UI_HIDDEN;
}

static void template_filepath_buttons(const bContext * /* C */,
                                      Layout &layout,
                                      TemplatePathData &path_data,
                                      const char *pathselect_op,
                                      std::optional<StringRef> text,
                                      const std::optional<StringRef> placeholder,
                                      const bool split_name)
{
  /* Don't align open button for split path, since it applies to both parts. */
  const bool align = !split_name;
  Layout &row = layout.row(align);

  Layout *decorate_column = nullptr;
  if (layout.use_property_split()) {
    PropertySplitWrapper split = uiItemPropertySplitWrapperCreate(&row);
    split.label_column->label(text ? *text : RNA_property_ui_name(path_data.path_prop), ICON_NONE);
    decorate_column = split.decorate_column;
    text = "";
  }

  if (split_name) {
    template_filepath_split_name_buttons(row, path_data, text, placeholder);
  }
  else {
    row.prop(&path_data.ptr,
             path_data.path_prop,
             RNA_NO_INDEX,
             0,
             ITEM_R_PATH_NO_OPEN_BUTTON,
             text,
             ICON_NONE,
             placeholder);
  }

  const int icon = RNA_property_editable(&path_data.ptr, path_data.path_prop) ?
                       ICON_FILEBROWSER :
                       ICON_FOLDER_REDIRECT;
  if (pathselect_op) {
    PointerRNA op_ptr = row.op(
        pathselect_op, std::nullopt, icon, wm::OpCallContext::InvokeDefault, ITEM_R_ICON_ONLY);
    /* Unknown operators return an empty pointer. */
    if (op_ptr.data && path_data.path_prop_subtype == PROP_FILEPATH && path_data.filter_glob) {
      PropertyRNA *filter_glob_prop = RNA_struct_find_property(&op_ptr, "filter_glob");
      if (filter_glob_prop) {
        RNA_property_string_set(&op_ptr, filter_glob_prop, path_data.filter_glob->c_str());
      }
    }
  }
  else if (path_data.path_prop_subtype == PROP_DIRPATH) {
    /* #BUTTONS_OT_directory_browse calls #context_active_but_prop_get_filebrowser. */
    row.op("BUTTONS_OT_directory_browse",
           std::nullopt,
           icon,
           wm::OpCallContext::InvokeDefault,
           ITEM_R_ICON_ONLY);
  }
  else {
    /* #BUTTONS_OT_file_browse calls #context_active_but_prop_get_filebrowser. */
    PointerRNA op_ptr = row.op("BUTTONS_OT_file_browse",
                               std::nullopt,
                               icon,
                               wm::OpCallContext::InvokeDefault,
                               ITEM_R_ICON_ONLY);
    if (path_data.filter_glob) {
      RNA_string_set(&op_ptr, "filter_glob", path_data.filter_glob->c_str());
    }
  }

  if (decorate_column) {
    decorate_column->decorator(&path_data.ptr, path_data.path_prop, RNA_NO_INDEX);
  }
}

void template_filepath(Layout *layout,
                       const bContext *C,
                       PointerRNA *ptr,
                       const StringRefNull propname,
                       const char *pathselect_op,
                       const char *filter_glob,
                       const std::optional<StringRef> text,
                       const std::optional<StringRef> placeholder,
                       const bool split_name)
{
  TemplatePathData template_path_data;
  template_path_data.ptr = *ptr;

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  if (!prop || RNA_property_type(prop) != PROP_STRING) {
    RNA_warning(
        "String property not found: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return;
  }
  template_path_data.path_prop = prop;

  template_path_data.path_prop_subtype = RNA_property_subtype(prop);
  if (!ELEM(template_path_data.path_prop_subtype, PROP_FILEPATH, PROP_DIRPATH)) {
    RNA_warning("String property is not a dirpath or filepath: %s.%s",
                RNA_struct_identifier(ptr->type),
                propname.c_str());
    return;
  }

  if (filter_glob) {
    template_path_data.filter_glob = filter_glob;
  }

  template_filepath_buttons(
      C, *layout, template_path_data, pathselect_op, text, placeholder, split_name);
}

}  // namespace blender::ui
