/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "rna_internal.hh"

#include "DNA_workspace_types.h"

#ifdef RNA_RUNTIME

#  include "BLI_listbase.h"
#  include "BLI_string.h"

#  include "BKE_global.hh"
#  include "BKE_paint.hh"
#  include "BKE_paint_types.hh"
#  include "BKE_report.hh"
#  include "BKE_workspace.hh"

#  include "DNA_screen_types.h"
#  include "DNA_space_types.h"

#  include "ED_asset.hh"
#  include "ED_paint.hh"
#  include "ED_sequencer.hh"

#  include "RNA_access.hh"

#  include "WM_toolsystem.hh"

static void rna_window_update_all(Main * /*bmain*/, Scene * /*scene*/, PointerRNA * /*ptr*/)
{
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

void rna_workspace_screens_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  WorkSpace *workspace = (WorkSpace *)ptr->owner_id;
  rna_iterator_listbase_begin(iter, ptr, &workspace->layouts, nullptr);
}

static PointerRNA rna_workspace_screens_item_get(CollectionPropertyIterator *iter)
{
  WorkSpaceLayout *layout = static_cast<WorkSpaceLayout *>(rna_iterator_listbase_get(iter));
  bScreen *screen = BKE_workspace_layout_screen_get(layout);

  return RNA_id_pointer_create(reinterpret_cast<ID *>(screen));
}

/* workspace.owner_ids */

static wmOwnerID *rna_WorkSpace_owner_ids_new(WorkSpace *workspace, const char *name)
{
  wmOwnerID *owner_id = MEM_callocN<wmOwnerID>(__func__);
  BLI_addtail(&workspace->owner_ids, owner_id);
  STRNCPY(owner_id->name, name);
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return owner_id;
}

static void rna_WorkSpace_owner_ids_remove(WorkSpace *workspace,
                                           ReportList *reports,
                                           PointerRNA *wstag_ptr)
{
  wmOwnerID *owner_id = static_cast<wmOwnerID *>(wstag_ptr->data);
  if (BLI_remlink_safe(&workspace->owner_ids, owner_id) == false) {
    BKE_reportf(reports,
                RPT_ERROR,
                "wmOwnerID '%s' not in workspace '%s'",
                owner_id->name,
                workspace->id.name + 2);
    return;
  }

  MEM_freeN(owner_id);
  wstag_ptr->invalidate();

  WM_main_add_notifier(NC_WINDOW, nullptr);
}

static void rna_WorkSpace_owner_ids_clear(WorkSpace *workspace)
{
  BLI_freelistN(&workspace->owner_ids);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, workspace);
}

static int rna_WorkSpace_asset_library_get(PointerRNA *ptr)
{
  const WorkSpace *workspace = static_cast<WorkSpace *>(ptr->data);
  return blender::ed::asset::library_reference_to_enum_value(&workspace->asset_library_ref);
}

static void rna_WorkSpace_asset_library_set(PointerRNA *ptr, int value)
{
  WorkSpace *workspace = static_cast<WorkSpace *>(ptr->data);
  workspace->asset_library_ref = blender::ed::asset::library_reference_from_enum_value(value);
}

static bToolRef *rna_WorkSpace_tools_from_tkey(WorkSpace *workspace,
                                               const bToolKey *tkey,
                                               bool create)
{
  if (create) {
    bToolRef *tref;
    WM_toolsystem_ref_ensure(workspace, tkey, &tref);
    return tref;
  }
  return WM_toolsystem_ref_find(workspace, tkey);
}

static bToolRef *rna_WorkSpace_tools_from_space_view3d_mode(WorkSpace *workspace,
                                                            int mode,
                                                            bool create)
{
  bToolKey key{};
  key.space_type = SPACE_VIEW3D;
  key.mode = mode;
  return rna_WorkSpace_tools_from_tkey(workspace, &key, create);
}

static bToolRef *rna_WorkSpace_tools_from_space_image_mode(WorkSpace *workspace,
                                                           int mode,
                                                           bool create)
{
  bToolKey key{};
  key.space_type = SPACE_IMAGE;
  key.mode = mode;
  return rna_WorkSpace_tools_from_tkey(workspace, &key, create);
}

