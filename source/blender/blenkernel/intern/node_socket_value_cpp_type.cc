/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_node_socket_value_cpp_type.hh"

namespace blender::bke {

static auto &get_from_self_map()
{
  static Map<const CPPType *, const SocketValueVariantCPPType *> map;
  return map;
}

static auto &get_from_value_map()
{
  static Map<const CPPType *, const SocketValueVariantCPPType *> map;
  return map;
}

void SocketValueVariantCPPType::register_self()
{
  get_from_value_map().add_new(&this->value, this);
  get_from_self_map().add_new(&this->self, this);
}

const SocketValueVariantCPPType *SocketValueVariantCPPType::get_from_self(const CPPType &self)
{
  const SocketValueVariantCPPType *type = get_from_self_map().lookup_default(&self, nullptr);
  BLI_assert(type == nullptr || type->self == self);
  return type;
}

const SocketValueVariantCPPType *SocketValueVariantCPPType::get_from_value(const CPPType &value)
{
  const SocketValueVariantCPPType *type = get_from_value_map().lookup_default(&value, nullptr);
  BLI_assert(type == nullptr || type->value == value);
  return type;
}

}  // namespace blender::bke
