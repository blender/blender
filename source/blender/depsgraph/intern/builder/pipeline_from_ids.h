/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "pipeline.h"

namespace blender::deg {

/* Optimized builders for dependency graph built from a given set of IDs.
 *
 * General notes:
 *
 * - We pull in all bases if their objects are in the set of IDs. This allows to have proper
 *   visibility and other flags assigned to the objects.
 *   All other bases (the ones which points to object which is outside of the set of IDs) are
 *   completely ignored.
 */

class FromIDsBuilderPipeline : public AbstractBuilderPipeline {
  Span<ID *> ids_;

 public:
  FromIDsBuilderPipeline(::Depsgraph *graph, Span<ID *> ids);

 protected:
  virtual unique_ptr<DepsgraphNodeBuilder> construct_node_builder() override;
  virtual unique_ptr<DepsgraphRelationBuilder> construct_relation_builder() override;

  virtual void build_nodes(DepsgraphNodeBuilder &node_builder) override;
  virtual void build_relations(DepsgraphRelationBuilder &relation_builder) override;
};

}  // namespace blender::deg
