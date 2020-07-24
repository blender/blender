/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SIM_SIMULATION_SOLVER_INFLUENCES_HH__
#define __SIM_SIMULATION_SOLVER_INFLUENCES_HH__

#include "BLI_float3.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_span.hh"

#include "DNA_simulation_types.h"

#include "FN_attributes_ref.hh"

#include "BKE_persistent_data_handle.hh"
#include "BKE_simulation.h"

#include "particle_allocator.hh"
#include "time_interval.hh"

namespace blender::sim {

using fn::AttributesInfo;
using fn::AttributesInfoBuilder;
using fn::AttributesRef;
using fn::CPPType;
using fn::GMutableSpan;
using fn::GSpan;
using fn::MutableAttributesRef;

class ParticleEmitterContext;
class ParticleForceContext;
class ParticleActionContext;

class ParticleEmitter {
 public:
  virtual ~ParticleEmitter();
  virtual void emit(ParticleEmitterContext &context) const = 0;
};

class ParticleForce {
 public:
  virtual ~ParticleForce();
  virtual void add_force(ParticleForceContext &context) const = 0;
};

class ParticleAction {
 public:
  virtual ~ParticleAction();
  virtual void execute(ParticleActionContext &context) const = 0;
};

struct SimulationInfluences {
  MultiValueMap<std::string, const ParticleForce *> particle_forces;
  MultiValueMap<std::string, const ParticleAction *> particle_birth_actions;
  Map<std::string, AttributesInfoBuilder *> particle_attributes_builder;
  Vector<const ParticleEmitter *> particle_emitters;
};

class SimulationStateMap {
 private:
  Map<StringRefNull, SimulationState *> states_by_name_;
  MultiValueMap<StringRefNull, SimulationState *> states_by_type_;

 public:
  void add(SimulationState *state)
  {
    states_by_name_.add_new(state->name, state);
    states_by_type_.add(state->type, state);
  }

  template<typename StateType> StateType *lookup(StringRef name) const
  {
    const char *type = BKE_simulation_get_state_type_name<StateType>();
    return (StateType *)this->lookup_name_type(name, type);
  }

  template<typename StateType> Span<StateType *> lookup() const
  {
    const char *type = BKE_simulation_get_state_type_name<StateType>();
    return this->lookup_type(type).cast<StateType *>();
  }

  SimulationState *lookup_name_type(StringRef name, StringRef type) const
  {
    SimulationState *state = states_by_name_.lookup_default_as(name, nullptr);
    if (state == nullptr) {
      return nullptr;
    }
    if (state->type == type) {
      return state;
    }
    return nullptr;
  }

  Span<SimulationState *> lookup_type(StringRef type) const
  {
    return states_by_type_.lookup_as(type);
  }
};

struct SimulationSolveContext {
  Simulation &simulation;
  Depsgraph &depsgraph;
  const SimulationInfluences &influences;
  TimeInterval solve_interval;
  const SimulationStateMap &state_map;
  const bke::PersistentDataHandleMap &handle_map;
};

class ParticleAllocators {
 private:
  Map<std::string, std::unique_ptr<ParticleAllocator>> &allocators_;

 public:
  ParticleAllocators(Map<std::string, std::unique_ptr<ParticleAllocator>> &allocators)
      : allocators_(allocators)
  {
  }

  ParticleAllocator *try_get_allocator(StringRef particle_simulation_name)
  {
    auto *ptr = allocators_.lookup_ptr_as(particle_simulation_name);
    if (ptr != nullptr) {
      return ptr->get();
    }
    else {
      return nullptr;
    }
  }
};

struct MutableParticleChunkContext {
  IndexMask index_mask;
  MutableAttributesRef attributes;
};

struct ParticleChunkContext {
  IndexMask index_mask;
  AttributesRef attributes;
};

struct ParticleEmitterContext {
  SimulationSolveContext &solve_context;
  ParticleAllocators &particle_allocators;
  TimeInterval emit_interval;

  template<typename StateType> StateType *lookup_state(StringRef name)
  {
    return solve_context.state_map.lookup<StateType>(name);
  }

  ParticleAllocator *try_get_particle_allocator(StringRef particle_simulation_name)
  {
    return particle_allocators.try_get_allocator(particle_simulation_name);
  }
};

struct ParticleForceContext {
  SimulationSolveContext &solve_context;
  ParticleChunkContext &particle_chunk_context;
  MutableSpan<float3> force_dst;
};

struct ParticleActionContext {
  SimulationSolveContext &solve_context;
  MutableParticleChunkContext &particle_chunk_context;
};

}  // namespace blender::sim

#endif /* __SIM_SIMULATION_SOLVER_INFLUENCES_HH__ */
