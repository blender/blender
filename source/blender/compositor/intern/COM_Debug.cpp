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

#include "COM_Debug.h"

#ifdef COM_DEBUG

#include <typeinfo>
#include <map>
#include <vector>

extern "C" {
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "DNA_node_types.h"
#include "BKE_node.h"
}

#include "COM_Node.h"
#include "COM_ExecutionSystem.h"
#include "COM_ExecutionGroup.h"

#include "COM_ReadBufferOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_WriteBufferOperation.h"


int DebugInfo::m_file_index = 0;
DebugInfo::NodeNameMap DebugInfo::m_node_names;
DebugInfo::OpNameMap DebugInfo::m_op_names;
std::string DebugInfo::m_current_node_name;
std::string DebugInfo::m_current_op_name;
DebugInfo::GroupStateMap DebugInfo::m_group_states;

std::string DebugInfo::node_name(const Node *node)
{
	NodeNameMap::const_iterator it = m_node_names.find(node);
	if (it != m_node_names.end())
		return it->second;
	else
		return "";
}

std::string DebugInfo::operation_name(const NodeOperation *op)
{
	OpNameMap::const_iterator it = m_op_names.find(op);
	if (it != m_op_names.end())
		return it->second;
	else
		return "";
}

void DebugInfo::convert_started()
{
	m_op_names.clear();
}

void DebugInfo::execute_started(const ExecutionSystem *system)
{
	m_file_index = 1;
	m_group_states.clear();
	for (ExecutionSystem::Groups::const_iterator it = system->m_groups.begin(); it != system->m_groups.end(); ++it)
		m_group_states[*it] = EG_WAIT;
}

void DebugInfo::node_added(const Node *node)
{
	m_node_names[node] = std::string(node->getbNode() ? node->getbNode()->name : "");
}

void DebugInfo::node_to_operations(const Node *node)
{
	m_current_node_name = m_node_names[node];
}

void DebugInfo::operation_added(const NodeOperation *operation)
{
	m_op_names[operation] = m_current_node_name;
}

void DebugInfo::operation_read_write_buffer(const NodeOperation *operation)
{
	m_current_op_name = m_op_names[operation];
}

void DebugInfo::execution_group_started(const ExecutionGroup *group)
{
	m_group_states[group] = EG_RUNNING;
}

void DebugInfo::execution_group_finished(const ExecutionGroup *group)
{
	m_group_states[group] = EG_FINISHED;
}

int DebugInfo::graphviz_operation(const ExecutionSystem *system, const NodeOperation *operation, const ExecutionGroup *group, char *str, int maxlen)
{
	int len = 0;
	
	std::string fillcolor = "gainsboro";
	if (operation->isViewerOperation()) {
		const ViewerOperation *viewer = (const ViewerOperation *)operation;
		if (viewer->isActiveViewerOutput()) {
			fillcolor = "lightskyblue1";
		}
		else {
			fillcolor = "lightskyblue3";
		}
	}
	else if (operation->isOutputOperation(system->getContext().isRendering())) {
		fillcolor = "dodgerblue1";
	}
	else if (operation->isSetOperation()) {
		fillcolor = "khaki1";
	}
	else if (operation->isReadBufferOperation()) {
		fillcolor = "darkolivegreen3";
	}
	else if (operation->isWriteBufferOperation()) {
		fillcolor = "darkorange";
	}
	
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "// OPERATION: %p\r\n", operation);
	if (group)
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\"O_%p_%p\"", operation, group);
	else
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\"O_%p\"", operation);
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, " [fillcolor=%s,style=filled,shape=record,label=\"{", fillcolor.c_str());
	
	int totinputs = operation->getNumberOfInputSockets();
	if (totinputs != 0) {
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "{");
		for (int k = 0; k < totinputs; k++) {
			NodeOperationInput *socket = operation->getInputSocket(k);
			if (k != 0) {
				len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
			}
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<IN_%p>", socket);
			switch (socket->getDataType()) {
				case COM_DT_VALUE:
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Value");
					break;
				case COM_DT_VECTOR:
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Vector");
					break;
				case COM_DT_COLOR:
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Color");
					break;
			}
		}
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}");
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
	}
	
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "%s\\n(%s)", m_op_names[operation].c_str(), typeid(*operation).name());
	
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, " (%d,%d)", operation->getWidth(), operation->getHeight());
	
	int totoutputs = operation->getNumberOfOutputSockets();
	if (totoutputs != 0) {
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "{");
		for (int k = 0; k < totoutputs; k++) {
			NodeOperationOutput *socket = operation->getOutputSocket(k);
			if (k != 0) {
				len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
			}
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<OUT_%p>", socket);
			switch (socket->getDataType()) {
				case COM_DT_VALUE:
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Value");
					break;
				case COM_DT_VECTOR:
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Vector");
					break;
				case COM_DT_COLOR:
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Color");
					break;
			}
		}
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}");
	}
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\"]");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\r\n");
	
	return len;
}

