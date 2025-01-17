/* SPDX-FileCopyrightText: 2011-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include <MaterialXCore/Document.h>

#include "material.h"
#include "node_item.h"

#include "BLI_string_ref.hh"

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
  NodeGraph(const NodeGraph &parent, const StringRef child_name);

  NodeItem empty_node() const;
  NodeItem get_node(const StringRef name) const;
  NodeItem get_output(const StringRef name) const;
  NodeItem get_input(const StringRef name) const;

 protected:
  MaterialX::GraphElement *graph_element_ = nullptr;
};

}  // namespace blender::nodes::materialx
