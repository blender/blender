/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2013, Blender Foundation.
 */

#pragma once

#include <map>
#include <string>

#include "COM_ExecutionSystem.h"
#include "COM_NodeOperation.h"
#include "COM_defines.h"

namespace blender::compositor {

static constexpr bool COM_EXPORT_GRAPHVIZ = false;
class Node;
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
  static int m_file_index;
  /** Map nodes to usable names for debug output. */
  static NodeNameMap m_node_names;
  /** Map operations to usable names for debug output. */
  static OpNameMap m_op_names;
  /** Base name for all operations added by a node. */
  static std::string m_current_node_name;
  /** Base name for automatic sub-operations. */
  static std::string m_current_op_name;
  /** For visualizing group states. */
  static GroupStateMap m_group_states;

 public:
  static void convert_started()
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_op_names.clear();
    }
  }

  static void execute_started(const ExecutionSystem *system)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_file_index = 1;
      m_group_states.clear();
      for (ExecutionGroup *execution_group : system->m_groups) {
        m_group_states[execution_group] = EG_WAIT;
      }
    }
  };

  static void node_added(const Node *node)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_node_names[node] = std::string(node->getbNode() ? node->getbNode()->name : "");
    }
  }

  static void node_to_operations(const Node *node)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_current_node_name = m_node_names[node];
    }
  }

  static void operation_added(const NodeOperation *operation)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_op_names[operation] = m_current_node_name;
    }
  };

  static void operation_read_write_buffer(const NodeOperation *operation)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_current_op_name = m_op_names[operation];
    }
  };

  static void execution_group_started(const ExecutionGroup *group)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_group_states[group] = EG_RUNNING;
    }
  };
  static void execution_group_finished(const ExecutionGroup *group)
  {
    if (COM_EXPORT_GRAPHVIZ) {
      m_group_states[group] = EG_FINISHED;
    }
  };

  static void graphviz(const ExecutionSystem *system);

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
  static int graphviz_legend(char *str, int maxlen);
  static bool graphviz_system(const ExecutionSystem *system, char *str, int maxlen);
};

}  // namespace blender::compositor
