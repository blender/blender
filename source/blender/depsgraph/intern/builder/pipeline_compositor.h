/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "pipeline.h"

struct bNodeTree;

namespace blender::deg {

class CompositorBuilderPipeline : public AbstractBuilderPipeline {
 public:
  CompositorBuilderPipeline(::Depsgraph *graph, bNodeTree *nodetree);

 protected:
  std::unique_ptr<DepsgraphNodeBuilder> construct_node_builder() override;
  std::unique_ptr<DepsgraphRelationBuilder> construct_relation_builder() override;

  void build_nodes(DepsgraphNodeBuilder &node_builder) override;
  void build_relations(DepsgraphRelationBuilder &relation_builder) override;

 private:
  bNodeTree *nodetree_;
};

}  // namespace blender::deg
