/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_hash_mm2a.hh"

/* NOTE: Reference results are taken from reference implementation
 * (C++ code, CMurmurHash2A variant):
 * https://smhasher.googlecode.com/svn-history/r130/trunk/MurmurHash2.cpp
 */

TEST(hash_mm2a, MM2ABasic)
{
  BLI_HashMurmur2A mm2;

  const char *data = "Blender";

  BLI_hash_mm2a_init(&mm2, 0);
  BLI_hash_mm2a_add(&mm2, (const uchar *)data, strlen(data));
  /* NOTE: this is endianness-sensitive. */
  /* On BE systems, the expected value would be 959283772. */
  EXPECT_EQ(BLI_hash_mm2a_end(&mm2), 1633988145);
}

TEST(hash_mm2a, MM2AConcatenateStrings)
{
  BLI_HashMurmur2A mm2;
  uint32_t hash;

  const char *data1 = "Blender";
  const char *data2 = " is ";
  const char *data3 = "FaNtAsTiC";
  const char *data123 = "Blender is FaNtAsTiC";

  BLI_hash_mm2a_init(&mm2, 0);
  BLI_hash_mm2a_add(&mm2, (const uchar *)data1, strlen(data1));
  BLI_hash_mm2a_add(&mm2, (const uchar *)data2, strlen(data2));
  BLI_hash_mm2a_add(&mm2, (const uchar *)data3, strlen(data3));
  hash = BLI_hash_mm2a_end(&mm2);
  BLI_hash_mm2a_init(&mm2, 0);
  BLI_hash_mm2a_add(&mm2, (const uchar *)data123, strlen(data123));
  /* NOTE: this is endianness-sensitive. */
  /* On BE systems, the expected value would be 2604964730. */
  EXPECT_EQ(hash, 1545105348);
  EXPECT_EQ(BLI_hash_mm2a_end(&mm2), hash);
}

TEST(hash_mm2a, MM2AIntegers)
{
  BLI_HashMurmur2A mm2;
  uint32_t hash;

  const int ints[4] = {1, 2, 3, 4};

  BLI_hash_mm2a_init(&mm2, 0);
  BLI_hash_mm2a_add_int(&mm2, ints[0]);
  BLI_hash_mm2a_add_int(&mm2, ints[1]);
  BLI_hash_mm2a_add_int(&mm2, ints[2]);
  BLI_hash_mm2a_add_int(&mm2, ints[3]);
  hash = BLI_hash_mm2a_end(&mm2);
  BLI_hash_mm2a_init(&mm2, 0);
  BLI_hash_mm2a_add(&mm2, (const uchar *)ints, sizeof(ints));
  /* NOTE: this is endianness-sensitive. */
  /* Actually, same hash here on little and big endian. */
  EXPECT_EQ(hash, 405493096);
  EXPECT_EQ(BLI_hash_mm2a_end(&mm2), hash);
}
