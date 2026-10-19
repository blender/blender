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

#include "BLI_fnmatch.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_autoexec.hh" /* own include */
#include "BKE_global.hh"

bool BKE_autoexec_match(const char *path)
{
  bPathCompare *path_cmp;

#ifdef WIN32
  const int fnmatch_flags = FNM_CASEFOLD;
#else
  const int fnmatch_flags = 0;
#endif

  BLI_assert((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0);

  for (path_cmp = static_cast<bPathCompare *>(U.autoexec_paths.first); path_cmp;
       path_cmp = path_cmp->next)
  {
    if (path_cmp->path[0] == '\0') {
      /* pass */
    }
    else if (path_cmp->flag & USER_PATHCMP_GLOB) {
      if (fnmatch(path_cmp->path, path, fnmatch_flags) == 0) {
        return true;
      }
    }
    else if (BLI_path_ncmp(path_cmp->path, path, strlen(path_cmp->path)) == 0) {
      return true;
    }
  }

  return false;
}

bool BKE_autoexec_default_trust_source(const char *filepath)
{
  if (G.autoexec_override.has_value()) {
    /* The command line forced auto-execution on or off, excluded paths don't apply. */
    return *G.autoexec_override;
  }
  if (U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) {
    /* Nothing is trusted by default, the user may still opt-in. */
    return false;
  }

  char dirpath[FILE_MAX];
  STRNCPY(dirpath, filepath);
  BLI_path_canonicalize_native(dirpath, sizeof(dirpath));
  /* Truncating at the file name leaves the trailing slash. */
  *const_cast<char *>(BLI_path_basename(dirpath)) = '\0';

  return !BKE_autoexec_match(dirpath);
}
