/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#include "attribute.h"
#include "graph.h"
#include "nodes.h"
#include "shader.h"

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
	usage = USE_ALL;
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
	special_type = SHADER_SPECIAL_TYPE_NONE;
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

ShaderInput *ShaderNode::add_input(const char *name, ShaderSocketType type, float value, int usage)
{
	ShaderInput *input = new ShaderInput(this, name, type);
	input->value.x = value;
	input->usage = usage;
	inputs.push_back(input);
	return input;
}

ShaderInput *ShaderNode::add_input(const char *name, ShaderSocketType type, float3 value, int usage)
{
	ShaderInput *input = new ShaderInput(this, name, type);
	input->value = value;
	input->usage = usage;
	inputs.push_back(input);
	return input;
}

ShaderInput *ShaderNode::add_input(const char *name, ShaderSocketType type, ShaderInput::DefaultValue value, int usage)
{
	ShaderInput *input = add_input(name, type);
	input->default_value = value;
	input->usage = usage;
	return input;
}

ShaderOutput *ShaderNode::add_output(const char *name, ShaderSocketType type)
{
	ShaderOutput *output = new ShaderOutput(this, name, type);
	outputs.push_back(output);
	return output;
}

void ShaderNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	foreach(ShaderInput *input, inputs) {
		if(!input->link) {
			if(input->default_value == ShaderInput::TEXTURE_GENERATED) {
				if(shader->has_surface)
					attributes->add(ATTR_STD_GENERATED);
				if(shader->has_volume)
					attributes->add(ATTR_STD_GENERATED_TRANSFORM);
			}
			else if(input->default_value == ShaderInput::TEXTURE_UV) {
				if(shader->has_surface)
					attributes->add(ATTR_STD_UV);
			}
		}
	}
}

/* Graph */

ShaderGraph::ShaderGraph()
{
	finalized = false;
	num_node_ids = 0;
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
	node->id = num_node_ids++;
	nodes.push_back(node);
	return node;
}

OutputNode *ShaderGraph::output()
{
	return (OutputNode*)nodes.front();
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
		fprintf(stderr, "Cycles shader graph connect: input already connected.\n");
		return;
	}

	if(from->type != to->type) {
		/* for closures we can't do automatic conversion */
		if(from->type == SHADER_SOCKET_CLOSURE || to->type == SHADER_SOCKET_CLOSURE) {
			fprintf(stderr, "Cycles shader graph connect: can only connect closure to closure "
			        "(%s.%s to %s.%s).\n",
			        from->parent->name.c_str(), from->name,
			        to->parent->name.c_str(), to->name);
			return;
		}

		/* add automatic conversion node in case of type mismatch */
		ShaderNode *convert = add(new ConvertNode(from->type, to->type, true));

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
		refine_bump_nodes();

		if(do_bump)
			bump_from_displacement();

		ShaderInput *surface_in = output()->input("Surface");
		ShaderInput *volume_in = output()->input("Volume");

		/* todo: make this work when surface and volume closures are tangled up */

		if(surface_in->link)
			transform_multi_closure(surface_in->link->parent, NULL, false);
		if(volume_in->link)
			transform_multi_closure(volume_in->link->parent, NULL, true);

		finalized = true;
	}
}

