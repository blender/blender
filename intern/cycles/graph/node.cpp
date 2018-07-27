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

#include "graph/node.h"
#include "graph/node_type.h"

#include "util/util_foreach.h"
#include "util/util_md5.h"
#include "util/util_param.h"
#include "util/util_transform.h"

CCL_NAMESPACE_BEGIN

/* Node Type */

Node::Node(const NodeType *type_, ustring name_)
: name(name_), type(type_)
{
	assert(type);

	/* assign non-empty name, convenient for debugging */
	if(name.empty()) {
		name = type->name;
	}

	/* initialize default values */
	foreach(const SocketType& socket, type->inputs) {
		set_default_value(socket);
	}
}

Node::~Node()
{
}

template<typename T>
static T& get_socket_value(const Node *node, const SocketType& socket)
{
	return (T&)*(((char*)node) + socket.struct_offset);
}

#ifndef NDEBUG
static bool is_socket_float3(const SocketType& socket)
{
	return socket.type == SocketType::COLOR ||
	       socket.type == SocketType::POINT ||
		   socket.type == SocketType::VECTOR ||
		   socket.type == SocketType::NORMAL;
}

static bool is_socket_array_float3(const SocketType& socket)
{
	return socket.type == SocketType::COLOR_ARRAY ||
	       socket.type == SocketType::POINT_ARRAY ||
		   socket.type == SocketType::VECTOR_ARRAY ||
		   socket.type == SocketType::NORMAL_ARRAY;
}
#endif

/* set values */
void Node::set(const SocketType& input, bool value)
{
	assert(input.type == SocketType::BOOLEAN);
	get_socket_value<bool>(this, input) = value;
}

void Node::set(const SocketType& input, int value)
{
	assert((input.type == SocketType::INT || input.type == SocketType::ENUM));
	get_socket_value<int>(this, input) = value;
}

void Node::set(const SocketType& input, uint value)
{
	assert(input.type == SocketType::UINT);
	get_socket_value<uint>(this, input) = value;
}

void Node::set(const SocketType& input, float value)
{
	assert(input.type == SocketType::FLOAT);
	get_socket_value<float>(this, input) = value;
}

void Node::set(const SocketType& input, float2 value)
{
	assert(input.type == SocketType::FLOAT);
	get_socket_value<float2>(this, input) = value;
}

void Node::set(const SocketType& input, float3 value)
{
	assert(is_socket_float3(input));
	get_socket_value<float3>(this, input) = value;
}

void Node::set(const SocketType& input, const char *value)
{
	set(input, ustring(value));
}

void Node::set(const SocketType& input, ustring value)
{
	if(input.type == SocketType::STRING) {
		get_socket_value<ustring>(this, input) = value;
	}
	else if(input.type == SocketType::ENUM) {
		const NodeEnum& enm = *input.enum_values;
		if(enm.exists(value)) {
			get_socket_value<int>(this, input) = enm[value];
		}
		else {
			assert(0);
		}
	}
	else {
		assert(0);
	}
}

void Node::set(const SocketType& input, const Transform& value)
{
	assert(input.type == SocketType::TRANSFORM);
	get_socket_value<Transform>(this, input) = value;
}

void Node::set(const SocketType& input, Node *value)
{
	assert(input.type == SocketType::TRANSFORM);
	get_socket_value<Node*>(this, input) = value;
}

/* set array values */
void Node::set(const SocketType& input, array<bool>& value)
{
	assert(input.type == SocketType::BOOLEAN_ARRAY);
	get_socket_value<array<bool> >(this, input).steal_data(value);
}

void Node::set(const SocketType& input, array<int>& value)
{
	assert(input.type == SocketType::INT_ARRAY);
	get_socket_value<array<int> >(this, input).steal_data(value);
}

void Node::set(const SocketType& input, array<float>& value)
{
	assert(input.type == SocketType::FLOAT_ARRAY);
	get_socket_value<array<float> >(this, input).steal_data(value);
}

