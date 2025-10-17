/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function.hh"

#include "BLI_task.hh"
#include "BLI_threads.h"

namespace blender::fn::multi_function {

using ExecutionHints = MultiFunction::ExecutionHints;

ExecutionHints MultiFunction::execution_hints() const
{
  return this->get_execution_hints();
}

ExecutionHints MultiFunction::get_execution_hints() const
{
  return ExecutionHints{};
}

static bool supports_threading_by_slicing_params(const MultiFunction &fn)
{
  for (const int i : fn.param_indices()) {
    const ParamType param_type = fn.param_type(i);
    if (ELEM(param_type.interface_type(),
             ParamType::InterfaceType::Mutable,
             ParamType::InterfaceType::Output))
    {
      if (param_type.data_type().is_vector()) {
        return false;
      }
    }
  }
  return true;
}

static int64_t compute_grain_size(const ExecutionHints &hints, const IndexMask &mask)
{
  int64_t grain_size = hints.min_grain_size;
  if (hints.uniform_execution_time) {
    const int thread_count = BLI_system_thread_count();
    /* Avoid using a small grain size even if it is not necessary. */
    const int64_t thread_based_grain_size = mask.size() / thread_count / 4;
    grain_size = std::max(grain_size, thread_based_grain_size);
  }
  if (hints.allocates_array) {
    const int64_t max_grain_size = 10000;
    /* Avoid allocating many large intermediate arrays. Better process data in smaller chunks to
     * keep peak memory usage lower. */
    grain_size = std::min(grain_size, max_grain_size);
  }
  return grain_size;
}

static int64_t compute_alignment(const int64_t grain_size)
{
  if (grain_size <= 512) {
    /* Don't use a number that's too large, or otherwise the work will be split quite unevenly. */
    return 8;
  }
  /* It's not common that more elements are processed in a loop at once. */
  return 32;
}

static void add_sliced_parameters(const Signature &signature,
                                  Params &full_params,
                                  const IndexRange slice_range,
                                  ParamsBuilder &r_sliced_params)
{
  for (const int param_index : signature.params.index_range()) {
    const ParamType &param_type = signature.params[param_index].type;
    switch (param_type.category()) {
      case ParamCategory::SingleInput: {
        const GVArray &varray = full_params.readonly_single_input(param_index);
        r_sliced_params.add_readonly_single_input(varray.slice(slice_range));
        break;
      }
      case ParamCategory::SingleMutable: {
        const GMutableSpan span = full_params.single_mutable(param_index);
        const GMutableSpan sliced_span = span.slice(slice_range);
        r_sliced_params.add_single_mutable(sliced_span);
        break;
      }
      case ParamCategory::SingleOutput: {
        if (flag_is_set(signature.params[param_index].flag, ParamFlag::SupportsUnusedOutput)) {
          const GMutableSpan span = full_params.uninitialized_single_output_if_required(
              param_index);
          if (span.is_empty()) {
            r_sliced_params.add_ignored_single_output();
          }
          else {
            const GMutableSpan sliced_span = span.slice(slice_range);
            r_sliced_params.add_uninitialized_single_output(sliced_span);
          }
        }
        else {
          const GMutableSpan span = full_params.uninitialized_single_output(param_index);
          const GMutableSpan sliced_span = span.slice(slice_range);
          r_sliced_params.add_uninitialized_single_output(sliced_span);
        }
        break;
      }
      case ParamCategory::VectorInput:
      case ParamCategory::VectorMutable:
      case ParamCategory::VectorOutput: {
        BLI_assert_unreachable();
        break;
      }
    }
  }
}

void MultiFunction::call_auto(const IndexMask &mask, Params params, Context context) const
{
  if (mask.is_empty()) {
    return;
  }
  const ExecutionHints hints = this->execution_hints();
  const int64_t grain_size = compute_grain_size(hints, mask);

  if (mask.size() <= grain_size) {
    this->call(mask, params, context);
    return;
  }

  const bool supports_threading = supports_threading_by_slicing_params(*this);
  if (!supports_threading) {
    this->call(mask, params, context);
    return;
  }

  const int64_t alignment = compute_alignment(grain_size);
  threading::parallel_for_aligned(
      mask.index_range(), grain_size, alignment, [&](const IndexRange sub_range) {
        const IndexMask sliced_mask = mask.slice(sub_range);
        if (!hints.allocates_array) {
          /* There is no benefit to changing indices in this case. */
          this->call(sliced_mask, params, context);
          return;
        }
        if (sliced_mask[0] < grain_size) {
          /* The indices are low, no need to offset them. */
          this->call(sliced_mask, params, context);
          return;
        }
        const int64_t input_slice_start = sliced_mask[0];
        const int64_t input_slice_size = sliced_mask.last() - input_slice_start + 1;
        const IndexRange input_slice_range{input_slice_start, input_slice_size};

        IndexMaskMemory memory;
        const int64_t offset = -input_slice_start;
        const IndexMask shifted_mask = mask.slice_and_shift(sub_range, offset, memory);

        ParamsBuilder sliced_params{*this, &shifted_mask};
        add_sliced_parameters(*signature_ref_, params, input_slice_range, sliced_params);
        this->call(shifted_mask, sliced_params, context);
      });
}

std::string MultiFunction::debug_name() const
{
  return signature_ref_->function_name;
}

}  // namespace blender::fn::multi_function
