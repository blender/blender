/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <memory>

#include "BLI_string.h"

#include "AS_asset_identifier.hh"
#include "AS_asset_library.hh"

#include "BKE_asset.hh"

#include "BLO_read_write.hh"

#include "DNA_asset_types.h"

#include "MEM_guardedalloc.h"

using namespace blender;

/* #AssetWeakReference -------------------------------------------- */

AssetWeakReference::AssetWeakReference()
    : asset_library_type(0), asset_library_identifier(nullptr), relative_asset_identifier(nullptr)
{
}

AssetWeakReference::AssetWeakReference(const AssetWeakReference &other)
    : asset_library_type(other.asset_library_type),
      asset_library_identifier(BLI_strdup_null(other.asset_library_identifier)),
      relative_asset_identifier(BLI_strdup_null(other.relative_asset_identifier))
{
}

AssetWeakReference::AssetWeakReference(AssetWeakReference &&other)
    : asset_library_type(other.asset_library_type),
      asset_library_identifier(other.asset_library_identifier),
      relative_asset_identifier(other.relative_asset_identifier)
{
  other.asset_library_type = 0; /* Not a valid type. */
  other.asset_library_identifier = nullptr;
  other.relative_asset_identifier = nullptr;
}

AssetWeakReference::~AssetWeakReference()
{
  MEM_delete(asset_library_identifier);
  MEM_delete(relative_asset_identifier);
}

AssetWeakReference &AssetWeakReference::operator=(const AssetWeakReference &other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) AssetWeakReference(other);
  return *this;
}

AssetWeakReference &AssetWeakReference::operator=(AssetWeakReference &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) AssetWeakReference(std::move(other));
  return *this;
}

bool operator==(const AssetWeakReference &a, const AssetWeakReference &b)
{
  if (a.asset_library_type != b.asset_library_type) {
    return false;
  }
  if (StringRef(a.asset_library_identifier) != StringRef(b.asset_library_identifier)) {
    return false;
  }
  if (StringRef(a.relative_asset_identifier) != StringRef(b.relative_asset_identifier)) {
    return false;
  }
  return true;
}

AssetWeakReference AssetWeakReference::make_reference(
    const asset_system::AssetLibrary &library,
    const asset_system::AssetIdentifier &asset_identifier)
{
  AssetWeakReference weak_ref{};

  weak_ref.asset_library_type = library.library_type();
  StringRefNull name = library.name();
  if (!name.is_empty()) {
    weak_ref.asset_library_identifier = BLI_strdupn(name.c_str(), name.size());
  }

  StringRefNull relative_identifier = asset_identifier.library_relative_identifier();
  weak_ref.relative_asset_identifier = BLI_strdupn(relative_identifier.c_str(),
                                                   relative_identifier.size());

  return weak_ref;
}

void BKE_asset_weak_reference_write(BlendWriter *writer, const AssetWeakReference *weak_ref)
{
  BLO_write_struct(writer, AssetWeakReference, weak_ref);
  BLO_write_string(writer, weak_ref->asset_library_identifier);
  BLO_write_string(writer, weak_ref->relative_asset_identifier);
}

void BKE_asset_weak_reference_read(BlendDataReader *reader, AssetWeakReference *weak_ref)
{
  BLO_read_string(reader, &weak_ref->asset_library_identifier);
  BLO_read_string(reader, &weak_ref->relative_asset_identifier);
}

void BKE_asset_catalog_path_list_free(ListBase &catalog_path_list)
{
  LISTBASE_FOREACH_MUTABLE (AssetCatalogPathLink *, catalog_path, &catalog_path_list) {
    MEM_delete(catalog_path->path);
    BLI_freelinkN(&catalog_path_list, catalog_path);
  }
  BLI_assert(BLI_listbase_is_empty(&catalog_path_list));
}

ListBase BKE_asset_catalog_path_list_duplicate(const ListBase &catalog_path_list)
{
  ListBase duplicated_list = {nullptr};

  LISTBASE_FOREACH (AssetCatalogPathLink *, catalog_path, &catalog_path_list) {
    AssetCatalogPathLink *copied_path = MEM_cnew<AssetCatalogPathLink>(__func__);
    copied_path->path = BLI_strdup(catalog_path->path);

    BLI_addtail(&duplicated_list, copied_path);
  }

  return duplicated_list;
}

void BKE_asset_catalog_path_list_blend_write(BlendWriter *writer,
                                             const ListBase &catalog_path_list)
{
  LISTBASE_FOREACH (const AssetCatalogPathLink *, catalog_path, &catalog_path_list) {
    BLO_write_struct(writer, AssetCatalogPathLink, catalog_path);
    BLO_write_string(writer, catalog_path->path);
  }
}

void BKE_asset_catalog_path_list_blend_read_data(BlendDataReader *reader,
                                                 ListBase &catalog_path_list)
{
  BLO_read_struct_list(reader, AssetCatalogPathLink, &catalog_path_list);
  LISTBASE_FOREACH (AssetCatalogPathLink *, catalog_path, &catalog_path_list) {
    BLO_read_data_address(reader, &catalog_path->path);
  }
}

bool BKE_asset_catalog_path_list_has_path(const ListBase &catalog_path_list,
                                          const char *catalog_path)
{
  return BLI_findstring_ptr(
             &catalog_path_list, catalog_path, offsetof(AssetCatalogPathLink, path)) != nullptr;
}

void BKE_asset_catalog_path_list_add_path(ListBase &catalog_path_list, const char *catalog_path)
{
  AssetCatalogPathLink *new_path = MEM_cnew<AssetCatalogPathLink>(__func__);
  new_path->path = BLI_strdup(catalog_path);
  BLI_addtail(&catalog_path_list, new_path);
}
