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

#include "FN_multi_function_params.hh"

namespace blender::fn {

GMutableSpan MFParams::ensure_dummy_single_output(int data_index)
{
  /* Lock because we are actually modifying #builder_ and it may be used by multiple threads. */
  std::lock_guard lock{builder_->mutex_};

  for (const std::pair<int, GMutableSpan> &items : builder_->dummy_output_spans_) {
    if (items.first == data_index) {
      return items.second;
    }
  }

  const CPPType &type = builder_->mutable_spans_[data_index].type();
  void *buffer = builder_->scope_.linear_allocator().allocate(
      builder_->min_array_size_ * type.size(), type.alignment());
  if (!type.is_trivially_destructible()) {
    builder_->scope_.add_destruct_call(
        [&type, buffer, mask = builder_->mask_]() { type.destruct_indices(buffer, mask); });
  }
  const GMutableSpan span{type, buffer, builder_->min_array_size_};
  builder_->dummy_output_spans_.append({data_index, span});
  return span;
}

}  // namespace blender::fn
