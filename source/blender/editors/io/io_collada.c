/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup collada
 */
#ifdef WITH_COLLADA
#  include "DNA_space_types.h"

#  include "BLT_translation.h"

#  include "BLI_blenlib.h"
#  include "BLI_utildefines.h"

#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_object.h"
#  include "BKE_report.h"

#  include "DEG_depsgraph.h"

#  include "ED_fileselect.h"
#  include "ED_object.h"

#  include "RNA_access.h"
#  include "RNA_define.h"

#  include "UI_interface.h"
#  include "UI_resources.h"

#  include "WM_api.h"
#  include "WM_types.h"

#  include "collada.h"

#  include "io_collada.h"

static int wm_collada_export_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  ED_fileselect_ensure_default_filepath(C, op, ".dae");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int wm_collada_export_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  int apply_modifiers;
  int global_forward;
  int global_up;
  int apply_global_orientation;
  int export_mesh_type;
  int selected;
  int include_children;
  int include_armatures;
  int include_shapekeys;
  int deform_bones_only;

  int include_animations;
  int include_all_actions;
  int sampling_rate;
  int keep_smooth_curves;
  int keep_keyframes;
  int keep_flat_curves;

  int export_animation_type;
  int use_texture_copies;
  int active_uv_only;

  int triangulate;
  int use_object_instantiation;
  int use_blender_profile;
  int sort_by_name;
  int export_object_transformation_type;
  int export_animation_transformation_type;

  int open_sim;
  int limit_precision;
  int keep_bind_info;

  int export_count;
  int sample_animations;

  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", filepath);
  BLI_path_extension_ensure(filepath, sizeof(filepath), ".dae");

  /* Avoid File write exceptions in Collada */
  if (!BLI_exists(filepath)) {
    BLI_file_ensure_parent_dir_exists(filepath);
    if (!BLI_file_touch(filepath)) {
      BKE_report(op->reports, RPT_ERROR, "Can't create export file");
      fprintf(stdout, "Collada export: Can not create: %s\n", filepath);
      return OPERATOR_CANCELLED;
    }
  }
  else if (!BLI_file_is_writable(filepath)) {
    BKE_report(op->reports, RPT_ERROR, "Can't overwrite export file");
    fprintf(stdout, "Collada export: Can not modify: %s\n", filepath);
    return OPERATOR_CANCELLED;
  }

  /* Now the exporter can create and write the export file */

  /* Options panel */
  apply_modifiers = RNA_boolean_get(op->ptr, "apply_modifiers");
  export_mesh_type = RNA_enum_get(op->ptr, "export_mesh_type_selection");
  global_forward = RNA_enum_get(op->ptr, "export_global_forward_selection");
  global_up = RNA_enum_get(op->ptr, "export_global_up_selection");
  apply_global_orientation = RNA_boolean_get(op->ptr, "apply_global_orientation");

  selected = RNA_boolean_get(op->ptr, "selected");
  include_children = RNA_boolean_get(op->ptr, "include_children");
  include_armatures = RNA_boolean_get(op->ptr, "include_armatures");
  include_shapekeys = RNA_boolean_get(op->ptr, "include_shapekeys");

  include_animations = RNA_boolean_get(op->ptr, "include_animations");
  include_all_actions = RNA_boolean_get(op->ptr, "include_all_actions");
  export_animation_type = RNA_enum_get(op->ptr, "export_animation_type_selection");
  sample_animations = (export_animation_type == BC_ANIMATION_EXPORT_SAMPLES);
  sampling_rate = (sample_animations) ? RNA_int_get(op->ptr, "sampling_rate") : 0;
  keep_smooth_curves = RNA_boolean_get(op->ptr, "keep_smooth_curves");
  keep_keyframes = RNA_boolean_get(op->ptr, "keep_keyframes");
  keep_flat_curves = RNA_boolean_get(op->ptr, "keep_flat_curves");

  deform_bones_only = RNA_boolean_get(op->ptr, "deform_bones_only");

  use_texture_copies = RNA_boolean_get(op->ptr, "use_texture_copies");
  active_uv_only = RNA_boolean_get(op->ptr, "active_uv_only");

  triangulate = RNA_boolean_get(op->ptr, "triangulate");
  use_object_instantiation = RNA_boolean_get(op->ptr, "use_object_instantiation");
  use_blender_profile = RNA_boolean_get(op->ptr, "use_blender_profile");
  sort_by_name = RNA_boolean_get(op->ptr, "sort_by_name");

  export_object_transformation_type = RNA_enum_get(op->ptr,
                                                   "export_object_transformation_type_selection");
  export_animation_transformation_type = RNA_enum_get(
      op->ptr, "export_animation_transformation_type_selection");

  open_sim = RNA_boolean_get(op->ptr, "open_sim");
  limit_precision = RNA_boolean_get(op->ptr, "limit_precision");
  keep_bind_info = RNA_boolean_get(op->ptr, "keep_bind_info");

  Main *bmain = CTX_data_main(C);

  /* get editmode results */
  ED_object_editmode_load(bmain, CTX_data_edit_object(C));

  // Scene *scene = CTX_data_scene(C);

  ExportSettings export_settings;

  export_settings.filepath = filepath;

  export_settings.apply_modifiers = apply_modifiers != 0;
  export_settings.global_forward = global_forward;
  export_settings.global_up = global_up;
  export_settings.apply_global_orientation = apply_global_orientation != 0;

  export_settings.export_mesh_type = export_mesh_type;
  export_settings.selected = selected != 0;
  export_settings.include_children = include_children != 0;
  export_settings.include_armatures = include_armatures != 0;
  export_settings.include_shapekeys = include_shapekeys != 0;
  export_settings.deform_bones_only = deform_bones_only != 0;
  export_settings.include_animations = include_animations != 0;
  export_settings.include_all_actions = include_all_actions != 0;
  export_settings.sampling_rate = sampling_rate;
  export_settings.keep_keyframes = keep_keyframes != 0 || sampling_rate < 1;
  export_settings.keep_flat_curves = keep_flat_curves != 0;

  export_settings.active_uv_only = active_uv_only != 0;
  export_settings.export_animation_type = export_animation_type;
  export_settings.use_texture_copies = use_texture_copies != 0;

  export_settings.triangulate = triangulate != 0;
  export_settings.use_object_instantiation = use_object_instantiation != 0;
  export_settings.use_blender_profile = use_blender_profile != 0;
  export_settings.sort_by_name = sort_by_name != 0;
  export_settings.object_transformation_type = export_object_transformation_type;
  export_settings.animation_transformation_type = export_animation_transformation_type;
  export_settings.keep_smooth_curves = keep_smooth_curves != 0;

  if (export_animation_type != BC_ANIMATION_EXPORT_SAMPLES) {
    /* When curves are exported then we can not export as matrix. */
    export_settings.animation_transformation_type = BC_TRANSFORMATION_TYPE_DECOMPOSED;
  }

  if (export_settings.animation_transformation_type != BC_TRANSFORMATION_TYPE_DECOMPOSED) {
    /* Can not export smooth curves when Matrix export is enabled. */
    export_settings.keep_smooth_curves = false;
  }

  if (include_animations) {
    export_settings.object_transformation_type = export_settings.animation_transformation_type;
  }

  export_settings.open_sim = open_sim != 0;
  export_settings.limit_precision = limit_precision != 0;
  export_settings.keep_bind_info = keep_bind_info != 0;

  export_count = collada_export(C, &export_settings);

  if (export_count == 0) {
    BKE_report(op->reports, RPT_WARNING, "No objects selected -- Created empty export file");
    return OPERATOR_CANCELLED;
  }
  if (export_count < 0) {
    BKE_report(op->reports, RPT_WARNING, "Error during export (see Console)");
    return OPERATOR_CANCELLED;
  }

  char buff[100];
  SNPRINTF(buff, "Exported %d Objects", export_count);
  BKE_report(op->reports, RPT_INFO, buff);
  return OPERATOR_FINISHED;
}

