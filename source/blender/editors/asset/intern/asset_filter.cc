/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edasset
 */

#include "BKE_idtype.h"

#include "BLI_listbase.h"

#include "DNA_asset_types.h"

#include "ED_asset_filter.h"
#include "ED_asset_handle.h"

/**
 * Compare \a asset against the settings of \a filter.
 *
 * Individual filter parameters are ORed with the asset properties. That means:
 * * The asset type must be one of the ID types filtered by, and
 * * The asset must contain at least one of the tags filtered by.
 * However for an asset to be matching it must have one match in each of the parameters. I.e. one
 * matching type __and__ at least one matching tag.
 *
 * \returns True if the asset should be visible with these filter settings (parameters match).
 *          Otherwise returns false (mismatch).
 */
bool ED_asset_filter_matches_asset(const AssetFilterSettings *filter, const AssetHandle *asset)
{
  ID_Type asset_type = ED_asset_handle_get_id_type(asset);
  uint64_t asset_id_filter = BKE_idtype_idcode_to_idfilter(asset_type);

  if ((filter->id_types & asset_id_filter) == 0) {
    return false;
  }
  /* Not very efficient (O(n^2)), could be improved quite a bit. */
  LISTBASE_FOREACH (const AssetTag *, filter_tag, &filter->tags) {
    AssetMetaData *asset_data = ED_asset_handle_get_metadata(asset);

    AssetTag *matched_tag = (AssetTag *)BLI_findstring(
        &asset_data->tags, filter_tag->name, offsetof(AssetTag, name));
    if (matched_tag == nullptr) {
      return false;
    }
  }

  /* Successfully passed through all filters. */
  return true;
}
