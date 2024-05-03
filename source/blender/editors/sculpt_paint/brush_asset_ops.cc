/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_asset.hh"
#include "BKE_asset_edit.hh"
#include "BKE_blendfile.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_paint.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"

#include "AS_asset_catalog_path.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_asset_handle.hh"
#include "ED_asset_library.hh"
#include "ED_asset_list.hh"
#include "ED_asset_mark_clear.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_asset_shelf.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"

#include "paint_intern.hh"

namespace blender::ed::sculpt_paint {

static int brush_asset_select_exec(bContext *C, wmOperator *op)
{
  /* This operator currently covers both cases: the file/asset browser file list and the asset list
   * used for the asset-view template. Once the asset list design is used by the Asset Browser,
   * this can be simplified to just that case. */
  Main *bmain = CTX_data_main(C);
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(*C, *op->ptr, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }

  AssetWeakReference brush_asset_reference = asset->make_weak_reference();
  Brush *brush = reinterpret_cast<Brush *>(
      bke::asset_edit_id_from_weak_reference(*bmain, ID_BR, brush_asset_reference));

  Paint *paint = BKE_paint_get_active_from_context(C);

  if (!BKE_paint_brush_asset_set(paint, brush, brush_asset_reference)) {
    /* Note brush datablock was still added, so was not a no-op. */
    BKE_report(op->reports, RPT_WARNING, "Unable to select brush, wrong object mode");
    return OPERATOR_FINISHED;
  }

  WM_main_add_notifier(NC_ASSET | NA_ACTIVATED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
  WM_toolsystem_ref_set_by_id(C, "builtin.brush");

  return OPERATOR_FINISHED;
}

void BRUSH_OT_asset_select(wmOperatorType *ot)
{
  ot->name = "Select Brush Asset";
  ot->description = "Select a brush asset as current sculpt and paint tool";
  ot->idname = "BRUSH_OT_asset_select";

  ot->exec = brush_asset_select_exec;

  asset::operator_asset_reference_props_register(*ot->srna);
}

/* FIXME Quick dirty hack to generate a weak ref from 'raw' paths.
 * This needs to be properly implemented in assetlib code.
 */
static AssetWeakReference brush_asset_create_weakref_hack(const bUserAssetLibrary *user_asset_lib,
                                                          const std::string &file_path)
{
  AssetWeakReference asset_weak_ref{};

  StringRef asset_root_path = user_asset_lib->dirpath;
  BLI_assert(file_path.find(asset_root_path) == 0);
  std::string relative_asset_path = file_path.substr(size_t(asset_root_path.size()) + 1);

  asset_weak_ref.asset_library_type = ASSET_LIBRARY_CUSTOM;
  asset_weak_ref.asset_library_identifier = BLI_strdup(user_asset_lib->name);
  asset_weak_ref.relative_asset_identifier = BLI_strdupn(relative_asset_path.c_str(),
                                                         relative_asset_path.size());

  return asset_weak_ref;
}

static std::optional<AssetLibraryReference> library_to_library_ref(
    const asset_system::AssetLibrary &library)
{
  for (const AssetLibraryReference &ref : asset_system::all_valid_asset_library_refs()) {
    const std::string root_path = AS_asset_library_root_path_from_library_ref(ref);
    /* Use #BLI_path_cmp_normalized because `library.root_path()` ends with a slash while
     * `root_path` doesn't. */
    if (BLI_path_cmp_normalized(root_path.c_str(), library.root_path().c_str()) == 0) {
      return ref;
    }
  }
  return std::nullopt;
}

static AssetLibraryReference user_library_to_library_ref(const bUserAssetLibrary &user_library)
{
  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = BLI_findindex(&U.asset_libraries, &user_library);
  library_ref.type = ASSET_LIBRARY_CUSTOM;
  return library_ref;
}

static const bUserAssetLibrary *library_ref_to_user_library(
    const AssetLibraryReference &library_ref)
{
  if (library_ref.type != ASSET_LIBRARY_CUSTOM) {
    return nullptr;
  }
  return static_cast<const bUserAssetLibrary *>(
      BLI_findlink(&U.asset_libraries, library_ref.custom_library_index));
}

static void refresh_asset_library(const bContext *C, const AssetLibraryReference &library_ref)
{
  asset::list::clear(&library_ref, C);
  /* TODO: Should the all library reference be automatically cleared? */
  AssetLibraryReference all_lib_ref = asset_system::all_library_reference();
  asset::list::clear(&all_lib_ref, C);
}

static void refresh_asset_library(const bContext *C, const bUserAssetLibrary &user_library)
{
  refresh_asset_library(C, user_library_to_library_ref(user_library));
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

  if (BLI_listbase_is_empty(&U.asset_libraries)) {
    CTX_wm_operator_poll_msg_set(C, "No asset library available to save to");
    return false;
  }

  return true;
}

static const bUserAssetLibrary *get_asset_library_from_prop(PointerRNA &ptr)
{
  const int enum_value = RNA_enum_get(&ptr, "asset_library_reference");
  const AssetLibraryReference lib_ref = asset::library_reference_from_enum_value(enum_value);
  return BKE_preferences_asset_library_find_index(&U, lib_ref.custom_library_index);
}

static asset_system::AssetCatalog &asset_library_ensure_catalog(
    asset_system::AssetLibrary &library, const asset_system::AssetCatalogPath &path)
{
  if (asset_system::AssetCatalog *catalog = library.catalog_service().find_catalog_by_path(path)) {
    return *catalog;
  }
  return *library.catalog_service().create_catalog(path);
}

static asset_system::AssetCatalog &asset_library_ensure_catalogs_in_path(
    asset_system::AssetLibrary &library, const asset_system::AssetCatalogPath &path)
{
  /* Adding multiple catalogs in a path at a time with #AssetCatalogService::create_catalog()
   * doesn't work; add each potentially new catalog in the hierarchy manually here. */
  asset_system::AssetCatalogPath parent = "";
  path.iterate_components([&](StringRef component_name, bool /*is_last_component*/) {
    asset_library_ensure_catalog(library, parent / component_name);
    parent = parent / component_name;
  });
  return *library.catalog_service().find_catalog_by_path(path);
}

static void show_catalog_in_asset_shelf(const bContext &C, const StringRefNull catalog_path)
{
  /* Enable catalog in all visible asset shelves. */
  wmWindowManager *wm = CTX_wm_manager(&C);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      const AssetShelf *shelf = asset::shelf::active_shelf_from_area(area);
      if (shelf && BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
                       &U, shelf->idname, catalog_path.c_str()))
      {
        U.runtime.is_dirty = true;
      }
    }
  }
}

