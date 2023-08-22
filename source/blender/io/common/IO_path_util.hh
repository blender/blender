/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "IO_path_util_types.h"

namespace blender::io {

/**
 * Return a filepath relative to a destination directory, for use with
 * exporters.
 *
 * When PATH_REFERENCE_COPY mode is used, the file path pair (source
 * path, destination path) is added to the `copy_set`.
 *
 * Equivalent of bpy_extras.io_utils.path_reference.
 */
std::string path_reference(StringRefNull filepath,
                           StringRefNull base_src,
                           StringRefNull base_dst,
                           ePathReferenceMode mode,
                           Set<std::pair<std::string, std::string>> *copy_set = nullptr);

/** Execute copying files of path_reference. */
void path_reference_copy(const Set<std::pair<std::string, std::string>> &copy_set);

}  // namespace blender::io
