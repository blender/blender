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

#include "render/constant_fold.h"
#include "render/graph.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

ConstantFolder::ConstantFolder(ShaderGraph *graph, ShaderNode *node, ShaderOutput *output, Scene *scene)
: graph(graph), node(node), output(output), scene(scene)
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
	VLOG(1) << "Folding " << node->name << "::" << output->name() << " to constant (" << value << ").";

	foreach(ShaderInput *sock, output->links) {
		sock->set(value);
	}

	graph->disconnect(output);
}

void ConstantFolder::make_constant(float3 value) const
{
	VLOG(1) << "Folding " << node->name << "::" << output->name() << " to constant " << value << ".";

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
	if(clamp) {
		value.x = saturate(value.x);
		value.y = saturate(value.y);
		value.z = saturate(value.z);
	}

	make_constant(value);
}

void ConstantFolder::make_zero() const
{
	if(output->type() == SocketType::FLOAT) {
		make_constant(0.0f);
	}
	else if(SocketType::is_float3(output->type())) {
		make_constant(make_float3(0.0f, 0.0f, 0.0f));
	}
	else {
		assert(0);
	}
}

void ConstantFolder::make_one() const
{
	if(output->type() == SocketType::FLOAT) {
		make_constant(1.0f);
	}
	else if(SocketType::is_float3(output->type())) {
		make_constant(make_float3(1.0f, 1.0f, 1.0f));
	}
	else {
		assert(0);
	}
}

void ConstantFolder::bypass(ShaderOutput *new_output) const
{
	assert(new_output);

	VLOG(1) << "Folding " << node->name << "::" << output->name() << " to socket " << new_output->parent->name << "::" << new_output->name() << ".";

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

	VLOG(1) << "Discarding closure " << node->name << ".";

	graph->disconnect(output);
}

void ConstantFolder::bypass_or_discard(ShaderInput *input) const
{
	assert(input->type() == SocketType::CLOSURE);

	if(input->link) {
		bypass(input->link);
	}
	else {
		discard();
	}
}

bool ConstantFolder::try_bypass_or_make_constant(ShaderInput *input, bool clamp) const
{
	if(input->type() != output->type()) {
		return false;
	}
	else if(!input->link) {
		if(input->type() == SocketType::FLOAT) {
			make_constant_clamp(node->get_float(input->socket_type), clamp);
			return true;
		}
		else if(SocketType::is_float3(input->type())) {
			make_constant_clamp(node->get_float3(input->socket_type), clamp);
			return true;
		}
	}
	else if(!clamp) {
		bypass(input->link);
		return true;
	}
	else {
		/* disconnect other inputs if we can't fully bypass due to clamp */
		foreach(ShaderInput *other, node->inputs) {
			if(other != input && other->link) {
				graph->disconnect(other);
			}
		}
	}

	return false;
}

bool ConstantFolder::is_zero(ShaderInput *input) const
{
	if(!input->link) {
		if(input->type() == SocketType::FLOAT) {
			return node->get_float(input->socket_type) == 0.0f;
		}
		else if(SocketType::is_float3(input->type())) {
			return node->get_float3(input->socket_type) ==
			       make_float3(0.0f, 0.0f, 0.0f);
		}
	}

	return false;
}

bool ConstantFolder::is_one(ShaderInput *input) const
{
	if(!input->link) {
		if(input->type() == SocketType::FLOAT) {
			return node->get_float(input->socket_type) == 1.0f;
		}
		else if(SocketType::is_float3(input->type())) {
			return node->get_float3(input->socket_type) ==
			       make_float3(1.0f, 1.0f, 1.0f);
		}
	}

	return false;
}

/* Specific nodes */

