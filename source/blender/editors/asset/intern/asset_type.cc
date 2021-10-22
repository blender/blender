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

#include "BLI_utildefines.h"

#include "DNA_userdef_types.h"

#include "BKE_lib_id.h"

#include "ED_asset_type.h"

bool ED_asset_type_id_is_non_experimental(const ID *id)
{
  /* Remember to update #ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_UI_STRING and
   * #ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_FLAGS() with this! */
  return ELEM(GS(id->name), ID_MA, ID_AC, ID_WO);
}

bool ED_asset_type_is_supported(const ID *id)
{
  if (!BKE_id_can_be_asset(id)) {
    return false;
  }

  if (U.experimental.use_extended_asset_browser) {
    /* The "Extended Asset Browser" experimental feature flag enables all asset types that can
     * technically be assets. */
    return true;
  }

  return ED_asset_type_id_is_non_experimental(id);
}

int64_t ED_asset_types_supported_as_filter_flags()
{
  if (U.experimental.use_extended_asset_browser) {
    return FILTER_ID_ALL;
  }

  return ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_FLAGS;
}
