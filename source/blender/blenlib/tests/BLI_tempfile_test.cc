/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_tempfile.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "testing/testing.h"

namespace blender::tests {

TEST(BLI_tempfile, BLI_temp_directory_path_get)
{
  char temp_dir[FILE_MAX];
  BLI_temp_directory_path_get(temp_dir, sizeof(temp_dir));

  ASSERT_STRNE(temp_dir, "");

  EXPECT_EQ(temp_dir[strlen(temp_dir) - 1], SEP);

  EXPECT_TRUE(BLI_exists(temp_dir));
  EXPECT_TRUE(BLI_is_dir(temp_dir));

  EXPECT_TRUE(BLI_path_is_abs_from_cwd(temp_dir));
}

}  // namespace blender::tests
