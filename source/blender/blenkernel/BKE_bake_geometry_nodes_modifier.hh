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
 * Stores the cached/baked data for simulation nodes in geometry nodes.
 */
struct NodeCache {
  CacheStatus cache_status = CacheStatus::Valid;

  /** All cached frames. */
  Vector<std::unique_ptr<FrameCache>> frame_caches;
  /** Previous simulation state when only that is stored (instead of the state for every frame). */
  std::optional<PrevCache> prev_cache;

  /** Where to load blobs from disk when loading the baked data lazily. */
  std::optional<std::string> blobs_dir;
  /** Used to avoid reading blobs multiple times for different frames. */
  std::unique_ptr<BlobSharing> blob_sharing;
  /** Used to avoid checking if a bake exists many times. */
  bool failed_finding_bake = false;

  void reset();
};

struct ModifierCache {
  mutable std::mutex mutex;
  Map<int, std::unique_ptr<NodeCache>> cache_by_id;
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
