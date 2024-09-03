/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>
#include <sstream>
#include <xxhash.h>

namespace blender::bke {

template<typename... Args> inline std::string hash_to_anonymous_attribute_name(Args &&...args)
{
  std::stringstream ss;
  ((ss << args), ...);
  const std::string long_name = ss.str();
  const XXH128_hash_t hash = XXH3_128bits(long_name.c_str(), long_name.size());
  return fmt::format(".a_{:x}{:x}", hash.low64, hash.high64);
}

}  // namespace blender::bke
