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

#ifndef _COM_NodeGraph_h
#define _COM_NodeGraph_h

#include <map>
#include <set>
#include <vector>

extern "C" {
#include "DNA_node_types.h"
}

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

class CompositorContext;
class Node;
class NodeInput;
class NodeOutput;

/** Internal representation of DNA node data.
 *  This structure is converted into operations by \a NodeCompiler.
 */
class NodeGraph {
public:
	class Link {
	private:
		NodeOutput *m_from;
		NodeInput *m_to;
		
	public:
		Link(NodeOutput *from, NodeInput *to) :
		    m_from(from),
		    m_to(to)
		{}
		
		NodeOutput *getFromSocket() const { return m_from; }
		NodeInput *getToSocket() const { return m_to; }
	};
	
	typedef std::vector<Node *> Nodes;
	typedef Nodes::iterator NodeIterator;
	typedef std::vector<Link> Links;
	
private:
	Nodes m_nodes;
	Links m_links;
	
public:
	NodeGraph();
	~NodeGraph();
	
	const Nodes &nodes() const { return m_nodes; }
	const Links &links() const { return m_links; }
	
	void from_bNodeTree(const CompositorContext &context, bNodeTree *tree);
	
protected:
	typedef std::pair<NodeIterator, NodeIterator> NodeRange;
	typedef std::vector<NodeInput *> NodeInputs;
	
	static bNodeSocket *find_b_node_input(bNode *b_node, const char *identifier);
	static bNodeSocket *find_b_node_output(bNode *b_node, const char *identifier);
	
	void add_node(Node *node, bNodeTree *b_ntree, bNodeInstanceKey key, bool is_active_group);
	void add_link(NodeOutput *fromSocket, NodeInput *toSocket);
	
	void add_bNodeTree(const CompositorContext &context, int nodes_start, bNodeTree *tree, bNodeInstanceKey parent_key);
	
	void add_bNode(const CompositorContext &context, bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	
	NodeInputs find_inputs(const NodeRange &node_range, bNodeSocket *b_socket);
	NodeOutput *find_output(const NodeRange &node_range, bNodeSocket *b_socket);
	void add_bNodeLink(const NodeRange &node_range, bNodeLink *bNodeLink);
	
	/* **** Special proxy node type conversions **** */
	/* These nodes are not represented in the node graph themselves,
	 * but converted into a number of proxy links
	 */
	
	void add_proxies_mute(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	void add_proxies_skip(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	
	void add_proxies_group_inputs(bNode *b_node, bNode *b_node_io);
	void add_proxies_group_outputs(bNode *b_node, bNode *b_node_io, bool use_buffer);
	void add_proxies_group(const CompositorContext &context, bNode *b_node, bNodeInstanceKey key);

	void add_proxies_reroute(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeGraph")
#endif
};

#endif /* _COM_NodeGraph_h */
