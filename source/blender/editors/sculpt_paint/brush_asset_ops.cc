/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_asset.hh"
#include "BKE_asset_edit.hh"
#include "BKE_blendfile.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_preferences.h"
#include "BKE_preview_image.hh"
#include "BKE_report.hh"

#include "AS_asset_catalog_path.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_asset.hh"
#include "ED_asset_library.hh"
#include "ED_asset_list.hh"
#include "ED_asset_mark_clear.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_asset_shelf.hh"

#include "UI_interface_icons.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"

#include "paint_intern.hh"

namespace blender::ed::sculpt_paint {

static wmOperatorStatus brush_asset_activate_exec(bContext *C, wmOperator *op)
{
  /* This operator currently covers both cases: the file/asset browser file list and the asset list
   * used for the asset-view template. Once the asset list design is used by the Asset Browser,
   * this can be simplified to just that case. */
  Main *bmain = CTX_data_main(C);

  if (G.background) {
    /* As asset loading can take upwards of a few minutes on production libraries, we typically
     * do not want this to execute in a blocking fashion. However, for testing / profiling
     * purposes, this is an acceptable workaround for now until a proper python API is created
     * for this use case. */
    asset::list::storage_fetch_blocking(asset_system::all_library_reference(), *C);
  }
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(*C, *op->ptr, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }

  const bool use_toggle = RNA_boolean_get(op->ptr, "use_toggle");
  AssetWeakReference brush_asset_reference = asset->make_weak_reference();
  Paint *paint = BKE_paint_get_active_from_context(C);
  std::optional<AssetWeakReference> asset_to_save;
  if (use_toggle) {
    BLI_assert(paint->brush_asset_reference);
    if (brush_asset_reference == *paint->brush_asset_reference) {
      if (paint->runtime->previous_active_brush_reference != nullptr) {
        brush_asset_reference = *paint->runtime->previous_active_brush_reference;
      }
    }
    else {
      asset_to_save = *paint->brush_asset_reference;
    }
  }
  Brush *brush = reinterpret_cast<Brush *>(
      bke::asset_edit_id_from_weak_reference(*bmain, ID_BR, brush_asset_reference));

  /* Activate brush through tool system rather than calling #BKE_paint_brush_set() directly, to let
   * the tool system switch tools if necessary, and update which brush was the last recently used
   * one for the current tool. */
  if (!WM_toolsystem_activate_brush_and_tool(C, paint, brush)) {
    /* Note brush datablock was still added, so was not a no-op. */
    BKE_report(op->reports, RPT_WARNING, "Unable to activate brush, wrong object mode");
    return OPERATOR_FINISHED;
  }

  if (asset_to_save) {
    BKE_paint_previous_asset_reference_set(paint, std::move(*asset_to_save));
  }
  else if (!use_toggle) {
    /* If we aren't toggling, clear the previous reference so that we don't swap back to an
     * incorrect "previous" asset */
    BKE_paint_previous_asset_reference_clear(paint);
  }

  WM_main_add_notifier(NC_ASSET | NA_ACTIVATED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

  return OPERATOR_FINISHED;
}

void BRUSH_OT_asset_activate(wmOperatorType *ot)
{
  ot->name = "Activate Brush Asset";
  ot->description = "Activate a brush asset as current sculpt and paint tool";
  ot->idname = "BRUSH_OT_asset_activate";

  ot->exec = brush_asset_activate_exec;

  asset::operator_asset_reference_props_register(*ot->srna);
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "use_toggle",
                         false,
                         "Toggle",
                         "Switch between the current and assigned brushes on consecutive uses.");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
}

static bool brush_asset_save_as_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }
  if (!paint->brush_asset_reference) {
    /* The brush should always be an imported asset. We use this asset reference to find
     * which library and catalog the brush came from, as defaults for the popup. */
    return false;
  }

  return true;
}

