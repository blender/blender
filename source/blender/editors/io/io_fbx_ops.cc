/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_IO_FBX

#  include "BKE_context.hh"
#  include "BKE_file_handler.hh"
#  include "BKE_report.hh"

#  include "BLI_string.h"
#  include "BLI_string_utf8.h"

#  include "WM_api.hh"

#  include "DNA_space_types.h"

#  include "ED_outliner.hh"

#  include "RNA_access.hh"
#  include "RNA_define.hh"

#  include "BLT_translation.hh"

#  include "UI_interface.hh"
#  include "UI_interface_layout.hh"

#  include "IO_fbx.hh"
#  include "io_fbx_ops.hh"
#  include "io_utils.hh"

static const EnumPropertyItem fbx_vertex_colors_mode[] = {
    {int(eFBXVertexColorMode::None), "NONE", 0, "None", "Do not import color attributes"},
    {int(eFBXVertexColorMode::sRGB),
     "SRGB",
     0,
     "sRGB",
     "Vertex colors in the file are in sRGB color space"},
    {int(eFBXVertexColorMode::Linear),
     "LINEAR",
     0,
     "Linear",
     "Vertex colors in the file are in linear color space"},
    {0, nullptr, 0, nullptr, nullptr}};

static wmOperatorStatus wm_fbx_import_exec(bContext *C, wmOperator *op)
{
  FBXImportParams params;
  params.global_scale = RNA_float_get(op->ptr, "global_scale");
  params.use_custom_normals = RNA_boolean_get(op->ptr, "use_custom_normals");
  params.use_custom_props = RNA_boolean_get(op->ptr, "use_custom_props");
  params.props_enum_as_string = RNA_boolean_get(op->ptr, "use_custom_props_enum_as_string");
  params.ignore_leaf_bones = RNA_boolean_get(op->ptr, "ignore_leaf_bones");
  params.import_subdivision = RNA_boolean_get(op->ptr, "import_subdivision");
  params.validate_meshes = RNA_boolean_get(op->ptr, "validate_meshes");
  params.use_anim = RNA_boolean_get(op->ptr, "use_anim");
  params.anim_offset = RNA_float_get(op->ptr, "anim_offset");
  params.vertex_colors = eFBXVertexColorMode(RNA_enum_get(op->ptr, "import_colors"));

  params.reports = op->reports;

  const auto paths = blender::ed::io::paths_from_operator_properties(op->ptr);

  if (paths.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }
  for (const auto &path : paths) {
    STRNCPY(params.filepath, path.c_str());
    FBX_import(C, params);
  }

  Scene *scene = CTX_data_scene(C);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

static bool wm_fbx_import_check(bContext * /*C*/, wmOperator * /*op*/)
{
  return false;
}

static void ui_fbx_import_settings(const bContext *C, uiLayout *layout, PointerRNA *ptr)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  if (uiLayout *panel = layout->panel(C, "FBX_import_general", false, IFACE_("General"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "global_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "use_custom_props", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    uiLayout &subcol = col->column(false);
    subcol.active_set(RNA_boolean_get(ptr, "use_custom_props"));
    subcol.prop(ptr, "use_custom_props_enum_as_string", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *panel = layout->panel(C, "FBX_import_geometry", false, IFACE_("Geometry"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "use_custom_normals", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_subdivision", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "import_colors", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(ptr, "validate_meshes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  {
    PanelLayout panel = layout->panel(C, "FBX_import_anim", true);
    panel.header->use_property_split_set(false);
    panel.header->prop(ptr, "use_anim", UI_ITEM_NONE, "", ICON_NONE);
    panel.header->label(IFACE_("Animation"), ICON_NONE);
    if (panel.body) {
      uiLayout *col = &panel.body->column(false);
      col->prop(ptr, "anim_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }

  if (uiLayout *panel = layout->panel(C, "FBX_import_armature", false, IFACE_("Armature"))) {
    uiLayout *col = &panel->column(false);
    col->prop(ptr, "ignore_leaf_bones", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void wm_fbx_import_draw(bContext *C, wmOperator *op)
{
  ui_fbx_import_settings(C, op->layout, op->ptr);
}

void WM_OT_fbx_import(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Import FBX";
  ot->description = "Import FBX file into current scene";
  ot->idname = "WM_OT_fbx_import";

  ot->invoke = blender::ed::io::filesel_drop_import_invoke;
  ot->exec = wm_fbx_import_exec;
  ot->poll = WM_operator_winactive;
  ot->check = wm_fbx_import_check;
  ot->ui = wm_fbx_import_draw;
  ot->flag = OPTYPE_UNDO | OPTYPE_PRESET;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_FILES | WM_FILESEL_DIRECTORY |
                                     WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  RNA_def_float(ot->srna, "global_scale", 1.0f, 1e-6f, 1e6f, "Scale", "", 0.001f, 1000.0f);
  RNA_def_enum(ot->srna,
               "import_colors",
               fbx_vertex_colors_mode,
               int(eFBXVertexColorMode::sRGB),
               "Vertex Colors",
               "Import vertex color attributes");

  RNA_def_boolean(ot->srna,
                  "use_custom_normals",
                  true,
                  "Custom Normals",
                  "Import custom normals, if available (otherwise Blender will compute them)");
  RNA_def_boolean(ot->srna,
                  "use_custom_props",
                  true,
                  "Custom Properties",
                  "Import user properties as custom properties");
  RNA_def_boolean(ot->srna,
                  "use_custom_props_enum_as_string",
                  true,
                  "Enums As Strings",
                  "Store custom property enumeration values as strings");
  RNA_def_boolean(ot->srna,
                  "import_subdivision",
                  false,
                  "Subdivision Data",
                  "Import FBX subdivision information as subdivision surface modifiers");
  RNA_def_boolean(ot->srna,
                  "ignore_leaf_bones",
                  false,
                  "Ignore Leaf Bones",
                  "Ignore the last bone at the end of each chain (used to mark the length of the "
                  "previous bone)");
  RNA_def_boolean(
      ot->srna,
      "validate_meshes",
      true,
      "Validate Meshes",
      "Ensure the data is valid "
      "(when disabled, data may be imported which causes crashes displaying or editing)");

  RNA_def_boolean(ot->srna, "use_anim", true, "Import Animation", "Import FBX animation");
  prop = RNA_def_float(ot->srna,
                       "anim_offset",
                       1.0f,
                       -1e6f,
                       1e6f,
                       "Offset",
                       "Offset to apply to animation timestamps, in frames",
                       -1e4f,
                       1e4f);
  RNA_def_property_ui_range(prop, -1e4f, 1e4f, 100, 1);

  /* Only show `.fbx` files by default. */
  prop = RNA_def_string(ot->srna, "filter_glob", "*.fbx", 0, "Extension Filter", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

namespace blender::ed::io {
void fbx_file_handler_add()
{
  auto fh = std::make_unique<blender::bke::FileHandlerType>();
  STRNCPY_UTF8(fh->idname, "IO_FH_fbx");
  STRNCPY_UTF8(fh->import_operator, "WM_OT_fbx_import");
  STRNCPY_UTF8(fh->export_operator, "export_scene.fbx"); /* Use Python add-on for export. */
  STRNCPY_UTF8(fh->label, "FBX");
  STRNCPY_UTF8(fh->file_extensions_str, ".fbx");
  fh->poll_drop = poll_file_object_drop;
  bke::file_handler_add(std::move(fh));
}

}  // namespace blender::ed::io

#endif /* WITH_IO_FBX */
