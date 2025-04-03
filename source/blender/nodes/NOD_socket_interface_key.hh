/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_vector.hh"

namespace blender::nodes {

/**
 * A key that identifies values in a bundle or inputs/outputs of a closure.
 * Note that this key does not have a hash and thus can't be used in a hash table. This wouldn't
 * work well if these items have multiple identifiers for compatibility reasons. While that's not
 * used currently, it's good to keep it possible.
 */
class SocketInterfaceKey {
 private:
  /** May have multiple keys to improve compatibility between systems that use different keys. */
  Vector<std::string> identifiers_;

 public:
  explicit SocketInterfaceKey(Vector<std::string> identifiers);
  explicit SocketInterfaceKey(std::string identifier);

  bool matches(const SocketInterfaceKey &other) const;
  Span<std::string> identifiers() const;
};

}  // namespace blender::nodes
