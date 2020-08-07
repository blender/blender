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
#include "BLI_vector_adaptor.hh"

#include "BKE_mesh_runtime.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

namespace blender::sim {

ParticleMeshEmitter::~ParticleMeshEmitter() = default;

struct EmitterSettings {
  Object *object;
  float rate;
};

static BLI_NOINLINE void compute_birth_times(float rate,
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
    if (birth_time > emit_interval.stop()) {
      break;
    }
    if (birth_time <= emit_interval.start()) {
      continue;
    }
    r_birth_times.append(birth_time);
  }
}

static BLI_NOINLINE Span<MLoopTri> get_mesh_triangles(Mesh &mesh)
{
  const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(&mesh);
  int amount = BKE_mesh_runtime_looptri_len(&mesh);
  return Span(triangles, amount);
}

static BLI_NOINLINE void compute_triangle_areas(Mesh &mesh,
                                                Span<MLoopTri> triangles,
                                                MutableSpan<float> r_areas)
{
  assert_same_size(triangles, r_areas);

  for (int i : triangles.index_range()) {
    const MLoopTri &tri = triangles[i];

    const float3 v1 = mesh.mvert[mesh.mloop[tri.tri[0]].v].co;
    const float3 v2 = mesh.mvert[mesh.mloop[tri.tri[1]].v].co;
    const float3 v3 = mesh.mvert[mesh.mloop[tri.tri[2]].v].co;

    const float area = area_tri_v3(v1, v2, v3);
    r_areas[i] = area;
  }
}

static BLI_NOINLINE void compute_triangle_weights(Mesh &mesh,
                                                  Span<MLoopTri> triangles,
                                                  MutableSpan<float> r_weights)
{
  assert_same_size(triangles, r_weights);
  compute_triangle_areas(mesh, triangles, r_weights);
}

static BLI_NOINLINE void compute_cumulative_distribution(Span<float> weights,
                                                         MutableSpan<float> r_cumulative_weights)
{
  BLI_assert(weights.size() + 1 == r_cumulative_weights.size());

  r_cumulative_weights[0] = 0;
  for (int i : weights.index_range()) {
    r_cumulative_weights[i + 1] = r_cumulative_weights[i] + weights[i];
  }
}

static void sample_cumulative_distribution_recursive(RandomNumberGenerator &rng,
                                                     int amount,
                                                     int start,
                                                     int one_after_end,
                                                     Span<float> cumulative_weights,
                                                     VectorAdaptor<int> &r_sampled_indices)
{
  BLI_assert(start <= one_after_end);
  const int size = one_after_end - start;
  if (size == 0) {
    BLI_assert(amount == 0);
  }
  else if (amount == 0) {
    return;
  }
  else if (size == 1) {
    r_sampled_indices.append_n_times(start, amount);
  }
  else {
    const int middle = start + size / 2;
    const float left_weight = cumulative_weights[middle] - cumulative_weights[start];
    const float right_weight = cumulative_weights[one_after_end] - cumulative_weights[middle];
    BLI_assert(left_weight >= 0.0f && right_weight >= 0.0f);
    const float weight_sum = left_weight + right_weight;
    BLI_assert(weight_sum > 0.0f);

    const float left_factor = left_weight / weight_sum;
    const float right_factor = right_weight / weight_sum;

    int left_amount = amount * left_factor;
    int right_amount = amount * right_factor;

    if (left_amount + right_amount < amount) {
      BLI_assert(left_amount + right_amount + 1 == amount);
      const float weight_per_item = weight_sum / amount;
      const float total_remaining_weight = weight_sum -
                                           (left_amount + right_amount) * weight_per_item;
      const float left_remaining_weight = left_weight - left_amount * weight_per_item;
      const float left_remaining_factor = left_remaining_weight / total_remaining_weight;
      if (rng.get_float() < left_remaining_factor) {
        left_amount++;
      }
      else {
        right_amount++;
      }
    }

    sample_cumulative_distribution_recursive(
        rng, left_amount, start, middle, cumulative_weights, r_sampled_indices);
    sample_cumulative_distribution_recursive(
        rng, right_amount, middle, one_after_end, cumulative_weights, r_sampled_indices);
  }
}

static BLI_NOINLINE void sample_cumulative_distribution(RandomNumberGenerator &rng,
                                                        Span<float> cumulative_weights,
                                                        MutableSpan<int> r_samples)
{
  VectorAdaptor<int> sampled_indices(r_samples);
  sample_cumulative_distribution_recursive(rng,
                                           r_samples.size(),
                                           0,
                                           cumulative_weights.size() - 1,
                                           cumulative_weights,
                                           sampled_indices);
  BLI_assert(sampled_indices.is_full());
}

static BLI_NOINLINE bool sample_weighted_buckets(RandomNumberGenerator &rng,
                                                 Span<float> weights,
                                                 MutableSpan<int> r_samples)
{
  Array<float> cumulative_weights(weights.size() + 1);
  compute_cumulative_distribution(weights, cumulative_weights);

  if (r_samples.size() > 0 && cumulative_weights.as_span().last() == 0.0f) {
    /* All weights are zero. */
    return false;
  }

  sample_cumulative_distribution(rng, cumulative_weights, r_samples);
  return true;
}

