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

#ifndef __BKE_ASSET_H__
#define __BKE_ASSET_H__

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendWriter;
struct ID;
struct PreviewImage;

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

struct PreviewImage *BKE_asset_metadata_preview_get_from_id(const struct AssetMetaData *asset_data,
                                                            const struct ID *owner_id);

void BKE_asset_metadata_write(struct BlendWriter *writer, struct AssetMetaData *asset_data);
void BKE_asset_metadata_read(struct BlendDataReader *reader, struct AssetMetaData *asset_data);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_ASSET_H__ */
