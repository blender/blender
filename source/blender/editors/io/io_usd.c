/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

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
#  include "BKE_screen.h"

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

#  include "IO_orientation.h"
#  include "io_ops.h"
#  include "io_usd.h"
#  include "usd.h"

#  include <stdio.h>


static const char *WM_OT_USD_EXPORT_IDNAME = "WM_OT_usd_export";


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
     "Convert MDL shaders to Blender materials; if no MDL shaders "
     "exist on the material, log a warning and import existing USD "
     "Preview Surface shaders instead"},
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
    {USD_XFORM_OP_SRT,
     "SRT",
     0,
     "Scale, Rotate, Translate",
     "Export scale, rotate, and translate Xform operators"},
    {USD_XFORM_OP_SOT,
     "SOT",
     0,
     "Scale, Orient, Translate",
     "Export scale, orient, and translate Xform operators"},
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

const EnumPropertyItem rna_enum_usd_attr_import_mode_items[] = {
    {USD_ATTR_IMPORT_NONE, "NONE", 0, "None", "Do not import attributes"},
    {USD_ATTR_IMPORT_USER,
     "USER",
     0,
     "User",
     "Import attributes in the 'userProperties' namespace as "
     "Blender custom properties.  The namespace will "
     "be stripped from the property names"},
    {USD_ATTR_IMPORT_ALL,
     "ALL",
     0,
     "All Custom",
     "Import all USD custom attributes as Blender custom properties. "
     "Namespaces will be retained in the property names"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem prop_usdz_downscale_size[] = {
    {USD_TEXTURE_SIZE_KEEP, "KEEP", 0, "Keep", "Keep all current texture sizes"},
    {USD_TEXTURE_SIZE_256,  "256", 0, "256", "Resize to a maximum of 256 pixels"},
    {USD_TEXTURE_SIZE_512,  "512", 0, "512", "Resize to a maximum of 512 pixels"},
    {USD_TEXTURE_SIZE_1024, "1024", 0, "1024", "Resize to a maximum of 1024 pixels"},
    {USD_TEXTURE_SIZE_2048, "2048", 0, "2048", "Resize to a maximum of 256 pixels"},
    {USD_TEXTURE_SIZE_4096, "4096", 0, "4096", "Resize to a maximum of 256 pixels"},
    {USD_TEXTURE_SIZE_CUSTOM, "CUSTOM", 0, "Custom", "Specify a custom size"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem prop_default_prim_kind_items[] = {
    { USD_KIND_NONE,      "NONE", 0, "None", "No kind is exported for default prim" },
    { USD_KIND_COMPONENT, "COMPONENT", 0, "Component", "Set Default Prim Kind to Component" },
    { USD_KIND_GROUP,     "GROUP", 0, "Group", "Set Default Prim Kind to Group" },
    { USD_KIND_ASSEMBLY,  "ASSEMBLY", 0, "Assembly", "Set Default Prim Kind to Assembly" },
    { USD_KIND_CUSTOM,    "CUSTOM", 0, "Custom", "Specify a custom Kind for the Default Prim" },
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

/* Forward declarations */
void usd_export_panel_register(const char* idname,
                               const char* label,
                               int flag,
                               bool (*poll)(const struct bContext *C, struct PanelType *pt),
                               void (*draw)(const struct bContext *C, struct Panel *panel),
                               void (*draw_header)(const struct bContext *C, struct Panel *panel)
                               );
void usd_export_panel_register_general(void);
void usd_export_panel_register_geometry(void);
void usd_export_panel_register_materials(void);
void usd_export_panel_register_lights(void);
void usd_export_panel_register_stage(void);
void usd_export_panel_register_animation(void);
void usd_export_panel_register_types(void);
void usd_export_panel_register_particles(void);
void usd_export_panel_register_rigging(void);


/* ====== USD Export ====== */

static int wm_usd_export_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  eUSDOperatorOptions *options = MEM_callocN(sizeof(eUSDOperatorOptions), "eUSDOperatorOptions");
  options->as_background_job = true;
  op->customdata = options;

  RNA_boolean_set(op->ptr, "init_scene_frame_range", true);

  ED_fileselect_ensure_default_filepath(C, op, ".usd");

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
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
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
  const bool add_properties_namespace = RNA_boolean_get(op->ptr, "add_properties_namespace");
  const bool export_identity_transforms = RNA_boolean_get(op->ptr, "export_identity_transforms");
  const bool apply_subdiv = RNA_boolean_get(op->ptr, "apply_subdiv");
  const bool author_blender_name = RNA_boolean_get(op->ptr, "author_blender_name");
  const bool vertex_data_as_face_varying = RNA_boolean_get(op->ptr, "vertex_data_as_face_varying");
  const float frame_step = RNA_float_get(op->ptr, "frame_step");

  const bool override_shutter = RNA_boolean_get(op->ptr, "override_shutter");
  const double shutter_open = (double)RNA_float_get(op->ptr, "shutter_open");
  const double shutter_close = (double)RNA_float_get(op->ptr, "shutter_close");

  // This default prim path is not sanitized. This happens in usd_capi.cc
  char *default_prim_path = RNA_string_get_alloc(op->ptr, "default_prim_path", NULL, 0, NULL);

  default_prim_path = usd_ensure_prim_path(default_prim_path);

  char *root_prim_path = RNA_string_get_alloc(op->ptr, "root_prim_path", NULL, 0, NULL);

  // Do not allow / path
  if (root_prim_path[0] == '/' && strlen(root_prim_path) == 1)
    root_prim_path[0] = '\0';

  root_prim_path = usd_ensure_prim_path(root_prim_path);

  char *material_prim_path = RNA_string_get_alloc(op->ptr, "material_prim_path", NULL, 0, NULL);

  material_prim_path = usd_ensure_prim_path(material_prim_path);

  int global_forward = RNA_enum_get(op->ptr, "export_global_forward_selection");
  int global_up = RNA_enum_get(op->ptr, "export_global_up_selection");

  bool export_textures = RNA_boolean_get(op->ptr, "export_textures");

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

  const bool fix_skel_root = RNA_boolean_get(op->ptr, "fix_skel_root");

  const bool overwrite_textures = RNA_boolean_get(op->ptr, "overwrite_textures");

  const bool relative_paths = RNA_boolean_get(op->ptr, "relative_paths");

  const bool export_blendshapes = RNA_boolean_get(op->ptr, "export_blendshapes");

  const eUSDZTextureDownscaleSize usdz_downscale_size = RNA_enum_get(op->ptr, "usdz_downscale_size");

  const int usdz_downscale_custom_size = RNA_int_get(op->ptr, "usdz_downscale_custom_size");

  const bool usdz_is_arkit = RNA_boolean_get(op->ptr, "usdz_is_arkit");

  const bool triangulate_meshes = RNA_boolean_get(op->ptr, "triangulate_meshes");

  const int quad_method = RNA_enum_get(op->ptr, "quad_method");
  const int ngon_method = RNA_enum_get(op->ptr, "ngon_method");

  const bool export_blender_metadata = RNA_boolean_get(op->ptr, "export_blender_metadata");

  /* USDKind support. */
  const bool export_usd_kind = RNA_boolean_get(op->ptr, "export_usd_kind");
  const int default_prim_kind = RNA_enum_get(op->ptr, "default_prim_kind");
  char *default_prim_custom_kind = RNA_string_get_alloc(op->ptr, "default_prim_custom_kind", NULL, 0, NULL);

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
                                   add_properties_namespace,
                                   export_identity_transforms,
                                   apply_subdiv,
                                   author_blender_name,
                                   vertex_data_as_face_varying,
                                   frame_step,
                                   override_shutter,
                                   shutter_open,
                                   shutter_close,
                                   export_textures,
                                   relative_paths,
                                   backward_compatible,
                                   light_intensity_scale,
                                   generate_mdl,
                                   convert_to_cm,
                                   convert_light_to_nits,
                                   scale_light_radius,
                                   convert_world_material,
                                   generate_cycles_shaders,
                                   export_armatures,
                                   xform_op_mode,
                                   fix_skel_root,
                                   overwrite_textures,
                                   export_blendshapes,
                                   usdz_downscale_size,
                                   usdz_downscale_custom_size,
                                   usdz_is_arkit,
                                   export_blender_metadata,
                                   triangulate_meshes,
                                   quad_method,
                                   ngon_method,
                                   .export_usd_kind = export_usd_kind,
                                   .default_prim_kind = default_prim_kind,
                                   .default_prim_custom_kind = default_prim_custom_kind
                                   };

  /* Take some defaults from the scene, if not specified explicitly. */
  Scene *scene = CTX_data_scene(C);

  if (scene) {
    if (params.frame_start == INT_MIN) {
      params.frame_start = scene->r.sfra;
    }
    if (params.frame_end == INT_MIN) {
      params.frame_end = scene->r.efra;
    }
  }

  bool ok = USD_export(C, filename, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void wm_usd_export_draw(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  struct PointerRNA *ptr = op->ptr;

  /* Conveniently set start and end frame to match the scene's frame range. */
  Scene *scene = CTX_data_scene(C);

  if (scene != NULL && RNA_boolean_get(ptr, "init_scene_frame_range")) {
    RNA_int_set(ptr, "start", scene->r.sfra);
    RNA_int_set(ptr, "end", scene->r.efra);

    RNA_boolean_set(ptr, "init_scene_frame_range", false);
  }

  uiItemR(layout, ptr, "evaluation_mode", 0, NULL, ICON_NONE);

  /* Note: all other pertinent settings are shown through the panels below! */
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
    BLI_path_extension_ensure(filepath, FILE_MAX, ".usd");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

static void forward_axis_update(struct Main *UNUSED(main),
                                struct Scene *UNUSED(scene),
                                struct PointerRNA *ptr)
{
  int forward = RNA_enum_get(ptr, "forward_axis");
  int up = RNA_enum_get(ptr, "up_axis");
  if ((forward % 3) == (up % 3)) {
    RNA_enum_set(ptr, "up_axis", (up + 1) % 6);
  }
}

static void up_axis_update(struct Main *UNUSED(main),
                           struct Scene *UNUSED(scene),
                           struct PointerRNA *ptr)
{
  int forward = RNA_enum_get(ptr, "forward_axis");
  int up = RNA_enum_get(ptr, "up_axis");
  if ((forward % 3) == (up % 3)) {
    RNA_enum_set(ptr, "forward_axis", (forward + 1) % 6);
  }
}

void WM_OT_usd_export(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Export USD";
  ot->description = "Export current scene in a USD archive";
  ot->idname = WM_OT_USD_EXPORT_IDNAME;

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
      ot->srna, "export_hair", true, "Hair", "Export hair particle systems as USD curves");
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
                  true,
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
                  "UV Maps", "Include all mesh UV maps in the export");

  RNA_def_boolean(ot->srna,
                  "export_normals",
                  true,
                  "Normals",
                  "Include normals of exported meshes in the export");
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
                  "Export viewport settings of materials as USD preview materials, and export "
                  "material assignments as geometry subsets");
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
                  "Armatures",
                  "Export armatures and skinned meshes");

  RNA_def_boolean(ot->srna,
                  "export_blendshapes",
                  false,
                  "Blend Shapes",
                  "Export shape keys as USD blend shapes");

  RNA_def_boolean(ot->srna,
                  "use_instancing",
                  false,
                  "Instancing",
                  "Export instanced objects as references in USD rather than real objects");

  RNA_def_boolean(ot->srna,
    "fix_skel_root",
    true,
    "Fix Skel Root",
    "If exporting armatures, attempt to automatically "
    "correct invalid USD Skel Root hierarchies");

  RNA_def_enum(ot->srna,
               "evaluation_mode",
               rna_enum_usd_export_evaluation_mode_items,
               DAG_EVAL_VIEWPORT,
               "Use Settings for",
               "Determines visibility of objects, modifier settings, and other areas where there "
               "are different settings for viewport and rendering");

  RNA_def_string(ot->srna,
                 "default_prim_path",
                 "/root",
                 1024,
                 "Default Prim Path",
                 "If set, this will set the default prim path in the usd document");
  RNA_def_string(ot->srna,
                 "root_prim_path",
                 "/root",
                 1024,
                 "Root Prim Path",
                 "If set, all primitives will live under this path");
  RNA_def_string(ot->srna,
                 "material_prim_path",
                 "/Materials",
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
      true,
      "Convert uv to st",
      "Export the active uv map as USD primvar named 'st'");

  RNA_def_boolean(ot->srna,
                  "convert_orientation",
                  false,
                  "Convert Orientation",
                  "When checked, the USD exporter will convert orientation axis");

  prop = RNA_def_enum(
      ot->srna, "export_global_forward_selection", io_transform_axis, IO_AXIS_NEGATIVE_Z, "Forward Axis", "Global Forward axis for export");
  RNA_def_property_update_runtime(prop, (void *)forward_axis_update);
  prop = RNA_def_enum(ot->srna, "export_global_up_selection", io_transform_axis, IO_AXIS_Y, "Up Axis", "Global Up axis for export");
  RNA_def_property_update_runtime(prop, (void *)up_axis_update);

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
  RNA_def_boolean(
      ot->srna,
      "add_properties_namespace",
      true,
      "Add Properties Namespace",
      "Add exported custom properties to the 'userProperties' USD attribute namespace");
  RNA_def_boolean(ot->srna,
                  "export_identity_transforms",
                  true,
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
                  "If exporting materials, export textures referenced by material nodes "
                  "to a 'textures' directory in the same directory as the USD file");

  RNA_def_boolean(ot->srna,
                  "overwrite_textures",
                  false,
                  "Overwrite Textures",
                  "Allow overwriting existing texture files when exporting textures");

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

  RNA_def_boolean(ot->srna,
                  "relative_paths",
                  true,
                  "Relative Paths",
                  "Use relative paths to reference external files (i.e. textures, volumes) in "
                  "USD, otherwise use absolute paths");

  RNA_def_enum(ot->srna,
               "usdz_downscale_size",
               prop_usdz_downscale_size,
               DAG_EVAL_VIEWPORT,
               "USDZ Texture Downsampling",
               "Choose a maximum size for all exported textures");

  RNA_def_int(ot->srna,
              "usdz_downscale_custom_size",
              128,
              128, 16384,
              "USDZ Custom Downscale Size",
              "Custom size for downscaling exported textures",
              128,
              8192
              );

  RNA_def_boolean(ot->srna,
                  "usdz_is_arkit",
                  false,
                  "Create ARKit Asset",
                  "Export USDZ files as ARKit Assets");

  RNA_def_boolean(ot->srna,
                  "export_blender_metadata",
                  true,
                  "Export Blender Metadata",
                  "Write Blender-specific information to the Stage's customLayerData");

  RNA_def_boolean(ot->srna,
                  "triangulate_meshes",
                  false,
                  "Triangulate Meshes",
                  "Triangulate meshes during export");

  RNA_def_enum(ot->srna,
               "quad_method",
               rna_enum_modifier_triangulate_quad_method_items,
               MOD_TRIANGULATE_QUAD_SHORTEDGE,
               "Quad Method",
               "Method for splitting the quads into triangles");

  RNA_def_enum(ot->srna,
               "ngon_method",
               rna_enum_modifier_triangulate_ngon_method_items,
               MOD_TRIANGULATE_NGON_BEAUTY,
               "N-gon Method",
               "Method for splitting the n-gons into triangles");

  RNA_def_boolean(ot->srna,
                  "export_usd_kind",
                  true,
                  "Export USD Kind",
                  "Export Kind per-prim when specified through 'usdkind' custom property.");

  RNA_def_enum(ot->srna,
               "default_prim_kind",
               prop_default_prim_kind_items,
               USD_KIND_NONE,
               "Default Prim Kind",
               "Kind to author on the Default Prim");

  RNA_def_string(ot->srna,
                 "default_prim_custom_kind",
                 NULL,
                 128,
                 "Default Prim Custom Kind",
                 "If default_prim_kind is True, author this value as the Default Prim's Kind");
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
  const bool import_blendshapes = RNA_boolean_get(op->ptr, "import_blendshapes");
  const bool import_volumes = RNA_boolean_get(op->ptr, "import_volumes");
  const bool import_skeletons = RNA_boolean_get(op->ptr, "import_skeletons");
  const bool import_shapes = RNA_boolean_get(op->ptr, "import_shapes");

  const bool import_subdiv = RNA_boolean_get(op->ptr, "import_subdiv");

  const bool import_instance_proxies = RNA_boolean_get(op->ptr, "import_instance_proxies");

  const bool import_visible_only = RNA_boolean_get(op->ptr, "import_visible_only");

  const bool import_defined_only = RNA_boolean_get(op->ptr, "import_defined_only");

  const bool create_collection = RNA_boolean_get(op->ptr, "create_collection");

  char *prim_path_mask = RNA_string_get_alloc(op->ptr, "prim_path_mask", NULL, 0, NULL);

  const bool import_guide = RNA_boolean_get(op->ptr, "import_guide");
  const bool import_proxy = RNA_boolean_get(op->ptr, "import_proxy");
  const bool import_render = RNA_boolean_get(op->ptr, "import_render");

  const bool use_instancing = RNA_boolean_get(op->ptr, "use_instancing");

  const char *import_shaders_mode_prop_name = USD_umm_module_loaded() ?
                                                  "import_shaders_mode" :
                                                  "import_shaders_mode_no_umm";

  const eUSDImportShadersMode import_shaders_mode = RNA_enum_get(op->ptr,
                                                                 import_shaders_mode_prop_name);
  const bool import_all_materials = RNA_boolean_get(op->ptr, "import_all_materials");

  const bool set_material_blend = RNA_boolean_get(op->ptr, "set_material_blend");

  const float light_intensity_scale = RNA_float_get(op->ptr, "light_intensity_scale");

  const bool convert_light_from_nits = RNA_boolean_get(op->ptr, "convert_light_from_nits");

  const bool scale_light_radius = RNA_boolean_get(op->ptr, "scale_light_radius");

  const bool create_background_shader = RNA_boolean_get(op->ptr, "create_background_shader");

  const eUSDMtlNameCollisionMode mtl_name_collision_mode = RNA_enum_get(op->ptr,
                                                                        "mtl_name_collision_mode");

  const eUSDAttrImportMode attr_import_mode = RNA_enum_get(op->ptr, "attr_import_mode");

  const bool validate_meshes = RNA_boolean_get(op->ptr, "validate_meshes");

  /* TODO(makowalski): Add support for sequences. */
  const bool is_sequence = false;
  int offset = 0;
  int sequence_len = 1;

  /* Switch out of edit mode to avoid being stuck in it (#54326). */
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    ED_object_mode_set(C, OB_MODE_EDIT);
  }

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
                                   .import_blendshapes = import_blendshapes,
                                   .import_volumes = import_volumes,
                                   .import_skeletons = import_skeletons,
                                   .prim_path_mask = prim_path_mask,
                                   .import_shapes = import_shapes,
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
                                   .create_background_shader = create_background_shader,
                                   .attr_import_mode = attr_import_mode,
                                   .import_defined_only = import_defined_only,
                                   .mtl_name_collision_mode = mtl_name_collision_mode,
                                   .import_textures_mode = import_textures_mode,
                                   .tex_name_collision_mode = tex_name_collision_mode,
                                   .import_all_materials = import_all_materials};

  STRNCPY(params.import_textures_dir, import_textures_dir);

  const bool ok = USD_import(C, filename, &params, as_background_job);

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
  uiItemR(col, ptr, "import_blendshapes", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_volumes", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_skeletons", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_shapes", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "prim_path_mask", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "scale", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "apply_unit_conversion_scale", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Mesh Data"));
  uiItemR(col, ptr, "read_mesh_uvs", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "read_mesh_colors", 0, NULL, ICON_NONE);
  uiItemR(box, ptr, "validate_meshes", 0, NULL, ICON_NONE);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Include"));
  uiItemR(col, ptr, "import_subdiv", 0, IFACE_("Subdivision"), ICON_NONE);
  uiItemR(col, ptr, "import_instance_proxies", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_visible_only", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "import_defined_only", 0, NULL, ICON_NONE);
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
  uiItemR(box, ptr, "attr_import_mode", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Materials"));
  uiItemR(col, ptr, "import_all_materials", 0, NULL, ICON_NONE);
  uiLayoutSetEnabled(col, RNA_boolean_get(ptr, "import_materials"));

  uiLayout *row = uiLayoutRow(col, true);
  const char *import_shaders_mode_prop_name = USD_umm_module_loaded() ?
                                                  "import_shaders_mode" :
                                                  "import_shaders_mode_no_umm";
  uiItemR(row, ptr, import_shaders_mode_prop_name, 0, NULL, ICON_NONE);
  bool import_mtls = RNA_boolean_get(ptr, "import_materials");
  uiLayoutSetEnabled(row, import_mtls);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "set_material_blend", 0, NULL, ICON_NONE);
  bool import_usd_preview = RNA_enum_get(op->ptr, import_shaders_mode_prop_name) ==
                            USD_IMPORT_USD_PREVIEW_SURFACE;
  uiLayoutSetEnabled(row, import_usd_preview);
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

  box = uiLayoutBox(layout);
  col = uiLayoutColumnWithHeading(box, true, IFACE_("Experimental"));
  uiItemR(col, ptr, "use_instancing", 0, NULL, ICON_NONE);
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
  RNA_def_boolean(ot->srna, "import_blendshapes", true, "Blend Shapes", "");
  RNA_def_boolean(ot->srna, "import_volumes", true, "Volumes", "");
  RNA_def_boolean(ot->srna, "import_skeletons", true, "Skeletons", "");
  RNA_def_boolean(ot->srna, "import_shapes", true, "USD Shapes", "");

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
                  "import_defined_only",
                  true,
                  "Defined Primitives Only",
                  "Turn this off to allow importing USD primitives which are not defined, "
                  "for example, to load overrides with the Path Mask");

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
                 "Multiple paths may be specified in a list delimited by spaces, commas or somicolons");

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
                  "import_all_materials",
                  false,
                  "Import All Materials",
                  "Also import materials that are not used by any geometry.  "
                  "Note that when this option is false, materials referenced "
                  "by geometry will still be imported");

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

  RNA_def_enum(
      ot->srna,
      "mtl_name_collision_mode",
      rna_enum_usd_mtl_name_collision_mode_items,
      USD_MTL_NAME_COLLISION_MAKE_UNIQUE,
      "Material Name Collision",
      "Behavior when the name of an imported material conflicts with an existing material");

  RNA_def_enum(ot->srna,
               "attr_import_mode",
               rna_enum_usd_attr_import_mode_items,
               USD_ATTR_IMPORT_NONE,
               "Import Attributes",
               "Behavior when importing USD attributes as Blender custom properties");

  RNA_def_boolean(ot->srna,
                  "validate_meshes",
                  false,
                  "Validate Meshes",
                  "Validate meshes for degenerate geometry on import");

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


