/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_OPENTIMELINEIO

#  include <cerrno>
#  include <cstring>

#  include "DNA_sequence_types.h"
#  include "DNA_space_enums.h"
#  include "DNA_windowmanager_enums.h"
#  include "DNA_workspace_types.h"

#  include "BKE_context.hh"
#  include "BKE_file_handler.hh"
#  include "BKE_report.hh"

#  include "BLI_listbase.hh"
#  include "BLI_path_utils.hh"
#  include "BLI_string.hh"
#  include "BLI_string_utf8.hh"
#  include "BLI_vector.hh"

#  include "BLT_translation.hh"

#  include "RNA_access.hh"
#  include "RNA_define.hh"
#  include "RNA_enum_types.hh"

#  include "ED_fileselect.hh"
#  include "ED_object.hh"
#  include "ED_sequencer.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  include "SEQ_sequencer.hh"

#  include "IO_otio.hh"
#  include "io_otio_ops.hh"
#  include "io_utils.hh"

namespace blender {

static wmOperatorStatus wm_otio_export_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  ED_fileselect_ensure_default_filepath(C, op, ".otio");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus wm_otio_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *editing = seq::editing_get(scene);

  if (!scene || !editing) {
    BKE_report(op->reports, RPT_ERROR, "No sequencer scene found");
    return OPERATOR_CANCELLED;
  }

  OTIOExportParams export_params;
  RNA_string_get(op->ptr, "filepath", export_params.filepath);
  export_params.use_resolve_metadata = RNA_boolean_get(op->ptr, "use_resolve_metadata");
  export_params.reports = op->reports;

  OTIO_export(C, export_params);

  return OPERATOR_FINISHED;
}

static bool wm_otio_export_check(bContext * /*C*/, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".otio")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".otio");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

static bool wm_otio_export_poll(bContext *C)
{
  if (!WM_operator_winactive(C)) {
    return false;
  }

  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *editing = seq::editing_get(scene);

  if (!scene) {
    CTX_wm_operator_poll_msg_set(C, "No sequencer scene");
    return false;
  }
  if (!editing || BLI_listbase_is_empty(&editing->seqbase)) {
    CTX_wm_operator_poll_msg_set(C, "Sequencer scene has no strips");
    return false;
  }

  return true;
}

void WM_OT_otio_export(wmOperatorType *ot)
{
  ot->name = "Export OTIO";
  ot->description = "Export the sequencer scene as an OpenTimelineIO timeline";
  ot->idname = "WM_OT_otio_export";

  ot->invoke = wm_otio_export_invoke;
  ot->exec = wm_otio_export_exec;
  ot->poll = wm_otio_export_poll;
  ot->check = wm_otio_export_check;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.otio", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_boolean(ot->srna,
                  "use_resolve_metadata",
                  false,
                  "Export DaVinci Resolve Metadata",
                  "Write additional metadata so DaVinci Resolve can recreate generated strips, "
                  "transforms, blending, audio levels and more");
}

static wmOperatorStatus wm_otio_import_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return ed::io::filesel_drop_import_invoke(C, op, event);
}

static wmOperatorStatus wm_otio_import_exec(bContext *C, wmOperator *op)
{
  OTIOImportParams params;
  params.reports = op->reports;

  const auto paths = ed::io::paths_from_operator_properties(op->ptr);

  if (paths.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  Scene *scene = nullptr;
  for (const auto &path : paths) {
    STRNCPY(params.filepath, path.c_str());
    if (Scene *imported_scene = OTIO_import(C, params)) {
      scene = imported_scene;
    }
  }

  if (scene) {
    wmWindow *win = CTX_wm_window(C);
    WorkSpace *workspace = CTX_wm_workspace(C);
    if (win && workspace && CTX_wm_space_seq(C)) {
      if (scene != workspace->sequencer_scene) {
        workspace->sequencer_scene = scene;
        WM_window_set_active_scene(CTX_data_main(C), C, win, scene);
        ed::vse::sync_active_scene_and_time_with_scene_strip(*C);
      }
    }
    else {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  TIP_("Imported OTIO timeline into scene '%s'"),
                  scene->id.name + 2);
    }
  }
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_otio_import(wmOperatorType *ot)
{
  ot->name = "Import OTIO";
  ot->description = "Import OpenTimelineIO Timeline into Sequencer Scene";
  ot->idname = "WM_OT_otio_import";

  ot->invoke = wm_otio_import_invoke;
  ot->exec = wm_otio_import_exec;
  ot->poll = WM_operator_winactive;
  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_DIRECTORY |
                                     WM_FILESEL_FILES,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.otio", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

namespace ed::io {
static bool otio_poll_drop(const bContext *C, bke::FileHandlerType * /*fh*/)
{
  return CTX_wm_space_seq(C) != nullptr;
}

void otio_file_handler_add()
{
  auto fh = std::make_unique<bke::FileHandlerType>();
  STRNCPY_UTF8(fh->idname, "IO_FH_otio");
  STRNCPY_UTF8(fh->import_operator, "WM_OT_otio_import");
  STRNCPY_UTF8(fh->label, "OpenTimelineIO");
  STRNCPY_UTF8(fh->file_extensions_str, ".otio");
  fh->poll_drop = otio_poll_drop;
  bke::file_handler_add(std::move(fh));
}
}  // namespace ed::io
}  // namespace blender

#endif
