/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_representation.hh"

#include "BKE_idtype.h"

#include "BLI_listbase.h"

#include "DNA_asset_types.h"

#include "ED_asset_filter.hh"

using namespace blender;

bool ED_asset_filter_matches_asset(const AssetFilterSettings *filter,
                                   const asset_system::AssetRepresentation &asset)
{
  ID_Type asset_type = asset.get_id_type();
  uint64_t asset_id_filter = BKE_idtype_idcode_to_idfilter(asset_type);

  if (filter->id_types && (filter->id_types & asset_id_filter) == 0) {
    return false;
  }
  /* Not very efficient (O(n^2)), could be improved quite a bit. */
  LISTBASE_FOREACH (const AssetTag *, filter_tag, &filter->tags) {
    AssetMetaData &asset_data = asset.get_metadata();

    AssetTag *matched_tag = (AssetTag *)BLI_findstring(
        &asset_data.tags, filter_tag->name, offsetof(AssetTag, name));
    if (matched_tag == nullptr) {
      return false;
    }
  }

  /* Successfully passed through all filters. */
  return true;
}
