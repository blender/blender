/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * This file implements some specific compute contexts for concepts in Blender.
 */

#include "BLI_compute_context.hh"

namespace blender::bke {

class ModifierComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "MODIFIER";

  /**
   * Use modifier name instead of something like `session_uuid` for now because:
   * - It's more obvious that the name matches between the original and evaluated object.
   * - We might want that the context hash is consistent between sessions in the future.
   */
  std::string modifier_name_;

 public:
  ModifierComputeContext(const ComputeContext *parent, std::string modifier_name);

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class NodeGroupComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "NODE_GROUP";

  std::string node_name_;

 public:
  NodeGroupComputeContext(const ComputeContext *parent, std::string node_name);

  StringRefNull node_name() const;

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

}  // namespace blender::bke