static bToolRef *rna_WorkSpace_tools_from_space_node(WorkSpace *workspace, bool create)
{
  bToolKey key{};
  key.space_type = SPACE_NODE;
  key.mode = 0;
  return rna_WorkSpace_tools_from_tkey(workspace, &key, create);
}
static bToolRef *rna_WorkSpace_tools_from_space_sequencer(WorkSpace *workspace,
                                                          int mode,
                                                          bool create)
{
  bToolKey key{};
  key.space_type = SPACE_SEQ;
  key.mode = mode;
  return rna_WorkSpace_tools_from_tkey(workspace, &key, create);
}
const EnumPropertyItem *rna_WorkSpace_tools_mode_itemf(bContext * /*C*/,
                                                       PointerRNA *ptr,
                                                       PropertyRNA * /*prop*/,
                                                       bool * /*r_free*/)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  switch (tref->space_type) {
    case SPACE_VIEW3D:
      return rna_enum_context_mode_items;
    case SPACE_IMAGE:
      return rna_enum_space_image_mode_all_items;
    case SPACE_SEQ:
      return rna_enum_space_sequencer_view_type_items;
  }
  return rna_enum_dummy_DEFAULT_items;
}

static bool rna_WorkSpaceTool_use_paint_canvas_get(PointerRNA *ptr)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  return ED_image_paint_brush_type_use_canvas(nullptr, tref);
}

static int rna_WorkSpaceTool_index_get(PointerRNA *ptr)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  return (tref->runtime) ? tref->runtime->index : 0;
}

static bool rna_WorkSpaceTool_has_datablock_get(PointerRNA *ptr)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  return (tref->runtime) ? (tref->runtime->data_block[0] != '\0') : false;
}

static bool rna_WorkSpaceTool_use_brushes_get(PointerRNA *ptr)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  return (tref->runtime) ? ((tref->runtime->flag & TOOLREF_FLAG_USE_BRUSHES) != 0) : false;
}

static int rna_WorkSpaceTool_brush_type_get(PointerRNA *ptr)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  return tref->runtime ? tref->runtime->brush_type : -1;
}

const EnumPropertyItem *rna_WorkSpaceTool_brush_type_itemf(bContext *C,
                                                           PointerRNA *ptr,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free)
{

  PaintMode paint_mode = [&]() {
    if (ptr->type == &RNA_WorkSpaceTool) {
      const bToolRef *tref = static_cast<bToolRef *>(ptr->data);
      return BKE_paintmode_get_from_tool(tref);
    }
    return C ? BKE_paintmode_get_active_from_context(C) : PaintMode::Invalid;
  }();

  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  EnumPropertyItem unset_item = {
      -1, "ANY", 0, "Any", "Do not limit this tool to a specific brush type"};
  RNA_enum_item_add(&items, &totitem, &unset_item);

  if (paint_mode != PaintMode::Invalid) {
    const EnumPropertyItem *valid_items = BKE_paint_get_tool_enum_from_paintmode(paint_mode);
    RNA_enum_items_add(&items, &totitem, valid_items);
  }

  RNA_enum_item_end(&items, &totitem);

  *r_free = true;
  return items;
}

static void rna_WorkSpaceTool_widget_get(PointerRNA *ptr, char *value)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  strcpy(value, tref->runtime ? tref->runtime->gizmo_group : "");
}

static int rna_WorkSpaceTool_widget_length(PointerRNA *ptr)
{
  bToolRef *tref = static_cast<bToolRef *>(ptr->data);
  return tref->runtime ? strlen(tref->runtime->gizmo_group) : 0;
}

static void rna_workspace_sync_scene_time_update(bContext *C, PointerRNA * /*ptr*/)
{
  blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);
}

#else /* RNA_RUNTIME */

static void rna_def_workspace_owner(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "wmOwnerID", nullptr);
  RNA_def_struct_sdna(srna, "wmOwnerID");
  RNA_def_struct_ui_text(srna, "Work Space UI Tag", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
}

