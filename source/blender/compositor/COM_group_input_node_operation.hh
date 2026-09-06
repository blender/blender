/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {
struct bNode;
}

namespace blender::compositor {

class Operation;
class Context;
class NodeOperation;

/* Returns an instance of a new GroupInputNodeOperation with the given parameters. See the class
 * for more information. */
NodeOperation *get_group_input_node_operation(Context &context,
                                              const bNode &node,
                                              Operation &node_group_operation);

}  // namespace blender::compositor
