/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "AS_essentials_library.hh"

namespace blender::asset_system::tests {

TEST(EssentialsLibraryTest, is_online_essentials_url)
{
  EXPECT_FALSE(is_online_essentials_url(""));
  EXPECT_FALSE(is_online_essentials_url("https://www.blender.org/asset-library/"));

  EXPECT_TRUE(
      is_online_essentials_url("https://cdn.extensions.blender.org/asset-libraries/essentials/"));
  EXPECT_TRUE(is_online_essentials_url(
      "https://cdn.extensions.blender.org/asset-libraries/essentials/_asset-library-meta.json"));

  EXPECT_FALSE(
      is_online_essentials_url("https://cdn.extensions.blender.org/asset-libraries/essentials"));
  EXPECT_FALSE(is_online_essentials_url(
      "https://cdn.extensions.blender.org/asset-libraries/essentials_asset-library-meta.json"));

  /* http instead of https. */
  EXPECT_FALSE(
      is_online_essentials_url("http://cdn.extensions.blender.org/asset-libraries/essentials/"));
}

}  // namespace blender::asset_system::tests
