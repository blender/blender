/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fn
 */

#include "FN_lazy_function.hh"

namespace blender::fn::lazy_function {

std::string LazyFunction::name() const
{
  return debug_name_;
}

std::string LazyFunction::input_name(int index) const
{
  return inputs_[index].debug_name;
}

std::string LazyFunction::output_name(int index) const
{
  return outputs_[index].debug_name;
}

void *LazyFunction::init_storage(LinearAllocator<> & /*allocator*/) const
{
  return nullptr;
}

void LazyFunction::destruct_storage(void *storage) const
{
  BLI_assert(storage == nullptr);
  UNUSED_VARS_NDEBUG(storage);
}

void LazyFunction::possible_output_dependencies(const int /*output_index*/,
                                                const FunctionRef<void(Span<int>)> fn) const
{
  /* The output depends on all inputs by default. */
  Vector<int, 16> indices(inputs_.size());
  for (const int i : inputs_.index_range()) {
    indices[i] = i;
  }
  fn(indices);
}

bool LazyFunction::always_used_inputs_available(const Params &params) const
{
  if (allow_missing_requested_inputs_) {
    return true;
  }
  for (const int i : inputs_.index_range()) {
    const Input &fn_input = inputs_[i];
    if (fn_input.usage == ValueUsage::Used) {
      if (params.try_get_input_data_ptr(i) == nullptr) {
        return false;
      }
    }
  }
  return true;
}

bool Params::try_enable_multi_threading_impl()
{
  return false;
}

destruct_ptr<LocalUserData> UserData::get_local(LinearAllocator<> & /*allocator*/)
{
  return {};
}

}  // namespace blender::fn::lazy_function
