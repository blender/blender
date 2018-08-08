/*
 * Copyright 2013, Blender Foundation.
 *
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
 * Contributor:
 *		Lukas Toenne
 */

#ifndef __COM_DEBUG_H__
#define __COM_DEBUG_H__

#include <map>
#include <string>

#include "COM_defines.h"

class Node;
class NodeOperation;
class ExecutionSystem;
class ExecutionGroup;

class DebugInfo {
public:
	typedef enum {
		EG_WAIT,
		EG_RUNNING,
		EG_FINISHED
	} GroupState;

	typedef std::map<const Node *, std::string> NodeNameMap;
	typedef std::map<const NodeOperation *, std::string> OpNameMap;
	typedef std::map<const ExecutionGroup *, GroupState> GroupStateMap;

	static std::string node_name(const Node *node);
	static std::string operation_name(const NodeOperation *op);

	static void convert_started();
	static void execute_started(const ExecutionSystem *system);

	static void node_added(const Node *node);
	static void node_to_operations(const Node *node);
	static void operation_added(const NodeOperation *operation);
	static void operation_read_write_buffer(const NodeOperation *operation);

	static void execution_group_started(const ExecutionGroup *group);
	static void execution_group_finished(const ExecutionGroup *group);

	static void graphviz(const ExecutionSystem *system);

#ifdef COM_DEBUG
protected:
	static int graphviz_operation(const ExecutionSystem *system, const NodeOperation *operation, const ExecutionGroup *group, char *str, int maxlen);
	static int graphviz_legend_color(const char *name, const char *color, char *str, int maxlen);
	static int graphviz_legend_line(const char *name, const char *color, const char *style, char *str, int maxlen);
	static int graphviz_legend_group(const char *name, const char *color, const char *style, char *str, int maxlen);
	static int graphviz_legend(char *str, int maxlen);
	static bool graphviz_system(const ExecutionSystem *system, char *str, int maxlen);

private:
	static int m_file_index;
	static NodeNameMap m_node_names;			/**< map nodes to usable names for debug output */
	static OpNameMap m_op_names;				/**< map operations to usable names for debug output */
	static std::string m_current_node_name;		/**< base name for all operations added by a node */
	static std::string m_current_op_name;		/**< base name for automatic sub-operations */
	static GroupStateMap m_group_states;		/**< for visualizing group states */
#endif
};

#endif