static void uiCollada_exportSettings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayout *box, *row, *col, *sub;
  bool include_animations = RNA_boolean_get(imfptr, "include_animations");
  int ui_section = RNA_enum_get(imfptr, "prop_bc_export_ui_section");

  BC_export_animation_type animation_type = RNA_enum_get(imfptr,
                                                         "export_animation_type_selection");

  BC_export_transformation_type animation_transformation_type = RNA_enum_get(
      imfptr, "export_animation_transformation_type_selection");

  bool sampling = animation_type == BC_ANIMATION_EXPORT_SAMPLES;

  /* Export Options: */
  row = uiLayoutRow(layout, false);
  uiItemR(row, imfptr, "prop_bc_export_ui_section", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  if (ui_section == BC_UI_SECTION_MAIN) {
    /* Export data options. */
    box = uiLayoutBox(layout);
    col = uiLayoutColumn(box, false);
    uiItemR(col, imfptr, "selected", 0, NULL, ICON_NONE);
    sub = uiLayoutColumn(col, false);
    uiLayoutSetEnabled(sub, RNA_boolean_get(imfptr, "selected"));
    uiItemR(sub, imfptr, "include_children", 0, NULL, ICON_NONE);
    uiItemR(sub, imfptr, "include_armatures", 0, NULL, ICON_NONE);
    uiItemR(sub, imfptr, "include_shapekeys", 0, NULL, ICON_NONE);

    box = uiLayoutBox(layout);
    row = uiLayoutRow(box, false);
    uiItemL(row, IFACE_("Global Orientation"), ICON_ORIENTATION_GLOBAL);

    uiItemR(box, imfptr, "apply_global_orientation", 0, IFACE_("Apply"), ICON_NONE);
    uiItemR(box, imfptr, "export_global_forward_selection", 0, IFACE_("Forward Axis"), ICON_NONE);
    uiItemR(box, imfptr, "export_global_up_selection", 0, IFACE_("Up Axis"), ICON_NONE);

    /* Texture options */
    box = uiLayoutBox(layout);
    uiItemL(box, IFACE_("Texture Options"), ICON_TEXTURE_DATA);

    col = uiLayoutColumn(box, false);
    uiItemR(col, imfptr, "use_texture_copies", 0, NULL, ICON_NONE);
    row = uiLayoutRowWithHeading(col, true, IFACE_("UV"));
    uiItemR(row, imfptr, "active_uv_only", 0, IFACE_("Only Selected Map"), ICON_NONE);
  }
  else if (ui_section == BC_UI_SECTION_GEOMETRY) {
    box = uiLayoutBox(layout);
    uiItemL(box, IFACE_("Export Data Options"), ICON_MESH_DATA);

    col = uiLayoutColumn(box, false);

    uiItemR(col, imfptr, "triangulate", 0, NULL, ICON_NONE);

    row = uiLayoutRowWithHeading(col, true, IFACE_("Apply Modifiers"));
    uiItemR(row, imfptr, "apply_modifiers", 0, "", ICON_NONE);
    sub = uiLayoutColumn(row, false);
    uiLayoutSetActive(sub, RNA_boolean_get(imfptr, "apply_modifiers"));
    uiItemR(sub, imfptr, "export_mesh_type_selection", 0, "", ICON_NONE);

    if (RNA_boolean_get(imfptr, "include_animations")) {
      uiItemR(col, imfptr, "export_animation_transformation_type_selection", 0, NULL, ICON_NONE);
    }
    else {
      uiItemR(col, imfptr, "export_object_transformation_type_selection", 0, NULL, ICON_NONE);
    }
  }
  else if (ui_section == BC_UI_SECTION_ARMATURE) {
    /* Armature options */
    box = uiLayoutBox(layout);
    uiItemL(box, IFACE_("Armature Options"), ICON_ARMATURE_DATA);

    col = uiLayoutColumn(box, false);
    uiItemR(col, imfptr, "deform_bones_only", 0, NULL, ICON_NONE);
    uiItemR(col, imfptr, "open_sim", 0, NULL, ICON_NONE);
  }
  else if (ui_section == BC_UI_SECTION_ANIMATION) {
    /* Animation options. */
    box = uiLayoutBox(layout);
    uiItemR(box, imfptr, "include_animations", 0, NULL, ICON_NONE);

    col = uiLayoutColumn(box, false);
    row = uiLayoutRow(col, false);
    uiLayoutSetActive(row, include_animations);
    uiItemR(row, imfptr, "export_animation_type_selection", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

    uiLayoutSetActive(row, include_animations && animation_type == BC_ANIMATION_EXPORT_SAMPLES);
    if (RNA_boolean_get(imfptr, "include_animations")) {
      uiItemR(box, imfptr, "export_animation_transformation_type_selection", 0, NULL, ICON_NONE);
    }
    else {
      uiItemR(box, imfptr, "export_object_transformation_type_selection", 0, NULL, ICON_NONE);
    }

    row = uiLayoutColumn(col, false);
    uiLayoutSetActive(row,
                      include_animations &&
                          (animation_transformation_type == BC_TRANSFORMATION_TYPE_DECOMPOSED ||
                           animation_type == BC_ANIMATION_EXPORT_KEYS));
    uiItemR(row, imfptr, "keep_smooth_curves", 0, NULL, ICON_NONE);

    sub = uiLayoutColumn(col, false);
    uiLayoutSetActive(sub, sampling && include_animations);
    uiItemR(sub, imfptr, "sampling_rate", 0, NULL, ICON_NONE);
    uiItemR(sub, imfptr, "keep_keyframes", 0, NULL, ICON_NONE);

    sub = uiLayoutColumn(col, false);
    uiLayoutSetActive(sub, include_animations);
    uiItemR(sub, imfptr, "keep_flat_curves", 0, NULL, ICON_NONE);
    uiItemR(sub, imfptr, "include_all_actions", 0, NULL, ICON_NONE);
  }
  else if (ui_section == BC_UI_SECTION_COLLADA) {
    /* Collada options: */
    box = uiLayoutBox(layout);
    row = uiLayoutRow(box, false);
    uiItemL(row, IFACE_("Collada Options"), ICON_MODIFIER);

    col = uiLayoutColumn(box, false);
    uiItemR(col, imfptr, "use_object_instantiation", 1, NULL, ICON_NONE);
    uiItemR(col, imfptr, "use_blender_profile", 1, NULL, ICON_NONE);
    uiItemR(col, imfptr, "sort_by_name", 0, NULL, ICON_NONE);
    uiItemR(col, imfptr, "keep_bind_info", 0, NULL, ICON_NONE);
    uiItemR(col, imfptr, "limit_precision", 0, NULL, ICON_NONE);
  }
}

static void wm_collada_export_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiCollada_exportSettings(op->layout, op->ptr);
}

