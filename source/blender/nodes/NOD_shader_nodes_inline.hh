/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"

struct bNodeTree;
struct bNode;

namespace blender::nodes {

struct InlineShaderNodeTreeParams {
  bool allow_preserving_repeat_zones = false;
  /**
   * In general, only a constant number of iterations per repeat zone is allowed, because otherwise
   * it can't be inlined. However, if the render engine supports repeat zones natively, it could
   * also support a dynamic number of iterations.
   */
  bool dynamic_repeat_zone_iterations_is_error = true;

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
