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

#include "FN_multi_function_parallel.hh"

#include "BLI_task.hh"

namespace blender::fn {

ParallelMultiFunction::ParallelMultiFunction(const MultiFunction &fn, const int64_t grain_size)
    : fn_(fn), grain_size_(grain_size)
{
  this->set_signature(&fn.signature());

  threading_supported_ = true;
  for (const int param_index : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(param_index);
    if (param_type.data_type().category() == MFDataType::Vector) {
      /* Vector parameters do not support threading yet. */
      threading_supported_ = false;
      break;
    }
  }
}

void ParallelMultiFunction::call(IndexMask full_mask, MFParams params, MFContext context) const
{
  if (full_mask.size() <= grain_size_ || !threading_supported_) {
    fn_.call(full_mask, params, context);
    return;
  }

  threading::parallel_for(full_mask.index_range(), grain_size_, [&](const IndexRange mask_slice) {
    Vector<int64_t> sub_mask_indices;
    const IndexMask sub_mask = full_mask.slice_and_offset(mask_slice, sub_mask_indices);
    if (sub_mask.is_empty()) {
      return;
    }
    const int64_t input_slice_start = full_mask[mask_slice.first()];
    const int64_t input_slice_size = full_mask[mask_slice.last()] - input_slice_start + 1;
    const IndexRange input_slice_range{input_slice_start, input_slice_size};

    MFParamsBuilder sub_params{fn_, sub_mask.min_array_size()};

    /* All parameters are sliced so that the wrapped multi-function does not have to take care of
     * the index offset. */
    for (const int param_index : fn_.param_indices()) {
      const MFParamType param_type = fn_.param_type(param_index);
      switch (param_type.category()) {
        case MFParamType::SingleInput: {
          const GVArray &varray = params.readonly_single_input(param_index);
          sub_params.add_readonly_single_input(varray.slice(input_slice_range));
          break;
        }
        case MFParamType::SingleMutable: {
          const GMutableSpan span = params.single_mutable(param_index);
          const GMutableSpan sliced_span = span.slice(input_slice_start, input_slice_size);
          sub_params.add_single_mutable(sliced_span);
          break;
        }
        case MFParamType::SingleOutput: {
          const GMutableSpan span = params.uninitialized_single_output(param_index);
          const GMutableSpan sliced_span = span.slice(input_slice_start, input_slice_size);
          sub_params.add_uninitialized_single_output(sliced_span);
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

    fn_.call(sub_mask, sub_params, context);
  });
}

}  // namespace blender::fn
