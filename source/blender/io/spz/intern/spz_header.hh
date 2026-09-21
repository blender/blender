/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace blender::io::spz {

/* Bytes N, G, S, P in file order, shared by all format versions. */
constexpr uint32_t SPZ_HEADER_MAGIC = 0x5053474e;

enum HeaderFlag {
  SPZ_HEADER_ANTIALIASED = 0x01,
  SPZ_HEADER_HAS_EXTENSIONS = 0x02,
};

/* Header inside the Gzip stream in SPZ versions 2 and 3. */
struct PackedGaussiansHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t num_points;
  uint8_t sh_degree;
  uint8_t fractional_bits;
  uint8_t flags;
  uint8_t reserved;
};
static_assert(sizeof(PackedGaussiansHeader) == 16);

/* Uncompressed header in SPZ version 4. */
struct NgspFileHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t num_points;
  uint8_t sh_degree;
  uint8_t fractional_bits;
  uint8_t flags;
  uint8_t num_streams;      /* The number of ZSTD-compressed attribute streams (typically 6). */
  uint32_t toc_byte_offset; /* Byte offset from file start to the TOC. */
  uint8_t reserved[12];     /* Zero, reserved for future use. */
};
static_assert(sizeof(NgspFileHeader) == 32);

/* Return an error message for a header the importer cannot use, or nullopt on success.
 * Only header fields are checked; this does not validate the file's payload or TOC entries. */
[[nodiscard]] std::optional<std::string> check_header_for_errors(
    const PackedGaussiansHeader &header);
[[nodiscard]] std::optional<std::string> check_header_for_errors(const NgspFileHeader &header);

}  // namespace blender::io::spz
