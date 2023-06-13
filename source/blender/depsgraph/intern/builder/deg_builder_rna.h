/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_operation.h"

struct ID;
struct PointerRNA;
struct PropertyRNA;

namespace blender::deg {

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
  Map<const ID *, unique_ptr<RNANodeQueryIDData>> id_data_map_;

  /* Construct identifier of the node which corresponds given configuration
   * of RNA property. */
  RNANodeIdentifier construct_node_identifier(const PointerRNA *ptr,
                                              const PropertyRNA *prop,
                                              RNAPointerSource source);

  /* Make sure ID data exists for the given ID, and returns it. */
  RNANodeQueryIDData *ensure_id_data(const ID *id);

  /* Check whether prop_identifier contains rna_path_component.
   *
   * This checks more than a sub-string:
   *
   * prop_identifier           contains(prop_identifier, "location")
   * ------------------------  -------------------------------------
   * location                  true
   * ["test_location"]         false
   * pose["bone"].location     true
   * pose["bone"].location.x   true
   */
  static bool contains(const char *prop_identifier, const char *rna_path_component);
};

bool rna_prop_affects_parameters_node(const PointerRNA *ptr, const PropertyRNA *prop);

}  // namespace blender::deg
