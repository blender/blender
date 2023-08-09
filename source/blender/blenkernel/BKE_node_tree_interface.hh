/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"

#include "BKE_node.h"

#include <queue>
#include <type_traits>

#include "BLI_parameter_pack_utils.hh"
#include "BLI_vector.hh"

namespace blender::bke {

/* Runtime topology cache for linear access to items. */
struct bNodeTreeInterfaceCache {
  Vector<bNodeTreeInterfaceItem *> items;
  Vector<bNodeTreeInterfaceSocket *> inputs;
  Vector<bNodeTreeInterfaceSocket *> outputs;

  void rebuild(bNodeTreeInterface &tree_interface);
};

namespace node_interface {

namespace detail {

template<typename T> static bool item_is_type(const bNodeTreeInterfaceItem &item)
{
  bool match = false;
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      match |= std::is_same<T, bNodeTreeInterfaceSocket>::value;
      break;
    }
    case NODE_INTERFACE_PANEL: {
      match |= std::is_same<T, bNodeTreeInterfacePanel>::value;
      break;
    }
  }
  return match;
}

}  // namespace detail

template<typename T> T &get_item_as(bNodeTreeInterfaceItem &item)
{
  BLI_assert(detail::item_is_type<T>(item));
  return reinterpret_cast<T &>(item);
}

template<typename T> const T &get_item_as(const bNodeTreeInterfaceItem &item)
{
  BLI_assert(detail::item_is_type<T>(item));
  return reinterpret_cast<const T &>(item);
}

template<typename T> T *get_item_as(bNodeTreeInterfaceItem *item)
{
  if (item && detail::item_is_type<T>(*item)) {
    return reinterpret_cast<T *>(item);
  }
  return nullptr;
}

template<typename T> const T *get_item_as(const bNodeTreeInterfaceItem *item)
{
  if (item && detail::item_is_type<T>(*item)) {
    return reinterpret_cast<const T *>(item);
  }
  return nullptr;
}

namespace socket_types {

constexpr const char *node_socket_data_float = "NodeSocketFloat";
constexpr const char *node_socket_data_int = "NodeSocketInt";
constexpr const char *node_socket_data_bool = "NodeSocketBool";
constexpr const char *node_socket_data_rotation = "NodeSocketRotation";
constexpr const char *node_socket_data_vector = "NodeSocketVector";
constexpr const char *node_socket_data_color = "NodeSocketColor";
constexpr const char *node_socket_data_string = "NodeSocketString";
constexpr const char *node_socket_data_object = "NodeSocketObject";
constexpr const char *node_socket_data_image = "NodeSocketImage";
constexpr const char *node_socket_data_collection = "NodeSocketCollection";
constexpr const char *node_socket_data_texture = "NodeSocketTexture";
constexpr const char *node_socket_data_material = "NodeSocketMaterial";

template<typename Fn> void socket_data_to_static_type(const char *socket_type, const Fn &fn)
{
  if (STREQ(socket_type, socket_types::node_socket_data_float)) {
    fn.template operator()<bNodeSocketValueFloat>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_int)) {
    fn.template operator()<bNodeSocketValueInt>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_bool)) {
    fn.template operator()<bNodeSocketValueBoolean>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_rotation)) {
    fn.template operator()<bNodeSocketValueRotation>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_vector)) {
    fn.template operator()<bNodeSocketValueVector>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_color)) {
    fn.template operator()<bNodeSocketValueRGBA>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_string)) {
    fn.template operator()<bNodeSocketValueString>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_object)) {
    fn.template operator()<bNodeSocketValueObject>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_image)) {
    fn.template operator()<bNodeSocketValueImage>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_collection)) {
    fn.template operator()<bNodeSocketValueCollection>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_texture)) {
    fn.template operator()<bNodeSocketValueTexture>();
  }
  else if (STREQ(socket_type, socket_types::node_socket_data_material)) {
    fn.template operator()<bNodeSocketValueMaterial>();
  }
}

namespace detail {

template<typename Fn> struct TypeTagExecutor {
  const Fn &fn;

  TypeTagExecutor(const Fn &fn_) : fn(fn_) {}

  template<typename T> void operator()() const
  {
    fn(TypeTag<T>{});
  }
};

}  // namespace detail

template<typename Fn> void socket_data_to_static_type_tag(const char *socket_type, const Fn &fn)
{
  detail::TypeTagExecutor executor{fn};
  socket_data_to_static_type(socket_type, executor);
}

}  // namespace socket_types

template<typename T> bool socket_data_is_type(const char *socket_type)
{
  bool match = false;
  socket_types::socket_data_to_static_type_tag(socket_type, [&match](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    match |= std::is_same_v<T, SocketDataType>;
  });
  return match;
}

template<typename T> T &get_socket_data_as(bNodeTreeInterfaceSocket &item)
{
  BLI_assert(socket_data_is_type<T>(item.socket_type));
  return *static_cast<T *>(item.socket_data);
}

template<typename T> const T &get_socket_data_as(const bNodeTreeInterfaceSocket &item)
{
  BLI_assert(socket_data_is_type<T>(item.socket_type));
  return *static_cast<const T *>(item.socket_data);
}

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode & /*from_node*/,
                                                                const bNodeSocket &from_sock,
                                                                const StringRefNull socket_type,
                                                                const StringRefNull name)
{
  eNodeTreeInterfaceSocketFlag flag = eNodeTreeInterfaceSocketFlag(0);
  SET_FLAG_FROM_TEST(flag, from_sock.in_out & SOCK_IN, NODE_INTERFACE_SOCKET_INPUT);
  SET_FLAG_FROM_TEST(flag, from_sock.in_out & SOCK_OUT, NODE_INTERFACE_SOCKET_OUTPUT);

  bNodeTreeInterfaceSocket *iosock = ntree.tree_interface.add_socket(
      name.data(), from_sock.description, socket_type, flag, nullptr);
  if (iosock == nullptr) {
    return nullptr;
  }
  const bNodeSocketType *typeinfo = iosock->socket_typeinfo();
  if (typeinfo->interface_from_socket) {
    /* XXX Enable when bNodeSocketType callbacks have been updated. */
    UNUSED_VARS(from_sock);
    //    typeinfo->interface_from_socket(ntree.id, iosock, &from_node, &from_sock);
  }
  return iosock;
}

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode &from_node,
                                                                const bNodeSocket &from_sock,
                                                                const StringRefNull socket_type)
{
  return add_interface_socket_from_node(ntree, from_node, from_sock, socket_type, from_sock.name);
}

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode &from_node,
                                                                const bNodeSocket &from_sock)
{
  return add_interface_socket_from_node(
      ntree, from_node, from_sock, from_sock.typeinfo->idname, from_sock.name);
}

}  // namespace node_interface

}  // namespace blender::bke
