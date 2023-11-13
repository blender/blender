/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "DNA_node_types.h"

namespace blender::dot {
class DirectedEdge;
}

namespace blender::bke {

/**
 * Allows customizing how the generated dot graph looks like.
 */
class bNodeTreeToDotOptions {
 public:
  virtual std::string socket_name(const bNodeSocket &socket) const;
  virtual std::optional<std::string> socket_font_color(const bNodeSocket &socket) const;
  virtual void add_edge_attributes(const bNodeLink &link, dot::DirectedEdge &dot_edge) const;
};

/**
 * Convert a node tree into the dot format. This can be visualized with tools like graphviz and is
 * very useful for debugging purposes.
 */
std::string node_tree_to_dot(const bNodeTree &tree,
                             const bNodeTreeToDotOptions &options = bNodeTreeToDotOptions());

}  // namespace blender::bke