static wmOperatorStatus brush_asset_save_as_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;

  /* Determine file path to save to. */
  PropertyRNA *name_prop = RNA_struct_find_property(op->ptr, "name");
  /* FIXME: MAX_ID_NAME & FILE_MAXFILE
   *
   * This `name` should be `MAX_ID_NAME - 2` long.
   *
   * This name might also be used as filename for the saved asset, thus hitting the size issue
   * between ID names and file names (FILE_MAXFILE). */
  char name[MAX_NAME] = "";
  if (RNA_property_is_set(op->ptr, name_prop)) {
    RNA_property_string_get(op->ptr, name_prop, name);
  }
  if (name[0] == '\0') {
    STRNCPY_UTF8(name, brush->id.name + 2);
  }

  const eAssetLibraryType enum_value = (eAssetLibraryType)RNA_enum_get(op->ptr,
                                                                       "asset_library_reference");
  const bool is_local_library = enum_value == ASSET_LIBRARY_LOCAL;

  AssetLibraryReference library_reference;
  const bUserAssetLibrary *user_library = nullptr;
  if (is_local_library) {
    library_reference = asset_system::current_file_library_reference();
  }
  else {
    user_library = asset::get_asset_library_from_opptr(*op->ptr);
    if (!user_library) {
      return OPERATOR_CANCELLED;
    }
    library_reference = asset::user_library_to_library_ref(*user_library);
  }
  asset_system::AssetLibrary *library = AS_asset_library_load(bmain, library_reference);
  if (!library) {
    BKE_report(op->reports, RPT_ERROR, "Failed to load asset library");
    return OPERATOR_CANCELLED;
  }

  BLI_assert(is_local_library || user_library);

  /* Turn brush into asset if it isn't yet. */
  if (!ID_IS_ASSET(&brush->id)) {
    asset::mark_id(&brush->id);
    asset::generate_preview(C, &brush->id);
  }
  BLI_assert(ID_IS_ASSET(&brush->id));

  if (is_local_library) {
    const Brush *original_brush = brush;
    brush = BKE_brush_duplicate(
        bmain, brush, USER_DUP_OBDATA | USER_DUP_LINKED_ID, LIB_ID_DUPLICATE_IS_ROOT_ID);

    BKE_libblock_rename(*bmain, brush->id, name);
    asset::mark_id(&brush->id);
    BLI_assert(brush->id.us == 1);

    BKE_asset_metadata_free(&brush->id.asset_data);
    brush->id.asset_data = BKE_asset_metadata_copy(original_brush->id.asset_data);
    BLI_assert(brush->id.asset_data != nullptr);
  }

  /* Add asset to catalog. */
  /* Note: This needs to happen after the local asset is created but BEFORE a non-local library
   * is saved */
  char catalog_path_c[MAX_NAME];
  RNA_string_get(op->ptr, "catalog_path", catalog_path_c);

  AssetMetaData &meta_data = *brush->id.asset_data;
  if (catalog_path_c[0]) {
    const asset_system::AssetCatalogPath catalog_path(catalog_path_c);
    const asset_system::AssetCatalog &catalog = asset::library_ensure_catalogs_in_path(
        *library, catalog_path);
    BKE_asset_metadata_catalog_id_set(&meta_data, catalog.catalog_id, catalog.simple_name.c_str());
  }

  if (!is_local_library) {
    AssetWeakReference brush_asset_reference;
    const std::optional<std::string> final_full_asset_filepath = bke::asset_edit_id_save_as(
        *bmain, brush->id, name, *user_library, brush_asset_reference, *op->reports);
    if (!final_full_asset_filepath) {
      return OPERATOR_CANCELLED;
    }
    library->catalog_service().write_to_disk(*final_full_asset_filepath);

    brush = reinterpret_cast<Brush *>(
        bke::asset_edit_id_from_weak_reference(*bmain, ID_BR, brush_asset_reference));
    brush->has_unsaved_changes = false;
  }

  asset::shelf::show_catalog_in_visible_shelves(*C, catalog_path_c);

  if (!WM_toolsystem_activate_brush_and_tool(C, paint, brush)) {
    /* Note brush asset was still saved in editable asset library, so was not a no-op. */
    BKE_report(op->reports, RPT_WARNING, "Unable to activate just-saved brush asset");
  }

  asset::refresh_asset_library(C, library_reference);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_ADDED, nullptr);
  if (is_local_library) {
    WM_main_add_notifier(NC_BRUSH | NA_ADDED, brush);
    WM_file_tag_modified();
  }
  else {
    WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);
  }

  return OPERATOR_FINISHED;
}

