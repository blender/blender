/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_cpp_type.hh"

#include <sstream>

namespace blender {

std::string CPPType::to_string(const void *value) const
{
  std::stringstream ss;
  this->print(value, ss);
  return ss.str();
}

void CPPType::print_or_default(const void *value,
                               std::stringstream &ss,
                               StringRef default_value) const
{
  if (this->is_printable()) {
    this->print(value, ss);
  }
  else {
    ss << default_value;
  }
}

} // namespace blender