void ConstantFolder::fold_mix(NodeMix type, bool clamp) const
{
    ShaderInput *fac_in = node->input("Fac");
    ShaderInput *color1_in = node->input("Color1");
    ShaderInput *color2_in = node->input("Color2");

	float fac = saturate(node->get_float(fac_in->socket_type));
	bool fac_is_zero = !fac_in->link && fac == 0.0f;
	bool fac_is_one = !fac_in->link && fac == 1.0f;

	/* remove no-op node when factor is 0.0 */
	if(fac_is_zero) {
		/* note that some of the modes will clamp out of bounds values even without use_clamp */
		if(!(type == NODE_MIX_LIGHT || type == NODE_MIX_DODGE || type == NODE_MIX_BURN)) {
			if(try_bypass_or_make_constant(color1_in, clamp)) {
				return;
			}
		}
	}

	switch(type) {
		case NODE_MIX_BLEND:
			/* remove useless mix colors nodes */
			if(color1_in->link && color2_in->link) {
				if(color1_in->link == color2_in->link) {
					try_bypass_or_make_constant(color1_in, clamp);
					break;
				}
			}
			else if(!color1_in->link && !color2_in->link) {
				float3 color1 = node->get_float3(color1_in->socket_type);
				float3 color2 = node->get_float3(color2_in->socket_type);
				if(color1 == color2) {
					try_bypass_or_make_constant(color1_in, clamp);
					break;
				}
			}
			/* remove no-op mix color node when factor is 1.0 */
			if(fac_is_one) {
				try_bypass_or_make_constant(color2_in, clamp);
				break;
			}
			break;
		case NODE_MIX_ADD:
			/* 0 + X (fac 1) == X */
			if(is_zero(color1_in) && fac_is_one) {
				try_bypass_or_make_constant(color2_in, clamp);
			}
			/* X + 0 (fac ?) == X */
			else if(is_zero(color2_in)) {
				try_bypass_or_make_constant(color1_in, clamp);
			}
			break;
		case NODE_MIX_SUB:
			/* X - 0 (fac ?) == X */
			if(is_zero(color2_in)) {
				try_bypass_or_make_constant(color1_in, clamp);
			}
			/* X - X (fac 1) == 0 */
			else if(color1_in->link && color1_in->link == color2_in->link && fac_is_one) {
				make_zero();
			}
			break;
		case NODE_MIX_MUL:
			/* X * 1 (fac ?) == X, 1 * X (fac 1) == X */
			if(is_one(color1_in) && fac_is_one) {
				try_bypass_or_make_constant(color2_in, clamp);
			}
			else if(is_one(color2_in)) {
				try_bypass_or_make_constant(color1_in, clamp);
			}
			/* 0 * ? (fac ?) == 0, ? * 0 (fac 1) == 0 */
			else if(is_zero(color1_in)) {
				make_zero();
			}
			else if(is_zero(color2_in) && fac_is_one) {
				make_zero();
			}
			break;
		case NODE_MIX_DIV:
			/* X / 1 (fac ?) == X */
			if(is_one(color2_in)) {
				try_bypass_or_make_constant(color1_in, clamp);
			}
			/* 0 / ? (fac ?) == 0 */
			else if(is_zero(color1_in)) {
				make_zero();
			}
			break;
		default:
			break;
	}
}

void ConstantFolder::fold_math(NodeMath type, bool clamp) const
{
	ShaderInput *value1_in = node->input("Value1");
	ShaderInput *value2_in = node->input("Value2");

	switch(type) {
		case NODE_MATH_ADD:
			/* X + 0 == 0 + X == X */
			if(is_zero(value1_in)) {
				try_bypass_or_make_constant(value2_in, clamp);
			}
			else if(is_zero(value2_in)) {
				try_bypass_or_make_constant(value1_in, clamp);
			}
			break;
		case NODE_MATH_SUBTRACT:
			/* X - 0 == X */
			if(is_zero(value2_in)) {
				try_bypass_or_make_constant(value1_in, clamp);
			}
			break;
		case NODE_MATH_MULTIPLY:
			/* X * 1 == 1 * X == X */
			if(is_one(value1_in)) {
				try_bypass_or_make_constant(value2_in, clamp);
			}
			else if(is_one(value2_in)) {
				try_bypass_or_make_constant(value1_in, clamp);
			}
			/* X * 0 == 0 * X == 0 */
			else if(is_zero(value1_in) || is_zero(value2_in)) {
				make_zero();
			}
			break;
		case NODE_MATH_DIVIDE:
			/* X / 1 == X */
			if(is_one(value2_in)) {
				try_bypass_or_make_constant(value1_in, clamp);
			}
			/* 0 / X == 0 */
			else if(is_zero(value1_in)) {
				make_zero();
			}
			break;
		case NODE_MATH_POWER:
			/* 1 ^ X == X ^ 0 == 1 */
			if(is_one(value1_in) || is_zero(value2_in)) {
				make_one();
			}
			/* X ^ 1 == X */
			else if(is_one(value2_in)) {
				try_bypass_or_make_constant(value1_in, clamp);
			}
		default:
			break;
	}
}

void ConstantFolder::fold_vector_math(NodeVectorMath type) const
{
	ShaderInput *vector1_in = node->input("Vector1");
	ShaderInput *vector2_in = node->input("Vector2");

	switch(type) {
		case NODE_VECTOR_MATH_ADD:
			/* X + 0 == 0 + X == X */
			if(is_zero(vector1_in)) {
				try_bypass_or_make_constant(vector2_in);
			}
			else if(is_zero(vector2_in)) {
				try_bypass_or_make_constant(vector1_in);
			}
			break;
		case NODE_VECTOR_MATH_SUBTRACT:
			/* X - 0 == X */
			if(is_zero(vector2_in)) {
				try_bypass_or_make_constant(vector1_in);
			}
			break;
		case NODE_VECTOR_MATH_DOT_PRODUCT:
		case NODE_VECTOR_MATH_CROSS_PRODUCT:
			/* X * 0 == 0 * X == 0 */
			if(is_zero(vector1_in) || is_zero(vector2_in)) {
				make_zero();
			}
			break;
		default:
			break;
	}
}

CCL_NAMESPACE_END