void Node::set(const SocketType& input, array<float2>& value)
{
	assert(input.type == SocketType::FLOAT_ARRAY);
	get_socket_value<array<float2> >(this, input).steal_data(value);
}

void Node::set(const SocketType& input, array<float3>& value)
{
	assert(is_socket_array_float3(input));
	get_socket_value<array<float3> >(this, input).steal_data(value);
}

void Node::set(const SocketType& input, array<ustring>& value)
{
	assert(input.type == SocketType::STRING_ARRAY);
	get_socket_value<array<ustring> >(this, input).steal_data(value);
}

void Node::set(const SocketType& input, array<Transform>& value)
{
	assert(input.type == SocketType::TRANSFORM_ARRAY);
	get_socket_value<array<Transform> >(this, input).steal_data(value);
}

void Node::set(const SocketType& input, array<Node*>& value)
{
	assert(input.type == SocketType::TRANSFORM_ARRAY);
	get_socket_value<array<Node*> >(this, input).steal_data(value);
}

/* get values */
bool Node::get_bool(const SocketType& input) const
{
	assert(input.type == SocketType::BOOLEAN);
	return get_socket_value<bool>(this, input);
}

int Node::get_int(const SocketType& input) const
{
	assert(input.type == SocketType::INT || input.type == SocketType::ENUM);
	return get_socket_value<int>(this, input);
}

uint Node::get_uint(const SocketType& input) const
{
	assert(input.type == SocketType::UINT);
	return get_socket_value<uint>(this, input);
}

float Node::get_float(const SocketType& input) const
{
	assert(input.type == SocketType::FLOAT);
	return get_socket_value<float>(this, input);
}

float2 Node::get_float2(const SocketType& input) const
{
	assert(input.type == SocketType::FLOAT);
	return get_socket_value<float2>(this, input);
}

float3 Node::get_float3(const SocketType& input) const
{
	assert(is_socket_float3(input));
	return get_socket_value<float3>(this, input);
}

ustring Node::get_string(const SocketType& input) const
{
	if(input.type == SocketType::STRING) {
		return get_socket_value<ustring>(this, input);
	}
	else if(input.type == SocketType::ENUM) {
		const NodeEnum& enm = *input.enum_values;
		int intvalue = get_socket_value<int>(this, input);
		return (enm.exists(intvalue)) ? enm[intvalue] : ustring();
	}
	else {
		assert(0);
		return ustring();
	}
}

Transform Node::get_transform(const SocketType& input) const
{
	assert(input.type == SocketType::TRANSFORM);
	return get_socket_value<Transform>(this, input);
}

Node *Node::get_node(const SocketType& input) const
{
	assert(input.type == SocketType::NODE);
	return get_socket_value<Node*>(this, input);
}

/* get array values */
const array<bool>& Node::get_bool_array(const SocketType& input) const
{
	assert(input.type == SocketType::BOOLEAN_ARRAY);
	return get_socket_value<array<bool> >(this, input);
}

const array<int>& Node::get_int_array(const SocketType& input) const
{
	assert(input.type == SocketType::INT_ARRAY);
	return get_socket_value<array<int> >(this, input);
}

const array<float>& Node::get_float_array(const SocketType& input) const
{
	assert(input.type == SocketType::FLOAT_ARRAY);
	return get_socket_value<array<float> >(this, input);
}

const array<float2>& Node::get_float2_array(const SocketType& input) const
{
	assert(input.type == SocketType::FLOAT_ARRAY);
	return get_socket_value<array<float2> >(this, input);
}

const array<float3>& Node::get_float3_array(const SocketType& input) const
{
	assert(is_socket_array_float3(input));
	return get_socket_value<array<float3> >(this, input);
}

const array<ustring>& Node::get_string_array(const SocketType& input) const
{
	assert(input.type == SocketType::STRING_ARRAY);
	return get_socket_value<array<ustring> >(this, input);
}

