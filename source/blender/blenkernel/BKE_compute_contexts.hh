/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * This file implements some specific compute contexts for concepts in Blender.
 */

#include <optional>

#include "BLI_compute_context.hh"

struct bNode;
struct bNodeTree;

namespace blender::bke {

class ModifierComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "MODIFIER";

  /**
   * Use modifier name instead of something like `session_uid` for now because:
   * - It's more obvious that the name matches between the original and evaluated object.
   * - We might want that the context hash is consistent between sessions in the future.
   */
  std::string modifier_name_;

 public:
  ModifierComputeContext(const ComputeContext *parent, std::string modifier_name);

  StringRefNull modifier_name() const
  {
    return modifier_name_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class GroupNodeComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "NODE_GROUP";

  int32_t node_id_;
  /**
   * The caller node tree and group node are not always necessary or even available, but storing
   * them here simplifies "walking up" the compute context to the parent node groups.
   */
  const bNodeTree *caller_tree_ = nullptr;
  const bNode *caller_group_node_ = nullptr;

 public:
  GroupNodeComputeContext(const ComputeContext *parent,
                          int32_t node_id,
                          const std::optional<ComputeContextHash> &cached_hash = {});
  GroupNodeComputeContext(const ComputeContext *parent,
                          const bNode &node,
                          const bNodeTree &caller_tree);

  int32_t node_id() const
  {
    return node_id_;
  }

  const bNode *caller_group_node() const
  {
    return caller_group_node_;
  }

  const bNodeTree *caller_tree() const
  {
    return caller_tree_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class SimulationZoneComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "SIMULATION_ZONE";

  int32_t output_node_id_;

 public:
  SimulationZoneComputeContext(const ComputeContext *parent, int output_node_id);
  SimulationZoneComputeContext(const ComputeContext *parent, const bNode &node);

  int32_t output_node_id() const
  {
    return output_node_id_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class RepeatZoneComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "REPEAT_ZONE";

  int32_t output_node_id_;
  int iteration_;

 public:
  RepeatZoneComputeContext(const ComputeContext *parent, int32_t output_node_id, int iteration);
  RepeatZoneComputeContext(const ComputeContext *parent, const bNode &node, int iteration);

  int32_t output_node_id() const
  {
    return output_node_id_;
  }

  int iteration() const
  {
    return iteration_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class OperatorComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "OPERATOR";

 public:
  OperatorComputeContext();
  OperatorComputeContext(const ComputeContext *parent);

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

}  // namespace blender::bke
