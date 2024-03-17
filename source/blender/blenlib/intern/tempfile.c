/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_tempfile.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

bool BLI_temp_directory_path_copy_if_valid(char *tempdir,
                                           const size_t tempdir_maxncpy,
                                           const char *dirpath)
{
  /* NOTE(@ideasman42): it is *not* the purpose of this function to check that
   * `dirpath` is writable under all circumstances.
   * Only check `dirpath` doesn't resolve to an empty string & points to a directory.
   *
   * While other checks could be added to avoid problems writing temporary files:
   * (read-only, permission failure, out-of-I-nodes, disk-full... etc)
   * it's out of scope for this function as these characteristics can change at run-time.
   * In general temporary file IO should handle failure properly with sufficient user feedback,
   * without attempting to *solve* the problem by anticipating file-system issues ahead of time. */

  /* Disallow paths starting with two forward slashes. While they are valid paths,
   * Blender interprets them as relative in situations relative paths aren't supported,
   * see #95411. */
  while (UNLIKELY(dirpath[0] == '/' && dirpath[1] == '/')) {
    dirpath++;
  }
  if (dirpath[0] == '\0') {
    return false;
  }
  if (!BLI_is_dir(dirpath)) {
    return false;
  }

  BLI_strncpy(tempdir, dirpath, tempdir_maxncpy);

  /* Add a trailing slash if needed. */
  BLI_path_slash_ensure(tempdir, tempdir_maxncpy);

  /* There's nothing preventing an environment variable (even preferences) from being CWD relative.
   * This causes:
   * - Asserts in code-paths which expect absolute paths (blend-file IO).
   * - The temporary directory to change if the CWD changes.
   * Avoid issues by ensuring the temporary directory is *never* CWD relative. */
  BLI_path_abs_from_cwd(tempdir, tempdir_maxncpy);

  return true;
}

void BLI_temp_directory_path_get(char *tempdir, const size_t tempdir_maxncpy)
{
  tempdir[0] = '\0';

  const char *env_vars[] = {
#ifdef WIN32
      "TEMP",
#else
      /* Non standard (could be removed). */
      "TMP",
      /* Posix standard. */
      "TMPDIR",
#endif
  };

  for (int i = 0; i < ARRAY_SIZE(env_vars); i++) {
    const char *tempdir_test = BLI_getenv(env_vars[i]);
    if (tempdir_test == NULL) {
      continue;
    }
    if (BLI_temp_directory_path_copy_if_valid(tempdir, tempdir_maxncpy, tempdir_test)) {
      break;
    }
  }

  if (tempdir[0] == '\0') {
    BLI_strncpy(tempdir, "/tmp/", tempdir_maxncpy);
  }
}
