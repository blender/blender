/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_IO_SPZ

#  include "io_spz_ops.hh"

#  include <memory>

#  include "BLI_string_utf8.hh"

#  include "BKE_context.hh"
#  include "BKE_file_handler.hh"

#  include "RNA_access.hh"
#  include "RNA_define.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  include "ED_outliner.hh"

#  include "IO_spz.hh"
#  include "io_utils.hh"

namespace blender {

static wmOperatorStatus wm_spz_import_exec(bContext *C, wmOperator *op)
{
  SPZImportParams params{
      .reports = op->reports,
  };

  const Vector<std::string> paths = ed::io::paths_from_operator_properties(op->ptr);

  if (paths.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  for (const std::string &path : paths) {
    params.filepath = path;
    SPZ_import(C, &params);
  }

  Scene *scene = CTX_data_scene(C);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void WM_OT_spz_import(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Import SPZ";
  ot->description = "Import an SPZ file as a gaussian splat point cloud object";
  ot->idname = "WM_OT_spz_import";

  ot->invoke = ed::io::filesel_drop_import_invoke;
  ot->exec = wm_spz_import_exec;
  ot->poll = WM_operator_winactive;
  ot->flag = OPTYPE_UNDO | OPTYPE_PRESET;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_FILES | WM_FILESEL_DIRECTORY |
                                     WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  /* Only show `.spz` files by default. */
  prop = RNA_def_string(ot->srna, "filter_glob", "*.spz", 0, "Extension Filter", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

namespace ed::io {

void spz_file_handler_add()
{
  auto fh = std::make_unique<bke::FileHandlerType>();
  STRNCPY_UTF8(fh->idname, "IO_FH_spz");
  STRNCPY_UTF8(fh->import_operator, "WM_OT_spz_import");
  STRNCPY_UTF8(fh->label, "SPZ");
  STRNCPY_UTF8(fh->file_extensions_str, ".spz");
  fh->poll_drop = poll_file_object_drop;
  bke::file_handler_add(std::move(fh));
}

}  // namespace ed::io
}  // namespace blender

#endif /* WITH_IO_SPZ */
