/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "BLI_hash_mm2a.h"
}

/* Note: Reference results are taken from reference implementation (cpp code, CMurmurHash2A variant):
 *       https://smhasher.googlecode.com/svn-history/r130/trunk/MurmurHash2.cpp
 */

TEST(hash_mm2a, MM2ABasic)
{
	BLI_HashMurmur2A mm2;

	const char *data = "Blender";

	BLI_hash_mm2a_init(&mm2, 0);
	BLI_hash_mm2a_add(&mm2, (const unsigned char *)data, strlen(data));
#ifdef __LITTLE_ENDIAN__
	EXPECT_EQ(1633988145, BLI_hash_mm2a_end(&mm2));
#else
	EXPECT_EQ(959283772, BLI_hash_mm2a_end(&mm2));
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
	BLI_hash_mm2a_add(&mm2, (const unsigned char *)data1, strlen(data1));
	BLI_hash_mm2a_add(&mm2, (const unsigned char *)data2, strlen(data2));
	BLI_hash_mm2a_add(&mm2, (const unsigned char *)data3, strlen(data3));
	hash = BLI_hash_mm2a_end(&mm2);
	BLI_hash_mm2a_init(&mm2, 0);
	BLI_hash_mm2a_add(&mm2, (const unsigned char *)data123, strlen(data123));
#ifdef __LITTLE_ENDIAN__
	EXPECT_EQ(1545105348, hash);
#else
	EXPECT_EQ(2604964730, hash);
#endif
	EXPECT_EQ(hash, BLI_hash_mm2a_end(&mm2));
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
	BLI_hash_mm2a_add(&mm2, (const unsigned char *)ints, sizeof(ints));
	/* Yes, same hash here on little and big endian. */
#ifdef __LITTLE_ENDIAN__
	EXPECT_EQ(405493096, hash);
#else
	EXPECT_EQ(405493096, hash);
#endif
	EXPECT_EQ(hash, BLI_hash_mm2a_end(&mm2));
}
