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

#include "simulation_solver.hh"

#include "BKE_customdata.h"
#include "BKE_persistent_data_handle.hh"

#include "BLI_rand.hh"
#include "BLI_set.hh"

#include "DEG_depsgraph_query.h"

namespace blender::sim {

static CustomDataType cpp_to_custom_data_type(const CPPType &type)
{
  if (type.is<float3>()) {
    return CD_PROP_FLOAT3;
  }
  if (type.is<float>()) {
    return CD_PROP_FLOAT;
  }
  if (type.is<int32_t>()) {
    return CD_PROP_INT32;
  }
  BLI_assert(false);
  return CD_PROP_FLOAT;
}

static const CPPType &custom_to_cpp_data_type(CustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT3:
      return CPPType::get<float3>();
    case CD_PROP_FLOAT:
      return CPPType::get<float>();
    case CD_PROP_INT32:
      return CPPType::get<int32_t>();
    default:
      BLI_assert(false);
      return CPPType::get<float>();
  }
}

class CustomDataAttributesRef {
 private:
  Array<void *> buffers_;
  int64_t size_;
  const AttributesInfo &info_;

 public:
  CustomDataAttributesRef(CustomData &custom_data, int64_t size, const AttributesInfo &info)
      : buffers_(info.size(), nullptr), size_(size), info_(info)
  {
    for (int attribute_index : info.index_range()) {
      StringRefNull name = info.name_of(attribute_index);
      const CPPType &cpp_type = info.type_of(attribute_index);
      CustomDataType custom_type = cpp_to_custom_data_type(cpp_type);
      void *data = CustomData_get_layer_named(&custom_data, custom_type, name.c_str());
      buffers_[attribute_index] = data;
    }
  }

  operator MutableAttributesRef()
  {
    return MutableAttributesRef(info_, buffers_, size_);
  }

  operator AttributesRef() const
  {
    return AttributesRef(info_, buffers_, size_);
  }
};

static void ensure_attributes_exist(ParticleSimulationState *state, const AttributesInfo &info)
{
  bool found_layer_to_remove;
  do {
    found_layer_to_remove = false;
    for (int layer_index = 0; layer_index < state->attributes.totlayer; layer_index++) {
      CustomDataLayer *layer = &state->attributes.layers[layer_index];
      BLI_assert(layer->name != nullptr);
      const CPPType &cpp_type = custom_to_cpp_data_type((CustomDataType)layer->type);
      StringRefNull name = layer->name;
      if (!info.has_attribute(name, cpp_type)) {
        found_layer_to_remove = true;
        CustomData_free_layer(&state->attributes, layer->type, state->tot_particles, layer_index);
        break;
      }
    }
  } while (found_layer_to_remove);

  for (int attribute_index : info.index_range()) {
    StringRefNull attribute_name = info.name_of(attribute_index);
    const CPPType &cpp_type = info.type_of(attribute_index);
    CustomDataType custom_type = cpp_to_custom_data_type(cpp_type);
    if (CustomData_get_layer_named(&state->attributes, custom_type, attribute_name.c_str()) ==
        nullptr) {
      void *data = CustomData_add_layer_named(&state->attributes,
                                              custom_type,
                                              CD_CALLOC,
                                              nullptr,
                                              state->tot_particles,
                                              attribute_name.c_str());
      cpp_type.fill_uninitialized(info.default_of(attribute_index), data, state->tot_particles);
    }
  }
}

