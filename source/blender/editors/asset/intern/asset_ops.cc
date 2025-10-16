/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_asset_edit.hh"
#include "BKE_blendfile.hh"
#include "BKE_bpath.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"
#include "BKE_preview_image.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLI_fnmatch.h"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_set.hh"
#include "BLI_string.h"

#include "ED_asset.hh"
#include "ED_screen.hh"
/* XXX needs access to the file list, should all be done via the asset system in future. */
#include "ED_fileselect.hh"
#include "ED_render.hh"
#include "ED_util.hh"
#include "ED_view3d_offscreen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "IMB_imbuf.hh"
#include "IMB_thumbs.hh"

#include "WM_api.hh"

#include "DNA_space_types.h"

#include "GPU_immediate.hh"

#include "UI_interface_c.hh"
#include "UI_resources.hh"

namespace blender::ed::asset {
/* -------------------------------------------------------------------- */

/**
 * Information about what's contained in a #Vector<PointerRNA>, returned by
 * #asset_operation_get_id_vec_stats_from_context().
 */
struct IDVecStats {
  bool has_asset = false;
  bool has_supported_type = false;
  bool is_single = false;
};

/**
 * Helper to report stats about the IDs in context. Operator polls use this, also to report a
 * helpful disabled hint to the user.
 */
static IDVecStats asset_operation_get_id_vec_stats_from_ids(const Span<PointerRNA> id_pointers)
{
  IDVecStats stats;

  stats.is_single = id_pointers.size() == 1;

  for (const PointerRNA &ptr : id_pointers) {
    BLI_assert(RNA_struct_is_ID(ptr.type));

    ID *id = static_cast<ID *>(ptr.data);
    if (id_type_is_supported(id)) {
      stats.has_supported_type = true;
    }
    if (ID_IS_ASSET(id)) {
      stats.has_asset = true;
    }
  }

  return stats;
}

static const char *asset_operation_unsupported_type_msg(const bool is_single)
{
  const char *msg_single =
      "Data-block does not support asset operations - must be "
      "a " ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_UI_STRING;
  const char *msg_multiple =
      "No data-block selected that supports asset operations - select at least "
      "one " ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_UI_STRING;
  return is_single ? msg_single : msg_multiple;
}

/* -------------------------------------------------------------------- */

class AssetMarkHelper {
 public:
  void operator()(const bContext &C, Span<PointerRNA> ids);

  void reportResults(ReportList &reports) const;
  bool wasSuccessful() const;

 private:
  struct Stats {
    int tot_created = 0;
    int tot_already_asset = 0;
    ID *last_id = nullptr;
  };

