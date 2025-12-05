/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once
#include <optional>

/* Forward references. */
namespace blender::asset_system {
struct URLWithHash;
}
namespace blender::io::serialize {
class DictionaryValue;
}

namespace blender::ed::asset::index {

/**
 * Parse a dictionary `{url: "https://some.url/", hash: "sha256:abcd"}` into a
 * URLWithHash object.
 *
 * If `url_with_hash_dict` is `nullptr`, or has no "url" field, `std::nullopt`
 * is returned.
 *
 * If the "hash" field is missing, it will simply be set to an empty string on
 * the returned URLWithHash.
 */
std::optional<asset_system::URLWithHash> parse_url_with_hash_dict(
    const io::serialize::DictionaryValue *url_with_hash_dict);

}  // namespace blender::ed::asset::index
