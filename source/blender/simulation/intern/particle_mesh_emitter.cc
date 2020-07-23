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

#include "particle_mesh_emitter.hh"

#include "BLI_float4x4.hh"
#include "BLI_rand.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

namespace blender::sim {

struct EmitterSettings {
  const Object *object;
  float rate;
};

static void compute_birth_times(float rate,
                                TimeInterval emit_interval,
                                ParticleMeshEmitterSimulationState &state,
                                Vector<float> &r_birth_times)
{
  const float time_between_particles = 1.0f / rate;
  int counter = 0;
  while (true) {
    counter++;
    const float time_offset = counter * time_between_particles;
    const float birth_time = state.last_birth_time + time_offset;
    if (birth_time > emit_interval.end()) {
      break;
    }
    if (birth_time <= emit_interval.start()) {
      continue;
    }
    r_birth_times.append(birth_time);
  }
}

static void compute_new_particle_attributes(EmitterSettings &settings,
                                            TimeInterval emit_interval,
                                            ParticleMeshEmitterSimulationState &state,
                                            Vector<float3> &r_positions,
                                            Vector<float3> &r_velocities,
                                            Vector<float> &r_birth_times)
{
  if (settings.object == nullptr) {
    return;
  }
  if (settings.rate <= 0.000001f) {
    return;
  }
  if (settings.object->type != OB_MESH) {
    return;
  }
  const Mesh &mesh = *(Mesh *)settings.object->data;
  if (mesh.totvert == 0) {
    return;
  }

  float4x4 local_to_world = settings.object->obmat;

  const float start_time = emit_interval.start();
  const uint32_t seed = DefaultHash<StringRef>{}(state.head.name);
  RandomNumberGenerator rng{(*(uint32_t *)&start_time) ^ seed};

  compute_birth_times(settings.rate, emit_interval, state, r_birth_times);
  if (r_birth_times.is_empty()) {
    return;
  }

  state.last_birth_time = r_birth_times.last();

  for (int i : r_birth_times.index_range()) {
    UNUSED_VARS(i);
    const int vertex_index = rng.get_int32() % mesh.totvert;
    float3 vertex_position = mesh.mvert[vertex_index].co;
    r_positions.append(local_to_world * vertex_position);
    r_velocities.append(rng.get_unit_float3());
  }
}

static EmitterSettings compute_settings(const fn::MultiFunction &inputs_fn,
                                        ParticleEmitterContext &context)
{
  EmitterSettings parameters;

  fn::MFContextBuilder mf_context;
  mf_context.add_global_context("PersistentDataHandleMap", &context.solve_context().handle_map());

  fn::MFParamsBuilder mf_params{inputs_fn, 1};
  bke::PersistentObjectHandle object_handle;
  mf_params.add_uninitialized_single_output(&object_handle, "Object");
  mf_params.add_uninitialized_single_output(&parameters.rate, "Rate");

  inputs_fn.call(IndexRange(1), mf_params, mf_context);

  parameters.object = context.solve_context().handle_map().lookup(object_handle);
  return parameters;
}

void ParticleMeshEmitter::emit(ParticleEmitterContext &context) const
{
  auto *state = context.lookup_state<ParticleMeshEmitterSimulationState>(own_state_name_);
  if (state == nullptr) {
    return;
  }

  EmitterSettings settings = compute_settings(inputs_fn_, context);

  Vector<float3> new_positions;
  Vector<float3> new_velocities;
  Vector<float> new_birth_times;

  compute_new_particle_attributes(
      settings, context.emit_interval(), *state, new_positions, new_velocities, new_birth_times);

  for (StringRef name : particle_names_) {
    ParticleAllocator *allocator = context.try_get_particle_allocator(name);
    if (allocator == nullptr) {
      continue;
    }

    int amount = new_positions.size();
    fn::MutableAttributesRef attributes = allocator->allocate(amount);

    attributes.get<float3>("Position").copy_from(new_positions);
    attributes.get<float3>("Velocity").copy_from(new_velocities);
    attributes.get<float>("Birth Time").copy_from(new_birth_times);
  }
}

}  // namespace blender::sim
