/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_range.hh"

#include <ostream>

namespace blender {

std::ostream &operator<<(std::ostream &stream, IndexRange range)
{
  stream << '[' << range.start() << ", " << range.one_after_last() << ')';
  return stream;
}

} // namespace blender
