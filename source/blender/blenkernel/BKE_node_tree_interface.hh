/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"

#include "BKE_node.hh"

#include <type_traits>

#include "BLI_cache_mutex.hh"
#include "BLI_parameter_pack_utils.hh"
#include "BLI_vector_set.hh"

namespace blender::bke {

class NodeTreeMainUpdater;

class bNodeTreeInterfaceRuntime {
  friend bNodeTreeInterface;
  friend bNodeTree;

 private:
  /**
   * Changes have been made to the interface items that invalidate dependent trees.
   */
  std::atomic<bool> interface_changed_ = true;

  /**
   * Protects access to item cache variables below. This is necessary so that the cache can be
   * updated on a const #bNodeTreeInterface.
   */
  CacheMutex items_cache_mutex_;

  /* Runtime topology cache for linear access to items. */
  VectorSet<bNodeTreeInterfaceItem *> items_;
  /* Socket-only lists for input/output access by index. */
  VectorSet<bNodeTreeInterfaceSocket *> inputs_;
  VectorSet<bNodeTreeInterfaceSocket *> outputs_;
};

namespace node_interface {

namespace detail {

template<typename T> static bool item_is_type(const bNodeTreeInterfaceItem &item)
{
  bool match = false;
  switch (NodeTreeInterfaceItemType(item.item_type)) {
    case NODE_INTERFACE_SOCKET: {
      match |= std::is_same_v<T, bNodeTreeInterfaceSocket>;
      break;
    }
    case NODE_INTERFACE_PANEL: {
      match |= std::is_same_v<T, bNodeTreeInterfacePanel>;
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

/* Info for generating static subtypes. */
struct bNodeSocketStaticTypeInfo {
  const char *socket_identifier;
  const char *interface_identifier;
  eNodeSocketDatatype type;
  PropertySubType subtype;
  const char *label;
};

/* NOTE: Socket and interface subtypes could be defined from a single central list,
 * but makesrna cannot have a dependency on BKE, so this list would have to live in RNA itself,
 * with BKE etc. accessing the RNA API to get the subtypes info. */
static const bNodeSocketStaticTypeInfo node_socket_subtypes[] = {
    {"NodeSocketFloat", "NodeTreeInterfaceSocketFloat", SOCK_FLOAT, PROP_NONE},
    {"NodeSocketFloatUnsigned", "NodeTreeInterfaceSocketFloatUnsigned", SOCK_FLOAT, PROP_UNSIGNED},
    {"NodeSocketFloatPercentage",
     "NodeTreeInterfaceSocketFloatPercentage",
     SOCK_FLOAT,
     PROP_PERCENTAGE},
    {"NodeSocketFloatFactor", "NodeTreeInterfaceSocketFloatFactor", SOCK_FLOAT, PROP_FACTOR},
    {"NodeSocketFloatAngle", "NodeTreeInterfaceSocketFloatAngle", SOCK_FLOAT, PROP_ANGLE},
    {"NodeSocketFloatTime", "NodeTreeInterfaceSocketFloatTime", SOCK_FLOAT, PROP_TIME},
    {"NodeSocketFloatTimeAbsolute",
     "NodeTreeInterfaceSocketFloatTimeAbsolute",
     SOCK_FLOAT,
     PROP_TIME_ABSOLUTE},
    {"NodeSocketFloatDistance", "NodeTreeInterfaceSocketFloatDistance", SOCK_FLOAT, PROP_DISTANCE},
    {"NodeSocketFloatWavelength",
     "NodeTreeInterfaceSocketFloatWavelength",
     SOCK_FLOAT,
     PROP_WAVELENGTH},
    {"NodeSocketFloatColorTemperature",
     "NodeTreeInterfaceSocketFloatColorTemperature",
     SOCK_FLOAT,
     PROP_COLOR_TEMPERATURE},
    {"NodeSocketFloatFrequency",
     "NodeTreeInterfaceSocketFloatFrequency",
     SOCK_FLOAT,
     PROP_FREQUENCY},
    {"NodeSocketInt", "NodeTreeInterfaceSocketInt", SOCK_INT, PROP_NONE},
    {"NodeSocketIntUnsigned", "NodeTreeInterfaceSocketIntUnsigned", SOCK_INT, PROP_UNSIGNED},
    {"NodeSocketIntPercentage", "NodeTreeInterfaceSocketIntPercentage", SOCK_INT, PROP_PERCENTAGE},
    {"NodeSocketIntFactor", "NodeTreeInterfaceSocketIntFactor", SOCK_INT, PROP_FACTOR},
    {"NodeSocketBool", "NodeTreeInterfaceSocketBool", SOCK_BOOLEAN, PROP_NONE},

    {"NodeSocketVector", "NodeTreeInterfaceSocketVector", SOCK_VECTOR, PROP_NONE},
    {"NodeSocketVectorFactor", "NodeTreeInterfaceSocketVectorFactor", SOCK_VECTOR, PROP_FACTOR},
    {"NodeSocketVectorPercentage",
     "NodeTreeInterfaceSocketVectorPercentage",
     SOCK_VECTOR,
     PROP_PERCENTAGE},
    {"NodeSocketVectorTranslation",
     "NodeTreeInterfaceSocketVectorTranslation",
     SOCK_VECTOR,
     PROP_TRANSLATION},
    {"NodeSocketVectorDirection",
     "NodeTreeInterfaceSocketVectorDirection",
     SOCK_VECTOR,
     PROP_DIRECTION},
    {"NodeSocketVectorVelocity",
     "NodeTreeInterfaceSocketVectorVelocity",
     SOCK_VECTOR,
     PROP_VELOCITY},
    {"NodeSocketVectorAcceleration",
     "NodeTreeInterfaceSocketVectorAcceleration",
     SOCK_VECTOR,
     PROP_ACCELERATION},
    {"NodeSocketVectorEuler", "NodeTreeInterfaceSocketVectorEuler", SOCK_VECTOR, PROP_EULER},
    {"NodeSocketVectorXYZ", "NodeTreeInterfaceSocketVectorXYZ", SOCK_VECTOR, PROP_XYZ},

    {"NodeSocketVector2D", "NodeTreeInterfaceSocketVector2D", SOCK_VECTOR, PROP_NONE},
    {"NodeSocketVectorFactor2D",
     "NodeTreeInterfaceSocketVectorFactor2D",
     SOCK_VECTOR,
     PROP_FACTOR},
    {"NodeSocketVectorPercentage2D",
     "NodeTreeInterfaceSocketVectorPercentage2D",
     SOCK_VECTOR,
     PROP_PERCENTAGE},
    {"NodeSocketVectorTranslation2D",
     "NodeTreeInterfaceSocketVectorTranslation2D",
     SOCK_VECTOR,
     PROP_TRANSLATION},
    {"NodeSocketVectorDirection2D",
     "NodeTreeInterfaceSocketVectorDirection2D",
     SOCK_VECTOR,
     PROP_DIRECTION},
    {"NodeSocketVectorVelocity2D",
     "NodeTreeInterfaceSocketVectorVelocity2D",
     SOCK_VECTOR,
     PROP_VELOCITY},
    {"NodeSocketVectorAcceleration2D",
     "NodeTreeInterfaceSocketVectorAcceleration2D",
     SOCK_VECTOR,
     PROP_ACCELERATION},
    {"NodeSocketVectorEuler2D", "NodeTreeInterfaceSocketVectorEuler2D", SOCK_VECTOR, PROP_EULER},
    {"NodeSocketVectorXYZ2D", "NodeTreeInterfaceSocketVectorXYZ2D", SOCK_VECTOR, PROP_XYZ},

    {"NodeSocketVector4D", "NodeTreeInterfaceSocketVector4D", SOCK_VECTOR, PROP_NONE},
    {"NodeSocketVectorFactor4D",
     "NodeTreeInterfaceSocketVectorFactor4D",
     SOCK_VECTOR,
     PROP_FACTOR},
    {"NodeSocketVectorPercentage4D",
     "NodeTreeInterfaceSocketVectorPercentage4D",
     SOCK_VECTOR,
     PROP_PERCENTAGE},
    {"NodeSocketVectorTranslation4D",
     "NodeTreeInterfaceSocketVectorTranslation4D",
     SOCK_VECTOR,
     PROP_TRANSLATION},
    {"NodeSocketVectorDirection4D",
     "NodeTreeInterfaceSocketVectorDirection4D",
     SOCK_VECTOR,
     PROP_DIRECTION},
    {"NodeSocketVectorVelocity4D",
     "NodeTreeInterfaceSocketVectorVelocity4D",
     SOCK_VECTOR,
     PROP_VELOCITY},
    {"NodeSocketVectorAcceleration4D",
     "NodeTreeInterfaceSocketVectorAcceleration4D",
     SOCK_VECTOR,
     PROP_ACCELERATION},
    {"NodeSocketVectorEuler4D", "NodeTreeInterfaceSocketVectorEuler4D", SOCK_VECTOR, PROP_EULER},
    {"NodeSocketVectorXYZ4D", "NodeTreeInterfaceSocketVectorXYZ4D", SOCK_VECTOR, PROP_XYZ},

    {"NodeSocketRotation", "NodeTreeInterfaceSocketRotation", SOCK_ROTATION, PROP_NONE},
    {"NodeSocketMatrix", "NodeTreeInterfaceSocketMatrix", SOCK_MATRIX, PROP_NONE},

    {"NodeSocketColor", "NodeTreeInterfaceSocketColor", SOCK_RGBA, PROP_NONE},
    {"NodeSocketString", "NodeTreeInterfaceSocketString", SOCK_STRING, PROP_NONE},
    {"NodeSocketStringFilePath",
     "NodeTreeInterfaceSocketStringFilePath",
     SOCK_STRING,
     PROP_FILEPATH},
    {"NodeSocketShader", "NodeTreeInterfaceSocketShader", SOCK_SHADER, PROP_NONE},
    {"NodeSocketObject", "NodeTreeInterfaceSocketObject", SOCK_OBJECT, PROP_NONE},
    {"NodeSocketImage", "NodeTreeInterfaceSocketImage", SOCK_IMAGE, PROP_NONE},
    {"NodeSocketGeometry", "NodeTreeInterfaceSocketGeometry", SOCK_GEOMETRY, PROP_NONE},
    {"NodeSocketCollection", "NodeTreeInterfaceSocketCollection", SOCK_COLLECTION, PROP_NONE},
    {"NodeSocketTexture", "NodeTreeInterfaceSocketTexture", SOCK_TEXTURE, PROP_NONE},
    {"NodeSocketMaterial", "NodeTreeInterfaceSocketMaterial", SOCK_MATERIAL, PROP_NONE},
    {"NodeSocketMenu", "NodeTreeInterfaceSocketMenu", SOCK_MENU, PROP_NONE},
    {"NodeSocketBundle", "NodeTreeInterfaceSocketBundle", SOCK_BUNDLE, PROP_NONE},
    {"NodeSocketClosure", "NodeTreeInterfaceSocketClosure", SOCK_CLOSURE, PROP_NONE},
};

template<typename Fn> bool socket_data_to_static_type(const eNodeSocketDatatype type, const Fn &fn)
{
  switch (type) {
    case SOCK_FLOAT:
      fn.template operator()<bNodeSocketValueFloat>();
      return true;
    case SOCK_INT:
      fn.template operator()<bNodeSocketValueInt>();
      return true;
    case SOCK_BOOLEAN:
      fn.template operator()<bNodeSocketValueBoolean>();
      return true;
    case SOCK_ROTATION:
      fn.template operator()<bNodeSocketValueRotation>();
      return true;
    case SOCK_VECTOR:
      fn.template operator()<bNodeSocketValueVector>();
      return true;
    case SOCK_RGBA:
      fn.template operator()<bNodeSocketValueRGBA>();
      return true;
    case SOCK_STRING:
      fn.template operator()<bNodeSocketValueString>();
      return true;
    case SOCK_OBJECT:
      fn.template operator()<bNodeSocketValueObject>();
      return true;
    case SOCK_IMAGE:
      fn.template operator()<bNodeSocketValueImage>();
      return true;
    case SOCK_COLLECTION:
      fn.template operator()<bNodeSocketValueCollection>();
      return true;
    case SOCK_TEXTURE:
      fn.template operator()<bNodeSocketValueTexture>();
      return true;
    case SOCK_MATERIAL:
      fn.template operator()<bNodeSocketValueMaterial>();
      return true;
    case SOCK_MENU:
      fn.template operator()<bNodeSocketValueMenu>();
      return true;

    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_MATRIX:
    case SOCK_GEOMETRY:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
      return true;
  }
  return false;
}

template<typename Fn> bool socket_data_to_static_type(const StringRef socket_type, const Fn &fn)
{
  for (const bNodeSocketStaticTypeInfo &info : node_socket_subtypes) {
    if (socket_type == info.socket_identifier) {
      return socket_data_to_static_type(info.type, fn);
    }
  }
  return false;
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

template<typename Fn>
void socket_data_to_static_type_tag(const StringRef socket_type, const Fn &fn)
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

bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                         const bNode &from_node,
                                                         const bNodeSocket &from_sock,
                                                         StringRef socket_type,
                                                         StringRef name);

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode &from_node,
                                                                const bNodeSocket &from_sock,
                                                                const StringRef socket_type)
{
  return add_interface_socket_from_node(
      ntree, from_node, from_sock, socket_type, node_socket_label(from_sock));
}

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode &from_node,
                                                                const bNodeSocket &from_sock)
{
  return add_interface_socket_from_node(
      ntree, from_node, from_sock, from_sock.typeinfo->idname, node_socket_label(from_sock));
}

/**
 * Reference to a node tree's interface item.
 *
 * Used by the node interface drag controller to reorder interface items and
 * the node space drop-boxes to drop Group Input/Output nodes into the node
 * editor with selected sockets.
 */
struct bNodeTreeInterfaceItemReference {
  bNodeTree *tree;
  bNodeTreeInterfaceItem *item;
};

}  // namespace node_interface

}  // namespace blender::bke
