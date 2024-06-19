/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "node_parser.h"

/* TODO: #pxr::UsdMtlxRead() doesn't perform node-graphs.
 * Uncomment USE_MATERIALX_NODEGRAPH after fixing it. */
// #define USE_MATERIALX_NODEGRAPH

namespace blender::nodes::materialx {

class GroupInputNodeParser;

class GroupNodeParser : public NodeParser {
  friend GroupInputNodeParser;

 protected:
  bool use_group_default_;

 public:
  GroupNodeParser(MaterialX::GraphElement *graph,
                  const Depsgraph *depsgraph,
                  const Material *material,
                  const bNode *node,
                  const bNodeSocket *socket_out,
                  NodeItem::Type to_type,
                  GroupNodeParser *group_parser,
                  ExportParams export_params,
                  bool use_group_default);
  NodeItem compute() override;
  NodeItem compute_full() override;
};

class GroupOutputNodeParser : public GroupNodeParser {
 public:
  using GroupNodeParser::GroupNodeParser;
  NodeItem compute() override;
  NodeItem compute_full() override;

 private:
  static std::string out_name(const bNodeSocket *out_socket);
};

class GroupInputNodeParser : public GroupNodeParser {
 public:
  using GroupNodeParser::GroupNodeParser;
  NodeItem compute() override;
  NodeItem compute_full() override;

 private:
  std::string in_name() const;
};

}  // namespace blender::nodes::materialx
