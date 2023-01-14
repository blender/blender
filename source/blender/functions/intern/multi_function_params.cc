/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_params.hh"

namespace blender::fn::multi_function {

void ParamsBuilder::add_unused_output_for_unsupporting_function(const CPPType &type)
{
  ResourceScope &scope = this->resource_scope();
  void *buffer = scope.linear_allocator().allocate(type.size() * min_array_size_,
                                                   type.alignment());
  const GMutableSpan span{type, buffer, min_array_size_};
  actual_params_.append_unchecked_as(std::in_place_type<GMutableSpan>, span);
  if (!type.is_trivially_destructible()) {
    scope.add_destruct_call(
        [&type, buffer, mask = mask_]() { type.destruct_indices(buffer, mask); });
  }
}

}  // namespace blender::fn::multi_function
