/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sets `tempdir` from `dirpath` when it's a valid directory.
 * Simple sanitize operations are performed and a trailing slash is ensured.
 */
bool BLI_temp_directory_path_copy_if_valid(char *tempdir,
                                           const size_t tempdir_maxncpy,
                                           const char *dirpath) ATTR_NONNULL(1, 3);

/**
 * Get the path to a directory suitable for temporary files.
 *
 * The return path is guaranteed to exist and to be a directory, as well as to contain a trailing
 * directory separator.
 *
 * At maximum the `tempdir_maxncpy` number of characters is written to the `tempdir`.
 * The directory path is always null-terminated.
 */
void BLI_temp_directory_path_get(char *tempdir, const size_t tempdir_maxncpy) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
