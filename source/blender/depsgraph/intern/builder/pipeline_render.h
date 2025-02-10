/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "pipeline.h"

namespace blender::deg {

class RenderBuilderPipeline : public AbstractBuilderPipeline {
 public:
  RenderBuilderPipeline(::Depsgraph *graph);

 protected:
  std::unique_ptr<DepsgraphNodeBuilder> construct_node_builder() override;
  std::unique_ptr<DepsgraphRelationBuilder> construct_relation_builder() override;

  void build_nodes(DepsgraphNodeBuilder &node_builder) override;
  void build_relations(DepsgraphRelationBuilder &relation_builder) override;
};

}  // namespace blender::deg