int DebugInfo::graphviz_legend_color(const char *name, const char *color, char *str, int maxlen)
{
	int len = 0;
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<TR><TD>%s</TD><TD BGCOLOR=\"%s\"></TD></TR>\r\n", name, color);
	return len;
}

int DebugInfo::graphviz_legend_line(const char *name, const char *color, const char *style, char *str, int maxlen)
{
	/* XXX TODO */
	int len = 0;
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\r\n");
	return len;
}

int DebugInfo::graphviz_legend_group(const char *name, const char *color, const char *style, char *str, int maxlen)
{
	int len = 0;
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<TR><TD>%s</TD><TD CELLPADDING=\"4\"><TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD BGCOLOR=\"%s\"></TD></TR></TABLE></TD></TR>\r\n", name, color);
	return len;
}

int DebugInfo::graphviz_legend(char *str, int maxlen)
{
	int len = 0;
	
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "{\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "rank = sink;\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Legend [shape=none, margin=0, label=<\r\n");

	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "  <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>\r\n");

	len += graphviz_legend_color("NodeOperation", "gainsboro", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_color("Output", "dodgerblue1", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_color("Viewer", "lightskyblue3", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_color("Active Viewer", "lightskyblue1", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_color("Write Buffer", "darkorange", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_color("Read Buffer", "darkolivegreen3", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_color("Input Value", "khaki1", str + len, maxlen > len ? maxlen - len : 0);

	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<TR><TD></TD></TR>\r\n");

	len += graphviz_legend_group("Group Waiting", "white", "dashed", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_group("Group Running", "firebrick1", "solid", str + len, maxlen > len ? maxlen - len : 0);
	len += graphviz_legend_group("Group Finished", "chartreuse4", "solid", str + len, maxlen > len ? maxlen - len : 0);

	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "</TABLE>\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, ">];\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\r\n");

	return len;
}

bool DebugInfo::graphviz_system(const ExecutionSystem *system, char *str, int maxlen)
{
	char strbuf[64];
	int len = 0;
	
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "digraph compositorexecution {\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "ranksep=1.5\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "rankdir=LR\r\n");
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "splines=false\r\n");
	
#if 0
	for (ExecutionSystem::Operations::const_iterator it = system->m_operations.begin();
	     it != system->m_operations.end(); ++it) {
		NodeOperation *op = *it;
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "// OPERATION: %s\r\n", node->getbNode()->typeinfo->ui_name);
	}
#endif
	
	int totops = system->m_operations.size();
	int totgroups = system->m_groups.size();
	std::map<NodeOperation *, std::vector<std::string> > op_groups;
	for (int i = 0; i < totgroups; ++i) {
		const ExecutionGroup *group = system->m_groups[i];
		
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "// GROUP: %d\r\n", i);
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "subgraph cluster_%d{\r\n", i);
		/* used as a check for executing group */
		if (m_group_states[group] == EG_WAIT) {
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "style=dashed\r\n");
		}
		else if (m_group_states[group] == EG_RUNNING) {
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "style=filled\r\n");
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "color=black\r\n");
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "fillcolor=firebrick1\r\n");
		}
		else if (m_group_states[group] == EG_FINISHED) {
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "style=filled\r\n");
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "color=black\r\n");
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "fillcolor=chartreuse4\r\n");
		}
		
		for (ExecutionGroup::Operations::const_iterator it = group->m_operations.begin(); it != group->m_operations.end(); ++it) {
			NodeOperation *operation = *it;
			
			sprintf(strbuf, "_%p", group);
			op_groups[operation].push_back(std::string(strbuf));
			
			len += graphviz_operation(system, operation, group, str + len, maxlen > len ? maxlen - len : 0);
		}
		
