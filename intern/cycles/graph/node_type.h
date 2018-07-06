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

#include "graph/node_enum.h"

#include "util/util_map.h"
#include "util/util_param.h"
#include "util/util_string.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

struct Node;
struct NodeType;

/* Socket Type */

struct SocketType
{
	enum Type
	{
		UNDEFINED,

		BOOLEAN,
		FLOAT,
		INT,
		UINT,
		COLOR,
		VECTOR,
		POINT,
		NORMAL,
		POINT2,
		CLOSURE,
		STRING,
		ENUM,
		TRANSFORM,
		NODE,

		BOOLEAN_ARRAY,
		FLOAT_ARRAY,
		INT_ARRAY,
		COLOR_ARRAY,
		VECTOR_ARRAY,
		POINT_ARRAY,
		NORMAL_ARRAY,
		POINT2_ARRAY,
		STRING_ARRAY,
		TRANSFORM_ARRAY,
		NODE_ARRAY,
	};

	enum Flags {
		LINKABLE               = (1 << 0),
		ANIMATABLE             = (1 << 1),

		SVM_INTERNAL           = (1 << 2),
		OSL_INTERNAL           = (1 << 3),
		INTERNAL               = (1 << 2) | (1 << 3),

		LINK_TEXTURE_GENERATED = (1 << 4),
		LINK_TEXTURE_NORMAL    = (1 << 5),
		LINK_TEXTURE_UV        = (1 << 6),
		LINK_INCOMING          = (1 << 7),
		LINK_NORMAL            = (1 << 8),
		LINK_POSITION          = (1 << 9),
		LINK_TANGENT           = (1 << 10),
		DEFAULT_LINK_MASK      = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10)
	};

	ustring name;
	Type type;
	int struct_offset;
	const void *default_value;
	const NodeEnum *enum_values;
	const NodeType **node_type;
	int flags;
	ustring ui_name;

	size_t size() const;
	bool is_array() const;
	static size_t size(Type type);
	static size_t max_size();
	static ustring type_name(Type type);
	static void *zero_default_value();
	static bool is_float3(Type type);
};

/* Node Type */

struct NodeType
{
	enum Type {
		NONE,
		SHADER
	};

	explicit NodeType(Type type = NONE);
	~NodeType();

	void register_input(ustring name, ustring ui_name, SocketType::Type type,
	                    int struct_offset, const void *default_value,
						const NodeEnum *enum_values = NULL,
						const NodeType **node_type = NULL,
						int flags = 0, int extra_flags = 0);
	void register_output(ustring name, ustring ui_name, SocketType::Type type);

	const SocketType *find_input(ustring name) const;
	const SocketType *find_output(ustring name) const;

	typedef Node *(*CreateFunc)(const NodeType *type);

	ustring name;
	Type type;
	vector<SocketType, std::allocator<SocketType> > inputs;
	vector<SocketType, std::allocator<SocketType> > outputs;
	CreateFunc create;

	static NodeType *add(const char *name, CreateFunc create, Type type = NONE);
	static const NodeType *find(ustring name);
	static unordered_map<ustring, NodeType, ustringHash>& types();
};

/* Node Definition Macros */

#define NODE_DECLARE                       \
template<typename T>                       \
static const NodeType *register_type();    \
static Node *create(const NodeType *type); \
static const NodeType *node_type;

#define NODE_DEFINE(structname)                                                  \
const NodeType *structname::node_type = structname::register_type<structname>(); \
Node *structname::create(const NodeType*) { return new structname(); }           \
template<typename T>                                                             \
const NodeType *structname::register_type()

/* Sock Definition Macros */

