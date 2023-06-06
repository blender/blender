/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_IO_PLY

#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_report.h"

#  include "WM_api.h"
#  include "WM_types.h"

#  include "DNA_space_types.h"

#  include "ED_fileselect.h"
#  include "ED_outliner.h"

#  include "RNA_access.h"
#  include "RNA_define.h"

#  include "BLT_translation.h"

#  include "MEM_guardedalloc.h"

#  include "UI_interface.h"
#  include "UI_resources.h"

#  include "DEG_depsgraph.h"

#  include "IO_orientation.h"
#  include "IO_path_util_types.h"

#  include "IO_ply.h"
#  include "io_ply_ops.hh"

static const EnumPropertyItem ply_vertex_colors_mode[] = {
    {PLY_VERTEX_COLOR_NONE, "NONE", 0, "None", "Do not import/export color attributes"},
    {PLY_VERTEX_COLOR_SRGB,
     "SRGB",
     0,
     "sRGB",
     "Vertex colors in the file are in sRGB color space"},
    {PLY_VERTEX_COLOR_LINEAR,
     "LINEAR",
     0,
     "Linear",
     "Vertex colors in the file are in linear color space"},
    {0, nullptr, 0, nullptr, nullptr}};

static int wm_ply_export_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ED_fileselect_ensure_default_filepath(C, op, ".ply");

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int wm_ply_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }
  PLYExportParams export_params = {"\0"};
  export_params.file_base_for_tests[0] = '\0';
  RNA_string_get(op->ptr, "filepath", export_params.filepath);
  export_params.blen_filepath = CTX_data_main(C)->filepath;

  export_params.forward_axis = eIOAxis(RNA_enum_get(op->ptr, "forward_axis"));
  export_params.up_axis = eIOAxis(RNA_enum_get(op->ptr, "up_axis"));
  export_params.global_scale = RNA_float_get(op->ptr, "global_scale");
  export_params.apply_modifiers = RNA_boolean_get(op->ptr, "apply_modifiers");

  export_params.export_selected_objects = RNA_boolean_get(op->ptr, "export_selected_objects");
  export_params.export_uv = RNA_boolean_get(op->ptr, "export_uv");
  export_params.export_normals = RNA_boolean_get(op->ptr, "export_normals");
  export_params.vertex_colors = ePLYVertexColorMode(RNA_enum_get(op->ptr, "export_colors"));
  export_params.export_triangulated_mesh = RNA_boolean_get(op->ptr, "export_triangulated_mesh");
  export_params.ascii_format = RNA_boolean_get(op->ptr, "ascii_format");

  PLY_export(C, &export_params);

  return OPERATOR_FINISHED;
}

static void ui_ply_export_settings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *box, *col, *sub;

  /* Object Transform options. */
  box = uiLayoutBox(layout);
  col = uiLayoutColumn(box, false);
  sub = uiLayoutColumnWithHeading(col, false, IFACE_("Format"));
  uiItemR(sub, imfptr, "ascii_format", 0, IFACE_("ASCII"), ICON_NONE);
  sub = uiLayoutColumnWithHeading(col, false, IFACE_("Limit to"));
  uiItemR(sub, imfptr, "export_selected_objects", 0, IFACE_("Selected Only"), ICON_NONE);
  uiItemR(sub, imfptr, "global_scale", 0, nullptr, ICON_NONE);

  uiItemR(sub, imfptr, "forward_axis", 0, IFACE_("Forward Axis"), ICON_NONE);
  uiItemR(sub, imfptr, "up_axis", 0, IFACE_("Up Axis"), ICON_NONE);

  col = uiLayoutColumn(box, false);
  sub = uiLayoutColumn(col, false);
  sub = uiLayoutColumnWithHeading(col, false, IFACE_("Objects"));
  uiItemR(sub, imfptr, "apply_modifiers", 0, IFACE_("Apply Modifiers"), ICON_NONE);

  /* Geometry options. */
  box = uiLayoutBox(layout);
  col = uiLayoutColumn(box, false);
  sub = uiLayoutColumnWithHeading(col, false, IFACE_("Geometry"));
  uiItemR(sub, imfptr, "export_uv", 0, IFACE_("UV Coordinates"), ICON_NONE);
  uiItemR(sub, imfptr, "export_normals", 0, IFACE_("Vertex Normals"), ICON_NONE);
  uiItemR(sub, imfptr, "export_colors", 0, IFACE_("Vertex Colors"), ICON_NONE);
  uiItemR(sub, imfptr, "export_triangulated_mesh", 0, IFACE_("Triangulated Mesh"), ICON_NONE);
}

static void wm_ply_export_draw(bContext * /*C*/, wmOperator *op)
{
  PointerRNA ptr;
  RNA_pointer_create(nullptr, op->type->srna, op->properties, &ptr);
  ui_ply_export_settings(op->layout, &ptr);
}

/**
 * Return true if any property in the UI is changed.
 */
static bool wm_ply_export_check(bContext * /*C*/, wmOperator *op)
{
  char filepath[FILE_MAX];
  bool changed = false;
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".ply")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".ply");
    RNA_string_set(op->ptr, "filepath", filepath);
    changed = true;
  }
  return changed;
}

