/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup editor/io
 */

#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"

#include "IO_wavefront_obj.h"
#include "io_obj.h"

const EnumPropertyItem io_obj_transform_axis_forward[] = {
    {OBJ_AXIS_X_FORWARD, "X_FORWARD", 0, "X", "Positive X axis"},
    {OBJ_AXIS_Y_FORWARD, "Y_FORWARD", 0, "Y", "Positive Y axis"},
    {OBJ_AXIS_Z_FORWARD, "Z_FORWARD", 0, "Z", "Positive Z axis"},
    {OBJ_AXIS_NEGATIVE_X_FORWARD, "NEGATIVE_X_FORWARD", 0, "-X", "Negative X axis"},
    {OBJ_AXIS_NEGATIVE_Y_FORWARD, "NEGATIVE_Y_FORWARD", 0, "-Y", "Negative Y axis"},
    {OBJ_AXIS_NEGATIVE_Z_FORWARD, "NEGATIVE_Z_FORWARD", 0, "-Z (Default)", "Negative Z axis"},
    {0, NULL, 0, NULL, NULL}};

const EnumPropertyItem io_obj_transform_axis_up[] = {
    {OBJ_AXIS_X_UP, "X_UP", 0, "X", "Positive X axis"},
    {OBJ_AXIS_Y_UP, "Y_UP", 0, "Y (Default)", "Positive Y axis"},
    {OBJ_AXIS_Z_UP, "Z_UP", 0, "Z", "Positive Z axis"},
    {OBJ_AXIS_NEGATIVE_X_UP, "NEGATIVE_X_UP", 0, "-X", "Negative X axis"},
    {OBJ_AXIS_NEGATIVE_Y_UP, "NEGATIVE_Y_UP", 0, "-Y", "Negative Y axis"},
    {OBJ_AXIS_NEGATIVE_Z_UP, "NEGATIVE_Z_UP", 0, "-Z", "Negative Z axis"},
    {0, NULL, 0, NULL, NULL}};

const EnumPropertyItem io_obj_export_evaluation_mode[] = {
    {DAG_EVAL_RENDER, "DAG_EVAL_RENDER", 0, "Render", "Export objects as they appear in render"},
    {DAG_EVAL_VIEWPORT,
     "DAG_EVAL_VIEWPORT",
     0,
     "Viewport (Default)",
     "Export objects as they appear in the viewport"},
    {0, NULL, 0, NULL, NULL}};

static int wm_obj_export_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    Main *bmain = CTX_data_main(C);
    char filepath[FILE_MAX];

    if (BKE_main_blendfile_path(bmain)[0] == '\0') {
      BLI_strncpy(filepath, "untitled", sizeof(filepath));
    }
    else {
      BLI_strncpy(filepath, BKE_main_blendfile_path(bmain), sizeof(filepath));
    }

    BLI_path_extension_replace(filepath, sizeof(filepath), ".obj");
    RNA_string_set(op->ptr, "filepath", filepath);
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int wm_obj_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }
  struct OBJExportParams export_params;
  RNA_string_get(op->ptr, "filepath", export_params.filepath);
  export_params.blen_filepath = CTX_data_main(C)->filepath;
  export_params.export_animation = RNA_boolean_get(op->ptr, "export_animation");
  export_params.start_frame = RNA_int_get(op->ptr, "start_frame");
  export_params.end_frame = RNA_int_get(op->ptr, "end_frame");

  export_params.forward_axis = RNA_enum_get(op->ptr, "forward_axis");
  export_params.up_axis = RNA_enum_get(op->ptr, "up_axis");
  export_params.scaling_factor = RNA_float_get(op->ptr, "scaling_factor");
  export_params.export_eval_mode = RNA_enum_get(op->ptr, "export_eval_mode");

  export_params.export_selected_objects = RNA_boolean_get(op->ptr, "export_selected_objects");
  export_params.export_uv = RNA_boolean_get(op->ptr, "export_uv");
  export_params.export_normals = RNA_boolean_get(op->ptr, "export_normals");
  export_params.export_materials = RNA_boolean_get(op->ptr, "export_materials");
  export_params.export_triangulated_mesh = RNA_boolean_get(op->ptr, "export_triangulated_mesh");
  export_params.export_curves_as_nurbs = RNA_boolean_get(op->ptr, "export_curves_as_nurbs");

  export_params.export_object_groups = RNA_boolean_get(op->ptr, "export_object_groups");
  export_params.export_material_groups = RNA_boolean_get(op->ptr, "export_material_groups");
  export_params.export_vertex_groups = RNA_boolean_get(op->ptr, "export_vertex_groups");
  export_params.export_smooth_groups = RNA_boolean_get(op->ptr, "export_smooth_groups");
  export_params.smooth_groups_bitflags = RNA_boolean_get(op->ptr, "smooth_group_bitflags");

  OBJ_export(C, &export_params);

  return OPERATOR_FINISHED;
}

