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

#ifndef _COM_Debug_h
#define _COM_Debug_h

#include <map>
#include <string>

#include "COM_defines.h"

class NodeBase;
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
	
	typedef std::map<NodeBase *, std::string> NodeNameMap;
	typedef std::map<ExecutionGroup *, GroupState> GroupStateMap;
	
	static std::string node_name(NodeBase *node);
	
	static void convert_started();
	static void execute_started(ExecutionSystem *system);
	
	static void node_added(Node *node);
	static void node_to_operations(Node *node);
	static void operation_added(NodeOperation *operation);
	static void operation_read_write_buffer(NodeOperation *operation);
	
	static void execution_group_started(ExecutionGroup *group);
	static void execution_group_finished(ExecutionGroup *group);
	
	static void graphviz(ExecutionSystem *system);
	
#ifdef COM_DEBUG
protected:
	static int graphviz_operation(ExecutionSystem *system, NodeOperation *operation, ExecutionGroup *group, char *str, int maxlen);
	static int graphviz_legend_color(const char *name, const char *color, char *str, int maxlen);
	static int graphviz_legend_line(const char *name, const char *color, const char *style, char *str, int maxlen);
	static int graphviz_legend_group(const char *name, const char *color, const char *style, char *str, int maxlen);
	static int graphviz_legend(char *str, int maxlen);
	static bool graphviz_system(ExecutionSystem *system, char *str, int maxlen);
	
private:
	static int m_file_index;
	static NodeNameMap m_node_names;			/**< map nodes to usable names for debug output */
	static std::string m_current_node_name;		/**< base name for all operations added by a node */
	static GroupStateMap m_group_states;		/**< for visualizing group states */
#endif
};

#endif
