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
 * limitations under the License.
 */

#include "device.h"
#include "graph.h"
#include "light.h"
#include "mesh.h"
#include "nodes.h"
#include "scene.h"
#include "shader.h"
#include "svm.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

/* Shader Manager */

SVMShaderManager::SVMShaderManager()
{
}

SVMShaderManager::~SVMShaderManager()
{
}

void SVMShaderManager::reset(Scene *scene)
{
}

void SVMShaderManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	/* test if we need to update */
	device_free(device, dscene, scene);

	/* determine which shaders are in use */
	device_update_shaders_used(scene);

	/* svm_nodes */
	vector<int4> svm_nodes;
	size_t i;

	for(i = 0; i < scene->shaders.size(); i++) {
		svm_nodes.push_back(make_int4(NODE_SHADER_JUMP, 0, 0, 0));
		svm_nodes.push_back(make_int4(NODE_SHADER_JUMP, 0, 0, 0));
	}
	
	for(i = 0; i < scene->shaders.size(); i++) {
		Shader *shader = scene->shaders[i];

		if(progress.get_cancel()) return;

		assert(shader->graph);

		if(shader->use_mis && shader->has_surface_emission)
			scene->light_manager->need_update = true;

		SVMCompiler compiler(scene->shader_manager, scene->image_manager);
		compiler.background = ((int)i == scene->default_background);
		compiler.compile(shader, svm_nodes, i);
	}

	dscene->svm_nodes.copy((uint4*)&svm_nodes[0], svm_nodes.size());
	device->tex_alloc("__svm_nodes", dscene->svm_nodes);

	for(i = 0; i < scene->shaders.size(); i++) {
		Shader *shader = scene->shaders[i];
		shader->need_update = false;
	}

	device_update_common(device, dscene, scene, progress);

	need_update = false;
}

void SVMShaderManager::device_free(Device *device, DeviceScene *dscene, Scene *scene)
{
	device_free_common(device, dscene, scene);

	device->tex_free(dscene->svm_nodes);
	dscene->svm_nodes.clear();
}

/* Graph Compiler */

SVMCompiler::SVMCompiler(ShaderManager *shader_manager_, ImageManager *image_manager_)
{
	shader_manager = shader_manager_;
	image_manager = image_manager_;
	max_stack_use = 0;
	current_type = SHADER_TYPE_SURFACE;
	current_shader = NULL;
	current_graph = NULL;
	background = false;
	mix_weight_offset = SVM_STACK_INVALID;
	compile_failed = false;
}

int SVMCompiler::stack_size(ShaderSocketType type)
{
	int size = 0;
	
	switch (type) {
		case SHADER_SOCKET_FLOAT:
		case SHADER_SOCKET_INT:
			size = 1;
			break;
		case SHADER_SOCKET_COLOR:
		case SHADER_SOCKET_VECTOR:
		case SHADER_SOCKET_NORMAL:
		case SHADER_SOCKET_POINT:
			size = 3;
			break;
		case SHADER_SOCKET_CLOSURE:
			size = 0;
			break;
		default:
			assert(0);
			break;
	}
	
	return size;
}

int SVMCompiler::stack_find_offset(ShaderSocketType type)
{
	int size = stack_size(type);
	int offset = -1;
	
	/* find free space in stack & mark as used */
	for(int i = 0, num_unused = 0; i < SVM_STACK_SIZE; i++) {
		if(active_stack.users[i]) num_unused = 0;
		else num_unused++;

		if(num_unused == size) {
			offset = i+1 - size;
			max_stack_use = max(i+1, max_stack_use);

			while(i >= offset)
				active_stack.users[i--] = 1;

			return offset;
		}
	}

	if(!compile_failed) {
		compile_failed = true;
		fprintf(stderr, "Cycles: out of SVM stack space, shader \"%s\" too big.\n", current_shader->name.c_str());
	}

	return 0;
}

