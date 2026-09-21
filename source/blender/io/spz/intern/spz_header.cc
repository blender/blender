/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "spz_header.hh"

#include <fmt/format.h>

#include "BLT_translation.hh"

#include "IO_validate.hh"

namespace blender::io::spz {

template<typename Header>
static std::optional<std::string> check_common_header_for_errors(const Header &header,
                                                                 const uint32_t min_version,
                                                                 const uint32_t max_version)
{
  if (header.magic != SPZ_HEADER_MAGIC) {
    return fmt::format(fmt::runtime(RPT_("Unexpected SPZ header magic 0x{:X}")), header.magic);
  }
  if (header.version < min_version || header.version > max_version) {
    return fmt::format(fmt::runtime(RPT_("Unsupported SPZ version {}")), header.version);
  }
  if (!validate::size_fits_in_int(header.num_points)) {
    return RPT_("Too many points");
  }
  /* Blender supports spherical harmonics up to degree 4. */
  if (header.sh_degree > 4) {
    return fmt::format(fmt::runtime(RPT_("Unsupported SH degree {}")), header.sh_degree);
  }
  /* Match the range supported by the 24-bit fixed-point position decoder. */
  if (header.fractional_bits >= 24) {
    return fmt::format(fmt::runtime(RPT_("Unsupported number of position fractional bits {}")),
                       header.fractional_bits);
  }
  return std::nullopt;
}

std::optional<std::string> check_header_for_errors(const PackedGaussiansHeader &header)
{
  return check_common_header_for_errors(header, 2, 3);
}

std::optional<std::string> check_header_for_errors(const NgspFileHeader &header)
{
  if (const std::optional<std::string> error = check_common_header_for_errors(header, 4, 4)) {
    return error;
  }
  if (header.toc_byte_offset < sizeof(NgspFileHeader)) {
    return RPT_("TOC byte offset is less than the size of the header");
  }
  /* Positions, alphas, colors, scales, rotations, and spherical harmonics. */
  if (header.num_streams < 6) {
    return RPT_("Unexpected number of Zstd streams");
  }
  return std::nullopt;
}

}  // namespace blender::io::spz
