/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

/**
 * This file implements some specific compute contexts for concepts in Blender.
 *
 * All compute contexts have to store the data that's required to uniquely identify them and to
 * compute its hash. Some compute contexts contain some optional additional data that provides more
 * information to code that uses the contexts.
 */

#include <optional>

#include "BLI_compute_context.hh"

#include "NOD_geometry_nodes_closure_location.hh"

struct bNode;
struct bNodeTree;
struct NodesModifierData;

namespace blender::nodes {
class Closure;
}

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
  /** The modifier data that this context is for. This may be null. */
  const NodesModifierData *nmd_ = nullptr;

 public:
  ModifierComputeContext(const ComputeContext *parent, const NodesModifierData &nmd);
  ModifierComputeContext(const ComputeContext *parent, std::string modifier_name);

  StringRefNull modifier_name() const
  {
    return modifier_name_;
  }

  const NodesModifierData *nmd() const
  {
    return nmd_;
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
                          const bNode &caller_group_node,
                          const bNodeTree &caller_tree,
                          const std::optional<ComputeContextHash> &cached_hash = {});

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

class ForeachGeometryElementZoneComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "FOREACH_GEOMETRY_ELEMENT_ZONE";

  int32_t output_node_id_;
  int index_;

 public:
  ForeachGeometryElementZoneComputeContext(const ComputeContext *parent,
                                           int32_t output_node_id,
                                           int index);
  ForeachGeometryElementZoneComputeContext(const ComputeContext *parent,
                                           const bNode &node,
                                           int index);

  int32_t output_node_id() const
  {
    return output_node_id_;
  }

  int index() const
  {
    return index_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class EvaluateClosureComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "CLOSURE";

  int32_t node_id_;

  /**
   * Extra information that might not always be available.
   */
  const bNode *evaluate_node_ = nullptr;
  std::optional<nodes::ClosureSourceLocation> closure_source_location_;

 public:
  EvaluateClosureComputeContext(const ComputeContext *parent, int32_t node_id);
  EvaluateClosureComputeContext(
      const ComputeContext *parent,
      const bNode &evaluate_node,
      const std::optional<nodes::ClosureSourceLocation> &closure_source_location);

  int32_t node_id() const
  {
    return node_id_;
  }
  const bNode *evaluate_node() const
  {
    return evaluate_node_;
  }

  std::optional<nodes::ClosureSourceLocation> closure_source_location() const
  {
    return closure_source_location_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class OperatorComputeContext : public ComputeContext {
 private:
  static constexpr const char *s_static_type = "OPERATOR";

  /** The tree that is executed. May be null. */
  const bNodeTree *tree_ = nullptr;

 public:
  OperatorComputeContext();
  OperatorComputeContext(const ComputeContext *parent);
  OperatorComputeContext(const ComputeContext *parent, const bNodeTree &tree);

  const bNodeTree *tree() const
  {
    return tree_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

}  // namespace blender::bke
