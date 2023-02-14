/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AssetMetaData;

/** C handle for #asset_system::AssetRepresentation. */
typedef struct AssetRepresentation AssetRepresentation;

const char *AS_asset_representation_name_get(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;
AssetMetaData *AS_asset_representation_metadata_get(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;
bool AS_asset_representation_is_local_id(const AssetRepresentation *asset) ATTR_WARN_UNUSED_RESULT;
bool AS_asset_representation_is_never_link(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif
