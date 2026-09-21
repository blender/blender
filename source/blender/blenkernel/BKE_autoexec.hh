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
 * when auto-execution isn't enabled.
 */
bool BKE_autoexec_match_unchecked(const char *path, bool canonicalize, bool strip_filename);

struct AutoExec_Params {
  /**
   * Only check the excluded paths, skipping the command line override & the preference.
   * Otherwise the result is the default for "Trusted Source" when opening the path.
   */
  bool skip_overrides = false;
  /** Resolve the path first, unless the caller knows it's resolved. */
  bool canonicalize = false;
  /** Strip the file name from the path, otherwise it's a directory already. */
  bool strip_filename = false;
};

/**
 * \return True when blend-files in `path` are trusted to run scripts automatically.
 */
bool BKE_autoexec_default_trust_source(const char *path, const AutoExec_Params &params);

}  // namespace blender
