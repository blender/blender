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

#include "FN_generic_virtual_vector_array.hh"

namespace blender::fn {

void GVArrayForGVVectorArrayIndex::get_impl(const int64_t index_in_vector, void *r_value) const
{
  vector_array_.get_vector_element(index_, index_in_vector, r_value);
}

void GVArrayForGVVectorArrayIndex::get_to_uninitialized_impl(const int64_t index_in_vector,
                                                             void *r_value) const
{
  type_->construct_default(r_value);
  vector_array_.get_vector_element(index_, index_in_vector, r_value);
}

int64_t GVVectorArrayForSingleGVArray::get_vector_size_impl(const int64_t UNUSED(index)) const
{
  return array_.size();
}

void GVVectorArrayForSingleGVArray::get_vector_element_impl(const int64_t UNUSED(index),
                                                            const int64_t index_in_vector,
                                                            void *r_value) const
{
  array_.get(index_in_vector, r_value);
}

bool GVVectorArrayForSingleGVArray::is_single_vector_impl() const
{
  return true;
}

int64_t GVVectorArrayForSingleGSpan::get_vector_size_impl(const int64_t UNUSED(index)) const
{
  return span_.size();
}

void GVVectorArrayForSingleGSpan::get_vector_element_impl(const int64_t UNUSED(index),
                                                          const int64_t index_in_vector,
                                                          void *r_value) const
{
  type_->copy_to_initialized(span_[index_in_vector], r_value);
}

bool GVVectorArrayForSingleGSpan::is_single_vector_impl() const
{
  return true;
}

}  // namespace blender::fn
