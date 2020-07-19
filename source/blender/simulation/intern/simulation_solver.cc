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
  uint size_;
  const fn::AttributesInfo &info_;

 public:
  CustomDataAttributesRef(CustomData &custom_data, uint size, const fn::AttributesInfo &info)
      : buffers_(info.size(), nullptr), size_(size), info_(info)
  {
    for (uint attribute_index : info.index_range()) {
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

  for (uint attribute_index : info.index_range()) {
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
      cpp_type.fill_uninitialized(
          info.default_of(attribute_index), data, (uint)state->tot_particles);
    }
  }
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
  SimulationSolveContext solve_context{simulation, depsgraph, influences};
  TimeInterval simulation_time_interval{simulation.current_simulation_time, time_step};

  Map<std::string, std::unique_ptr<fn::AttributesInfo>> attribute_infos;
  Map<std::string, std::unique_ptr<ParticleAllocator>> particle_allocators;
  LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation.states) {
    const fn::AttributesInfoBuilder &builder = *influences.particle_attributes_builder.lookup_as(
        state->head.name);
    auto info = std::make_unique<fn::AttributesInfo>(builder);

    ensure_attributes_exist(state, *info);

    particle_allocators.add_new(
        state->head.name, std::make_unique<ParticleAllocator>(*info, state->next_particle_id));
    attribute_infos.add_new(state->head.name, std::move(info));
  }

  LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation.states) {
    const fn::AttributesInfo &attributes_info = *attribute_infos.lookup_as(state->head.name);
    CustomDataAttributesRef custom_data_attributes{
        state->attributes, (uint)state->tot_particles, attributes_info};
    fn::MutableAttributesRef attributes = custom_data_attributes;

    MutableSpan<float3> positions = attributes.get<float3>("Position");
    MutableSpan<float3> velocities = attributes.get<float3>("Velocity");

    Array<float3> force_vectors{(uint)state->tot_particles, {0, 0, 0}};
    const Vector<const ParticleForce *> *forces = influences.particle_forces.lookup_ptr(
        state->head.name);

    if (forces != nullptr) {
      ParticleChunkContext particle_chunk_context{IndexMask((uint)state->tot_particles),
                                                  attributes};
      ParticleForceContext particle_force_context{
          solve_context, particle_chunk_context, force_vectors};

      for (const ParticleForce *force : *forces) {
        force->add_force(particle_force_context);
      }
    }

    for (uint i : positions.index_range()) {
      velocities[i] += force_vectors[i] * time_step;
      positions[i] += velocities[i] * time_step;
    }
  }

  for (const ParticleEmitter *emitter : influences.particle_emitters) {
    ParticleEmitterContext emitter_context{
        solve_context, particle_allocators, simulation_time_interval};
    emitter->emit(emitter_context);
  }

  LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation.states) {
    const fn::AttributesInfo &attributes_info = *attribute_infos.lookup_as(state->head.name);
    ParticleAllocator &allocator = *particle_allocators.lookup_as(state->head.name);

    const uint emitted_particle_amount = allocator.total_allocated();
    const uint old_particle_amount = state->tot_particles;
    const uint new_particle_amount = old_particle_amount + emitted_particle_amount;

    CustomData_realloc(&state->attributes, new_particle_amount);

    CustomDataAttributesRef custom_data_attributes{
        state->attributes, new_particle_amount, attributes_info};
    fn::MutableAttributesRef attributes = custom_data_attributes;

    uint offset = old_particle_amount;
    for (fn::MutableAttributesRef emitted_attributes : allocator.get_allocations()) {
      fn::MutableAttributesRef dst_attributes = attributes.slice(
          IndexRange(offset, emitted_attributes.size()));
      for (uint attribute_index : attributes.info().index_range()) {
        fn::GMutableSpan emitted_data = emitted_attributes.get(attribute_index);
        fn::GMutableSpan dst = dst_attributes.get(attribute_index);
        const fn::CPPType &type = dst.type();
        type.copy_to_uninitialized_n(
            emitted_data.buffer(), dst.buffer(), emitted_attributes.size());
      }
      offset += emitted_attributes.size();
    }

    state->tot_particles = new_particle_amount;
    state->next_particle_id += emitted_particle_amount;
  }

  simulation.current_simulation_time = simulation_time_interval.end();
}

}  // namespace blender::sim
