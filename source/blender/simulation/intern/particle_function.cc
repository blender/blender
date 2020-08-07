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

#include "particle_function.hh"

namespace blender::sim {

ParticleFunction::ParticleFunction(const fn::MultiFunction *global_fn,
                                   const fn::MultiFunction *per_particle_fn,
                                   Span<const ParticleFunctionInput *> global_inputs,
                                   Span<const ParticleFunctionInput *> per_particle_inputs,
                                   Span<bool> output_is_global)
    : global_fn_(global_fn),
      per_particle_fn_(per_particle_fn),
      global_inputs_(global_inputs),
      per_particle_inputs_(per_particle_inputs),
      output_is_global_(output_is_global)
{
  for (int i : output_is_global_.index_range()) {
    if (output_is_global_[i]) {
      int param_index = global_inputs_.size() + global_output_indices_.size();
      fn::MFParamType param_type = global_fn_->param_type(param_index);
      BLI_assert(param_type.is_output());
      output_types_.append(param_type.data_type());
      output_names_.append(global_fn_->param_name(param_index));
      global_output_indices_.append(i);
    }
    else {
      int param_index = per_particle_inputs_.size() + per_particle_output_indices_.size();
      fn::MFParamType param_type = per_particle_fn_->param_type(param_index);
      BLI_assert(param_type.is_output());
      output_types_.append(param_type.data_type());
      output_names_.append(per_particle_fn_->param_name(param_index));
      per_particle_output_indices_.append(i);
    }
  }
}

ParticleFunctionEvaluator::ParticleFunctionEvaluator(const ParticleFunction &particle_fn,
                                                     const SimulationSolveContext &solve_context,
                                                     const ParticleChunkContext &particles)
    : particle_fn_(particle_fn),
      solve_context_(solve_context),
      particles_(particles),
      mask_(particles_.index_mask),
      outputs_(particle_fn_.output_types_.size(), nullptr)
{
  global_context_.add_global_context("PersistentDataHandleMap", &solve_context_.handle_map);
  per_particle_context_.add_global_context("PersistentDataHandleMap", &solve_context_.handle_map);
}

ParticleFunctionEvaluator::~ParticleFunctionEvaluator()
{
  for (int output_index : outputs_.index_range()) {
    void *buffer = outputs_[output_index];
    fn::MFDataType data_type = particle_fn_.output_types_[output_index];
    BLI_assert(data_type.is_single()); /* For now. */
    const fn::CPPType &type = data_type.single_type();

    if (particle_fn_.output_is_global_[output_index]) {
      type.destruct(buffer);
    }
    else {
      type.destruct_indices(outputs_[0], mask_);
    }
  }
}

void ParticleFunctionEvaluator::compute()
{
  BLI_assert(!is_computed_);
  this->compute_globals();
  this->compute_per_particle();
  is_computed_ = true;
}

fn::GVSpan ParticleFunctionEvaluator::get(int output_index, StringRef expected_name) const
{
#ifdef DEBUG
  if (expected_name != "") {
    StringRef real_name = particle_fn_.output_names_[output_index];
    BLI_assert(expected_name == real_name);
  }
  BLI_assert(is_computed_);
#endif
  UNUSED_VARS_NDEBUG(expected_name);
  const void *buffer = outputs_[output_index];
  const fn::CPPType &type = particle_fn_.output_types_[output_index].single_type();
  if (particle_fn_.output_is_global_[output_index]) {
    return fn::GVSpan::FromSingleWithMaxSize(type, buffer);
  }

  return fn::GVSpan(fn::GSpan(type, buffer, mask_.min_array_size()));
}

void ParticleFunctionEvaluator::compute_globals()
{
  if (particle_fn_.global_fn_ == nullptr) {
    return;
  }

  fn::MFParamsBuilder params(*particle_fn_.global_fn_, mask_.min_array_size());

  /* Add input parameters. */
  ParticleFunctionInputContext input_context{solve_context_, particles_};
  for (const ParticleFunctionInput *input : particle_fn_.global_inputs_) {
    input->add_input(input_context, params, resources_);
  }

  /* Add output parameters. */
  for (int output_index : particle_fn_.global_output_indices_) {
    fn::MFDataType data_type = particle_fn_.output_types_[output_index];
    BLI_assert(data_type.is_single()); /* For now. */

    const fn::CPPType &type = data_type.single_type();
    void *buffer = resources_.linear_allocator().allocate(type.size(), type.alignment());
    params.add_uninitialized_single_output(fn::GMutableSpan(type, buffer, 1));
    outputs_[output_index] = buffer;
  }

  particle_fn_.global_fn_->call({0}, params, global_context_);
}

void ParticleFunctionEvaluator::compute_per_particle()
{
  if (particle_fn_.per_particle_fn_ == nullptr) {
    return;
  }

  fn::MFParamsBuilder params(*particle_fn_.per_particle_fn_, mask_.min_array_size());

  /* Add input parameters. */
  ParticleFunctionInputContext input_context{solve_context_, particles_};
  for (const ParticleFunctionInput *input : particle_fn_.per_particle_inputs_) {
    input->add_input(input_context, params, resources_);
  }

  /* Add output parameters. */
  for (int output_index : particle_fn_.per_particle_output_indices_) {
    fn::MFDataType data_type = particle_fn_.output_types_[output_index];
    BLI_assert(data_type.is_single()); /* For now. */

    const fn::CPPType &type = data_type.single_type();
    void *buffer = resources_.linear_allocator().allocate(type.size() * mask_.min_array_size(),
                                                          type.alignment());
    params.add_uninitialized_single_output(fn::GMutableSpan(type, buffer, mask_.min_array_size()));
    outputs_[output_index] = buffer;
  }

  particle_fn_.per_particle_fn_->call(mask_, params, global_context_);
}

}  // namespace blender::sim