static bool library_is_editable(const AssetLibraryReference &library)
{
  if (library.type == ASSET_LIBRARY_ESSENTIALS) {
    return false;
  }
  return true;
}

static wmOperatorStatus brush_asset_save_as_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  const AssetWeakReference &brush_weak_ref = *paint->brush_asset_reference;
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, brush_weak_ref, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }
  const asset_system::AssetLibrary &library = asset->owner_asset_library();
  const std::optional<AssetLibraryReference> library_ref = library.library_reference();
  if (!library_ref) {
    BLI_assert_unreachable();
    return OPERATOR_CANCELLED;
  }

  RNA_string_set(op->ptr, "name", asset->get_name().c_str());

  /* If the library isn't saved from the operator's last execution, find the current library or the
   * first library if the current library isn't editable. */
  if (!RNA_struct_property_is_set_ex(op->ptr, "asset_library_reference", false)) {
    if (library_is_editable(*library_ref)) {
      RNA_enum_set(op->ptr,
                   "asset_library_reference",
                   asset::library_reference_to_enum_value(&*library_ref));
    }
    else {
      const AssetLibraryReference first_library = asset::user_library_to_library_ref(
          *static_cast<const bUserAssetLibrary *>(U.asset_libraries.first));
      RNA_enum_set(op->ptr,
                   "asset_library_reference",
                   asset::library_reference_to_enum_value(&first_library));
    }
  }

  /* By default, put the new asset in the same catalog as the existing asset. */
  if (!RNA_struct_property_is_set(op->ptr, "catalog_path")) {
    const asset_system::CatalogID &id = asset->get_metadata().catalog_id;
    if (const asset_system::AssetCatalog *catalog = library.catalog_service().find_catalog(id)) {
      RNA_string_set(op->ptr, "catalog_path", catalog->path.c_str());
    }
  }

  return WM_operator_props_dialog_popup(C, op, 400, std::nullopt, IFACE_("Save"));
}

static const EnumPropertyItem *rna_asset_library_reference_itemf(bContext * /*C*/,
                                                                 PointerRNA * /*ptr*/,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool *r_free)
{
  const EnumPropertyItem *items = asset::library_reference_to_rna_enum_itemf(
      /* Only get writable libraries. */
      /*include_readonly=*/false,
      /*include_current_file=*/true);
  if (!items) {
    *r_free = false;
    return nullptr;
  }

  *r_free = true;
  return items;
}

static void visit_library_prop_catalogs_catalog_for_search_fn(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA * /*prop*/,
    const char *edit_text,
    FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  /* NOTE: Using the all library would also be a valid choice. */
  asset::visit_library_catalogs_catalog_for_search(
      *CTX_data_main(C), asset::get_asset_library_ref_from_opptr(*ptr), edit_text, visit_fn);
}

void BRUSH_OT_asset_save_as(wmOperatorType *ot)
{
  ot->name = "Save as Brush Asset";
  ot->description =
      "Save a copy of the active brush asset into the default asset library, and make it the "
      "active brush";
  ot->idname = "BRUSH_OT_asset_save_as";

  ot->exec = brush_asset_save_as_exec;
  ot->invoke = brush_asset_save_as_invoke;
  ot->poll = brush_asset_save_as_poll;

  ot->prop = RNA_def_string(
      ot->srna, "name", nullptr, MAX_NAME, "Name", "Name for the new brush asset");

  PropertyRNA *prop = RNA_def_property(ot->srna, "asset_library_reference", PROP_ENUM, PROP_NONE);
  RNA_def_enum_funcs(prop, rna_asset_library_reference_itemf);
  RNA_def_property_ui_text(prop, "Library", "Asset library used to store the new brush");

  prop = RNA_def_string(
      ot->srna, "catalog_path", nullptr, MAX_NAME, "Catalog", "Catalog to use for the new asset");
  RNA_def_property_string_search_func_runtime(
      prop, visit_library_prop_catalogs_catalog_for_search_fn, PROP_STRING_SEARCH_SUGGESTION);
}