void ShaderGraph::find_dependencies(set<ShaderNode*>& dependencies, ShaderInput *input)
{
	/* find all nodes that this input depends on directly and indirectly */
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

void ShaderGraph::remove_unneeded_nodes()
{
	vector<bool> removed(num_node_ids, false);
	bool any_node_removed = false;

	/* find and unlink proxy nodes */
	foreach(ShaderNode *node, nodes) {
		if(node->special_type == SHADER_SPECIAL_TYPE_PROXY) {
			ProxyNode *proxy = static_cast<ProxyNode*>(node);
			ShaderInput *input = proxy->inputs[0];
			ShaderOutput *output = proxy->outputs[0];

			/* temp. copy of the output links list.
			 * output->links is modified when we disconnect!
			 */
			vector<ShaderInput*> links(output->links);
			ShaderOutput *from = input->link;

			/* bypass the proxy node */
			if(from) {
				disconnect(input);
				foreach(ShaderInput *to, links) {
					disconnect(to);
					connect(from, to);
				}
			}
			else {
				foreach(ShaderInput *to, links) {
					/* remove any autoconvert nodes too if they lead to
					 * sockets with an automatically set default value */
					ShaderNode *tonode = to->parent;

					if(tonode->special_type == SHADER_SPECIAL_TYPE_AUTOCONVERT) {
						bool all_links_removed = true;
						vector<ShaderInput*> links = tonode->outputs[0]->links;

						foreach(ShaderInput *autoin, links) {
							if(autoin->default_value == ShaderInput::NONE)
								all_links_removed = false;
							else
								disconnect(autoin);
						}

						if(all_links_removed)
							removed[tonode->id] = true;
					}

					disconnect(to);
					
					/* transfer the default input value to the target socket */
					to->set(input->value);
					to->set(input->value_string);
				}
			}
			
			removed[proxy->id] = true;
			any_node_removed = true;
		}
		else if(node->special_type == SHADER_SPECIAL_TYPE_MIX_CLOSURE) {
			MixClosureNode *mix = static_cast<MixClosureNode*>(node);

			/* remove useless mix closures nodes */
			if(mix->outputs[0]->links.size() && mix->inputs[1]->link == mix->inputs[2]->link) {
				ShaderOutput *output = mix->inputs[1]->link;
				vector<ShaderInput*> inputs = mix->outputs[0]->links;

				foreach(ShaderInput *sock, mix->inputs)
					if(sock->link)
						disconnect(sock);

				foreach(ShaderInput *input, inputs) {
					disconnect(input);
					if(output)
						connect(output, input);
				}
				removed[mix->id] = true;
				any_node_removed = true;
			}
		
			/* remove unused mix closure input when factor is 0.0 or 1.0 */
			/* check for closure links and make sure factor link is disconnected */
			if(mix->outputs[0]->links.size() && mix->inputs[1]->link && mix->inputs[2]->link && !mix->inputs[0]->link) {
				/* factor 0.0 */
				if(mix->inputs[0]->value.x == 0.0f) {
					ShaderOutput *output = mix->inputs[1]->link;
					vector<ShaderInput*> inputs = mix->outputs[0]->links;

					foreach(ShaderInput *sock, mix->inputs)
						if(sock->link)
							disconnect(sock);

					foreach(ShaderInput *input, inputs) {
						disconnect(input);
						if(output)
							connect(output, input);
					}
					removed[mix->id] = true;
					any_node_removed = true;
				}
				/* factor 1.0 */
				else if(mix->inputs[0]->value.x == 1.0f) {
					ShaderOutput *output = mix->inputs[2]->link;
					vector<ShaderInput*> inputs = mix->outputs[0]->links;
					
					foreach(ShaderInput *sock, mix->inputs)
						if(sock->link)
							disconnect(sock);

					foreach(ShaderInput *input, inputs) {
						disconnect(input);
						if(output)
							connect(output, input);
					}
					removed[mix->id] = true;
					any_node_removed = true;
				}
			}
		}
		else if(node->special_type == SHADER_SPECIAL_TYPE_MIX_RGB) {
			MixNode *mix = static_cast<MixNode*>(node);

			/* remove unused Mix RGB inputs when factor is 0.0 or 1.0 */
			/* check for color links and make sure factor link is disconnected */
			if(mix->outputs[0]->links.size() && mix->inputs[1]->link && mix->inputs[2]->link && !mix->inputs[0]->link) {
				/* factor 0.0 */
				if(mix->inputs[0]->value.x == 0.0f) {
					ShaderOutput *output = mix->inputs[1]->link;
					vector<ShaderInput*> inputs = mix->outputs[0]->links;

					foreach(ShaderInput *sock, mix->inputs)
						if(sock->link)
							disconnect(sock);

					foreach(ShaderInput *input, inputs) {
						disconnect(input);
						if(output)
							connect(output, input);
					}
					removed[mix->id] = true;
					any_node_removed = true;
				}
				/* factor 1.0 */
				else if(mix->inputs[0]->value.x == 1.0f) {
					ShaderOutput *output = mix->inputs[2]->link;
					vector<ShaderInput*> inputs = mix->outputs[0]->links;

					foreach(ShaderInput *sock, mix->inputs)
						if(sock->link)
							disconnect(sock);

					foreach(ShaderInput *input, inputs) {
						disconnect(input);
						if(output)
							connect(output, input);
					}
					removed[mix->id] = true;
					any_node_removed = true;
				}
			}
		}
	}

	/* remove nodes */
	if(any_node_removed) {
		list<ShaderNode*> newnodes;

		foreach(ShaderNode *node, nodes) {
			if(!removed[node->id])
				newnodes.push_back(node);
			else
				delete node;
		}

		nodes = newnodes;
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
				fprintf(stderr, "Cycles shader graph: detected cycle in graph, connection removed.\n");
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
	/* remove proxy and unnecessary mix nodes */
	remove_unneeded_nodes();

	/* we do two things here: find cycles and break them, and remove unused
	 * nodes that don't feed into the output. how cycles are broken is
	 * undefined, they are invalid input, the important thing is to not crash */

	vector<bool> visited(num_node_ids, false);
	vector<bool> on_stack(num_node_ids, false);
	
	/* break cycles */
	break_cycles(output(), visited, on_stack);

	/* disconnect unused nodes */
	foreach(ShaderNode *node, nodes) {
		if(!visited[node->id]) {
			foreach(ShaderInput *to, node->inputs) {
				ShaderOutput *from = to->link;

				if(from) {
					to->link = NULL;
					from->links.erase(remove(from->links.begin(), from->links.end(), to), from->links.end());
				}
			}
		}
	}

	/* remove unused nodes */
	list<ShaderNode*> newnodes;

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
			if(!input->link && ((input->usage & ShaderInput::USE_SVM) || do_osl)) {
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
				else if(input->default_value == ShaderInput::TANGENT) {
					if(!geom)
						geom = new GeometryNode();

					connect(geom->output("Tangent"), input);
				}
			}
		}
	}

	if(geom)
		add(geom);
	if(texco)
		add(texco);
}