static int brush_asset_save_as_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;

  /* Determine file path to save to. */
  PropertyRNA *name_prop = RNA_struct_find_property(op->ptr, "name");
  char name[MAX_NAME] = "";
  if (RNA_property_is_set(op->ptr, name_prop)) {
    RNA_property_string_get(op->ptr, name_prop, name);
  }
  if (name[0] == '\0') {
    STRNCPY(name, brush->id.name + 2);
  }

  const bUserAssetLibrary *user_library = get_asset_library_from_prop(*op->ptr);
  if (!user_library) {
    return OPERATOR_CANCELLED;
  }

  asset_system::AssetLibrary *library = AS_asset_library_load(
      bmain, user_library_to_library_ref(*user_library));
  if (!library) {
    BKE_report(op->reports, RPT_ERROR, "Failed to load asset library");
    return OPERATOR_CANCELLED;
  }

  /* Turn brush into asset if it isn't yet. */
  if (!ID_IS_ASSET(&brush->id)) {
    asset::mark_id(&brush->id);
    asset::generate_preview(C, &brush->id);
  }
  BLI_assert(ID_IS_ASSET(&brush->id));

  /* Add asset to catalog. */
  char catalog_path[MAX_NAME];
  RNA_string_get(op->ptr, "catalog_path", catalog_path);

  AssetMetaData &meta_data = *brush->id.asset_data;
  if (catalog_path[0]) {
    const asset_system::AssetCatalog &catalog = asset_library_ensure_catalogs_in_path(
        *library, catalog_path);
    BKE_asset_metadata_catalog_id_set(&meta_data, catalog.catalog_id, catalog.simple_name.c_str());
  }

  const std::optional<std::string> final_full_asset_filepath = bke::asset_edit_id_save_as(
      *bmain, brush->id, name, *user_library, *op->reports);
  if (!final_full_asset_filepath) {
    return OPERATOR_CANCELLED;
  }

  library->catalog_service().write_to_disk(*final_full_asset_filepath);
  show_catalog_in_asset_shelf(*C, catalog_path);

  AssetWeakReference new_brush_weak_ref = brush_asset_create_weakref_hack(
      user_library, *final_full_asset_filepath);

  brush = reinterpret_cast<Brush *>(
      bke::asset_edit_id_from_weak_reference(*bmain, ID_BR, new_brush_weak_ref));

  if (!BKE_paint_brush_asset_set(paint, brush, new_brush_weak_ref)) {
    /* Note brush sset was still saved in editable asset library, so was not a no-op. */
    BKE_report(op->reports, RPT_WARNING, "Unable to activate just-saved brush asset");
  }

  refresh_asset_library(C, *user_library);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_ADDED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

