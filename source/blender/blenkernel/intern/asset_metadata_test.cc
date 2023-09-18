/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_asset.h"

#include "BLI_uuid.h"

#include "DNA_asset_types.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(AssetMetadataTest, set_catalog_id)
{
  AssetMetaData meta{};
  const bUUID uuid = BLI_uuid_generate_random();

  /* Test trivial values. */
  BKE_asset_metadata_catalog_id_clear(&meta);
  EXPECT_TRUE(BLI_uuid_is_nil(meta.catalog_id));
  EXPECT_STREQ("", meta.catalog_simple_name);

  /* Test simple situation where the given short name is used as-is. */
  BKE_asset_metadata_catalog_id_set(&meta, uuid, "simple");
  EXPECT_TRUE(BLI_uuid_equal(uuid, meta.catalog_id));
  EXPECT_STREQ("simple", meta.catalog_simple_name);

  /* Test white-space trimming. */
  BKE_asset_metadata_catalog_id_set(&meta, uuid, " Govoriš angleško?    ");
  EXPECT_STREQ("Govoriš angleško?", meta.catalog_simple_name);

  /* Test length trimming to 63 chars + terminating zero. */
  constexpr char len66[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20";
  constexpr char len63[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1";
  BKE_asset_metadata_catalog_id_set(&meta, uuid, len66);
  EXPECT_STREQ(len63, meta.catalog_simple_name);

  /* Test length trimming happens after white-space trimming. */
  constexpr char len68[] =
      "     \
      000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20 ";
  BKE_asset_metadata_catalog_id_set(&meta, uuid, len68);
  EXPECT_STREQ(len63, meta.catalog_simple_name);

  /* Test length trimming to 63 bytes, and not 63 characters. ✓ in UTF-8 is three bytes long. */
  constexpr char with_utf8[] =
      "00010203040506✓0708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20";
  BKE_asset_metadata_catalog_id_set(&meta, uuid, with_utf8);
  EXPECT_STREQ("00010203040506✓0708090a0b0c0d0e0f101112131415161718191a1b1c1d",
               meta.catalog_simple_name);
}

}  // namespace blender::bke::tests