static void rna_def_workspace_owner_ids(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "wmOwnerIDs");
  srna = RNA_def_struct(brna, "wmOwnerIDs", nullptr);
  RNA_def_struct_sdna(srna, "WorkSpace");
  RNA_def_struct_ui_text(srna, "WorkSpace UI Tags", "");

  /* add owner_id */
  func = RNA_def_function(srna, "new", "rna_WorkSpace_owner_ids_new");
  RNA_def_function_ui_description(func, "Add ui tag");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the tag");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "owner_id", "wmOwnerID", "", "");
  RNA_def_function_return(func, parm);

  /* remove owner_id */
  func = RNA_def_function(srna, "remove", "rna_WorkSpace_owner_ids_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove ui tag");
  /* owner_id to remove */
  parm = RNA_def_pointer(func, "owner_id", "wmOwnerID", "", "Tag to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* clear all modifiers */
  func = RNA_def_function(srna, "clear", "rna_WorkSpace_owner_ids_clear");
  RNA_def_function_ui_description(func, "Remove all tags");
}

static void rna_def_workspace_tool(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WorkSpaceTool", nullptr);
  RNA_def_struct_sdna(srna, "bToolRef");
  RNA_def_struct_ui_text(srna, "Work Space Tool", "");

  prop = RNA_def_property(srna, "idname", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Identifier", "");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "idname_fallback", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Identifier Fallback", "");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Index", "");
  RNA_def_property_int_funcs(prop, "rna_WorkSpaceTool_index_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "space_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "space_type");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  RNA_def_property_ui_text(prop, "Space Type", "");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_WorkSpace_tools_mode_itemf");
  RNA_def_property_ui_text(prop, "Tool Mode", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "use_paint_canvas", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Index", "");
  RNA_def_property_boolean_funcs(prop, "rna_WorkSpaceTool_use_paint_canvas_get", nullptr);
  RNA_def_property_ui_text(prop, "Use Paint Canvas", "Does this tool use a painting canvas");

  RNA_define_verify_sdna(false);

  prop = RNA_def_property(srna, "has_datablock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has Data-Block", "");
  RNA_def_property_boolean_funcs(prop, "rna_WorkSpaceTool_has_datablock_get", nullptr);

  prop = RNA_def_property(srna, "use_brushes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Uses Brushes", "");
  RNA_def_property_boolean_funcs(prop, "rna_WorkSpaceTool_use_brushes_get", nullptr);

  prop = RNA_def_property(srna, "brush_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Brush Type",
                           "If the tool uses brushes and is limited to a specific brush type, the "
                           "identifier of the brush type");
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(
      prop, "rna_WorkSpaceTool_brush_type_get", nullptr, "rna_WorkSpaceTool_brush_type_itemf");

  RNA_define_verify_sdna(true);

  prop = RNA_def_property(srna, "widget", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Widget", "");
  RNA_def_property_string_funcs(
      prop, "rna_WorkSpaceTool_widget_get", "rna_WorkSpaceTool_widget_length", nullptr);

  RNA_api_workspace_tool(srna);
}

static void rna_def_workspace_tools(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "wmTools");
  srna = RNA_def_struct(brna, "wmTools", nullptr);
  RNA_def_struct_sdna(srna, "WorkSpace");
  RNA_def_struct_ui_text(srna, "WorkSpace UI Tags", "");

  /* add owner_id */
  func = RNA_def_function(
      srna, "from_space_view3d_mode", "rna_WorkSpace_tools_from_space_view3d_mode");
  RNA_def_function_ui_description(func, "");
  parm = RNA_def_enum(func, "mode", rna_enum_context_mode_items, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "create", false, "Create", "");
  /* return type */
  parm = RNA_def_pointer(func, "result", "WorkSpaceTool", "", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(
      srna, "from_space_image_mode", "rna_WorkSpace_tools_from_space_image_mode");
  RNA_def_function_ui_description(func, "");
  parm = RNA_def_enum(func, "mode", rna_enum_space_image_mode_all_items, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "create", false, "Create", "");
  /* return type */
  parm = RNA_def_pointer(func, "result", "WorkSpaceTool", "", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "from_space_node", "rna_WorkSpace_tools_from_space_node");
  RNA_def_function_ui_description(func, "");
  RNA_def_boolean(func, "create", false, "Create", "");
  /* return type */
  parm = RNA_def_pointer(func, "result", "WorkSpaceTool", "", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(
      srna, "from_space_sequencer", "rna_WorkSpace_tools_from_space_sequencer");
  RNA_def_function_ui_description(func, "");
  parm = RNA_def_enum(func, "mode", rna_enum_space_sequencer_view_type_items, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "create", false, "Create", "");
  /* return type */
  parm = RNA_def_pointer(func, "result", "WorkSpaceTool", "", "");
  RNA_def_function_return(func, parm);
}

static void rna_def_workspace(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WorkSpace", "ID");
  RNA_def_struct_sdna(srna, "WorkSpace");
  RNA_def_struct_ui_text(
      srna, "Workspace", "Workspace data-block, defining the working environment for the user");
  /* TODO: real icon, just to show something */
  RNA_def_struct_ui_icon(srna, ICON_WORKSPACE);

  prop = RNA_def_property(srna, "screens", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "layouts", nullptr);
  RNA_def_property_struct_type(prop, "Screen");
  RNA_def_property_collection_funcs(prop,
                                    "rna_workspace_screens_begin",
                                    nullptr,
                                    nullptr,
                                    "rna_workspace_screens_item_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Screens", "Screen layouts of a workspace");

  prop = RNA_def_property(srna, "owner_ids", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "wmOwnerID");
  RNA_def_property_ui_text(prop, "UI Tags", "");
  rna_def_workspace_owner_ids(brna, prop);

  prop = RNA_def_property(srna, "tools", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "tools", nullptr);
  RNA_def_property_struct_type(prop, "WorkSpaceTool");
  RNA_def_property_ui_text(prop, "Tools", "");
  rna_def_workspace_tools(brna, prop);

  prop = RNA_def_property(srna, "object_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_workspace_object_mode_items);
  RNA_def_property_ui_text(
      prop, "Object Mode", "Switch to this object mode when activating the workspace");

  prop = RNA_def_property(srna, "use_pin_scene", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", WORKSPACE_USE_PIN_SCENE);
  RNA_def_property_ui_text(prop,
                           "Pin Scene",
                           "Remember the last used scene for the workspace and switch to it "
                           "whenever this workspace is activated again");
  RNA_def_property_update(prop, NC_WORKSPACE, nullptr);

  /* Flags */
  prop = RNA_def_property(srna, "use_filter_by_owner", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", WORKSPACE_USE_FILTER_BY_ORIGIN);
  RNA_def_property_ui_text(prop, "Use UI Tags", "Filter the UI by tags");
  RNA_def_property_update(prop, 0, "rna_window_update_all");

  prop = rna_def_asset_library_reference_common(
      srna, "rna_WorkSpace_asset_library_get", "rna_WorkSpace_asset_library_set");
  RNA_def_property_ui_text(prop,
                           "Asset Library",
                           "Active asset library to show in the UI, not used by the Asset Browser "
                           "(which has its own active asset library)");
  RNA_def_property_update(prop, NC_ASSET | ND_ASSET_LIST_READING, nullptr);

  prop = RNA_def_property(srna, "sequencer_scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "sequencer_scene");
  RNA_def_property_ui_text(prop, "Sequencer Scene", "");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_update(prop, 0, "rna_window_update_all");

  prop = RNA_def_property(srna, "use_scene_time_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", WORKSPACE_SYNC_SCENE_TIME);
  RNA_def_property_ui_text(
      prop, "Sync Active Scene", "Set the active scene and time based on the current scene strip");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_WINDOW, "rna_workspace_sync_scene_time_update");

  RNA_api_workspace(srna);
}

void RNA_def_workspace(BlenderRNA *brna)
{
  rna_def_workspace_owner(brna);
  rna_def_workspace_tool(brna);

  rna_def_workspace(brna);
}

#endif /* RNA_RUNTIME */
