/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_hash.hh"
#include "BLI_string_ref.hh"

namespace blender {

struct Report;

namespace nodes {

/** These values are also written to .blend files, so don't change them lightly. */
enum class NodeWarningType {
  Error = 0,
  Warning = 1,
  Info = 2,
};

struct NodeWarning {
  NodeWarningType type = NodeWarningType::Error;
  std::string message;

  NodeWarning() = default;
  NodeWarning(NodeWarningType type, StringRef message) : type(type), message(message) {}
  explicit NodeWarning(const Report &report);

  uint64_t hash() const
  {
    return get_default_hash(this->type, this->message);
  }

  friend bool operator==(const NodeWarning &a, const NodeWarning &b) = default;
};

int node_warning_type_icon(NodeWarningType type);
int node_warning_type_severity(NodeWarningType type);
StringRefNull node_warning_type_name(NodeWarningType type);

}  // namespace nodes
}  // namespace blender
