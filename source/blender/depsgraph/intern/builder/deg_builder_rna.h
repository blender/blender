/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_operation.h"

struct GHash;
struct ID;
struct PointerRNA;
struct PropertyRNA;

namespace DEG {

struct Depsgraph;
struct Node;
class RNANodeQueryIDData;
class DepsgraphBuilder;

/* For queries which gives operation node or key defines whether we are
 * interested in a result of the given property or whether we are linking some
 * dependency to that property. */
enum class RNAPointerSource {
  /* Query will return pointer to an entry operation of component which is
   * responsible for evaluation of the given property. */
  ENTRY,
  /* Query will return pointer to an exit operation of component which is
   * responsible for evaluation of the given property.
   * More precisely, it will return operation at which the property is known
   * to be evaluated. */
  EXIT,
};

/* A helper structure which wraps all fields needed to find a node inside of
 * the dependency graph. */
class RNANodeIdentifier {
 public:
  RNANodeIdentifier();

  /* Check whether this identifier is valid and usable. */
  bool is_valid() const;

  ID *id;
  NodeType type;
  const char *component_name;
  OperationCode operation_code;
  const char *operation_name;
  int operation_name_tag;
};

/* Helper class which performs optimized lookups of a node within a given
 * dependency graph which satisfies given RNA pointer or RAN path. */
class RNANodeQuery {
 public:
  RNANodeQuery(Depsgraph *depsgraph, DepsgraphBuilder *builder);
  ~RNANodeQuery();

  Node *find_node(const PointerRNA *ptr, const PropertyRNA *prop, RNAPointerSource source);

 protected:
  Depsgraph *depsgraph_;
  DepsgraphBuilder *builder_;

  /* Indexed by an ID, returns RNANodeQueryIDData associated with that ID. */
  GHash *id_data_map_;

  /* Construct identifier of the node which correspods given configuration
   * of RNA property. */
  RNANodeIdentifier construct_node_identifier(const PointerRNA *ptr,
                                              const PropertyRNA *prop,
                                              RNAPointerSource source);

  /* Make sure ID data exists for the given ID, and returns it. */
  RNANodeQueryIDData *ensure_id_data(const ID *id);
};

}  // namespace DEG
