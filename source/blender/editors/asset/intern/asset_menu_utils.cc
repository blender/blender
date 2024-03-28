/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "DNA_screen_types.h"

#include "BKE_asset.hh"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "ED_asset_list.hh"
#include "ED_asset_menu_utils.hh"

#include "UI_interface.hh"

namespace blender::ed::asset {

void operator_asset_reference_props_register(StructRNA &srna)
{
  PropertyRNA *prop;
  prop = RNA_def_enum(&srna,
                      "asset_library_type",
                      rna_enum_asset_library_type_items,
                      ASSET_LIBRARY_LOCAL,
                      "Asset Library Type",
                      "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_string(
      &srna, "asset_library_identifier", nullptr, 0, "Asset Library Identifier", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_string(
      &srna, "relative_asset_identifier", nullptr, 0, "Relative Asset Identifier", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void operator_asset_reference_props_set(const asset_system::AssetRepresentation &asset,
                                        PointerRNA &ptr)
{
  const AssetWeakReference weak_ref = asset.make_weak_reference();
  RNA_enum_set(&ptr, "asset_library_type", weak_ref.asset_library_type);
  RNA_string_set(&ptr, "asset_library_identifier", weak_ref.asset_library_identifier);
  RNA_string_set(&ptr, "relative_asset_identifier", weak_ref.relative_asset_identifier);
}

/**
 * #AssetLibrary::resolve_asset_weak_reference_to_full_path() currently does not support local
 * assets.
 */
static const asset_system::AssetRepresentation *get_local_asset_from_relative_identifier(
    const bContext &C, const StringRefNull relative_identifier, ReportList *reports)
{
  AssetLibraryReference library_ref{};
  library_ref.type = ASSET_LIBRARY_LOCAL;
  list::storage_fetch(&library_ref, &C);

  const asset_system::AssetRepresentation *matching_asset = nullptr;
  list::iterate(library_ref, [&](asset_system::AssetRepresentation &asset) {
    if (asset.get_identifier().library_relative_identifier() == relative_identifier) {
      matching_asset = &asset;
      return false;
    }
    return true;
  });

  if (reports && !matching_asset) {
    if (list::is_loaded(&library_ref)) {
      BKE_reportf(
          reports, RPT_ERROR, "No asset found at path \"%s\"", relative_identifier.c_str());
    }
    else {
      BKE_report(reports, RPT_WARNING, "Asset loading is unfinished");
    }
  }
  return matching_asset;
}

static const asset_system::AssetRepresentation *find_asset_from_weak_ref(
    const bContext &C, const AssetWeakReference &weak_ref, ReportList *reports)
{
  if (weak_ref.asset_library_type == ASSET_LIBRARY_LOCAL) {
    return get_local_asset_from_relative_identifier(
        C, weak_ref.relative_asset_identifier, reports);
  }

  const AssetLibraryReference library_ref = asset_system::all_library_reference();
  list::storage_fetch(&library_ref, &C);
  asset_system::AssetLibrary *all_library = list::library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    BKE_report(reports, RPT_WARNING, "Asset loading is unfinished");
    return nullptr;
  }

  const std::string full_path = all_library->resolve_asset_weak_reference_to_full_path(weak_ref);

  const asset_system::AssetRepresentation *matching_asset = nullptr;
  list::iterate(library_ref, [&](asset_system::AssetRepresentation &asset) {
    if (asset.get_identifier().full_path() == full_path) {
      matching_asset = &asset;
      return false;
    }
    return true;
  });

  if (reports && !matching_asset) {
    if (list::is_loaded(&library_ref)) {
      BKE_reportf(reports, RPT_ERROR, "No asset found at path \"%s\"", full_path.c_str());
    }
  }
  return matching_asset;
}

const asset_system::AssetRepresentation *operator_asset_reference_props_get_asset_from_all_library(
    const bContext &C, PointerRNA &ptr, ReportList *reports)
{
  AssetWeakReference weak_ref{};
  weak_ref.asset_library_type = RNA_enum_get(&ptr, "asset_library_type");
  weak_ref.asset_library_identifier = RNA_string_get_alloc(
      &ptr, "asset_library_identifier", nullptr, 0, nullptr);
  weak_ref.relative_asset_identifier = RNA_string_get_alloc(
      &ptr, "relative_asset_identifier", nullptr, 0, nullptr);
  return find_asset_from_weak_ref(C, weak_ref, reports);
}

PointerRNA persistent_catalog_path_rna_pointer(const bScreen &owner_screen,
                                               const asset_system::AssetLibrary &library,
                                               const asset_system::AssetCatalogTreeItem &item)
{
  const asset_system::AssetCatalog *catalog = library.catalog_service().find_catalog_by_path(
      item.catalog_path());
  if (!catalog) {
    return PointerRNA_NULL;
  }

  const asset_system::AssetCatalogPath &path = catalog->path;
  return {&const_cast<ID &>(owner_screen.id),
          &RNA_AssetCatalogPath,
          const_cast<asset_system::AssetCatalogPath *>(&path)};
}

void draw_menu_for_catalog(const bScreen &owner_screen,
                           const asset_system::AssetLibrary &library,
                           const asset_system::AssetCatalogTreeItem &item,
                           const StringRefNull menu_name,
                           uiLayout &layout)
{
  PointerRNA path_ptr = asset::persistent_catalog_path_rna_pointer(owner_screen, library, item);
  if (path_ptr.data == nullptr) {
    return;
  }

  uiLayout *col = uiLayoutColumn(&layout, false);
  uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
  uiItemM(col, menu_name.c_str(), IFACE_(item.get_name().c_str()), ICON_NONE);
}

}  // namespace blender::ed::asset
