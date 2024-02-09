/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_path_util.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"

#include "DNA_space_types.h"

#include "ED_fileselect.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"

#include "io_utils.hh"

namespace blender::ed::io {

int filesel_drop_import_invoke(bContext *C, wmOperator *op, const wmEvent * /* event */)
{

  PropertyRNA *filepath_prop = RNA_struct_find_property(op->ptr, "filepath");
  PropertyRNA *directory_prop = RNA_struct_find_property(op->ptr, "directory");
  if ((filepath_prop && RNA_property_is_set(op->ptr, filepath_prop)) ||
      (directory_prop && RNA_property_is_set(op->ptr, directory_prop)))
  {
    std::string title;
    PropertyRNA *files_prop = RNA_struct_find_property(op->ptr, "files");
    if (directory_prop && files_prop) {
      const auto files = paths_from_operator_properties(op->ptr);
      if (files.size() == 1) {
        title = files[0];
      }
      else {
        title = fmt::format(TIP_("Import {} files"), files.size());
      }
    }
    else {
      char filepath[FILE_MAX];
      RNA_string_get(op->ptr, "filepath", filepath);
      title = filepath;
    }
    return WM_operator_props_dialog_popup(
        C, op, 350, std::move(title), WM_operatortype_name(op->type, op->ptr));
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

bool poll_file_object_drop(const bContext *C, blender::bke::FileHandlerType * /*fh*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  if (!region || region->regiontype != RGN_TYPE_WINDOW) {
    return false;
  }
  if (v3d) {
    return true;
  }
  if (space_outliner && space_outliner->outlinevis == SO_VIEW_LAYER) {
    return true;
  }
  return false;
}

Vector<std::string> paths_from_operator_properties(PointerRNA *ptr)
{
  Vector<std::string> paths;
  PropertyRNA *directory_prop = RNA_struct_find_property(ptr, "directory");
  if (RNA_property_is_set(ptr, directory_prop)) {
    char directory[FILE_MAX], name[FILE_MAX];

    RNA_string_get(ptr, "directory", directory);

    PropertyRNA *files_prop = RNA_struct_find_collection_property_check(
        *ptr, "files", &RNA_OperatorFileListElement);

    BLI_assert(files_prop);

    RNA_PROP_BEGIN (ptr, file_ptr, files_prop) {
      RNA_string_get(&file_ptr, "name", name);
      char path[FILE_MAX];
      BLI_path_join(path, sizeof(path), directory, name);
      paths.append_non_duplicates(path);
    }
    RNA_PROP_END;
  }
  PropertyRNA *filepath_prop = RNA_struct_find_property(ptr, "filepath");
  if (filepath_prop && RNA_property_is_set(ptr, filepath_prop)) {
    char filepath[FILE_MAX];
    RNA_string_get(ptr, "filepath", filepath);
    paths.append_non_duplicates(filepath);
  }
  return paths;
}
}  // namespace blender::ed::io
