/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

#include "NOD_socket_interface_key.hh"

namespace blender::nodes {

struct BundleSignature {
  struct Item {
    SocketInterfaceKey key;
    const bke::bNodeSocketType *type = nullptr;
  };

  Vector<Item> items;

  bool matches_exactly(const BundleSignature &other) const;
};

}  // namespace blender::nodes