/* Panel Types */
static wmOperator *get_named_operator(const bContext *C, const char *idname) {
  SpaceFile *space = CTX_wm_space_file(C);
  if (!space) {
    return NULL;
  }

  wmOperator *op = space->op;
  if (!op) {
    return NULL;
  }

  if (strcasecmp(op->idname, idname) != 0) {
    return NULL;
  }

    return op;
}

static bool usd_export_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
  return (bool)get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
}


/* export panel - general */
static void usd_export_panel_general_draw(const bContext *C, Panel *panel)
{
  wmOperator *op   = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr  = op->ptr;

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);

  uiItemFullR(col, ptr, RNA_struct_find_property(ptr, "selected_objects_only"), 0, 0, 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "visible_objects_only", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "convert_orientation", 0, NULL, ICON_NONE);
  if (RNA_boolean_get(ptr, "convert_orientation")) {
    uiItemR(col, ptr, "export_global_forward_selection", 0, NULL, ICON_NONE);
    uiItemR(col, ptr, "export_global_up_selection", 0, NULL, ICON_NONE);
  }
  uiItemR(col, ptr, "usdz_is_arkit", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "convert_to_cm", 0, NULL, ICON_NONE);

  col = uiLayoutColumnWithHeading(panel->layout, true, "External Files");
  uiLayoutSetPropSep(col, true);
  uiItemFullR(col, ptr, RNA_struct_find_property(ptr, "relative_paths"), 0, 0, 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_as_overs", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "merge_transform_and_shape", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);
  uiItemR(col, ptr, "xform_op_mode", 0, NULL, ICON_NONE);
}


