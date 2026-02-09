/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_hash.hh"

namespace blender::nodes {

/**
 * Don't use integer for menus directly, so that there is a each static single value type maps to
 * exactly one socket type. Also it avoids accidentally casting the menu value to other types.
 */
struct MenuValue {
  int value = 0;

  MenuValue() = default;
  explicit MenuValue(const int value) : value(value) {}

  template<typename EnumT>
  MenuValue(const EnumT value)
    requires(std::is_enum_v<EnumT>)
      : value(int(value))
  {
  }

  uint64_t hash() const
  {
    return get_default_hash(this->value);
  }

  friend bool operator==(const MenuValue &a, const MenuValue &b) = default;
};

}  // namespace blender::nodes
