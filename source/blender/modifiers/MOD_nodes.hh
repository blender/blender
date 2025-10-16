/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_array.hh"
#include "NOD_socket_usage_inference_fwd.hh"

struct NodesModifierData;
struct NodesModifierDataBlock;
struct Object;
struct NodesModifierPackedBake;
struct NodesModifierBake;

namespace blender::bke::bake {
struct ModifierCache;
}
namespace blender::nodes::geo_eval_log {
class GeoNodesLog;
}

/**
 * Rebuild the list of properties based on the sockets exposed as the modifier's node group
 * inputs. If any properties correspond to the old properties by name and type, carry over
 * the values.
 */
void MOD_nodes_update_interface(Object *object, NodesModifierData *nmd);

namespace blender {

class NodesModifierUsageInferenceCache {
 private:
  uint64_t input_values_hash_ = 0;

 public:
  Array<nodes::socket_usage_inference::SocketUsage> inputs;
  Array<nodes::socket_usage_inference::SocketUsage> outputs;

  void ensure(const NodesModifierData &nmd);
  void reset();
};

struct NodesModifierRuntime {
  /**
   * Contains logged information from the last evaluation.
   * This can be used to help the user to debug a node tree.
   * This is a shared pointer because we might want to keep it around in some cases after the
   * evaluation (e.g. for gizmo backpropagation).
   */
  std::shared_ptr<nodes::geo_eval_log::GeoNodesLog> eval_log;
  /**
   * Simulation cache that is shared between original and evaluated modifiers. This allows the
   * original modifier to be removed, without also removing the simulation state which may still be
   * used by the evaluated modifier.
   */
  std::shared_ptr<bke::bake::ModifierCache> cache;
  /**
   * Cache the usage of the node group inputs and outputs to accelerate drawing the UI when no
   * properties change.
   */
  NodesModifierUsageInferenceCache usage_cache;
};

void nodes_modifier_data_block_destruct(NodesModifierDataBlock *data_block, bool do_id_user);
void nodes_modifier_packed_bake_free(NodesModifierPackedBake *packed_bake);
void nodes_modifier_bake_destruct(NodesModifierBake *bake, bool do_id_user);

}  // namespace blender
