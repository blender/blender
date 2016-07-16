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

#include "constant_fold.h"
#include "graph.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

ConstantFolder::ConstantFolder(ShaderGraph *graph, ShaderNode *node, ShaderOutput *output)
: graph(graph), node(node), output(output)
{
}

bool ConstantFolder::all_inputs_constant() const
{
	foreach(ShaderInput *input, node->inputs) {
		if(input->link) {
			return false;
		}
	}

	return true;
}

void ConstantFolder::make_constant(float value) const
{
	foreach(ShaderInput *sock, output->links) {
		sock->set(value);
	}

	graph->disconnect(output);
}

void ConstantFolder::make_constant(float3 value) const
{
	foreach(ShaderInput *sock, output->links) {
		sock->set(value);
	}

	graph->disconnect(output);
}

void ConstantFolder::make_constant_clamp(float value, bool clamp) const
{
	make_constant(clamp ? saturate(value) : value);
}

void ConstantFolder::make_constant_clamp(float3 value, bool clamp) const
{
	if (clamp) {
		value.x = saturate(value.x);
		value.y = saturate(value.y);
		value.z = saturate(value.z);
	}

	make_constant(value);
}

void ConstantFolder::bypass(ShaderOutput *new_output) const
{
	assert(new_output);

	/* Remove all outgoing links from socket and connect them to new_output instead.
	 * The graph->relink method affects node inputs, so it's not safe to use in constant
	 * folding if the node has multiple outputs and will thus be folded multiple times. */
	vector<ShaderInput*> outputs = output->links;

	graph->disconnect(output);

	foreach(ShaderInput *sock, outputs) {
		graph->connect(new_output, sock);
	}
}

void ConstantFolder::discard() const
{
	assert(output->type() == SocketType::CLOSURE);
	graph->disconnect(output);
}

void ConstantFolder::bypass_or_discard(ShaderInput *input) const
{
	assert(input->type() == SocketType::CLOSURE);

	if (input->link) {
		bypass(input->link);
	}
	else {
		discard();
	}
}

bool ConstantFolder::try_bypass_or_make_constant(ShaderInput *input, float3 input_value, bool clamp) const
{
	if(!input->link) {
		make_constant_clamp(input_value, clamp);
		return true;
	}
	else if(!clamp) {
		bypass(input->link);
		return true;
	}

	return false;
}

CCL_NAMESPACE_END
