/* SPDX-FileCopyrightText: 2011-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include <MaterialXCore/Document.h>

#include "material.h"
#include "node_item.h"

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

struct bNode;
struct Material;
struct Depsgraph;

namespace blender::nodes::materialx {

/*
 * Wrapper around MaterialX graph.
 */
struct NodeGraph {
 public:
  const Depsgraph *depsgraph = nullptr;
  const Material *material = nullptr;
  const ExportParams &export_params;

  NodeGraph(const Depsgraph *depsgraph,
            const Material *material,
            const ExportParams &export_params,
            const MaterialX::DocumentPtr &document);
  NodeGraph(const NodeGraph &parent, StringRef child_name);

  NodeItem empty_node() const;
  NodeItem get_node(StringRef name) const;
  NodeItem get_output(StringRef name) const;
  NodeItem get_input(StringRef name) const;

  std::string unique_node_name(const bNode *node,
                               StringRef socket_out_name,
                               NodeItem::Type to_type);
  void set_output_node_name(const NodeItem &item) const;

  static std::string unique_anonymous_node_name(MaterialX::GraphElement *graph_element);

 protected:
  struct NodeKey {
    const bNode *node;
    std::string socket_name;
    NodeItem::Type to_type;
    MaterialX::GraphElement *graph_element;

    uint64_t hash() const;
    bool operator==(const NodeKey &other) const;
  };

  MaterialX::GraphElement *graph_element_ = nullptr;
  Map<NodeKey, const std::string> root_key_to_name_map_;
  Map<NodeKey, const std::string> &key_to_name_map_;
  Set<std::string> used_node_names_;
  std::string node_name_prefix_;
};

}  // namespace blender::nodes::materialx