static wmOperatorStatus brush_asset_edit_metadata_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  BLI_assert(ID_IS_ASSET(&brush->id));
  const AssetWeakReference &brush_weak_ref = *paint->brush_asset_reference;
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, brush_weak_ref, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }
  asset_system::AssetLibrary &library = asset->owner_asset_library();

  char catalog_path_c[MAX_NAME];
  RNA_string_get(op->ptr, "catalog_path", catalog_path_c);

  AssetMetaData &meta_data = *brush->id.asset_data;
  MEM_SAFE_FREE(meta_data.author);
  meta_data.author = RNA_string_get_alloc(op->ptr, "author", nullptr, 0, nullptr);
  MEM_SAFE_FREE(meta_data.description);
  meta_data.description = RNA_string_get_alloc(op->ptr, "description", nullptr, 0, nullptr);

  if (catalog_path_c[0]) {
    const asset_system::AssetCatalogPath catalog_path(catalog_path_c);
    const asset_system::AssetCatalog &catalog = asset::library_ensure_catalogs_in_path(
        library, catalog_path);
    BKE_asset_metadata_catalog_id_set(&meta_data, catalog.catalog_id, catalog.simple_name.c_str());
  }

  if (!bke::asset_edit_id_save(*bmain, brush->id, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  asset::catalogs_save_from_asset_reference(library, brush_weak_ref);

  asset::refresh_asset_library_from_asset(C, *asset);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus brush_asset_edit_metadata_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent * /*event*/)
{
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const AssetWeakReference &brush_weak_ref = *paint->brush_asset_reference;
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, brush_weak_ref, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }
  const asset_system::AssetLibrary &library = asset->owner_asset_library();
  const AssetMetaData &meta_data = asset->get_metadata();

  if (!RNA_struct_property_is_set(op->ptr, "catalog_path")) {
    const asset_system::CatalogID &id = meta_data.catalog_id;
    if (const asset_system::AssetCatalog *catalog = library.catalog_service().find_catalog(id)) {
      RNA_string_set(op->ptr, "catalog_path", catalog->path.c_str());
    }
  }
  if (!RNA_struct_property_is_set(op->ptr, "author")) {
    RNA_string_set(op->ptr, "author", meta_data.author ? meta_data.author : "");
  }
  if (!RNA_struct_property_is_set(op->ptr, "description")) {
    RNA_string_set(op->ptr, "description", meta_data.description ? meta_data.description : "");
  }

  return WM_operator_props_dialog_popup(C, op, 400, std::nullopt, IFACE_("Edit Metadata"));
}

static void visit_active_library_catalogs_catalog_for_search_fn(
    const bContext *C,
    PointerRNA * /*ptr*/,
    PropertyRNA * /*prop*/,
    const char *edit_text,
    FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const AssetWeakReference &brush_weak_ref = *paint->brush_asset_reference;
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, brush_weak_ref, nullptr);
  if (!asset) {
    return;
  }

  const asset_system::AssetLibrary &library = asset->owner_asset_library();

  /* NOTE: Using the all library would also be a valid choice. */
  asset::visit_library_catalogs_catalog_for_search(
      *CTX_data_main(C), *library.library_reference(), edit_text, visit_fn);
}