//		len += snprintf(str+len, maxlen>len ? maxlen-len : 0, "//  OUTPUTOPERATION: %p\r\n", group->getOutputOperation());
//		len += snprintf(str+len, maxlen>len ? maxlen-len : 0, " O_%p\r\n", group->getOutputOperation());
		len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\r\n");
	}
	
	/* operations not included in any group */
	for (int j = 0; j < totops; ++j) {
		NodeOperation *operation = system->m_operations[j];
		if (op_groups.find(operation) != op_groups.end())
			continue;
		
		op_groups[operation].push_back(std::string(""));
		
		len += graphviz_operation(system, operation, 0, str + len, maxlen > len ? maxlen - len : 0);
	}
	
	for (int i = 0; i < totops; i++) {
		NodeOperation *operation = system->m_operations[i];
		
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *read = (ReadBufferOperation *)operation;
			WriteBufferOperation *write = read->getMemoryProxy()->getWriteBufferOperation();
			std::vector<std::string> &read_groups = op_groups[read];
			std::vector<std::string> &write_groups = op_groups[write];
			
			for (int k = 0; k < write_groups.size(); ++k) {
				for (int l = 0; l < read_groups.size(); ++l) {
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\"O_%p%s\" -> \"O_%p%s\" [style=dotted]\r\n", write, write_groups[k].c_str(), read, read_groups[l].c_str());
				}
			}
		}
	}
	
	for (int i = 0; i < totops; i++) {
		NodeOperation *op = system->m_operations[i];
		
		for (NodeOperation::Inputs::const_iterator it = op->m_inputs.begin(); it != op->m_inputs.end(); ++it) {
			NodeOperationInput *to = *it;
			NodeOperationOutput *from = to->getLink();
			
			if (!from)
				continue;
			
			std::string color;
			switch (from->getDataType()) {
				case COM_DT_VALUE:
					color = "grey";
					break;
				case COM_DT_VECTOR:
					color = "blue";
					break;
				case COM_DT_COLOR:
					color = "orange";
					break;
			}
			
			NodeOperation *to_op = &to->getOperation();
			NodeOperation *from_op = &from->getOperation();
			std::vector<std::string> &from_groups = op_groups[from_op];
			std::vector<std::string> &to_groups = op_groups[to_op];
			
			len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "// CONNECTION: %p.%p -> %p.%p\r\n",
			                from_op, from, to_op, to);
			for (int k = 0; k < from_groups.size(); ++k) {
				for (int l = 0; l < to_groups.size(); ++l) {
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\"O_%p%s\":\"OUT_%p\":e -> \"O_%p%s\":\"IN_%p\":w",
					                from_op, from_groups[k].c_str(), from, to_op, to_groups[l].c_str(), to);
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, " [color=%s]", color.c_str());
					len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\r\n");
				}
			}
		}
	}
	
	len += graphviz_legend(str + len, maxlen > len ? maxlen - len : 0);
	
	len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\r\n");
	
	return (len < maxlen);
}

void DebugInfo::graphviz(const ExecutionSystem *system)
{
	char str[1000000];
	if (graphviz_system(system, str, sizeof(str) - 1)) {
		char basename[FILE_MAX];
		char filename[FILE_MAX];
		
		BLI_snprintf(basename, sizeof(basename), "compositor_%d.dot", m_file_index);
		BLI_join_dirfile(filename, sizeof(filename), BKE_tempdir_session(), basename);
		++m_file_index;
		
		FILE *fp = BLI_fopen(filename, "wb");
		fputs(str, fp);
		fclose(fp);
	}
}

#else

std::string DebugInfo::node_name(const Node * /*node*/) { return ""; }
std::string DebugInfo::operation_name(const NodeOperation * /*op*/) { return ""; }
void DebugInfo::convert_started() {}
void DebugInfo::execute_started(const ExecutionSystem * /*system*/) {}
void DebugInfo::node_added(const Node * /*node*/) {}
void DebugInfo::node_to_operations(const Node * /*node*/) {}
void DebugInfo::operation_added(const NodeOperation * /*operation*/) {}
void DebugInfo::operation_read_write_buffer(const NodeOperation * /*operation*/) {}
void DebugInfo::execution_group_started(const ExecutionGroup * /*group*/) {}
void DebugInfo::execution_group_finished(const ExecutionGroup * /*group*/) {}
void DebugInfo::graphviz(const ExecutionSystem * /*system*/) {}

#endif
