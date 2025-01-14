/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>
#include <string.h>

#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"  // IWYU pragma: export

#include "NOD_multi_function.hh"
#include "NOD_register.hh"
#include "NOD_socket_declarations.hh"

#include "node_util.hh"

#include "FN_multi_function_builder.hh"

#include "RNA_access.hh"

void fn_node_type_base(blender::bke::bNodeType *ntype,
                       std::string idname,
                       std::optional<int16_t> legacy_type = std::nullopt);
