/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "AS_remote_library.hh"

namespace blender::asset_system::tests {

TEST(RemoteLibraryTest, url_ends_with_top_meta_file_name)
{
  EXPECT_FALSE(remote_library_url_ends_with_top_meta_file_name(""));
  EXPECT_FALSE(remote_library_url_ends_with_top_meta_file_name("_asset-library-meta.json"));

  EXPECT_TRUE(remote_library_url_ends_with_top_meta_file_name(
      "https://example.com/_asset-library-meta.json"));

  EXPECT_FALSE(remote_library_url_ends_with_top_meta_file_name("https://example.com/"));
  EXPECT_FALSE(remote_library_url_ends_with_top_meta_file_name("https://example.com/abc"));
  EXPECT_FALSE(remote_library_url_ends_with_top_meta_file_name("https://example.com/abc/"));
  EXPECT_FALSE(remote_library_url_ends_with_top_meta_file_name(
      "https://example.com/_asset-library-meta.json/"));
  /* Missing slash. */
  EXPECT_FALSE(remote_library_url_ends_with_top_meta_file_name(
      "https://example.com_asset-library-meta.json"));
}

}  // namespace blender::asset_system::tests