void SVMCompiler::stack_clear_offset(ShaderSocketType type, int offset)
{
	int size = stack_size(type);

	for(int i = 0; i < size; i++)
		active_stack.users[offset + i]--;
}

void SVMCompiler::stack_backup(StackBackup& backup, set<ShaderNode*>& done)
{
	backup.done = done;
	backup.stack = active_stack;

	foreach(ShaderNode *node, current_graph->nodes) {
		foreach(ShaderInput *input, node->inputs)
			backup.offsets.push_back(input->stack_offset);
		foreach(ShaderOutput *output, node->outputs)
			backup.offsets.push_back(output->stack_offset);
	}
}

void SVMCompiler::stack_restore(StackBackup& backup, set<ShaderNode*>& done)
{
	int i = 0;

	done = backup.done;
	active_stack = backup.stack;

	foreach(ShaderNode *node, current_graph->nodes) {
		foreach(ShaderInput *input, node->inputs)
			input->stack_offset = backup.offsets[i++];
		foreach(ShaderOutput *output, node->outputs)
			output->stack_offset = backup.offsets[i++];
	}
}

void SVMCompiler::stack_assign(ShaderInput *input)
{
	/* stack offset assign? */
	if(input->stack_offset == SVM_STACK_INVALID) {
		if(input->link) {
			/* linked to output -> use output offset */
			input->stack_offset = input->link->stack_offset;
		}
		else {
			/* not linked to output -> add nodes to load default value */
			input->stack_offset = stack_find_offset(input->type);

			if(input->type == SHADER_SOCKET_FLOAT) {
				add_node(NODE_VALUE_F, __float_as_int(input->value.x), input->stack_offset);
			}
			else if(input->type == SHADER_SOCKET_INT) {
				add_node(NODE_VALUE_F, (int)input->value.x, input->stack_offset);
			}
			else if(input->type == SHADER_SOCKET_VECTOR ||
			        input->type == SHADER_SOCKET_NORMAL ||
			        input->type == SHADER_SOCKET_POINT ||
			        input->type == SHADER_SOCKET_COLOR)
			{

				add_node(NODE_VALUE_V, input->stack_offset);
				add_node(NODE_VALUE_V, input->value);
			}
			else /* should not get called for closure */
				assert(0);
		}
	}
}

void SVMCompiler::stack_assign(ShaderOutput *output)
{
	/* if no stack offset assigned yet, find one */
	if(output->stack_offset == SVM_STACK_INVALID)
		output->stack_offset = stack_find_offset(output->type);
}

void SVMCompiler::stack_link(ShaderInput *input, ShaderOutput *output)
{
	if(output->stack_offset == SVM_STACK_INVALID) {
		assert(input->link);
		assert(stack_size(output->type) == stack_size(input->link->type));

		output->stack_offset = input->link->stack_offset;

		int size = stack_size(output->type);

		for(int i = 0; i < size; i++)
			active_stack.users[output->stack_offset + i]++;
	}
}

void SVMCompiler::stack_clear_users(ShaderNode *node, set<ShaderNode*>& done)
{
	/* optimization we should add:
	 * find and lower user counts for outputs for which all inputs are done.
	 * this is done before the node is compiled, under the assumption that the
	 * node will first load all inputs from the stack and then writes its
	 * outputs. this used to work, but was disabled because it gave trouble
	 * with inputs getting stack positions assigned */

	foreach(ShaderInput *input, node->inputs) {
		ShaderOutput *output = input->link;

		if(output && output->stack_offset != SVM_STACK_INVALID) {
			bool all_done = true;

			/* optimization we should add: verify if in->parent is actually used */
			foreach(ShaderInput *in, output->links)
				if(in->parent != node && done.find(in->parent) == done.end())
					all_done = false;

			if(all_done) {
				stack_clear_offset(output->type, output->stack_offset);
				output->stack_offset = SVM_STACK_INVALID;

				foreach(ShaderInput *in, output->links)
					in->stack_offset = SVM_STACK_INVALID;
			}
		}
	}
}

