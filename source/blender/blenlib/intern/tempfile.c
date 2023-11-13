/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_tempfile.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

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
    const char *tmp = BLI_getenv(env_vars[i]);
    if (tmp && (tmp[0] != '\0') && BLI_is_dir(tmp)) {
      BLI_strncpy(temp_directory, tmp, buffer_size);
      break;
    }
  }

  if (temp_directory[0] == '\0') {
    BLI_strncpy(temp_directory, "/tmp/", buffer_size);
  }
  else {
    /* Add a trailing slash if needed. */
    BLI_path_slash_ensure(temp_directory, buffer_size);
  }

  BLI_dir_create_recursive(temp_directory);
}
