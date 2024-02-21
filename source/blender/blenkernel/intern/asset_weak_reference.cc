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

AssetWeakReference &AssetWeakReference::operator=(AssetWeakReference &&other)
{
  if (&other == this) {
    return *this;
  }
  asset_library_type = other.asset_library_type;
  asset_library_identifier = other.asset_library_identifier;
  relative_asset_identifier = other.relative_asset_identifier;
  other.asset_library_type = 0; /* Not a valid type. */
  other.asset_library_identifier = nullptr;
  other.relative_asset_identifier = nullptr;
  return *this;
}

bool operator==(const AssetWeakReference &a, const AssetWeakReference &b)
{
  if (a.asset_library_type != b.asset_library_type) {
    return false;
  }
  if (!STREQ(a.asset_library_identifier, b.asset_library_identifier)) {
    return false;
  }
  if (!STREQ(a.relative_asset_identifier, b.relative_asset_identifier)) {
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
  BLO_read_data_address(reader, &weak_ref->asset_library_identifier);
  BLO_read_data_address(reader, &weak_ref->relative_asset_identifier);
}
