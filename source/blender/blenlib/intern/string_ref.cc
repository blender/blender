/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include <ostream>

namespace blender {

std::ostream &operator<<(std::ostream &stream, StringRef ref)
{
  stream << std::string(ref);
  return stream;
}

std::ostream &operator<<(std::ostream &stream, StringRefNull ref)
{
  stream << std::string(ref.data(), size_t(ref.size()));
  return stream;
}

}  // namespace blender
