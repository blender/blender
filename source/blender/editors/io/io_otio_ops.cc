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

#  include "BKE_context.hh"
#  include "BKE_report.hh"

#  include "BLI_listbase.hh"
#  include "BLI_path_utils.hh"

#  include "RNA_access.hh"
#  include "RNA_define.hh"
#  include "RNA_enum_types.hh"

#  include "ED_fileselect.hh"
#  include "ED_object.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  include "SEQ_sequencer.hh"

#  include "IO_otio.hh"
#  include "io_otio_ops.hh"

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

}  // namespace blender

#endif
