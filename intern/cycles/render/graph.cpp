/*
 * Copyright 2011, Blender Foundation.
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
 */

#include "attribute.h"
#include "graph.h"
#include "nodes.h"

#include "util_algorithm.h"
#include "util_debug.h"
#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Input and Output */

ShaderInput::ShaderInput(ShaderNode *parent_, const char *name_, ShaderSocketType type_)
{
	parent = parent_;
	name = name_;
	type = type_;
	link = NULL;
	value = make_float3(0, 0, 0);
	stack_offset = SVM_STACK_INVALID;
	default_value = NONE;
	osl_only = false;
}

ShaderOutput::ShaderOutput(ShaderNode *parent_, const char *name_, ShaderSocketType type_)
{
	parent = parent_;
	name = name_;
	type = type_;
	stack_offset = SVM_STACK_INVALID;
}

/* Node */

ShaderNode::ShaderNode(const char *name_)
{
	name = name_;
	id = -1;
	bump = SHADER_BUMP_NONE;
}

ShaderNode::~ShaderNode()
{
	foreach(ShaderInput *socket, inputs)
		delete socket;

	foreach(ShaderOutput *socket, outputs)
		delete socket;
}

ShaderInput *ShaderNode::input(const char *name)
{
	foreach(ShaderInput *socket, inputs)
		if(strcmp(socket->name, name) == 0)
			return socket;

	return NULL;
}

ShaderOutput *ShaderNode::output(const char *name)
{
	foreach(ShaderOutput *socket, outputs)
		if(strcmp(socket->name, name) == 0)
			return socket;

	return NULL;
}

ShaderInput *ShaderNode::add_input(const char *name, ShaderSocketType type, float value)
{
	ShaderInput *input = new ShaderInput(this, name, type);
	input->value.x = value;
	inputs.push_back(input);
	return input;
}

ShaderInput *ShaderNode::add_input(const char *name, ShaderSocketType type, float3 value)
{
	ShaderInput *input = new ShaderInput(this, name, type);
	input->value = value;
	inputs.push_back(input);
	return input;
}

ShaderInput *ShaderNode::add_input(const char *name, ShaderSocketType type, ShaderInput::DefaultValue value, bool osl_only)
{
	ShaderInput *input = add_input(name, type);
	input->default_value = value;
	input->osl_only = osl_only;
	return input;
}

ShaderOutput *ShaderNode::add_output(const char *name, ShaderSocketType type)
{
	ShaderOutput *output = new ShaderOutput(this, name, type);
	outputs.push_back(output);
	return output;
}

void ShaderNode::attributes(AttributeRequestSet *attributes)
{
	foreach(ShaderInput *input, inputs) {
		if(!input->link) {
			if(input->default_value == ShaderInput::TEXTURE_GENERATED)
				attributes->add(ATTR_STD_GENERATED);
			else if(input->default_value == ShaderInput::TEXTURE_UV)
				attributes->add(ATTR_STD_UV);
		}
	}
}

/* Graph */

ShaderGraph::ShaderGraph()
{
	finalized = false;
	add(new OutputNode());
}

ShaderGraph::~ShaderGraph()
{
	foreach(ShaderNode *node, nodes)
		delete node;
}

ShaderNode *ShaderGraph::add(ShaderNode *node)
{
	assert(!finalized);
	node->id = nodes.size();
	nodes.push_back(node);
	return node;
}

ShaderNode *ShaderGraph::output()
{
	return nodes.front();
}

ShaderGraph *ShaderGraph::copy()
{
	ShaderGraph *newgraph = new ShaderGraph();

	/* copy nodes */
	set<ShaderNode*> nodes_all;
	foreach(ShaderNode *node, nodes)
		nodes_all.insert(node);

	map<ShaderNode*, ShaderNode*> nodes_copy;
	copy_nodes(nodes_all, nodes_copy);

	/* add nodes (in same order, so output is still first) */
	newgraph->nodes.clear();
	foreach(ShaderNode *node, nodes)
		newgraph->add(nodes_copy[node]);

	return newgraph;
}

void ShaderGraph::connect(ShaderOutput *from, ShaderInput *to)
{
	assert(!finalized);
	assert(from && to);

	if(to->link) {
		fprintf(stderr, "ShaderGraph connect: input already connected.\n");
		return;
	}

	if(from->type != to->type) {
		/* for closures we can't do automatic conversion */
		if(from->type == SHADER_SOCKET_CLOSURE || to->type == SHADER_SOCKET_CLOSURE) {
			fprintf(stderr, "ShaderGraph connect: can only connect closure to closure.\n");
			return;
		}

		/* add automatic conversion node in case of type mismatch */
		ShaderNode *convert = add(new ConvertNode(from->type, to->type));

		connect(from, convert->inputs[0]);
		connect(convert->outputs[0], to);
	}
	else {
		/* types match, just connect */
		to->link = from;
		from->links.push_back(to);
	}
}

void ShaderGraph::disconnect(ShaderInput *to)
{
	assert(!finalized);
	assert(to->link);

	ShaderOutput *from = to->link;

	to->link = NULL;
	from->links.erase(remove(from->links.begin(), from->links.end(), to), from->links.end());
}

