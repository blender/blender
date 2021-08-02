/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

#include "BKE_asset_catalog.hh"

#include "testing/testing.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace blender::bke::tests {

TEST(AssetCatalogTest, load_single_file)
{
  const fs::path test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  AssetCatalogService service;
  service.load_from_disk(test_files_dir / "asset_library/single_catalog_definition_file.cats.txt");

  // Test getting a non-existant catalog ID.
  EXPECT_EQ(nullptr, service.find_catalog("NONEXISTANT"));

  // Test getting a 7-bit ASCII catalog ID.
  AssetCatalog *poses_elly = service.find_catalog("POSES_ELLY");
  ASSERT_NE(nullptr, poses_elly);
  EXPECT_EQ("POSES_ELLY", poses_elly->catalog_id);
  EXPECT_EQ("character/Elly/poselib", poses_elly->path);

  // Test whitespace stripping.
  AssetCatalog *poses_whitespace = service.find_catalog("POSES_ELLY_WHITESPACE");
  ASSERT_NE(nullptr, poses_whitespace);
  EXPECT_EQ("POSES_ELLY_WHITESPACE", poses_whitespace->catalog_id);
  EXPECT_EQ("character/Elly/poselib/whitespace", poses_whitespace->path);

  // Test getting a UTF-8 catalog ID.
  AssetCatalog *poses_ruzena = service.find_catalog("POSES_RUŽENA");
  ASSERT_NE(nullptr, poses_ruzena);
  EXPECT_EQ("POSES_RUŽENA", poses_ruzena->catalog_id);
  EXPECT_EQ("character/Ružena/poselib", poses_ruzena->path);
}

}  // namespace blender::bke::tests