static bool library_is_editable(const AssetLibraryReference &library)
{
  if (library.type == ASSET_LIBRARY_ESSENTIALS) {
    return false;
  }
  return true;
}

static int brush_asset_save_as_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  const AssetWeakReference &brush_weak_ref = *paint->brush_asset_reference;
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, brush_weak_ref, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }
  const asset_system::AssetLibrary &library = asset->owner_asset_library();
  const std::optional<AssetLibraryReference> library_ref = library_to_library_ref(library);
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
      const AssetLibraryReference first_library = user_library_to_library_ref(
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
  const EnumPropertyItem *items = asset::library_reference_to_rna_enum_itemf(false);
  if (!items) {
    *r_free = false;
    return nullptr;
  }

  *r_free = true;
  return items;
}

static void visit_library_catalogs_catalog_for_search(
    const Main &bmain,
    const bUserAssetLibrary &user_library,
    const StringRef edit_text,
    const FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  const asset_system::AssetLibrary *library = AS_asset_library_load(
      &bmain, user_library_to_library_ref(user_library));
  if (!library) {
    return;
  }

  if (!edit_text.is_empty()) {
    const asset_system::AssetCatalogPath edit_path = edit_text;
    if (!library->catalog_service().find_catalog_by_path(edit_path)) {
      visit_fn(StringPropertySearchVisitParams{edit_path.str(), std::nullopt, ICON_ADD});
    }
  }

  const asset_system::AssetCatalogTree &full_tree = library->catalog_service().catalog_tree();
  full_tree.foreach_item([&](const asset_system::AssetCatalogTreeItem &item) {
    visit_fn(StringPropertySearchVisitParams{item.catalog_path().str(), std::nullopt});
  });
}

