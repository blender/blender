/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BKE_context.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh" /* own include */

const EnumPropertyItem rna_enum_context_mode_items[] = {
    {CTX_MODE_EDIT_MESH, "EDIT_MESH", 0, "Mesh Edit", ""},
    {CTX_MODE_EDIT_CURVE, "EDIT_CURVE", 0, "Curve Edit", ""},
    {CTX_MODE_EDIT_CURVES, "EDIT_CURVES", 0, "Curves Edit", ""},
    {CTX_MODE_EDIT_SURFACE, "EDIT_SURFACE", 0, "Surface Edit", ""},
    {CTX_MODE_EDIT_TEXT, "EDIT_TEXT", 0, "Text Edit", ""},
    /* PARSKEL reuse will give issues */
    {CTX_MODE_EDIT_ARMATURE, "EDIT_ARMATURE", 0, "Armature Edit", ""},
    {CTX_MODE_EDIT_METABALL, "EDIT_METABALL", 0, "Metaball Edit", ""},
    {CTX_MODE_EDIT_LATTICE, "EDIT_LATTICE", 0, "Lattice Edit", ""},
    {CTX_MODE_EDIT_GREASE_PENCIL, "EDIT_GREASE_PENCIL", 0, "Grease Pencil Edit", ""},
    {CTX_MODE_EDIT_POINTCLOUD, "EDIT_POINTCLOUD", 0, "Point Cloud Edit", ""},
    {CTX_MODE_POSE, "POSE", 0, "Pose", ""},
    {CTX_MODE_SCULPT, "SCULPT", 0, "Sculpt", ""},
    {CTX_MODE_PAINT_WEIGHT, "PAINT_WEIGHT", 0, "Weight Paint", ""},
    {CTX_MODE_PAINT_VERTEX, "PAINT_VERTEX", 0, "Vertex Paint", ""},
    {CTX_MODE_PAINT_TEXTURE, "PAINT_TEXTURE", 0, "Texture Paint", ""},
    {CTX_MODE_PARTICLE, "PARTICLE", 0, "Particle", ""},
    {CTX_MODE_OBJECT, "OBJECT", 0, "Object", ""},
    {CTX_MODE_PAINT_GPENCIL_LEGACY, "PAINT_GPENCIL", 0, "Grease Pencil Paint", ""},
    {CTX_MODE_EDIT_GPENCIL_LEGACY, "EDIT_GPENCIL", 0, "Grease Pencil Edit", ""},
    {CTX_MODE_SCULPT_GPENCIL_LEGACY, "SCULPT_GPENCIL", 0, "Grease Pencil Sculpt", ""},
    {CTX_MODE_WEIGHT_GPENCIL_LEGACY, "WEIGHT_GPENCIL", 0, "Grease Pencil Weight Paint", ""},
    {CTX_MODE_VERTEX_GPENCIL_LEGACY, "VERTEX_GPENCIL", 0, "Grease Pencil Vertex Paint", ""},
    {CTX_MODE_SCULPT_CURVES, "SCULPT_CURVES", 0, "Curves Sculpt", ""},
    {CTX_MODE_PAINT_GREASE_PENCIL, "PAINT_GREASE_PENCIL", 0, "Grease Pencil Paint", ""},
    {CTX_MODE_SCULPT_GREASE_PENCIL, "SCULPT_GREASE_PENCIL", 0, "Grease Pencil Sculpt", ""},
    {CTX_MODE_WEIGHT_GREASE_PENCIL, "WEIGHT_GREASE_PENCIL", 0, "Grease Pencil Weight Paint", ""},
    {CTX_MODE_VERTEX_GREASE_PENCIL, "VERTEX_GREASE_PENCIL", 0, "Grease Pencil Vertex Paint", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "DNA_asset_types.h"
#  include "DNA_userdef_types.h"

#  ifdef WITH_PYTHON
#    include "BPY_extern.hh"
#  endif

#  include "RE_engine.h"

static PointerRNA rna_Context_manager_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_id_pointer_create(reinterpret_cast<ID *>(CTX_wm_manager(C)));
}

static PointerRNA rna_Context_window_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_manager(C)), &RNA_Window, CTX_wm_window(C));
}