static bool wm_collada_export_check(bContext *UNUSED(C), wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".dae")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".dae");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

void WM_OT_collada_export(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_bc_export_mesh_type[] = {
      {BC_MESH_TYPE_VIEW, "view", 0, "Viewport", "Apply modifier's viewport settings"},
      {BC_MESH_TYPE_RENDER, "render", 0, "Render", "Apply modifier's render settings"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_bc_export_global_forward[] = {
      {BC_GLOBAL_FORWARD_X, "X", 0, "X", "Global Forward is positive X Axis"},
      {BC_GLOBAL_FORWARD_Y, "Y", 0, "Y", "Global Forward is positive Y Axis"},
      {BC_GLOBAL_FORWARD_Z, "Z", 0, "Z", "Global Forward is positive Z Axis"},
      {BC_GLOBAL_FORWARD_MINUS_X, "-X", 0, "-X", "Global Forward is negative X Axis"},
      {BC_GLOBAL_FORWARD_MINUS_Y, "-Y", 0, "-Y", "Global Forward is negative Y Axis"},
      {BC_GLOBAL_FORWARD_MINUS_Z, "-Z", 0, "-Z", "Global Forward is negative Z Axis"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_bc_export_global_up[] = {
      {BC_GLOBAL_UP_X, "X", 0, "X", "Global UP is positive X Axis"},
      {BC_GLOBAL_UP_Y, "Y", 0, "Y", "Global UP is positive Y Axis"},
      {BC_GLOBAL_UP_Z, "Z", 0, "Z", "Global UP is positive Z Axis"},
      {BC_GLOBAL_UP_MINUS_X, "-X", 0, "-X", "Global UP is negative X Axis"},
      {BC_GLOBAL_UP_MINUS_Y, "-Y", 0, "-Y", "Global UP is negative Y Axis"},
      {BC_GLOBAL_UP_MINUS_Z, "-Z", 0, "-Z", "Global UP is negative Z Axis"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_bc_export_transformation_type[] = {
      {BC_TRANSFORMATION_TYPE_MATRIX,
       "matrix",
       0,
       "Matrix",
       "Use <matrix> representation for exported transformations"},
      {BC_TRANSFORMATION_TYPE_DECOMPOSED,
       "decomposed",
       0,
       "Decomposed",
       "Use <rotate>, <translate> and <scale> representation for exported transformations"},
      {0, NULL, 0, NULL, NULL}};

  static const EnumPropertyItem prop_bc_export_animation_type[] = {
      {BC_ANIMATION_EXPORT_SAMPLES,
       "sample",
       0,
       "Samples",
       "Export Sampled points guided by sampling rate"},
      {BC_ANIMATION_EXPORT_KEYS,
       "keys",
       0,
       "Curves",
       "Export Curves (note: guided by curve keys)"},
      {0, NULL, 0, NULL, NULL}};

  static const EnumPropertyItem prop_bc_export_ui_section[] = {
      {BC_UI_SECTION_MAIN, "main", 0, "Main", "Data export section"},
      {BC_UI_SECTION_GEOMETRY, "geometry", 0, "Geom", "Geometry export section"},
      {BC_UI_SECTION_ARMATURE, "armature", 0, "Arm", "Armature export section"},
      {BC_UI_SECTION_ANIMATION, "animation", 0, "Anim", "Animation export section"},
      {BC_UI_SECTION_COLLADA, "collada", 0, "Extra", "Collada export section"},
      {0, NULL, 0, NULL, NULL}};

  ot->name = "Export COLLADA";
  ot->description = "Save a Collada file";
  ot->idname = "WM_OT_collada_export";

  ot->invoke = wm_collada_export_invoke;
  ot->exec = wm_collada_export_exec;
  ot->poll = WM_operator_winactive;
  ot->check = wm_collada_export_check;

  ot->flag = OPTYPE_PRESET;

  ot->ui = wm_collada_export_draw;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_COLLADA,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.dae", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_enum(ot->srna,
               "prop_bc_export_ui_section",
               prop_bc_export_ui_section,
               0,
               "Export Section",
               "Only for User Interface organization");

  RNA_def_boolean(ot->srna,
                  "apply_modifiers",
                  0,
                  "Apply Modifiers",
                  "Apply modifiers to exported mesh (non destructive)");

  RNA_def_int(ot->srna,
              "export_mesh_type",
              0,
              INT_MIN,
              INT_MAX,
              "Resolution",
              "Modifier resolution for export",
              INT_MIN,
              INT_MAX);

  RNA_def_enum(ot->srna,
               "export_mesh_type_selection",
               prop_bc_export_mesh_type,
               0,
               "Resolution",
               "Modifier resolution for export");

  RNA_def_enum(ot->srna,
               "export_global_forward_selection",
               prop_bc_export_global_forward,
               BC_DEFAULT_FORWARD,
               "Global Forward Axis",
               "Global Forward axis for export");

  RNA_def_enum(ot->srna,
               "export_global_up_selection",
               prop_bc_export_global_up,
               BC_DEFAULT_UP,
               "Global Up Axis",
               "Global Up axis for export");

  RNA_def_boolean(ot->srna,
                  "apply_global_orientation",
                  false,
                  "Apply Global Orientation",
                  "Rotate all root objects to match the global orientation settings "
                  "otherwise set the global orientation per Collada asset");

  RNA_def_boolean(ot->srna, "selected", false, "Selection Only", "Export only selected elements");

  RNA_def_boolean(ot->srna,
                  "include_children",
                  false,
                  "Include Children",
                  "Export all children of selected objects (even if not selected)");

  RNA_def_boolean(ot->srna,
                  "include_armatures",
                  false,
                  "Include Armatures",
                  "Export related armatures (even if not selected)");

  RNA_def_boolean(ot->srna,
                  "include_shapekeys",
                  false,
                  "Include Shape Keys",
                  "Export all Shape Keys from Mesh Objects");

  RNA_def_boolean(ot->srna,
                  "deform_bones_only",
                  false,
                  "Deform Bones Only",
                  "Only export deforming bones with armatures");

  RNA_def_boolean(
      ot->srna,
      "include_animations",
      true,
      "Include Animations",
      "Export animations if available (exporting animations will enforce the decomposition of "
      "node transforms into  <translation> <rotation> and <scale> components)");

  RNA_def_boolean(ot->srna,
                  "include_all_actions",
                  true,
                  "Include all Actions",
                  "Export also unassigned actions (this allows you to export entire animation "
                  "libraries for your character(s))");

  RNA_def_enum(ot->srna,
               "export_animation_type_selection",
               prop_bc_export_animation_type,
               0,
               "Key Type",
               "Type for exported animations (use sample keys or Curve keys)");

  RNA_def_int(ot->srna,
              "sampling_rate",
              1,
              1,
              INT_MAX,
              "Sampling Rate",
              "The distance between 2 keyframes (1 to key every frame)",
              1,
              INT_MAX);

  RNA_def_boolean(ot->srna,
                  "keep_smooth_curves",
                  0,
                  "Keep Smooth curves",
                  "Export also the curve handles (if available) (this does only work when the "
                  "inverse parent matrix "
                  "is the unity matrix, otherwise you may end up with odd results)");

  RNA_def_boolean(ot->srna,
                  "keep_keyframes",
                  0,
                  "Keep Keyframes",
                  "Use existing keyframes as additional sample points (this helps when you want "
                  "to keep manual tweaks)");

  RNA_def_boolean(ot->srna,
                  "keep_flat_curves",
                  0,
                  "All Keyed Curves",
                  "Export also curves which have only one key or are totally flat");

  RNA_def_boolean(
      ot->srna, "active_uv_only", 0, "Only Selected UV Map", "Export only the selected UV Map");

  RNA_def_boolean(ot->srna,
                  "use_texture_copies",
                  1,
                  "Copy",
                  "Copy textures to same folder where the .dae file is exported");

  RNA_def_boolean(ot->srna,
                  "triangulate",
                  1,
                  "Triangulate",
                  "Export polygons (quads and n-gons) as triangles");

  RNA_def_boolean(ot->srna,
                  "use_object_instantiation",
                  1,
                  "Use Object Instances",
                  "Instantiate multiple Objects from same Data");

  RNA_def_boolean(
      ot->srna,
      "use_blender_profile",
      1,
      "Use Blender Profile",
      "Export additional Blender specific information (for material, shaders, bones, etc.)");

  RNA_def_boolean(
      ot->srna, "sort_by_name", 0, "Sort by Object name", "Sort exported data by Object name");

  RNA_def_int(ot->srna,
              "export_object_transformation_type",
              0,
              INT_MIN,
              INT_MAX,
              "Transform",
              "Object Transformation type for translation, scale and rotation",
              INT_MIN,
              INT_MAX);

  RNA_def_enum(ot->srna,
               "export_object_transformation_type_selection",
               prop_bc_export_transformation_type,
               0,
               "Transform",
               "Object Transformation type for translation, scale and rotation");

  RNA_def_int(ot->srna,
              "export_animation_transformation_type",
              0,
              INT_MIN,
              INT_MAX,
              "Transform",
              "Transformation type for translation, scale and rotation. "
              "Note: The Animation transformation type in the Anim Tab "
              "is always equal to the Object transformation type in the Geom tab",
              INT_MIN,
              INT_MAX);

  RNA_def_enum(ot->srna,
               "export_animation_transformation_type_selection",
               prop_bc_export_transformation_type,
               0,
               "Transform",
               "Transformation type for translation, scale and rotation. "
               "Note: The Animation transformation type in the Anim Tab "
               "is always equal to the Object transformation type in the Geom tab");

  RNA_def_boolean(ot->srna,
                  "open_sim",
                  0,
                  "Export to SL/OpenSim",
                  "Compatibility mode for SL, OpenSim and other compatible online worlds");

  RNA_def_boolean(ot->srna,
                  "limit_precision",
                  0,
                  "Limit Precision",
                  "Reduce the precision of the exported data to 6 digits");

  RNA_def_boolean(
      ot->srna,
      "keep_bind_info",
      0,
      "Keep Bind Info",
      "Store Bindpose information in custom bone properties for later use during Collada export");
}

static int wm_collada_import_exec(bContext *C, wmOperator *op)
{
  char filename[FILE_MAX];
  int import_units;
  int find_chains;
  int auto_connect;
  int fix_orientation;
  int min_chain_length;

  int keep_bind_info;
  int custom_normals;
  ImportSettings import_settings;

  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  /* Options panel */
  import_units = RNA_boolean_get(op->ptr, "import_units");
  custom_normals = RNA_boolean_get(op->ptr, "custom_normals");
  find_chains = RNA_boolean_get(op->ptr, "find_chains");
  auto_connect = RNA_boolean_get(op->ptr, "auto_connect");
  fix_orientation = RNA_boolean_get(op->ptr, "fix_orientation");

  keep_bind_info = RNA_boolean_get(op->ptr, "keep_bind_info");

  min_chain_length = RNA_int_get(op->ptr, "min_chain_length");

  RNA_string_get(op->ptr, "filepath", filename);

  import_settings.filepath = filename;
  import_settings.import_units = import_units != 0;
  import_settings.custom_normals = custom_normals != 0;
  import_settings.auto_connect = auto_connect != 0;
  import_settings.find_chains = find_chains != 0;
  import_settings.fix_orientation = fix_orientation != 0;
  import_settings.min_chain_length = min_chain_length;
  import_settings.keep_bind_info = keep_bind_info != 0;

  if (collada_import(C, &import_settings)) {
    DEG_id_tag_update(&CTX_data_scene(C)->id, ID_RECALC_BASE_FLAGS);
    return OPERATOR_FINISHED;
  }

  BKE_report(op->reports, RPT_ERROR, "Parsing errors in Document (see Blender Console)");
  return OPERATOR_CANCELLED;
}

static void uiCollada_importSettings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayout *box, *col;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  /* Import Options: */
  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Import Data Options"), ICON_MESH_DATA);

  uiItemR(box, imfptr, "import_units", 0, NULL, ICON_NONE);
  uiItemR(box, imfptr, "custom_normals", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Armature Options"), ICON_ARMATURE_DATA);

  col = uiLayoutColumn(box, false);
  uiItemR(col, imfptr, "fix_orientation", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "find_chains", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "auto_connect", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "min_chain_length", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);

  uiItemR(box, imfptr, "keep_bind_info", 0, NULL, ICON_NONE);
}

static void wm_collada_import_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiCollada_importSettings(op->layout, op->ptr);
}

void WM_OT_collada_import(wmOperatorType *ot)
{
  ot->name = "Import COLLADA";
  ot->description = "Load a Collada file";
  ot->idname = "WM_OT_collada_import";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_PRESET;

  ot->invoke = WM_operator_filesel;
  ot->exec = wm_collada_import_exec;
  ot->poll = WM_operator_winactive;

  ot->ui = wm_collada_import_draw;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_COLLADA,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.dae", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_boolean(ot->srna,
                  "import_units",
                  0,
                  "Import Units",
                  "If disabled match import to Blender's current Unit settings, "
                  "otherwise use the settings from the Imported scene");

  RNA_def_boolean(ot->srna,
                  "custom_normals",
                  1,
                  "Custom Normals",
                  "Import custom normals, if available (otherwise Blender will compute them)");

  RNA_def_boolean(ot->srna,
                  "fix_orientation",
                  0,
                  "Fix Leaf Bones",
                  "Fix Orientation of Leaf Bones (Collada does only support Joints)");

  RNA_def_boolean(ot->srna,
                  "find_chains",
                  0,
                  "Find Bone Chains",
                  "Find best matching Bone Chains and ensure bones in chain are connected");

  RNA_def_boolean(ot->srna,
                  "auto_connect",
                  0,
                  "Auto Connect",
                  "Set use_connect for parent bones which have exactly one child bone");

  RNA_def_int(ot->srna,
              "min_chain_length",
              0,
              0,
              INT_MAX,
              "Minimum Chain Length",
              "When searching Bone Chains disregard chains of length below this value",
              0,
              INT_MAX);

  RNA_def_boolean(
      ot->srna,
      "keep_bind_info",
      0,
      "Keep Bind Info",
      "Store Bindpose information in custom bone properties for later use during Collada export");
}
#endif
