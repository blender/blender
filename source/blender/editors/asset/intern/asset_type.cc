/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  return ELEM(GS(id->name), ID_MA, ID_GR, ID_OB, ID_AC, ID_WO, ID_NT);
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
