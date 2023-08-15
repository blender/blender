/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <memory>

#include "AS_asset_identifier.hh"
#include "AS_asset_library.hh"

#include "BKE_asset.h"

#include "BLO_read_write.h"

#include "DNA_asset_types.h"

#include "MEM_guardedalloc.h"

using namespace blender;

/* #AssetWeakReference -------------------------------------------- */

AssetWeakReference::AssetWeakReference()
    : asset_library_type(0), asset_library_identifier(nullptr), relative_asset_identifier(nullptr)
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

void BKE_asset_weak_reference_free(AssetWeakReference **weak_ref)
{
  MEM_delete(*weak_ref);
  *weak_ref = nullptr;
}

AssetWeakReference *BKE_asset_weak_reference_copy(AssetWeakReference *weak_ref)
{
  if (weak_ref == nullptr) {
    return nullptr;
  }

  AssetWeakReference *weak_ref_copy = MEM_new<AssetWeakReference>(__func__);
  weak_ref_copy->asset_library_type = weak_ref->asset_library_type;
  weak_ref_copy->asset_library_identifier = BLI_strdup(weak_ref->asset_library_identifier);
  weak_ref_copy->relative_asset_identifier = BLI_strdup(weak_ref->relative_asset_identifier);

  return weak_ref_copy;
}

std::unique_ptr<AssetWeakReference> AssetWeakReference::make_reference(
    const asset_system::AssetLibrary &library,
    const asset_system::AssetIdentifier &asset_identifier)
{
  std::unique_ptr weak_ref = std::make_unique<AssetWeakReference>();

  weak_ref->asset_library_type = library.library_type();
  StringRefNull name = library.name();
  if (!name.is_empty()) {
    weak_ref->asset_library_identifier = BLI_strdupn(name.c_str(), name.size());
  }

  StringRefNull relative_identifier = asset_identifier.library_relative_identifier();
  weak_ref->relative_asset_identifier = BLI_strdupn(relative_identifier.c_str(),
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
  BLO_read_data_address(reader, &weak_ref->asset_library_identifier);
  BLO_read_data_address(reader, &weak_ref->relative_asset_identifier);
}
