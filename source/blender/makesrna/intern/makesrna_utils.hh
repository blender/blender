/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <iosfwd>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender {

void rna_write_struct_forward_declarations(std::ostringstream &buffer, Vector<StringRef> structs);

}  // namespace blender
