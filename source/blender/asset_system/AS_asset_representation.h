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
struct AssetWeakReference;

/** C handle for #asset_system::AssetRepresentation. */
typedef struct AssetRepresentation AssetRepresentation;

const char *AS_asset_representation_name_get(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;
AssetMetaData *AS_asset_representation_metadata_get(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;
struct ID *AS_asset_representation_local_id_get(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;
bool AS_asset_representation_is_local_id(const AssetRepresentation *asset) ATTR_WARN_UNUSED_RESULT;
bool AS_asset_representation_is_never_link(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;

/**
 * C version of #AssetRepresentation::make_weak_reference. Returned pointer needs freeing with
 * #MEM_delete() or #BKE_asset_weak_reference_free().
 */
AssetWeakReference *AS_asset_representation_weak_reference_create(const AssetRepresentation *asset)
    ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif
