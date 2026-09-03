/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

namespace blender {

/**
 * \param path: The path to check against.
 * \param canonicalize: Resolve `path` first, unless the caller knows it's resolved.
 * \param strip_filename: Strip the file name from `path`, otherwise it's a directory already.
 * \return Success
 */
bool BKE_autoexec_match(const char *path, bool canonicalize, bool strip_filename);

/**
 * Version of #BKE_autoexec_match which may be called
 * when auto-execution isn't enabled, used by the Python API.
 */
bool BKE_autoexec_match_unchecked(const char *path, bool canonicalize, bool strip_filename);

}  // namespace blender
