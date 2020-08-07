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
  for (int i : defaults_.index_range()) {
    types_[i]->destruct(defaults_[i]);
  }
}

bool AttributesInfoBuilder::add(StringRef name, const CPPType &type, const void *default_value)
{
  if (name.size() == 0) {
    std::cout << "Warning: Tried to add an attribute with empty name.\n";
    return false;
  }
  if (names_.add_as(name)) {
    types_.append(&type);

    if (default_value == nullptr) {
      default_value = type.default_value();
    }
    void *dst = allocator_.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(default_value, dst);
    defaults_.append(dst);
    return true;
  }

  const CPPType &stored_type = *types_[names_.index_of_as(name)];
  if (stored_type != type) {
    std::cout << "Warning: Tried to add an attribute twice with different types (" << name << ": "
              << stored_type.name() << ", " << type.name() << ").\n";
  }
  return false;
}

AttributesInfo::AttributesInfo(const AttributesInfoBuilder &builder)
{
  for (int i : builder.types_.index_range()) {
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
  for (int i : defaults_.index_range()) {
    type_by_index_[i]->destruct(defaults_[i]);
  }
}

}  // namespace blender::fn