static void ui_obj_export_settings(uiLayout *layout, PointerRNA *imfptr)
{

  const bool export_animation = RNA_boolean_get(imfptr, "export_animation");
  const bool export_smooth_groups = RNA_boolean_get(imfptr, "export_smooth_groups");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  /* Animation options. */
  uiLayout *box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Animation"), ICON_ANIM);
  uiLayout *col = uiLayoutColumn(box, false);
  uiLayout *sub = uiLayoutColumn(col, false);
  uiItemR(sub, imfptr, "export_animation", 0, NULL, ICON_NONE);
  sub = uiLayoutColumn(sub, true);
  uiItemR(sub, imfptr, "start_frame", 0, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(sub, imfptr, "end_frame", 0, IFACE_("End"), ICON_NONE);
  uiLayoutSetEnabled(sub, export_animation);

  /* Object Transform options. */
  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Object Properties"), ICON_OBJECT_DATA);
  col = uiLayoutColumn(box, false);
  sub = uiLayoutColumn(col, false);
  uiItemR(sub, imfptr, "forward_axis", 0, IFACE_("Axis Forward"), ICON_NONE);
  uiItemR(sub, imfptr, "up_axis", 0, IFACE_("Up"), ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiItemR(sub, imfptr, "scaling_factor", 0, NULL, ICON_NONE);
  sub = uiLayoutColumnWithHeading(col, false, IFACE_("Objects"));
  uiItemR(sub, imfptr, "export_selected_objects", 0, IFACE_("Selected Only"), ICON_NONE);
  uiItemR(sub, imfptr, "export_eval_mode", 0, IFACE_("Properties"), ICON_NONE);

  /* Options for what to write. */
  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Geometry Export"), ICON_EXPORT);
  col = uiLayoutColumn(box, false);
  sub = uiLayoutColumnWithHeading(col, false, IFACE_("Export"));
  uiItemR(sub, imfptr, "export_uv", 0, IFACE_("UV Coordinates"), ICON_NONE);
  uiItemR(sub, imfptr, "export_normals", 0, IFACE_("Normals"), ICON_NONE);
  uiItemR(sub, imfptr, "export_materials", 0, IFACE_("Materials"), ICON_NONE);
  uiItemR(sub, imfptr, "export_triangulated_mesh", 0, IFACE_("Triangulated Mesh"), ICON_NONE);
  uiItemR(sub, imfptr, "export_curves_as_nurbs", 0, IFACE_("Curves as NURBS"), ICON_NONE);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Grouping"), ICON_GROUP);
  col = uiLayoutColumn(box, false);
  sub = uiLayoutColumnWithHeading(col, false, IFACE_("Export"));
  uiItemR(sub, imfptr, "export_object_groups", 0, IFACE_("Object Groups"), ICON_NONE);
  uiItemR(sub, imfptr, "export_material_groups", 0, IFACE_("Material Groups"), ICON_NONE);
  uiItemR(sub, imfptr, "export_vertex_groups", 0, IFACE_("Vertex Groups"), ICON_NONE);
  uiItemR(sub, imfptr, "export_smooth_groups", 0, IFACE_("Smooth Groups"), ICON_NONE);
  sub = uiLayoutColumn(sub, false);
  uiLayoutSetEnabled(sub, export_smooth_groups);
  uiItemR(sub, imfptr, "smooth_group_bitflags", 0, IFACE_("Smooth Group Bitflags"), ICON_NONE);
}

static void wm_obj_export_draw(bContext *UNUSED(C), wmOperator *op)
{
  PointerRNA ptr;
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  ui_obj_export_settings(op->layout, &ptr);
}

/**
 * Return true if any property in the UI is changed.
 */
static bool wm_obj_export_check(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  Scene *scene = CTX_data_scene(C);
  bool changed = false;
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".obj")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".obj");
    RNA_string_set(op->ptr, "filepath", filepath);
    changed = true;
  }

  {
    int start = RNA_int_get(op->ptr, "start_frame");
    int end = RNA_int_get(op->ptr, "end_frame");
    /* Set the defaults. */
    if (start == INT_MIN) {
      start = SFRA;
      changed = true;
    }
    if (end == INT_MAX) {
      end = EFRA;
      changed = true;
    }
    /* Fix user errors. */
    if (end < start) {
      end = start;
      changed = true;
    }
    RNA_int_set(op->ptr, "start_frame", start);
    RNA_int_set(op->ptr, "end_frame", end);
  }

  /* Both forward and up axes cannot be the same (or same except opposite sign). */
  if (RNA_enum_get(op->ptr, "forward_axis") % TOTAL_AXES ==
      (RNA_enum_get(op->ptr, "up_axis") % TOTAL_AXES)) {
    /* TODO(@ankitm): Show a warning here. */
    RNA_enum_set(op->ptr, "up_axis", RNA_enum_get(op->ptr, "up_axis") % TOTAL_AXES + 1);
    changed = true;
  }
  return changed;
}

