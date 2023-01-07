/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_params.hh"

namespace blender::fn::multi_function {

GMutableSpan MFParams::ensure_dummy_single_output(int param_index)
{
  /* Lock because we are actually modifying #builder_ and it may be used by multiple threads. */
  std::lock_guard lock{builder_->mutex_};

  for (const std::pair<int, GMutableSpan> &items : builder_->dummy_output_spans_) {
    if (items.first == param_index) {
      return items.second;
    }
  }

  const CPPType &type = std::get_if<GMutableSpan>(&builder_->actual_params_[param_index])->type();
  void *buffer = builder_->scope_.linear_allocator().allocate(
      builder_->min_array_size_ * type.size(), type.alignment());
  if (!type.is_trivially_destructible()) {
    builder_->scope_.add_destruct_call(
        [&type, buffer, mask = builder_->mask_]() { type.destruct_indices(buffer, mask); });
  }
  const GMutableSpan span{type, buffer, builder_->min_array_size_};
  builder_->dummy_output_spans_.append({param_index, span});
  return span;
}

}  // namespace blender::fn::multi_function
