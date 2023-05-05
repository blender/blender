/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

#include "BLI_map.hh"
#include "BLI_sub_frame.hh"

namespace blender::bke::sim {

class BDataSharing;
class ModifierSimulationCache;

class SimulationStateItem {
 public:
  virtual ~SimulationStateItem() = default;
};

class GeometrySimulationStateItem : public SimulationStateItem {
 private:
  GeometrySet geometry_;

 public:
  GeometrySimulationStateItem(GeometrySet geometry);

  const GeometrySet &geometry() const
  {
    return geometry_;
  }

  GeometrySet &geometry()
  {
    return geometry_;
  }
};

class AttributeSimulationStateItem : public SimulationStateItem {
 private:
  std::string name_;

 public:
  AttributeSimulationStateItem(std::string name) : name_(std::move(name)) {}

  StringRefNull name() const
  {
    return name_;
  }
};

class PrimitiveSimulationStateItem : public SimulationStateItem {
 private:
  const CPPType &type_;
  void *value_;

 public:
  PrimitiveSimulationStateItem(const CPPType &type, const void *value);
  ~PrimitiveSimulationStateItem();

  const void *value() const
  {
    return value_;
  }

  const CPPType &type() const
  {
    return type_;
  }
};

class StringSimulationStateItem : public SimulationStateItem {
 private:
  std::string value_;

 public:
  StringSimulationStateItem(std::string value);

  StringRefNull value() const
  {
    return value_;
  }
};

class SimulationZoneState {
 public:
  Map<int, std::unique_ptr<SimulationStateItem>> item_by_identifier;
};

struct SimulationZoneID {
  Vector<int> node_ids;

  uint64_t hash() const
  {
    return get_default_hash(this->node_ids);
  }

  friend bool operator==(const SimulationZoneID &a, const SimulationZoneID &b)
  {
    return a.node_ids == b.node_ids;
  }
};

class ModifierSimulationState {
 private:
  mutable bool bake_loaded_;

 public:
  ModifierSimulationCache *owner_;
  mutable std::mutex mutex_;
  Map<SimulationZoneID, std::unique_ptr<SimulationZoneState>> zone_states_;
  std::optional<std::string> meta_path_;
  std::optional<std::string> bdata_dir_;

  const SimulationZoneState *get_zone_state(const SimulationZoneID &zone_id) const;
  SimulationZoneState &get_zone_state_for_write(const SimulationZoneID &zone_id);
  void ensure_bake_loaded() const;
};

struct ModifierSimulationStateAtFrame {
  SubFrame frame;
  ModifierSimulationState state;
};

enum class CacheState {
  Valid,
  Invalid,
  Baked,
};

struct StatesAroundFrame {
  const ModifierSimulationStateAtFrame *prev = nullptr;
  const ModifierSimulationStateAtFrame *current = nullptr;
  const ModifierSimulationStateAtFrame *next = nullptr;
};

class ModifierSimulationCache {
 private:
  Vector<std::unique_ptr<ModifierSimulationStateAtFrame>> states_at_frames_;
  std::unique_ptr<BDataSharing> bdata_sharing_;

  friend ModifierSimulationState;

 public:
  CacheState cache_state_ = CacheState::Valid;
  bool failed_finding_bake_ = false;

  void try_discover_bake(StringRefNull meta_dir, StringRefNull bdata_dir);

  bool has_state_at_frame(const SubFrame &frame) const;
  bool has_states() const;
  const ModifierSimulationState *get_state_at_exact_frame(const SubFrame &frame) const;
  ModifierSimulationState &get_state_at_frame_for_write(const SubFrame &frame);
  StatesAroundFrame get_states_around_frame(const SubFrame &frame) const;

  void invalidate()
  {
    cache_state_ = CacheState::Invalid;
  }

  CacheState cache_state() const
  {
    return cache_state_;
  }

  void reset();
};

}  // namespace blender::bke::sim