#define SOCKET_OFFSETOF(T, name) (((char *)&(((T *)1)->name)) - (char *)1)
#define SOCKET_SIZEOF(T, name) (sizeof(((T *)1)->name))
#define SOCKET_DEFINE(name, ui_name, default_value, datatype, TYPE, flags, ...) \
	{ \
		static datatype defval = default_value; \
		CHECK_TYPE(((T *)1)->name, datatype); \
		type->register_input(ustring(#name), ustring(ui_name), TYPE, SOCKET_OFFSETOF(T, name), &defval, NULL, NULL, flags, ##__VA_ARGS__); \
	}

#define SOCKET_BOOLEAN(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, bool, SocketType::BOOLEAN, 0, ##__VA_ARGS__)
#define SOCKET_INT(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, int, SocketType::INT, 0, ##__VA_ARGS__)
#define SOCKET_UINT(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, uint, SocketType::UINT, 0, ##__VA_ARGS__)
#define SOCKET_FLOAT(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float, SocketType::FLOAT, 0, ##__VA_ARGS__)
#define SOCKET_COLOR(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::COLOR, 0, ##__VA_ARGS__)
#define SOCKET_VECTOR(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::VECTOR, 0, ##__VA_ARGS__)
#define SOCKET_POINT(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::POINT, 0, ##__VA_ARGS__)
#define SOCKET_NORMAL(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::NORMAL, 0, ##__VA_ARGS__)
#define SOCKET_POINT2(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float2, SocketType::POINT2, 0, ##__VA_ARGS__)
#define SOCKET_STRING(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, ustring, SocketType::STRING, 0, ##__VA_ARGS__)
#define SOCKET_TRANSFORM(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, Transform, SocketType::TRANSFORM, 0, ##__VA_ARGS__)
#define SOCKET_ENUM(name, ui_name, values, default_value, ...) \
	{ \
		static int defval = default_value; \
		assert(SOCKET_SIZEOF(T, name) == sizeof(int)); \
		type->register_input(ustring(#name), ustring(ui_name), SocketType::ENUM, SOCKET_OFFSETOF(T, name), &defval, &values, NULL, ##__VA_ARGS__); \
	}
#define SOCKET_NODE(name, ui_name, node_type, ...) \
	{ \
	    static Node *defval = NULL; \
		assert(SOCKET_SIZEOF(T, name) == sizeof(Node*)); \
		type->register_input(ustring(#name), ustring(ui_name), SocketType::NODE, SOCKET_OFFSETOF(T, name), &defval, NULL, node_type, ##__VA_ARGS__); \
	}

#define SOCKET_BOOLEAN_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<bool>, SocketType::BOOLEAN_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_INT_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<int>, SocketType::INT_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_FLOAT_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<float>, SocketType::FLOAT_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_COLOR_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<float3>, SocketType::COLOR_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_VECTOR_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<float3>, SocketType::VECTOR_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_POINT_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<float3>, SocketType::POINT_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_NORMAL_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<float3>, SocketType::NORMAL_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_POINT2_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<float2>, SocketType::POINT2_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_STRING_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<ustring>, SocketType::STRING_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_TRANSFORM_ARRAY(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, array<Transform>, SocketType::TRANSFORM_ARRAY, 0, ##__VA_ARGS__)
#define SOCKET_NODE_ARRAY(name, ui_name, node_type, ...) \
	{ \
	    static Node *defval = NULL; \
		assert(SOCKET_SIZEOF(T, name) == sizeof(Node*)); \
		type->register_input(ustring(#name), ustring(ui_name), SocketType::NODE_ARRAY, SOCKET_OFFSETOF(T, name), &defval, NULL, node_type, ##__VA_ARGS__); \
	}

#define SOCKET_IN_BOOLEAN(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, bool, SocketType::BOOLEAN, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_INT(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, int, SocketType::INT, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_FLOAT(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float, SocketType::FLOAT, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_COLOR(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::COLOR, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_VECTOR(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::VECTOR, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_POINT(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::POINT, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_NORMAL(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, float3, SocketType::NORMAL, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_STRING(name, ui_name, default_value, ...) \
	SOCKET_DEFINE(name, ui_name, default_value, ustring, SocketType::STRING, SocketType::LINKABLE, ##__VA_ARGS__)
#define SOCKET_IN_CLOSURE(name, ui_name, ...) \
	type->register_input(ustring(#name), ustring(ui_name), SocketType::CLOSURE, 0, NULL, NULL, NULL, SocketType::LINKABLE, ##__VA_ARGS__)

#define SOCKET_OUT_BOOLEAN(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::BOOLEAN); }
#define SOCKET_OUT_INT(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::INT); }
#define SOCKET_OUT_FLOAT(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::FLOAT); }
#define SOCKET_OUT_COLOR(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::COLOR); }
#define SOCKET_OUT_VECTOR(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::VECTOR); }
#define SOCKET_OUT_POINT(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::POINT); }
#define SOCKET_OUT_NORMAL(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::NORMAL); }
#define SOCKET_OUT_CLOSURE(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::CLOSURE); }
#define SOCKET_OUT_STRING(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::STRING); }
#define SOCKET_OUT_ENUM(name, ui_name) \
	{ type->register_output(ustring(#name), ustring(ui_name), SocketType::ENUM); }

CCL_NAMESPACE_END
