/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "pipeline.h"

namespace blender {

struct Collection;

namespace deg {

/* Optimized builders for dependency graph built from a given Collection.
 *
 * General notes:
 *
 * - We pull in all bases if their objects are in the set of IDs. This allows to have proper
 *   visibility and other flags assigned to the objects.
 *   All other bases (the ones which points to object which is outside of the set of IDs) are
 *   completely ignored.
 */

class FromCollectionBuilderPipeline : public AbstractBuilderPipeline {
 public:
  FromCollectionBuilderPipeline(blender::Depsgraph *graph, Collection *collection);

 protected:
  std::unique_ptr<DepsgraphNodeBuilder> construct_node_builder() override;
  std::unique_ptr<DepsgraphRelationBuilder> construct_relation_builder() override;

  void build_nodes(DepsgraphNodeBuilder &node_builder) override;
  void build_relations(DepsgraphRelationBuilder &relation_builder) override;

 private:
  Set<ID *> ids_;
};

}  // namespace deg
}  // namespace blender