const array<Transform>& Node::get_transform_array(const SocketType& input) const
{
	assert(input.type == SocketType::TRANSFORM_ARRAY);
	return get_socket_value<array<Transform> >(this, input);
}

const array<Node*>& Node::get_node_array(const SocketType& input) const
{
	assert(input.type == SocketType::NODE_ARRAY);
	return get_socket_value<array<Node*> >(this, input);
}

/* generic value operations */

bool Node::has_default_value(const SocketType& input) const
{
	const void *src = input.default_value;
	void *dst = &get_socket_value<char>(this, input);
	return memcmp(dst, src, input.size()) == 0;
}

void Node::set_default_value(const SocketType& socket)
{
	const void *src = socket.default_value;
	void *dst = ((char*)this) + socket.struct_offset;
	memcpy(dst, src, socket.size());
}

template<typename T>
static void copy_array(const Node *node, const SocketType& socket, const Node *other, const SocketType& other_socket)
{
	const array<T>* src = (const array<T>*)(((char*)other) + other_socket.struct_offset);
	array<T>* dst = (array<T>*)(((char*)node) + socket.struct_offset);
	*dst = *src;
}

void Node::copy_value(const SocketType& socket, const Node& other, const SocketType& other_socket)
{
	assert(socket.type == other_socket.type);

	if(socket.is_array()) {
		switch(socket.type) {
			case SocketType::BOOLEAN_ARRAY: copy_array<bool>(this, socket, &other, other_socket); break;
			case SocketType::FLOAT_ARRAY: copy_array<float>(this, socket, &other, other_socket); break;
			case SocketType::INT_ARRAY: copy_array<int>(this, socket, &other, other_socket); break;
			case SocketType::COLOR_ARRAY: copy_array<float3>(this, socket, &other, other_socket); break;
			case SocketType::VECTOR_ARRAY: copy_array<float3>(this, socket, &other, other_socket); break;
			case SocketType::POINT_ARRAY: copy_array<float3>(this, socket, &other, other_socket); break;
			case SocketType::NORMAL_ARRAY: copy_array<float3>(this, socket, &other, other_socket); break;
			case SocketType::POINT2_ARRAY: copy_array<float2>(this, socket, &other, other_socket); break;
			case SocketType::STRING_ARRAY: copy_array<ustring>(this, socket, &other, other_socket); break;
			case SocketType::TRANSFORM_ARRAY: copy_array<Transform>(this, socket, &other, other_socket); break;
			case SocketType::NODE_ARRAY: copy_array<void*>(this, socket, &other, other_socket); break;
			default: assert(0); break;
		}
	}
	else {
		const void *src = ((char*)&other) + other_socket.struct_offset;
		void *dst = ((char*)this) + socket.struct_offset;
		memcpy(dst, src, socket.size());
	}
}

template<typename T>
static bool is_array_equal(const Node *node, const Node *other, const SocketType& socket)
{
	const array<T>* a = (const array<T>*)(((char*)node) + socket.struct_offset);
	const array<T>* b = (const array<T>*)(((char*)other) + socket.struct_offset);
	return *a == *b;
}

template<typename T>
static bool is_value_equal(const Node *node, const Node *other, const SocketType& socket)
{
	const T *a = (const T*)(((char*)node) + socket.struct_offset);
	const T *b = (const T*)(((char*)other) + socket.struct_offset);
	return *a == *b;
}