static PointerRNA rna_Context_workspace_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_id_pointer_create(reinterpret_cast<ID *>(CTX_wm_workspace(C)));
}

static PointerRNA rna_Context_screen_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_id_pointer_create(reinterpret_cast<ID *>(CTX_wm_screen(C)));
}

static PointerRNA rna_Context_area_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  PointerRNA newptr = RNA_pointer_create_discrete(
      (ID *)CTX_wm_screen(C), &RNA_Area, CTX_wm_area(C));
  return newptr;
}

static PointerRNA rna_Context_space_data_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  PointerRNA newptr = RNA_pointer_create_discrete(
      (ID *)CTX_wm_screen(C), &RNA_Space, CTX_wm_space_data(C));
  return newptr;
}

static PointerRNA rna_Context_region_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  PointerRNA newptr = RNA_pointer_create_discrete(
      (ID *)CTX_wm_screen(C), &RNA_Region, CTX_wm_region(C));
  return newptr;
}

static PointerRNA rna_Context_region_data_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;

  /* only exists for one space still, no generic system yet */
  if (CTX_wm_view3d(C)) {
    PointerRNA newptr = RNA_pointer_create_discrete(
        (ID *)CTX_wm_screen(C), &RNA_RegionView3D, CTX_wm_region_data(C));
    return newptr;
  }

  return PointerRNA_NULL;
}

static PointerRNA rna_Context_region_popup_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  PointerRNA newptr = RNA_pointer_create_discrete(
      (ID *)CTX_wm_screen(C), &RNA_Region, CTX_wm_region_popup(C));
  return newptr;
}

static PointerRNA rna_Context_gizmo_group_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  PointerRNA newptr = RNA_pointer_create_discrete(nullptr, &RNA_GizmoGroup, CTX_wm_gizmo_group(C));
  return newptr;
}

static PointerRNA rna_Context_asset_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_pointer_create_discrete(nullptr, &RNA_AssetRepresentation, CTX_wm_asset(C));
}

static PointerRNA rna_Context_main_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_main_pointer_create(CTX_data_main(C));
}

static PointerRNA rna_Context_scene_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_id_pointer_create(reinterpret_cast<ID *>(CTX_data_scene(C)));
}

static PointerRNA rna_Context_view_layer_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_pointer_create_id_subdata(
      *reinterpret_cast<ID *>(CTX_data_scene(C)), &RNA_ViewLayer, CTX_data_view_layer(C));
}

static void rna_Context_engine_get(PointerRNA *ptr, char *value)
{
  bContext *C = (bContext *)ptr->data;
  RenderEngineType *engine_type = CTX_data_engine_type(C);
  strcpy(value, engine_type->idname);
}

static int rna_Context_engine_length(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  RenderEngineType *engine_type = CTX_data_engine_type(C);
  return strlen(engine_type->idname);
}

static PointerRNA rna_Context_collection_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_id_pointer_create(reinterpret_cast<ID *>(CTX_data_collection(C)));
}

static PointerRNA rna_Context_layer_collection_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return RNA_pointer_create_discrete(reinterpret_cast<ID *>(CTX_data_scene(C)),
                                     &RNA_LayerCollection,
                                     CTX_data_layer_collection(C));
}

static PointerRNA rna_Context_tool_settings_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  if (is_sequencer) {
    Scene *scene = CTX_data_sequencer_scene(C);
    if (scene) {
      ToolSettings *toolsettings = scene->toolsettings;
      return RNA_pointer_create_id_subdata(
          *reinterpret_cast<ID *>(scene), &RNA_ToolSettings, toolsettings);
    }
  }
  return RNA_pointer_create_id_subdata(
      *reinterpret_cast<ID *>(CTX_data_scene(C)), &RNA_ToolSettings, CTX_data_tool_settings(C));
}

