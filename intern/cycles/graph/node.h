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

#pragma once

#include "graph/node_type.h"

#include "util/util_map.h"
#include "util/util_param.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class MD5Hash;
struct Node;
struct NodeType;
struct Transform;

/* Node */

struct Node
{
	explicit Node(const NodeType *type, ustring name = ustring());
	virtual ~Node();

	/* set values */
	void set(const SocketType& input, bool value);
	void set(const SocketType& input, int value);
	void set(const SocketType& input, uint value);
	void set(const SocketType& input, float value);
	void set(const SocketType& input, float2 value);
	void set(const SocketType& input, float3 value);
	void set(const SocketType& input, const char *value);
	void set(const SocketType& input, ustring value);
	void set(const SocketType& input, const Transform& value);
	void set(const SocketType& input, Node *value);

	/* set array values. the memory from the input array will taken over
	 * by the node and the input array will be empty after return */
	void set(const SocketType& input, array<bool>& value);
	void set(const SocketType& input, array<int>& value);
	void set(const SocketType& input, array<float>& value);
	void set(const SocketType& input, array<float2>& value);
	void set(const SocketType& input, array<float3>& value);
	void set(const SocketType& input, array<ustring>& value);
	void set(const SocketType& input, array<Transform>& value);
	void set(const SocketType& input, array<Node*>& value);

	/* get values */
	bool get_bool(const SocketType& input) const;
	int get_int(const SocketType& input) const;
	uint get_uint(const SocketType& input) const;
	float get_float(const SocketType& input) const;
	float2 get_float2(const SocketType& input) const;
	float3 get_float3(const SocketType& input) const;
	ustring get_string(const SocketType& input) const;
	Transform get_transform(const SocketType& input) const;
	Node *get_node(const SocketType& input) const;

	/* get array values */
	const array<bool>& get_bool_array(const SocketType& input) const;
	const array<int>& get_int_array(const SocketType& input) const;
	const array<float>& get_float_array(const SocketType& input) const;
	const array<float2>& get_float2_array(const SocketType& input) const;
	const array<float3>& get_float3_array(const SocketType& input) const;
	const array<ustring>& get_string_array(const SocketType& input) const;
	const array<Transform>& get_transform_array(const SocketType& input) const;
	const array<Node*>& get_node_array(const SocketType& input) const;

	/* generic values operations */
	bool has_default_value(const SocketType& input) const;
	void set_default_value(const SocketType& input);
	bool equals_value(const Node& other, const SocketType& input) const;
	void copy_value(const SocketType& input, const Node& other, const SocketType& other_input);

	/* equals */
	bool equals(const Node& other) const;

	/* compute hash of node and its socket values */
	void hash(MD5Hash& md5);

	/* Get total size of this node. */
	size_t get_total_size_in_bytes() const;

	ustring name;
	const NodeType *type;
};

CCL_NAMESPACE_END
