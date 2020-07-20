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

#include "BLI_rand.hh"

namespace blender::sim {

ParticleForce::~ParticleForce()
{
}

ParticleEmitter::~ParticleEmitter()
{
}

static CustomDataType cpp_to_custom_data_type(const fn::CPPType &type)
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

static const fn::CPPType &custom_to_cpp_data_type(CustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT3:
      return fn::CPPType::get<float3>();
    case CD_PROP_FLOAT:
      return fn::CPPType::get<float>();
    case CD_PROP_INT32:
      return fn::CPPType::get<int32_t>();
    default:
      BLI_assert(false);
      return fn::CPPType::get<float>();
  }
}

class CustomDataAttributesRef {
 private:
  Array<void *> buffers_;
  int64_t size_;
  const fn::AttributesInfo &info_;

 public:
  CustomDataAttributesRef(CustomData &custom_data, int64_t size, const fn::AttributesInfo &info)
      : buffers_(info.size(), nullptr), size_(size), info_(info)
  {
    for (int attribute_index : info.index_range()) {
      StringRefNull name = info.name_of(attribute_index);
      const fn::CPPType &cpp_type = info.type_of(attribute_index);
      CustomDataType custom_type = cpp_to_custom_data_type(cpp_type);
      void *data = CustomData_get_layer_named(&custom_data, custom_type, name.c_str());
      buffers_[attribute_index] = data;
    }
  }

  operator fn::MutableAttributesRef()
  {
    return fn::MutableAttributesRef(info_, buffers_, size_);
  }

  operator fn::AttributesRef() const
  {
    return fn::AttributesRef(info_, buffers_, size_);
  }
};

