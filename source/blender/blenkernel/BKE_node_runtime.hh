/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_sys_types.h"
#include "BLI_utility_mixins.hh"

namespace blender::nodes {
struct FieldInferencingInterface;
struct NodeDeclaration;
}  // namespace blender::nodes

namespace blender::bke {

class bNodeTreeRuntime : NonCopyable, NonMovable {
 public:
  /**
   * Keeps track of what changed in the node tree until the next update.
   * Should not be changed directly, instead use the functions in `BKE_node_tree_update.h`.
   * #eNodeTreeChangedFlag.
   */
  uint32_t changed_flag = 0;
  /**
   * A hash of the topology of the node tree leading up to the outputs. This is used to determine
   * of the node tree changed in a way that requires updating geometry nodes or shaders.
   */
  uint32_t output_topology_hash = 0;

  /**
   * Used to cache run-time information of the node tree.
   * #eNodeTreeRuntimeFlag.
   */
  uint8_t runtime_flag = 0;

  /** Information about how inputs and outputs of the node group interact with fields. */
  std::unique_ptr<nodes::FieldInferencingInterface> field_inferencing_interface;
};

/**
 * Run-time data for every socket. This should only contain data that is somewhat persistent (i.e.
 * data that lives longer than a single depsgraph evaluation + redraw). Data that's only used in
 * smaller scopes should generally be stored in separate arrays and/or maps.
 */
class bNodeSocketRuntime : NonCopyable, NonMovable {
 public:
  /**
   * References a socket declaration that is owned by `node->declaration`. This is only runtime
   * data. It has to be updated when the node declaration changes.
   */
  const SocketDeclarationHandle *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;
};

/**
 * Run-time data for every node. This should only contain data that is somewhat persistent (i.e.
 * data that lives longer than a single depsgraph evaluation + redraw). Data that's only used in
 * smaller scopes should generally be stored in separate arrays and/or maps.
 */
class bNodeRuntime : NonCopyable, NonMovable {
 public:
  /**
   * Describes the desired interface of the node. This is run-time data only.
   * The actual interface of the node may deviate from the declaration temporarily.
   * It's possible to sync the actual state of the node to the desired state. Currently, this is
   * only done when a node is created or loaded.
   *
   * In the future, we may want to keep more data only in the declaration, so that it does not have
   * to be synced to other places that are stored in files. That especially applies to data that
   * can't be edited by users directly (e.g. min/max values of sockets, tooltips, ...).
   *
   * The declaration of a node can be recreated at any time when it is used. Caching it here is
   * just a bit more efficient when it is used a lot. To make sure that the cache is up-to-date,
   * call #nodeDeclarationEnsure before using it.
   *
   * Currently, the declaration is the same for every node of the same type. Going forward, that is
   * intended to change though. Especially when nodes become more dynamic with respect to how many
   * sockets they have.
   */
  NodeDeclarationHandle *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;
};

}  // namespace blender::bke
