/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_hash.hh"
#include "BLI_struct_equality_utils.hh"

namespace blender::nodes {

/**
 * Don't use integer for menus directly, so that there is a each static single value type maps to
 * exactly one socket type. Also it avoids accidentally casting the menu value to other types.
 */
struct MenuValue {
  int value = 0;

  MenuValue() = default;
  explicit MenuValue(const int value) : value(value) {}

  template<typename EnumT, BLI_ENABLE_IF((std::is_enum_v<EnumT>))>
  MenuValue(const EnumT value) : value(int(value))
  {
  }

  uint64_t hash() const
  {
    return get_default_hash(this->value);
  }

  BLI_STRUCT_EQUALITY_OPERATORS_1(MenuValue, value)
};

}  // namespace blender::nodes
