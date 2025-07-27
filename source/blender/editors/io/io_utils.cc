/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_main.hh"

#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

#include "io_utils.hh"

namespace blender::ed::io {

wmOperatorStatus filesel_drop_import_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
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
        title = fmt::format(fmt::runtime(TIP_("Import {} files")), files.size());
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
  PropertyRNA *relative_path_prop = RNA_struct_find_property(ptr, "relative_path");
  const bool is_relative_path = relative_path_prop ?
                                    RNA_property_boolean_get(ptr, relative_path_prop) :
                                    false;
  if (RNA_property_is_set(ptr, directory_prop)) {
    char directory[FILE_MAX], name[FILE_MAX];

    RNA_string_get(ptr, "directory", directory);
    if (is_relative_path && !BLI_path_is_rel(directory)) {
      BLI_path_rel(directory, BKE_main_blendfile_path_from_global());
    }

    PropertyRNA *files_prop = RNA_struct_find_collection_property_check(
        *ptr, "files", &RNA_OperatorFileListElement);

    BLI_assert(files_prop);

    RNA_PROP_BEGIN (ptr, file_ptr, files_prop) {
      RNA_string_get(&file_ptr, "name", name);
      char path[FILE_MAX];
      BLI_path_join(path, sizeof(path), directory, name);
      BLI_path_normalize(path);
      paths.append_non_duplicates(path);
    }
    RNA_PROP_END;
  }
  PropertyRNA *filepath_prop = RNA_struct_find_property(ptr, "filepath");
  if (filepath_prop && RNA_property_is_set(ptr, filepath_prop)) {
    char filepath[FILE_MAX];
    RNA_string_get(ptr, "filepath", filepath);
    if (is_relative_path && !BLI_path_is_rel(filepath)) {
      BLI_path_rel(filepath, BKE_main_blendfile_path_from_global());
    }
    paths.append_non_duplicates(filepath);
  }
  return paths;
}

void paths_to_operator_properties(PointerRNA *ptr, const Span<std::string> paths)
{
  char dir[FILE_MAX];
  BLI_path_split_dir_part(paths[0].c_str(), dir, sizeof(dir));
  RNA_string_set(ptr, "directory", dir);

  RNA_collection_clear(ptr, "files");
  for (const auto &path : paths) {
    char file[FILE_MAX];
    STRNCPY_UTF8(file, path.c_str());
    BLI_path_rel(file, dir);

    PointerRNA itemptr{};
    RNA_collection_add(ptr, "files", &itemptr);
    BLI_assert_msg(BLI_path_is_rel(file), "Expected path to be relative (start with '//')");
    RNA_string_set(&itemptr, "name", file + 2);
  }
}

}  // namespace blender::ed::io
