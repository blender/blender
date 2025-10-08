/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"

struct bNodeTree;
struct bNode;

namespace blender::nodes {

struct InlineShaderNodeTreeParams {
  /**
   * Disable loop unrolling and keep Repeat Zone nodes in the tree.
   * (For engines with native support for Repeat Zones)
   *
   * Some Repeat Zones may still be unrolled (eg. if they have Closure or Bundle Zone Items).
   */
  bool allow_preserving_repeat_zones = false;

  struct ErrorMessage {
    /* In theory, more contextual information could be added here like the entire context path to
     * that node. In practice, we can't report errors with that level of detail in shader nodes
     * yet. */
    const bNode *node;
    std::string message;
  };
  Vector<ErrorMessage> r_error_messages;
};

bool inline_shader_node_tree(const bNodeTree &src_tree,
                             bNodeTree &dst_tree,
                             InlineShaderNodeTreeParams &params);

}  // namespace blender::nodes
