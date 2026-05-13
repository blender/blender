/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "graph/node_type.h"

#include "util/log.h"
#include "util/transform.h"
#include "util/types_float3.h"

CCL_NAMESPACE_BEGIN

/* Node Socket Type */

size_t SocketType::storage_size() const
{
  return size(type, false);
}

size_t SocketType::packed_size() const
{
  return size(type, true);
}

bool SocketType::is_array() const
{
  return (type >= BOOLEAN_ARRAY);
}

size_t SocketType::size(Type type, bool packed)
{
  switch (type) {
    case UNDEFINED:
    case NUM_TYPES:
      return 0;

    case BOOLEAN:
      return sizeof(bool);
    case FLOAT:
      return sizeof(float);
    case INT:
      return sizeof(int);
    case UINT:
      return sizeof(uint);
    case UINT64:
      return sizeof(uint64_t);
    case COLOR:
    case VECTOR:
    case POINT:
    case NORMAL:
      return (packed) ? sizeof(packed_float3) : sizeof(float3);
    case POINT2:
      return sizeof(float2);
    case CLOSURE:
      return 0;
    case STRING:
      return sizeof(ustring);
    case ENUM:
      return sizeof(int);
    case TRANSFORM:
      return sizeof(Transform);
    case NODE:
      return sizeof(void *);

    case BOOLEAN_ARRAY:
      return sizeof(array<bool>);
    case FLOAT_ARRAY:
      return sizeof(array<float>);
    case INT_ARRAY:
      return sizeof(array<int>);
    case COLOR_ARRAY:
      return sizeof(array<float3>);
    case VECTOR_ARRAY:
      return sizeof(array<float3>);
    case POINT_ARRAY:
      return sizeof(array<float3>);
    case NORMAL_ARRAY:
      return sizeof(array<float3>);
    case POINT2_ARRAY:
      return sizeof(array<float2>);
    case STRING_ARRAY:
      return sizeof(array<ustring>);
    case TRANSFORM_ARRAY:
      return sizeof(array<Transform>);
    case NODE_ARRAY:
      return sizeof(array<void *>);
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
  static Transform zero_transform = transform_zero();
  return &zero_transform;
}

ustring SocketType::type_name(Type type)
{
  static const ustring names[] = {ustring("undefined"),

                                  ustring("boolean"),       ustring("float"),
                                  ustring("int"),           ustring("uint"),
                                  ustring("uint64"),        ustring("color"),
                                  ustring("vector"),        ustring("point"),
                                  ustring("normal"),        ustring("point2"),
                                  ustring("closure"),       ustring("string"),
                                  ustring("enum"),          ustring("transform"),
                                  ustring("node"),

                                  ustring("array_boolean"), ustring("array_float"),
                                  ustring("array_int"),     ustring("array_color"),
                                  ustring("array_vector"),  ustring("array_point"),
                                  ustring("array_normal"),  ustring("array_point2"),
                                  ustring("array_string"),  ustring("array_transform"),
                                  ustring("array_node")};

  constexpr size_t num_names = sizeof(names) / sizeof(*names);
  static_assert(num_names == NUM_TYPES);

  return names[(int)type];
}

bool SocketType::is_float3(Type type)
{
  return (type == COLOR || type == VECTOR || type == POINT || type == NORMAL);
}

/* Node Type */

NodeType::NodeType(Type type, const NodeType *base) : type(type), base(base)
{
  if (base) {
    /* Inherit sockets. */
    inputs = base->inputs;
    outputs = base->outputs;
  }
}

NodeType::~NodeType() = default;

void NodeType::register_input(ustring name,
                              ustring ui_name,
                              SocketType::Type type,
                              const int struct_offset,
                              const void *default_value,
                              const NodeEnum *enum_values,
                              const NodeType *node_type,
                              const int flags,
                              const int extra_flags)
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
  assert(inputs.size() < SocketModifiedFlags().size());
  socket.modified_flag_bit = inputs.size();
  inputs.push_back(socket);
}

void NodeType::register_output(ustring name, ustring ui_name, SocketType::Type type)
{
  SocketType socket;
  socket.name = name;
  socket.ui_name = ui_name;
  socket.type = type;
  socket.struct_offset = 0;
  socket.default_value = nullptr;
  socket.enum_values = nullptr;
  socket.node_type = nullptr;
  socket.flags = SocketType::LINKABLE;
  outputs.push_back(socket);
}

const SocketType *NodeType::find_input(ustring name) const
{
  for (const SocketType &socket : inputs) {
    if (socket.name == name) {
      return &socket;
    }
  }

  return nullptr;
}

const SocketType *NodeType::find_output(ustring name) const
{
  for (const SocketType &socket : outputs) {
    if (socket.name == name) {
      return &socket;
    }
  }

  return nullptr;
}

/* Node Type Registry
 *
 * We want to be able to find and enumerate types without the need for a centralized
 * function for all node types. Due to static initialization order issues and guarded
 * allocators, there are some subtle implementation choices.
 *
 * Only the init functions are gathered on static initialization, as registering the
 * node type itself could otherwise happen in the wrong order for base and derived
 * types, and Blender's guarded allocator may not have been initialized yet.
 *
 * The initialization functions are stored in std::vector without guarded allocation
 * as that may not been properly initialized yet. */

static thread_mutex &types_mutex()
{
  static thread_mutex types_mutex_;
  return types_mutex_;
}

static unordered_map<ustring, NodeType> &types()
{
  static unordered_map<ustring, NodeType> _types;
  return _types;
}

static thread_mutex &types_on_init_mutex()
{
  static thread_mutex types_on_init_mutex_;
  return types_on_init_mutex_;
}

static std::vector<const NodeType *(*)()> &types_on_init()
{
  static std::vector<const NodeType *(*)()> _types_on_init;
  return _types_on_init;
}

static void ensure_types_initialized()
{
  thread_scoped_lock lock(types_on_init_mutex());
  for (const auto &init_func : types_on_init()) {
    init_func();
  }
  types_on_init().clear();
}

bool NodeType::register_on_init(const NodeType *(*init_func)())
{
  thread_scoped_lock lock(types_on_init_mutex());
  types_on_init().push_back(init_func);
  return true;
}

NodeType *NodeType::add(const char *name_, CreateFunc create_, Type type_, const NodeType *base_)
{
  const ustring name(name_);

  /* Types can be lazily registered from multiple threads. */
  thread_scoped_lock lock(types_mutex());

  if (types().contains(name)) {
    LOG_ERROR << "Node type " << name_ << " registered twice";
    assert(0);
    return nullptr;
  }

  types()[name] = NodeType(type_, base_);

  NodeType *type = &types()[name];
  type->name = name;
  type->create = create_;
  return type;
}

const NodeType *NodeType::find(ustring name)
{
  ensure_types_initialized();

  thread_scoped_lock lock(types_mutex());
  const unordered_map<ustring, NodeType>::iterator it = types().find(name);
  return (it == types().end()) ? nullptr : &it->second;
}

vector<ustring> NodeType::type_names()
{
  ensure_types_initialized();

  thread_scoped_lock lock(types_mutex());
  vector<ustring> names;
  for (const auto &pair : types()) {
    names.push_back(pair.first);
  }

  return names;
}

CCL_NAMESPACE_END
