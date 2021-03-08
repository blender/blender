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

#pragma once

#include <string>

#include "BKE_cryptomatte.hh"
#include "BLI_map.hh"

#include "MEM_guardedalloc.h"

/* Forward declarations. */
struct RenderResult;

/* Cryptomatte includes hash in its meta data keys. The hash is generated from the render
 * layer/pass name. Compositing happens without the knowledge of the original layer and pass. The
 * next keys are used to transfer the cryptomatte meta data in a neutral way. The file output node
 * will generate a hash based on the layer name configured by the user.
 *
 * The `{hash}` has no special meaning except to make sure that the meta data stays unique. */
constexpr blender::StringRef META_DATA_KEY_CRYPTOMATTE_HASH("cryptomatte/{hash}/hash");
constexpr blender::StringRef META_DATA_KEY_CRYPTOMATTE_CONVERSION("cryptomatte/{hash}/conversion");
constexpr blender::StringRef META_DATA_KEY_CRYPTOMATTE_MANIFEST("cryptomatte/{hash}/manifest");
constexpr blender::StringRef META_DATA_KEY_CRYPTOMATTE_NAME("cryptomatte/{hash}/name");

class MetaData {
 private:
  blender::Map<std::string, std::string> entries_;
  void addCryptomatteEntry(const blender::StringRef layer_name,
                           const blender::StringRefNull key,
                           const blender::StringRef value);

 public:
  void add(const blender::StringRef key, const blender::StringRef value);
  void replaceHashNeutralCryptomatteKeys(const blender::StringRef layer_name);
  void addToRenderResult(RenderResult *render_result) const;
#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:MetaData")
#endif
};

struct MetaDataExtractCallbackData {
  std::unique_ptr<MetaData> meta_data;
  std::string hash_key;
  std::string conversion_key;
  std::string manifest_key;

  void addMetaData(blender::StringRef key, blender::StringRefNull value);
  void setCryptomatteKeys(blender::StringRef cryptomatte_layer_name);
  /* C type callback function (StampCallback). */
  static void extract_cryptomatte_meta_data(void *_data,
                                            const char *propname,
                                            char *propvalue,
                                            int UNUSED(len));
};
