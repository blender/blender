/* SPDX-FileCopyrightText: 2013 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <map>
#include <string>

#include "BLI_vector.hh"

#include "COM_ExecutionSystem.h"
#include "COM_MemoryBuffer.h"
#include "COM_Node.h"

namespace blender::compositor {

static constexpr bool COM_EXPORT_GRAPHVIZ = false;
static constexpr bool COM_GRAPHVIZ_SHOW_NODE_NAME = false;

/* Saves operations results to image files. */
static constexpr bool COM_EXPORT_OPERATION_BUFFERS = false;

class Node;
class NodeOperation;
class ExecutionSystem;
class ExecutionGroup;

class DebugInfo {
 public:
  typedef enum { EG_WAIT, EG_RUNNING, EG_FINISHED } GroupState;

  typedef std::map<const Node *, std::string> NodeNameMap;
  typedef std::map<const NodeOperation *, std::string> OpNameMap;
  typedef std::map<const ExecutionGroup *, GroupState> GroupStateMap;

  static std::string node_name(const Node *node);
  static std::string operation_name(const NodeOperation *op);

 private:
  static int file_index_;
  /** Map nodes to usable names for debug output. */
  static NodeNameMap node_names_;
  /** Map operations to usable names for debug output. */
  static OpNameMap op_names_;
  /** Base name for all operations added by a node. */
  static std::string current_node_name_;
  /** Base name for automatic sub-operations. */
  static std::string current_op_name_;
  /** For visualizing group states. */
  static GroupStateMap group_states_;

 public:
  static void convert_started()
  {
    if (COM_EXPORT_GRAPHVIZ) {
      op_names_.clear();
    }
  }

  static void execute_started(const ExecutionSystem *system)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      file_index_ = 1;
      group_states_.clear();
      for (ExecutionGroup *execution_group : system->groups_) {
        group_states_[execution_group] = EG_WAIT;
      }
    }
    if (COM_EXPORT_OPERATION_BUFFERS) {
      delete_operation_exports();
    }
  };

  static void node_added(const Node *node)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      node_names_[node] = std::string(node->get_bnode() ? node->get_bnode()->name : "");
    }
  }

  static void node_to_operations(const Node *node)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      current_node_name_ = node_names_[node];
    }
  }

  static void operation_added(const NodeOperation *operation)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      op_names_[operation] = current_node_name_;
    }
  };

  static void operation_read_write_buffer(const NodeOperation *operation)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      current_op_name_ = op_names_[operation];
    }
  };

  static void execution_group_started(const ExecutionGroup *group)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      group_states_[group] = EG_RUNNING;
    }
  };
  static void execution_group_finished(const ExecutionGroup *group)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      group_states_[group] = EG_FINISHED;
    }
  };

  static void operation_rendered(const NodeOperation *op, MemoryBuffer *render)
  {
    /* Don't export constant operations as there are too many and it's rarely useful. */
    if (COM_EXPORT_OPERATION_BUFFERS && render && !render->is_a_single_elem()) {
      export_operation(op, render);
    }
  }

  static void graphviz(const ExecutionSystem *system, StringRefNull name = "");

 protected:
  static int graphviz_operation(const ExecutionSystem *system,
                                NodeOperation *operation,
                                const ExecutionGroup *group,
                                char *str,
                                int maxlen);
  static int graphviz_legend_color(const char *name, const char *color, char *str, int maxlen);
  static int graphviz_legend_line(
      const char *name, const char *color, const char *style, char *str, int maxlen);
  static int graphviz_legend_group(
      const char *name, const char *color, const char *style, char *str, int maxlen);
  static int graphviz_legend(char *str, int maxlen, bool has_execution_groups);
  static bool graphviz_system(const ExecutionSystem *system, char *str, int maxlen);

  static void export_operation(const NodeOperation *op, MemoryBuffer *render);
  static void delete_operation_exports();
};

}  // namespace blender::compositor