static bool brush_asset_edit_metadata_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }
  if (!ID_IS_ASSET(&brush->id)) {
    BLI_assert_unreachable();
    return false;
  }
  const AssetWeakReference *brush_weak_ref = paint->brush_asset_reference;
  if (!brush_weak_ref) {
    BLI_assert_unreachable();
    return false;
  }
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, *brush_weak_ref, CTX_wm_reports(C));
  if (!asset) {
    /* May happen if library loading hasn't finished. */
    return false;
  }
  const std::optional<AssetLibraryReference> library_ref =
      asset->owner_asset_library().library_reference();
  if (!library_ref) {
    BLI_assert_unreachable();
    return false;
  }
  if (!library_is_editable(*library_ref)) {
    CTX_wm_operator_poll_msg_set(C, "Asset library is not editable");
    return false;
  }
  if (!(library_ref->type & ASSET_LIBRARY_LOCAL) && !bke::asset_edit_id_is_writable(brush->id)) {
    CTX_wm_operator_poll_msg_set(C, "Asset file is not editable");
    return false;
  }
  return true;
}

void BRUSH_OT_asset_edit_metadata(wmOperatorType *ot)
{
  ot->name = "Edit Metadata";
  ot->description = "Edit asset information like the catalog, preview image, tags, or author";
  ot->idname = "BRUSH_OT_asset_edit_metadata";

  ot->exec = brush_asset_edit_metadata_exec;
  ot->invoke = brush_asset_edit_metadata_invoke;
  ot->poll = brush_asset_edit_metadata_poll;

  PropertyRNA *prop = RNA_def_string(
      ot->srna, "catalog_path", nullptr, MAX_NAME, "Catalog", "The asset's catalog path");
  RNA_def_property_string_search_func_runtime(
      prop, visit_active_library_catalogs_catalog_for_search_fn, PROP_STRING_SEARCH_SUGGESTION);
  RNA_def_string(ot->srna, "author", nullptr, 0, "Author", "");
  RNA_def_string(ot->srna, "description", nullptr, 0, "Description", "");
}

static wmOperatorStatus brush_asset_load_preview_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  BLI_assert(ID_IS_ASSET(&brush->id));
  const AssetWeakReference &brush_weak_ref = *paint->brush_asset_reference;
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, brush_weak_ref, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (!BLI_is_file(filepath)) {
    BKE_reportf(op->reports, RPT_ERROR, "File not found '%s'", filepath);
    return OPERATOR_CANCELLED;
  }

  BKE_previewimg_id_custom_set(&brush->id, filepath);

  if (!bke::asset_edit_id_save(*bmain, brush->id, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  asset::refresh_asset_library_from_asset(C, *asset);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus brush_asset_load_preview_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return brush_asset_load_preview_exec(C, op);
  }
  return WM_operator_filesel(C, op, event);
}

void BRUSH_OT_asset_load_preview(wmOperatorType *ot)
{
  ot->name = "Load Preview Image";
  ot->description = "Choose a preview image for the brush";
  ot->idname = "BRUSH_OT_asset_load_preview";

  ot->exec = brush_asset_load_preview_exec;
  ot->invoke = brush_asset_load_preview_invoke;
  ot->poll = brush_asset_edit_metadata_poll;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

static bool brush_asset_delete_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  /* Linked brush, check if belongs to an editable blend file. */
  if (ID_IS_LINKED(brush)) {
    if (!bke::asset_edit_id_is_writable(brush->id)) {
      CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
      return false;
    }
  }

  return true;
}

static wmOperatorStatus brush_asset_delete_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  Main *bmain = CTX_data_main(C);
  bUserAssetLibrary *library = (paint->brush_asset_reference) ?
                                   BKE_preferences_asset_library_find_by_name(
                                       &U,
                                       paint->brush_asset_reference->asset_library_identifier) :
                                   nullptr;

  bke::asset_edit_id_delete(*bmain, brush->id, *op->reports);

  BKE_paint_brush_set_default(bmain, paint);

  if (library) {
    asset::refresh_asset_library(C, *library);
  }

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_REMOVED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus brush_asset_delete_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  return WM_operator_confirm_ex(
      C,
      op,
      IFACE_("Delete Brush Asset"),
      ID_IS_LINKED(brush) ?
          IFACE_("Permanently delete brush asset blend file. This cannot be undone.") :
          IFACE_("Permanently delete brush. This cannot be undone."),
      IFACE_("Delete"),
      ALERT_ICON_WARNING,
      false);
}