static void ensure_attributes_exist(ParticleSimulationState *state, const fn::AttributesInfo &info)
{
  bool found_layer_to_remove;
  do {
    found_layer_to_remove = false;
    for (int layer_index = 0; layer_index < state->attributes.totlayer; layer_index++) {
      CustomDataLayer *layer = &state->attributes.layers[layer_index];
      BLI_assert(layer->name != nullptr);
      const fn::CPPType &cpp_type = custom_to_cpp_data_type((CustomDataType)layer->type);
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
    const fn::CPPType &cpp_type = info.type_of(attribute_index);
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

BLI_NOINLINE static void simulate_existing_particles(SimulationSolveContext &solve_context,
                                                     ParticleSimulationState &state,
                                                     const fn::AttributesInfo &attributes_info)
{
  CustomDataAttributesRef custom_data_attributes{
      state.attributes, state.tot_particles, attributes_info};
  fn::MutableAttributesRef attributes = custom_data_attributes;

  Array<float3> force_vectors{state.tot_particles, {0, 0, 0}};
  const Vector<const ParticleForce *> *forces =
      solve_context.influences().particle_forces.lookup_ptr(state.head.name);

  if (forces != nullptr) {
    ParticleChunkContext particle_chunk_context{IndexMask(state.tot_particles), attributes};
    ParticleForceContext particle_force_context{
        solve_context, particle_chunk_context, force_vectors};

    for (const ParticleForce *force : *forces) {
      force->add_force(particle_force_context);
    }
  }

  MutableSpan<float3> positions = attributes.get<float3>("Position");
  MutableSpan<float3> velocities = attributes.get<float3>("Velocity");
  MutableSpan<float> birth_times = attributes.get<float>("Birth Time");
  MutableSpan<int> dead_states = attributes.get<int>("Dead");
  float end_time = solve_context.solve_interval().end();
  float time_step = solve_context.solve_interval().duration();
  for (int i : positions.index_range()) {
    velocities[i] += force_vectors[i] * time_step;
    positions[i] += velocities[i] * time_step;

    if (end_time - birth_times[i] > 2) {
      dead_states[i] = true;
    }
  }
}

BLI_NOINLINE static void run_emitters(SimulationSolveContext &solve_context,
                                      ParticleAllocators &particle_allocators)
{
  for (const ParticleEmitter *emitter : solve_context.influences().particle_emitters) {
    ParticleEmitterContext emitter_context{
        solve_context, particle_allocators, solve_context.solve_interval()};
    emitter->emit(emitter_context);
  }
}

BLI_NOINLINE static int count_particles_after_time_step(ParticleSimulationState &state,
                                                        ParticleAllocator &allocator)
{
  CustomDataAttributesRef custom_data_attributes{
      state.attributes, state.tot_particles, allocator.attributes_info()};
  fn::MutableAttributesRef attributes = custom_data_attributes;
  int new_particle_amount = attributes.get<int>("Dead").count(0);

  for (fn::MutableAttributesRef emitted_attributes : allocator.get_allocations()) {
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

  Vector<fn::MutableAttributesRef> particle_sources;
  particle_sources.append(custom_data_attributes);
  particle_sources.extend(allocator.get_allocations());

  CustomDataLayer *dead_layer = nullptr;

  for (CustomDataLayer &layer : MutableSpan(state.attributes.layers, state.attributes.totlayer)) {
    StringRefNull name = layer.name;
    if (name == "Dead") {
      dead_layer = &layer;
      continue;
    }
    const fn::CPPType &cpp_type = custom_to_cpp_data_type((CustomDataType)layer.type);
    fn::GMutableSpan new_buffer{
        cpp_type,
        MEM_mallocN_aligned(new_particle_amount * cpp_type.size(), cpp_type.alignment(), AT),
        new_particle_amount};

    int current = 0;
    for (fn::MutableAttributesRef attributes : particle_sources) {
      Span<int> dead_states = attributes.get<int>("Dead");
      fn::GSpan source_buffer = attributes.get(name);
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
    layer.data = new_buffer.buffer();
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
                                  const SimulationInfluences &UNUSED(influences))
{
  simulation.current_simulation_time = 0.0f;
}

void solve_simulation_time_step(Simulation &simulation,
                                Depsgraph &depsgraph,
                                const SimulationInfluences &influences,
                                float time_step)
{
  SimulationSolveContext solve_context{
      simulation,
      depsgraph,
      influences,
      TimeInterval(simulation.current_simulation_time, time_step)};
  TimeInterval simulation_time_interval{simulation.current_simulation_time, time_step};

  Vector<SimulationState *> simulation_states{simulation.states};
  Vector<ParticleSimulationState *> particle_simulation_states;
  for (SimulationState *state : simulation_states) {
    if (state->type == SIM_STATE_TYPE_PARTICLES) {
      particle_simulation_states.append((ParticleSimulationState *)state);
    }
  }

  Map<std::string, std::unique_ptr<fn::AttributesInfo>> attribute_infos;
  Map<std::string, std::unique_ptr<ParticleAllocator>> particle_allocators_map;
  for (ParticleSimulationState *state : particle_simulation_states) {
    const fn::AttributesInfoBuilder &builder = *influences.particle_attributes_builder.lookup_as(
        state->head.name);
    auto info = std::make_unique<fn::AttributesInfo>(builder);

    ensure_attributes_exist(state, *info);

    particle_allocators_map.add_new(
        state->head.name, std::make_unique<ParticleAllocator>(*info, state->next_particle_id));
    attribute_infos.add_new(state->head.name, std::move(info));
  }

  ParticleAllocators particle_allocators{particle_allocators_map};

  for (ParticleSimulationState *state : particle_simulation_states) {
    const fn::AttributesInfo &attributes_info = *attribute_infos.lookup_as(state->head.name);
    simulate_existing_particles(solve_context, *state, attributes_info);
  }

  run_emitters(solve_context, particle_allocators);

  for (ParticleSimulationState *state : particle_simulation_states) {
    ParticleAllocator &allocator = *particle_allocators.try_get_allocator(state->head.name);
    remove_dead_and_add_new_particles(*state, allocator);
  }

  simulation.current_simulation_time = simulation_time_interval.end();
}

}  // namespace blender::sim