static void usd_export_panel_geometry_draw(const bContext *C, Panel *panel) {
  wmOperator *op   = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr  = op->ptr;

  uiLayout *col = uiLayoutColumn(panel->layout, false);

  uiLayoutSetPropSep(col, true);
  uiItemR(col, ptr, "apply_subdiv", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_vertex_colors", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_vertex_groups", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_normals", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_uvmaps", 0, NULL, ICON_NONE);
  if (RNA_boolean_get(ptr, "export_uvmaps")) {
    uiItemR(col, ptr, "convert_uv_to_st", 0, NULL, ICON_NONE);
  }
  uiItemR(col, ptr, "triangulate_meshes", 0, NULL, ICON_NONE);

  uiLayout *sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "triangulate_meshes"));
  uiItemR(sub, ptr, "quad_method", 0, IFACE_("Method Quads"), ICON_NONE);
  uiItemR(sub, ptr, "ngon_method", 0, IFACE_("Polygons"), ICON_NONE);
}


static void usd_export_panel_materials_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  const bool is_enabled = RNA_boolean_get(ptr, "export_materials");

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);
  uiLayoutSetEnabled(col, is_enabled);

  uiItemFullR(col,
              ptr,
              RNA_struct_find_property(ptr, "generate_preview_surface"),
              0,
              0,
              0,
              NULL,
              ICON_NONE);
  uiItemR(col, ptr, "generate_cycles_shaders", 0, NULL, ICON_NONE);
  if (USD_umm_module_loaded()) {
    uiItemR(col, ptr, "generate_mdl", 0, NULL, ICON_NONE);
  }

  col = uiLayoutColumnWithHeading(panel->layout, true, IFACE_("Textures: "));
  uiLayoutSetEnabled(col, is_enabled);
  uiLayoutSetPropSep(col, true);

  uiItemFullR(
      col, ptr, RNA_struct_find_property(ptr, "export_textures"), 0, 0, 0, NULL, ICON_NONE);
  if (RNA_boolean_get(ptr, "export_textures")) {
    uiItemR(col, ptr, "overwrite_textures", 0, NULL, ICON_NONE);
  }

  /* Without this column the spacing is off by a pixel or two */
  col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);
  uiLayoutSetEnabled(col, is_enabled);

  uiItemR(col, ptr, "usdz_downscale_size", 0, NULL, ICON_NONE);
  if (RNA_enum_get(ptr, "usdz_downscale_size") == USD_TEXTURE_SIZE_CUSTOM) {
    uiItemR(col, ptr, "usdz_downscale_custom_size", 0, NULL, ICON_NONE);
  }
}


