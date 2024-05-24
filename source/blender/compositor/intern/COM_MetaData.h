/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BKE_cryptomatte.hh"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"

#include "MEM_guardedalloc.h"

/* Forward declarations. */
struct RenderResult;

namespace blender::compositor {

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
  Map<std::string, std::string> entries_;
  void add_cryptomatte_entry(const blender::StringRef layer_name,
                             const blender::StringRefNull key,
                             const blender::StringRef value);

 public:
  /* The pixels in the result represents data, which is not to be color-managed. */
  bool is_data = false;

  void add(const blender::StringRef key, const blender::StringRef value);
  /**
   * Replace the hash neutral cryptomatte keys with hashed versions.
   *
   * When a conversion happens it will also add the cryptomatte name key with the given
   * `layer_name`.
   */
  void replace_hash_neutral_cryptomatte_keys(const blender::StringRef layer_name);
  void add_to_render_result(RenderResult *render_result) const;

  /* Invokes the given callback on each entry of the meta data. */
  void for_each_entry(FunctionRef<void(const std::string &, const std::string &)> callback) const;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:MetaData")
#endif
};

struct MetaDataExtractCallbackData {
  std::unique_ptr<MetaData> meta_data;
  std::string hash_key;
  std::string conversion_key;
  std::string manifest_key;

  void add_meta_data(blender::StringRef key, blender::StringRefNull value);
  void set_cryptomatte_keys(blender::StringRef cryptomatte_layer_name);
  /* C type callback function (StampCallback). */
  static void extract_cryptomatte_meta_data(void *_data,
                                            const char *propname,
                                            char *propvalue,
                                            int /*len*/);
};

}  // namespace blender::compositor
