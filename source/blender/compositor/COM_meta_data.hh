/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string>

#pragma once

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Cryptomatte Meta Data
 *
 * Stores the Cryptomatte meta data as specified in Section 1 "Metadata" in the Cryptomatte
 * specification. The Cryptomatte layer name is not stored because it is determined by the user
 * when saving the result to file. */
struct CryptomatteMetaData {
  std::string hash;
  std::string conversion;
  std::string manifest;
};

/* ------------------------------------------------------------------------------------------------
 * Meta Data
 *
 * Stores extra information about results such as image meta data that can eventually be saved to
 * file. */
struct MetaData {
  /* The result stores non color data, which is not to be color-managed. */
  bool is_non_color_data = false;
  /* The result stores a 4D vector as opposed to a 3D vector. This is the case for things like
   * velocity passes, and we need to mark them as 4D in order to write them to file correctly. This
   * field can be ignored for results that are not of type Vector. */
  bool is_4d_vector = false;
  /* Stores Cryptomatte meta data. This will only be initialized for results that represent
   * Cryptomatte information. See the CryptomatteMetaData structure for more information. */
  CryptomatteMetaData cryptomatte;

  /* Identifies if the result represents a Cryptomatte layer. This is identified based on whether
   * the Cryptomatte meta data are initialized. */
  bool is_cryptomatte_layer() const;
};

}  // namespace blender::compositor