void WM_OT_ply_export(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Export PLY";
  ot->description = "Save the scene to a PLY file";
  ot->idname = "WM_OT_ply_export";

  ot->invoke = wm_ply_export_invoke;
  ot->exec = wm_ply_export_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_ply_export_draw;
  ot->check = wm_ply_export_check;

  ot->flag = OPTYPE_PRESET;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  /* Object transform options. */
  prop = RNA_def_enum(ot->srna, "forward_axis", io_transform_axis, IO_AXIS_Y, "Forward Axis", "");
  RNA_def_property_update_runtime(prop, (void *)io_ui_forward_axis_update);
  prop = RNA_def_enum(ot->srna, "up_axis", io_transform_axis, IO_AXIS_Z, "Up Axis", "");
  RNA_def_property_update_runtime(prop, (void *)io_ui_up_axis_update);
  RNA_def_float(
      ot->srna,
      "global_scale",
      1.0f,
      0.0001f,
      10000.0f,
      "Scale",
      "Value by which to enlarge or shrink the objects with respect to the world's origin",
      0.0001f,
      10000.0f);
  /* File Writer options. */
  RNA_def_boolean(
      ot->srna, "apply_modifiers", true, "Apply Modifiers", "Apply modifiers to exported meshes");
  RNA_def_boolean(ot->srna,
                  "export_selected_objects",
                  false,
                  "Export Selected Objects",
                  "Export only selected objects instead of all supported objects");
  RNA_def_boolean(ot->srna, "export_uv", true, "Export UVs", "");
  RNA_def_boolean(
      ot->srna,
      "export_normals",
      false,
      "Export Vertex Normals",
      "Export specific vertex normals if available, export calculated normals otherwise");
  RNA_def_enum(ot->srna,
               "export_colors",
               ply_vertex_colors_mode,
               PLY_VERTEX_COLOR_SRGB,
               "Export Vertex Colors",
               "Export vertex color attributes");

  RNA_def_boolean(ot->srna,
                  "export_triangulated_mesh",
                  false,
                  "Export Triangulated Mesh",
                  "All ngons with four or more vertices will be triangulated. Meshes in "
                  "the scene will not be affected. Behaves like Triangulate Modifier with "
                  "ngon-method: \"Beauty\", quad-method: \"Shortest Diagonal\", min vertices: 4");
  RNA_def_boolean(ot->srna,
                  "ascii_format",
                  false,
                  "ASCII Format",
                  "Export file in ASCII format, export as binary otherwise");

  /* Only show .ply files by default. */
  prop = RNA_def_string(ot->srna, "filter_glob", "*.ply", 0, "Extension Filter", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int wm_ply_import_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return WM_operator_filesel(C, op, event);
}

static int wm_ply_import_execute(bContext *C, wmOperator *op)
{
  PLYImportParams params{};
  params.forward_axis = eIOAxis(RNA_enum_get(op->ptr, "forward_axis"));
  params.up_axis = eIOAxis(RNA_enum_get(op->ptr, "up_axis"));
  params.use_scene_unit = RNA_boolean_get(op->ptr, "use_scene_unit");
  params.global_scale = RNA_float_get(op->ptr, "global_scale");
  params.merge_verts = RNA_boolean_get(op->ptr, "merge_verts");
  params.vertex_colors = ePLYVertexColorMode(RNA_enum_get(op->ptr, "import_colors"));

  int files_len = RNA_collection_length(op->ptr, "files");

  if (files_len) {
    PointerRNA fileptr;
    PropertyRNA *prop;
    char dir_only[FILE_MAX], file_only[FILE_MAX];

    RNA_string_get(op->ptr, "directory", dir_only);
    prop = RNA_struct_find_property(op->ptr, "files");
    for (int i = 0; i < files_len; i++) {
      RNA_property_collection_lookup_int(op->ptr, prop, i, &fileptr);
      RNA_string_get(&fileptr, "name", file_only);
      BLI_path_join(params.filepath, sizeof(params.filepath), dir_only, file_only);
      PLY_import(C, &params, op);
    }
  }
  else if (RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    RNA_string_get(op->ptr, "filepath", params.filepath);
    PLY_import(C, &params, op);
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  Scene *scene = CTX_data_scene(C);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void WM_OT_ply_import(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Import PLY";
  ot->description = "Import an PLY file as an object";
  ot->idname = "WM_OT_ply_import";

  ot->invoke = wm_ply_import_invoke;
  ot->exec = wm_ply_import_execute;
  ot->poll = WM_operator_winactive;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_PRESET;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_FILES | WM_FILESEL_DIRECTORY |
                                     WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  RNA_def_float(ot->srna, "global_scale", 1.0f, 1e-6f, 1e6f, "Scale", "", 0.001f, 1000.0f);
  RNA_def_boolean(ot->srna,
                  "use_scene_unit",
                  false,
                  "Scene Unit",
                  "Apply current scene's unit (as defined by unit scale) to imported data");
  prop = RNA_def_enum(ot->srna, "forward_axis", io_transform_axis, IO_AXIS_Y, "Forward Axis", "");
  RNA_def_property_update_runtime(prop, (void *)io_ui_forward_axis_update);
  prop = RNA_def_enum(ot->srna, "up_axis", io_transform_axis, IO_AXIS_Z, "Up Axis", "");
  RNA_def_property_update_runtime(prop, (void *)io_ui_up_axis_update);
  RNA_def_boolean(ot->srna, "merge_verts", false, "Merge Vertices", "Merges vertices by distance");
  RNA_def_enum(ot->srna,
               "import_colors",
               ply_vertex_colors_mode,
               PLY_VERTEX_COLOR_SRGB,
               "Import Vertex Colors",
               "Import vertex color attributes");

  /* Only show .ply files by default. */
  prop = RNA_def_string(ot->srna, "filter_glob", "*.ply", 0, "Extension Filter", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

#endif /* WITH_IO_PLY */