void SVMCompiler::stack_clear_temporary(ShaderNode *node)
{
	foreach(ShaderInput *input, node->inputs) {
		if(!input->link && input->stack_offset != SVM_STACK_INVALID) {
			stack_clear_offset(input->type, input->stack_offset);
			input->stack_offset = SVM_STACK_INVALID;
		}
	}
}

uint SVMCompiler::encode_uchar4(uint x, uint y, uint z, uint w)
{
	assert(x <= 255);
	assert(y <= 255);
	assert(z <= 255);
	assert(w <= 255);

	return (x) | (y << 8) | (z << 16) | (w << 24);
}

void SVMCompiler::add_node(int a, int b, int c, int d)
{
	svm_nodes.push_back(make_int4(a, b, c, d));
}

void SVMCompiler::add_node(NodeType type, int a, int b, int c)
{
	svm_nodes.push_back(make_int4(type, a, b, c));
}

void SVMCompiler::add_node(NodeType type, const float3& f)
{
	svm_nodes.push_back(make_int4(type,
		__float_as_int(f.x),
		__float_as_int(f.y),
		__float_as_int(f.z)));
}

void SVMCompiler::add_node(const float4& f)
{
	svm_nodes.push_back(make_int4(
		__float_as_int(f.x),
		__float_as_int(f.y),
		__float_as_int(f.z),
		__float_as_int(f.w)));
}

void SVMCompiler::add_array(float4 *f, int num)
{
	for(int i = 0; i < num; i++)
		add_node(f[i]);
}

uint SVMCompiler::attribute(ustring name)
{
	return shader_manager->get_attribute_id(name);
}

uint SVMCompiler::attribute(AttributeStandard std)
{
	return shader_manager->get_attribute_id(std);
}

bool SVMCompiler::node_skip_input(ShaderNode *node, ShaderInput *input)
{
	/* nasty exception .. */
	if(current_type == SHADER_TYPE_DISPLACEMENT && input->link && input->link->parent->name == ustring("bump"))
		return true;
	
	return false;
}

void SVMCompiler::find_dependencies(set<ShaderNode*>& dependencies,
                                    const set<ShaderNode*>& done,
                                    ShaderInput *input,
                                    ShaderNode *skip_node)
{
	ShaderNode *node = (input->link)? input->link->parent: NULL;

	if(node && done.find(node) == done.end() && node != skip_node) {
		foreach(ShaderInput *in, node->inputs)
			if(!node_skip_input(node, in))
				find_dependencies(dependencies, done, in, skip_node);

		dependencies.insert(node);
	}
}

void SVMCompiler::generate_node(ShaderNode *node, set<ShaderNode*>& done)
{
	node->compile(*this);
	stack_clear_users(node, done);
	stack_clear_temporary(node);

	if(current_type == SHADER_TYPE_VOLUME) {
		if(node->has_spatial_varying())
			current_shader->has_heterogeneous_volume = true;
	}

	/* detect if we have a blackbody converter, to prepare lookup table */
	if(node->has_converter_blackbody())
		current_shader->has_converter_blackbody = true;
}

void SVMCompiler::generate_svm_nodes(const set<ShaderNode*>& nodes, set<ShaderNode*>& done)
{
	bool nodes_done;

	do {
		nodes_done = true;

		foreach(ShaderNode *node, nodes) {
			if(done.find(node) == done.end()) {
				bool inputs_done = true;

				foreach(ShaderInput *input, node->inputs)
					if(!node_skip_input(node, input))
						if(input->link && done.find(input->link->parent) == done.end())
							inputs_done = false;

				if(inputs_done) {
					generate_node(node, done);
					done.insert(node);
				}
				else
					nodes_done = false;
			}
		}
	} while(!nodes_done);
}