  Stats stats;
};

void AssetMarkHelper::operator()(const bContext &C, const Span<PointerRNA> ids)
{
  for (const PointerRNA &ptr : ids) {
    BLI_assert(RNA_struct_is_ID(ptr.type));

    ID *id = static_cast<ID *>(ptr.data);
    if (id->asset_data) {
      stats.tot_already_asset++;
      continue;
    }

    if (mark_id(id)) {
      generate_preview(&C, id);

      stats.last_id = id;
      stats.tot_created++;
    }
  }
}

bool AssetMarkHelper::wasSuccessful() const
{
  return stats.tot_created > 0;
}

void AssetMarkHelper::reportResults(ReportList &reports) const
{
  /* User feedback on failure. */
  if (!wasSuccessful()) {
    if (stats.tot_already_asset > 0) {
      BKE_report(&reports,
                 RPT_ERROR,
                 "Selected data-blocks are already assets (or do not support use as assets)");
    }
    else {
      BKE_report(&reports,
                 RPT_ERROR,
                 "No data-blocks to create assets for found (or do not support use as assets)");
    }
  }
  /* User feedback on success. */
  else if (stats.tot_created == 1) {
    /* If only one data-block: Give more useful message by printing asset name. */
    BKE_reportf(&reports, RPT_INFO, "Data-block '%s' is now an asset", stats.last_id->name + 2);
  }
  else {
    BKE_reportf(&reports, RPT_INFO, "%i data-blocks are now assets", stats.tot_created);
  }
}

static wmOperatorStatus asset_mark_exec(const bContext *C,
                                        const wmOperator *op,
                                        const Span<PointerRNA> ids)
{
  AssetMarkHelper mark_helper;
  mark_helper(*C, ids);
  mark_helper.reportResults(*op->reports);

  if (!mark_helper.wasSuccessful()) {
    return OPERATOR_CANCELLED;
  }

  WM_main_add_notifier(NC_ID | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_ASSET | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

static bool asset_mark_poll(bContext *C, const Span<PointerRNA> ids)
{
  IDVecStats ctx_stats = asset_operation_get_id_vec_stats_from_ids(ids);

  if (!ctx_stats.has_supported_type) {
    CTX_wm_operator_poll_msg_set(C, asset_operation_unsupported_type_msg(ctx_stats.is_single));
    return false;
  }

  return true;
}

static void ASSET_OT_mark(wmOperatorType *ot)
{
  ot->name = "Mark as Asset";
  ot->description =
      "Enable easier reuse of selected data-blocks through the Asset Browser, with the help of "
      "customizable metadata (like previews, descriptions and tags)";
  ot->idname = "ASSET_OT_mark";

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    return asset_mark_exec(C, op, ED_operator_get_ids_from_context_as_vec(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_mark_poll(C, ED_operator_get_ids_from_context_as_vec(C));
  };

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**
 * Variant of #ASSET_OT_mark that only works on the "id" context member.
 */
static void ASSET_OT_mark_single(wmOperatorType *ot)
{
  ot->name = "Mark as Single Asset";
  ot->description =
      "Enable easier reuse of a data-block through the Asset Browser, with the help of "
      "customizable metadata (like previews, descriptions and tags)";
  ot->idname = "ASSET_OT_mark_single";

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    return asset_mark_exec(C, op, ED_operator_single_id_from_context_as_vec(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_mark_poll(C, ED_operator_single_id_from_context_as_vec(C));
  };

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------------------------------------- */

class AssetClearHelper {
  const bool set_fake_user_;

 public:
  AssetClearHelper(const bool set_fake_user) : set_fake_user_(set_fake_user) {}

  void operator()(Span<PointerRNA> ids);

  void reportResults(const bContext *C, ReportList &reports) const;
  bool wasSuccessful() const;

 private:
  struct Stats {
    int tot_cleared = 0;
    ID *last_id = nullptr;
  };

  Stats stats;
};

void AssetClearHelper::operator()(const Span<PointerRNA> ids)
{
  for (const PointerRNA &ptr : ids) {
    BLI_assert(RNA_struct_is_ID(ptr.type));

    ID *id = static_cast<ID *>(ptr.data);
    if (!id->asset_data) {
      continue;
    }

    if (!clear_id(id)) {
      continue;
    }

    if (set_fake_user_) {
      id_fake_user_set(id);
    }

    stats.tot_cleared++;
    stats.last_id = id;
  }
}

void AssetClearHelper::reportResults(const bContext *C, ReportList &reports) const
{
  if (!wasSuccessful()) {
    /* Dedicated error message for when there is an active asset detected, but it's not an ID local
     * to this file. Helps users better understanding what's going on. */
    if (AssetRepresentationHandle *active_asset = CTX_wm_asset(C); !active_asset->is_local_id()) {
      BKE_report(&reports,
                 RPT_ERROR,
                 "No asset data-blocks from the current file selected (assets must be stored in "
                 "the current file to be able to edit or clear them)");
    }
    else {
      BKE_report(&reports, RPT_ERROR, "No asset data-blocks selected/focused");
    }
  }
  else if (stats.tot_cleared == 1) {
    /* If only one data-block: Give more useful message by printing asset name. */
    BKE_reportf(
        &reports, RPT_INFO, "Data-block '%s' is not an asset anymore", stats.last_id->name + 2);
  }
  else {
    BKE_reportf(&reports, RPT_INFO, "%i data-blocks are not assets anymore", stats.tot_cleared);
  }
}

bool AssetClearHelper::wasSuccessful() const
{
  return stats.tot_cleared > 0;
}

static wmOperatorStatus asset_clear_exec(const bContext *C,
                                         const wmOperator *op,
                                         const Span<PointerRNA> ids)
{
  const bool set_fake_user = RNA_boolean_get(op->ptr, "set_fake_user");
  AssetClearHelper clear_helper(set_fake_user);
  clear_helper(ids);
  clear_helper.reportResults(C, *op->reports);

  if (!clear_helper.wasSuccessful()) {
    return OPERATOR_CANCELLED;
  }

  WM_main_add_notifier(NC_ID | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_ASSET | NA_REMOVED, nullptr);

  return OPERATOR_FINISHED;
}

static bool asset_clear_poll(bContext *C, const Span<PointerRNA> ids)
{
  IDVecStats ctx_stats = asset_operation_get_id_vec_stats_from_ids(ids);

  if (!ctx_stats.has_asset) {
    const char *msg_single = N_("Data-block is not marked as asset");
    const char *msg_multiple = N_("No data-block selected that is marked as asset");
    CTX_wm_operator_poll_msg_set(C, ctx_stats.is_single ? msg_single : msg_multiple);
    return false;
  }
  if (!ctx_stats.has_supported_type) {
    CTX_wm_operator_poll_msg_set(C, asset_operation_unsupported_type_msg(ctx_stats.is_single));
    return false;
  }

  return true;
}

static std::string asset_clear_get_description(bContext * /*C*/,
                                               wmOperatorType * /*ot*/,
                                               PointerRNA *ptr)
{
  const bool set_fake_user = RNA_boolean_get(ptr, "set_fake_user");
  if (!set_fake_user) {
    return "";
  }
  return TIP_(
      "Delete all asset metadata, turning the selected asset data-blocks back into normal "
      "data-blocks, and set Fake User to ensure the data-blocks will still be saved");
}

/**
 * Variant of #ASSET_OT_clear that only works on the "id" context member.
 */
static void ASSET_OT_clear(wmOperatorType *ot)
{
  ot->name = "Clear Asset";
  ot->description =
      "Delete all asset metadata and turn the selected asset data-blocks back into normal "
      "data-blocks";
  ot->get_description = asset_clear_get_description;
  ot->idname = "ASSET_OT_clear";

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    return asset_clear_exec(C, op, ED_operator_get_ids_from_context_as_vec(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_clear_poll(C, ED_operator_get_ids_from_context_as_vec(C));
  };

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "set_fake_user",
                  false,
                  "Set Fake User",
                  "Ensure the data-block is saved, even when it is no longer marked as asset");
}

static void ASSET_OT_clear_single(wmOperatorType *ot)
{
  ot->name = "Clear Single Asset";
  ot->description =
      "Delete all asset metadata and turn the asset data-block back into a normal data-block";
  ot->get_description = asset_clear_get_description;
  ot->idname = "ASSET_OT_clear_single";

  ot->exec = [](bContext *C, wmOperator *op) -> wmOperatorStatus {
    return asset_clear_exec(C, op, ED_operator_single_id_from_context_as_vec(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_clear_poll(C, ED_operator_single_id_from_context_as_vec(C));
  };

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "set_fake_user",
                  false,
                  "Set Fake User",
                  "Ensure the data-block is saved, even when it is no longer marked as asset");
}

/* -------------------------------------------------------------------- */

static bool asset_library_refresh_poll(bContext *C)
{
  if (ED_operator_asset_browsing_active(C)) {
    return true;
  }

  /* While not inside an Asset Browser, check if there's a asset list stored for the active asset
   * library (stored in the workspace, obtained via context). */
  const AssetLibraryReference *library = CTX_wm_asset_library_ref(C);
  if (!library) {
    return false;
  }

  return list::has_list_storage_for_library(library) ||
         list::has_asset_browser_storage_for_library(library, C);
}

static wmOperatorStatus asset_library_refresh_exec(bContext *C, wmOperator * /*unused*/)
{
  const AssetLibraryReference *library = CTX_wm_asset_library_ref(C);
  /* Handles both global asset list storage and asset browsers. */
  list::clear(library, C);
  WM_event_add_notifier(C, NC_ASSET | ND_ASSET_LIST_READING, nullptr);

  return OPERATOR_FINISHED;
}

static void ASSET_OT_library_refresh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Refresh Asset Library";
  ot->description = "Reread assets and asset catalogs from the asset library on disk";
  ot->idname = "ASSET_OT_library_refresh";

  /* API callbacks. */
  ot->exec = asset_library_refresh_exec;
  ot->poll = asset_library_refresh_poll;
}

/* -------------------------------------------------------------------- */

static bool asset_catalog_operator_poll(bContext *C)
{
  const SpaceFile *sfile = CTX_wm_space_file(C);
  if (!sfile) {
    return false;
  }
  const asset_system::AssetLibrary *asset_library = ED_fileselect_active_asset_library_get(sfile);
  if (!asset_library) {
    return false;
  }
  if (catalogs_read_only(*asset_library)) {
    CTX_wm_operator_poll_msg_set(C, "Asset catalogs cannot be edited in this asset library");
    return false;
  }
  return true;
}

static wmOperatorStatus asset_catalog_new_exec(bContext *C, wmOperator *op)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  asset_system::AssetLibrary *asset_library = ED_fileselect_active_asset_library_get(sfile);
  std::string parent_path = RNA_string_get(op->ptr, "parent_path");

  asset_system::AssetCatalog *new_catalog = catalog_add(
      asset_library, DATA_("Catalog"), parent_path);

  if (sfile) {
    ED_fileselect_activate_asset_catalog(sfile, new_catalog->catalog_id);
  }

  WM_event_add_notifier_ex(
      CTX_wm_manager(C), CTX_wm_window(C), NC_ASSET | ND_ASSET_CATALOGS, nullptr);

  return OPERATOR_FINISHED;
}

static void ASSET_OT_catalog_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Asset Catalog";
  ot->description = "Create a new catalog to put assets in";
  ot->idname = "ASSET_OT_catalog_new";

  /* API callbacks. */
  ot->exec = asset_catalog_new_exec;
  ot->poll = asset_catalog_operator_poll;

  RNA_def_string(ot->srna,
                 "parent_path",
                 nullptr,
                 0,
                 "Parent Path",
                 "Optional path defining the location to put the new catalog under");
}

static wmOperatorStatus asset_catalog_delete_exec(bContext *C, wmOperator *op)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  asset_system::AssetLibrary *asset_library = ED_fileselect_active_asset_library_get(sfile);
  std::string catalog_id_str = RNA_string_get(op->ptr, "catalog_id");
  asset_system::CatalogID catalog_id;
  if (!BLI_uuid_parse_string(&catalog_id, catalog_id_str.c_str())) {
    return OPERATOR_CANCELLED;
  }

  catalog_remove(asset_library, catalog_id);

  WM_event_add_notifier_ex(
      CTX_wm_manager(C), CTX_wm_window(C), NC_ASSET | ND_ASSET_CATALOGS, nullptr);

  return OPERATOR_FINISHED;
}

static void ASSET_OT_catalog_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Asset Catalog";
  ot->description =
      "Remove an asset catalog from the asset library (contained assets will not be affected and "
      "show up as unassigned)";
  ot->idname = "ASSET_OT_catalog_delete";

  /* API callbacks. */
  ot->exec = asset_catalog_delete_exec;
  ot->poll = asset_catalog_operator_poll;

  RNA_def_string(ot->srna, "catalog_id", nullptr, 0, "Catalog ID", "ID of the catalog to delete");
}

static asset_system::AssetCatalogService *get_catalog_service(bContext *C)
{
  const SpaceFile *sfile = CTX_wm_space_file(C);
  if (!sfile || ED_fileselect_is_file_browser(sfile)) {
    return nullptr;
  }

  asset_system::AssetLibrary *asset_lib = ED_fileselect_active_asset_library_get(sfile);
  if (asset_lib) {
    return &asset_lib->catalog_service();
  }

  return nullptr;
}

static wmOperatorStatus asset_catalog_undo_exec(bContext *C, wmOperator * /*op*/)
{
  asset_system::AssetCatalogService *catalog_service = get_catalog_service(C);
  if (!catalog_service) {
    return OPERATOR_CANCELLED;
  }

  catalog_service->undo();
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  return OPERATOR_FINISHED;
}

static bool asset_catalog_undo_poll(bContext *C)
{
  const asset_system::AssetCatalogService *catalog_service = get_catalog_service(C);
  return catalog_service && catalog_service->is_undo_possbile();
}

static void ASSET_OT_catalog_undo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Undo Catalog Edits";
  ot->description = "Undo the last edit to the asset catalogs";
  ot->idname = "ASSET_OT_catalog_undo";

  /* API callbacks. */
  ot->exec = asset_catalog_undo_exec;
  ot->poll = asset_catalog_undo_poll;
}

static wmOperatorStatus asset_catalog_redo_exec(bContext *C, wmOperator * /*op*/)
{
  asset_system::AssetCatalogService *catalog_service = get_catalog_service(C);
  if (!catalog_service) {
    return OPERATOR_CANCELLED;
  }

  catalog_service->redo();
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  return OPERATOR_FINISHED;
}

static bool asset_catalog_redo_poll(bContext *C)
{
  const asset_system::AssetCatalogService *catalog_service = get_catalog_service(C);
  return catalog_service && catalog_service->is_redo_possbile();
}

static void ASSET_OT_catalog_redo(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Redo Catalog Edits";
  ot->description = "Redo the last undone edit to the asset catalogs";
  ot->idname = "ASSET_OT_catalog_redo";

  /* API callbacks. */
  ot->exec = asset_catalog_redo_exec;
  ot->poll = asset_catalog_redo_poll;
}

static wmOperatorStatus asset_catalog_undo_push_exec(bContext *C, wmOperator * /*op*/)
{
  asset_system::AssetCatalogService *catalog_service = get_catalog_service(C);
  if (!catalog_service) {
    return OPERATOR_CANCELLED;
  }

  catalog_service->undo_push();
  return OPERATOR_FINISHED;
}

static bool asset_catalog_undo_push_poll(bContext *C)
{
  return get_catalog_service(C) != nullptr;
}

static void ASSET_OT_catalog_undo_push(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Store undo snapshot for asset catalog edits";
  ot->description = "Store the current state of the asset catalogs in the undo buffer";
  ot->idname = "ASSET_OT_catalog_undo_push";

  /* API callbacks. */
  ot->exec = asset_catalog_undo_push_exec;
  ot->poll = asset_catalog_undo_push_poll;

  /* Generally artists don't need to find & use this operator, it's meant for scripts only. */
  ot->flag = OPTYPE_INTERNAL;
}

/* -------------------------------------------------------------------- */

static bool asset_catalogs_save_poll(bContext *C)
{
  if (!asset_catalog_operator_poll(C)) {
    return false;
  }

  const Main *bmain = CTX_data_main(C);
  if (!bmain->filepath[0]) {
    CTX_wm_operator_poll_msg_set(C, "Cannot save asset catalogs before the Blender file is saved");
    return false;
  }

  if (!AS_asset_library_has_any_unsaved_catalogs()) {
    CTX_wm_operator_poll_msg_set(C, "No changes to be saved");
    return false;
  }

  return true;
}

static wmOperatorStatus asset_catalogs_save_exec(bContext *C, wmOperator * /*op*/)
{
  const SpaceFile *sfile = CTX_wm_space_file(C);
  asset_system::AssetLibrary *asset_library = ED_fileselect_active_asset_library_get(sfile);

  catalogs_save_from_main_path(asset_library, CTX_data_main(C));

  WM_event_add_notifier_ex(
      CTX_wm_manager(C), CTX_wm_window(C), NC_ASSET | ND_ASSET_CATALOGS, nullptr);

  return OPERATOR_FINISHED;
}

static void ASSET_OT_catalogs_save(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Save Asset Catalogs";
  ot->description =
      "Make any edits to any catalogs permanent by writing the current set up to the asset "
      "library";
  ot->idname = "ASSET_OT_catalogs_save";

  /* API callbacks. */
  ot->exec = asset_catalogs_save_exec;
  ot->poll = asset_catalogs_save_poll;
}

/* -------------------------------------------------------------------- */

static bool could_be_asset_bundle(const Main *bmain);
static const bUserAssetLibrary *selected_asset_library(wmOperator *op);
static bool is_contained_in_selected_asset_library(wmOperator *op, const char *filepath);
static bool set_filepath_for_asset_lib(const Main *bmain, wmOperator *op);
static bool has_external_files(Main *bmain, ReportList *reports);

static bool asset_bundle_install_poll(bContext *C)
{
  /* This operator only works when the asset browser is set to Current File. */
  const SpaceFile *sfile = CTX_wm_space_file(C);
  if (sfile == nullptr) {
    return false;
  }
  if (!ED_fileselect_is_local_asset_library(sfile)) {
    return false;
  }

  const Main *bmain = CTX_data_main(C);
  if (!could_be_asset_bundle(bmain)) {
    return false;
  }

  /* Check whether this file is already located inside any asset library. */
  const bUserAssetLibrary *asset_lib = BKE_preferences_asset_library_containing_path(
      &U, bmain->filepath);
  if (asset_lib) {
    return false;
  }

  return true;
}

static wmOperatorStatus asset_bundle_install_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  Main *bmain = CTX_data_main(C);
  if (has_external_files(bmain, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_fileselect(C, op);

  /* Make the "Save As" dialog box default to "${ASSET_LIB_ROOT}/${CURRENT_FILE}.blend". */
  if (!set_filepath_for_asset_lib(bmain, op)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus asset_bundle_install_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  if (has_external_files(bmain, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  /* Check file path, copied from #wm_file_write(). */
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  const size_t len = strlen(filepath);

  if (len == 0) {
    BKE_report(op->reports, RPT_ERROR, "Path is empty, cannot save");
    return OPERATOR_CANCELLED;
  }

  if (len >= FILE_MAX) {
    BKE_report(op->reports, RPT_ERROR, "Path too long, cannot save");
    return OPERATOR_CANCELLED;
  }

  /* Check that the destination is actually contained in the selected asset library. */
  if (!is_contained_in_selected_asset_library(op, filepath)) {
    BKE_reportf(op->reports, RPT_ERROR, "Selected path is outside of the selected asset library");
    return OPERATOR_CANCELLED;
  }

  WM_cursor_wait(true);
  asset_system::AssetCatalogService *cat_service = get_catalog_service(C);
  /* Store undo step, such that on a failed save the 'prepare_to_merge_on_write' call can be
   * un-done. */
  cat_service->undo_push();
  cat_service->prepare_to_merge_on_write();

  const wmOperatorStatus operator_result = WM_operator_name_call(
      C, "WM_OT_save_mainfile", wm::OpCallContext::ExecDefault, op->ptr, nullptr);
  WM_cursor_wait(false);

  if (operator_result != OPERATOR_FINISHED) {
    cat_service->undo();
    return operator_result;
  }

  const bUserAssetLibrary *lib = selected_asset_library(op);
  BLI_assert_msg(lib, "If the asset library is not known, how did we get here?");
  BKE_reportf(op->reports,
              RPT_INFO,
              R"(Saved "%s" to asset library "%s")",
              BLI_path_basename(bmain->filepath),
              lib->name);
  return OPERATOR_FINISHED;
}

static const EnumPropertyItem *rna_asset_library_reference_itemf(bContext * /*C*/,
                                                                 PointerRNA * /*ptr*/,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool *r_free)
{
  const EnumPropertyItem *items = custom_libraries_rna_enum_itemf();
  if (!items) {
    *r_free = false;
    return nullptr;
  }

  *r_free = true;
  return items;
}

static void ASSET_OT_bundle_install(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy to Asset Library";
  ot->description =
      "Copy the current .blend file into an Asset Library. Only works on standalone .blend files "
      "(i.e. when no other files are referenced)";
  ot->idname = "ASSET_OT_bundle_install";

  /* API callbacks. */
  ot->exec = asset_bundle_install_exec;
  ot->invoke = asset_bundle_install_invoke;
  ot->poll = asset_bundle_install_poll;

  ot->prop = RNA_def_property(ot->srna, "asset_library_reference", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(ot->prop, PROP_HIDDEN);
  RNA_def_enum_funcs(ot->prop, rna_asset_library_reference_itemf);

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/* Cheap check to see if this is an "asset bundle" just by checking main file name.
 * A proper check will be done in the exec function, to ensure that no external files will be
 * referenced. */
static bool could_be_asset_bundle(const Main *bmain)
{
  return fnmatch("*_bundle.blend", bmain->filepath, FNM_CASEFOLD) == 0;
}

static const bUserAssetLibrary *selected_asset_library(wmOperator *op)
{
  const int enum_value = RNA_enum_get(op->ptr, "asset_library_reference");
  const AssetLibraryReference lib_ref = library_reference_from_enum_value(enum_value);
  const bUserAssetLibrary *lib = BKE_preferences_asset_library_find_index(
      &U, lib_ref.custom_library_index);
  return lib;
}

static bool is_contained_in_selected_asset_library(wmOperator *op, const char *filepath)
{
  const bUserAssetLibrary *lib = selected_asset_library(op);
  if (!lib) {
    return false;
  }
  return BLI_path_contains(lib->dirpath, filepath);
}

/**
 * Set the "filepath" RNA property based on selected "asset_library_reference".
 * \return true if ok, false if error.
 */
static bool set_filepath_for_asset_lib(const Main *bmain, wmOperator *op)
{
  /* Find the directory path of the selected asset library. */
  const bUserAssetLibrary *lib = selected_asset_library(op);
  if (lib == nullptr) {
    return false;
  }

  /* Concatenate the filename of the current blend file. */
  const char *blend_filename = BLI_path_basename(bmain->filepath);
  if (blend_filename == nullptr || blend_filename[0] == '\0') {
    return false;
  }

  char file_path[FILE_MAX];
  BLI_path_join(file_path, sizeof(file_path), lib->dirpath, blend_filename);
  RNA_string_set(op->ptr, "filepath", file_path);

  return true;
}

struct FileCheckCallbackInfo {
  ReportList *reports;
  Set<std::string> external_files;
};

static bool external_file_check_callback(BPathForeachPathData *bpath_data,
                                         char * /*path_dst*/,
                                         size_t /*path_dst_maxncpy*/,
                                         const char *path_src)
{
  FileCheckCallbackInfo *callback_info = static_cast<FileCheckCallbackInfo *>(
      bpath_data->user_data);
  callback_info->external_files.add(std::string(path_src));
  return false;
}

/**
 * Do a check on any external files (.blend, textures, etc.) being used.
 * The #ASSET_OT_bundle_install operator only works on standalone `.blend` files
 * (catalog definition files are fine, though).
 *
 * \return true when there are external files, false otherwise.
 */
static bool has_external_files(Main *bmain, ReportList *reports)
{
  FileCheckCallbackInfo callback_info = {reports, Set<std::string>()};

  eBPathForeachFlag flag =
      (BKE_BPATH_FOREACH_PATH_SKIP_PACKED          /* Packed files are fine. */
       | BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE     /* Only report multi-files once, it's enough. */
       | BKE_BPATH_TRAVERSE_SKIP_WEAK_REFERENCES); /* Only care about actually used files. */

  BPathForeachPathData bpath_data = {
      /*bmain*/ bmain,
      /*callback_function*/ &external_file_check_callback,
      /*flag*/ flag,
      /*user_data*/ &callback_info,
      /*absolute_base_path*/ nullptr,
  };
  BKE_bpath_foreach_path_main(&bpath_data);

  if (callback_info.external_files.is_empty()) {
    /* No external dependencies. */
    return false;
  }

  if (callback_info.external_files.size() == 1) {
    /* Only one external dependency, report it directly. */
    BKE_reportf(callback_info.reports,
                RPT_ERROR,
                "Unable to copy bundle due to external dependency: \"%s\"",
                callback_info.external_files.begin()->c_str());
    return true;
  }

  /* Multiple external dependencies, report the aggregate and put details on console. */
  BKE_reportf(
      callback_info.reports,
      RPT_ERROR,
      "Unable to copy bundle due to %zu external dependencies; more details on the console",
      size_t(callback_info.external_files.size()));
  printf("Unable to copy bundle due to %zu external dependencies:\n",
         size_t(callback_info.external_files.size()));
  for (const std::string &path : callback_info.external_files) {
    printf("   \"%s\"\n", path.c_str());
  }
  return true;
}

constexpr int DRAG_THRESHOLD = 4;

struct ScreenshotOperatorData {
  void *draw_handle;
  int2 drag_start, drag_end, last_cursor;
  /* Screenshot points may not be set immediately to allow for clicking to create a screenshot with
   * the previous size. */
  int2 p1, p2;

  bool is_mouse_down;
  /* Dragged far enough to create the screenshot are instead of registering as a click. */
  bool crossed_threshold;
  /* Move the whole screenshot area when moving the cursor instead of placing `drag_end`. */
  bool shift_area;
  bool force_square;
};

/* Sort points so p1 is lower left, and p2 is top right. */
static inline void sort_points(int2 &p1, int2 &p2)
{
  if (p1.x > p2.x) {
    std::swap(p1.x, p2.x);
  }
  if (p1.y > p2.y) {
    std::swap(p1.y, p2.y);
  }
}

/* Clamps the point to the window bounds. */
static inline int2 clamp_point_to_window(const int2 &point, const wmWindow *window)
{
  const int2 win_size = WM_window_native_pixel_size(window);
  return {clamp_i(point.x, 0, win_size.x - 1), clamp_i(point.y, 0, win_size.y - 1)};
}

/* Ensures that the x and y distance to from p1 to p2 is equal and the resulting square remains
 * fully within the window bounds. The two points can be in any spacial relation to each other i.e.
 * if p1 was top left, it remains top left. */
static inline void square_points_clamp_to_window(const int2 &p1, int2 &p2, const wmWindow *window)
{
  const int2 delta = p2 - p1;

  /* Determine the drag direction for each axis. */
  const int dir_x = (delta.x >= 0) ? 1 : -1;
  const int dir_y = (delta.y >= 0) ? 1 : -1;

  const int size_x = std::abs(delta.x);
  const int size_y = std::abs(delta.y);
  int square_size = std::max(size_x, size_y);

  /* Compute maximum size that fits within window bounds in the drag direction. */
  const int2 win_size = WM_window_native_pixel_size(window);
  const int max_size_x = (dir_x > 0) ? win_size.x - p1.x - 1 : p1.x;
  const int max_size_y = (dir_y > 0) ? win_size.y - p1.y - 1 : p1.y;

  /* Clamp the square size so it does not exceed window bounds. */
  square_size = std::min(square_size, std::min(max_size_x, max_size_y));

  /* Update p2 to form a clamped square in the same direction as the drag. */
  p2.x = p1.x + dir_x * square_size;
  p2.y = p1.y + dir_y * square_size;
}

static void generate_previewimg_from_buffer(ID *id, const ImBuf *image_buffer)
{
  PreviewImage *preview_image = BKE_previewimg_id_ensure(id);
  BKE_previewimg_clear(preview_image);

  for (int size_type = 0; size_type < NUM_ICON_SIZES; size_type++) {
    BKE_previewimg_ensure(preview_image, size_type);
    int width = image_buffer->x;
    int height = image_buffer->y;
    int max_size = 0;
    switch (size_type) {
      case ICON_SIZE_ICON:
        max_size = ICON_RENDER_DEFAULT_HEIGHT;
        break;
      case ICON_SIZE_PREVIEW:
        max_size = PREVIEW_RENDER_LARGE_HEIGHT;
        break;
    }
    if (max_size == 0) {
      /* Can only be reached if a new icon size is added. */
      BLI_assert_unreachable();
      continue;
    }

    /* Scales down the image to `max_size` while maintaining the
     * aspect ratio. */
    if (image_buffer->x > image_buffer->y) {
      width = max_size;
      height = image_buffer->y * (width / float(image_buffer->x));
    }
    else if (image_buffer->y > image_buffer->x) {
      height = max_size;
      width = image_buffer->x * (height / float(image_buffer->y));
    }
    else {
      width = height = max_size;
    }

    ImBuf *scaled_imbuf = IMB_scale_into_new(
        image_buffer, width, height, IMBScaleFilter::Nearest, false);
    preview_image->rect[size_type] = (uint *)MEM_dupallocN(scaled_imbuf->byte_buffer.data);
    preview_image->w[size_type] = width;
    preview_image->h[size_type] = height;
    preview_image->flag[size_type] |= PRV_USER_EDITED;
    IMB_freeImBuf(scaled_imbuf);
  }
}

/**
 * Takes a screenshot of Blender for the given rect. The returned `ImBuf` has to be freed by the
 * caller with `IMB_freeImBuf()`.
 */
static ImBuf *take_screenshot_crop(bContext *C, const rcti &crop_rect)
{
  int dumprect_size[2];
  wmWindow *win = CTX_wm_window(C);
  uint8_t *dumprect = WM_window_pixels_read(C, win, dumprect_size);

  /* Clamp coordinates to window bounds. */
  rcti safe_rect = crop_rect;
  safe_rect.xmin = max_ii(0, crop_rect.xmin);
  safe_rect.ymin = max_ii(0, crop_rect.ymin);
  safe_rect.xmax = min_ii(dumprect_size[0] - 1, crop_rect.xmax);
  safe_rect.ymax = min_ii(dumprect_size[1] - 1, crop_rect.ymax);

  /* Validate rectangle. */
  if (!BLI_rcti_is_valid(&safe_rect)) {
    MEM_freeN(dumprect);
    return nullptr;
  }

  ImBuf *image_buffer = IMB_allocImBuf(dumprect_size[0], dumprect_size[1], 24, 0);
  /* Using IB_TAKE_OWNERSHIP because the crop does kind of take ownership already it seems. At
   * least freeing the memory after would cause a crash if ownership isn't taken. */
  IMB_assign_byte_buffer(image_buffer, dumprect, IB_TAKE_OWNERSHIP);

  IMB_rect_crop(image_buffer, &safe_rect);
  return image_buffer;
}

static wmOperatorStatus screenshot_preview_exec(bContext *C, wmOperator *op)
{
  int2 p1, p2;
  wmWindow *win = CTX_wm_window(C);
  RNA_int_get_array(op->ptr, "p1", p1);
  RNA_int_get_array(op->ptr, "p2", p2);

  /* Clamp points to window bounds, so the screenshot area is always valid. */
  p1 = clamp_point_to_window(p1, win);
  p2 = clamp_point_to_window(p2, win);

  /* Squaring has to happen before sorting so the area is squared from the point where
   * dragging started. */
  if (RNA_boolean_get(op->ptr, "force_square")) {
    square_points_clamp_to_window(p1, p2, win);
  }

  sort_points(p1, p2);

  /* The min side is chosen arbitrarily to avoid accidental creations of very small screenshots. */
  constexpr int min_side = 16;
  if (p2.x - p1.x < min_side || p2.y - p1.y < min_side) {
    BKE_reportf(
        op->reports, RPT_ERROR, "Screenshot cannot be smaller than %i pixels on a side", min_side);
    return OPERATOR_CANCELLED;
  }

  ImBuf *image_buffer;

  ScrArea *area_p1 = ED_area_find_under_cursor(C, SPACE_TYPE_ANY, p1);
  ScrArea *area_p2 = ED_area_find_under_cursor(C, SPACE_TYPE_ANY, p2);
  /* Special case for taking a screenshot from a 3D viewport. In that case we do an offscreen
   * render to support transparency. Render settings are used as currently set up in the viewport
   * to comply with WYSIWYG as much as possible. One limitation is that GUI elements will not be
   * visible in the render. */
  bool render_offscreen = false;
  if (area_p1 == area_p2 && area_p1 != nullptr && area_p1->spacetype == SPACE_VIEW3D) {
    Scene *scene = CTX_data_scene(C);
    View3D *v3d = static_cast<View3D *>(area_p1->spacedata.first);
    /* For #ED_view3d_draw_offscreen_imbuf only EEVEE only produces a good result. See #141732. */
    if (eDrawType(v3d->shading.type) == OB_RENDER) {
      const char *engine_name = scene->r.engine;
      render_offscreen = STR_ELEM(engine_name,
                                  RE_engine_id_BLENDER_EEVEE,
                                  RE_engine_id_BLENDER_EEVEE_NEXT,
                                  RE_engine_id_BLENDER_WORKBENCH);
    }
    else {
      render_offscreen = true;
    }
  }
  if (render_offscreen) {
    View3D *v3d = static_cast<View3D *>(area_p1->spacedata.first);
    ARegion *region = BKE_area_find_region_type(area_p1, RGN_TYPE_WINDOW);
    if (!region) {
      /* Unlikely to be hit, but just being cautious. */
      BLI_assert_unreachable();
      return OPERATOR_CANCELLED;
    }
    char err_out[256] = "unknown";
    image_buffer = ED_view3d_draw_offscreen_imbuf(CTX_data_ensure_evaluated_depsgraph(C),
                                                  CTX_data_scene(C),
                                                  eDrawType(v3d->shading.type),
                                                  v3d,
                                                  region,
                                                  region->winx,
                                                  region->winy,
                                                  IB_byte_data,
                                                  R_ALPHAPREMUL,
                                                  nullptr,
                                                  false,
                                                  nullptr,
                                                  nullptr,
                                                  err_out);

    /* Convert crop rect into the space relative to the area. */
    const rcti crop_rect = {p1.x - area_p1->totrct.xmin,
                            p2.x - area_p1->totrct.xmin,
                            p1.y - area_p1->totrct.ymin,
                            p2.y - area_p1->totrct.ymin};
    IMB_rect_crop(image_buffer, &crop_rect);
  }
  else {
    const rcti crop_rect = {p1.x, p2.x, p1.y, p2.y};
    image_buffer = take_screenshot_crop(C, crop_rect);
    if (!image_buffer) {
      BKE_report(op->reports, RPT_ERROR, "Invalid screenshot area selection");
      return OPERATOR_CANCELLED;
    }
  }

  const AssetRepresentationHandle *asset_handle = CTX_wm_asset(C);
  BLI_assert_msg(asset_handle != nullptr, "This is ensured by poll");
  AssetWeakReference asset_reference = asset_handle->make_weak_reference();

  Main *bmain = CTX_data_main(C);
  ID *id = bke::asset_edit_id_from_weak_reference(
      *bmain, asset_handle->get_id_type(), asset_reference);
  BLI_assert(id != nullptr);

  ED_preview_kill_jobs_for_id(CTX_wm_manager(C), id);

  generate_previewimg_from_buffer(id, image_buffer);
  IMB_freeImBuf(image_buffer);

  if (bke::asset_edit_id_is_writable(*id)) {
    const bool saved = bke::asset_edit_id_save(*bmain, *id, *op->reports);
    if (!saved) {
      BKE_report(op->reports, RPT_ERROR, "Saving failed");
    }
  }

  asset::list::storage_tag_main_data_dirty();
  asset::refresh_asset_library_from_asset(C, *asset_handle);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void screenshot_preview_draw(const wmWindow *window, void *operator_data)
{
  ScreenshotOperatorData *data = static_cast<ScreenshotOperatorData *>(operator_data);
  int2 p1 = data->p1;
  int2 p2 = data->p2;

  /* Clamp points to window bounds, so the screenshot area is always valid. */
  p1 = clamp_point_to_window(p1, window);
  p2 = clamp_point_to_window(p2, window);

  /* Squaring has to happen before sorting so the area is squared from the point where
   * dragging started. */
  if (data->force_square) {
    square_points_clamp_to_window(p1, p2, window);
  }

  sort_points(p1, p2);

  /* Drawing rect just out of the screenshot area to not capture the box in the picture. */
  const rctf screenshot_rect = {
      float(p1.x - 1), float(p2.x + 1), float(p1.y - 1), float(p2.y + 1)};

  /* Drawing a semi-transparent mask to highlight the area that will be captured. */
  float4 mask_color = {1, 1, 1, 0.25};
  const int2 win_size = WM_window_native_pixel_size(window);
  const rctf mask_rect_bottom = {0, float(win_size.x), 0, screenshot_rect.ymin};
  UI_draw_roundbox_aa(&mask_rect_bottom, true, 0, mask_color);
  const rctf mask_rect_top = {0, float(win_size.x), screenshot_rect.ymax, float(win_size.y)};
  UI_draw_roundbox_aa(&mask_rect_top, true, 0, mask_color);
  const rctf mask_rect_left = {
      0, screenshot_rect.xmin, screenshot_rect.ymin, screenshot_rect.ymax};
  UI_draw_roundbox_aa(&mask_rect_left, true, 0, mask_color);
  const rctf mask_rect_right = {
      screenshot_rect.xmax, float(win_size.x), screenshot_rect.ymin, screenshot_rect.ymax};
  UI_draw_roundbox_aa(&mask_rect_right, true, 0, mask_color);

  float4 color;
  UI_GetThemeColor4fv(TH_EDITOR_BORDER, color);
  UI_draw_roundbox_aa(&screenshot_rect, false, 0, color);
}

static void screenshot_preview_exit(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  WM_cursor_modal_restore(win);
  ScreenshotOperatorData *data = static_cast<ScreenshotOperatorData *>(op->customdata);
  WM_draw_cb_exit(win, data->draw_handle);
  MEM_freeN(data);
  ED_workspace_status_text(C, nullptr);
}

static inline void screenshot_area_transfer_to_rna(wmOperator *op, ScreenshotOperatorData *data)
{
  RNA_boolean_set(op->ptr, "force_square", data->force_square);
  RNA_int_set_array(op->ptr, "p1", data->p1);
  RNA_int_set_array(op->ptr, "p2", data->p2);
}

static wmOperatorStatus screenshot_preview_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  wmWindow *win = CTX_wm_window(C);
  ScreenshotOperatorData *data = static_cast<ScreenshotOperatorData *>(op->customdata);

  const int2 screen_space_cursor = {
      event->mval[0] + region->winrct.xmin,
      event->mval[1] + region->winrct.ymin,
  };
  switch (event->type) {
    case LEFTMOUSE: {
      switch (event->val) {
        case KM_PRESS:
          data->is_mouse_down = true;
          data->crossed_threshold = false;
          data->drag_start = screen_space_cursor;
          break;
        case KM_RELEASE:
          data->is_mouse_down = false;
          data->drag_end = clamp_point_to_window(screen_space_cursor, win);
          screenshot_area_transfer_to_rna(op, data);
          screenshot_preview_exec(C, op);
          screenshot_preview_exit(C, op);
          return OPERATOR_FINISHED;
      }
      break;
    }

    case EVT_PADENTER:
    case EVT_RETKEY: {
      screenshot_area_transfer_to_rna(op, data);
      screenshot_preview_exec(C, op);
      screenshot_preview_exit(C, op);
      return OPERATOR_FINISHED;
    }

    case RIGHTMOUSE:
    case EVT_ESCKEY: {
      screenshot_preview_exit(C, op);
      CTX_wm_screen(C)->do_draw = true;
      return OPERATOR_CANCELLED;
    }

    case EVT_SPACEKEY: {
      switch (event->val) {
        case KM_PRESS:
          data->shift_area = true;
          break;
        case KM_RELEASE:
          data->shift_area = false;
          break;

        default:
          break;
      }
      break;
    }

    case EVT_LEFTSHIFTKEY:
    case EVT_RIGHTSHIFTKEY: {
      switch (event->val) {
        case KM_PRESS:
          data->force_square = false;
          break;
        case KM_RELEASE:
          data->force_square = true;
          break;

        default:
          break;
      }
      break;
    }

    case MOUSEMOVE: {
      if (data->shift_area) {
        const int2 delta = screen_space_cursor - data->last_cursor;
        const int2 new_p1 = data->p1 + delta;
        const int2 new_p2 = data->p2 + delta;

        auto is_within_window = [win](const int2 &pt) -> bool {
          const int2 win_size = WM_window_native_pixel_size(win);
          return pt.x >= 0 && pt.x < win_size.x && pt.y >= 0 && pt.y < win_size.y;
        };

        /* Apply movement only if the entire rectangle stays within window bounds. */
        if (is_within_window(new_p1) && is_within_window(new_p2)) {
          data->p1 = new_p1;
          data->p2 = new_p2;
        }
      }
      else if (data->is_mouse_down) {
        data->drag_end = clamp_point_to_window(screen_space_cursor, win);

        if (!data->crossed_threshold) {
          const int2 delta = data->drag_end - data->drag_start;
          if (std::abs(delta.x) > DRAG_THRESHOLD && std::abs(delta.y) > DRAG_THRESHOLD) {
            /* Only set the points once the threshold has been crossed. This allows to just
             * click to confirm using a potentially existing screenshot rect. */
            data->crossed_threshold = true;
            data->p1 = data->drag_start;
          }
        }

        if (data->crossed_threshold) {
          data->p2 = data->drag_end;
        }
      }

      CTX_wm_screen(C)->do_draw = true;
      data->last_cursor = screen_space_cursor;
      break;
    }

    default:
      break;
  }

  WorkspaceStatus status(C);
  if (data->is_mouse_down) {
    status.item(IFACE_("Cancel"), ICON_EVENT_ESC, ICON_MOUSE_RMB);
  }
  else {
    status.item(IFACE_("Start"), ICON_MOUSE_LMB_DRAG);
  }
  status.item(IFACE_("Confirm"), ICON_MOUSE_LMB, ICON_EVENT_RETURN);
  status.item(IFACE_("Move"), ICON_EVENT_SPACEKEY);
  status.item(IFACE_("Unlock Aspect Ratio"), ICON_EVENT_SHIFT);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus screenshot_preview_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /* event */)
{
  wmWindow *win = CTX_wm_window(C);
  WM_cursor_modal_set(win, WM_CURSOR_CROSS);

  op->customdata = MEM_callocN(sizeof(ScreenshotOperatorData), __func__);
  ScreenshotOperatorData *data = static_cast<ScreenshotOperatorData *>(op->customdata);
  data->draw_handle = WM_draw_cb_activate(win, screenshot_preview_draw, data);
  data->is_mouse_down = false;
  RNA_int_get_array(op->ptr, "p1", data->p1);
  RNA_int_get_array(op->ptr, "p2", data->p2);
  data->last_cursor = data->p1;
  data->shift_area = false;
  data->crossed_threshold = false;
  data->force_square = RNA_boolean_get(op->ptr, "force_square");

  WM_event_add_modal_handler(C, op);
  CTX_wm_screen(C)->do_draw = true;

  return OPERATOR_RUNNING_MODAL;
}

static bool screenshot_preview_poll(bContext *C)
{
  if (G.background) {
    return false;
  }

  const AssetRepresentationHandle *asset_handle = CTX_wm_asset(C);
  if (!asset_handle) {
    CTX_wm_operator_poll_msg_set(C, "No selected asset");
    return false;
  }
  if (asset_handle->is_local_id()) {
    return WM_operator_winactive(C);
  }

  std::string lib_path = asset_handle->full_library_path();
  if (StringRef(lib_path).endswith(BLENDER_ASSET_FILE_SUFFIX)) {
    return true;
  }

  CTX_wm_operator_poll_msg_set(C, "Asset cannot be modified from this file");
  return false;
}

static void ASSET_OT_screenshot_preview(wmOperatorType *ot)
{
  /* This should be a generic operator for assets not linked to the pose-library. */

  ot->name = "Capture Screenshot Preview";
  ot->description = "Capture a screenshot to use as a preview for the selected asset";
  ot->idname = "ASSET_OT_screenshot_preview";

  ot->poll = screenshot_preview_poll;
  ot->invoke = screenshot_preview_invoke;
  ot->modal = screenshot_preview_modal;
  ot->exec = screenshot_preview_exec;

  RNA_def_int_array(ot->srna,
                    "p1",
                    2,
                    nullptr,
                    0,
                    INT_MAX,
                    "Point 1",
                    "First point of the screenshot in screenspace",
                    0,
                    3840);
  RNA_def_int_array(ot->srna,
                    "p2",
                    2,
                    nullptr,
                    0,
                    INT_MAX,
                    "Point 2",
                    "Second point of the screenshot in screenspace",
                    0,
                    3840);
  RNA_def_boolean(ot->srna,
                  "force_square",
                  true,
                  "Force Square",
                  "If enabled, the screenshot will have the same height as width");
}

/* -------------------------------------------------------------------- */

void operatortypes_asset()
{
  WM_operatortype_append(ASSET_OT_mark);
  WM_operatortype_append(ASSET_OT_mark_single);
  WM_operatortype_append(ASSET_OT_clear);
  WM_operatortype_append(ASSET_OT_clear_single);

  WM_operatortype_append(ASSET_OT_catalog_new);
  WM_operatortype_append(ASSET_OT_catalog_delete);
  WM_operatortype_append(ASSET_OT_catalogs_save);
  WM_operatortype_append(ASSET_OT_catalog_undo);
  WM_operatortype_append(ASSET_OT_catalog_redo);
  WM_operatortype_append(ASSET_OT_catalog_undo_push);
  WM_operatortype_append(ASSET_OT_bundle_install);

  WM_operatortype_append(ASSET_OT_library_refresh);

  WM_operatortype_append(ASSET_OT_screenshot_preview);
}

}  // namespace blender::ed::asset