static void visit_library_prop_catalogs_catalog_for_search_fn(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA * /*prop*/,
    const char *edit_text,
    FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  /* NOTE: Using the all library would also be a valid choice. */
  if (const bUserAssetLibrary *user_library = get_asset_library_from_prop(*ptr)) {
    visit_library_catalogs_catalog_for_search(
        *CTX_data_main(C), *user_library, edit_text, visit_fn);
  }
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

static int brush_asset_edit_metadata_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = (paint) ? BKE_paint_brush_for_read(paint) : nullptr;
  BLI_assert(ID_IS_ASSET(&brush->id));
  const AssetWeakReference &brush_weak_ref = *paint->brush_asset_reference;
  const asset_system::AssetRepresentation *asset = asset::find_asset_from_weak_ref(
      *C, brush_weak_ref, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }
  const asset_system::AssetLibrary &library_const = asset->owner_asset_library();
  const AssetLibraryReference library_ref = *library_to_library_ref(library_const);
  asset_system::AssetLibrary *library = AS_asset_library_load(bmain, library_ref);

  char catalog_path[MAX_NAME];
  RNA_string_get(op->ptr, "catalog_path", catalog_path);

  AssetMetaData &meta_data = *brush->id.asset_data;
  MEM_SAFE_FREE(meta_data.author);
  meta_data.author = RNA_string_get_alloc(op->ptr, "author", nullptr, 0, nullptr);
  MEM_SAFE_FREE(meta_data.description);
  meta_data.description = RNA_string_get_alloc(op->ptr, "description", nullptr, 0, nullptr);

  if (catalog_path[0]) {
    const asset_system::AssetCatalog &catalog = asset_library_ensure_catalogs_in_path(
        *library, catalog_path);
    BKE_asset_metadata_catalog_id_set(&meta_data, catalog.catalog_id, catalog.simple_name.c_str());
  }

  if (!bke::asset_edit_id_save(*bmain, brush->id, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  char asset_full_path_buffer[FILE_MAX_LIBEXTRA];
  char *file_path = nullptr;
  AS_asset_full_path_explode_from_weak_ref(
      &brush_weak_ref, asset_full_path_buffer, &file_path, nullptr, nullptr);
  if (!file_path) {
    BLI_assert_unreachable();
    return OPERATOR_CANCELLED;
  }

  library->catalog_service().write_to_disk(file_path);

  refresh_asset_library(C, library_ref);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static int brush_asset_edit_metadata_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
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
  visit_library_catalogs_catalog_for_search(
      *CTX_data_main(C),
      *library_ref_to_user_library(*library_to_library_ref(library)),
      edit_text,
      visit_fn);
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
      *C, *brush_weak_ref, nullptr);
  if (!asset) {
    BLI_assert_unreachable();
    return false;
  }
  const std::optional<AssetLibraryReference> library_ref = library_to_library_ref(
      asset->owner_asset_library());
  if (!library_ref) {
    BLI_assert_unreachable();
    return false;
  }
  if (!library_is_editable(*library_ref)) {
    CTX_wm_operator_poll_msg_set(C, "Asset library is not editable");
    return false;
  }
  if (!bke::asset_edit_id_is_editable(brush->id)) {
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
  RNA_def_string(ot->srna, "author", nullptr, MAX_NAME, "Author", "");
  RNA_def_string(ot->srna, "description", nullptr, MAX_NAME, "Description", "");
}

static bool brush_asset_delete_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }
  if (!paint->brush_asset_reference) {
    return false;
  }

  /* Asset brush, check if belongs to an editable blend file. */
  if (ID_IS_ASSET(brush)) {
    if (!bke::asset_edit_id_is_editable(brush->id)) {
      CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
      return false;
    }
  }

  return true;
}

static int brush_asset_delete_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  Main *bmain = CTX_data_main(C);

  bUserAssetLibrary *library = BKE_preferences_asset_library_find_by_name(
      &U, paint->brush_asset_reference->asset_library_identifier);
  if (!library) {
    return OPERATOR_CANCELLED;
  }

  bke::asset_edit_id_delete(*bmain, brush->id, *op->reports);

  refresh_asset_library(C, *library);

  BKE_paint_brush_set_default(bmain, paint);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_REMOVED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static int brush_asset_delete_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_confirm_ex(
      C,
      op,
      IFACE_("Delete Brush Asset"),
      IFACE_("Permanently delete brush asset blend file. This can't be undone."),
      IFACE_("Delete"),
      ALERT_ICON_WARNING,
      false);
}

void BRUSH_OT_asset_delete(wmOperatorType *ot)
{
  ot->name = "Delete Brush Asset";
  ot->description = "Delete the active brush asset both from the local session and asset library";
  ot->idname = "BRUSH_OT_asset_delete";

  ot->exec = brush_asset_delete_exec;
  ot->invoke = brush_asset_delete_invoke;
  ot->poll = brush_asset_delete_poll;
}

static bool brush_asset_update_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  if ((brush->id.tag & LIB_TAG_ASSET_MAIN) == 0) {
    return false;
  }

  if (!(paint->brush_asset_reference && ID_IS_ASSET(brush))) {
    return false;
  }

  if (!bke::asset_edit_id_is_editable(brush->id)) {
    CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
    return false;
  }

  return true;
}

static int brush_asset_update_exec(bContext *C, wmOperator *op)
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

  refresh_asset_library(C, *user_library);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

void BRUSH_OT_asset_update(wmOperatorType *ot)
{
  ot->name = "Update Brush Asset";
  ot->description = "Update the active brush asset in the asset library with current settings";
  ot->idname = "BRUSH_OT_asset_update";

  ot->exec = brush_asset_update_exec;
  ot->poll = brush_asset_update_poll;
}

static bool brush_asset_revert_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  return paint->brush_asset_reference && (brush->id.tag & LIB_TAG_ASSET_MAIN);
}

static int brush_asset_revert_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  bke::asset_edit_id_revert(*bmain, brush->id, *op->reports);

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