void SVMCompiler::generate_closure_node(ShaderNode *node, set<ShaderNode*>& done)
{
	/* execute dependencies for closure */
	foreach(ShaderInput *in, node->inputs) {
		if(!node_skip_input(node, in) && in->link) {
			set<ShaderNode*> dependencies;
			find_dependencies(dependencies, done, in);
			generate_svm_nodes(dependencies, done);
		}
	}

	/* closure mix weight */
	const char *weight_name = (current_type == SHADER_TYPE_VOLUME)? "VolumeMixWeight": "SurfaceMixWeight";
	ShaderInput *weight_in = node->input(weight_name);

	if(weight_in && (weight_in->link || weight_in->value.x != 1.0f)) {
		stack_assign(weight_in);
		mix_weight_offset = weight_in->stack_offset;
	}
	else
		mix_weight_offset = SVM_STACK_INVALID;

	/* compile closure itself */
	generate_node(node, done);

	mix_weight_offset = SVM_STACK_INVALID;

	if(current_type == SHADER_TYPE_SURFACE) {
		if(node->has_surface_emission())
			current_shader->has_surface_emission = true;
		if(node->has_surface_transparent())
			current_shader->has_surface_transparent = true;
		if(node->has_surface_bssrdf()) {
			current_shader->has_surface_bssrdf = true;
			if(node->has_bssrdf_bump())
				current_shader->has_bssrdf_bump = true;
		}
	}
}

void SVMCompiler::generated_shared_closure_nodes(ShaderNode *root_node,
                                                 ShaderNode *node,
                                                 set<ShaderNode*>& done,
                                                 set<ShaderNode*>& closure_done,
                                                 const set<ShaderNode*>& shared)
{
	if(shared.find(node) != shared.end()) {
		generate_multi_closure(root_node, node, done, closure_done);
	}
	else {
		foreach(ShaderInput *in, node->inputs) {
			if(in->type == SHADER_SOCKET_CLOSURE && in->link)
				generated_shared_closure_nodes(root_node, in->link->parent,
				                               done, closure_done, shared);
		}
	}
}