static void usd_export_panel_lights_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);
  uiLayoutSetEnabled(col, RNA_boolean_get(ptr, "export_lights"));

  uiItemR(col, ptr, "light_intensity_scale", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "convert_light_to_nits", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "scale_light_radius", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "convert_world_material", 0, NULL, ICON_NONE);
}


static void usd_export_panel_stage_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);

  uiItemR(col, ptr, "default_prim_path", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "root_prim_path", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "material_prim_path", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "default_prim_kind", 0, NULL, ICON_NONE);
  if (RNA_enum_get(ptr, "default_prim_kind") == USD_KIND_CUSTOM) {
    uiItemR(col, ptr, "default_prim_custom_kind", 0, NULL, ICON_NONE);
  }
}


static void usd_export_panel_animation_draw_header(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  uiItemR(panel->layout, ptr, "export_animation", 0, NULL, ICON_NONE);
}


static void usd_export_panel_animation_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  const bool is_enabled = RNA_boolean_get(ptr, "export_animation");

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);
  uiLayoutSetEnabled(col, is_enabled);

  col = uiLayoutColumn(col, true);
  uiItemR(col, ptr, "start", 0, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(col, ptr, "end", 0, IFACE_("End"), ICON_NONE);
  uiItemR(col, ptr, "frame_step", 0, IFACE_("Frame Step"), ICON_NONE);

  uiLayout *sub = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(sub, true);
  uiLayoutSetEnabled(sub, is_enabled);
}


