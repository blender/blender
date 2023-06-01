/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include <type_traits>

#include "graph/node_type.h"

#include "util/array.h"
#include "util/map.h"
#include "util/param.h"

CCL_NAMESPACE_BEGIN

class MD5Hash;
struct Node;
struct NodeType;
struct Transform;

/* NOTE: in the following macros we use "type const &" instead of "const type &"
 * to avoid issues when pasting a pointer type. */
#define NODE_SOCKET_API_BASE_METHODS(type_, name, string_name) \
  const SocketType *get_##name##_socket() const \
  { \
    /* Explicitly cast to base class to use `Node::type` even if the derived class defines \
     * `type`. */ \
    const Node *self_node = this; \
    static const SocketType *socket = self_node->type->find_input(ustring(string_name)); \
    return socket; \
  } \
  bool name##_is_modified() const \
  { \
    const SocketType *socket = get_##name##_socket(); \
    return socket_is_modified(*socket); \
  } \
  void tag_##name##_modified() \
  { \
    const SocketType *socket = get_##name##_socket(); \
    socket_modified |= socket->modified_flag_bit; \
  } \
  type_ const &get_##name() const \
  { \
    const SocketType *socket = get_##name##_socket(); \
    return get_socket_value<type_>(this, *socket); \
  }

#define NODE_SOCKET_API_BASE(type_, name, string_name) \
 protected: \
  type_ name; \
\
 public: \
  NODE_SOCKET_API_BASE_METHODS(type_, name, string_name)

#define NODE_SOCKET_API(type_, name) \
  NODE_SOCKET_API_BASE(type_, name, #name) \
  void set_##name(type_ value) \
  { \
    const SocketType *socket = get_##name##_socket(); \
    this->set(*socket, value); \
  }

#define NODE_SOCKET_API_ARRAY(type_, name) \
  NODE_SOCKET_API_BASE(type_, name, #name) \
  void set_##name(type_ &value) \
  { \
    const SocketType *socket = get_##name##_socket(); \
    this->set(*socket, value); \
  } \
  type_ &get_##name() \
  { \
    const SocketType *socket = get_##name##_socket(); \
    return get_socket_value<type_>(this, *socket); \
  }

#define NODE_SOCKET_API_STRUCT_MEMBER(type_, name, member) \
  NODE_SOCKET_API_BASE_METHODS(type_, name##_##member, #name "." #member) \
  void set_##name##_##member(type_ value) \
  { \
    const SocketType *socket = get_##name##_##member##_socket(); \
    this->set(*socket, value); \
  }

/* Node */

struct NodeOwner {
  virtual ~NodeOwner();
};

struct Node {
  explicit Node(const NodeType *type, ustring name = ustring());
  virtual ~Node() = 0;

  /* set values */
  void set(const SocketType &input, bool value);
  void set(const SocketType &input, int value);
  void set(const SocketType &input, uint value);
  void set(const SocketType &input, uint64_t value);
  void set(const SocketType &input, float value);
  void set(const SocketType &input, float2 value);
  void set(const SocketType &input, float3 value);
  void set(const SocketType &input, const char *value);
  void set(const SocketType &input, ustring value);
  void set(const SocketType &input, const Transform &value);
  void set(const SocketType &input, Node *value);

  /* Implicitly cast enums and enum classes to integer, which matches an internal way of how
   * enumerator values are stored and accessed in a generic API. */
  template<class ValueType, typename std::enable_if_t<std::is_enum_v<ValueType>> * = nullptr>
  void set(const SocketType &input, const ValueType &value)
  {
    static_assert(sizeof(ValueType) <= sizeof(int), "Enumerator type should fit int");
    set(input, static_cast<int>(value));
  }

  /* set array values. the memory from the input array will taken over
   * by the node and the input array will be empty after return */
  void set(const SocketType &input, array<bool> &value);
  void set(const SocketType &input, array<int> &value);
  void set(const SocketType &input, array<float> &value);
  void set(const SocketType &input, array<float2> &value);
  void set(const SocketType &input, array<float3> &value);
  void set(const SocketType &input, array<ustring> &value);
  void set(const SocketType &input, array<Transform> &value);
  void set(const SocketType &input, array<Node *> &value);

  /* get values */
  bool get_bool(const SocketType &input) const;
  int get_int(const SocketType &input) const;
  uint get_uint(const SocketType &input) const;
  uint64_t get_uint64(const SocketType &input) const;
  float get_float(const SocketType &input) const;
  float2 get_float2(const SocketType &input) const;
  float3 get_float3(const SocketType &input) const;
  ustring get_string(const SocketType &input) const;
  Transform get_transform(const SocketType &input) const;
  Node *get_node(const SocketType &input) const;

  /* get array values */
  const array<bool> &get_bool_array(const SocketType &input) const;
  const array<int> &get_int_array(const SocketType &input) const;
  const array<float> &get_float_array(const SocketType &input) const;
  const array<float2> &get_float2_array(const SocketType &input) const;
  const array<float3> &get_float3_array(const SocketType &input) const;
  const array<ustring> &get_string_array(const SocketType &input) const;
  const array<Transform> &get_transform_array(const SocketType &input) const;
  const array<Node *> &get_node_array(const SocketType &input) const;

  /* generic values operations */
  bool has_default_value(const SocketType &input) const;
  void set_default_value(const SocketType &input);
  bool equals_value(const Node &other, const SocketType &input) const;
  void copy_value(const SocketType &input, const Node &other, const SocketType &other_input);
  void set_value(const SocketType &input, const Node &other, const SocketType &other_input);

  /* equals */
  bool equals(const Node &other) const;

  /* compute hash of node and its socket values */
  void hash(MD5Hash &md5);

  /* Get total size of this node. */
  size_t get_total_size_in_bytes() const;

  /* Type testing, taking into account base classes. */
  bool is_a(const NodeType *type);

  bool socket_is_modified(const SocketType &input) const;

  bool is_modified() const;

  void tag_modified();
  void clear_modified();

  void print_modified_sockets() const;

  ustring name;
  const NodeType *type;

  const NodeOwner *get_owner() const;
  void set_owner(const NodeOwner *owner_);

  int reference_count() const
  {
    return ref_count;
  }

  void reference()
  {
    ref_count += 1;
  }

  void dereference()
  {
    ref_count -= 1;
  }

  /* Set the reference count to zero. This should only be called when we know for sure that the
   * Node is not used by anyone else. For now, this is only the case when "deleting" shaders, as
   * they are never actually deleted. */
  void clear_reference_count()
  {
    ref_count = 0;
  }

 protected:
  const NodeOwner *owner;
  int ref_count{0};

  template<typename T> static T &get_socket_value(const Node *node, const SocketType &socket)
  {
    return (T &)*(((char *)node) + socket.struct_offset);
  }

  SocketModifiedFlags socket_modified;

  template<typename T> void set_if_different(const SocketType &input, T value);

  /* Explicit overload for Node sockets so we can handle reference counting. The old Node is
   * dereferenced, and the new one is referenced. */
  void set_if_different(const SocketType &input, Node *value);

  template<typename T> void set_if_different(const SocketType &input, array<T> &value);

  /* Explicit overload for Node sockets so we can handle reference counting. The old Nodes are
   * dereferenced, and the new ones are referenced. */
  void set_if_different(const SocketType &input, array<Node *> &value);

  /* Call this function in derived classes' destructors to ensure that used Nodes are dereferenced
   * properly. */
  void dereference_all_used_nodes();
};

CCL_NAMESPACE_END
