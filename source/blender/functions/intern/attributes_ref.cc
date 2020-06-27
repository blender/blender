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

namespace blender {
namespace fn {

AttributesInfoBuilder::~AttributesInfoBuilder()
{
  for (uint i : m_defaults.index_range()) {
    m_types[i]->destruct(m_defaults[i]);
  }
}

void AttributesInfoBuilder::add(StringRef name, const CPPType &type, const void *default_value)
{
  if (m_names.add_as(name)) {
    m_types.append(&type);

    if (default_value == nullptr) {
      default_value = type.default_value();
    }
    void *dst = m_allocator.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(default_value, dst);
    m_defaults.append(dst);
  }
  else {
    /* The same name can be added more than once as long as the type is always the same. */
    BLI_assert(m_types[m_names.index_of_as(name)] == &type);
  }
}

AttributesInfo::AttributesInfo(const AttributesInfoBuilder &builder)
{
  for (uint i : builder.m_types.index_range()) {
    StringRefNull name = m_allocator.copy_string(builder.m_names[i]);
    const CPPType &type = *builder.m_types[i];
    const void *default_value = builder.m_defaults[i];

    m_index_by_name.add_new(name, i);
    m_name_by_index.append(name);
    m_type_by_index.append(&type);

    void *dst = m_allocator.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(default_value, dst);
    m_defaults.append(dst);
  }
}

AttributesInfo::~AttributesInfo()
{
  for (uint i : m_defaults.index_range()) {
    m_type_by_index[i]->destruct(m_defaults[i]);
  }
}

}  // namespace fn
}  // namespace blender