BLI_NOINLINE static void simulate_particle_chunk(SimulationSolveContext &solve_context,
                                                 ParticleSimulationState &state,
                                                 MutableAttributesRef attributes,
                                                 MutableSpan<float> remaining_durations,
                                                 float end_time)
{
  int particle_amount = attributes.size();

  Span<const ParticleAction *> begin_actions =
      solve_context.influences.particle_time_step_begin_actions.lookup_as(state.head.name);
  for (const ParticleAction *action : begin_actions) {
    ParticleChunkContext particles{IndexMask(particle_amount), attributes};
    ParticleActionContext action_context{solve_context, particles};
    action->execute(action_context);
  }

  Array<float3> force_vectors{particle_amount, {0, 0, 0}};
  Span<const ParticleForce *> forces = solve_context.influences.particle_forces.lookup_as(
      state.head.name);
  for (const ParticleForce *force : forces) {
    ParticleChunkContext particles{IndexMask(particle_amount), attributes};
    ParticleForceContext particle_force_context{solve_context, particles, force_vectors};
    force->add_force(particle_force_context);
  }

  MutableSpan<float3> positions = attributes.get<float3>("Position");
  MutableSpan<float3> velocities = attributes.get<float3>("Velocity");
  MutableSpan<float> birth_times = attributes.get<float>("Birth Time");
  MutableSpan<int> dead_states = attributes.get<int>("Dead");

  for (int i : IndexRange(particle_amount)) {
    const float time_step = remaining_durations[i];
    velocities[i] += force_vectors[i] * time_step;
    positions[i] += velocities[i] * time_step;

    if (end_time - birth_times[i] > 2) {
      dead_states[i] = true;
    }
  }

  Span<const ParticleAction *> end_actions =
      solve_context.influences.particle_time_step_end_actions.lookup_as(state.head.name);
  for (const ParticleAction *action : end_actions) {
    ParticleChunkContext particles{IndexMask(particle_amount), attributes};
    ParticleActionContext action_context{solve_context, particles};
    action->execute(action_context);
  }
}

BLI_NOINLINE static void simulate_existing_particles(SimulationSolveContext &solve_context,
                                                     ParticleSimulationState &state,
                                                     const AttributesInfo &attributes_info)
{
  CustomDataAttributesRef custom_data_attributes{
      state.attributes, state.tot_particles, attributes_info};
  MutableAttributesRef attributes = custom_data_attributes;

  Array<float> remaining_durations(state.tot_particles, solve_context.solve_interval.duration());
  simulate_particle_chunk(
      solve_context, state, attributes, remaining_durations, solve_context.solve_interval.stop());
}

BLI_NOINLINE static void run_emitters(SimulationSolveContext &solve_context,
                                      ParticleAllocators &particle_allocators)
{
  for (const ParticleEmitter *emitter : solve_context.influences.particle_emitters) {
    ParticleEmitterContext emitter_context{
        solve_context, particle_allocators, solve_context.solve_interval};
    emitter->emit(emitter_context);
  }
}

BLI_NOINLINE static int count_particles_after_time_step(ParticleSimulationState &state,
                                                        ParticleAllocator &allocator)
{
  CustomDataAttributesRef custom_data_attributes{
      state.attributes, state.tot_particles, allocator.attributes_info()};
  MutableAttributesRef attributes = custom_data_attributes;
  int new_particle_amount = attributes.get<int>("Dead").count(0);

  for (MutableAttributesRef emitted_attributes : allocator.get_allocations()) {
    new_particle_amount += emitted_attributes.get<int>("Dead").count(0);
  }

  return new_particle_amount;
}

BLI_NOINLINE static void remove_dead_and_add_new_particles(ParticleSimulationState &state,
                                                           ParticleAllocator &allocator)
{
  const int new_particle_amount = count_particles_after_time_step(state, allocator);

  CustomDataAttributesRef custom_data_attributes{
      state.attributes, state.tot_particles, allocator.attributes_info()};

  Vector<MutableAttributesRef> particle_sources;
  particle_sources.append(custom_data_attributes);
  particle_sources.extend(allocator.get_allocations());

  CustomDataLayer *dead_layer = nullptr;

  for (CustomDataLayer &layer : MutableSpan(state.attributes.layers, state.attributes.totlayer)) {
    StringRefNull name = layer.name;
    if (name == "Dead") {
      dead_layer = &layer;
      continue;
    }
    const CPPType &cpp_type = custom_to_cpp_data_type((CustomDataType)layer.type);
    GMutableSpan new_buffer{
        cpp_type,
        MEM_mallocN_aligned(new_particle_amount * cpp_type.size(), cpp_type.alignment(), AT),
        new_particle_amount};

    int current = 0;
    for (MutableAttributesRef attributes : particle_sources) {
      Span<int> dead_states = attributes.get<int>("Dead");
      GSpan source_buffer = attributes.get(name);
      BLI_assert(source_buffer.type() == cpp_type);
      for (int i : attributes.index_range()) {
        if (dead_states[i] == 0) {
          cpp_type.copy_to_uninitialized(source_buffer[i], new_buffer[current]);
          current++;
        }
      }
    }

    if (layer.data != nullptr) {
      MEM_freeN(layer.data);
    }
    layer.data = new_buffer.data();
  }

  BLI_assert(dead_layer != nullptr);
  if (dead_layer->data != nullptr) {
    MEM_freeN(dead_layer->data);
  }
  dead_layer->data = MEM_callocN(sizeof(int) * new_particle_amount, AT);

  state.tot_particles = new_particle_amount;
  state.next_particle_id += allocator.total_allocated();
}

