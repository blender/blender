/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_bpath.hh"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"

#include "BLI_fnmatch.h"
#include "BLI_path_util.h"
#include "BLI_set.hh"

#include "ED_asset.hh"
#include "ED_screen.hh"
/* XXX needs access to the file list, should all be done via the asset system in future. */
#include "ED_fileselect.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"

#include "DNA_space_types.h"

namespace blender::ed::asset {
/* -------------------------------------------------------------------- */

static Vector<PointerRNA> get_single_id_vec_from_context(const bContext *C)
{
  Vector<PointerRNA> ids;
  PointerRNA idptr = CTX_data_pointer_get_type(C, "id", &RNA_ID);
  if (idptr.data) {
    ids.append(idptr);
  }
  return ids;
}

/**
 * Return the IDs to operate on as PointerRNA vector. Prioritizes multiple selected ones
 * ("selected_ids" context member) over a single active one ("id" context member), since usually
 * batch operations are more useful.
 */
static Vector<PointerRNA> asset_operation_get_ids_from_context(const bContext *C)
{
  Vector<PointerRNA> ids;

  /* "selected_ids" context member. */
  {
    ListBase list;
    CTX_data_selected_ids(C, &list);
    LISTBASE_FOREACH (CollectionPointerLink *, link, &list) {
      ids.append(link->ptr);
    }
    BLI_freelistN(&list);

    if (!ids.is_empty()) {
      return ids;
    }
  }

  /* "id" context member. */
  return get_single_id_vec_from_context(C);
}

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

static int asset_mark_exec(const bContext *C, const wmOperator *op, const Span<PointerRNA> ids)
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

  ot->exec = [](bContext *C, wmOperator *op) -> int {
    return asset_mark_exec(C, op, asset_operation_get_ids_from_context(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_mark_poll(C, asset_operation_get_ids_from_context(C));
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

  ot->exec = [](bContext *C, wmOperator *op) -> int {
    return asset_mark_exec(C, op, get_single_id_vec_from_context(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_mark_poll(C, get_single_id_vec_from_context(C));
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
    BKE_reportf(&reports, RPT_INFO, "%i data-blocks are no assets anymore", stats.tot_cleared);
  }
}

bool AssetClearHelper::wasSuccessful() const
{
  return stats.tot_cleared > 0;
}

static int asset_clear_exec(const bContext *C, const wmOperator *op, const Span<PointerRNA> ids)
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
                                               PointerRNA *values)
{
  const bool set_fake_user = RNA_boolean_get(values, "set_fake_user");
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

  ot->exec = [](bContext *C, wmOperator *op) -> int {
    return asset_clear_exec(C, op, asset_operation_get_ids_from_context(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_clear_poll(C, asset_operation_get_ids_from_context(C));
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

  ot->exec = [](bContext *C, wmOperator *op) -> int {
    return asset_clear_exec(C, op, get_single_id_vec_from_context(C));
  };
  ot->poll = [](bContext *C) -> bool {
    return asset_clear_poll(C, get_single_id_vec_from_context(C));
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

  return list::storage_has_list_for_library(library);
}

static int asset_library_refresh_exec(bContext *C, wmOperator * /*unused*/)
{
  /* Execution mode #1: Inside the Asset Browser. */
  if (ED_operator_asset_browsing_active(C)) {
    SpaceFile *sfile = CTX_wm_space_file(C);
    ED_fileselect_clear(CTX_wm_manager(C), sfile);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  }
  else {
    /* Execution mode #2: Outside the Asset Browser, use the asset list. */
    const AssetLibraryReference *library = CTX_wm_asset_library_ref(C);
    list::clear(library, C);
  }

  return OPERATOR_FINISHED;
}

/**
 * This operator currently covers both cases, the File/Asset Browser file list and the asset list
 * used for the asset-view template. Once the asset list design is used by the Asset Browser, this
 * can be simplified to just that case.
 */
static void ASSET_OT_library_refresh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Refresh Asset Library";
  ot->description = "Reread assets and asset catalogs from the asset library on disk";
  ot->idname = "ASSET_OT_library_refresh";

  /* api callbacks */
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

static int asset_catalog_new_exec(bContext *C, wmOperator *op)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  asset_system::AssetLibrary *asset_library = ED_fileselect_active_asset_library_get(sfile);
  char *parent_path = RNA_string_get_alloc(op->ptr, "parent_path", nullptr, 0, nullptr);

  asset_system::AssetCatalog *new_catalog = catalog_add(
      asset_library, DATA_("Catalog"), parent_path);

  if (sfile) {
    ED_fileselect_activate_asset_catalog(sfile, new_catalog->catalog_id);
  }

  MEM_freeN(parent_path);

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

  /* api callbacks */
  ot->exec = asset_catalog_new_exec;
  ot->poll = asset_catalog_operator_poll;

  RNA_def_string(ot->srna,
                 "parent_path",
                 nullptr,
                 0,
                 "Parent Path",
                 "Optional path defining the location to put the new catalog under");
}

static int asset_catalog_delete_exec(bContext *C, wmOperator *op)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  asset_system::AssetLibrary *asset_library = ED_fileselect_active_asset_library_get(sfile);
  char *catalog_id_str = RNA_string_get_alloc(op->ptr, "catalog_id", nullptr, 0, nullptr);
  asset_system::CatalogID catalog_id;
  if (!BLI_uuid_parse_string(&catalog_id, catalog_id_str)) {
    return OPERATOR_CANCELLED;
  }

  catalog_remove(asset_library, catalog_id);

  MEM_freeN(catalog_id_str);

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

  /* api callbacks */
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
  return &asset_lib->catalog_service();
}

static int asset_catalog_undo_exec(bContext *C, wmOperator * /*op*/)
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

  /* api callbacks */
  ot->exec = asset_catalog_undo_exec;
  ot->poll = asset_catalog_undo_poll;
}

static int asset_catalog_redo_exec(bContext *C, wmOperator * /*op*/)
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

  /* api callbacks */
  ot->exec = asset_catalog_redo_exec;
  ot->poll = asset_catalog_redo_poll;
}

static int asset_catalog_undo_push_exec(bContext *C, wmOperator * /*op*/)
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

  /* api callbacks */
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

static int asset_catalogs_save_exec(bContext *C, wmOperator * /*op*/)
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

  /* api callbacks */
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

static int asset_bundle_install_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
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

static int asset_bundle_install_exec(bContext *C, wmOperator *op)
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

  const int operator_result = WM_operator_name_call(
      C, "WM_OT_save_mainfile", WM_OP_EXEC_DEFAULT, op->ptr, nullptr);
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
  const EnumPropertyItem *items = library_reference_to_rna_enum_itemf(false);
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

  /* api callbacks */
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
 * The ASSET_OT_bundle_install operator only works on standalone .blend files
 * (catalog definition files are fine, though).
 *
 * \return true when there are external files, false otherwise.
 */
static bool has_external_files(Main *bmain, ReportList *reports)
{
  FileCheckCallbackInfo callback_info = {reports, Set<std::string>()};

  eBPathForeachFlag flag = static_cast<eBPathForeachFlag>(
      BKE_BPATH_FOREACH_PATH_SKIP_PACKED          /* Packed files are fine. */
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
}

}  // namespace blender::ed::asset
