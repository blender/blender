/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include <climits>
#include <cstdint>
#include <limits>

#include "spz_header.hh"

namespace blender::io::spz::tests {

/* -------------------------------------------------------------------- */
/** \name Common test logic
 * \{ */

template<class Header> Header make_default_valid_header();

template<class Header> static void run_magic_tests()
{
  auto make_header_with_magic = [](const uint32_t magic) {
    Header header = make_default_valid_header<Header>();
    header.magic = magic;
    return header;
  };

  EXPECT_EQ(check_header_for_errors(make_header_with_magic(0xDEADBEEF)),
            "Unexpected SPZ header magic 0xDEADBEEF");
}

template<class Header> static void run_num_points_tests()
{
  auto make_header_with_num_points = [](const uint32_t num_points) {
    Header header = make_default_valid_header<Header>();
    header.num_points = num_points;
    return header;
  };

  EXPECT_EQ(check_header_for_errors(make_header_with_num_points(0)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_num_points(uint32_t(INT_MAX))), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_num_points(uint32_t(INT_MAX) + 1)),
            "Too many points");
  EXPECT_EQ(check_header_for_errors(make_header_with_num_points(UINT32_MAX)), "Too many points");
}

template<class Header> static void run_sh_degrees_tests()
{
  auto make_header_with_sh_degree = [](const uint8_t sh_degree) {
    Header header = make_default_valid_header<Header>();
    header.sh_degree = sh_degree;
    return header;
  };

  EXPECT_EQ(check_header_for_errors(make_header_with_sh_degree(0)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_sh_degree(1)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_sh_degree(2)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_sh_degree(3)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_sh_degree(4)), std::nullopt);

  for (int sh_degree = 5; sh_degree <= std::numeric_limits<uint8_t>::max(); ++sh_degree) {
    EXPECT_EQ(check_header_for_errors(make_header_with_sh_degree(sh_degree)),
              "Unsupported SH degree " + std::to_string(sh_degree))
        << "Unexpected result for sh_degree=" << int(sh_degree);
  }
}

template<class Header> static void run_fractional_bits_tests()
{
  auto make_header_with_fractional_bits = [](const uint8_t fractional_bits) {
    Header header = make_default_valid_header<Header>();
    header.fractional_bits = fractional_bits;
    return header;
  };

  for (uint8_t bits = 0; bits < 24; ++bits) {
    EXPECT_EQ(check_header_for_errors(make_header_with_fractional_bits(bits)), std::nullopt)
        << "Unexpected result for bits=" << int(bits);
  }

  for (int bits = 24; bits <= std::numeric_limits<uint8_t>::max(); ++bits) {
    EXPECT_EQ(check_header_for_errors(make_header_with_fractional_bits(bits)),
              "Unsupported number of position fractional bits " + std::to_string(bits))
        << "Unexpected result for bits=" << int(bits);
  }
}

template<class Header> static void run_flags_tests()
{
  auto make_header_with_flags = [](const uint8_t flags) {
    Header header = make_default_valid_header<Header>();
    header.flags = flags;
    return header;
  };

  /* Flags are handled separately by the importer and do not make the header invalid. */
  EXPECT_EQ(check_header_for_errors(
                make_header_with_flags(SPZ_HEADER_ANTIALIASED | SPZ_HEADER_HAS_EXTENSIONS)),
            std::nullopt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for PackedGaussiansHeader
 * \{ */

template<> PackedGaussiansHeader make_default_valid_header()
{
  return {
      .magic = SPZ_HEADER_MAGIC,
      .version = 3,
      .num_points = 1,
      .sh_degree = 3,
      .fractional_bits = 12,
  };
}

TEST(PackedGaussiansHeader, check_for_errors_valid)
{
  EXPECT_EQ(check_header_for_errors(make_default_valid_header<PackedGaussiansHeader>()),
            std::nullopt);
}

TEST(PackedGaussiansHeader, check_for_errors_magic)
{
  run_magic_tests<PackedGaussiansHeader>();
}

TEST(PackedGaussiansHeader, check_for_errors_version)
{
  auto make_header_with_version = [](const uint32_t version) {
    PackedGaussiansHeader header = make_default_valid_header<PackedGaussiansHeader>();
    header.version = version;
    return header;
  };

  EXPECT_EQ(check_header_for_errors(make_header_with_version(0)), "Unsupported SPZ version 0");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(1)), "Unsupported SPZ version 1");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(2)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_version(3)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_version(4)), "Unsupported SPZ version 4");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(5)), "Unsupported SPZ version 5");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(UINT32_MAX)),
            "Unsupported SPZ version 4294967295");
}

