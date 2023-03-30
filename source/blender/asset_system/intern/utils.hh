/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "BLI_string_ref.hh"

namespace blender::asset_system::utils {

/**
 * Returns a normalized directory path with a trailing slash, and a maximum length of #PATH_MAX.
 * Slashes are not converted to native format (they probably should be though?).
 */
std::string normalize_directory_path(StringRef directory);

/**
 * Normalize the given `path` (remove 'parent directory' and double-slashes element etc., and
 * convert to native path separators).
 *
 * If \a max_len is not #StringRef::not_found (default value), only the first part of the given
 * string up to the given length is processed, the rest remains unchanged. Needed to avoid
 * modifying ID name part of linked library paths.
 */
std::string normalize_path(StringRefNull path, int64_t max_len = StringRef::not_found);

}  // namespace blender::asset_system::utils
