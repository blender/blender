/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

#include "NOD_geometry_nodes_bundle_fwd.hh"
#include "NOD_socket_interface_key.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

/**
 * A bundle is a map containing keys and their corresponding values. Values are stored as the type
 * they have in Geometry Nodes (#bNodeSocketType::geometry_nodes_cpp_type).
 */
class Bundle : public ImplicitSharingMixin {
 public:
  struct StoredItem {
    SocketInterfaceKey key;
    const bke::bNodeSocketType *type;
    void *value;
  };

 private:
  Vector<StoredItem> items_;
  Vector<void *> buffers_;

 public:
  struct Item {
    const bke::bNodeSocketType *type;
    const void *value;
  };

  Bundle();
  Bundle(const Bundle &other);
  Bundle(Bundle &&other) noexcept;
  Bundle &operator=(const Bundle &other);
  Bundle &operator=(Bundle &&other) noexcept;
  ~Bundle();

  static BundlePtr create()
  {
    return BundlePtr(MEM_new<Bundle>(__func__));
  }

  void add_new(SocketInterfaceKey key, const bke::bNodeSocketType &type, const void *value);
  bool add(const SocketInterfaceKey &key, const bke::bNodeSocketType &type, const void *value);
  bool add(SocketInterfaceKey &&key, const bke::bNodeSocketType &type, const void *value);
  bool remove(const SocketInterfaceKey &key);
  bool contains(const SocketInterfaceKey &key) const;

  std::optional<Item> lookup(const SocketInterfaceKey &key) const;

  Span<StoredItem> items() const
  {
    return items_;
  }

  void delete_self() override;
};

}  // namespace blender::nodes