void WM_OT_obj_export(struct wmOperatorType *ot)
{
  ot->name = "Export Wavefront OBJ";
  ot->description = "Save the scene to a Wavefront OBJ file";
  ot->idname = "WM_OT_obj_export";

  ot->invoke = wm_obj_export_invoke;
  ot->exec = wm_obj_export_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_obj_export_draw;
  ot->check = wm_obj_export_check;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_OBJECT_IO,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);

  /* Animation options. */
  RNA_def_boolean(ot->srna,
                  "export_animation",
                  false,
                  "Export Animation",
                  "Export multiple frames instead of the current frame only");
  RNA_def_int(ot->srna,
              "start_frame",
              INT_MIN, /* wm_obj_export_check uses this to set SFRA. */
              INT_MIN,
              INT_MAX,
              "Start Frame",
              "The first frame to be exported",
              INT_MIN,
              INT_MAX);
  RNA_def_int(ot->srna,
              "end_frame",
              INT_MAX, /* wm_obj_export_check uses this to set EFRA. */
              INT_MIN,
              INT_MAX,
              "End Frame",
              "The last frame to be exported",
              INT_MIN,
              INT_MAX);
  /* Object transform options. */
  RNA_def_enum(ot->srna,
               "forward_axis",
               io_obj_transform_axis_forward,
               OBJ_AXIS_NEGATIVE_Z_FORWARD,
               "Forward Axis",
               "");
  RNA_def_enum(ot->srna, "up_axis", io_obj_transform_axis_up, OBJ_AXIS_Y_UP, "Up Axis", "");
  RNA_def_float(ot->srna,
                "scaling_factor",
                1.0f,
                0.001f,
                10000.0f,
                "Scale",
                "Upscale the object by this factor",
                0.01,
                1000.0f);
  /* File Writer options. */
  RNA_def_enum(ot->srna,
               "export_eval_mode",
               io_obj_export_evaluation_mode,
               DAG_EVAL_VIEWPORT,
               "Object Properties",
               "Determines properties like object visibility, modifiers etc., where they differ "
               "for Render and Viewport");
  RNA_def_boolean(ot->srna,
                  "export_selected_objects",
                  false,
                  "Export Selected Objects",
                  "Export only selected objects instead of all supported objects");
  RNA_def_boolean(ot->srna, "export_uv", true, "Export UVs", "");
  RNA_def_boolean(ot->srna,
                  "export_normals",
                  true,
                  "Export Normals",
                  "Export per-face normals if the face is flat-shaded, per-face-per-loop "
                  "normals if smooth-shaded");
  RNA_def_boolean(ot->srna,
                  "export_materials",
                  true,
                  "Export Materials",
                  "Export MTL library. There must be a Principled-BSDF node for image textures to "
                  "be exported to the MTL file");
  RNA_def_boolean(ot->srna,
                  "export_triangulated_mesh",
                  false,
                  "Export Triangulated Mesh",
                  "All ngons with four or more vertices will be triangulated. Meshes in "
                  "the scene will not be affected. Behaves like Triangulate Modifier with "
                  "ngon-method: \"Beauty\", quad-method: \"Shortest Diagonal\", min vertices: 4");
  RNA_def_boolean(ot->srna,
                  "export_curves_as_nurbs",
                  false,
                  "Export Curves as NURBS",
                  "Export curves in parametric form instead of exporting as mesh");

  RNA_def_boolean(ot->srna,
                  "export_object_groups",
                  false,
                  "Export Object Groups",
                  "Append mesh name to object name, separated by a '_'");
  RNA_def_boolean(ot->srna,
                  "export_material_groups",
                  false,
                  "Export Material Groups",
                  "Append mesh name and material name to object name, separated by a '_'");
  RNA_def_boolean(
      ot->srna,
      "export_vertex_groups",
      false,
      "Export Vertex Groups",
      "Export the name of the vertex group of a face. It is approximated "
      "by choosing the vertex group with the most members among the vertices of a face");
  RNA_def_boolean(
      ot->srna,
      "export_smooth_groups",
      false,
      "Export Smooth Groups",
      "Every smooth-shaded face is assigned group \"1\" and every flat-shaded face \"off\"");
  RNA_def_boolean(
      ot->srna, "smooth_group_bitflags", false, "Generate Bitflags for Smooth Groups", "");
}
