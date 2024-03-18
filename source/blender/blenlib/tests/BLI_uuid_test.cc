/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"
#include <cstring>

#include "BLI_uuid.h"

namespace blender::tests {

TEST(BLI_uuid, generate_random)
{
  const bUUID uuid = BLI_uuid_generate_random();

  /* The 4 MSbits represent the "version" of the UUID. */
  const uint16_t version = uuid.time_hi_and_version >> 12;
  EXPECT_EQ(version, 4);

  /* The 2 MSbits should be 0b10, indicating compliance with RFC4122. */
  const uint8_t reserved = uuid.clock_seq_hi_and_reserved >> 6;
  EXPECT_EQ(reserved, 0b10);
}

TEST(BLI_uuid, generate_many_random)
{
  const bUUID first_uuid = BLI_uuid_generate_random();

  /* Generate lots of UUIDs to get some indication that the randomness is okay. */
  for (int i = 0; i < 1000000; ++i) {
    const bUUID uuid = BLI_uuid_generate_random();
    EXPECT_NE(first_uuid, uuid);

    /* Check that the non-random bits are set according to RFC4122. */
    const uint16_t version = uuid.time_hi_and_version >> 12;
    EXPECT_EQ(version, 4);
    const uint8_t reserved = uuid.clock_seq_hi_and_reserved >> 6;
    EXPECT_EQ(reserved, 0b10);
  }
}

TEST(BLI_uuid, nil_value)
{
  const bUUID nil_uuid = BLI_uuid_nil();
  const bUUID zeroes_uuid{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const bUUID default_constructed{};

  EXPECT_EQ(nil_uuid, zeroes_uuid);
  EXPECT_TRUE(BLI_uuid_is_nil(nil_uuid));
  EXPECT_TRUE(BLI_uuid_is_nil(default_constructed))
      << "Default constructor should produce the nil value.";

  std::string buffer(36, '\0');
  BLI_uuid_format(buffer.data(), nil_uuid);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", buffer);
}

TEST(BLI_uuid, equality)
{
  const bUUID uuid1 = BLI_uuid_generate_random();
  const bUUID uuid2 = BLI_uuid_generate_random();

  EXPECT_EQ(uuid1, uuid1);
  EXPECT_NE(uuid1, uuid2);
}

TEST(BLI_uuid, comparison_trivial)
{
  const bUUID uuid0{};
  const bUUID uuid1("11111111-1111-1111-1111-111111111111");
  const bUUID uuid2("22222222-2222-2222-2222-222222222222");

  EXPECT_LT(uuid0, uuid1);
  EXPECT_LT(uuid0, uuid2);
  EXPECT_LT(uuid1, uuid2);
}

TEST(BLI_uuid, comparison_byte_order_check)
{
  const bUUID uuid0{};
  /* Chosen to test byte ordering is taken into account correctly when comparing. */
  const bUUID uuid12("12222222-2222-2222-2222-222222222222");
  const bUUID uuid21("21111111-1111-1111-1111-111111111111");

  EXPECT_LT(uuid0, uuid12);
  EXPECT_LT(uuid0, uuid21);
  EXPECT_LT(uuid12, uuid21);
}

TEST(BLI_uuid, string_formatting)
{
  bUUID uuid;
  std::string buffer(36, '\0');

  memset(&uuid, 0, sizeof(uuid));
  BLI_uuid_format(buffer.data(), uuid);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", buffer);

  /* Demo of where the bits end up in the formatted string. */
  uuid.time_low = 1;
  uuid.time_mid = 2;
  uuid.time_hi_and_version = 3;
  uuid.clock_seq_hi_and_reserved = 4;
  uuid.clock_seq_low = 5;
  uuid.node[0] = 6;
  uuid.node[5] = 7;
  BLI_uuid_format(buffer.data(), uuid);
  EXPECT_EQ("00000001-0002-0003-0405-060000000007", buffer);

  /* Somewhat more complex bit patterns. This is a version 1 UUID generated from Python. */
  const bUUID uuid1 = {3540651616, 5282, 4588, 139, 153, 0xf7, 0x73, 0x69, 0x44, 0xdb, 0x8b};
  BLI_uuid_format(buffer.data(), uuid1);
  EXPECT_EQ("d30a0e60-14a2-11ec-8b99-f7736944db8b", buffer);

  /* Namespace UUID, example listed in RFC4211. */
  const bUUID namespace_dns = {
      0x6ba7b810, 0x9dad, 0x11d1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8};
  BLI_uuid_format(buffer.data(), namespace_dns);
  EXPECT_EQ("6ba7b810-9dad-11d1-80b4-00c04fd430c8", buffer);
}

TEST(BLI_uuid, string_parsing_ok)
{
  bUUID uuid;
  std::string buffer(36, '\0');

  const bool parsed_ok = BLI_uuid_parse_string(&uuid, "d30a0e60-14a2-11ec-8b99-f7736944db8b");
  EXPECT_TRUE(parsed_ok);
  BLI_uuid_format(buffer.data(), uuid);
  EXPECT_EQ("d30a0e60-14a2-11ec-8b99-f7736944db8b", buffer);
}

TEST(BLI_uuid, string_parsing_capitalisation)
{
  bUUID uuid;
  std::string buffer(36, '\0');

  /* RFC4122 demands acceptance of upper-case hex digits. */
  const bool parsed_ok = BLI_uuid_parse_string(&uuid, "D30A0E60-14A2-11EC-8B99-F7736944DB8B");
  EXPECT_TRUE(parsed_ok);
  BLI_uuid_format(buffer.data(), uuid);

  /* Software should still output lower-case hex digits, though. */
  EXPECT_EQ("d30a0e60-14a2-11ec-8b99-f7736944db8b", buffer);
}

TEST(BLI_uuid, string_parsing_fail)
{
  bUUID uuid;
  std::string buffer(36, '\0');

  const bool parsed_ok = BLI_uuid_parse_string(&uuid, "d30a0e60!14a2-11ec-8b99-f7736944db8b");
  EXPECT_FALSE(parsed_ok);
}

TEST(BLI_uuid, stream_operator)
{
  std::stringstream ss;
  const bUUID uuid = {3540651616, 5282, 4588, 139, 153, 0xf7, 0x73, 0x69, 0x44, 0xdb, 0x8b};
  ss << uuid;
  EXPECT_EQ(ss.str(), "d30a0e60-14a2-11ec-8b99-f7736944db8b");
}

}  // namespace blender::tests
