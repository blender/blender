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

#include "FN_attributes_ref.hh"

namespace blender::fn {

AttributesInfoBuilder::~AttributesInfoBuilder()
{
  for (uint i : defaults_.index_range()) {
    types_[i]->destruct(defaults_[i]);
  }
}

void AttributesInfoBuilder::add(StringRef name, const CPPType &type, const void *default_value)
{
  if (names_.add_as(name)) {
    types_.append(&type);

    if (default_value == nullptr) {
      default_value = type.default_value();
    }
    void *dst = allocator_.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(default_value, dst);
    defaults_.append(dst);
  }
  else {
    /* The same name can be added more than once as long as the type is always the same. */
    BLI_assert(types_[names_.index_of_as(name)] == &type);
  }
}

AttributesInfo::AttributesInfo(const AttributesInfoBuilder &builder)
{
  for (uint i : builder.types_.index_range()) {
    StringRefNull name = allocator_.copy_string(builder.names_[i]);
    const CPPType &type = *builder.types_[i];
    const void *default_value = builder.defaults_[i];

    index_by_name_.add_new(name, i);
    name_by_index_.append(name);
    type_by_index_.append(&type);

    void *dst = allocator_.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(default_value, dst);
    defaults_.append(dst);
  }
}

AttributesInfo::~AttributesInfo()
{
  for (uint i : defaults_.index_range()) {
    type_by_index_[i]->destruct(defaults_[i]);
  }
}

}  // namespace blender::fn
