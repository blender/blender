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

#include "FN_multi_function.hh"

#include "BLI_task.hh"
#include "BLI_threads.h"

namespace blender::fn {

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
    const MFParamType param_type = fn.param_type(i);
    if (ELEM(param_type.interface_type(),
             MFParamType::InterfaceType::Mutable,
             MFParamType::InterfaceType::Output)) {
      if (param_type.data_type().is_vector()) {
        return false;
      }
    }
  }
  return true;
}

static int64_t compute_grain_size(const ExecutionHints &hints, const IndexMask mask)
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

void MultiFunction::call_auto(IndexMask mask, MFParams params, MFContext context) const
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

  threading::parallel_for(mask.index_range(), grain_size, [&](const IndexRange sub_range) {
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

    Vector<int64_t> offset_mask_indices;
    const IndexMask offset_mask = mask.slice_and_offset(sub_range, offset_mask_indices);

    MFParamsBuilder offset_params{*this, offset_mask.min_array_size()};

    /* Slice all parameters so that for the actual function call. */
    for (const int param_index : this->param_indices()) {
      const MFParamType param_type = this->param_type(param_index);
      switch (param_type.category()) {
        case MFParamType::SingleInput: {
          const GVArray &varray = params.readonly_single_input(param_index);
          offset_params.add_readonly_single_input(varray.slice(input_slice_range));
          break;
        }
        case MFParamType::SingleMutable: {
          const GMutableSpan span = params.single_mutable(param_index);
          const GMutableSpan sliced_span = span.slice(input_slice_range);
          offset_params.add_single_mutable(sliced_span);
          break;
        }
        case MFParamType::SingleOutput: {
          const GMutableSpan span = params.uninitialized_single_output_if_required(param_index);
          if (span.is_empty()) {
            offset_params.add_ignored_single_output();
          }
          else {
            const GMutableSpan sliced_span = span.slice(input_slice_range);
            offset_params.add_uninitialized_single_output(sliced_span);
          }
          break;
        }
        case MFParamType::VectorInput:
        case MFParamType::VectorMutable:
        case MFParamType::VectorOutput: {
          BLI_assert_unreachable();
          break;
        }
      }
    }

    this->call(offset_mask, offset_params, context);
  });
}

std::string MultiFunction::debug_name() const
{
  return signature_ref_->function_name;
}

}  // namespace blender::fn
