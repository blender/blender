/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_node_group_operation.hh"
#include "COM_node_operation.hh"

namespace blender::compositor {

/* Returns an instance of a new GroupNodeOperation with the given parameters. See the class for
 * more information.. */
NodeOperation *get_group_node_operation(Context &context,
                                        const bNode &node,
                                        const NodeGroupOutputTypes &needed_outputs,
                                        const bNodeInstanceKey active_node_group_instance_key);

}  // namespace blender::compositor
