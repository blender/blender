/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_node_operation.hh"

namespace blender::compositor {

/* Returns an instance of a new UndefinedNodeOperation for the given node. See the class for more
 * information, */
NodeOperation *get_undefined_node_operation(Context &context, const bNode &node);

}  // namespace blender::compositor
