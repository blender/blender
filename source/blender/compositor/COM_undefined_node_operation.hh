/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"
#include "COM_node_operation.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

NodeOperation *get_undefined_node_operation(Context &context, DNode node);

}  // namespace blender::compositor
