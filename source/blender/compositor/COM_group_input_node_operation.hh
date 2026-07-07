/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "COM_context.hh"
#include "COM_node_group_operation.hh"

namespace blender::compositor {

/* Returns an instance of a new GroupInputNodeOperation with the given parameters. See the class
 * for more information. */
NodeOperation *get_group_input_node_operation(Context &context,
                                              const bNode &node,
                                              NodeGroupOperation &node_group_operation);

}  // namespace blender::compositor
