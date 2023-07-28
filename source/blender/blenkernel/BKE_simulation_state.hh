/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_bake_items_serialize.hh"
#include "BKE_geometry_set.hh"

#include "BLI_map.hh"
#include "BLI_sub_frame.hh"

struct bNodeTree;

namespace blender::bke::sim {

class ModifierSimulationCache;

/**
 * Storage of values for a single simulation input and output node pair.
 * Used as a cache to allow random access in time, and as an intermediate form before data is
 * baked.
 */
class SimulationZoneState {
 public:
  Map<int, std::unique_ptr<BakeItem>> item_by_identifier;
};

/** Identifies a simulation zone (input and output node pair) used by a modifier. */
struct SimulationZoneID {
  /** ID of the #bNestedNodeRef that references the output node of the zone. */
  int32_t nested_node_id;

  uint64_t hash() const
  {
    return this->nested_node_id;
  }

  friend bool operator==(const SimulationZoneID &a, const SimulationZoneID &b)
  {
    return a.nested_node_id == b.nested_node_id;
  }
};

/**
 * Stores a single frame of simulation states for all simulation zones in a modifier's node
 * hierarchy.
 */
class ModifierSimulationState {
 private:
  mutable bool bake_loaded_;

 public:
  ModifierSimulationCache *owner_;
  mutable std::mutex mutex_;
  Map<SimulationZoneID, std::unique_ptr<SimulationZoneState>> zone_states_;
  /** File path to folder containing baked meta-data. */
  std::optional<std::string> meta_path_;
  /** File path to folder containing baked data. */
  std::optional<std::string> bdata_dir_;

  SimulationZoneState *get_zone_state(const SimulationZoneID &zone_id);
  const SimulationZoneState *get_zone_state(const SimulationZoneID &zone_id) const;
  SimulationZoneState &get_zone_state_for_write(const SimulationZoneID &zone_id);
  void ensure_bake_loaded(const bNodeTree &ntree) const;
};

struct ModifierSimulationStateAtFrame {
  SubFrame frame;
  ModifierSimulationState state;
};

enum class CacheState {
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

struct StatesAroundFrame {
  const ModifierSimulationStateAtFrame *prev = nullptr;
  const ModifierSimulationStateAtFrame *current = nullptr;
  const ModifierSimulationStateAtFrame *next = nullptr;
};

struct ModifierSimulationCacheRealtime {
  std::unique_ptr<ModifierSimulationState> prev_state;
  std::unique_ptr<ModifierSimulationState> current_state;
  SubFrame prev_frame;
  SubFrame current_frame;
};

class ModifierSimulationCache {
 private:
  mutable std::mutex states_at_frames_mutex_;
  /**
   * All simulation states, sorted by frame.
   */
  Vector<std::unique_ptr<ModifierSimulationStateAtFrame>> states_at_frames_;
  /**
   * Used for baking to deduplicate arrays when writing and writing from storage. Sharing info
   * must be kept alive for multiple frames to detect if each data array's version has changed.
   */
  std::unique_ptr<BDataSharing> bdata_sharing_;

  friend ModifierSimulationState;

  bool failed_finding_bake_ = false;

 public:
  CacheState cache_state = CacheState::Valid;

  /** A non-persistent cache used only to pass simulation state data from one frame to the next. */
  ModifierSimulationCacheRealtime realtime_cache;

  void try_discover_bake(StringRefNull absolute_bake_dir);

  bool has_state_at_frame(const SubFrame &frame) const;
  bool has_states() const;
  const ModifierSimulationState *get_state_at_exact_frame(const SubFrame &frame) const;
  ModifierSimulationState &get_state_at_frame_for_write(const SubFrame &frame);
  StatesAroundFrame get_states_around_frame(const SubFrame &frame) const;

  void invalidate()
  {
    this->cache_state = CacheState::Invalid;
  }

  void reset();
};

/**
 * Reset all simulation caches in the scene, for use when some fundamental change made them
 * impossible to reuse.
 */
void scene_simulation_states_reset(Scene &scene);

}  // namespace blender::bke::sim