void ShaderGraph::refine_bump_nodes()
{
	/* we transverse the node graph looking for bump nodes, when we find them,
	 * like in bump_from_displacement(), we copy the sub-graph defined from "bump"
	 * input to the inputs "center","dx" and "dy" What is in "bump" input is moved
	 * to "center" input. */

	foreach(ShaderNode *node, nodes) {
		if(node->name == ustring("bump") && node->input("Height")->link) {
			ShaderInput *bump_input = node->input("Height");
			set<ShaderNode*> nodes_bump;

			/* make 2 extra copies of the subgraph defined in Bump input */
			map<ShaderNode*, ShaderNode*> nodes_dx;
			map<ShaderNode*, ShaderNode*> nodes_dy;

			/* find dependencies for the given input */
			find_dependencies(nodes_bump, bump_input );

			copy_nodes(nodes_bump, nodes_dx);
			copy_nodes(nodes_bump, nodes_dy);
	
			/* mark nodes to indicate they are use for bump computation, so
			   that any texture coordinates are shifted by dx/dy when sampling */
			foreach(ShaderNode *node, nodes_bump)
				node->bump = SHADER_BUMP_CENTER;
			foreach(NodePair& pair, nodes_dx)
				pair.second->bump = SHADER_BUMP_DX;
			foreach(NodePair& pair, nodes_dy)
				pair.second->bump = SHADER_BUMP_DY;

			ShaderOutput *out = bump_input->link;
			ShaderOutput *out_dx = nodes_dx[out->parent]->output(out->name);
			ShaderOutput *out_dy = nodes_dy[out->parent]->output(out->name);

			connect(out_dx, node->input("SampleX"));
			connect(out_dy, node->input("SampleY"));
			
			/* add generated nodes */
			foreach(NodePair& pair, nodes_dx)
				add(pair.second);
			foreach(NodePair& pair, nodes_dy)
				add(pair.second);
			
			/* connect what is connected is bump to samplecenter input*/
			connect(out , node->input("SampleCenter"));

			/* bump input is just for connectivity purpose for the graph input,
			 * we re-connected this input to samplecenter, so lets disconnect it
			 * from bump input */
			disconnect(bump_input);
		}
	}
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
	 * output the perturbed normal. */

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

	/* add set normal node and connect the bump normal ouput to the set normal
	 * output, so it can finally set the shader normal, note we are only doing
	 * this for bump from displacement, this will be the only bump allowed to
	 * overwrite the shader normal */
	ShaderNode *set_normal = add(new SetNormalNode());
	
	/* add bump node and connect copied graphs to it */
	ShaderNode *bump = add(new BumpNode());

	ShaderOutput *out = displacement_in->link;
	ShaderOutput *out_center = nodes_center[out->parent]->output(out->name);
	ShaderOutput *out_dx = nodes_dx[out->parent]->output(out->name);
	ShaderOutput *out_dy = nodes_dy[out->parent]->output(out->name);

	connect(out_center, bump->input("SampleCenter"));
	connect(out_dx, bump->input("SampleX"));
	connect(out_dy, bump->input("SampleY"));
	
	/* connect the bump out to the set normal in: */
	connect(bump->output("Normal"), set_normal->input("Direction"));

	/* connect bump output to normal input nodes that aren't set yet. actually
	 * this will only set the normal input to the geometry node that we created
	 * and connected to all other normal inputs already. */
	foreach(ShaderNode *node, nodes)
		foreach(ShaderInput *input, node->inputs)
			if(!input->link && input->default_value == ShaderInput::NORMAL)
				connect(set_normal->output("Normal"), input);

	/* for displacement bump, clear the normal input in case the above loop
	 * connected the setnormal out to the bump normalin */
	ShaderInput *bump_normal_in = bump->input("Normal");
	if(bump_normal_in)
		bump_normal_in->link = NULL;

	/* finally, add the copied nodes to the graph. we can't do this earlier
	 * because we would create dependency cycles in the above loop */
	foreach(NodePair& pair, nodes_center)
		add(pair.second);
	foreach(NodePair& pair, nodes_dx)
		add(pair.second);
	foreach(NodePair& pair, nodes_dy)
		add(pair.second);
}

