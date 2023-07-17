/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Currently just checks if a blend file can be trusted to autoexec,
 * may add signing here later.
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_userdef_types.h"

#include "BLI_fnmatch.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#ifdef WIN32
#  include "BLI_string.h"
#endif

#include "BKE_autoexec.h" /* own include */

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
