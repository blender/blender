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

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_cpp_type_make.hh"
#include "FN_field.hh"

namespace blender::fn {

template<typename T> struct FieldCPPTypeParam {
};

class FieldCPPType : public CPPType {
 private:
  const CPPType &base_type_;

 public:
  template<typename T>
  FieldCPPType(FieldCPPTypeParam<Field<T>> /* unused */, StringRef debug_name)
      : CPPType(CPPTypeParam<Field<T>, CPPTypeFlags::None>(), debug_name),
        base_type_(CPPType::get<T>())
  {
  }

  const CPPType &base_type() const
  {
    return base_type_;
  }

  /* Ensure that #GField and #Field<T> have the same layout, to enable casting between the two. */
  static_assert(sizeof(Field<int>) == sizeof(GField));
  static_assert(sizeof(Field<int>) == sizeof(Field<std::string>));

  const GField &get_gfield(const void *field) const
  {
    return *(const GField *)field;
  }

  void construct_from_gfield(void *r_value, const GField &gfield) const
  {
    new (r_value) GField(gfield);
  }
};

}  // namespace blender::fn

#define MAKE_FIELD_CPP_TYPE(DEBUG_NAME, FIELD_TYPE) \
  template<> \
  const blender::fn::CPPType &blender::fn::CPPType::get_impl<blender::fn::Field<FIELD_TYPE>>() \
  { \
    static blender::fn::FieldCPPType cpp_type{ \
        blender::fn::FieldCPPTypeParam<blender::fn::Field<FIELD_TYPE>>(), STRINGIFY(DEBUG_NAME)}; \
    return cpp_type; \
  }
