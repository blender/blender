/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Geometry nodes has the concept of anonymous attributes. These are attributes that are created by
 * some node with an internal name that is not exposed to the user. The only way to use this
 * attribute is to make a link in the node tree to the node that should use it. This explicitness
 * allows us to automatically determine when anonymous attributes are not needed anymore and should
 * be deleted (or not created in the first place).
 *
 * This file is used to determine the lifetimes of these anonymous attributes. The logic is fairly
 * straight forward to extend to other kinds of referenced data, but for now we only have anonymous
 * attributes.
 *
 * The lifetime analysis uses information provided in the node declaration and hardcoded behavior
 * for some special nodes like zones to determine the following things among others.
 * - Where are new references (like anonymous attributes) created?
 * - Which data sockets (like geometry) contain the referenced data?
 * - Which nodes have to propagate which referenced data to which outputs?
 *
 * This information is later used when evaluating geometry nodes to reduce the lifetime of
 * anonymous attributes automatically.
 */

#include "BLI_bit_group_vector.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "NOD_node_declaration.hh"

namespace blender::bke::node_tree_reference_lifetimes {

enum class ReferenceSetType {
  /**
   * Corresponds to geometry outputs that may contain attributes that are propagated from a group
   * input. In such cases, the caller may provide a set of attributes that should be propagated.
   */
  GroupOutputData,
  /**
   * Field inputs may require attributes that need to be propagated from other geometry inputs to
   * the node that evaluates the field.
   */
  GroupInputReferenceSet,
  /**
   * Locally created anonymous attributes (like with the Capture Attribute node) need to be
   * propagated to the nodes that use them or even to the group output.
   */
  LocalReferenceSet,
};

struct ReferenceSetInfo {
  ReferenceSetType type;
  union {
    /** Used for group interface sockets. */
    int index;
    /** Used for local sockets. */
    const bNodeSocket *socket;
  };

  /**
   * Sockets that may contain the referenced data (e.g. the geometry output of a Capture Attribute
   * node).
   */
  Vector<const bNodeSocket *> potential_data_origins;

  ReferenceSetInfo(ReferenceSetType type, const int index) : type(type), index(index)
  {
    BLI_assert(
        ELEM(type, ReferenceSetType::GroupInputReferenceSet, ReferenceSetType::GroupOutputData));
  }

  ReferenceSetInfo(ReferenceSetType type, const bNodeSocket *socket) : type(type), socket(socket)
  {
    BLI_assert(ELEM(type, ReferenceSetType::LocalReferenceSet));
  }

  friend std::ostream &operator<<(std::ostream &stream, const ReferenceSetInfo &source);
};

struct ReferenceLifetimesInfo {
  Vector<ReferenceSetInfo> reference_sets;

  /**
   * Has a bit for each socket and each reference set. If the bit is set, the corresponding
   * reference set should be propagated to that socket.
   */
  BitGroupVector<> required_data_by_socket;

  /**
   * Relations used for group nodes that use this group.
   */
  nodes::aal::RelationsInNode tree_relations;
};

bool analyse_reference_lifetimes(bNodeTree &tree);

}  // namespace blender::bke::node_tree_reference_lifetimes
