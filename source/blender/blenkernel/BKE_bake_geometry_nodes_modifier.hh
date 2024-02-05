/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_sub_frame.hh"

#include "BKE_bake_items.hh"
#include "BKE_bake_items_paths.hh"
#include "BKE_bake_items_serialize.hh"

struct NodesModifierData;
struct Main;
struct Object;
struct Scene;

namespace blender::bke::bake {

enum class CacheStatus {
  /** The cache is up-to-date with the inputs. */
  Valid,
  /**
   * Nodes or input values have changed since the cache was created, i.e. the output would be
   * different if the simulation was run again.
   */
  Invalid,
  /** The cache has been baked and will not be invalidated by changing inputs. */
  Baked,
};

/**
 * Stores the state for a specific frame.
 */
struct FrameCache {
  SubFrame frame;
  BakeState state;
  /** Used when the baked data is loaded lazily. */
  std::optional<std::string> meta_path;
};

/**
 * Stores the state after the previous simulation step. This is only used, when the frame-cache is
 * not used.
 */
struct PrevCache {
  BakeState state;
  SubFrame frame;
};

/**
 * Baked data that corresponds to either a Simulation Output or Bake node.
 */
struct NodeBakeCache {
  /** All cached frames sorted by frame. */
  Vector<std::unique_ptr<FrameCache>> frames;

  /** Where to load blobs from disk when loading the baked data lazily. */
  std::optional<std::string> blobs_dir;
  /** Used to avoid reading blobs multiple times for different frames. */
  std::unique_ptr<BlobReadSharing> blob_sharing;
  /** Used to avoid checking if a bake exists many times. */
  bool failed_finding_bake = false;

  /** Range spanning from the first to the last baked frame. */
  IndexRange frame_range() const;

  void reset();
};

struct SimulationNodeCache {
  NodeBakeCache bake;

  CacheStatus cache_status = CacheStatus::Valid;

  /** Previous simulation state when only that is stored (instead of the state for every frame). */
  std::optional<PrevCache> prev_cache;

  void reset();
};

struct BakeNodeCache {
  NodeBakeCache bake;

  void reset();
};

struct ModifierCache {
  mutable std::mutex mutex;
  /**
   * Set of nested node IDs (see #bNestedNodeRef) that is expected to be baked in the next
   * evaluation. This is filled and cleared by the bake operator.
   */
  Set<int> requested_bakes;
  Map<int, std::unique_ptr<SimulationNodeCache>> simulation_cache_by_id;
  Map<int, std::unique_ptr<BakeNodeCache>> bake_cache_by_id;

  SimulationNodeCache *get_simulation_node_cache(const int id);
  BakeNodeCache *get_bake_node_cache(const int id);
  NodeBakeCache *get_node_bake_cache(const int id);
};

/**
 * Reset all simulation caches in the scene, for use when some fundamental change made them
 * impossible to reuse.
 */
void scene_simulation_states_reset(Scene &scene);

std::optional<BakePath> get_node_bake_path(const Main &bmain,
                                           const Object &object,
                                           const NodesModifierData &nmd,
                                           int node_id);
std::optional<IndexRange> get_node_bake_frame_range(const Scene &scene,
                                                    const Object &object,
                                                    const NodesModifierData &nmd,
                                                    int node_id);
std::optional<std::string> get_modifier_bake_path(const Main &bmain,
                                                  const Object &object,
                                                  const NodesModifierData &nmd);

/**
 * Get the directory that contains all baked data for the given modifier by default.
 */
std::string get_default_modifier_bake_directory(const Main &bmain,
                                                const Object &object,
                                                const NodesModifierData &nmd);

}  // namespace blender::bke::bake
