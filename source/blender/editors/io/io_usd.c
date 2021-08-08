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
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

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

#  include "stdio.h"

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

const EnumPropertyItem rna_enum_usd_import_shaders_mode_items[] = {
    {USD_IMPORT_SHADERS_NONE, "NONE", 0, "None", "Don't import USD shaders"},
    {USD_IMPORT_USD_PREVIEW_SURFACE,
     "USD_PREVIEW_SURFACE",
     0,
     "USD Preview Surface",
     "Convert USD Preview Surface shaders to Blender Principled BSDF"},
    {USD_IMPORT_MDL,
     "USD MDL",
     0,
     "MDL",
     "Convert USD Preview Surface shaders to Blender Principled BSDF"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_usd_import_shaders_mode_items_no_umm[] = {
    {USD_IMPORT_SHADERS_NONE, "NONE", 0, "None", "Don't import USD shaders"},
    {USD_IMPORT_USD_PREVIEW_SURFACE,
     "USD_PREVIEW_SURFACE",
     0,
     "USD Preview Surface",
     "Convert USD Preview Surface shaders to Blender Principled BSDF"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_usd_xform_op_mode_items[] = {
    {USD_XFORM_OP_SRT, "SRT", 0, "Scale, Rotate, Translate", "Export scale, rotate, and translate Xform operators"},
    {USD_XFORM_OP_SOT, "SOT", 0, "Scale, Orient, Translate", "Export scale, orient, and translate Xform operators"},
    {USD_XFORM_OP_MAT, "MAT", 0, "Matrix", "Export matrix operator"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem prop_usd_export_global_forward[] = {
    {USD_GLOBAL_FORWARD_X, "X", 0, "X Forward", "Global Forward is positive X Axis"},
    {USD_GLOBAL_FORWARD_Y, "Y", 0, "Y Forward", "Global Forward is positive Y Axis"},
    {USD_GLOBAL_FORWARD_Z, "Z", 0, "Z Forward", "Global Forward is positive Z Axis"},
    {USD_GLOBAL_FORWARD_MINUS_X, "-X", 0, "-X Forward", "Global Forward is negative X Axis"},
    {USD_GLOBAL_FORWARD_MINUS_Y, "-Y", 0, "-Y Forward", "Global Forward is negative Y Axis"},
    {USD_GLOBAL_FORWARD_MINUS_Z, "-Z", 0, "-Z Forward", "Global Forward is negative Z Axis"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem prop_usd_export_global_up[] = {
    {USD_GLOBAL_UP_X, "X", 0, "X Up", "Global UP is positive X Axis"},
    {USD_GLOBAL_UP_Y, "Y", 0, "Y Up", "Global UP is positive Y Axis"},
    {USD_GLOBAL_UP_Z, "Z", 0, "Z Up", "Global UP is positive Z Axis"},
    {USD_GLOBAL_UP_MINUS_X, "-X", 0, "-X Up", "Global UP is negative X Axis"},
    {USD_GLOBAL_UP_MINUS_Y, "-Y", 0, "-Y Up", "Global UP is negative Y Axis"},
    {USD_GLOBAL_UP_MINUS_Z, "-Z", 0, "-Z Up", "Global UP is negative Z Axis"},
    {0, NULL, 0, NULL, NULL},
};

/* Stored in the wmOperator's customdata field to indicate it should run as a background job.
 * This is set when the operator is invoked, and not set when it is only executed. */
enum { AS_BACKGROUND_JOB = 1 };
typedef struct eUSDOperatorOptions {
  bool as_background_job;
} eUSDOperatorOptions;

/* ====== USD Export ====== */

static int wm_usd_export_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  eUSDOperatorOptions *options = MEM_callocN(sizeof(eUSDOperatorOptions), "eUSDOperatorOptions");
  options->as_background_job = true;
  op->customdata = options;

  RNA_boolean_set(op->ptr, "init_scene_frame_range", true);

  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    Main *bmain = CTX_data_main(C);
    char filepath[FILE_MAX];
    const char *main_blendfile_path = BKE_main_blendfile_path(bmain);

    if (main_blendfile_path[0] == '\0') {
      BLI_strncpy(filepath, "untitled", sizeof(filepath));
    }
    else {
      BLI_strncpy(filepath, main_blendfile_path, sizeof(filepath));
    }

    BLI_path_extension_replace(filepath, sizeof(filepath), ".usd");
    RNA_string_set(op->ptr, "filepath", filepath);
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static char *usd_ensure_prim_path(char *primpath)
{
  if (primpath != NULL && primpath[0] != '/' && primpath[0] != '\0') {
    char *legal_path = BLI_strdupcat("/", primpath);
    MEM_freeN(primpath);
    primpath = legal_path;
    return legal_path;
  }
  return primpath;
}

static int wm_usd_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  char filename[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filename);

  eUSDOperatorOptions *options = (eUSDOperatorOptions *)op->customdata;
  const bool as_background_job = (options != NULL && options->as_background_job);
  MEM_SAFE_FREE(op->customdata);

  const bool selected_objects_only = RNA_boolean_get(op->ptr, "selected_objects_only");
  const bool visible_objects_only = RNA_boolean_get(op->ptr, "visible_objects_only");
  const bool export_animation = RNA_boolean_get(op->ptr, "export_animation");
  const bool export_hair = RNA_boolean_get(op->ptr, "export_hair");
  const bool export_vertices = RNA_boolean_get(op->ptr, "export_vertices");
  const bool export_vertex_colors = RNA_boolean_get(op->ptr, "export_vertex_colors");
  const bool export_vertex_groups = RNA_boolean_get(op->ptr, "export_vertex_groups");
  const bool export_face_maps = RNA_boolean_get(op->ptr, "export_face_maps");
  const bool export_uvmaps = RNA_boolean_get(op->ptr, "export_uvmaps");
  const bool export_normals = RNA_boolean_get(op->ptr, "export_normals");
  const bool export_transforms = RNA_boolean_get(op->ptr, "export_transforms");
  const bool export_materials = RNA_boolean_get(op->ptr, "export_materials");
  const bool export_meshes = RNA_boolean_get(op->ptr, "export_meshes");
  const bool export_lights = RNA_boolean_get(op->ptr, "export_lights");
  const bool export_cameras = RNA_boolean_get(op->ptr, "export_cameras");
  const bool export_curves = RNA_boolean_get(op->ptr, "export_curves");
  const bool export_particles = RNA_boolean_get(op->ptr, "export_particles");
  const bool use_instancing = RNA_boolean_get(op->ptr, "use_instancing");
  const bool evaluation_mode = RNA_enum_get(op->ptr, "evaluation_mode");
  const bool generate_preview_surface = RNA_boolean_get(op->ptr, "generate_preview_surface");
  const bool convert_uv_to_st = RNA_boolean_get(op->ptr, "convert_uv_to_st");
  const bool convert_orientation = RNA_boolean_get(op->ptr, "convert_orientation");
  const bool export_child_particles = RNA_boolean_get(op->ptr, "export_child_particles");
  const bool export_as_overs = RNA_boolean_get(op->ptr, "export_as_overs");
  const bool merge_transform_and_shape = RNA_boolean_get(op->ptr, "merge_transform_and_shape");
  const bool export_custom_properties = RNA_boolean_get(op->ptr, "export_custom_properties");
  const bool export_identity_transforms = RNA_boolean_get(op->ptr, "export_identity_transforms");
  const bool apply_subdiv = RNA_boolean_get(op->ptr, "apply_subdiv");
  const bool author_blender_name = RNA_boolean_get(op->ptr, "author_blender_name");
  const bool vertex_data_as_face_varying = RNA_boolean_get(op->ptr, "vertex_data_as_face_varying");
  const float frame_step = RNA_float_get(op->ptr, "frame_step");

  const bool override_shutter = RNA_boolean_get(op->ptr, "override_shutter");
  const double shutter_open = (double)RNA_float_get(op->ptr, "shutter_open");
  const double shutter_close = (double)RNA_float_get(op->ptr, "shutter_close");

  // This default prim path is not sanitized. This happens in usd_capi.cc
  char *default_prim_path = RNA_string_get_alloc(op->ptr, "default_prim_path", NULL, 0);

  default_prim_path = usd_ensure_prim_path(default_prim_path);

  char *root_prim_path = RNA_string_get_alloc(op->ptr, "root_prim_path", NULL, 0);

  // Do not allow / path
  if (root_prim_path[0] == '/' && strlen(root_prim_path) == 1)
    root_prim_path[0] = '\0';

  root_prim_path = usd_ensure_prim_path(root_prim_path);

  char *material_prim_path = RNA_string_get_alloc(op->ptr, "material_prim_path", NULL, 0);

  int global_forward = RNA_enum_get(op->ptr, "export_global_forward_selection");
  int global_up = RNA_enum_get(op->ptr, "export_global_up_selection");

  bool export_textures = RNA_boolean_get(op->ptr, "export_textures");

  bool relative_texture_paths = RNA_boolean_get(op->ptr, "relative_texture_paths");

  const bool backward_compatible = true;

  const float light_intensity_scale = RNA_float_get(op->ptr, "light_intensity_scale");

  const bool generate_mdl = USD_umm_module_loaded() ? RNA_boolean_get(op->ptr, "generate_mdl") :
                                                      false;

  const bool convert_to_cm = RNA_boolean_get(op->ptr, "convert_to_cm");

  const bool convert_light_to_nits = RNA_boolean_get(op->ptr, "convert_light_to_nits");

  const bool scale_light_radius = RNA_boolean_get(op->ptr, "scale_light_radius");

  const bool convert_world_material = RNA_boolean_get(op->ptr, "convert_world_material");

  const bool generate_cycles_shaders = RNA_boolean_get(op->ptr, "generate_cycles_shaders");

  const bool export_armatures = RNA_boolean_get(op->ptr, "export_armatures");

  const eUSDXformOpMode xform_op_mode = RNA_enum_get(op->ptr, "xform_op_mode");

  struct USDExportParams params = {RNA_int_get(op->ptr, "start"),
                                   RNA_int_get(op->ptr, "end"),
                                   export_animation,
                                   export_hair,
                                   export_vertices,
                                   export_vertex_colors,
                                   export_vertex_groups,
                                   export_face_maps,
                                   export_uvmaps,
                                   export_normals,
                                   export_transforms,
                                   export_materials,
                                   export_meshes,
                                   export_lights,
                                   export_cameras,
                                   export_curves,
                                   export_particles,
                                   selected_objects_only,
                                   visible_objects_only,
                                   use_instancing,
                                   evaluation_mode,
                                   default_prim_path,
                                   root_prim_path,
                                   material_prim_path,
                                   generate_preview_surface,
                                   convert_uv_to_st,
                                   convert_orientation,
                                   global_forward,
                                   global_up,
                                   export_child_particles,
                                   export_as_overs,
                                   merge_transform_and_shape,
                                   export_custom_properties,
                                   export_identity_transforms,
                                   apply_subdiv,
                                   author_blender_name,
                                   vertex_data_as_face_varying,
                                   frame_step,
                                   override_shutter,
                                   shutter_open,
                                   shutter_close,
                                   export_textures,
                                   relative_texture_paths,
                                   backward_compatible,
                                   light_intensity_scale,
                                   generate_mdl,
                                   convert_to_cm,
                                   convert_light_to_nits,
                                   scale_light_radius,
                                   convert_world_material,
                                   generate_cycles_shaders,
                                   export_armatures,
                                   xform_op_mode};

  /* Take some defaults from the scene, if not specified explicitly. */
  Scene *scene = CTX_data_scene(C);
  if (params.frame_start == INT_MIN) {
    params.frame_start = SFRA;
  }
  if (params.frame_end == INT_MIN) {
    params.frame_end = EFRA;
  }

  bool ok = USD_export(C, filename, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void wm_usd_export_draw(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *box;
  struct PointerRNA *ptr = op->ptr;

  /* Conveniently set start and end frame to match the scene's frame range. */
  Scene *scene = CTX_data_scene(C);

  if (scene != NULL && RNA_boolean_get(ptr, "init_scene_frame_range")) {
    RNA_int_set(ptr, "start", SFRA);
    RNA_int_set(ptr, "end", EFRA);

    RNA_boolean_set(ptr, "init_scene_frame_range", false);
  }

  uiLayoutSetPropSep(layout, true);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("USD Export"), ICON_NONE);
  uiItemR(box, ptr, "evaluation_mode", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "apply_subdiv", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "author_blender_name", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "selected_objects_only", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "visible_objects_only", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_animation", 0, NULL, ICON_NONE);
  if (RNA_boolean_get(ptr, "export_animation")) {
    uiItemR(box, ptr, "start", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "end", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "frame_step", 0, NULL, ICON_NONE);
  }
  uiItemR(box, ptr, "export_as_overs", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "merge_transform_and_shape", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_custom_properties", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_identity_transforms", 0, NULL, ICON_NONE);

  if (RNA_boolean_get(ptr, "export_hair") || RNA_boolean_get(ptr, "export_particles")) {
    uiItemR(box, ptr, "export_child_particles", 0, NULL, ICON_NONE);
  }

  if (RNA_boolean_get(ptr, "export_vertex_colors") ||
      RNA_boolean_get(ptr, "export_vertex_groups")) {
    uiItemR(box, ptr, "vertex_data_as_face_varying", 0, NULL, ICON_NONE);
  }

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Cycles Settings:"), ICON_NONE);
  uiItemR(box, ptr, "override_shutter", 0, NULL, ICON_NONE);

  if (RNA_boolean_get(ptr, "override_shutter")) {
    uiItemR(box, ptr, "shutter_open", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "shutter_close", 0, NULL, ICON_NONE);
  }

  if (RNA_boolean_get(ptr, "export_meshes")) {
    box = uiLayoutBox(layout);
    uiItemL(box, IFACE_("Mesh Options:"), ICON_MESH_DATA);
    uiItemR(box, ptr, "export_vertices", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "export_vertex_colors", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "export_vertex_groups", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "export_face_maps", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "export_uvmaps", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "export_normals", 0, NULL, ICON_NONE);
  }

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Primitive Types:"), ICON_OBJECT_DATA);
  uiItemR(box, ptr, "export_transforms", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_meshes", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_materials", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_lights", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_cameras", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_curves", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_hair", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_particles", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "export_armatures", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Stage Options:"), ICON_SCENE_DATA);
  uiItemR(box, ptr, "default_prim_path", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "root_prim_path", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "material_prim_path", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Conversion:"), ICON_ORIENTATION_GLOBAL);
  uiItemR(box, ptr, "convert_orientation", 0, NULL, ICON_NONE);

  if (RNA_boolean_get(ptr, "convert_orientation")) {
    uiItemR(box, ptr, "export_global_forward_selection", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "export_global_up_selection", 0, NULL, ICON_NONE);
  }

  uiItemR(box, ptr, "convert_to_cm", 0, NULL, ICON_NONE);

  if (RNA_boolean_get(ptr, "export_materials")) {
    uiItemR(box, ptr, "generate_cycles_shaders", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "generate_preview_surface", 0, NULL, ICON_NONE);
    if (USD_umm_module_loaded()) {
      uiItemR(box, ptr, "generate_mdl", 0, NULL, ICON_NONE);
    }
  }

  if (RNA_boolean_get(ptr, "export_lights")) {
    uiItemR(box, ptr, "light_intensity_scale", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "convert_light_to_nits", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "scale_light_radius", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "convert_world_material", 0, NULL, ICON_NONE);
  }

  if (RNA_boolean_get(ptr, "export_uvmaps")) {
    uiItemR(box, ptr, "convert_uv_to_st", 0, NULL, ICON_NONE);
  }

  uiItemR(box, ptr, "xform_op_mode", 0, NULL, ICON_NONE);

  if (RNA_boolean_get(ptr, "export_materials")) {
    box = uiLayoutBox(layout);
    uiItemL(box, IFACE_("Textures:"), ICON_NONE);
    uiItemR(box, ptr, "export_textures", 0, NULL, ICON_NONE);
    uiItemR(box, ptr, "relative_texture_paths", 0, NULL, ICON_NONE);
  }

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Experimental:"), ICON_NONE);
  uiItemR(box, ptr, "use_instancing", 0, NULL, ICON_NONE);
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

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_USD,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  RNA_def_int(ot->srna,
              "start",
              INT_MIN,
              INT_MIN,
              INT_MAX,
              "Start Frame",
              "Start frame of the export, use the default value to "
              "take the start frame of the current scene",
              INT_MIN,
              INT_MAX);
  RNA_def_int(ot->srna,
              "end",
              INT_MIN,
              INT_MIN,
              INT_MAX,
              "End Frame",
              "End frame of the export, use the default value to "
              "take the end frame of the current scene",
              INT_MIN,
              INT_MAX);

  RNA_def_boolean(ot->srna,
                  "selected_objects_only",
                  false,
                  "Selection Only",
                  "Only selected objects are exported. Unselected parents of selected objects are "
                  "exported as empty transform");

  RNA_def_boolean(ot->srna,
                  "visible_objects_only",
                  true,
                  "Visible Only",
                  "Only visible objects are exported. Invisible parents of exported objects are "
                  "exported as empty transform");

  RNA_def_boolean(ot->srna,
                  "export_animation",
                  false,
                  "Animation",
                  "When checked, the render frame range is exported. When false, only the current "
                  "frame is exported");
  RNA_def_boolean(
      ot->srna, "export_hair", true, "Hair", "When checked, hair is exported as USD curves");
  RNA_def_boolean(ot->srna,
                  "export_vertices",
                  true,
                  "Vertices",
                  "When checked, vertex and point data are included in the export");
  RNA_def_boolean(ot->srna,
                  "export_vertex_colors",
                  true,
                  "Vertex Colors",
                  "When checked, all vertex colors are included in the export");
  RNA_def_boolean(ot->srna,
                  "export_vertex_groups",
                  false,
                  "Vertex Groups",
                  "When checked, all vertex groups are included in the export");
  RNA_def_boolean(ot->srna,
                  "export_face_maps",
                  false,
                  "Face Maps",
                  "When checked, all face maps are included in the export");
  RNA_def_boolean(ot->srna,
                  "export_uvmaps",
                  true,
                  "UV Maps",
                  "When checked, all UV maps of exported meshes are included in the export");
  RNA_def_boolean(ot->srna,
                  "export_normals",
                  true,
                  "Normals",
                  "When checked, normals of exported meshes are included in the export");
  RNA_def_boolean(
      ot->srna,
      "export_transforms",
      true,
      "Transforms",
      "When checked, transform data/operations will be exported for all applicable prims");
  RNA_def_boolean(ot->srna,
                  "export_materials",
                  true,
                  "Materials",
                  "When checked, the viewport settings of materials are exported as USD preview "
                  "materials, and material assignments are exported as geometry subsets");
  RNA_def_boolean(
      ot->srna, "export_meshes", true, "Meshes", "When checked, all meshes will be exported");
  RNA_def_boolean(
      ot->srna, "export_lights", true, "Lights", "When checked, all lights will be exported");
  RNA_def_boolean(
      ot->srna, "export_cameras", true, "Cameras", "When checked, all cameras will be exported");
  RNA_def_boolean(
      ot->srna, "export_curves", true, "Curves", "When checked, all curves will be exported");
  RNA_def_boolean(ot->srna,
                  "export_particles",
                  true,
                  "Particles",
                  "When checked, all particle systems will be exported");

  RNA_def_boolean(ot->srna,
                  "export_armatures",
                  false,
                  "Armatures (Experimental)",
                  "Export armatures and skinned meshes");

  RNA_def_boolean(ot->srna,
                  "use_instancing",
                  false,
                  "Instancing",
                  "When checked, instanced objects are exported as references in USD. "
                  "When unchecked, instanced objects are exported as real objects");

  RNA_def_enum(ot->srna,
               "evaluation_mode",
               rna_enum_usd_export_evaluation_mode_items,
               DAG_EVAL_VIEWPORT,
               "Use Settings for",
               "Determines visibility of objects, modifier settings, and other areas where there "
               "are different settings for viewport and rendering");

  RNA_def_string(ot->srna,
                 "default_prim_path",
                 NULL,
                 1024,
                 "Default Prim Path",
                 "If set, this will set the default prim path in the usd document");
  RNA_def_string(ot->srna,
                 "root_prim_path",
                 NULL,
                 1024,
                 "Root Prim Path",
                 "If set, all primitives will live under this path");
  RNA_def_string(ot->srna,
                 "material_prim_path",
                 "/materials",
                 1024,
                 "Material Prim Path",
                 "This specifies where all generated USD Shade Materials and Shaders get placed");

  RNA_def_boolean(ot->srna,
                  "generate_cycles_shaders",
                  false,
                  "Export Cycles Shaders",
                  "Export Cycles shader nodes to USD");
  RNA_def_boolean(
      ot->srna,
      "generate_preview_surface",
      true,
      "Convert to USD Preview Surface",
      "When checked, the USD exporter will generate an approximate USD Preview Surface. "
      "(Experimental, only works on simple material graphs)");
  RNA_def_boolean(ot->srna,
                  "generate_mdl",
                  true,
                  "Convert to MDL",
                  "When checked, the USD exporter will generate an MDL material");
  RNA_def_boolean(
      ot->srna,
      "convert_uv_to_st",
      false,
      "Convert uv to st",
      "When checked, the USD exporter will convert all uv map names to interchangeable 'st'"
      "(Assumes one uv layout per mesh)");

  RNA_def_boolean(ot->srna,
                  "convert_orientation",
                  false,
                  "Convert Orientation",
                  "When checked, the USD exporter will convert orientation axis");

  RNA_def_boolean(ot->srna,
                  "convert_to_cm",
                  true,
                  "Convert to Centimeters",
                  "Set the USD units to centimeters and scale the scene to convert from meters");

  RNA_def_enum(ot->srna,
               "export_global_forward_selection",
               prop_usd_export_global_forward,
               USD_DEFAULT_FORWARD,
               "Forward Axis",
               "Global Forward axis for export");

  RNA_def_enum(ot->srna,
               "export_global_up_selection",
               prop_usd_export_global_up,
               USD_DEFAULT_UP,
               "Up Axis",
               "Global Up axis for export");

  RNA_def_boolean(ot->srna,
                  "export_child_particles",
                  false,
                  "Export Child Particles",
                  "When checked, the USD exporter will export child particles");

  RNA_def_boolean(ot->srna,
                  "export_as_overs",
                  false,
                  "Export As Overs",
                  "When checked, the USD exporter will create all prims as overrides");

  RNA_def_boolean(ot->srna,
                  "merge_transform_and_shape",
                  false,
                  "Merge Transform and Shape",
                  "When checked, transforms and shapes will be merged into the one prim path");
  RNA_def_boolean(ot->srna,
                  "export_custom_properties",
                  true,
                  "Export Custom Properties",
                  "When checked, custom properties will be exported as USD User Properties");
  RNA_def_boolean(ot->srna,
                  "export_identity_transforms",
                  false,
                  "Export Identity Transforms",
                  "If enabled, transforms (xforms) will always author a transform operation, "
                  "even if transform is identity/unit/zeroed.");

  RNA_def_boolean(ot->srna,
                  "apply_subdiv",
                  true,
                  "Apply Subdiv",
                  "When checked, subdivision modifiers will be used mesh evaluation.");

  RNA_def_boolean(ot->srna,
                  "author_blender_name",
                  true,
                  "Author Blender Name",
                  "When checked, custom userProperties will be authored to allow a round trip.");

  RNA_def_boolean(ot->srna,
                  "vertex_data_as_face_varying",
                  false,
                  "Vertex Groups As faceVarying",
                  "When enabled, vertex groups will be exported as faceVarying primvars. "
                  "This takes up more disk space, and is somewhat redundant with Blender's "
                  "current authoring tools.");

  RNA_def_float(
      ot->srna,
      "frame_step",
      1.0f,
      0.00001f,
      10000.0f,
      "Frame Step",
      "The length of one frame step, less than 1 will export subframes, greater will skip frames.",
      0.00001f,
      10000.0f);

  RNA_def_boolean(ot->srna,
                  "override_shutter",
                  false,
                  "Override Shutter",
                  "Allows the ability to override the explicit shutter open and close attributes."
                  "When disabled, the shutter is used from cycles render settings");

  RNA_def_float(
      ot->srna,
      "shutter_open",
      -0.25f,
      -FLT_MAX,
      FLT_MAX,
      "Shutter Open",
      "Allows the ability to set the frame relative shutter open time in UsdTimeCode units",
      -FLT_MAX,
      FLT_MAX);

  RNA_def_float(
      ot->srna,
      "shutter_close",
      0.25f,
      -FLT_MAX,
      FLT_MAX,
      "Shutter Close",
      "Allows the ability to set the frame relative shutter close time in UsdTimeCode units",
      -FLT_MAX,
      FLT_MAX);

  /* This dummy prop is used to check whether we need to init the start and
   * end frame values to that of the scene's, otherwise they are reset at
   * every change, draw update. */
  RNA_def_boolean(ot->srna, "init_scene_frame_range", false, "", "");

  RNA_def_boolean(ot->srna,
                  "export_textures",
                  true,
                  "Export Textures",
                  "When checked and if exporting materials, textures referenced by material nodes "
                  "will be exported to a 'textures' directory in the same directory as the USD.");

  RNA_def_boolean(
      ot->srna,
      "relative_texture_paths",
      true,
      "Relative Texture Paths",
      "When checked, material texture asset paths will be saved as relative paths in the USD.");

  RNA_def_float(ot->srna,
                "light_intensity_scale",
                1.0f,
                0.0001f,
                10000.0f,
                "Light Intensity Scale",
                "Value by which to scale the intensity of exported lights",
                0.0001f,
                1000.0f);

  RNA_def_boolean(ot->srna,
                  "convert_light_to_nits",
                  true,
                  "Convert Light Units to Nits",
                  "Convert light energy units to nits");

  RNA_def_boolean(ot->srna,
                  "scale_light_radius",
                  true,
                  "Scale Light Radius",
                  "Apply the scene scale factor (from unit conversion or manual scaling) "
                  "to the radius size of spot and sphere lights");

  RNA_def_boolean(
      ot->srna,
      "convert_world_material",
      true,
      "Convert World Material",
      "Convert the world material to a USD dome light. "
      "Currently works for simple materials, consisting of an environment texture "
      "connected to a background shader, with an optional vector multiply of the texture color.");

  RNA_def_enum(ot->srna,
    "xform_op_mode",
    rna_enum_usd_xform_op_mode_items,
    USD_XFORM_OP_SRT,
    "Xform Ops",
    "The type of transform operators to write");
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
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  char filename[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filename);

  eUSDOperatorOptions *options = (eUSDOperatorOptions *)op->customdata;
  const bool as_background_job = (options != NULL && options->as_background_job);
  MEM_SAFE_FREE(op->customdata);

  const float scale = RNA_float_get(op->ptr, "scale");
  const bool apply_unit_conversion_scale = RNA_boolean_get(op->ptr, "apply_unit_conversion_scale");

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

  const bool import_subdiv = RNA_boolean_get(op->ptr, "import_subdiv");

  const bool import_instance_proxies = RNA_boolean_get(op->ptr, "import_instance_proxies");

  const bool import_visible_only = RNA_boolean_get(op->ptr, "import_visible_only");

  const bool create_collection = RNA_boolean_get(op->ptr, "create_collection");

  char *prim_path_mask = malloc(1024);
  RNA_string_get(op->ptr, "prim_path_mask", prim_path_mask);

  const bool import_guide = RNA_boolean_get(op->ptr, "import_guide");
  const bool import_proxy = RNA_boolean_get(op->ptr, "import_proxy");
  const bool import_render = RNA_boolean_get(op->ptr, "import_render");

  const bool use_instancing = RNA_boolean_get(op->ptr, "use_instancing");

  const char *import_shaders_mode_prop_name = USD_umm_module_loaded() ?
                                                  "import_shaders_mode" :
                                                  "import_shaders_mode_no_umm";

  const eUSDImportShadersMode import_shaders_mode = RNA_enum_get(op->ptr,
                                                                 import_shaders_mode_prop_name);
  const bool set_material_blend = RNA_boolean_get(op->ptr, "set_material_blend");

  const float light_intensity_scale = RNA_float_get(op->ptr, "light_intensity_scale");

  const bool convert_light_from_nits = RNA_boolean_get(op->ptr, "convert_light_from_nits");

  const bool scale_light_radius = RNA_boolean_get(op->ptr, "scale_light_radius");

  const bool create_background_shader = RNA_boolean_get(op->ptr, "create_background_shader");

  /* TODO(makowalski): Add support for sequences. */
  const bool is_sequence = false;
  int offset = 0;
  int sequence_len = 1;

  /* Switch out of edit mode to avoid being stuck in it (T54326). */
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    ED_object_mode_set(C, OB_MODE_EDIT);
  }

  const bool validate_meshes = false;

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
                                   .prim_path_mask = prim_path_mask,
                                   .import_subdiv = import_subdiv,
                                   .import_instance_proxies = import_instance_proxies,
                                   .create_collection = create_collection,
                                   .import_guide = import_guide,
                                   .import_proxy = import_proxy,
                                   .import_render = import_render,
                                   .import_visible_only = import_visible_only,
                                   .use_instancing = use_instancing,
                                   .import_shaders_mode = import_shaders_mode,
                                   .set_material_blend = set_material_blend,
                                   .light_intensity_scale = light_intensity_scale,
                                   .apply_unit_conversion_scale = apply_unit_conversion_scale,
                                   .convert_light_from_nits = convert_light_from_nits,
                                   .scale_light_radius = scale_light_radius,
                                   .create_background_shader = create_background_shader};

  const bool ok = USD_import(C, filename, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
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
  uiItemR(box, ptr, "prim_path_mask", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "scale", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "apply_unit_conversion_scale", 0, NULL, ICON_NONE);

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
  uiItemR(box, ptr, "convert_light_from_nits", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "scale_light_radius", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "create_background_shader", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Experimental"));
  uiItemR(col, ptr, "use_instancing", 0, NULL, ICON_NONE);
  uiLayout *row = uiLayoutRow(col, true);
  const char *import_shaders_mode_prop_name = USD_umm_module_loaded() ?
                                                  "import_shaders_mode" :
                                                  "import_shaders_mode_no_umm";
  uiItemR(row, ptr, import_shaders_mode_prop_name, 0, NULL, ICON_NONE);
  bool import_mtls = RNA_boolean_get(ptr, "import_materials");
  uiLayoutSetEnabled(row, import_mtls);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "set_material_blend", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(row, import_mtls);
}

void WM_OT_usd_import(struct wmOperatorType *ot)
{
  ot->name = "Import USD";
  ot->description = "Import USD stage into current scene";
  ot->idname = "WM_OT_usd_import";

  ot->invoke = wm_usd_import_invoke;
  ot->exec = wm_usd_import_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_usd_import_draw;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_USD,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);

  RNA_def_float(
      ot->srna,
      "scale",
      1.0f,
      0.0001f,
      1000.0f,
      "Scale",
      "Value by which to enlarge or shrink the objects with respect to the world's origin. "
      "This scaling is applied in addition to the Stage's meters-per-unit scaling value if "
      "the Apply Unit Conversion Scale option is enabled",
      0.0001f,
      1000.0f);

  RNA_def_boolean(
      ot->srna,
      "apply_unit_conversion_scale",
      true,
      "Apply Unit Conversion Scale",
      "Scale the scene objects by the USD stage's meters-per-unit value. "
      "This scaling is applied in addition to the value specified in the Scale option");

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

  RNA_def_boolean(ot->srna, "read_mesh_colors", false, "Vertex Colors", "Read mesh vertex colors");

  RNA_def_string(ot->srna,
                 "prim_path_mask",
                 NULL,
                 1024,
                 "Path Mask",
                 "Import only the subset of the USD scene rooted at the given primitive");

  RNA_def_boolean(ot->srna, "import_guide", false, "Guide", "Import guide geometry");

  RNA_def_boolean(ot->srna, "import_proxy", true, "Proxy", "Import proxy geometry");

  RNA_def_boolean(ot->srna, "import_render", true, "Render", "Import final render geometry");

  RNA_def_boolean(ot->srna,
                  "use_instancing",
                  false,
                  "Instancing",
                  "Import USD scenegraph instances as Blender collection instances. "
                  "Note that point instancers are not yet handled by this option");

  RNA_def_enum(ot->srna,
               "import_shaders_mode",
               rna_enum_usd_import_shaders_mode_items,
               USD_IMPORT_MDL,
               "Import Shaders ",
               "Determines which type of USD shaders to convert to Blender Principled BSDF shader "
               "networks");

  RNA_def_enum(ot->srna,
               "import_shaders_mode_no_umm",
               rna_enum_usd_import_shaders_mode_items_no_umm,
               USD_IMPORT_USD_PREVIEW_SURFACE,
               "Import Shaders ",
               "Determines which type of USD shaders to convert to Blender Principled BSDF shader "
               "networks");

  RNA_def_boolean(ot->srna,
                  "set_material_blend",
                  true,
                  "Set Material Blend",
                  "If the Import Shaders option is set to a valid type, "
                  "the material blend method will automatically be set based on the "
                  "shader opacity");

  RNA_def_float(ot->srna,
                "light_intensity_scale",
                1.0f,
                0.0001f,
                10000.0f,
                "Light Intensity Scale",
                "Scale for the intensity of imported lights",
                0.0001f,
                1000.0f);

  RNA_def_boolean(ot->srna,
                  "convert_light_from_nits",
                  true,
                  "Convert Light Units from Nits",
                  "Convert light intensity units from nits");

  RNA_def_boolean(ot->srna,
                  "scale_light_radius",
                  true,
                  "Scale Light Radius",
                  "Apply the scene scale factor (from unit conversion or manual scaling) "
                  "to the radius size of spot and local lights");

  RNA_def_boolean(ot->srna,
                  "create_background_shader",
                  true,
                  "Create Background Shader",
                  "Convert USD dome lights to world background shaders");
}

#endif /* WITH_USD */
