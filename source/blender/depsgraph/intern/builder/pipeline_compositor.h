/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

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
  virtual void build_nodes(DepsgraphNodeBuilder &node_builder) override;
  virtual void build_relations(DepsgraphRelationBuilder &relation_builder) override;

 private:
  bNodeTree *nodetree_;
};

}  // namespace blender::deg