void BRUSH_OT_asset_delete(wmOperatorType *ot)
{
  ot->name = "Delete Brush Asset";
  ot->description = "Delete the active brush asset";
  ot->idname = "BRUSH_OT_asset_delete";

  ot->exec = brush_asset_delete_exec;
  ot->invoke = brush_asset_delete_invoke;
  ot->poll = brush_asset_delete_poll;
}

static std::optional<AssetLibraryReference> get_asset_library_reference(const bContext &C,
                                                                        const Paint &paint,
                                                                        const Brush &brush)
{
  if (!ID_IS_ASSET(&brush.id)) {
    BLI_assert_unreachable();
    return std::nullopt;
  }
  const AssetWeakReference *brush_weak_ref = paint.brush_asset_reference;
  if (!brush_weak_ref) {
    BLI_assert_unreachable();
    return std::nullopt;
  }
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      C, *brush_weak_ref, CTX_wm_reports(&C));
  if (!asset) {
    /* May happen if library loading hasn't finished. */
    return std::nullopt;
  }
  return asset->owner_asset_library().library_reference();
}

static bool brush_asset_save_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  const std::optional<AssetLibraryReference> library_ref = get_asset_library_reference(
      *C, *paint, *brush);
  if (!library_ref) {
    BLI_assert_unreachable();
    return false;
  }

  if (library_ref->type == ASSET_LIBRARY_LOCAL) {
    CTX_wm_operator_poll_msg_set(C, "Assets in the current file cannot be individually saved");
    return false;
  }

  if (!bke::asset_edit_id_is_writable(brush->id)) {
    CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
    return false;
  }

  return true;
}

static wmOperatorStatus brush_asset_save_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  const AssetWeakReference *asset_weak_ref = paint->brush_asset_reference;

  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_by_name(
      &U, asset_weak_ref->asset_library_identifier);
  if (!user_library) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ID_IS_ASSET(brush));

  bke::asset_edit_id_save(*bmain, brush->id, *op->reports);
  brush->has_unsaved_changes = false;

  asset::refresh_asset_library(C, *user_library);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

void BRUSH_OT_asset_save(wmOperatorType *ot)
{
  ot->name = "Save Brush Asset";
  ot->description = "Update the active brush asset in the asset library with current settings";
  ot->idname = "BRUSH_OT_asset_save";

  ot->exec = brush_asset_save_exec;
  ot->poll = brush_asset_save_poll;
}

static bool brush_asset_revert_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  const std::optional<AssetLibraryReference> library_ref = get_asset_library_reference(
      *C, *paint, *brush);
  if (!library_ref) {
    BLI_assert_unreachable();
    return false;
  }
  if (library_ref->type == ASSET_LIBRARY_LOCAL) {
    CTX_wm_operator_poll_msg_set(C, "Assets in the current file cannot be reverted");
    return false;
  }

  return bke::asset_edit_id_is_editable(brush->id);
}

static wmOperatorStatus brush_asset_revert_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  if (ID *reverted_id = bke::asset_edit_id_revert(*bmain, brush->id, *op->reports)) {
    BLI_assert(GS(reverted_id->name) == ID_BR);
    BKE_paint_brush_set(paint, reinterpret_cast<Brush *>(reverted_id));
  }
  else {
    /* bke::asset_edit_id_revert() deleted the brush for sure, even on failure. Fall back to the
     * default. */
    BKE_paint_brush_set_default(bmain, paint);
  }

  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_TEXTURE | ND_NODES, nullptr);

  return OPERATOR_FINISHED;
}

void BRUSH_OT_asset_revert(wmOperatorType *ot)
{
  ot->name = "Revert Brush Asset";
  ot->description =
      "Revert the active brush settings to the default values from the asset library";
  ot->idname = "BRUSH_OT_asset_revert";

  ot->exec = brush_asset_revert_exec;
  ot->poll = brush_asset_revert_poll;
}

}  // namespace blender::ed::sculpt_paint