void SVMCompiler::generate_multi_closure(ShaderNode *root_node,
                                         ShaderNode *node,
                                         set<ShaderNode*>& done,
                                         set<ShaderNode*>& closure_done)
{
	/* only generate once */
	if(closure_done.find(node) != closure_done.end())
		return;

	closure_done.insert(node);

	if(node->name == ustring("mix_closure") || node->name == ustring("add_closure")) {
		/* weighting is already taken care of in ShaderGraph::transform_multi_closure */
		ShaderInput *cl1in = node->input("Closure1");
		ShaderInput *cl2in = node->input("Closure2");
		ShaderInput *facin = node->input("Fac");

		/* skip empty mix/add closure nodes */
		if(!cl1in->link && !cl2in->link)
			return;

		if(facin && facin->link) {
			/* mix closure: generate instructions to compute mix weight */
			set<ShaderNode*> dependencies;
			find_dependencies(dependencies, done, facin);
			generate_svm_nodes(dependencies, done);

			stack_assign(facin);

			/* execute shared dependencies. this is needed to allow skipping
			 * of zero weight closures and their dependencies later, so we
			 * ensure that they only skip dependencies that are unique to them */
			set<ShaderNode*> cl1deps, cl2deps, shareddeps;

			find_dependencies(cl1deps, done, cl1in);
			find_dependencies(cl2deps, done, cl2in);

			set_intersection(cl1deps.begin(), cl1deps.end(),
			                 cl2deps.begin(), cl2deps.end(),
			                 std::inserter(shareddeps, shareddeps.begin()));

			/* it's possible some nodes are not shared between this mix node
			 * inputs, but still needed to be always executed, this mainly
			 * happens when a node of current subbranch is used by a parent
			 * node or so */
			if(root_node != node) {
				foreach(ShaderInput *in, root_node->inputs) {
					set<ShaderNode*> rootdeps;
					find_dependencies(rootdeps, done, in, node);
					set_intersection(rootdeps.begin(), rootdeps.end(),
					                 cl1deps.begin(), cl1deps.end(),
					                 std::inserter(shareddeps, shareddeps.begin()));
					set_intersection(rootdeps.begin(), rootdeps.end(),
					                 cl2deps.begin(), cl2deps.end(),
					                 std::inserter(shareddeps, shareddeps.begin()));
				}
			}

			if(!shareddeps.empty()) {
				if(cl1in->link) {
					generated_shared_closure_nodes(root_node, cl1in->link->parent,
					                               done, closure_done, shareddeps);
				}
				if(cl2in->link) {
					generated_shared_closure_nodes(root_node, cl2in->link->parent,
					                               done, closure_done, shareddeps);
				}

				generate_svm_nodes(shareddeps, done);
			}

			/* generate instructions for input closure 1 */
			if(cl1in->link) {
				/* add instruction to skip closure and its dependencies if mix weight is zero */
				svm_nodes.push_back(make_int4(NODE_JUMP_IF_ONE, 0, facin->stack_offset, 0));
				int node_jump_skip_index = svm_nodes.size() - 1;

				generate_multi_closure(root_node, cl1in->link->parent, done, closure_done);

				/* fill in jump instruction location to be after closure */
				svm_nodes[node_jump_skip_index].y = svm_nodes.size() - node_jump_skip_index - 1;
			}

			/* generate instructions for input closure 2 */
			if(cl2in->link) {
				/* add instruction to skip closure and its dependencies if mix weight is zero */
				svm_nodes.push_back(make_int4(NODE_JUMP_IF_ZERO, 0, facin->stack_offset, 0));
				int node_jump_skip_index = svm_nodes.size() - 1;

				generate_multi_closure(root_node, cl2in->link->parent, done, closure_done);

				/* fill in jump instruction location to be after closure */
				svm_nodes[node_jump_skip_index].y = svm_nodes.size() - node_jump_skip_index - 1;
			}

			/* unassign */
			facin->stack_offset = SVM_STACK_INVALID;
		}
		else {
			/* execute closures and their dependencies, no runtime checks
			 * to skip closures here because was already optimized due to
			 * fixed weight or add closure that always needs both */
			if(cl1in->link)
				generate_multi_closure(root_node, cl1in->link->parent, done, closure_done);
			if(cl2in->link)
				generate_multi_closure(root_node, cl2in->link->parent, done, closure_done);
		}
	}
	else {
		generate_closure_node(node, done);
	}

	done.insert(node);
}


