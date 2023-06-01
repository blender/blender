/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_USD
#  include "DNA_modifier_types.h"
#  include "DNA_space_types.h"
#  include <string.h>

#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_report.h"

#  include "BLI_blenlib.h"
#  include "BLI_path_util.h"
#  include "BLI_string.h"
#  include "BLI_utildefines.h"

#  include "BLT_translation.h"

#  include "ED_fileselect.h"
#  include "ED_object.h"

#  include "MEM_guardedalloc.h"

#  include "RNA_access.h"
#  include "RNA_define.h"

#  include "RNA_enum_types.h"

#  include "UI_interface.h"
#  include "UI_resources.h"

#  include "WM_api.h"
#  include "WM_types.h"

#  include "DEG_depsgraph.h"

#  include "io_usd.h"
#  include "usd.h"

#  include <stdio.h>

const EnumPropertyItem rna_enum_usd_export_evaluation_mode_items[] = {
    {DAG_EVAL_RENDER,
     "RENDER",
     0,
     "Render",
     "Use Render settings for object visibility, modifier settings, etc"},
    {DAG_EVAL_VIEWPORT,
     "VIEWPORT",
     0,
     "Viewport",
     "Use Viewport settings for object visibility, modifier settings, etc"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_usd_mtl_name_collision_mode_items[] = {
    {USD_MTL_NAME_COLLISION_MAKE_UNIQUE,
     "MAKE_UNIQUE",
     0,
     "Make Unique",
     "Import each USD material as a unique Blender material"},
    {USD_MTL_NAME_COLLISION_REFERENCE_EXISTING,
     "REFERENCE_EXISTING",
     0,
     "Reference Existing",
     "If a material with the same name already exists, reference that instead of importing"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_usd_tex_import_mode_items[] = {
    {USD_TEX_IMPORT_NONE, "IMPORT_NONE", 0, "None", "Don't import textures"},
    {USD_TEX_IMPORT_PACK, "IMPORT_PACK", 0, "Packed", "Import textures as packed data"},
    {USD_TEX_IMPORT_COPY, "IMPORT_COPY", 0, "Copy", "Copy files to textures directory"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_usd_tex_name_collision_mode_items[] = {
    {USD_TEX_NAME_COLLISION_USE_EXISTING,
     "USE_EXISTING",
     0,
     "Use Existing",
     "If a file with the same name already exists, use that instead of copying"},
    {USD_TEX_NAME_COLLISION_OVERWRITE, "OVERWRITE", 0, "Overwrite", "Overwrite existing files"},
    {0, NULL, 0, NULL, NULL},
};

/* Stored in the wmOperator's customdata field to indicate it should run as a background job.
 * This is set when the operator is invoked, and not set when it is only executed. */
enum { AS_BACKGROUND_JOB = 1 };
typedef struct eUSDOperatorOptions {
  bool as_background_job;
} eUSDOperatorOptions;

/* Ensure that the prim_path is not set to
 * the absolute root path '/'. */
static void process_prim_path(char *prim_path)
{
  if (prim_path == NULL || prim_path[0] == '\0') {
    return;
  }

  /* The absolute root "/" path indicates a no-op,
   * so clear the string. */
  if (prim_path[0] == '/' && strlen(prim_path) == 1) {
    prim_path[0] = '\0';
  }
}

static int wm_usd_export_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  eUSDOperatorOptions *options = MEM_callocN(sizeof(eUSDOperatorOptions), "eUSDOperatorOptions");
  options->as_background_job = true;
  op->customdata = options;

  ED_fileselect_ensure_default_filepath(C, op, ".usdc");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int wm_usd_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  eUSDOperatorOptions *options = (eUSDOperatorOptions *)op->customdata;
  const bool as_background_job = (options != NULL && options->as_background_job);
  MEM_SAFE_FREE(op->customdata);

  const bool selected_objects_only = RNA_boolean_get(op->ptr, "selected_objects_only");
  const bool visible_objects_only = RNA_boolean_get(op->ptr, "visible_objects_only");
  const bool export_animation = RNA_boolean_get(op->ptr, "export_animation");
  const bool export_hair = RNA_boolean_get(op->ptr, "export_hair");
  const bool export_uvmaps = RNA_boolean_get(op->ptr, "export_uvmaps");
  const bool export_normals = RNA_boolean_get(op->ptr, "export_normals");
  const bool export_materials = RNA_boolean_get(op->ptr, "export_materials");
  const bool use_instancing = RNA_boolean_get(op->ptr, "use_instancing");
  const bool evaluation_mode = RNA_enum_get(op->ptr, "evaluation_mode");

  const bool generate_preview_surface = RNA_boolean_get(op->ptr, "generate_preview_surface");
  const bool export_textures = RNA_boolean_get(op->ptr, "export_textures");
  const bool overwrite_textures = RNA_boolean_get(op->ptr, "overwrite_textures");
  const bool relative_paths = RNA_boolean_get(op->ptr, "relative_paths");

  char root_prim_path[FILE_MAX];
  RNA_string_get(op->ptr, "root_prim_path", root_prim_path);
  process_prim_path(root_prim_path);

  struct USDExportParams params = {
      export_animation,
      export_hair,
      export_uvmaps,
      export_normals,
      export_materials,
      selected_objects_only,
      visible_objects_only,
      use_instancing,
      evaluation_mode,
      generate_preview_surface,
      export_textures,
      overwrite_textures,
      relative_paths,
  };

  STRNCPY(params.root_prim_path, root_prim_path);

  bool ok = USD_export(C, filepath, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void wm_usd_export_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *col;
  struct PointerRNA *ptr = op->ptr;

  uiLayoutSetPropSep(layout, true);

  uiLayout *box = uiLayoutBox(layout);

  col = uiLayoutColumn(box, true);
  uiItemR(col, ptr, "selected_objects_only", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "visible_objects_only", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(box, true);
  uiItemR(col, ptr, "export_animation", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_hair", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_uvmaps", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_normals", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_materials", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "root_prim_path", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(box, true);
  uiItemR(col, ptr, "evaluation_mode", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Materials"));
  uiItemR(col, ptr, "generate_preview_surface", 0, NULL, ICON_NONE);
  const bool export_mtl = RNA_boolean_get(ptr, "export_materials");
  uiLayoutSetActive(col, export_mtl);

  uiLayout *row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "export_textures", 0, NULL, ICON_NONE);
  const bool preview = RNA_boolean_get(ptr, "generate_preview_surface");
  uiLayoutSetActive(row, export_mtl && preview);

  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "overwrite_textures", 0, NULL, ICON_NONE);
  const bool export_tex = RNA_boolean_get(ptr, "export_textures");
  uiLayoutSetActive(row, export_mtl && preview && export_tex);

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("File References"));
  uiItemR(col, ptr, "relative_paths", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Experimental"), ICON_NONE);
  uiItemR(box, ptr, "use_instancing", 0, NULL, ICON_NONE);
}

static void free_operator_customdata(wmOperator *op)
{
  if (op->customdata) {
    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }
}

static void wm_usd_export_cancel(bContext *UNUSED(C), wmOperator *op)
{
  free_operator_customdata(op);
}

static bool wm_usd_export_check(bContext *UNUSED(C), wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check_n(filepath, ".usd", ".usda", ".usdc", ".usdz", NULL)) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".usdc");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

void WM_OT_usd_export(struct wmOperatorType *ot)
{
  ot->name = "Export USD";
  ot->description = "Export current scene in a USD archive";
  ot->idname = "WM_OT_usd_export";

  ot->invoke = wm_usd_export_invoke;
  ot->exec = wm_usd_export_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_usd_export_draw;
  ot->cancel = wm_usd_export_cancel;
  ot->check = wm_usd_export_check;

  ot->flag = OPTYPE_REGISTER | OPTYPE_PRESET; /* No UNDO possible. */

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_USD,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.usd", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_boolean(ot->srna,
                  "selected_objects_only",
                  false,
                  "Selection Only",
                  "Only export selected objects. Unselected parents of selected objects are "
                  "exported as empty transform");

  RNA_def_boolean(ot->srna,
                  "visible_objects_only",
                  true,
                  "Visible Only",
                  "Only export visible objects. Invisible parents of exported objects are "
                  "exported as empty transforms");

  RNA_def_boolean(
      ot->srna,
      "export_animation",
      false,
      "Animation",
      "Export all frames in the render frame range, rather than only the current frame");
  RNA_def_boolean(
      ot->srna, "export_hair", false, "Hair", "Export hair particle systems as USD curves");
  RNA_def_boolean(
      ot->srna, "export_uvmaps", true, "UV Maps", "Include all mesh UV maps in the export");
  RNA_def_boolean(ot->srna,
                  "export_normals",
                  true,
                  "Normals",
                  "Include normals of exported meshes in the export");
  RNA_def_boolean(ot->srna,
                  "export_materials",
                  true,
                  "Materials",
                  "Export viewport settings of materials as USD preview materials, and export "
                  "material assignments as geometry subsets");

  RNA_def_boolean(ot->srna,
                  "use_instancing",
                  false,
                  "Instancing",
                  "Export instanced objects as references in USD rather than real objects");

  RNA_def_enum(ot->srna,
               "evaluation_mode",
               rna_enum_usd_export_evaluation_mode_items,
               DAG_EVAL_RENDER,
               "Use Settings for",
               "Determines visibility of objects, modifier settings, and other areas where there "
               "are different settings for viewport and rendering");

  RNA_def_boolean(ot->srna,
                  "generate_preview_surface",
                  true,
                  "To USD Preview Surface",
                  "Generate an approximate USD Preview Surface shader "
                  "representation of a Principled BSDF node network");

  RNA_def_boolean(ot->srna,
                  "export_textures",
                  true,
                  "Export Textures",
                  "If exporting materials, export textures referenced by material nodes "
                  "to a 'textures' directory in the same directory as the USD file");

  RNA_def_boolean(ot->srna,
                  "overwrite_textures",
                  false,
                  "Overwrite Textures",
                  "Overwrite existing files when exporting textures");

  RNA_def_boolean(ot->srna,
                  "relative_paths",
                  true,
                  "Relative Paths",
                  "Use relative paths to reference external files (i.e. textures, volumes) in "
                  "USD, otherwise use absolute paths");

  RNA_def_string(ot->srna,
                 "root_prim_path",
                 NULL,
                 FILE_MAX,
                 "Root Prim",
                 "If set, add a transform primitive with the given path to the stage "
                 "as the parent of all exported data");
}

/* ====== USD Import ====== */

static int wm_usd_import_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  eUSDOperatorOptions *options = MEM_callocN(sizeof(eUSDOperatorOptions), "eUSDOperatorOptions");
  options->as_background_job = true;
  op->customdata = options;

  return WM_operator_filesel(C, op, event);
}

static int wm_usd_import_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  eUSDOperatorOptions *options = (eUSDOperatorOptions *)op->customdata;
  const bool as_background_job = (options != NULL && options->as_background_job);
  MEM_SAFE_FREE(op->customdata);

  const float scale = RNA_float_get(op->ptr, "scale");

  const bool set_frame_range = RNA_boolean_get(op->ptr, "set_frame_range");

  const bool read_mesh_uvs = RNA_boolean_get(op->ptr, "read_mesh_uvs");
  const bool read_mesh_colors = RNA_boolean_get(op->ptr, "read_mesh_colors");

  char mesh_read_flag = MOD_MESHSEQ_READ_VERT | MOD_MESHSEQ_READ_POLY;
  if (read_mesh_uvs) {
    mesh_read_flag |= MOD_MESHSEQ_READ_UV;
  }
  if (read_mesh_colors) {
    mesh_read_flag |= MOD_MESHSEQ_READ_COLOR;
  }

  const bool import_cameras = RNA_boolean_get(op->ptr, "import_cameras");
  const bool import_curves = RNA_boolean_get(op->ptr, "import_curves");
  const bool import_lights = RNA_boolean_get(op->ptr, "import_lights");
  const bool import_materials = RNA_boolean_get(op->ptr, "import_materials");
  const bool import_meshes = RNA_boolean_get(op->ptr, "import_meshes");
  const bool import_volumes = RNA_boolean_get(op->ptr, "import_volumes");
  const bool import_shapes = RNA_boolean_get(op->ptr, "import_shapes");

  const bool import_subdiv = RNA_boolean_get(op->ptr, "import_subdiv");

  const bool import_instance_proxies = RNA_boolean_get(op->ptr, "import_instance_proxies");

  const bool import_visible_only = RNA_boolean_get(op->ptr, "import_visible_only");

  const bool create_collection = RNA_boolean_get(op->ptr, "create_collection");

  char *prim_path_mask = RNA_string_get_alloc(op->ptr, "prim_path_mask", NULL, 0, NULL);

  const bool import_guide = RNA_boolean_get(op->ptr, "import_guide");
  const bool import_proxy = RNA_boolean_get(op->ptr, "import_proxy");
  const bool import_render = RNA_boolean_get(op->ptr, "import_render");

  const bool import_all_materials = RNA_boolean_get(op->ptr, "import_all_materials");

  const bool import_usd_preview = RNA_boolean_get(op->ptr, "import_usd_preview");
  const bool set_material_blend = RNA_boolean_get(op->ptr, "set_material_blend");

  const float light_intensity_scale = RNA_float_get(op->ptr, "light_intensity_scale");

  const eUSDMtlNameCollisionMode mtl_name_collision_mode = RNA_enum_get(op->ptr,
                                                                        "mtl_name_collision_mode");

  /* TODO(makowalski): Add support for sequences. */
  const bool is_sequence = false;
  int offset = 0;
  int sequence_len = 1;

  /* Switch out of edit mode to avoid being stuck in it (#54326). */
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    ED_object_mode_set(C, OB_MODE_EDIT);
  }

  const bool validate_meshes = false;
  const bool use_instancing = false;

  const eUSDTexImportMode import_textures_mode = RNA_enum_get(op->ptr, "import_textures_mode");

  char import_textures_dir[FILE_MAXDIR];
  RNA_string_get(op->ptr, "import_textures_dir", import_textures_dir);

  const eUSDTexNameCollisionMode tex_name_collision_mode = RNA_enum_get(op->ptr,
                                                                        "tex_name_collision_mode");

  struct USDImportParams params = {.scale = scale,
                                   .is_sequence = is_sequence,
                                   .set_frame_range = set_frame_range,
                                   .sequence_len = sequence_len,
                                   .offset = offset,
                                   .validate_meshes = validate_meshes,
                                   .mesh_read_flag = mesh_read_flag,
                                   .import_cameras = import_cameras,
                                   .import_curves = import_curves,
                                   .import_lights = import_lights,
                                   .import_materials = import_materials,
                                   .import_meshes = import_meshes,
                                   .import_volumes = import_volumes,
                                   .import_shapes = import_shapes,
                                   .prim_path_mask = prim_path_mask,
                                   .import_subdiv = import_subdiv,
                                   .import_instance_proxies = import_instance_proxies,
                                   .create_collection = create_collection,
                                   .import_guide = import_guide,
                                   .import_proxy = import_proxy,
                                   .import_render = import_render,
                                   .import_visible_only = import_visible_only,
                                   .use_instancing = use_instancing,
                                   .import_usd_preview = import_usd_preview,
                                   .set_material_blend = set_material_blend,
                                   .light_intensity_scale = light_intensity_scale,
                                   .mtl_name_collision_mode = mtl_name_collision_mode,
                                   .import_textures_mode = import_textures_mode,
                                   .tex_name_collision_mode = tex_name_collision_mode,
                                   .import_all_materials = import_all_materials};

  STRNCPY(params.import_textures_dir, import_textures_dir);

  const bool ok = USD_import(C, filepath, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void wm_usd_import_cancel(bContext *UNUSED(C), wmOperator *op)
{
  free_operator_customdata(op);
}

static void wm_usd_import_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  struct PointerRNA *ptr = op->ptr;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *box = uiLayoutBox(layout);
  uiLayout *col = uiLayoutColumnWithHeading(box, true, IFACE_("Data Types"));
  uiItemR(col, ptr, "import_cameras", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_curves", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_lights", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_materials", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_meshes", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_volumes", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_shapes", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "prim_path_mask", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "scale", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Mesh Data"));
  uiItemR(col, ptr, "read_mesh_uvs", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "read_mesh_colors", 0, NULL, ICON_NONE);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Include"));
  uiItemR(col, ptr, "import_subdiv", 0, IFACE_("Subdivision"), ICON_NONE);
  uiItemR(col, ptr, "import_instance_proxies", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_visible_only", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_guide", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_proxy", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_render", 0, NULL, ICON_NONE);

  col = uiLayoutColumnWithHeading(box, true, IFACE_("Options"));
  uiItemR(col, ptr, "set_frame_range", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "relative_path", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "create_collection", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "light_intensity_scale", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Materials"));
  uiItemR(col, ptr, "import_all_materials", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_usd_preview", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(col, RNA_boolean_get(ptr, "import_materials"));
  uiLayout *row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "set_material_blend", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(row, RNA_boolean_get(ptr, "import_usd_preview"));
  uiItemR(col, ptr, "mtl_name_collision_mode", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  col = uiLayoutColumn(box, true);
  uiItemR(col, ptr, "import_textures_mode", 0, NULL, ICON_NONE);
  bool copy_textures = RNA_enum_get(op->ptr, "import_textures_mode") == USD_TEX_IMPORT_COPY;
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "import_textures_dir", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(row, copy_textures);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "tex_name_collision_mode", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(row, copy_textures);
  uiLayoutSetEnabled(col, RNA_boolean_get(ptr, "import_materials"));
}

void WM_OT_usd_import(struct wmOperatorType *ot)
{
  ot->name = "Import USD";
  ot->description = "Import USD stage into current scene";
  ot->idname = "WM_OT_usd_import";

  ot->invoke = wm_usd_import_invoke;
  ot->exec = wm_usd_import_exec;
  ot->cancel = wm_usd_import_cancel;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_usd_import_draw;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_PRESET;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_USD,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.usd", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_float(
      ot->srna,
      "scale",
      1.0f,
      0.0001f,
      1000.0f,
      "Scale",
      "Value by which to enlarge or shrink the objects with respect to the world's origin",
      0.0001f,
      1000.0f);

  RNA_def_boolean(ot->srna,
                  "set_frame_range",
                  true,
                  "Set Frame Range",
                  "Update the scene's start and end frame to match those of the USD archive");

  RNA_def_boolean(ot->srna, "import_cameras", true, "Cameras", "");
  RNA_def_boolean(ot->srna, "import_curves", true, "Curves", "");
  RNA_def_boolean(ot->srna, "import_lights", true, "Lights", "");
  RNA_def_boolean(ot->srna, "import_materials", true, "Materials", "");
  RNA_def_boolean(ot->srna, "import_meshes", true, "Meshes", "");
  RNA_def_boolean(ot->srna, "import_volumes", true, "Volumes", "");
  RNA_def_boolean(ot->srna, "import_shapes", true, "Shapes", "");

  RNA_def_boolean(ot->srna,
                  "import_subdiv",
                  false,
                  "Import Subdivision Scheme",
                  "Create subdivision surface modifiers based on the USD "
                  "SubdivisionScheme attribute");

  RNA_def_boolean(ot->srna,
                  "import_instance_proxies",
                  true,
                  "Import Instance Proxies",
                  "Create unique Blender objects for USD instances");

  RNA_def_boolean(ot->srna,
                  "import_visible_only",
                  true,
                  "Visible Primitives Only",
                  "Do not import invisible USD primitives. "
                  "Only applies to primitives with a non-animated visibility attribute. "
                  "Primitives with animated visibility will always be imported");

  RNA_def_boolean(ot->srna,
                  "create_collection",
                  false,
                  "Create Collection",
                  "Add all imported objects to a new collection");

  RNA_def_boolean(ot->srna, "read_mesh_uvs", true, "UV Coordinates", "Read mesh UV coordinates");

  RNA_def_boolean(
      ot->srna, "read_mesh_colors", true, "Color Attributes", "Read mesh color attributes");

  RNA_def_string(ot->srna,
                 "prim_path_mask",
                 NULL,
                 0,
                 "Path Mask",
                 "Import only the primitive at the given path and its descendents.  "
                 "Multiple paths may be specified in a list delimited by commas or semicolons");

  RNA_def_boolean(ot->srna, "import_guide", false, "Guide", "Import guide geometry");

  RNA_def_boolean(ot->srna, "import_proxy", true, "Proxy", "Import proxy geometry");

  RNA_def_boolean(ot->srna, "import_render", true, "Render", "Import final render geometry");

  RNA_def_boolean(ot->srna,
                  "import_all_materials",
                  false,
                  "Import All Materials",
                  "Also import materials that are not used by any geometry.  "
                  "Note that when this option is false, materials referenced "
                  "by geometry will still be imported");

  RNA_def_boolean(ot->srna,
                  "import_usd_preview",
                  true,
                  "Import USD Preview",
                  "Convert UsdPreviewSurface shaders to Principled BSDF shader networks");

  RNA_def_boolean(ot->srna,
                  "set_material_blend",
                  true,
                  "Set Material Blend",
                  "If the Import USD Preview option is enabled, "
                  "the material blend method will automatically be set based on the "
                  "shader's opacity and opacityThreshold inputs");

  RNA_def_float(ot->srna,
                "light_intensity_scale",
                1.0f,
                0.0001f,
                10000.0f,
                "Light Intensity Scale",
                "Scale for the intensity of imported lights",
                0.0001f,
                1000.0f);

  RNA_def_enum(
      ot->srna,
      "mtl_name_collision_mode",
      rna_enum_usd_mtl_name_collision_mode_items,
      USD_MTL_NAME_COLLISION_MAKE_UNIQUE,
      "Material Name Collision",
      "Behavior when the name of an imported material conflicts with an existing material");

  RNA_def_enum(ot->srna,
               "import_textures_mode",
               rna_enum_usd_tex_import_mode_items,
               USD_TEX_IMPORT_PACK,
               "Import Textures",
               "Behavior when importing textures from a USDZ archive");

  RNA_def_string(ot->srna,
                 "import_textures_dir",
                 "//textures/",
                 FILE_MAXDIR,
                 "Textures Directory",
                 "Path to the directory where imported textures will be copied");

  RNA_def_enum(
      ot->srna,
      "tex_name_collision_mode",
      rna_enum_usd_tex_name_collision_mode_items,
      USD_TEX_NAME_COLLISION_USE_EXISTING,
      "File Name Collision",
      "Behavior when the name of an imported texture file conflicts with an existing file");
}

#endif /* WITH_USD */
