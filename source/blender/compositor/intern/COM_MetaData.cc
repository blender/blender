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
 *
 * Copyright 2021, Blender Foundation.
 */

#include "COM_MetaData.h"

#include "BKE_image.h"

#include "RE_pipeline.h"

#include <string_view>

void MetaData::add(const blender::StringRef key, const blender::StringRef value)
{
  entries_.add(key, value);
}

void MetaData::addCryptomatteEntry(const blender::StringRef layer_name,
                                   const blender::StringRefNull key,
                                   const blender::StringRef value)
{
  add(blender::bke::cryptomatte::BKE_cryptomatte_meta_data_key(layer_name, key), value);
}

/* Replace the hash neutral cryptomatte keys with hashed versions.
 *
 * When a conversion happens it will also add the cryptomatte name key with the given
 * `layer_name`.*/
void MetaData::replaceHashNeutralCryptomatteKeys(const blender::StringRef layer_name)
{
  std::string cryptomatte_hash = entries_.pop_default(META_DATA_KEY_CRYPTOMATTE_HASH, "");
  std::string cryptomatte_conversion = entries_.pop_default(META_DATA_KEY_CRYPTOMATTE_CONVERSION,
                                                            "");
  std::string cryptomatte_manifest = entries_.pop_default(META_DATA_KEY_CRYPTOMATTE_MANIFEST, "");

  if (cryptomatte_hash.length() || cryptomatte_conversion.length() ||
      cryptomatte_manifest.length()) {
    addCryptomatteEntry(layer_name, "name", layer_name);
  }
  if (cryptomatte_hash.length()) {
    addCryptomatteEntry(layer_name, "hash", cryptomatte_hash);
  }
  if (cryptomatte_conversion.length()) {
    addCryptomatteEntry(layer_name, "conversion", cryptomatte_conversion);
  }
  if (cryptomatte_manifest.length()) {
    addCryptomatteEntry(layer_name, "manifest", cryptomatte_manifest);
  }
}

void MetaData::addToRenderResult(RenderResult *render_result) const
{
  for (blender::Map<std::string, std::string>::Item entry : entries_.items()) {
    BKE_render_result_stamp_data(render_result, entry.key.c_str(), entry.value.c_str());
  }
}

void MetaDataExtractCallbackData::addMetaData(blender::StringRef key, blender::StringRefNull value)
{
  if (!meta_data) {
    meta_data = std::make_unique<MetaData>();
  }
  meta_data->add(key, value);
}

void MetaDataExtractCallbackData::setCryptomatteKeys(blender::StringRef cryptomatte_layer_name)
{
  manifest_key = blender::bke::cryptomatte::BKE_cryptomatte_meta_data_key(cryptomatte_layer_name,
                                                                          "manifest");
  hash_key = blender::bke::cryptomatte::BKE_cryptomatte_meta_data_key(cryptomatte_layer_name,
                                                                      "hash");
  conversion_key = blender::bke::cryptomatte::BKE_cryptomatte_meta_data_key(cryptomatte_layer_name,
                                                                            "conversion");
}

void MetaDataExtractCallbackData::extract_cryptomatte_meta_data(void *_data,
                                                                const char *propname,
                                                                char *propvalue,
                                                                int UNUSED(len))
{
  MetaDataExtractCallbackData *data = static_cast<MetaDataExtractCallbackData *>(_data);
  blender::StringRefNull key(propname);
  if (key == data->hash_key) {
    data->addMetaData(META_DATA_KEY_CRYPTOMATTE_HASH, propvalue);
  }
  else if (key == data->conversion_key) {
    data->addMetaData(META_DATA_KEY_CRYPTOMATTE_CONVERSION, propvalue);
  }
  else if (key == data->manifest_key) {
    data->addMetaData(META_DATA_KEY_CRYPTOMATTE_MANIFEST, propvalue);
  }
}