TEST(PackedGaussiansHeader, check_for_errors_num_points)
{
  run_num_points_tests<PackedGaussiansHeader>();
}

TEST(PackedGaussiansHeader, check_for_errors_sh_degree)
{
  run_sh_degrees_tests<PackedGaussiansHeader>();
}

TEST(PackedGaussiansHeader, check_for_errors_fractional_bits)
{
  run_fractional_bits_tests<PackedGaussiansHeader>();
}

TEST(PackedGaussiansHeader, check_for_errors_flags)
{
  run_flags_tests<PackedGaussiansHeader>();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tests for NgspFileHeader
 * \{ */

template<> NgspFileHeader make_default_valid_header()
{
  return {
      .magic = SPZ_HEADER_MAGIC,
      .version = 4,
      .num_points = 1,
      .sh_degree = 3,
      .fractional_bits = 12,
      .num_streams = 6,
      .toc_byte_offset = sizeof(NgspFileHeader),
  };
}

TEST(NgspFileHeader, check_for_errors_valid)
{
  EXPECT_EQ(check_header_for_errors(make_default_valid_header<NgspFileHeader>()), std::nullopt);
}

TEST(NgspFileHeader, check_for_errors_magic)
{
  run_magic_tests<NgspFileHeader>();
}

TEST(NgspFileHeader, check_for_errors_version)
{
  auto make_header_with_version = [](const uint32_t version) {
    NgspFileHeader header = make_default_valid_header<NgspFileHeader>();
    header.version = version;
    return header;
  };

  EXPECT_EQ(check_header_for_errors(make_header_with_version(0)), "Unsupported SPZ version 0");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(1)), "Unsupported SPZ version 1");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(2)), "Unsupported SPZ version 2");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(3)), "Unsupported SPZ version 3");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(4)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_version(5)), "Unsupported SPZ version 5");
  EXPECT_EQ(check_header_for_errors(make_header_with_version(UINT32_MAX)),
            "Unsupported SPZ version 4294967295");
}

TEST(NgspFileHeader, check_for_errors_num_points)
{
  run_num_points_tests<NgspFileHeader>();
}

TEST(NgspFileHeader, check_for_errors_sh_degree)
{
  run_sh_degrees_tests<NgspFileHeader>();
}

TEST(NgspFileHeader, check_for_errors_fractional_bits)
{
  run_fractional_bits_tests<NgspFileHeader>();
}

TEST(NgspFileHeader, check_for_errors_flags)
{
  run_flags_tests<NgspFileHeader>();
}

TEST(NgspFileHeader, check_for_errors_num_streams)
{
  auto make_header_with_num_streams = [](const uint8_t num_streams) {
    NgspFileHeader header = make_default_valid_header<NgspFileHeader>();
    header.num_streams = num_streams;
    return header;
  };

  for (uint8_t num_streams = 0; num_streams < 6; ++num_streams) {
    EXPECT_EQ(check_header_for_errors(make_header_with_num_streams(num_streams)),
              "Unexpected number of Zstd streams")
        << "Unexpected result for num_streams=" << int(num_streams);
  }

  for (int num_streams = 6; num_streams <= std::numeric_limits<uint8_t>::max(); ++num_streams) {
    EXPECT_EQ(check_header_for_errors(make_header_with_num_streams(num_streams)), std::nullopt)
        << "Unexpected result for num_streams=" << int(num_streams);
  }
}

TEST(NgspFileHeader, check_for_errors_toc_byte_offset)
{
  auto make_header_with_toc_byte_offset = [](const uint32_t toc_byte_offset) {
    NgspFileHeader header = make_default_valid_header<NgspFileHeader>();
    header.toc_byte_offset = toc_byte_offset;
    return header;
  };

  EXPECT_EQ(check_header_for_errors(make_header_with_toc_byte_offset(0)),
            "TOC byte offset is less than the size of the header");
  EXPECT_EQ(check_header_for_errors(make_header_with_toc_byte_offset(31)),
            "TOC byte offset is less than the size of the header");
  EXPECT_EQ(check_header_for_errors(make_header_with_toc_byte_offset(32)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_toc_byte_offset(33)), std::nullopt);
  EXPECT_EQ(check_header_for_errors(make_header_with_toc_byte_offset(UINT32_MAX)), std::nullopt);
}

/** \} */

}  // namespace blender::io::spz::tests