static BLI_NOINLINE void sample_looptris(RandomNumberGenerator &rng,
                                         Mesh &mesh,
                                         Span<MLoopTri> triangles,
                                         Span<int> triangles_to_sample,
                                         MutableSpan<float3> r_sampled_positions,
                                         MutableSpan<float3> r_sampled_normals)
{
  assert_same_size(triangles_to_sample, r_sampled_positions, r_sampled_normals);

  MLoop *loops = mesh.mloop;
  MVert *verts = mesh.mvert;

  for (uint i : triangles_to_sample.index_range()) {
    const uint triangle_index = triangles_to_sample[i];
    const MLoopTri &triangle = triangles[triangle_index];

    const float3 v1 = verts[loops[triangle.tri[0]].v].co;
    const float3 v2 = verts[loops[triangle.tri[1]].v].co;
    const float3 v3 = verts[loops[triangle.tri[2]].v].co;

    const float3 bary_coords = rng.get_barycentric_coordinates();

    float3 position;
    interp_v3_v3v3v3(position, v1, v2, v3, bary_coords);

    float3 normal;
    normal_tri_v3(normal, v1, v2, v3);

    r_sampled_positions[i] = position;
    r_sampled_normals[i] = normal;
  }
}

static BLI_NOINLINE bool compute_new_particle_attributes(ParticleEmitterContext &context,
                                                         EmitterSettings &settings,
                                                         ParticleMeshEmitterSimulationState &state,
                                                         Vector<float3> &r_positions,
                                                         Vector<float3> &r_velocities,
                                                         Vector<float> &r_birth_times)
{
  if (settings.object == nullptr) {
    return false;
  }
  if (settings.rate <= 0.000001f) {
    return false;
  }
  if (settings.object->type != OB_MESH) {
    return false;
  }
  Mesh &mesh = *static_cast<Mesh *>(settings.object->data);
  if (mesh.totvert == 0) {
    return false;
  }

  const float start_time = context.emit_interval.start();
  const uint32_t seed = DefaultHash<StringRef>{}(state.head.name);
  RandomNumberGenerator rng{*reinterpret_cast<const uint32_t *>(&start_time) ^ seed};

  compute_birth_times(settings.rate, context.emit_interval, state, r_birth_times);
  const int particle_amount = r_birth_times.size();
  if (particle_amount == 0) {
    return false;
  }

  const float last_birth_time = r_birth_times.last();
  rng.shuffle(r_birth_times.as_mutable_span());

  Span<MLoopTri> triangles = get_mesh_triangles(mesh);
  if (triangles.is_empty()) {
    return false;
  }

  Array<float> triangle_weights(triangles.size());
  compute_triangle_weights(mesh, triangles, triangle_weights);

  Array<int> triangles_to_sample(particle_amount);
  if (!sample_weighted_buckets(rng, triangle_weights, triangles_to_sample)) {
    return false;
  }

  r_positions.resize(particle_amount);
  r_velocities.resize(particle_amount);
  sample_looptris(rng, mesh, triangles, triangles_to_sample, r_positions, r_velocities);

  if (context.solve_context.dependency_animations.is_object_transform_changing(*settings.object)) {
    Array<float4x4> local_to_world_matrices(particle_amount);
    context.solve_context.dependency_animations.get_object_transforms(
        *settings.object, r_birth_times, local_to_world_matrices);

    for (int i : IndexRange(particle_amount)) {
      const float4x4 &position_to_world = local_to_world_matrices[i];
      const float4x4 normal_to_world = position_to_world.inverted_transposed_affine();
      r_positions[i] = position_to_world * r_positions[i];
      r_velocities[i] = normal_to_world * r_velocities[i];
    }
  }
  else {
    const float4x4 position_to_world = settings.object->obmat;
    const float4x4 normal_to_world = position_to_world.inverted_transposed_affine();
    for (int i : IndexRange(particle_amount)) {
      r_positions[i] = position_to_world * r_positions[i];
      r_velocities[i] = normal_to_world * r_velocities[i];
    }
  }

  for (int i : IndexRange(particle_amount)) {
    r_velocities[i].normalize();
  }

  state.last_birth_time = last_birth_time;
  return true;
}

static BLI_NOINLINE EmitterSettings compute_settings(const fn::MultiFunction &inputs_fn,
                                                     ParticleEmitterContext &context)
{
  EmitterSettings parameters;

  fn::MFContextBuilder mf_context;
  mf_context.add_global_context("PersistentDataHandleMap", &context.solve_context.handle_map);

  fn::MFParamsBuilder mf_params{inputs_fn, 1};
  bke::PersistentObjectHandle object_handle;
  mf_params.add_uninitialized_single_output(&object_handle, "Object");
  mf_params.add_uninitialized_single_output(&parameters.rate, "Rate");

  inputs_fn.call(IndexRange(1), mf_params, mf_context);

  parameters.object = context.solve_context.handle_map.lookup(object_handle);
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

  if (!compute_new_particle_attributes(
          context, settings, *state, new_positions, new_velocities, new_birth_times)) {
    return;
  }

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

    if (action_ != nullptr) {
      ParticleChunkContext particles{
          *context.solve_context.state_map.lookup<ParticleSimulationState>(name),
          IndexRange(amount),
          attributes,
          nullptr};
      ParticleActionContext action_context{context.solve_context, particles};
      action_->execute(action_context);
    }
  }
}

}  // namespace blender::sim
