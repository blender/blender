/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_IO_GREASE_PENCIL

#  include "BLI_path_utils.hh"
#  include "BLI_string.h"

#  include "DNA_space_types.h"
#  include "DNA_view3d_types.h"

#  include "BKE_context.hh"
#  include "BKE_file_handler.hh"
#  include "BKE_report.hh"
#  include "BKE_screen.hh"

#  include "BLT_translation.hh"

#  include "RNA_access.hh"
#  include "RNA_define.hh"

#  include "ED_fileselect.hh"

#  include "UI_interface.hh"
#  include "UI_resources.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  include "io_grease_pencil.hh"
#  include "io_utils.hh"

#  include "grease_pencil_io.hh"

namespace blender::ed::io {

#  if defined(WITH_PUGIXML) || defined(WITH_HARU)

/* Definition of enum elements to export. */
/* Common props for exporting. */
static void grease_pencil_export_common_props_definition(wmOperatorType *ot)
{
  using blender::io::grease_pencil::ExportParams;
  using SelectMode = ExportParams::SelectMode;

  static const EnumPropertyItem select_mode_items[] = {
      {int(SelectMode::Active), "ACTIVE", 0, "Active", "Include only the active object"},
      {int(SelectMode::Selected), "SELECTED", 0, "Selected", "Include selected objects"},
      {int(SelectMode::Visible), "VISIBLE", 0, "Visible", "Include all visible objects"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_boolean(ot->srna, "use_fill", true, "Fill", "Export strokes with fill enabled");
  RNA_def_enum(ot->srna,
               "selected_object_type",
               select_mode_items,
               int(SelectMode::Active),
               "Object",
               "Which objects to include in the export");
  RNA_def_float(ot->srna,
                "stroke_sample",
                0.0f,
                0.0f,
                100.0f,
                "Sampling",
                "Precision of stroke sampling. Low values mean a more precise result, and zero "
                "disables sampling",
                0.0f,
                100.0f);
  RNA_def_boolean(
      ot->srna, "use_uniform_width", false, "Uniform Width", "Export strokes with uniform width");
}

#  endif

/* Note: Region data is found using "big area" functions, rather than context. This is necessary
 * since export operators are not always invoked from a View3D. This enables the operator to find
 * the most relevant 3D view for projection of strokes. */
static bool get_invoke_region(bContext *C,
                              ARegion **r_region,
                              View3D **r_view3d,
                              RegionView3D **r_rv3d)
{
  bScreen *screen = CTX_wm_screen(C);
  if (screen == nullptr) {
    return false;
  }
  ScrArea *area = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
  if (area == nullptr) {
    return false;
  }

  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  *r_region = region;
  *r_view3d = static_cast<View3D *>(area->spacedata.first);
  *r_rv3d = static_cast<RegionView3D *>(region->regiondata);
  return true;
}

}  // namespace blender::ed::io

/* -------------------------------------------------------------------- */
/** \name SVG single frame import
 * \{ */

namespace blender::ed::io {

static bool grease_pencil_import_svg_check(bContext * /*C*/, wmOperator *op)
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

static int grease_pencil_import_svg_exec(bContext *C, wmOperator *op)
{
  using blender::io::grease_pencil::ImportParams;
  using blender::io::grease_pencil::IOContext;

  Scene *scene = CTX_data_scene(C);

  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false) ||
      !RNA_struct_find_property(op->ptr, "directory"))
  {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  ARegion *region;
  View3D *v3d;
  RegionView3D *rv3d;
  if (!get_invoke_region(C, &region, &v3d, &rv3d)) {
    BKE_report(op->reports, RPT_ERROR, "Unable to find valid 3D View area");
    return OPERATOR_CANCELLED;
  }

  const int resolution = RNA_int_get(op->ptr, "resolution");
  const float scale = RNA_float_get(op->ptr, "scale");
  const bool use_scene_unit = RNA_boolean_get(op->ptr, "use_scene_unit");
  const bool recenter_bounds = true;

  const IOContext io_context(*C, region, v3d, rv3d, op->reports);
  const ImportParams params = {scale, scene->r.cfra, resolution, use_scene_unit, recenter_bounds};

  /* Loop all selected files to import them. All SVG imported shared the same import
   * parameters, but they are created in separated grease pencil objects. */
  const auto paths = blender::ed::io::paths_from_operator_properties(op->ptr);
  for (const auto &path : paths) {
    /* Do Import. */
    WM_cursor_wait(true);

    const bool done = blender::io::grease_pencil::import_svg(io_context, params, path);
    WM_cursor_wait(false);
    if (!done) {
      BKE_reportf(op->reports, RPT_WARNING, "Unable to import '%s'", path.c_str());
    }
  }

  return OPERATOR_FINISHED;
}

static void grease_pencil_import_svg_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiLayout *box = uiLayoutBox(layout);
  uiLayout *col = uiLayoutColumn(box, false);
  uiItemR(col, op->ptr, "resolution", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, op->ptr, "scale", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static bool grease_pencil_import_svg_poll(bContext *C)
{
  if ((CTX_wm_window(C) == nullptr) || (CTX_data_mode_enum(C) != CTX_MODE_OBJECT)) {
    return false;
  }

  return true;
}

}  // namespace blender::ed::io

void WM_OT_grease_pencil_import_svg(wmOperatorType *ot)
{
  ot->name = "Import SVG as Grease Pencil";
  ot->description = "Import SVG into Grease Pencil";
  ot->idname = "WM_OT_grease_pencil_import_svg";

  ot->invoke = blender::ed::io::filesel_drop_import_invoke;
  ot->exec = blender::ed::io::grease_pencil_import_svg_exec;
  ot->poll = blender::ed::io::grease_pencil_import_svg_poll;
  ot->ui = blender::ed::io::grease_pencil_import_svg_draw;
  ot->check = blender::ed::io::grease_pencil_import_svg_check;

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
              100000,
              "Resolution",
              "Resolution of the generated strokes",
              1,
              20);

  RNA_def_float(ot->srna,
                "scale",
                10.0f,
                0.000001f,
                1000000.0f,
                "Scale",
                "Scale of the final strokes",
                0.001f,
                100.0f);

  RNA_def_boolean(ot->srna,
                  "use_scene_unit",
                  false,
                  "Scene Unit",
                  "Apply current scene's unit (as defined by unit scale) to imported data");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name SVG single frame export
 * \{ */

#  ifdef WITH_PUGIXML

namespace blender::ed::io {

static bool grease_pencil_export_svg_check(bContext * /*C*/, wmOperator *op)
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

static int grease_pencil_export_svg_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ED_fileselect_ensure_default_filepath(C, op, ".svg");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_export_svg_exec(bContext *C, wmOperator *op)
{
  using blender::io::grease_pencil::ExportParams;
  using blender::io::grease_pencil::IOContext;

  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  ARegion *region;
  View3D *v3d;
  RegionView3D *rv3d;
  if (!get_invoke_region(C, &region, &v3d, &rv3d)) {
    BKE_report(op->reports, RPT_ERROR, "Unable to find valid 3D View area");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  const bool export_stroke_materials = true;
  const bool export_fill_materials = RNA_boolean_get(op->ptr, "use_fill");
  const bool use_uniform_width = RNA_boolean_get(op->ptr, "use_uniform_width");
  const ExportParams::SelectMode select_mode = ExportParams::SelectMode(
      RNA_enum_get(op->ptr, "selected_object_type"));
  const ExportParams::FrameMode frame_mode = ExportParams::FrameMode::Active;
  const bool use_clip_camera = RNA_boolean_get(op->ptr, "use_clip_camera");
  const float stroke_sample = RNA_float_get(op->ptr, "stroke_sample");

  const IOContext io_context(*C, region, v3d, rv3d, op->reports);
  const ExportParams params = {ob,
                               select_mode,
                               frame_mode,
                               export_stroke_materials,
                               export_fill_materials,
                               use_clip_camera,
                               use_uniform_width,
                               stroke_sample};

  WM_cursor_wait(true);
  const bool done = blender::io::grease_pencil::export_svg(io_context, params, *scene, filepath);
  WM_cursor_wait(false);

  if (!done) {
    BKE_report(op->reports, RPT_WARNING, "Unable to export SVG");
  }

  return OPERATOR_FINISHED;
}

static void grease_pencil_export_svg_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *box, *row;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  box = uiLayoutBox(layout);

  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Scene Options"), ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, op->ptr, "selected_object_type", UI_ITEM_NONE, nullptr, ICON_NONE);

  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Export Options"), ICON_NONE);

  uiLayout *col = uiLayoutColumn(box, false);
  uiItemR(col, op->ptr, "stroke_sample", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, op->ptr, "use_fill", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, op->ptr, "use_uniform_width", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, op->ptr, "use_clip_camera", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static bool grease_pencil_export_svg_poll(bContext *C)
{
  if ((CTX_wm_window(C) == nullptr) || (CTX_data_mode_enum(C) != CTX_MODE_OBJECT)) {
    return false;
  }

  return true;
}

}  // namespace blender::ed::io

void WM_OT_grease_pencil_export_svg(wmOperatorType *ot)
{
  ot->name = "Export to SVG";
  ot->description = "Export Grease Pencil to SVG";
  ot->idname = "WM_OT_grease_pencil_export_svg";

  ot->invoke = blender::ed::io::grease_pencil_export_svg_invoke;
  ot->exec = blender::ed::io::grease_pencil_export_svg_exec;
  ot->poll = blender::ed::io::grease_pencil_export_svg_poll;
  ot->ui = blender::ed::io::grease_pencil_export_svg_draw;
  ot->check = blender::ed::io::grease_pencil_export_svg_check;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_OBJECT_IO,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  blender::ed::io::grease_pencil_export_common_props_definition(ot);

  RNA_def_boolean(ot->srna,
                  "use_clip_camera",
                  false,
                  "Clip Camera",
                  "Clip drawings to camera size when exporting in camera view");
}

#  endif /* WITH_PUGIXML */

/** \} */

/* -------------------------------------------------------------------- */
/** \name PDF single frame export
 * \{ */

#  ifdef WITH_HARU

namespace blender::ed::io {

static bool grease_pencil_export_pdf_check(bContext * /*C*/, wmOperator *op)
{

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".pdf")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".pdf");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

static int grease_pencil_export_pdf_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ED_fileselect_ensure_default_filepath(C, op, ".pdf");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_export_pdf_exec(bContext *C, wmOperator *op)
{
  using blender::io::grease_pencil::ExportParams;
  using blender::io::grease_pencil::IOContext;

  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  ARegion *region;
  View3D *v3d;
  RegionView3D *rv3d;
  if (!get_invoke_region(C, &region, &v3d, &rv3d)) {
    BKE_report(op->reports, RPT_ERROR, "Unable to find valid 3D View area");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  const bool export_stroke_materials = true;
  const bool export_fill_materials = RNA_boolean_get(op->ptr, "use_fill");
  const bool use_uniform_width = RNA_boolean_get(op->ptr, "use_uniform_width");
  const ExportParams::SelectMode select_mode = ExportParams::SelectMode(
      RNA_enum_get(op->ptr, "selected_object_type"));
  const ExportParams::FrameMode frame_mode = ExportParams::FrameMode(
      RNA_enum_get(op->ptr, "frame_mode"));
  const bool use_clip_camera = false;
  const float stroke_sample = RNA_float_get(op->ptr, "stroke_sample");

  const IOContext io_context(*C, region, v3d, rv3d, op->reports);
  const ExportParams params = {ob,
                               select_mode,
                               frame_mode,
                               export_stroke_materials,
                               export_fill_materials,
                               use_clip_camera,
                               use_uniform_width,
                               stroke_sample};

  WM_cursor_wait(true);
  const bool done = blender::io::grease_pencil::export_pdf(io_context, params, *scene, filepath);
  WM_cursor_wait(false);

  if (!done) {
    BKE_report(op->reports, RPT_WARNING, "Unable to export PDF");
  }

  return OPERATOR_FINISHED;
}

static void ui_gpencil_export_pdf_settings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayout *box, *row, *col, *sub;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  box = uiLayoutBox(layout);

  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Scene Options"), ICON_NONE);

  row = uiLayoutRow(box, false);
  uiItemR(row, imfptr, "selected_object_type", UI_ITEM_NONE, nullptr, ICON_NONE);

  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Export Options"), ICON_NONE);

  col = uiLayoutColumn(box, false);
  sub = uiLayoutColumn(col, true);
  uiItemR(sub, imfptr, "frame_mode", UI_ITEM_NONE, IFACE_("Frame"), ICON_NONE);

  uiLayoutSetPropSep(box, true);

  sub = uiLayoutColumn(col, true);
  uiItemR(sub, imfptr, "stroke_sample", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(sub, imfptr, "use_fill", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(sub, imfptr, "use_uniform_width", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void grease_pencil_export_pdf_draw(bContext * /*C*/, wmOperator *op)
{
  ui_gpencil_export_pdf_settings(op->layout, op->ptr);
}

static bool grease_pencil_export_pdf_poll(bContext *C)
{
  if ((CTX_wm_window(C) == nullptr) || (CTX_data_mode_enum(C) != CTX_MODE_OBJECT)) {
    return false;
  }

  return true;
}

}  // namespace blender::ed::io

void WM_OT_grease_pencil_export_pdf(wmOperatorType *ot)
{
  ot->name = "Export to PDF";
  ot->description = "Export Grease Pencil to PDF";
  ot->idname = "WM_OT_grease_pencil_export_pdf";

  ot->invoke = blender::ed::io::grease_pencil_export_pdf_invoke;
  ot->exec = blender::ed::io::grease_pencil_export_pdf_exec;
  ot->poll = blender::ed::io::grease_pencil_export_pdf_poll;
  ot->ui = blender::ed::io::grease_pencil_export_pdf_draw;
  ot->check = blender::ed::io::grease_pencil_export_pdf_check;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_OBJECT_IO,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  using blender::io::grease_pencil::ExportParams;

  static const EnumPropertyItem frame_mode_items[] = {
      {int(ExportParams::FrameMode::Active), "ACTIVE", 0, "Active", "Include only active frame"},
      {int(ExportParams::FrameMode::Selected),
       "SELECTED",
       0,
       "Selected",
       "Include selected frames"},
      {int(ExportParams::FrameMode::Scene), "SCENE", 0, "Scene", "Include all scene frames"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  blender::ed::io::grease_pencil_export_common_props_definition(ot);
  ot->prop = RNA_def_enum(ot->srna,
                          "frame_mode",
                          frame_mode_items,
                          int(ExportParams::FrameMode::Active),
                          "Frames",
                          "Which frames to include in the export");
}

#  endif /* WITH_HARU */

/** \} */

namespace blender::ed::io {

void grease_pencil_file_handler_add()
{
  auto fh = std::make_unique<blender::bke::FileHandlerType>();
  STRNCPY(fh->idname, "IO_FH_grease_pencil_svg");
  STRNCPY(fh->import_operator, "WM_OT_grease_pencil_import_svg");
  STRNCPY(fh->label, "SVG as Grease Pencil");
  STRNCPY(fh->file_extensions_str, ".svg");
  fh->poll_drop = poll_file_object_drop;
  bke::file_handler_add(std::move(fh));
}

}  // namespace blender::ed::io

#endif /* WITH_IO_GREASE_PENCIL */
