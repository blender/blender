/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Currently just checks if a blend file can be trusted to autoexec,
 * may add signing here later.
 */

#include <cstdlib>
#include <cstring>

#include "DNA_userdef_types.h"

#include "BLI_fnmatch.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BKE_autoexec.hh" /* own include */
#include "BKE_global.hh"

namespace blender {

/**
 * \return `path` when there is nothing to do, otherwise `dirpath`.
 */
static const char *autoexec_dirpath_resolve(const char *path,
                                            const bool canonicalize,
                                            const bool strip_filename,
                                            char *dirpath,
                                            const size_t dirpath_maxncpy)
{
  if (!(canonicalize || strip_filename)) {
    return path;
  }

  const size_t path_len = BLI_strncpy_rlen(dirpath, path, dirpath_maxncpy);
  /* A path ending with a slash is already a directory & has no file name to strip,
   * canonicalizing removes the slash so this must be checked first. */
  const bool is_dirpath = path_len && BLI_path_slash_is_native_compat(dirpath[path_len - 1]);
  const bool do_strip_filename = strip_filename && !is_dirpath;

  if (canonicalize) {
    BLI_path_canonicalize_native(dirpath, dirpath_maxncpy);
    if (!do_strip_filename) {
      /* Canonicalizing removed the trailing slash. */
      BLI_path_slash_ensure(dirpath, dirpath_maxncpy);
    }
  }
  if (do_strip_filename) {
    /* Truncating at the file name leaves the trailing slash. */
    *const_cast<char *>(BLI_path_basename(dirpath)) = '\0';
  }
  return dirpath;
}

bool BKE_autoexec_match_unchecked(const char *path,
                                  const bool canonicalize,
                                  const bool strip_filename)
{
  bPathCompare *path_cmp;

  char dirpath_buf[FILE_MAX];
  const char *dirpath = autoexec_dirpath_resolve(
      path, canonicalize, strip_filename, dirpath_buf, sizeof(dirpath_buf));

#ifdef WIN32
  const int fnmatch_flags = FNM_CASEFOLD;
#else
  const int fnmatch_flags = 0;
#endif

  for (path_cmp = U.autoexec_paths.first(); path_cmp; path_cmp = path_cmp->next) {
    if (path_cmp->path[0] == '\0') {
      /* pass */
    }
    else if (path_cmp->flag & USER_PATHCMP_GLOB) {
      if (fnmatch(path_cmp->path, dirpath, fnmatch_flags) == 0) {
        return true;
      }
    }
    else if (BLI_path_ncmp(path_cmp->path, dirpath, strlen(path_cmp->path)) == 0) {
      return true;
    }
  }

  return false;
}

bool BKE_autoexec_match(const char *path, const bool canonicalize, const bool strip_filename)
{
  /* Auto-execution must be enabled by the preference or trusted for this session. */
  BLI_assert((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0 || (G.f & G_FLAG_SCRIPT_AUTOEXEC));

  return BKE_autoexec_match_unchecked(path, canonicalize, strip_filename);
}

}  // namespace blender