void ShaderGraph::finalize(bool do_bump, bool do_osl)
{
	/* before compiling, the shader graph may undergo a number of modifications.
	 * currently we set default geometry shader inputs, and create automatic bump
	 * from displacement. a graph can be finalized only once, and should not be
	 * modified afterwards. */

	if(!finalized) {
		clean();
		default_inputs(do_osl);
		if(do_bump)
			bump_from_displacement();

		finalized = true;
	}
}

void ShaderGraph::find_dependencies(set<ShaderNode*>& dependencies, ShaderInput *input)
{
	/* find all nodes that this input dependes on directly and indirectly */
	ShaderNode *node = (input->link)? input->link->parent: NULL;

	if(node) {
		foreach(ShaderInput *in, node->inputs)
			find_dependencies(dependencies, in);

		dependencies.insert(node);
	}
}

void ShaderGraph::copy_nodes(set<ShaderNode*>& nodes, map<ShaderNode*, ShaderNode*>& nnodemap)
{
	/* copy a set of nodes, and the links between them. the assumption is
	 * made that all nodes that inputs are linked to are in the set too. */

	/* copy nodes */
	foreach(ShaderNode *node, nodes) {
		ShaderNode *nnode = node->clone();
		nnodemap[node] = nnode;

		nnode->inputs.clear();
		nnode->outputs.clear();

		foreach(ShaderInput *input, node->inputs) {
			ShaderInput *ninput = new ShaderInput(*input);
			nnode->inputs.push_back(ninput);

			ninput->parent = nnode;
			ninput->link = NULL;
		}

		foreach(ShaderOutput *output, node->outputs) {
			ShaderOutput *noutput = new ShaderOutput(*output);
			nnode->outputs.push_back(noutput);

			noutput->parent = nnode;
			noutput->links.clear();
		}
	}

	/* recreate links */
	foreach(ShaderNode *node, nodes) {
		foreach(ShaderInput *input, node->inputs) {
			if(input->link) {
				/* find new input and output */
				ShaderNode *nfrom = nnodemap[input->link->parent];
				ShaderNode *nto = nnodemap[input->parent];
				ShaderOutput *noutput = nfrom->output(input->link->name);
				ShaderInput *ninput = nto->input(input->name);

				/* connect */
				connect(noutput, ninput);
			}
		}
	}
}

void ShaderGraph::remove_proxy_nodes(vector<bool>& removed)
{
	foreach(ShaderNode *node, nodes) {
		ProxyNode *proxy = dynamic_cast<ProxyNode*>(node);
		if (proxy) {
			ShaderInput *input = proxy->inputs[0];
			ShaderOutput *output = proxy->outputs[0];
			
			/* temp. copy of the output links list.
			 * output->links is modified when we disconnect!
			 */
			vector<ShaderInput*> links(output->links);
			ShaderOutput *from = input->link;
			
			/* bypass the proxy node */
			if (from) {
				disconnect(input);
				foreach(ShaderInput *to, links) {
					disconnect(to);
					connect(from, to);
				}
			}
			else {
				foreach(ShaderInput *to, links) {
					disconnect(to);
					
					/* transfer the default input value to the target socket */
					to->set(input->value);
				}
			}
			
			removed[proxy->id] = true;
		}

		/* remove useless mix closures nodes */
		MixClosureNode *mix = dynamic_cast<MixClosureNode*>(node);

		if(mix) {
			if(mix->outputs[0]->links.size() && mix->inputs[1]->link == mix->inputs[2]->link) {
				ShaderOutput *output = mix->inputs[1]->link;
				vector<ShaderInput*> inputs = mix->outputs[0]->links;

				foreach(ShaderInput *sock, mix->inputs)
					if(sock->link)
						disconnect(sock);

				foreach(ShaderInput *input, inputs) {
					disconnect(input);
					if (output)
						connect(output, input);
				}
			}
		}
	}
}

void ShaderGraph::break_cycles(ShaderNode *node, vector<bool>& visited, vector<bool>& on_stack)
{
	visited[node->id] = true;
	on_stack[node->id] = true;

	foreach(ShaderInput *input, node->inputs) {
		if(input->link) {
			ShaderNode *depnode = input->link->parent;

			if(on_stack[depnode->id]) {
				/* break cycle */
				disconnect(input);
				fprintf(stderr, "ShaderGraph: detected cycle in graph, connection removed.\n");
			}
			else if(!visited[depnode->id]) {
				/* visit dependencies */
				break_cycles(depnode, visited, on_stack);
			}
		}
	}

	on_stack[node->id] = false;
}