static void usd_export_panel_export_types_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);

  uiItemR(col, ptr, "export_transforms", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_meshes", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_materials", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_lights", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_cameras", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_curves", 0, NULL, ICON_NONE);
}


static void usd_export_panel_particles_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  uiLayout *col = uiLayoutColumnWithHeading(panel->layout, true, IFACE_("Particles: "));
  uiLayoutSetPropSep(col, true);

  uiItemR(col, ptr, "export_particles", 0, "Export Particles", ICON_NONE);
  uiItemR(col, ptr, "export_hair", 0, "Export Hair as Curves", ICON_NONE);
  if (RNA_boolean_get(ptr, "export_hair") || RNA_boolean_get(ptr, "export_particles")) {
    uiItemR(col, ptr, "export_child_particles", 0, NULL, ICON_NONE);
  }

  uiItemSpacer(col);

  col = uiLayoutColumnWithHeading(panel->layout, true, IFACE_("Instances: "));
  uiLayoutSetPropSep(col, true);
  uiItemR(col, ptr, "use_instancing", 0, "Export As Instances", ICON_NONE);
}


static void usd_export_panel_rigging_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = get_named_operator(C, WM_OT_USD_EXPORT_IDNAME);
  PointerRNA *ptr = op->ptr;

  uiLayout *col = uiLayoutColumnWithHeading(panel->layout, true, IFACE_("Armatures: "));
  uiLayoutSetPropSep(col, true);

  uiItemR(col, ptr, "export_armatures", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(panel->layout, false);
  uiLayoutSetPropSep(col, true);
  uiLayoutSetEnabled(col, RNA_boolean_get(ptr, "export_armatures"));
  uiItemR(col, ptr, "fix_skel_root", 0, NULL, ICON_NONE);

  col = uiLayoutColumnWithHeading(panel->layout, true, IFACE_("Shapes: "));
  uiLayoutSetPropSep(col, true);
  uiItemR(col, ptr, "export_blendshapes", 0, "Export Shape Keys", ICON_NONE);
}


