/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_USD
#  include "DNA_modifier_types.h"
#  include "DNA_space_types.h"

#  include "BKE_context.hh"
#  include "BKE_file_handler.hh"
#  include "BKE_report.hh"

#  include "BLI_path_utils.hh"
#  include "BLI_string_utf8.h"

#  include "BLT_translation.hh"

#  include "ED_fileselect.hh"
#  include "ED_object.hh"

#  include "MEM_guardedalloc.h"

#  include "RNA_access.hh"
#  include "RNA_define.hh"
#  include "RNA_enum_types.hh"

#  include "UI_interface.hh"
#  include "UI_interface_layout.hh"
#  include "UI_resources.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  include "DEG_depsgraph.hh"

#  include "IO_orientation.hh"
#  include "io_usd.hh"
#  include "io_utils.hh"
#  include "usd.hh"

#  include <pxr/pxr.h>

#  include <string>
#  include <utility>

using namespace blender::io::usd;

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
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usd_property_import_mode_items[] = {
    {USD_ATTR_IMPORT_NONE, "NONE", 0, "None", "Do not import USD custom attributes"},
    {USD_ATTR_IMPORT_USER,
     "USER",
     0,
     "User",
     "Import USD attributes in the 'userProperties' namespace as Blender custom "
     "properties. The namespace will be stripped from the property names"},
    {USD_ATTR_IMPORT_ALL,
     "ALL",
     0,
     "All Custom",
     "Import all USD custom attributes as Blender custom properties. "
     "Namespaces will be retained in the property names"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usd_tex_import_mode_items[] = {
    {USD_TEX_IMPORT_NONE, "IMPORT_NONE", 0, "None", "Don't import textures"},
    {USD_TEX_IMPORT_PACK, "IMPORT_PACK", 0, "Packed", "Import textures as packed data"},
    {USD_TEX_IMPORT_COPY, "IMPORT_COPY", 0, "Copy", "Copy files to textures directory"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usd_tex_name_collision_mode_items[] = {
    {USD_TEX_NAME_COLLISION_USE_EXISTING,
     "USE_EXISTING",
     0,
     "Use Existing",
     "If a file with the same name already exists, use that instead of copying"},
    {USD_TEX_NAME_COLLISION_OVERWRITE, "OVERWRITE", 0, "Overwrite", "Overwrite existing files"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usd_export_subdiv_mode_items[] = {
    {USD_SUBDIV_IGNORE,
     "IGNORE",
     0,
     "Ignore",
     "Scheme = None. Export base mesh without subdivision"},
    {USD_SUBDIV_TESSELLATE,
     "TESSELLATE",
     0,
     "Tessellate",
     "Scheme = None. Export subdivided mesh"},
    {USD_SUBDIV_BEST_MATCH,
     "BEST_MATCH",
     0,
     "Best Match",
     "Scheme = Catmull-Clark, when possible. "
     "Reverts to exporting the subdivided mesh for the Simple subdivision type"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usd_xform_op_mode_items[] = {
    {USD_XFORM_OP_TRS,
     "TRS",
     0,
     "Translate, Rotate, Scale",
     "Export with translate, rotate, and scale Xform operators"},
    {USD_XFORM_OP_TOS,
     "TOS",
     0,
     "Translate, Orient, Scale",
     "Export with translate, orient quaternion, and scale Xform operators"},
    {USD_XFORM_OP_MAT, "MAT", 0, "Matrix", "Export matrix operator"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usdz_downscale_size[] = {
    {USD_TEXTURE_SIZE_KEEP, "KEEP", 0, "Keep", "Keep all current texture sizes"},
    {USD_TEXTURE_SIZE_256, "256", 0, "256", "Resize to a maximum of 256 pixels"},
    {USD_TEXTURE_SIZE_512, "512", 0, "512", "Resize to a maximum of 512 pixels"},
    {USD_TEXTURE_SIZE_1024, "1024", 0, "1024", "Resize to a maximum of 1024 pixels"},
    {USD_TEXTURE_SIZE_2048, "2048", 0, "2048", "Resize to a maximum of 2048 pixels"},
    {USD_TEXTURE_SIZE_4096, "4096", 0, "4096", "Resize to a maximum of 4096 pixels"},
    {USD_TEXTURE_SIZE_CUSTOM, "CUSTOM", 0, "Custom", "Specify a custom size"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usd_tex_export_mode_items[] = {
    {USD_TEX_EXPORT_KEEP, "KEEP", 0, "Keep", "Use original location of textures"},
    {USD_TEX_EXPORT_PRESERVE,
     "PRESERVE",
     0,
     "Preserve",
     "Preserve file paths of textures from already imported USD files.\n"
     "Export remaining textures to a 'textures' folder next to the USD file"},
    {USD_TEX_EXPORT_NEW_PATH,
     "NEW",
     0,
     "New Path",
     "Export textures to a 'textures' folder next to the USD file"},
    {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_usd_mtl_purpose_items[] = {
    {USD_MTL_PURPOSE_ALL,
     "MTL_ALL_PURPOSE",
     0,
     "All Purpose",
     "Attempt to import 'allPurpose' materials."},
    {USD_MTL_PURPOSE_PREVIEW,
     "MTL_PREVIEW",
     0,
     "Preview",
     "Attempt to import 'preview' materials. "
     "Load 'allPurpose' materials as a fallback"},
    {USD_MTL_PURPOSE_FULL,
     "MTL_FULL",
     0,
     "Full",
     "Attempt to import 'full' materials. "
     "Load 'allPurpose' or 'preview' materials, in that order, as a fallback"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_usd_convert_scene_units_items[] = {
    {USD_SCENE_UNITS_METERS, "METERS", 0, "Meters", "Scene meters per unit to 1.0"},
    {USD_SCENE_UNITS_KILOMETERS, "KILOMETERS", 0, "Kilometers", "Scene meters per unit to 1000.0"},
    {USD_SCENE_UNITS_CENTIMETERS,
     "CENTIMETERS",
     0,
     "Centimeters",
     "Scene meters per unit to 0.01"},
    {USD_SCENE_UNITS_MILLIMETERS,
     "MILLIMETERS",
     0,
     "Millimeters",
     "Scene meters per unit to 0.001"},
    {USD_SCENE_UNITS_INCHES, "INCHES", 0, "Inches", "Scene meters per unit to 0.0254"},
    {USD_SCENE_UNITS_FEET, "FEET", 0, "Feet", "Scene meters per unit to 0.3048"},
    {USD_SCENE_UNITS_YARDS, "YARDS", 0, "Yards", "Scene meters per unit to 0.9144"},
    {USD_SCENE_UNITS_CUSTOM,
     "CUSTOM",
     0,
     "Custom",
     "Specify a custom scene meters per unit value"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Stored in the wmOperator's customdata field to indicate it should run as a background job.
 * This is set when the operator is invoked, and not set when it is only executed. */
struct USDOperatorOptions {
  bool as_background_job;
};

static void free_operator_customdata(wmOperator *op)
{
  if (op->customdata) {
    USDOperatorOptions *options = static_cast<USDOperatorOptions *>(op->customdata);
    MEM_freeN(options);
    op->customdata = nullptr;
  }
}

/* Ensure that the prim_path is not set to
 * the absolute root path '/'. */
static void process_prim_path(std::string &prim_path)
{
  if (prim_path.empty()) {
    return;
  }

  /* The absolute root "/" path indicates a no-op, so clear the string. */
  if (prim_path == "/") {
    prim_path.clear();
  }
  /* If a prim path doesn't start with a "/" it is invalid when creating the prim. */
  else if (prim_path[0] != '/') {
    prim_path.insert(0, 1, '/');
  }
}

static wmOperatorStatus wm_usd_export_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  USDOperatorOptions *options = MEM_callocN<USDOperatorOptions>("USDOperatorOptions");
  options->as_background_job = true;
  op->customdata = options;

  ED_fileselect_ensure_default_filepath(C, op, ".usdc");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus wm_usd_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    free_operator_customdata(op);
    return OPERATOR_CANCELLED;
  }

  const USDOperatorOptions *options = static_cast<USDOperatorOptions *>(op->customdata);
  const bool as_background_job = (options != nullptr && options->as_background_job);
  free_operator_customdata(op);

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  const eUSDTexExportMode textures_mode = eUSDTexExportMode(
      RNA_enum_get(op->ptr, "export_textures_mode"));
  bool export_textures = false;
  bool use_original_paths = false;

  switch (textures_mode) {
    case eUSDTexExportMode::USD_TEX_EXPORT_PRESERVE:
      export_textures = false;
      use_original_paths = true;
      break;
    case eUSDTexExportMode::USD_TEX_EXPORT_NEW_PATH:
      export_textures = true;
      use_original_paths = false;
      break;
    case eUSDTexExportMode::USD_TEX_EXPORT_KEEP:
      export_textures = false;
      use_original_paths = false;
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  USDExportParams params;
  params.export_animation = RNA_boolean_get(op->ptr, "export_animation");
  params.selected_objects_only = RNA_boolean_get(op->ptr, "selected_objects_only");

  params.export_meshes = RNA_boolean_get(op->ptr, "export_meshes");
  params.export_lights = RNA_boolean_get(op->ptr, "export_lights");
  params.convert_world_material = params.export_lights &&
                                  RNA_boolean_get(op->ptr, "convert_world_material");
  params.export_cameras = RNA_boolean_get(op->ptr, "export_cameras");
  params.export_curves = RNA_boolean_get(op->ptr, "export_curves");
  params.export_points = RNA_boolean_get(op->ptr, "export_points");
  params.export_volumes = RNA_boolean_get(op->ptr, "export_volumes");
  params.export_hair = RNA_boolean_get(op->ptr, "export_hair");
  params.export_uvmaps = RNA_boolean_get(op->ptr, "export_uvmaps");
  params.rename_uvmaps = RNA_boolean_get(op->ptr, "rename_uvmaps");
  params.export_normals = RNA_boolean_get(op->ptr, "export_normals");
  params.export_mesh_colors = RNA_boolean_get(op->ptr, "export_mesh_colors");
  params.export_materials = RNA_boolean_get(op->ptr, "export_materials");

  params.export_armatures = RNA_boolean_get(op->ptr, "export_armatures");
  params.export_shapekeys = RNA_boolean_get(op->ptr, "export_shapekeys");
  params.only_deform_bones = RNA_boolean_get(op->ptr, "only_deform_bones");

  params.use_instancing = RNA_boolean_get(op->ptr, "use_instancing");
  params.export_custom_properties = RNA_boolean_get(op->ptr, "export_custom_properties");
  params.author_blender_name = RNA_boolean_get(op->ptr, "author_blender_name");
  params.allow_unicode = RNA_boolean_get(op->ptr, "allow_unicode");

  params.export_subdiv = eSubdivExportMode(RNA_enum_get(op->ptr, "export_subdivision"));
  params.evaluation_mode = eEvaluationMode(RNA_enum_get(op->ptr, "evaluation_mode"));

  params.generate_preview_surface = RNA_boolean_get(op->ptr, "generate_preview_surface");
  params.generate_materialx_network = RNA_boolean_get(op->ptr, "generate_materialx_network");
  params.overwrite_textures = RNA_boolean_get(op->ptr, "overwrite_textures");
  params.relative_paths = RNA_boolean_get(op->ptr, "relative_paths");
  params.export_textures = export_textures;
  params.use_original_paths = use_original_paths;

  params.triangulate_meshes = RNA_boolean_get(op->ptr, "triangulate_meshes");
  params.quad_method = RNA_enum_get(op->ptr, "quad_method");
  params.ngon_method = RNA_enum_get(op->ptr, "ngon_method");

  params.convert_orientation = RNA_boolean_get(op->ptr, "convert_orientation");
  params.forward_axis = eIOAxis(RNA_enum_get(op->ptr, "export_global_forward_selection"));
  params.up_axis = eIOAxis(RNA_enum_get(op->ptr, "export_global_up_selection"));
  params.xform_op_mode = eUSDXformOpMode(RNA_enum_get(op->ptr, "xform_op_mode"));

  params.usdz_downscale_size = eUSDZTextureDownscaleSize(
      RNA_enum_get(op->ptr, "usdz_downscale_size"));
  params.usdz_downscale_custom_size = RNA_int_get(op->ptr, "usdz_downscale_custom_size");
  params.convert_scene_units = eUSDSceneUnits(RNA_enum_get(op->ptr, "convert_scene_units"));
  params.custom_meters_per_unit = RNA_float_get(op->ptr, "meters_per_unit");

  params.merge_parent_xform = RNA_boolean_get(op->ptr, "merge_parent_xform");

  params.root_prim_path = RNA_string_get(op->ptr, "root_prim_path");
  process_prim_path(params.root_prim_path);

  RNA_string_get(op->ptr, "custom_properties_namespace", params.custom_properties_namespace);
  RNA_string_get(op->ptr, "collection", params.collection);

  bool ok = USD_export(C, filepath, &params, as_background_job, op->reports);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void wm_usd_export_draw(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  PointerRNA *ptr = op->ptr;

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  if (uiLayout *panel = layout->panel(C, "USD_export_general", false, IFACE_("General"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "root_prim_path", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    uiLayout *sub = &col->column(true, IFACE_("Include"));
    if (CTX_wm_space_file(C)) {
      sub->prop(ptr, "selected_objects_only", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    sub->prop(ptr, "export_animation", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    sub = &col->column(true, IFACE_("Blender Data"));
    sub->prop(ptr, "export_custom_properties", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    uiLayout *props_col = &sub->column(true);
    props_col->prop(ptr, "custom_properties_namespace", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    props_col->prop(ptr, "author_blender_name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    props_col->active_set(RNA_boolean_get(op->ptr, "export_custom_properties"));
    sub->prop(ptr, "allow_unicode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    sub = &col->column(true, IFACE_("File References"));
    sub->prop(ptr, "relative_paths", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &panel->column(false);
    col->prop(ptr, "convert_orientation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (RNA_boolean_get(ptr, "convert_orientation")) {
      col->prop(ptr, "export_global_forward_selection", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col->prop(ptr, "export_global_up_selection", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }

    col->prop(ptr, "convert_scene_units", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (eUSDSceneUnits(RNA_enum_get(ptr, "convert_scene_units")) == USD_SCENE_UNITS_CUSTOM) {
      col->prop(ptr, "meters_per_unit", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }

    col->prop(ptr, "xform_op_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &panel->column(false);
    col->prop(ptr, "evaluation_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_export_types", false, IFACE_("Object Types"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "export_meshes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "export_lights", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    uiLayout *row = &col->row(true);
    row->prop(ptr, "convert_world_material", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    const bool export_lights = RNA_boolean_get(ptr, "export_lights");
    row->active_set(export_lights);

    col->prop(ptr, "export_cameras", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "export_curves", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "export_points", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "export_volumes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "export_hair", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_export_geometry", false, IFACE_("Geometry"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "export_uvmaps", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "rename_uvmaps", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "export_normals", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->prop(ptr, "merge_parent_xform", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "triangulate_meshes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (RNA_boolean_get(ptr, "triangulate_meshes")) {
      col->prop(ptr, "quad_method", UI_ITEM_NONE, IFACE_("Method Quads"), ICON_NONE);
      col->prop(ptr, "ngon_method", UI_ITEM_NONE, IFACE_("Polygons"), ICON_NONE);
    }

    col->prop(ptr, "export_subdivision", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_export_rigging", true, IFACE_("Rigging"))) {
    uiLayout *col = &panel->column(false);

    col->prop(ptr, "export_shapekeys", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "export_armatures", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    uiLayout *row = &col->row(true);
    row->prop(ptr, "only_deform_bones", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row->active_set(RNA_boolean_get(ptr, "export_armatures"));
  }

  {
    PanelLayout panel = layout->panel(C, "USD_export_materials", true);
    panel.header->use_property_split_set(false);
    panel.header->prop(ptr, "export_materials", UI_ITEM_NONE, "", ICON_NONE);
    panel.header->label(IFACE_("Materials"), ICON_NONE);
    if (panel.body) {
      const bool export_materials = RNA_boolean_get(ptr, "export_materials");
      panel.body->active_set(export_materials);

      uiLayout *col = &panel.body->column(false);
      col->prop(ptr, "generate_preview_surface", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col->prop(ptr, "generate_materialx_network", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col = &panel.body->column(true);
      col->use_property_split_set(true);

      col->prop(ptr, "export_textures_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

      const eUSDTexExportMode textures_mode = eUSDTexExportMode(
          RNA_enum_get(op->ptr, "export_textures_mode"));

      uiLayout *col2 = &col->column(true);
      col2->use_property_split_set(true);
      col2->enabled_set(textures_mode == USD_TEX_EXPORT_NEW_PATH);
      col2->prop(ptr, "overwrite_textures", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col2->prop(ptr, "usdz_downscale_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      if (RNA_enum_get(ptr, "usdz_downscale_size") == USD_TEXTURE_SIZE_CUSTOM) {
        col2->prop(ptr, "usdz_downscale_custom_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
    }
  }

  if (uiLayout *panel = layout->panel(C, "USD_export_experimental", true, IFACE_("Experimental")))
  {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "use_instancing", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void wm_usd_export_cancel(bContext * /*C*/, wmOperator *op)
{
  free_operator_customdata(op);
}

static bool wm_usd_export_check(bContext * /*C*/, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check_n(filepath, ".usd", ".usda", ".usdc", ".usdz", nullptr)) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".usdc");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

static void forward_axis_update(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
{
  int forward = RNA_enum_get(ptr, "export_global_forward_selection");
  int up = RNA_enum_get(ptr, "export_global_up_selection");
  if ((forward % 3) == (up % 3)) {
    RNA_enum_set(ptr, "export_global_up_selection", (up + 1) % 6);
  }
}

static void up_axis_update(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
{
  int forward = RNA_enum_get(ptr, "export_global_forward_selection");
  int up = RNA_enum_get(ptr, "export_global_up_selection");
  if ((forward % 3) == (up % 3)) {
    RNA_enum_set(ptr, "export_global_forward_selection", (forward + 1) % 6);
  }
}

void WM_OT_usd_export(wmOperatorType *ot)
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

  prop = RNA_def_string(ot->srna, "collection", nullptr, MAX_ID_NAME - 2, "Collection", nullptr);
  RNA_def_property_flag(prop, PROP_HIDDEN);

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
                  "rename_uvmaps",
                  true,
                  "Rename UV Maps",
                  "Rename active render UV map to \"st\" to match USD conventions");
  RNA_def_boolean(ot->srna,
                  "export_mesh_colors",
                  true,
                  "Color Attributes",
                  "Include mesh color attributes in the export");
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

  RNA_def_enum(ot->srna,
               "export_subdivision",
               rna_enum_usd_export_subdiv_mode_items,
               USD_SUBDIV_BEST_MATCH,
               "Subdivision",
               "Choose how subdivision modifiers will be mapped to the USD subdivision scheme "
               "during export");

  RNA_def_boolean(ot->srna,
                  "export_armatures",
                  true,
                  "Armatures",
                  "Export armatures and meshes with armature modifiers as USD skeletons and "
                  "skinned meshes");

  RNA_def_boolean(ot->srna,
                  "only_deform_bones",
                  false,
                  "Only Deform Bones",
                  "Only export deform bones and their parents");

  RNA_def_boolean(
      ot->srna, "export_shapekeys", true, "Shape Keys", "Export shape keys as USD blend shapes");

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
                  "USD Preview Surface Network",
                  "Generate an approximate USD Preview Surface shader "
                  "representation of a Principled BSDF node network");

  RNA_def_boolean(ot->srna,
                  "generate_materialx_network",
                  false,
                  "MaterialX Network",
                  "Generate a MaterialX network representation of the materials");

  RNA_def_boolean(
      ot->srna,
      "convert_orientation",
      false,
      "Convert Orientation",
      "Convert orientation axis to a different convention to match other applications");

  prop = RNA_def_enum(ot->srna,
                      "export_global_forward_selection",
                      io_transform_axis,
                      IO_AXIS_NEGATIVE_Z,
                      "Forward Axis",
                      "");
  RNA_def_property_update_runtime(prop, forward_axis_update);

  prop = RNA_def_enum(
      ot->srna, "export_global_up_selection", io_transform_axis, IO_AXIS_Y, "Up Axis", "");
  RNA_def_property_update_runtime(prop, up_axis_update);

  RNA_def_enum(ot->srna,
               "export_textures_mode",
               rna_enum_usd_tex_export_mode_items,
               USD_TEX_EXPORT_NEW_PATH,
               "Export Textures",
               "Texture export method");

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

  RNA_def_enum(ot->srna,
               "xform_op_mode",
               rna_enum_usd_xform_op_mode_items,
               USD_XFORM_OP_TRS,
               "Xform Ops",
               "The type of transform operators to write");

  RNA_def_string(ot->srna,
                 "root_prim_path",
                 "/root",
                 0,
                 "Root Prim",
                 "If set, add a transform primitive with the given path to the stage "
                 "as the parent of all exported data");

  RNA_def_boolean(ot->srna,
                  "export_custom_properties",
                  true,
                  "Custom Properties",
                  "Export custom properties as USD attributes");

  RNA_def_string(ot->srna,
                 "custom_properties_namespace",
                 "userProperties",
                 MAX_IDPROP_NAME,
                 "Namespace",
                 "If set, add the given namespace as a prefix to exported custom property names. "
                 "This only applies to property names that do not already have a prefix "
                 "(e.g., it would apply to name 'bar' but not 'foo:bar') and does not apply "
                 "to blender object and data names which are always exported in the "
                 "'userProperties:blender' namespace");

  RNA_def_boolean(ot->srna,
                  "author_blender_name",
                  true,
                  "Blender Names",
                  "Author USD custom attributes containing the original Blender object and "
                  "object data names");

  RNA_def_boolean(
      ot->srna,
      "convert_world_material",
      true,
      "World Dome Light",
      "Convert the world material to a USD dome light. "
      "Currently works for simple materials, consisting of an environment texture "
      "connected to a background shader, with an optional vector multiply of the texture color");

  RNA_def_boolean(
      ot->srna,
      "allow_unicode",
      true,
      "Allow Unicode",
      "Preserve UTF-8 encoded characters when writing USD prim and property names "
      "(requires software utilizing USD 24.03 or greater when opening the resulting files)");

  RNA_def_boolean(ot->srna, "export_meshes", true, "Meshes", "Export all meshes");

  RNA_def_boolean(ot->srna, "export_lights", true, "Lights", "Export all lights");

  RNA_def_boolean(ot->srna, "export_cameras", true, "Cameras", "Export all cameras");

  RNA_def_boolean(ot->srna, "export_curves", true, "Curves", "Export all curves");

  RNA_def_boolean(ot->srna, "export_points", true, "Point Clouds", "Export all point clouds");

  RNA_def_boolean(ot->srna, "export_volumes", true, "Volumes", "Export all volumes");

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

  RNA_def_enum(ot->srna,
               "usdz_downscale_size",
               rna_enum_usdz_downscale_size,
               DAG_EVAL_VIEWPORT,
               "USDZ Texture Downsampling",
               "Choose a maximum size for all exported textures");

  RNA_def_int(ot->srna,
              "usdz_downscale_custom_size",
              128,
              64,
              16384,
              "USDZ Custom Downscale Size",
              "Custom size for downscaling exported textures",
              128,
              8192);

  RNA_def_boolean(ot->srna,
                  "merge_parent_xform",
                  false,
                  "Merge parent Xform",
                  "Merge USD primitives with their Xform parent if possible. USD does not allow "
                  "nested UsdGeomGprims, intermediary Xform prims will be defined to keep the USD "
                  "file valid when encountering object hierarchies.");

  RNA_def_enum(ot->srna,
               "convert_scene_units",
               rna_enum_usd_convert_scene_units_items,
               eUSDSceneUnits::USD_SCENE_UNITS_METERS,
               "Units",
               "Set the USD Stage meters per unit to the chosen measurement, or a custom value");

  RNA_def_float(ot->srna,
                "meters_per_unit",
                1.0f,
                0.0001f,
                1000.0f,
                "Meters Per Unit",
                "Custom value for meters per unit in the USD Stage",
                0.0001f,
                1000.0f);
}

/* ====== USD Import ====== */

static wmOperatorStatus wm_usd_import_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  USDOperatorOptions *options = MEM_callocN<USDOperatorOptions>("USDOperatorOptions");
  options->as_background_job = true;
  op->customdata = options;

  return blender::ed::io::filesel_drop_import_invoke(C, op, event);
}

static wmOperatorStatus wm_usd_import_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    free_operator_customdata(op);
    return OPERATOR_CANCELLED;
  }

  const USDOperatorOptions *options = static_cast<USDOperatorOptions *>(op->customdata);
  const bool as_background_job = (options != nullptr && options->as_background_job);
  free_operator_customdata(op);

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  const bool read_mesh_uvs = RNA_boolean_get(op->ptr, "read_mesh_uvs");
  const bool read_mesh_colors = RNA_boolean_get(op->ptr, "read_mesh_colors");
  const bool read_mesh_attributes = RNA_boolean_get(op->ptr, "read_mesh_attributes");

  char mesh_read_flag = MOD_MESHSEQ_READ_VERT | MOD_MESHSEQ_READ_POLY;
  if (read_mesh_uvs) {
    mesh_read_flag |= MOD_MESHSEQ_READ_UV;
  }
  if (read_mesh_colors) {
    mesh_read_flag |= MOD_MESHSEQ_READ_COLOR;
  }
  if (read_mesh_attributes) {
    mesh_read_flag |= MOD_MESHSEQ_READ_ATTRIBUTES;
  }

  USDImportParams params{};
  params.scale = RNA_float_get(op->ptr, "scale");
  params.light_intensity_scale = RNA_float_get(op->ptr, "light_intensity_scale");
  params.apply_unit_conversion_scale = RNA_boolean_get(op->ptr, "apply_unit_conversion_scale");

  params.mesh_read_flag = mesh_read_flag;
  params.set_frame_range = RNA_boolean_get(op->ptr, "set_frame_range");

  /* TODO(makowalski): Add support for sequences. */
  params.is_sequence = false;
  params.sequence_len = 1;
  params.offset = 0;
  params.relative_path = RNA_boolean_get(op->ptr, "relative_path");

  params.import_visible_only = RNA_boolean_get(op->ptr, "import_visible_only");
  params.import_defined_only = RNA_boolean_get(op->ptr, "import_defined_only");

  params.import_cameras = RNA_boolean_get(op->ptr, "import_cameras");
  params.import_curves = RNA_boolean_get(op->ptr, "import_curves");
  params.import_lights = RNA_boolean_get(op->ptr, "import_lights");
  params.create_world_material = params.import_lights &&
                                 RNA_boolean_get(op->ptr, "create_world_material");
  params.import_materials = RNA_boolean_get(op->ptr, "import_materials");
  params.import_all_materials = RNA_boolean_get(op->ptr, "import_all_materials");
  params.import_meshes = RNA_boolean_get(op->ptr, "import_meshes");
  params.import_points = RNA_boolean_get(op->ptr, "import_points");
  params.import_subdivision = RNA_boolean_get(op->ptr, "import_subdivision");
  params.import_volumes = RNA_boolean_get(op->ptr, "import_volumes");

  params.create_collection = RNA_boolean_get(op->ptr, "create_collection");
  params.support_scene_instancing = RNA_boolean_get(op->ptr, "support_scene_instancing");

  params.import_shapes = RNA_boolean_get(op->ptr, "import_shapes");
  params.import_skeletons = RNA_boolean_get(op->ptr, "import_skeletons");
  params.import_blendshapes = RNA_boolean_get(op->ptr, "import_blendshapes");

  params.validate_meshes = RNA_boolean_get(op->ptr, "validate_meshes");
  params.merge_parent_xform = RNA_boolean_get(op->ptr, "merge_parent_xform");

  params.import_guide = RNA_boolean_get(op->ptr, "import_guide");
  params.import_proxy = RNA_boolean_get(op->ptr, "import_proxy");
  params.import_render = RNA_boolean_get(op->ptr, "import_render");

  params.import_usd_preview = RNA_boolean_get(op->ptr, "import_usd_preview");
  params.set_material_blend = RNA_boolean_get(op->ptr, "set_material_blend");
  params.mtl_purpose = eUSDMtlPurpose(RNA_enum_get(op->ptr, "mtl_purpose"));
  params.mtl_name_collision_mode = eUSDMtlNameCollisionMode(
      RNA_enum_get(op->ptr, "mtl_name_collision_mode"));
  params.import_textures_mode = eUSDTexImportMode(RNA_enum_get(op->ptr, "import_textures_mode"));
  params.tex_name_collision_mode = eUSDTexNameCollisionMode(
      RNA_enum_get(op->ptr, "tex_name_collision_mode"));

  params.property_import_mode = eUSDPropertyImportMode(
      RNA_enum_get(op->ptr, "property_import_mode"));

  params.prim_path_mask = RNA_string_get(op->ptr, "prim_path_mask");

  RNA_string_get(op->ptr, "import_textures_dir", params.import_textures_dir);

  /* Switch out of edit mode to avoid being stuck in it (#54326). */
  const Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    blender::ed::object::mode_set(C, OB_MODE_EDIT);
  }

  const bool ok = USD_import(C, filepath, &params, as_background_job, op->reports);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void wm_usd_import_cancel(bContext * /*C*/, wmOperator *op)
{
  free_operator_customdata(op);
}

static void wm_usd_import_draw(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  PointerRNA *ptr = op->ptr;

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  if (uiLayout *panel = layout->panel(C, "USD_import_general", false, IFACE_("General"))) {
    uiLayout *col = &panel->column(false);

    col->prop(ptr, "prim_path_mask", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    uiLayout *sub = &col->column(true, IFACE_("Include"));
    sub->prop(ptr, "import_visible_only", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub->prop(ptr, "import_defined_only", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &panel->column(false);
    col->prop(ptr, "set_frame_range", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "create_collection", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "relative_path", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->prop(ptr, "apply_unit_conversion_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "light_intensity_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "property_import_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_import_types", false, IFACE_("Object Types"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "import_cameras", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_curves", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_lights", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    uiLayout *row = &col->row(true);
    row->prop(ptr, "create_world_material", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    const bool import_lights = RNA_boolean_get(ptr, "import_lights");
    row->active_set(import_lights);

    col->prop(ptr, "import_materials", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_meshes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_volumes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_points", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_shapes", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &panel->column(true, IFACE_("Display Purpose"));
    col->prop(ptr, "import_render", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_proxy", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_guide", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &panel->column(true, IFACE_("Material Purpose"));
    col->prop(ptr, "mtl_purpose", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_import_geometry", true, IFACE_("Geometry"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "read_mesh_uvs", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "read_mesh_colors", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "read_mesh_attributes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_subdivision", UI_ITEM_NONE, IFACE_("Subdivision"), ICON_NONE);

    col = &panel->column(false);
    col->prop(ptr, "validate_meshes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "merge_parent_xform", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_import_rigging", true, IFACE_("Rigging"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "import_blendshapes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_skeletons", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_import_material", true, IFACE_("Materials"))) {
    uiLayout *col = &panel->column(false);

    col->prop(ptr, "import_all_materials", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_usd_preview", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->enabled_set(RNA_boolean_get(ptr, "import_materials"));

    uiLayout *row = &col->row(true);
    row->prop(ptr, "set_material_blend", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row->enabled_set(RNA_boolean_get(ptr, "import_usd_preview"));
    col->prop(ptr, "mtl_name_collision_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "USD_import_texture", true, IFACE_("Textures"))) {
    uiLayout *col = &panel->column(false);

    col->prop(ptr, "import_textures_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    bool copy_textures = RNA_enum_get(op->ptr, "import_textures_mode") == USD_TEX_IMPORT_COPY;

    uiLayout *row = &col->row(true);
    row->prop(ptr, "import_textures_dir", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row->enabled_set(copy_textures);
    row = &col->row(true);
    row->prop(ptr, "tex_name_collision_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row->enabled_set(copy_textures);
    col->enabled_set(RNA_boolean_get(ptr, "import_materials"));
  }

  if (uiLayout *panel = layout->panel(
          C, "USD_import_instancing", true, IFACE_("Particles and Instancing")))
  {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "support_scene_instancing", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

void WM_OT_usd_import(wmOperatorType *ot)
{
  ot->name = "Import USD";
  ot->description = "Import USD stage into current scene";
  ot->idname = "WM_OT_usd_import";

  ot->invoke = wm_usd_import_invoke;
  ot->exec = wm_usd_import_exec;
  ot->cancel = wm_usd_import_cancel;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_usd_import_draw;

  ot->flag = OPTYPE_UNDO | OPTYPE_PRESET;

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
  RNA_def_boolean(ot->srna, "import_shapes", true, "USD Shapes", "");
  RNA_def_boolean(ot->srna, "import_skeletons", true, "Armatures", "");
  RNA_def_boolean(ot->srna, "import_blendshapes", true, "Shape Keys", "");
  RNA_def_boolean(ot->srna, "import_points", true, "Point Clouds", "");

  RNA_def_boolean(ot->srna,
                  "import_subdivision",
                  false,
                  "Import Subdivision Scheme",
                  "Create subdivision surface modifiers based on the USD "
                  "SubdivisionScheme attribute");

  RNA_def_boolean(ot->srna,
                  "support_scene_instancing",
                  true,
                  "Scene Instancing",
                  "Import USD scene graph instances as collection instances");

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

  RNA_def_boolean(ot->srna,
                  "read_mesh_attributes",
                  true,
                  "Mesh Attributes",
                  "Read USD Primvars as mesh attributes");

  RNA_def_string(ot->srna,
                 "prim_path_mask",
                 nullptr,
                 0,
                 "Path Mask",
                 "Import only the primitive at the given path and its descendants. "
                 "Multiple paths may be specified in a list delimited by commas or semicolons");

  RNA_def_boolean(ot->srna, "import_guide", false, "Guide", "Import guide geometry");

  RNA_def_boolean(ot->srna, "import_proxy", false, "Proxy", "Import proxy geometry");

  RNA_def_boolean(ot->srna, "import_render", true, "Render", "Import final render geometry");

  RNA_def_boolean(ot->srna,
                  "import_all_materials",
                  false,
                  "Import All Materials",
                  "Also import materials that are not used by any geometry. "
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

  RNA_def_enum(ot->srna,
               "mtl_purpose",
               rna_enum_usd_mtl_purpose_items,
               USD_MTL_PURPOSE_FULL,
               "Material Purpose",
               "Attempt to import materials with the given purpose. "
               "If no material with this purpose is bound to the primitive, "
               "fall back on loading any other bound material");

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

  RNA_def_enum(ot->srna,
               "property_import_mode",
               rna_enum_usd_property_import_mode_items,
               USD_ATTR_IMPORT_ALL,
               "Custom Properties",
               "Behavior when importing USD attributes as Blender custom properties");

  RNA_def_boolean(
      ot->srna,
      "validate_meshes",
      false,
      "Validate Meshes",
      "Ensure the data is valid "
      "(when disabled, data may be imported which causes crashes displaying or editing)");

  RNA_def_boolean(ot->srna,
                  "create_world_material",
                  true,
                  "World Dome Light",
                  "Convert the first discovered USD dome light to a world background shader");

  RNA_def_boolean(ot->srna,
                  "import_defined_only",
                  true,
                  "Defined Primitives Only",
                  "Import only defined USD primitives. When disabled this allows importing USD "
                  "primitives which are not defined, such as those with an override specifier");

  RNA_def_boolean(ot->srna,
                  "merge_parent_xform",
                  true,
                  "Merge parent Xform",
                  "Allow USD primitives to merge with their Xform parent "
                  "if they are the only child in the hierarchy");

  RNA_def_boolean(
      ot->srna,
      "apply_unit_conversion_scale",
      true,
      "Apply Unit Conversion Scale",
      "Scale the scene objects by the USD stage's meters per unit value. "
      "This scaling is applied in addition to the value specified in the Scale option");
}

namespace blender::ed::io {
void usd_file_handler_add()
{
  auto fh = std::make_unique<blender::bke::FileHandlerType>();
  STRNCPY_UTF8(fh->idname, "IO_FH_usd");
  STRNCPY_UTF8(fh->import_operator, "WM_OT_usd_import");
  STRNCPY_UTF8(fh->export_operator, "WM_OT_usd_export");
  STRNCPY_UTF8(fh->label, "Universal Scene Description");
  STRNCPY_UTF8(fh->file_extensions_str, ".usd;.usda;.usdc;.usdz");
  fh->poll_drop = poll_file_object_drop;
  bke::file_handler_add(std::move(fh));
}
}  // namespace blender::ed::io

#endif /* WITH_USD */
