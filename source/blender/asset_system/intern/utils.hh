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

}  // namespace blender::asset_system::utils