void usd_export_panel_register(const char* idname,
                               const char* label,
                               int flag,
                               bool (*poll)(const struct bContext *C, struct PanelType *pt),
                               void (*draw)(const struct bContext *C, struct Panel *panel),
                               void (*draw_header)(const struct bContext *C, struct Panel *panel)
                               )
{
  PanelType *pt = MEM_callocN(sizeof(PanelType), idname);
  BLI_strncpy(pt->idname, idname, BKE_ST_MAXNAME);
  if (label) {
    BLI_strncpy(pt->label, N_(label), BKE_ST_MAXNAME);
  }
  BLI_strncpy(pt->category, "View", BKE_ST_MAXNAME);
  pt->region_type = RGN_TYPE_TOOL_PROPS;
  pt->space_type = SPACE_FILE;
  pt->flag = flag;
  BLI_strncpy(pt->parent_id, "FILE_PT_operator", BKE_ST_MAXNAME);
  BLI_strncpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA, BKE_ST_MAXNAME);
  pt->draw = draw;
  pt->draw_header = draw_header;
  pt->poll = poll;

  IO_paneltype_set_parent(pt);
  WM_paneltype_add(pt);
}


void usd_export_panel_register_general() {
  usd_export_panel_register("FILE_PT_usd_export_general",
                            "General",
                            0,
                            usd_export_panel_poll,
                            usd_export_panel_general_draw,
                            NULL
                            );
}


