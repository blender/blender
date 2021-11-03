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
 * \ingroup bke
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#include "DNA_asset_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AssetLibraryReference;
struct AssetMetaData;
struct BlendDataReader;
struct BlendWriter;
struct ID;
struct IDProperty;
struct PreviewImage;

typedef void (*PreSaveFn)(void *asset_ptr, struct AssetMetaData *asset_data);

typedef struct AssetTypeInfo {
  /**
   * For local assets (assets in the current .blend file), a callback to execute before the file is
   * saved.
   */
  PreSaveFn pre_save_fn;
} AssetTypeInfo;

struct AssetMetaData *BKE_asset_metadata_create(void);
void BKE_asset_metadata_free(struct AssetMetaData **asset_data);

struct AssetTagEnsureResult {
  struct AssetTag *tag;
  /* Set to false if a tag of this name was already present. */
  bool is_new;
};

struct AssetTag *BKE_asset_metadata_tag_add(struct AssetMetaData *asset_data, const char *name);
struct AssetTagEnsureResult BKE_asset_metadata_tag_ensure(struct AssetMetaData *asset_data,
                                                          const char *name);
void BKE_asset_metadata_tag_remove(struct AssetMetaData *asset_data, struct AssetTag *tag);

/** Clean up the catalog ID (white-spaces removed, length reduced, etc.) and assign it. */
void BKE_asset_metadata_catalog_id_clear(struct AssetMetaData *asset_data);
void BKE_asset_metadata_catalog_id_set(struct AssetMetaData *asset_data,
                                       bUUID catalog_id,
                                       const char *catalog_simple_name);

void BKE_asset_library_reference_init_default(struct AssetLibraryReference *library_ref);

void BKE_asset_metadata_idprop_ensure(struct AssetMetaData *asset_data, struct IDProperty *prop);
struct IDProperty *BKE_asset_metadata_idprop_find(const struct AssetMetaData *asset_data,
                                                  const char *name) ATTR_WARN_UNUSED_RESULT;

struct PreviewImage *BKE_asset_metadata_preview_get_from_id(const struct AssetMetaData *asset_data,
                                                            const struct ID *owner_id);

void BKE_asset_metadata_write(struct BlendWriter *writer, struct AssetMetaData *asset_data);
void BKE_asset_metadata_read(struct BlendDataReader *reader, struct AssetMetaData *asset_data);

#ifdef __cplusplus
}
#endif
