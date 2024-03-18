/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_tempfile.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

bool BLI_temp_directory_path_copy_if_valid(char *temp_directory,
                                           const size_t buffer_size,
                                           const char *dirpath)
{
  if (dirpath == NULL) {
    return false;
  }

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

  BLI_strncpy(temp_directory, dirpath, buffer_size);

  /* Add a trailing slash if needed. */
  BLI_path_slash_ensure(temp_directory, buffer_size);

  /* There's nothing preventing an environment variable (even preferences) from being CWD relative.
   * This causes:
   * - Asserts in code-paths which expect absolute paths (blend-file IO).
   * - The temporary directory to change if the CWD changes.
   * Avoid issues by ensuring the temporary directory is *never* CWD relative. */
  BLI_path_abs_from_cwd(temp_directory, buffer_size);

  return true;
}

void BLI_temp_directory_path_get(char *temp_directory, const size_t buffer_size)
{
  temp_directory[0] = '\0';

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
    if (BLI_temp_directory_path_copy_if_valid(
            temp_directory, buffer_size, BLI_getenv(env_vars[i])))
    {
      break;
    }
  }

  if (temp_directory[0] == '\0') {
    BLI_strncpy(temp_directory, "/tmp/", buffer_size);
  }

  BLI_dir_create_recursive(temp_directory);
}
