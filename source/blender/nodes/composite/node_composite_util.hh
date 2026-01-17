/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include <optional>

#include "DNA_ID.h"  // IWYU pragma: export
#include "DNA_node_types.h"

#include "BKE_node_legacy_types.hh"  // IWYU pragma: export

#include "node_util.hh"  // IWYU pragma: export

#include "NOD_composite.hh"            // IWYU pragma: export
#include "NOD_menu_value.hh"           // IWYU pragma: export
#include "NOD_register.hh"             // IWYU pragma: export
#include "NOD_socket.hh"               // IWYU pragma: export
#include "NOD_socket_declarations.hh"  // IWYU pragma: export
#include "NOD_socket_search_link.hh"

namespace blender {

#define CMP_SCALE_MAX 12000

bool cmp_node_poll_default(const bke::bNodeType *ntype,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint);
void cmp_node_type_base(bke::bNodeType *ntype,
                        std::string idname,
                        std::optional<int16_t> legacy_type = std::nullopt);

}  // namespace blender