static PointerRNA rna_Context_preferences_get(PointerRNA * /*ptr*/)
{
  PointerRNA newptr = RNA_pointer_create_discrete(nullptr, &RNA_Preferences, &U);
  return newptr;
}

static int rna_Context_mode_get(PointerRNA *ptr)
{
  bContext *C = (bContext *)ptr->data;
  return CTX_data_mode_enum(C);
}

static Depsgraph *rna_Context_evaluated_depsgraph_get(bContext *C)
{
  Depsgraph *depsgraph;

#  ifdef WITH_PYTHON
  /* Allow drivers to be evaluated */
  BPy_BEGIN_ALLOW_THREADS;
#  endif

  depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

#  ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#  endif

  return depsgraph;
}

#else

void RNA_def_context(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "Context", nullptr);
  RNA_def_struct_ui_text(srna, "Context", "Current windowmanager and data context");
  RNA_def_struct_sdna(srna, "bContext");

  /* WM */
  prop = RNA_def_property(srna, "window_manager", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "WindowManager");
  RNA_def_property_pointer_funcs(prop, "rna_Context_manager_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "window", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Window");
  RNA_def_property_pointer_funcs(prop, "rna_Context_window_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "workspace", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "WorkSpace");
  RNA_def_property_pointer_funcs(prop, "rna_Context_workspace_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "screen", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Screen");
  RNA_def_property_pointer_funcs(prop, "rna_Context_screen_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "area", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Area");
  RNA_def_property_pointer_funcs(prop, "rna_Context_area_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "space_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Space");
  RNA_def_property_pointer_funcs(prop, "rna_Context_space_data_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "",
                           "The current space, may be None in background-mode, "
                           "when the cursor is outside the window or "
                           "when using menu-search");

  prop = RNA_def_property(srna, "region", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Region");
  RNA_def_property_pointer_funcs(prop, "rna_Context_region_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "region_popup", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Region");
  RNA_def_property_pointer_funcs(prop, "rna_Context_region_popup_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Popup Region", "The temporary region for pop-ups (including menus and pop-overs)");

  prop = RNA_def_property(srna, "region_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "RegionView3D");
  RNA_def_property_pointer_funcs(prop, "rna_Context_region_data_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "gizmo_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "GizmoGroup");
  RNA_def_property_pointer_funcs(prop, "rna_Context_gizmo_group_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "asset", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "AssetRepresentation");
  RNA_def_property_pointer_funcs(prop, "rna_Context_asset_get", nullptr, nullptr, nullptr);

  /* Data */
  prop = RNA_def_property(srna, "blend_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "BlendData");
  RNA_def_property_pointer_funcs(prop, "rna_Context_main_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_pointer_funcs(prop, "rna_Context_scene_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "view_layer", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_pointer_funcs(prop, "rna_Context_view_layer_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "engine", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Context_engine_get", "rna_Context_engine_length", nullptr);

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_pointer_funcs(prop, "rna_Context_collection_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "layer_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "LayerCollection");
  RNA_def_property_pointer_funcs(
      prop, "rna_Context_layer_collection_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "tool_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "ToolSettings");
  RNA_def_property_pointer_funcs(prop, "rna_Context_tool_settings_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "preferences", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Preferences");
  RNA_def_property_pointer_funcs(prop, "rna_Context_preferences_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_context_mode_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_funcs(prop, "rna_Context_mode_get", nullptr, nullptr);

  func = RNA_def_function(srna, "evaluated_depsgraph_get", "rna_Context_evaluated_depsgraph_get");
  RNA_def_function_ui_description(
      func,
      "Get the dependency graph for the current scene and view layer, to access to data-blocks "
      "with animation and modifiers applied. If any data-blocks have been edited, the dependency "
      "graph will be updated. This invalidates all references to evaluated data-blocks from the "
      "dependency graph.");
  parm = RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "Evaluated dependency graph");
  RNA_def_function_return(func, parm);
}

#endif
