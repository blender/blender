/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_field_cpp_type.hh"

namespace blender::fn {

static auto &get_from_self_map()
{
  static Map<const CPPType *, const ValueOrFieldCPPType *> map;
  return map;
}

static auto &get_from_value_map()
{
  static Map<const CPPType *, const ValueOrFieldCPPType *> map;
  return map;
}

void ValueOrFieldCPPType::register_self()
{
  get_from_value_map().add_new(&this->value, this);
  get_from_self_map().add_new(&this->self, this);
}

const ValueOrFieldCPPType *ValueOrFieldCPPType::get_from_self(const CPPType &self)
{
  const ValueOrFieldCPPType *type = get_from_self_map().lookup_default(&self, nullptr);
  BLI_assert(type == nullptr || type->self == self);
  return type;
}

const ValueOrFieldCPPType *ValueOrFieldCPPType::get_from_value(const CPPType &value)
{
  const ValueOrFieldCPPType *type = get_from_value_map().lookup_default(&value, nullptr);
  BLI_assert(type == nullptr || type->value == value);
  return type;
}

}  // namespace blender::fn
