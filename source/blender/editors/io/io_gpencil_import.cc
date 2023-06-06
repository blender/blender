/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_IO_GPENCIL

#  include "BLI_path_util.h"

#  include "MEM_guardedalloc.h"

#  include "DNA_gpencil_legacy_types.h"
#  include "DNA_space_types.h"

#  include "BKE_context.h"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_report.h"

#  include "BLT_translation.h"

#  include "RNA_access.h"
#  include "RNA_define.h"

#  include "UI_interface.h"
#  include "UI_resources.h"

#  include "WM_api.h"
#  include "WM_types.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_query.h"

#  include "ED_gpencil_legacy.h"

#  include "io_gpencil.hh"

#  include "gpencil_io.h"

/* <-------- SVG single frame import. --------> */
static bool wm_gpencil_import_svg_common_check(bContext * /*C*/, wmOperator *op)
{

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".svg")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".svg");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

static int wm_gpencil_import_svg_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int wm_gpencil_import_svg_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false) ||
      !RNA_struct_find_property(op->ptr, "directory"))
  {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  ARegion *region = get_invoke_region(C);
  if (region == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Unable to find valid 3D View area");
    return OPERATOR_CANCELLED;
  }
  View3D *v3d = get_invoke_view3d(C);

  /* Set flags. */
  int flag = 0;

  const int resolution = RNA_int_get(op->ptr, "resolution");
  const float scale = RNA_float_get(op->ptr, "scale");

  GpencilIOParams params {};
  params.C = C;
  params.region = region;
  params.v3d = v3d;
  params.ob = nullptr;
  params.mode = GP_IMPORT_FROM_SVG;
  params.frame_start = scene->r.cfra;
  params.frame_end = scene->r.cfra;
  params.frame_cur = scene->r.cfra;
  params.flag = flag;
  params.scale = scale;
  params.select_mode = 0;
  params.frame_mode = 0;
  params.stroke_sample = 0.0f;
  params.resolution = resolution;

  /* Loop all selected files to import them. All SVG imported shared the same import
   * parameters, but they are created in separated grease pencil objects. */
  PropertyRNA *prop;
  if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
    char *directory = RNA_string_get_alloc(op->ptr, "directory", nullptr, 0, nullptr);

    if ((prop = RNA_struct_find_property(op->ptr, "files"))) {
      char file_path[FILE_MAX];
      RNA_PROP_BEGIN (op->ptr, itemptr, prop) {
        char *filename = RNA_string_get_alloc(&itemptr, "name", nullptr, 0, nullptr);
        BLI_path_join(file_path, sizeof(file_path), directory, filename);
        MEM_freeN(filename);

        /* Do Import. */
        WM_cursor_wait(1);
        RNA_string_get(&itemptr, "name", params.filename);
        const bool done = gpencil_io_import(file_path, &params);
        WM_cursor_wait(0);
        if (!done) {
          BKE_reportf(op->reports, RPT_WARNING, "Unable to import '%s'", file_path);
        }
      }
      RNA_PROP_END;
    }
    MEM_freeN(directory);
  }

  return OPERATOR_FINISHED;
}

static void ui_gpencil_import_svg_settings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, imfptr, "resolution", 0, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "scale", 0, nullptr, ICON_NONE);
}

static void wm_gpencil_import_svg_draw(bContext * /*C*/, wmOperator *op)
{
  ui_gpencil_import_svg_settings(op->layout, op->ptr);
}

static bool wm_gpencil_import_svg_poll(bContext *C)
{
  if ((CTX_wm_window(C) == nullptr) || (CTX_data_mode_enum(C) != CTX_MODE_OBJECT)) {
    return false;
  }

  return true;
}

void WM_OT_gpencil_import_svg(wmOperatorType *ot)
{
  ot->name = "Import SVG";
  ot->description = "Import SVG into grease pencil";
  ot->idname = "WM_OT_gpencil_import_svg";

  ot->invoke = wm_gpencil_import_svg_invoke;
  ot->exec = wm_gpencil_import_svg_exec;
  ot->poll = wm_gpencil_import_svg_poll;
  ot->ui = wm_gpencil_import_svg_draw;
  ot->check = wm_gpencil_import_svg_common_check;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_OBJECT_IO,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_SHOW_PROPS |
                                     WM_FILESEL_DIRECTORY | WM_FILESEL_FILES,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  RNA_def_int(ot->srna,
              "resolution",
              10,
              1,
              30,
              "Resolution",
              "Resolution of the generated strokes",
              1,
              20);

  RNA_def_float(ot->srna,
                "scale",
                10.0f,
                0.001f,
                100.0f,
                "Scale",
                "Scale of the final strokes",
                0.001f,
                100.0f);
}

#endif /* WITH_IO_GPENCIL */
