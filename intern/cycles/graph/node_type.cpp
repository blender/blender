/*
 * Copyright 2011-2016 Blender Foundation
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

#include "graph/node_type.h"
#include "util/util_foreach.h"
#include "util/util_transform.h"

CCL_NAMESPACE_BEGIN

/* Node Socket Type */

size_t SocketType::size() const
{
	return size(type);
}

bool SocketType::is_array() const
{
	return (type >= BOOLEAN_ARRAY);
}

size_t SocketType::size(Type type)
{
	switch(type)
	{
		case UNDEFINED: return 0;

		case BOOLEAN: return sizeof(bool);
		case FLOAT: return sizeof(float);
		case INT: return sizeof(int);
		case UINT: return sizeof(uint);
		case COLOR: return sizeof(float3);
		case VECTOR: return sizeof(float3);
		case POINT: return sizeof(float3);
		case NORMAL: return sizeof(float3);
		case POINT2: return sizeof(float2);
		case CLOSURE: return 0;
		case STRING: return sizeof(ustring);
		case ENUM: return sizeof(int);
		case TRANSFORM: return sizeof(Transform);
		case NODE: return sizeof(void*);

		case BOOLEAN_ARRAY: return sizeof(array<bool>);
		case FLOAT_ARRAY: return sizeof(array<float>);
		case INT_ARRAY: return sizeof(array<int>);
		case COLOR_ARRAY: return sizeof(array<float3>);
		case VECTOR_ARRAY: return sizeof(array<float3>);
		case POINT_ARRAY: return sizeof(array<float3>);
		case NORMAL_ARRAY: return sizeof(array<float3>);
		case POINT2_ARRAY: return sizeof(array<float2>);
		case STRING_ARRAY: return sizeof(array<ustring>);
		case TRANSFORM_ARRAY: return sizeof(array<Transform>);
		case NODE_ARRAY: return sizeof(array<void*>);
	}

	assert(0);
	return 0;
}

size_t SocketType::max_size()
{
	return sizeof(Transform);
}

void *SocketType::zero_default_value()
{
	static Transform zero_transform = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
	return &zero_transform;
}

ustring SocketType::type_name(Type type)
{
	static ustring names[] = {
		ustring("undefined"),

		ustring("boolean"),
		ustring("float"),
		ustring("int"),
		ustring("uint"),
		ustring("color"),
		ustring("vector"),
		ustring("point"),
		ustring("normal"),
		ustring("point2"),
		ustring("closure"),
		ustring("string"),
		ustring("enum"),
		ustring("transform"),
		ustring("node"),

		ustring("array_boolean"),
		ustring("array_float"),
		ustring("array_int"),
		ustring("array_color"),
		ustring("array_vector"),
		ustring("array_point"),
		ustring("array_normal"),
		ustring("array_point2"),
		ustring("array_string"),
		ustring("array_transform"),
		ustring("array_node")};

	return names[(int)type];
}

bool SocketType::is_float3(Type type)
{
	return (type == COLOR || type == VECTOR || type == POINT || type == NORMAL);
}

/* Node Type */

NodeType::NodeType(Type type_)
: type(type_)
{
}

NodeType::~NodeType()
{
}

void NodeType::register_input(ustring name, ustring ui_name, SocketType::Type type, int struct_offset,
                              const void *default_value, const NodeEnum *enum_values,
                              const NodeType **node_type, int flags, int extra_flags)
{
	SocketType socket;
	socket.name = name;
	socket.ui_name = ui_name;
	socket.type = type;
	socket.struct_offset = struct_offset;
	socket.default_value = default_value;
	socket.enum_values = enum_values;
	socket.node_type = node_type;
	socket.flags = flags | extra_flags;
	inputs.push_back(socket);
}

void NodeType::register_output(ustring name, ustring ui_name, SocketType::Type type)
{
	SocketType socket;
	socket.name = name;
	socket.ui_name = ui_name;
	socket.type = type;
	socket.struct_offset = 0;
	socket.default_value = NULL;
	socket.enum_values = NULL;
	socket.node_type = NULL;
	socket.flags = SocketType::LINKABLE;
	outputs.push_back(socket);
}

const SocketType *NodeType::find_input(ustring name) const
{
	foreach(const SocketType& socket, inputs) {
		if(socket.name == name) {
			return &socket;
		}
	}

	return NULL;
}

const SocketType *NodeType::find_output(ustring name) const
{
	foreach(const SocketType& socket, outputs) {
		if(socket.name == name) {
			return &socket;
		}
	}

	return NULL;
}

/* Node Type Registry */

unordered_map<ustring, NodeType, ustringHash>& NodeType::types()
{
	static unordered_map<ustring, NodeType, ustringHash> _types;
	return _types;
}

NodeType *NodeType::add(const char *name_, CreateFunc create_, Type type_)
{
	ustring name(name_);

	if(types().find(name) != types().end()) {
		fprintf(stderr, "Node type %s registered twice!\n", name_);
		assert(0);
		return NULL;
	}

	types()[name] = NodeType(type_);

	NodeType *type = &types()[name];
	type->name = name;
	type->create = create_;
	return type;
}

const NodeType *NodeType::find(ustring name)
{
	unordered_map<ustring, NodeType, ustringHash>::iterator it = types().find(name);
	return (it == types().end()) ? NULL : &it->second;
}

CCL_NAMESPACE_END