void ShaderGraph::clean()
{
	/* we do two things here: find cycles and break them, and remove unused
	 * nodes that don't feed into the output. how cycles are broken is
	 * undefined, they are invalid input, the important thing is to not crash */

	vector<bool> removed(nodes.size(), false);
	vector<bool> visited(nodes.size(), false);
	vector<bool> on_stack(nodes.size(), false);
	
	list<ShaderNode*> newnodes;
	
	/* remove proxy nodes */
	remove_proxy_nodes(removed);
	
	foreach(ShaderNode *node, nodes) {
		if(!removed[node->id])
			newnodes.push_back(node);
		else
			delete node;
	}
	nodes = newnodes;
	newnodes.clear();

	/* break cycles */
	break_cycles(output(), visited, on_stack);

	/* remove unused nodes */
	foreach(ShaderNode *node, nodes) {
		if(visited[node->id])
			newnodes.push_back(node);
		else
			delete node;
	}
	
	nodes = newnodes;
}

void ShaderGraph::default_inputs(bool do_osl)
{
	/* nodes can specify default texture coordinates, for now we give
	 * everything the position by default, except for the sky texture */

	ShaderNode *geom = NULL;
	ShaderNode *texco = NULL;

	foreach(ShaderNode *node, nodes) {
		foreach(ShaderInput *input, node->inputs) {
			if(!input->link && !(input->osl_only && !do_osl)) {
				if(input->default_value == ShaderInput::TEXTURE_GENERATED) {
					if(!texco)
						texco = new TextureCoordinateNode();

					connect(texco->output("Generated"), input);
				}
				else if(input->default_value == ShaderInput::TEXTURE_UV) {
					if(!texco)
						texco = new TextureCoordinateNode();

					connect(texco->output("UV"), input);
				}
				else if(input->default_value == ShaderInput::INCOMING) {
					if(!geom)
						geom = new GeometryNode();

					connect(geom->output("Incoming"), input);
				}
				else if(input->default_value == ShaderInput::NORMAL) {
					if(!geom)
						geom = new GeometryNode();

					connect(geom->output("Normal"), input);
				}
				else if(input->default_value == ShaderInput::POSITION) {
					if(!geom)
						geom = new GeometryNode();

					connect(geom->output("Position"), input);
				}
			}
		}
	}

	if(geom)
		add(geom);
	if(texco)
		add(texco);
}

void ShaderGraph::bump_from_displacement()
{
	/* generate bump mapping automatically from displacement. bump mapping is
	 * done using a 3-tap filter, computing the displacement at the center,
	 * and two other positions shifted by ray differentials.
	 *
	 * since the input to displacement is a node graph, we need to ensure that
	 * all texture coordinates use are shift by the ray differentials. for this
	 * reason we make 3 copies of the node subgraph defining the displacement,
	 * with each different geometry and texture coordinate nodes that generate
	 * different shifted coordinates.
	 *
	 * these 3 displacement values are then fed into the bump node, which will
	 * modify the normal. */

	ShaderInput *displacement_in = output()->input("Displacement");

	if(!displacement_in->link)
		return;
	
	/* find dependencies for the given input */
	set<ShaderNode*> nodes_displace;
	find_dependencies(nodes_displace, displacement_in);

	/* copy nodes for 3 bump samples */
	map<ShaderNode*, ShaderNode*> nodes_center;
	map<ShaderNode*, ShaderNode*> nodes_dx;
	map<ShaderNode*, ShaderNode*> nodes_dy;

	copy_nodes(nodes_displace, nodes_center);
	copy_nodes(nodes_displace, nodes_dx);
	copy_nodes(nodes_displace, nodes_dy);

	/* mark nodes to indicate they are use for bump computation, so
	 * that any texture coordinates are shifted by dx/dy when sampling */
	foreach(NodePair& pair, nodes_center)
		pair.second->bump = SHADER_BUMP_CENTER;
	foreach(NodePair& pair, nodes_dx)
		pair.second->bump = SHADER_BUMP_DX;
	foreach(NodePair& pair, nodes_dy)
		pair.second->bump = SHADER_BUMP_DY;

	/* add bump node and connect copied graphs to it */
	ShaderNode *bump = add(new BumpNode());

	ShaderOutput *out = displacement_in->link;
	ShaderOutput *out_center = nodes_center[out->parent]->output(out->name);
	ShaderOutput *out_dx = nodes_dx[out->parent]->output(out->name);
	ShaderOutput *out_dy = nodes_dy[out->parent]->output(out->name);

	connect(out_center, bump->input("SampleCenter"));
	connect(out_dx, bump->input("SampleX"));
	connect(out_dy, bump->input("SampleY"));

	/* connect bump output to normal input nodes that aren't set yet. actually
	 * this will only set the normal input to the geometry node that we created
	 * and connected to all other normal inputs already. */
	foreach(ShaderNode *node, nodes)
		foreach(ShaderInput *input, node->inputs)
			if(!input->link && input->default_value == ShaderInput::NORMAL)
				connect(bump->output("Normal"), input);
	
	/* finally, add the copied nodes to the graph. we can't do this earlier
	 * because we would create dependency cycles in the above loop */
	foreach(NodePair& pair, nodes_center)
		add(pair.second);
	foreach(NodePair& pair, nodes_dx)
		add(pair.second);
	foreach(NodePair& pair, nodes_dy)
		add(pair.second);
}

CCL_NAMESPACE_END

