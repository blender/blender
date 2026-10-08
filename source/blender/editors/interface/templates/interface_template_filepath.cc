/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

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

static void template_filepath_buttons(const bContext * /* C */,
                                      Layout &layout,
                                      TemplatePathData &path_data,
                                      const char *pathselect_op,
                                      std::optional<StringRef> text,
                                      const std::optional<StringRef> placeholder)
{
  Layout &row = layout.row(true);

  row.prop(&path_data.ptr,
           path_data.path_prop,
           RNA_NO_INDEX,
           0,
           ITEM_R_PATH_NO_OPEN_BUTTON,
           text,
           ICON_NONE,
           placeholder);

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
}

void template_filepath(Layout *layout,
                       const bContext *C,
                       PointerRNA *ptr,
                       const StringRefNull propname,
                       const char *pathselect_op,
                       const char *filter_glob,
                       const std::optional<StringRef> text,
                       const std::optional<StringRef> placeholder)
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

  template_filepath_buttons(C, *layout, template_path_data, pathselect_op, text, placeholder);
}

}  // namespace blender::ui