void usd_export_panel_register_geometry() {
  usd_export_panel_register("FILE_PT_usd_export_geometry",
                            "Geometry",
                            0,
                            usd_export_panel_poll,
                            usd_export_panel_geometry_draw,
                            NULL
                            );
}


void usd_export_panel_register_materials() {
  usd_export_panel_register("FILE_PT_usd_export_materials",
                            "Materials",
                            PANEL_TYPE_DEFAULT_CLOSED,
                            usd_export_panel_poll,
                            usd_export_panel_materials_draw,
                            NULL
                            );
}


void usd_export_panel_register_lights() {
  usd_export_panel_register("FILE_PT_usd_export_lights",
                            "Lights",
                            PANEL_TYPE_DEFAULT_CLOSED,
                            usd_export_panel_poll,
                            usd_export_panel_lights_draw,
                            NULL
                            );
}


void usd_export_panel_register_stage() {
  usd_export_panel_register("FILE_PT_usd_export_stage",
                            "Stage",
                            PANEL_TYPE_DEFAULT_CLOSED,
                            usd_export_panel_poll,
                            usd_export_panel_stage_draw,
                            NULL
                            );
}


void usd_export_panel_register_animation() {
  usd_export_panel_register("FILE_PT_usd_export_animation",
                            NULL,
                            PANEL_TYPE_DEFAULT_CLOSED,
                            usd_export_panel_poll,
                            usd_export_panel_animation_draw,
                            usd_export_panel_animation_draw_header
                            );
}