void SVMCompiler::compile_type(Shader *shader, ShaderGraph *graph, ShaderType type)
{
	/* Converting a shader graph into svm_nodes that can be executed
	 * sequentially on the virtual machine is fairly simple. We can keep
	 * looping over nodes and each time all the inputs of a node are
	 * ready, we add svm_nodes for it that read the inputs from the
	 * stack and write outputs back to the stack.
	 *
	 * With the SVM, we always sample only a single closure. We can think
	 * of all closures nodes as a binary tree with mix closures as inner
	 * nodes and other closures as leafs. The SVM will traverse that tree,
	 * each time deciding to go left or right depending on the mix weights,
	 * until a closure is found.
	 *
	 * We only execute nodes that are needed for the mix weights and chosen
	 * closure.
	 */

	current_type = type;
	current_graph = graph;

	/* get input in output node */
	ShaderNode *node = graph->output();
	ShaderInput *clin = NULL;
	
	switch (type) {
		case SHADER_TYPE_SURFACE:
			clin = node->input("Surface");
			break;
		case SHADER_TYPE_VOLUME:
			clin = node->input("Volume");
			break;
		case SHADER_TYPE_DISPLACEMENT:
			clin = node->input("Displacement");
			break;
		default:
			assert(0);
			break;
	}

	/* clear all compiler state */
	memset(&active_stack, 0, sizeof(active_stack));
	svm_nodes.clear();

	foreach(ShaderNode *node_iter, graph->nodes) {
		foreach(ShaderInput *input, node_iter->inputs)
			input->stack_offset = SVM_STACK_INVALID;
		foreach(ShaderOutput *output, node_iter->outputs)
			output->stack_offset = SVM_STACK_INVALID;
	}

	if(shader->used) {
		if(clin->link) {
			bool generate = false;
			
			switch (type) {
				case SHADER_TYPE_SURFACE: /* generate surface shader */		
					generate = true;
					shader->has_surface = true;
					break;
				case SHADER_TYPE_VOLUME: /* generate volume shader */
					generate = true;
					shader->has_volume = true;
					break;
				case SHADER_TYPE_DISPLACEMENT: /* generate displacement shader */
					generate = true;
					shader->has_displacement = true;
					break;
				default:
					break;
			}

			if(generate) {
				set<ShaderNode*> done, closure_done;
				generate_multi_closure(clin->link->parent, clin->link->parent,
				                       done, closure_done);
			}
		}

		/* compile output node */
		node->compile(*this);
	}

	/* if compile failed, generate empty shader */
	if(compile_failed) {
		svm_nodes.clear();
		compile_failed = false;
	}

	add_node(NODE_END, 0, 0, 0);
}

void SVMCompiler::compile(Shader *shader, vector<int4>& global_svm_nodes, int index)
{
	/* copy graph for shader with bump mapping */
	ShaderNode *node = shader->graph->output();

	if(node->input("Surface")->link && node->input("Displacement")->link)
		if(!shader->graph_bump)
			shader->graph_bump = shader->graph->copy();

	/* finalize */
	shader->graph->finalize(false, false);
	if(shader->graph_bump)
		shader->graph_bump->finalize(true, false);

	current_shader = shader;

	shader->has_surface = false;
	shader->has_surface_emission = false;
	shader->has_surface_transparent = false;
	shader->has_surface_bssrdf = false;
	shader->has_bssrdf_bump = false;
	shader->has_converter_blackbody = false;
	shader->has_volume = false;
	shader->has_displacement = false;
	shader->has_heterogeneous_volume = false;

	/* generate surface shader */
	compile_type(shader, shader->graph, SHADER_TYPE_SURFACE);
	global_svm_nodes[index*2 + 0].y = global_svm_nodes.size();
	global_svm_nodes[index*2 + 1].y = global_svm_nodes.size();
	global_svm_nodes.insert(global_svm_nodes.end(), svm_nodes.begin(), svm_nodes.end());

	if(shader->graph_bump) {
		compile_type(shader, shader->graph_bump, SHADER_TYPE_SURFACE);
		global_svm_nodes[index*2 + 1].y = global_svm_nodes.size();
		global_svm_nodes.insert(global_svm_nodes.end(), svm_nodes.begin(), svm_nodes.end());
	}

	/* generate volume shader */
	compile_type(shader, shader->graph, SHADER_TYPE_VOLUME);
	global_svm_nodes[index*2 + 0].z = global_svm_nodes.size();
	global_svm_nodes[index*2 + 1].z = global_svm_nodes.size();
	global_svm_nodes.insert(global_svm_nodes.end(), svm_nodes.begin(), svm_nodes.end());

	/* generate displacement shader */
	compile_type(shader, shader->graph, SHADER_TYPE_DISPLACEMENT);
	global_svm_nodes[index*2 + 0].w = global_svm_nodes.size();
	global_svm_nodes[index*2 + 1].w = global_svm_nodes.size();
	global_svm_nodes.insert(global_svm_nodes.end(), svm_nodes.begin(), svm_nodes.end());
}

CCL_NAMESPACE_END

