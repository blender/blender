/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct NodesModifierData;
struct Object;

namespace blender::bke::sim {
class ModifierSimulationCache;
}
namespace blender::nodes::geo_eval_log {
class GeoModifierLog;
}

/**
 * Rebuild the list of properties based on the sockets exposed as the modifier's node group
 * inputs. If any properties correspond to the old properties by name and type, carry over
 * the values.
 */
void MOD_nodes_update_interface(Object *object, NodesModifierData *nmd);

namespace blender {

struct NodesModifierRuntime {
  /**
   * Contains logged information from the last evaluation.
   * This can be used to help the user to debug a node tree.
   */
  std::unique_ptr<nodes::geo_eval_log::GeoModifierLog> eval_log;
  /**
   * Simulation cache that is shared between original and evaluated modifiers. This allows the
   * original modifier to be removed, without also removing the simulation state which may still be
   * used by the evaluated modifier.
   */
  std::shared_ptr<bke::sim::ModifierSimulationCache> simulation_cache;
};

}  // namespace blender