bool Node::equals_value(const Node& other, const SocketType& socket) const
{
	switch(socket.type) {
		case SocketType::BOOLEAN: return is_value_equal<bool>(this, &other, socket);
		case SocketType::FLOAT: return is_value_equal<float>(this, &other, socket);
		case SocketType::INT: return is_value_equal<int>(this, &other, socket);
		case SocketType::UINT: return is_value_equal<uint>(this, &other, socket);
		case SocketType::COLOR: return is_value_equal<float3>(this, &other, socket);
		case SocketType::VECTOR: return is_value_equal<float3>(this, &other, socket);
		case SocketType::POINT: return is_value_equal<float3>(this, &other, socket);
		case SocketType::NORMAL: return is_value_equal<float3>(this, &other, socket);
		case SocketType::POINT2: return is_value_equal<float2>(this, &other, socket);
		case SocketType::CLOSURE: return true;
		case SocketType::STRING: return is_value_equal<ustring>(this, &other, socket);
		case SocketType::ENUM: return is_value_equal<int>(this, &other, socket);
		case SocketType::TRANSFORM: return is_value_equal<Transform>(this, &other, socket);
		case SocketType::NODE: return is_value_equal<void*>(this, &other, socket);

		case SocketType::BOOLEAN_ARRAY: return is_array_equal<bool>(this, &other, socket);
		case SocketType::FLOAT_ARRAY: return is_array_equal<float>(this, &other, socket);
		case SocketType::INT_ARRAY: return is_array_equal<int>(this, &other, socket);
		case SocketType::COLOR_ARRAY: return is_array_equal<float3>(this, &other, socket);
		case SocketType::VECTOR_ARRAY: return is_array_equal<float3>(this, &other, socket);
		case SocketType::POINT_ARRAY: return is_array_equal<float3>(this, &other, socket);
		case SocketType::NORMAL_ARRAY: return is_array_equal<float3>(this, &other, socket);
		case SocketType::POINT2_ARRAY: return is_array_equal<float2>(this, &other, socket);
		case SocketType::STRING_ARRAY: return is_array_equal<ustring>(this, &other, socket);
		case SocketType::TRANSFORM_ARRAY: return is_array_equal<Transform>(this, &other, socket);
		case SocketType::NODE_ARRAY: return is_array_equal<void*>(this, &other, socket);

		case SocketType::UNDEFINED: return true;
	}

	return true;
}

/* equals */

bool Node::equals(const Node& other) const
{
	assert(type == other.type);

	foreach(const SocketType& socket, type->inputs) {
		if(!equals_value(other, socket))
			return false;
	}

	return true;
}

/* Hash */

namespace {

template<typename T>
void value_hash(const Node *node, const SocketType& socket, MD5Hash& md5)
{
	md5.append(((uint8_t*)node) + socket.struct_offset, socket.size());
}

void float3_hash(const Node *node, const SocketType& socket, MD5Hash& md5)
{
	/* Don't compare 4th element used for padding. */
	md5.append(((uint8_t*)node) + socket.struct_offset, sizeof(float) * 3);
}

template<typename T>
void array_hash(const Node *node, const SocketType& socket, MD5Hash& md5)
{
	const array<T>& a = *(const array<T>*)(((char*)node) + socket.struct_offset);
	for (size_t i = 0; i < a.size(); i++) {
		md5.append((uint8_t*)&a[i], sizeof(T));
	}
}

void float3_array_hash(const Node *node, const SocketType& socket, MD5Hash& md5)
{
	/* Don't compare 4th element used for padding. */
	const array<float3>& a = *(const array<float3>*)(((char*)node) + socket.struct_offset);
	for (size_t i = 0; i < a.size(); i++) {
		md5.append((uint8_t*)&a[i], sizeof(float) * 3);
	}
}

}  // namespace

