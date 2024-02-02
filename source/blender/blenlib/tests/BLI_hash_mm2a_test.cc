/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_hash_mm2a.hh"

/* NOTE: Reference results are taken from reference implementation
 * (cpp code, CMurmurHash2A variant):
 * https://smhasher.googlecode.com/svn-history/r130/trunk/MurmurHash2.cpp
 */

TEST(hash_mm2a, MM2ABasic)
{
  BLI_HashMurmur2A mm2;

  const char *data = "Blender";

  BLI_hash_mm2a_init(&mm2, 0);
  BLI_hash_mm2a_add(&mm2, (const uchar *)data, strlen(data));
#ifdef __LITTLE_ENDIAN__
  EXPECT_EQ(BLI_hash_mm2a_end(&mm2), 1633988145);
#else
  EXPECT_EQ(BLI_hash_mm2a_end(&mm2), 959283772);
#endif
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
#ifdef __LITTLE_ENDIAN__
  EXPECT_EQ(hash, 1545105348);
#else
  EXPECT_EQ(hash, 2604964730);
#endif
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
  /* Yes, same hash here on little and big endian. */
#ifdef __LITTLE_ENDIAN__
  EXPECT_EQ(hash, 405493096);
#else
  EXPECT_EQ(hash, 405493096);
#endif
  EXPECT_EQ(BLI_hash_mm2a_end(&mm2), hash);
}