void ShaderGraph::transform_multi_closure(ShaderNode *node, ShaderOutput *weight_out, bool volume)
{
	/* for SVM in multi closure mode, this transforms the shader mix/add part of
	 * the graph into nodes that feed weights into closure nodes. this is too
	 * avoid building a closure tree and then flattening it, and instead write it
	 * directly to an array */
	
	if(node->name == ustring("mix_closure") || node->name == ustring("add_closure")) {
		ShaderInput *fin = node->input("Fac");
		ShaderInput *cl1in = node->input("Closure1");
		ShaderInput *cl2in = node->input("Closure2");
		ShaderOutput *weight1_out, *weight2_out;

		if(fin) {
			/* mix closure: add node to mix closure weights */
			ShaderNode *mix_node = add(new MixClosureWeightNode());
			ShaderInput *fac_in = mix_node->input("Fac"); 
			ShaderInput *weight_in = mix_node->input("Weight"); 

			if(fin->link)
				connect(fin->link, fac_in);
			else
				fac_in->value = fin->value;

			if(weight_out)
				connect(weight_out, weight_in);

			weight1_out = mix_node->output("Weight1");
			weight2_out = mix_node->output("Weight2");
		}
		else {
			/* add closure: just pass on any weights */
			weight1_out = weight_out;
			weight2_out = weight_out;
		}

		if(cl1in->link)
			transform_multi_closure(cl1in->link->parent, weight1_out, volume);
		if(cl2in->link)
			transform_multi_closure(cl2in->link->parent, weight2_out, volume);
	}
	else {
		ShaderInput *weight_in = node->input((volume)? "VolumeMixWeight": "SurfaceMixWeight");

		/* not a closure node? */
		if(!weight_in)
			return;

		/* already has a weight connected to it? add weights */
		if(weight_in->link || weight_in->value.x != 0.0f) {
			ShaderNode *math_node = add(new MathNode());
			ShaderInput *value1_in = math_node->input("Value1");
			ShaderInput *value2_in = math_node->input("Value2");

			if(weight_in->link)
				connect(weight_in->link, value1_in);
			else
				value1_in->value = weight_in->value;

			if(weight_out)
				connect(weight_out, value2_in);
			else
				value2_in->value.x = 1.0f;

			weight_out = math_node->output("Value");
			if(weight_in->link)
				disconnect(weight_in);
		}

		/* connected to closure mix weight */
		if(weight_out)
			connect(weight_out, weight_in);
		else
			weight_in->value.x += 1.0f;
	}
}

void ShaderGraph::dump_graph(const char *filename)
{
	FILE *fd = fopen(filename, "w");

	if(fd == NULL) {
		printf("Error opening file for dumping the graph: %s\n", filename);
		return;
	}

	fprintf(fd, "digraph shader_graph {\n");
	fprintf(fd, "ranksep=1.5\n");
	fprintf(fd, "splines=false\n");

	foreach(ShaderNode *node, nodes) {
		fprintf(fd, "// NODE: %p\n", node);
		fprintf(fd,
		        "\"%p\" [shape=record,label=\"%s\"]\n",
		        node,
		        node->name.c_str());
	}

	foreach(ShaderNode *node, nodes) {
		foreach(ShaderOutput *output, node->outputs) {
			foreach(ShaderInput *input, output->links) {
				fprintf(fd,
				        "// CONNECTION: %p->%p (%s:%s)\n",
				        output,
				        input,
				        output->name, input->name);
				fprintf(fd,
				        "\"%p\":s -> \"%p\":n [label=\"%s:%s\"]\n",
				        output->parent,
				        input->parent,
				        output->name, input->name);
			}
		}
	}

	fprintf(fd, "}\n");
	fclose(fd);
}

CCL_NAMESPACE_END

