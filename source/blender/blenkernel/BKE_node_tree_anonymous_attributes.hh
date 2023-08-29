/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <variant>

#include "DNA_node_types.h"

#include "BLI_bit_group_vector.hh"
#include "BLI_vector.hh"

#include "NOD_node_declaration.hh"

namespace blender::bke::anonymous_attribute_inferencing {

/** Represents a group input socket that may be a field. */
struct InputFieldSource {
  int input_index;
};

/** Represents an output of a node that is a field which may contain a new anonymous attribute. */
struct SocketFieldSource {
  const bNodeSocket *socket;
};

struct FieldSource {
  std::variant<InputFieldSource, SocketFieldSource> data;
  /** Geometry source which may contain the anonymous attributes referenced by this field. */
  Vector<int> geometry_sources;
};

/** Represents a geometry group input. */
struct InputGeometrySource {
  int input_index;
};

/** Represents a geometry output of a node that may contain a new anonymous attribute. */
struct SocketGeometrySource {
  const bNodeSocket *socket;
};

struct GeometrySource {
  std::variant<InputGeometrySource, SocketGeometrySource> data;
  /** Field sources that originate in this geometry source. */
  Vector<int> field_sources;
};

struct AnonymousAttributeInferencingResult {
  /** All field sockets that may introduce new anonymous attributes into the node tree. */
  Vector<FieldSource> all_field_sources;
  /** All geometry sockets that may introduce new anonymous attributes into the node tree. */
  Vector<GeometrySource> all_geometry_sources;

  /** Encodes which field sources are propagated to every field socket. */
  BitGroupVector<> propagated_fields_by_socket;
  /** Encodes which geometry sources are propagated to every geometry socket. */
  BitGroupVector<> propagated_geometries_by_socket;
  /** Encodes which field sources may be available on every geometry socket. */
  BitGroupVector<> available_fields_by_geometry_socket;
  /**
   * Encodes which field sources have to be propagated to each geometry socket at run-time to
   * ensure correct evaluation.
   */
  BitGroupVector<> required_fields_by_geometry_socket;

  /** Set of geometry output indices which may propagate attributes from input geometries. */
  VectorSet<int> propagated_output_geometry_indices;
  /** Encodes to which group outputs a geometry is propagated to. */
  BitGroupVector<> propagate_to_output_by_geometry_socket;

  nodes::aal::RelationsInNode tree_relations;
};

Array<const nodes::aal::RelationsInNode *> get_relations_by_node(const bNodeTree &tree,
                                                                 ResourceScope &scope);
bool update_anonymous_attribute_relations(bNodeTree &tree);

}  // namespace blender::bke::anonymous_attribute_inferencing
