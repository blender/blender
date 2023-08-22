/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bit_ref.hh"

#include <ostream>

namespace blender::bits {

std::ostream &operator<<(std::ostream &stream, const BitRef &bit)
{
  return stream << (bit ? '1' : '0');
}

std::ostream &operator<<(std::ostream &stream, const MutableBitRef &bit)
{
  return stream << BitRef(bit);
}

}  // namespace blender::bits