void initialize_simulation_states(Simulation &simulation,
                                  Depsgraph &UNUSED(depsgraph),
                                  const SimulationInfluences &UNUSED(influences),
                                  const bke::PersistentDataHandleMap &UNUSED(handle_map))
{
  simulation.current_simulation_time = 0.0f;
}

void solve_simulation_time_step(Simulation &simulation,
                                Depsgraph &depsgraph,
                                const SimulationInfluences &influences,
                                const bke::PersistentDataHandleMap &handle_map,
                                const DependencyAnimations &dependency_animations,
                                float time_step)
{
  SimulationStateMap state_map;
  LISTBASE_FOREACH (SimulationState *, state, &simulation.states) {
    state_map.add(state);
  }

  SimulationSolveContext solve_context{simulation,
                                       depsgraph,
                                       influences,
                                       TimeInterval(simulation.current_simulation_time, time_step),
                                       state_map,
                                       handle_map,
                                       dependency_animations};

  Span<ParticleSimulationState *> particle_simulation_states =
      state_map.lookup<ParticleSimulationState>();

  Map<std::string, std::unique_ptr<AttributesInfo>> attribute_infos;
  Map<std::string, std::unique_ptr<ParticleAllocator>> particle_allocators_map;
  for (ParticleSimulationState *state : particle_simulation_states) {
    const AttributesInfoBuilder &builder = *influences.particle_attributes_builder.lookup_as(
        state->head.name);
    auto info = std::make_unique<AttributesInfo>(builder);

    ensure_attributes_exist(state, *info);

    uint32_t hash_seed = DefaultHash<StringRef>{}(state->head.name);
    particle_allocators_map.add_new(
        state->head.name,
        std::make_unique<ParticleAllocator>(*info, state->next_particle_id, hash_seed));
    attribute_infos.add_new(state->head.name, std::move(info));
  }

  ParticleAllocators particle_allocators{particle_allocators_map};

  for (ParticleSimulationState *state : particle_simulation_states) {
    const AttributesInfo &attributes_info = *attribute_infos.lookup_as(state->head.name);
    simulate_existing_particles(solve_context, *state, attributes_info);
  }

  run_emitters(solve_context, particle_allocators);

  for (ParticleSimulationState *state : particle_simulation_states) {
    ParticleAllocator &allocator = *particle_allocators.try_get_allocator(state->head.name);

    for (MutableAttributesRef attributes : allocator.get_allocations()) {
      Span<const ParticleAction *> actions = influences.particle_birth_actions.lookup_as(
          state->head.name);
      for (const ParticleAction *action : actions) {
        ParticleChunkContext chunk_context{IndexRange(attributes.size()), attributes};
        ParticleActionContext action_context{solve_context, chunk_context};
        action->execute(action_context);
      }
    }
  }

  for (ParticleSimulationState *state : particle_simulation_states) {
    ParticleAllocator &allocator = *particle_allocators.try_get_allocator(state->head.name);

    for (MutableAttributesRef attributes : allocator.get_allocations()) {
      Array<float> remaining_durations(attributes.size());
      Span<float> birth_times = attributes.get<float>("Birth Time");
      const float end_time = solve_context.solve_interval.stop();
      for (int i : attributes.index_range()) {
        remaining_durations[i] = end_time - birth_times[i];
      }
      simulate_particle_chunk(solve_context, *state, attributes, remaining_durations, end_time);
    }

    remove_dead_and_add_new_particles(*state, allocator);
  }

  simulation.current_simulation_time = solve_context.solve_interval.stop();
}

}  // namespace blender::sim
