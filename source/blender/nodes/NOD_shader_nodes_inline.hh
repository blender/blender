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
