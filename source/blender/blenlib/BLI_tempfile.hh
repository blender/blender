/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include "BLI_compiler_attrs.hh"
#include "BLI_span.hh"
#include "BLI_sys_types.hh"

namespace blender {

/**
 * Sets `tempdir` from `dirpath` when it's a valid directory.
 * Simple sanitize operations are performed and a trailing slash is ensured.
 */
bool BLI_temp_directory_path_copy_if_valid(MutableCStr tempdir, const char *dirpath)
    ATTR_NONNULL(2);

/**
 * Get the path to a directory suitable for temporary files.
 *
 * The return path is guaranteed to exist and to be a directory, as well as to contain a trailing
 * directory separator.
 *
 * At maximum `tempdir.size()` characters are written to `tempdir`.
 * The directory path is always null-terminated.
 */
void BLI_temp_directory_path_get(MutableCStr tempdir);

}  // namespace blender