void Node::hash(MD5Hash& md5)
{
	md5.append(type->name.string());

	foreach(const SocketType& socket, type->inputs) {
		md5.append(socket.name.string());

		switch(socket.type) {
			case SocketType::BOOLEAN: value_hash<bool>(this, socket, md5); break;
			case SocketType::FLOAT: value_hash<float>(this, socket, md5); break;
			case SocketType::INT: value_hash<int>(this, socket, md5); break;
			case SocketType::UINT: value_hash<uint>(this, socket, md5); break;
			case SocketType::COLOR: float3_hash(this, socket, md5); break;
			case SocketType::VECTOR: float3_hash(this, socket, md5); break;
			case SocketType::POINT: float3_hash(this, socket, md5); break;
			case SocketType::NORMAL: float3_hash(this, socket, md5); break;
			case SocketType::POINT2: value_hash<float2>(this, socket, md5); break;
			case SocketType::CLOSURE: break;
			case SocketType::STRING: value_hash<ustring>(this, socket, md5); break;
			case SocketType::ENUM: value_hash<int>(this, socket, md5); break;
			case SocketType::TRANSFORM: value_hash<Transform>(this, socket, md5); break;
			case SocketType::NODE: value_hash<void*>(this, socket, md5); break;

			case SocketType::BOOLEAN_ARRAY: array_hash<bool>(this, socket, md5); break;
			case SocketType::FLOAT_ARRAY: array_hash<float>(this, socket, md5); break;
			case SocketType::INT_ARRAY: array_hash<int>(this, socket, md5); break;
			case SocketType::COLOR_ARRAY: float3_array_hash(this, socket, md5); break;
			case SocketType::VECTOR_ARRAY: float3_array_hash(this, socket, md5); break;
			case SocketType::POINT_ARRAY: float3_array_hash(this, socket, md5); break;
			case SocketType::NORMAL_ARRAY: float3_array_hash(this, socket, md5); break;
			case SocketType::POINT2_ARRAY: array_hash<float2>(this, socket, md5); break;
			case SocketType::STRING_ARRAY: array_hash<ustring>(this, socket, md5); break;
			case SocketType::TRANSFORM_ARRAY: array_hash<Transform>(this, socket, md5); break;
			case SocketType::NODE_ARRAY: array_hash<void*>(this, socket, md5); break;

			case SocketType::UNDEFINED: break;
		}
	}
}

namespace {

template<typename T>
size_t array_size_in_bytes(const Node *node, const SocketType& socket)
{
	const array<T>& a = *(const array<T>*)(((char*)node) + socket.struct_offset);
	return a.size() * sizeof(T);
}

}  // namespace

size_t Node::get_total_size_in_bytes() const
{
	size_t total_size = 0;
	foreach(const SocketType& socket, type->inputs) {
		switch(socket.type) {
			case SocketType::BOOLEAN:
			case SocketType::FLOAT:
			case SocketType::INT:
			case SocketType::UINT:
			case SocketType::COLOR:
			case SocketType::VECTOR:
			case SocketType::POINT:
			case SocketType::NORMAL:
			case SocketType::POINT2:
			case SocketType::CLOSURE:
			case SocketType::STRING:
			case SocketType::ENUM:
			case SocketType::TRANSFORM:
			case SocketType::NODE:
				total_size += socket.size();
				break;

			case SocketType::BOOLEAN_ARRAY:
				total_size += array_size_in_bytes<bool>(this, socket);
				break;
			case SocketType::FLOAT_ARRAY:
				total_size += array_size_in_bytes<float>(this, socket);
				break;
			case SocketType::INT_ARRAY:
				total_size += array_size_in_bytes<int>(this, socket);
				break;
			case SocketType::COLOR_ARRAY:
				total_size += array_size_in_bytes<float3>(this, socket);
				break;
			case SocketType::VECTOR_ARRAY:
				total_size += array_size_in_bytes<float3>(this, socket);
				break;
			case SocketType::POINT_ARRAY:
				total_size += array_size_in_bytes<float3>(this, socket);
				break;
			case SocketType::NORMAL_ARRAY:
				total_size += array_size_in_bytes<float3>(this, socket);
				break;
			case SocketType::POINT2_ARRAY:
				total_size += array_size_in_bytes<float2>(this, socket);
				break;
			case SocketType::STRING_ARRAY:
				total_size += array_size_in_bytes<ustring>(this, socket);
				break;
			case SocketType::TRANSFORM_ARRAY:
				total_size += array_size_in_bytes<Transform>(this, socket);
				break;
			case SocketType::NODE_ARRAY:
				total_size += array_size_in_bytes<void*>(this, socket);
				break;

			case SocketType::UNDEFINED: break;
		}
	}
	return total_size;
}

CCL_NAMESPACE_END
