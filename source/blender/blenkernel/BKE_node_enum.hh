/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_implicit_sharing.hh"
#include "BLI_vector.hh"

namespace blender::bke {

/* Flags for #bNodeSocketValueMenu. */
enum NodeSocketValueMenuRuntimeFlag {
  /* Socket has conflicting menu connections and cannot resolve items. */
  NODE_MENU_ITEMS_CONFLICT = (1 << 0),
};

/* -------------------------------------------------------------------- */
/** \name Runtime enum items list.
 * \{ */

/**
 * Runtime copy of #NodeEnumItem for use in #RuntimeNodeEnumItems.
 */
struct RuntimeNodeEnumItem {
  std::string name;
  std::string description;
  /* Immutable unique identifier. */
  int identifier;
};

/**
 * Shared immutable list of enum items.
 * These are owned by a node and can be referenced by node sockets.
 */
struct RuntimeNodeEnumItems : ImplicitSharingMixin {
  Vector<RuntimeNodeEnumItem> items;

  const RuntimeNodeEnumItem *find_item_by_identifier(int identifier) const;

  void delete_self() override
  {
    delete this;
  }
};

/** \} */

}  // namespace blender::bke
