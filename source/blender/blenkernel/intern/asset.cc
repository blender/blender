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

#include <cstring>

#include "DNA_ID.h"
#include "DNA_defaults.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.h"
#include "BLI_uuid.h"

#include "BKE_asset.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"

#include "BLO_read_write.h"

#include "MEM_guardedalloc.h"

using namespace blender;

AssetMetaData *BKE_asset_metadata_create(void)
{
  AssetMetaData *asset_data = (AssetMetaData *)MEM_callocN(sizeof(*asset_data), __func__);
  memcpy(asset_data, DNA_struct_default_get(AssetMetaData), sizeof(*asset_data));
  return asset_data;
}

void BKE_asset_metadata_free(AssetMetaData **asset_data)
{
  if ((*asset_data)->properties) {
    IDP_FreeProperty((*asset_data)->properties);
  }
  MEM_SAFE_FREE((*asset_data)->author);
  MEM_SAFE_FREE((*asset_data)->description);
  BLI_freelistN(&(*asset_data)->tags);

  MEM_SAFE_FREE(*asset_data);
}

static AssetTag *asset_metadata_tag_add(AssetMetaData *asset_data, const char *const name)
{
  AssetTag *tag = (AssetTag *)MEM_callocN(sizeof(*tag), __func__);
  BLI_strncpy(tag->name, name, sizeof(tag->name));

  BLI_addtail(&asset_data->tags, tag);
  asset_data->tot_tags++;
  /* Invariant! */
  BLI_assert(BLI_listbase_count(&asset_data->tags) == asset_data->tot_tags);

  return tag;
}

AssetTag *BKE_asset_metadata_tag_add(AssetMetaData *asset_data, const char *name)
{
  AssetTag *tag = asset_metadata_tag_add(asset_data, name);
  BLI_uniquename(&asset_data->tags, tag, name, '.', offsetof(AssetTag, name), sizeof(tag->name));
  return tag;
}

/**
 * Make sure there is a tag with name \a name, create one if needed.
 */
struct AssetTagEnsureResult BKE_asset_metadata_tag_ensure(AssetMetaData *asset_data,
                                                          const char *name)
{
  struct AssetTagEnsureResult result = {nullptr};
  if (!name[0]) {
    return result;
  }

  AssetTag *tag = (AssetTag *)BLI_findstring(&asset_data->tags, name, offsetof(AssetTag, name));

  if (tag) {
    result.tag = tag;
    result.is_new = false;
    return result;
  }

  tag = asset_metadata_tag_add(asset_data, name);

  result.tag = tag;
  result.is_new = true;
  return result;
}

void BKE_asset_metadata_tag_remove(AssetMetaData *asset_data, AssetTag *tag)
{
  BLI_assert(BLI_findindex(&asset_data->tags, tag) >= 0);
  BLI_freelinkN(&asset_data->tags, tag);
  asset_data->tot_tags--;
  /* Invariant! */
  BLI_assert(BLI_listbase_count(&asset_data->tags) == asset_data->tot_tags);
}

void BKE_asset_library_reference_init_default(AssetLibraryReference *library_ref)
{
  memcpy(library_ref, DNA_struct_default_get(AssetLibraryReference), sizeof(*library_ref));
}

void BKE_asset_metadata_catalog_id_clear(struct AssetMetaData *asset_data)
{
  asset_data->catalog_id = BLI_uuid_nil();
  asset_data->catalog_simple_name[0] = '\0';
}

void BKE_asset_metadata_catalog_id_set(struct AssetMetaData *asset_data,
                                       const ::bUUID catalog_id,
                                       const char *catalog_simple_name)
{
  asset_data->catalog_id = catalog_id;

  constexpr size_t max_simple_name_length = sizeof(asset_data->catalog_simple_name);

  /* The substr() call is necessary to make copy() copy the first N characters (instead of refusing
   * to copy and producing an empty string). */
  StringRef trimmed_id =
      StringRef(catalog_simple_name).trim().substr(0, max_simple_name_length - 1);
  trimmed_id.copy(asset_data->catalog_simple_name, max_simple_name_length);
}

void BKE_asset_metadata_idprop_ensure(AssetMetaData *asset_data, IDProperty *prop)
{
  if (!asset_data->properties) {
    IDPropertyTemplate val = {0};
    asset_data->properties = IDP_New(IDP_GROUP, &val, "AssetMetaData.properties");
  }
  /* Important: The property may already exist. For now just allow always allow a newly allocated
   * property, and replace the existing one as a way of updating. */
  IDP_ReplaceInGroup(asset_data->properties, prop);
}

IDProperty *BKE_asset_metadata_idprop_find(const AssetMetaData *asset_data, const char *name)
{
  if (!asset_data->properties) {
    return nullptr;
  }
  return IDP_GetPropertyFromGroup(asset_data->properties, name);
}

/* Queries -------------------------------------------- */

PreviewImage *BKE_asset_metadata_preview_get_from_id(const AssetMetaData *UNUSED(asset_data),
                                                     const ID *id)
{
  return BKE_previewimg_id_get(id);
}

/* .blend file API -------------------------------------------- */

void BKE_asset_metadata_write(BlendWriter *writer, AssetMetaData *asset_data)
{
  BLO_write_struct(writer, AssetMetaData, asset_data);

  if (asset_data->properties) {
    IDP_BlendWrite(writer, asset_data->properties);
  }

  if (asset_data->author) {
    BLO_write_string(writer, asset_data->author);
  }
  if (asset_data->description) {
    BLO_write_string(writer, asset_data->description);
  }
  LISTBASE_FOREACH (AssetTag *, tag, &asset_data->tags) {
    BLO_write_struct(writer, AssetTag, tag);
  }
}

void BKE_asset_metadata_read(BlendDataReader *reader, AssetMetaData *asset_data)
{
  /* asset_data itself has been read already. */
  asset_data->local_type_info = nullptr;

  if (asset_data->properties) {
    BLO_read_data_address(reader, &asset_data->properties);
    IDP_BlendDataRead(reader, &asset_data->properties);
  }

  BLO_read_data_address(reader, &asset_data->author);
  BLO_read_data_address(reader, &asset_data->description);
  BLO_read_list(reader, &asset_data->tags);
  BLI_assert(BLI_listbase_count(&asset_data->tags) == asset_data->tot_tags);
}