void usd_export_panel_register_types() {
  usd_export_panel_register("FILE_PT_usd_export_types",
                            "Export Types",
                            PANEL_TYPE_DEFAULT_CLOSED,
                            usd_export_panel_poll,
                            usd_export_panel_export_types_draw,
                            NULL
                            );
}


void usd_export_panel_register_particles() {
  usd_export_panel_register("FILE_PT_usd_export_particles",
                            "Particles and Instancing",
                            PANEL_TYPE_DEFAULT_CLOSED,
                            usd_export_panel_poll,
                            usd_export_panel_particles_draw,
                            NULL
                            );
}


void usd_export_panel_register_rigging() {
  usd_export_panel_register("FILE_PT_usd_export_rigging",
                            "Rigging",
                            PANEL_TYPE_DEFAULT_CLOSED,
                            usd_export_panel_poll,
                            usd_export_panel_rigging_draw,
                            NULL
                            );
}


void WM_PT_USDExportPanelsRegister()
{
  usd_export_panel_register_general();
  usd_export_panel_register_stage();
  usd_export_panel_register_types();
  usd_export_panel_register_geometry();
  usd_export_panel_register_materials();
  usd_export_panel_register_lights();
  usd_export_panel_register_rigging();
  usd_export_panel_register_animation();
  usd_export_panel_register_particles();
}


#endif /* WITH_USD */
