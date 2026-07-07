/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <string>

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_preferences.h"
#include "BKE_preview_image.hh"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"

#include "BLT_translation.hh"

#include "UI_interface_c.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "DNA_userdef_types.h"

#include "RNA_access.hh"

#include "ED_asset.hh"

namespace blender::ed::asset {

void asset_tooltip(const asset_system::AssetRepresentation &asset,
                   ui::TooltipData &tip,
                   const bool include_name)
{
  if (include_name) {
    tooltip_text_field_add(tip, asset.get_name(), {}, ui::TIP_STYLE_HEADER, ui::TIP_LC_MAIN);
    tooltip_text_field_add(tip, {}, {}, ui::TIP_STYLE_SPACER, ui::TIP_LC_NORMAL, false);
  }

  const AssetMetaData &meta_data = asset.get_metadata();
  if (meta_data.description) {
    tooltip_text_field_add(tip, meta_data.description, {}, ui::TIP_STYLE_HEADER, ui::TIP_LC_MAIN);
  }

  switch (asset.owner_asset_library().library_type()) {
    case ASSET_LIBRARY_CUSTOM: {
      tooltip_text_field_add(tip, {}, {}, ui::TIP_STYLE_SPACER, ui::TIP_LC_NORMAL, false);

      const std::string full_blend_path = asset.full_library_path();

      char dir[FILE_MAX], file[FILE_MAX];
      BLI_path_split_dir_file(full_blend_path.c_str(), dir, sizeof(dir), file, sizeof(file));

      if (file[0]) {
        tooltip_text_field_add(tip, file, {}, ui::TIP_STYLE_NORMAL, ui::TIP_LC_MAIN);
      }
      if (dir[0]) {
        tooltip_text_field_add(tip, dir, {}, ui::TIP_STYLE_NORMAL, ui::TIP_LC_MAIN);
      }
      break;
    }
    case ASSET_LIBRARY_LOCAL:
      tooltip_text_field_add(tip, {}, {}, ui::TIP_STYLE_SPACER, ui::TIP_LC_NORMAL, false);
      tooltip_text_field_add(
          tip, TIP_("Asset Library: Current File"), {}, ui::TIP_STYLE_NORMAL, ui::TIP_LC_VALUE);
      break;
    case ASSET_LIBRARY_ESSENTIALS:
      tooltip_text_field_add(tip, {}, {}, ui::TIP_STYLE_SPACER, ui::TIP_LC_NORMAL, false);
      tooltip_text_field_add(
          tip, TIP_("Asset Library: Essentials"), {}, ui::TIP_STYLE_NORMAL, ui::TIP_LC_VALUE);
      break;
    default:
      /* Intentionally empty. */
      break;
  }
}

BIFIconID asset_preview_icon_id(const asset_system::AssetRepresentation &asset)
{
  if (const PreviewImage *preview = asset.get_preview()) {
    if (!BKE_previewimg_is_invalid(preview, ICON_SIZE_ICON)) {
      return preview->runtime->icon_id;
    }
  }

  return ICON_NONE;
}

BIFIconID asset_preview_or_icon(const asset_system::AssetRepresentation &asset)
{
  const BIFIconID preview_icon = asset_preview_icon_id(asset);
  if (preview_icon != ICON_NONE) {
    return preview_icon;
  }

  /* Preview image not found or invalid. Use type icon. */
  return ui::icon_from_idcode(asset.get_id_type());
}

const bUserAssetLibrary *get_asset_library_from_opptr(PointerRNA &ptr)
{
  const int enum_value = RNA_enum_get(&ptr, "asset_library_reference");
  const AssetLibraryReference lib_ref = asset::library_reference_from_enum_value(enum_value);
  return BKE_preferences_asset_library_find_index(&U, lib_ref.custom_library_index);
}

AssetLibraryReference get_asset_library_ref_from_opptr(PointerRNA &ptr)
{
  const int enum_value = RNA_enum_get(&ptr, "asset_library_reference");
  return asset::library_reference_from_enum_value(enum_value);
}

std::optional<AssetLibraryReference> get_user_library_ref_for_save(
    const asset_system::AssetLibrary *preferred_library)
{
  std::optional<AssetLibraryReference> preferred_library_ref =
      preferred_library ? preferred_library->library_reference() : std::nullopt;
  BLI_assert(!preferred_library || bool(preferred_library_ref));

  if (preferred_library_ref &&
      !ELEM(preferred_library_ref->type, ASSET_LIBRARY_ALL, ASSET_LIBRARY_ESSENTIALS))
  {
    return preferred_library_ref;
  }

  /* Fallback to the first enabled user library. */
  for (const bUserAssetLibrary &asset_library : U.asset_libraries) {
    if (asset_library.flag & ASSET_LIBRARY_DISABLED) {
      continue;
    }
    return asset::user_library_to_library_ref(asset_library);
  }

  /* No enabled user asset library found. */
  return {};
}

void visit_library_catalogs_catalog_for_search(
    const Main &bmain,
    const AssetLibraryReference lib,
    const StringRef edit_text,
    const FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  const asset_system::AssetLibrary *library = AS_asset_library_load(&bmain, lib);
  if (!library) {
    return;
  }

  if (!edit_text.is_empty()) {
    const asset_system::AssetCatalogPath edit_path = edit_text;
    if (!library->catalog_service().find_catalog_by_path(edit_path)) {
      visit_fn(StringPropertySearchVisitParams{edit_path.str(), std::nullopt, ICON_ADD});
    }
  }

  const std::shared_ptr<const asset_system::AssetCatalogTree> full_tree =
      library->catalog_service().catalog_tree();
  full_tree->foreach_item([&](const asset_system::AssetCatalogTreeItem &item) {
    visit_fn(StringPropertySearchVisitParams{item.catalog_path().str(), std::nullopt});
  });
}

}  // namespace blender::ed::asset
