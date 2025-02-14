/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstring>
#include <optional>

#include "BLI_math_vector.hh"  // IWYU pragma: export

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"  // IWYU pragma: export

#include "NOD_multi_function.hh"       // IWYU pragma: export
#include "NOD_register.hh"             // IWYU pragma: export
#include "NOD_socket_declarations.hh"  // IWYU pragma: export

#include "node_util.hh"  // IWYU pragma: export

#include "FN_multi_function_builder.hh"  // IWYU pragma: export

#include "RNA_access.hh"  // IWYU pragma: export

void fn_node_type_base(blender::bke::bNodeType *ntype,
                       std::string idname,
                       std::optional<int16_t> legacy_type = std::nullopt);
