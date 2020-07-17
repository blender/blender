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

class CustomDataAttributesRef {
 private:
  Vector<void *> buffers_;
  uint size_;
  std::unique_ptr<fn::AttributesInfo> info_;

 public:
  CustomDataAttributesRef(CustomData &custom_data, uint size)
  {
    fn::AttributesInfoBuilder builder;
    for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
      buffers_.append(layer.data);
      switch (layer.type) {
        case CD_PROP_INT32: {
          builder.add<int32_t>(layer.name, 0);
          break;
        }
        case CD_PROP_FLOAT3: {
          builder.add<float3>(layer.name, {0, 0, 0});
          break;
        }
      }
    }
    info_ = std::make_unique<fn::AttributesInfo>(builder);
    size_ = size;
  }

  operator fn::MutableAttributesRef()
  {
    return fn::MutableAttributesRef(*info_, buffers_, size_);
  }

  operator fn::AttributesRef() const
  {
    return fn::AttributesRef(*info_, buffers_, size_);
  }
};

static void ensure_attributes_exist(ParticleSimulationState *state)
{
  if (CustomData_get_layer_named(&state->attributes, CD_PROP_FLOAT3, "Position") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_PROP_FLOAT3, CD_CALLOC, nullptr, state->tot_particles, "Position");
  }
  if (CustomData_get_layer_named(&state->attributes, CD_PROP_FLOAT3, "Velocity") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_PROP_FLOAT3, CD_CALLOC, nullptr, state->tot_particles, "Velocity");
  }
  if (CustomData_get_layer_named(&state->attributes, CD_PROP_INT32, "ID") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_PROP_INT32, CD_CALLOC, nullptr, state->tot_particles, "ID");
  }
}

void initialize_simulation_states(Simulation &simulation,
                                  Depsgraph &UNUSED(depsgraph),
                                  const SimulationInfluences &UNUSED(influences))
{
  RandomNumberGenerator rng;

  LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation.states) {
    state->tot_particles = 1000;
    CustomData_realloc(&state->attributes, state->tot_particles);
    ensure_attributes_exist(state);

    CustomDataAttributesRef custom_data_attributes{state->attributes, (uint)state->tot_particles};

    fn::MutableAttributesRef attributes = custom_data_attributes;
    MutableSpan<float3> positions = attributes.get<float3>("Position");
    MutableSpan<float3> velocities = attributes.get<float3>("Velocity");
    MutableSpan<int32_t> ids = attributes.get<int32_t>("ID");

    for (uint i : positions.index_range()) {
      positions[i] = {i / 100.0f, 0, 0};
      velocities[i] = {0, rng.get_float() - 0.5f, rng.get_float() - 0.5f};
      ids[i] = i;
    }
  }
}

void solve_simulation_time_step(Simulation &simulation,
                                Depsgraph &UNUSED(depsgraph),
                                const SimulationInfluences &influences,
                                float time_step)
{
  LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation.states) {
    ensure_attributes_exist(state);

    CustomDataAttributesRef custom_data_attributes{state->attributes, (uint)state->tot_particles};

    fn::MutableAttributesRef attributes = custom_data_attributes;
    MutableSpan<float3> positions = attributes.get<float3>("Position");
    MutableSpan<float3> velocities = attributes.get<float3>("Velocity");

    Array<float3> force_vectors{(uint)state->tot_particles, {0, 0, 0}};
    const Vector<const ParticleForce *> *forces = influences.particle_forces.lookup_ptr(
        state->head.name);
    if (forces != nullptr) {
      for (const ParticleForce *force : *forces) {
        force->add_force(attributes, force_vectors);
      }
    }

    for (uint i : positions.index_range()) {
      velocities[i] += force_vectors[i] * time_step;
      positions[i] += velocities[i] * time_step;
    }
  }
}

}  // namespace blender::sim
