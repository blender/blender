/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_idprop.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_node.hh"
#include "BKE_node_tree_interface.hh"

#include "BLI_math_vector.h"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BLO_read_write.hh"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"
#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"

namespace blender::bke::node_interface {

namespace socket_types {

/* Try to get a supported socket type from some final type.
 * Built-in socket can have multiple registered RNA types for the base type, e.g.
 * `NodeSocketFloatUnsigned`, `NodeSocketFloatFactor`. Only the "base type" (`NodeSocketFloat`)
 * is considered valid for interface sockets.
 */
static const char *try_get_supported_socket_type(StringRefNull socket_type)
{
  const bNodeSocketType *typeinfo = nodeSocketTypeFind(socket_type.c_str());
  if (typeinfo == nullptr) {
    return nullptr;
  }
  /* For builtin socket types only the base type is supported. */
  if (nodeIsStaticSocketType(typeinfo)) {
    return nodeStaticSocketType(typeinfo->type, PROP_NONE);
  }
  return typeinfo->idname;
}

/* -------------------------------------------------------------------- */
/** \name ID User Increment in Socket Data
 * \{ */

template<typename T> void socket_data_id_user_increment(T & /*data*/) {}
template<> void socket_data_id_user_increment(bNodeSocketValueObject &data)
{
  id_us_plus(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_increment(bNodeSocketValueImage &data)
{
  id_us_plus(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_increment(bNodeSocketValueCollection &data)
{
  id_us_plus(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_increment(bNodeSocketValueTexture &data)
{
  id_us_plus(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_increment(bNodeSocketValueMaterial &data)
{
  id_us_plus(reinterpret_cast<ID *>(data.value));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID User Decrement in Socket Data
 * \{ */

template<typename T> void socket_data_id_user_decrement(T & /*data*/) {}
template<> void socket_data_id_user_decrement(bNodeSocketValueObject &data)
{
  id_us_min(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_decrement(bNodeSocketValueImage &data)
{
  id_us_min(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_decrement(bNodeSocketValueCollection &data)
{
  id_us_min(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_decrement(bNodeSocketValueTexture &data)
{
  id_us_min(reinterpret_cast<ID *>(data.value));
}
template<> void socket_data_id_user_decrement(bNodeSocketValueMaterial &data)
{
  id_us_min(reinterpret_cast<ID *>(data.value));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialize Socket Data
 * \{ */

template<typename T> void socket_data_init_impl(T & /*data*/) {}
template<> void socket_data_init_impl(bNodeSocketValueFloat &data)
{
  data.subtype = PROP_NONE;
  data.value = 0.0f;
  data.min = -FLT_MAX;
  data.max = FLT_MAX;
}
template<> void socket_data_init_impl(bNodeSocketValueInt &data)
{
  data.subtype = PROP_NONE;
  data.value = 0;
  data.min = INT_MIN;
  data.max = INT_MAX;
}
template<> void socket_data_init_impl(bNodeSocketValueBoolean &data)
{
  data.value = false;
}
template<> void socket_data_init_impl(bNodeSocketValueRotation & /*data*/) {}
template<> void socket_data_init_impl(bNodeSocketValueVector &data)
{
  static float default_value[] = {0.0f, 0.0f, 0.0f};
  data.subtype = PROP_NONE;
  copy_v3_v3(data.value, default_value);
  data.min = -FLT_MAX;
  data.max = FLT_MAX;
}
template<> void socket_data_init_impl(bNodeSocketValueRGBA &data)
{
  static float default_value[] = {0.0f, 0.0f, 0.0f, 1.0f};
  copy_v4_v4(data.value, default_value);
}
template<> void socket_data_init_impl(bNodeSocketValueString &data)
{
  data.subtype = PROP_NONE;
  data.value[0] = '\0';
}
template<> void socket_data_init_impl(bNodeSocketValueObject &data)
{
  data.value = nullptr;
}
template<> void socket_data_init_impl(bNodeSocketValueImage &data)
{
  data.value = nullptr;
}
template<> void socket_data_init_impl(bNodeSocketValueCollection &data)
{
  data.value = nullptr;
}
template<> void socket_data_init_impl(bNodeSocketValueTexture &data)
{
  data.value = nullptr;
}
template<> void socket_data_init_impl(bNodeSocketValueMaterial &data)
{
  data.value = nullptr;
}

static void *make_socket_data(const char *socket_type)
{
  void *socket_data = nullptr;
  socket_data_to_static_type_tag(socket_type, [&socket_data](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    SocketDataType *new_socket_data = MEM_cnew<SocketDataType>(__func__);
    socket_data_init_impl(*new_socket_data);
    socket_data = new_socket_data;
  });
  return socket_data;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Allocated Socket Data
 * \{ */

template<typename T> void socket_data_free_impl(T & /*data*/, const bool /*do_id_user*/) {}

static void socket_data_free(bNodeTreeInterfaceSocket &socket, const bool do_id_user)
{
  socket_data_to_static_type_tag(socket.socket_type, [&](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    if (do_id_user) {
      socket_data_id_user_decrement(get_socket_data_as<SocketDataType>(socket));
    }
    socket_data_free_impl(get_socket_data_as<SocketDataType>(socket), do_id_user);
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Allocated Socket Data
 * \{ */

template<typename T> void socket_data_copy_impl(T & /*dst*/, const T & /*src*/) {}

static void socket_data_copy(bNodeTreeInterfaceSocket &dst,
                             const bNodeTreeInterfaceSocket &src,
                             int flag)
{
  socket_data_to_static_type_tag(dst.socket_type, [&](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    dst.socket_data = MEM_dupallocN(src.socket_data);
    socket_data_copy_impl(get_socket_data_as<SocketDataType>(dst),
                          get_socket_data_as<SocketDataType>(src));
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      socket_data_id_user_increment(get_socket_data_as<SocketDataType>(dst));
    }
  });
}

/* Copy socket data from a raw pointer, e.g. from a #bNodeSocket. */
static void socket_data_copy_ptr(bNodeTreeInterfaceSocket &dst,
                                 const void *src_socket_data,
                                 int flag)
{
  socket_data_to_static_type_tag(dst.socket_type, [&](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;

    if (dst.socket_data != nullptr) {
      socket_data_free(dst, true);
      MEM_SAFE_FREE(dst.socket_data);
    }

    dst.socket_data = MEM_dupallocN(src_socket_data);
    socket_data_copy_impl(get_socket_data_as<SocketDataType>(dst),
                          *static_cast<const SocketDataType *>(src_socket_data));
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      socket_data_id_user_increment(get_socket_data_as<SocketDataType>(dst));
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Write Socket Data to Blend File
 * \{ */

/* Note: no default implementation, every used type must write at least the base struct. */

inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueFloat &data)
{
  BLO_write_struct(writer, bNodeSocketValueFloat, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueInt &data)
{
  BLO_write_struct(writer, bNodeSocketValueInt, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueBoolean &data)
{
  BLO_write_struct(writer, bNodeSocketValueBoolean, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueRotation &data)
{
  BLO_write_struct(writer, bNodeSocketValueRotation, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueVector &data)
{
  BLO_write_struct(writer, bNodeSocketValueVector, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueRGBA &data)
{
  BLO_write_struct(writer, bNodeSocketValueRGBA, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueString &data)
{
  BLO_write_struct(writer, bNodeSocketValueString, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueObject &data)
{
  BLO_write_struct(writer, bNodeSocketValueObject, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueImage &data)
{
  BLO_write_struct(writer, bNodeSocketValueImage, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueCollection &data)
{
  BLO_write_struct(writer, bNodeSocketValueCollection, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueTexture &data)
{
  BLO_write_struct(writer, bNodeSocketValueTexture, &data);
}
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueMaterial &data)
{
  BLO_write_struct(writer, bNodeSocketValueMaterial, &data);
}

static void socket_data_write(BlendWriter *writer, bNodeTreeInterfaceSocket &socket)
{
  socket_data_to_static_type_tag(socket.socket_type, [&](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    socket_data_write_impl(writer, get_socket_data_as<SocketDataType>(socket));
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Socket Data from Blend File
 * \{ */

template<typename T> void socket_data_read_data_impl(BlendDataReader *reader, T **data)
{
  BLO_read_data_address(reader, data);
}

static void socket_data_read_data(BlendDataReader *reader, bNodeTreeInterfaceSocket &socket)
{
  socket_data_to_static_type_tag(socket.socket_type, [&](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    socket_data_read_data_impl(reader, reinterpret_cast<SocketDataType **>(&socket.socket_data));
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callback per ID Pointer
 * \{ */

template<typename T>
void socket_data_foreach_id_impl(LibraryForeachIDData * /*data*/, T & /*data*/)
{
}
template<> void socket_data_foreach_id_impl(LibraryForeachIDData *cb, bNodeSocketValueObject &data)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(cb, data.value, IDWALK_CB_USER);
}
template<> void socket_data_foreach_id_impl(LibraryForeachIDData *cb, bNodeSocketValueImage &data)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(cb, data.value, IDWALK_CB_USER);
}
template<>
void socket_data_foreach_id_impl(LibraryForeachIDData *cb, bNodeSocketValueCollection &data)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(cb, data.value, IDWALK_CB_USER);
}
template<>
void socket_data_foreach_id_impl(LibraryForeachIDData *cb, bNodeSocketValueTexture &data)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(cb, data.value, IDWALK_CB_USER);
}
template<>
void socket_data_foreach_id_impl(LibraryForeachIDData *cb, bNodeSocketValueMaterial &data)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(cb, data.value, IDWALK_CB_USER);
}

static void socket_data_foreach_id(LibraryForeachIDData *data, bNodeTreeInterfaceSocket &socket)
{
  socket_data_to_static_type_tag(socket.socket_type, [&](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    socket_data_foreach_id_impl(data, get_socket_data_as<SocketDataType>(socket));
  });
}

/** \} */

}  // namespace socket_types

namespace item_types {

static void item_copy(bNodeTreeInterfaceItem &dst,
                      const bNodeTreeInterfaceItem &src,
                      const int flag)
{
  switch (dst.item_type) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &dst_socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(dst);
      const bNodeTreeInterfaceSocket &src_socket =
          reinterpret_cast<const bNodeTreeInterfaceSocket &>(src);
      BLI_assert(src_socket.name != nullptr);
      BLI_assert(src_socket.socket_type != nullptr);

      dst_socket.name = BLI_strdup(src_socket.name);
      dst_socket.description = BLI_strdup_null(src_socket.description);
      dst_socket.socket_type = BLI_strdup(src_socket.socket_type);
      dst_socket.default_attribute_name = BLI_strdup_null(src_socket.default_attribute_name);
      dst_socket.identifier = BLI_strdup(src_socket.identifier);
      if (src_socket.properties) {
        dst_socket.properties = IDP_CopyProperty_ex(src_socket.properties, flag);
      }
      if (src_socket.socket_data != nullptr) {
        socket_types::socket_data_copy(dst_socket, src_socket, flag);
      }
      break;
    }
    case NODE_INTERFACE_PANEL: {
      bNodeTreeInterfacePanel &dst_panel = reinterpret_cast<bNodeTreeInterfacePanel &>(dst);
      const bNodeTreeInterfacePanel &src_panel = reinterpret_cast<const bNodeTreeInterfacePanel &>(
          src);
      BLI_assert(src_panel.name != nullptr);

      dst_panel.name = BLI_strdup(src_panel.name);
      dst_panel.description = BLI_strdup_null(src_panel.description);
      dst_panel.copy_from(src_panel.items(), flag);
      break;
    }
  }
}

static void item_free(bNodeTreeInterfaceItem &item, const bool do_id_user)
{
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);

      if (socket.socket_data != nullptr) {
        socket_types::socket_data_free(socket, do_id_user);
        MEM_SAFE_FREE(socket.socket_data);
      }

      MEM_SAFE_FREE(socket.name);
      MEM_SAFE_FREE(socket.description);
      MEM_SAFE_FREE(socket.socket_type);
      MEM_SAFE_FREE(socket.default_attribute_name);
      MEM_SAFE_FREE(socket.identifier);
      if (socket.properties) {
        IDP_FreePropertyContent_ex(socket.properties, do_id_user);
        MEM_freeN(socket.properties);
      }
      break;
    }
    case NODE_INTERFACE_PANEL: {
      bNodeTreeInterfacePanel &panel = reinterpret_cast<bNodeTreeInterfacePanel &>(item);

      panel.clear(do_id_user);
      MEM_SAFE_FREE(panel.name);
      MEM_SAFE_FREE(panel.description);
      break;
    }
  }

  MEM_freeN(&item);
}

void item_write_struct(BlendWriter *writer, bNodeTreeInterfaceItem &item);

static void item_write_data(BlendWriter *writer, bNodeTreeInterfaceItem &item)
{
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);
      BLO_write_string(writer, socket.name);
      BLO_write_string(writer, socket.identifier);
      BLO_write_string(writer, socket.description);
      BLO_write_string(writer, socket.socket_type);
      BLO_write_string(writer, socket.default_attribute_name);
      if (socket.properties) {
        IDP_BlendWrite(writer, socket.properties);
      }

      socket_types::socket_data_write(writer, socket);
      break;
    }
    case NODE_INTERFACE_PANEL: {
      bNodeTreeInterfacePanel &panel = reinterpret_cast<bNodeTreeInterfacePanel &>(item);
      BLO_write_string(writer, panel.name);
      BLO_write_string(writer, panel.description);
      BLO_write_pointer_array(writer, panel.items_num, panel.items_array);
      for (bNodeTreeInterfaceItem *child_item : panel.items()) {
        item_write_struct(writer, *child_item);
      }
      break;
    }
  }
}

void item_write_struct(BlendWriter *writer, bNodeTreeInterfaceItem &item)
{
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      BLO_write_struct(writer, bNodeTreeInterfaceSocket, &item);
      break;
    }
    case NODE_INTERFACE_PANEL: {
      BLO_write_struct(writer, bNodeTreeInterfacePanel, &item);
      break;
    }
  }

  item_write_data(writer, item);
}

static void item_read_data(BlendDataReader *reader, bNodeTreeInterfaceItem &item)
{
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);
      BLO_read_data_address(reader, &socket.name);
      BLO_read_data_address(reader, &socket.description);
      BLO_read_data_address(reader, &socket.socket_type);
      BLO_read_data_address(reader, &socket.default_attribute_name);
      BLO_read_data_address(reader, &socket.identifier);
      BLO_read_data_address(reader, &socket.properties);
      IDP_BlendDataRead(reader, &socket.properties);

      socket_types::socket_data_read_data(reader, socket);
      break;
    }
    case NODE_INTERFACE_PANEL: {
      bNodeTreeInterfacePanel &panel = reinterpret_cast<bNodeTreeInterfacePanel &>(item);
      BLO_read_data_address(reader, &panel.name);
      BLO_read_data_address(reader, &panel.description);
      BLO_read_pointer_array(reader, reinterpret_cast<void **>(&panel.items_array));
      for (const int i : blender::IndexRange(panel.items_num)) {
        BLO_read_data_address(reader, &panel.items_array[i]);
        item_read_data(reader, *panel.items_array[i]);
      }
      break;
    }
  }
}

static void item_foreach_id(LibraryForeachIDData *data, bNodeTreeInterfaceItem &item)
{
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);

      BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
          data,
          IDP_foreach_property(socket.properties,
                               IDP_TYPE_FILTER_ID,
                               BKE_lib_query_idpropertiesForeachIDLink_callback,
                               data));

      socket_types::socket_data_foreach_id(data, socket);
      break;
    }
    case NODE_INTERFACE_PANEL: {
      bNodeTreeInterfacePanel &panel = reinterpret_cast<bNodeTreeInterfacePanel &>(item);
      for (bNodeTreeInterfaceItem *item : panel.items()) {
        item_foreach_id(data, *item);
      }
      break;
    }
  }
}

/* Move all child items to the new parent. */
static Span<bNodeTreeInterfaceItem *> item_children(bNodeTreeInterfaceItem &item)
{
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      return {};
    }
    case NODE_INTERFACE_PANEL: {
      bNodeTreeInterfacePanel &panel = reinterpret_cast<bNodeTreeInterfacePanel &>(item);
      return panel.items();
    }
  }
  return {};
}

}  // namespace item_types

}  // namespace blender::bke::node_interface

using namespace blender::bke::node_interface;

bNodeSocketType *bNodeTreeInterfaceSocket::socket_typeinfo() const
{
  return nodeSocketTypeFind(socket_type);
}

blender::ColorGeometry4f bNodeTreeInterfaceSocket::socket_color() const
{
  bNodeSocketType *typeinfo = this->socket_typeinfo();
  if (!typeinfo || !typeinfo->draw_color_simple) {
    return blender::ColorGeometry4f(1.0f, 0.0f, 1.0f, 1.0f);
  }

  float color[4];
  if (typeinfo->draw_color_simple) {
    typeinfo->draw_color_simple(typeinfo, color);
  }
  return blender::ColorGeometry4f(color);
}

bool bNodeTreeInterfaceSocket::set_socket_type(const char *new_socket_type)
{
  const char *idname = socket_types::try_get_supported_socket_type(new_socket_type);
  if (idname == nullptr) {
    return false;
  }

  if (this->socket_data != nullptr) {
    socket_types::socket_data_free(*this, true);
    MEM_SAFE_FREE(this->socket_data);
  }
  MEM_SAFE_FREE(this->socket_type);

  this->socket_type = BLI_strdup(new_socket_type);
  this->socket_data = socket_types::make_socket_data(new_socket_type);

  return true;
}

void bNodeTreeInterfaceSocket::init_from_socket_instance(const bNodeSocket *socket)
{
  const char *idname = socket_types::try_get_supported_socket_type(socket->idname);
  BLI_assert(idname != nullptr);

  if (this->socket_data != nullptr) {
    socket_types::socket_data_free(*this, true);
    MEM_SAFE_FREE(this->socket_data);
  }
  MEM_SAFE_FREE(this->socket_type);

  this->socket_type = BLI_strdup(idname);
  this->socket_data = socket_types::make_socket_data(idname);
  socket_types::socket_data_copy_ptr(*this, socket->default_value, 0);
}

blender::IndexRange bNodeTreeInterfacePanel::items_range() const
{
  return blender::IndexRange(items_num);
}

blender::Span<const bNodeTreeInterfaceItem *> bNodeTreeInterfacePanel::items() const
{
  return blender::Span(items_array, items_num);
}

blender::MutableSpan<bNodeTreeInterfaceItem *> bNodeTreeInterfacePanel::items()
{
  return blender::MutableSpan(items_array, items_num);
}

bool bNodeTreeInterfacePanel::contains(const bNodeTreeInterfaceItem &item) const
{
  return items().contains(&item);
}

bool bNodeTreeInterfacePanel::contains_recursive(const bNodeTreeInterfaceItem &item) const
{
  bool is_child = false;
  /* Have to capture item address here instead of just a reference,
   * otherwise pointer comparison will not work. */
  this->foreach_item(
      [&](const bNodeTreeInterfaceItem &titem) {
        if (&titem == &item) {
          is_child = true;
          return false;
        }
        return true;
      },
      true);
  return is_child;
}

int bNodeTreeInterfacePanel::item_position(const bNodeTreeInterfaceItem &item) const
{
  return items().first_index_try(&item);
}

int bNodeTreeInterfacePanel::item_index(const bNodeTreeInterfaceItem &item) const
{
  int index = 0;
  bool found = false;
  /* Have to capture item address here instead of just a reference,
   * otherwise pointer comparison will not work. */
  this->foreach_item([&](const bNodeTreeInterfaceItem &titem) {
    if (&titem == &item) {
      found = true;
      return false;
    }
    ++index;
    return true;
  });
  return found ? index : -1;
}

const bNodeTreeInterfaceItem *bNodeTreeInterfacePanel::item_at_index(int index) const
{
  int i = 0;
  const bNodeTreeInterfaceItem *result = nullptr;
  this->foreach_item([&](const bNodeTreeInterfaceItem &item) {
    if (i == index) {
      result = &item;
      return false;
    }
    ++i;
    return true;
  });
  return result;
}

bNodeTreeInterfacePanel *bNodeTreeInterfacePanel::find_parent_recursive(
    const bNodeTreeInterfaceItem &item)
{
  std::queue<bNodeTreeInterfacePanel *> queue;

  if (this->contains(item)) {
    return this;
  }
  queue.push(this);

  while (!queue.empty()) {
    bNodeTreeInterfacePanel *parent = queue.front();
    queue.pop();

    for (bNodeTreeInterfaceItem *titem : parent->items()) {
      if (titem->item_type != NODE_INTERFACE_PANEL) {
        continue;
      }

      bNodeTreeInterfacePanel *tpanel = get_item_as<bNodeTreeInterfacePanel>(titem);
      if (tpanel->contains(item)) {
        return tpanel;
      }
      queue.push(tpanel);
    }
  }

  return nullptr;
}

int bNodeTreeInterfacePanel::find_valid_insert_position_for_item(
    const bNodeTreeInterfaceItem &item, const int initial_pos) const
{
  const bool sockets_above_panels = !(this->flag &
                                      NODE_INTERFACE_PANEL_ALLOW_SOCKETS_AFTER_PANELS);
  const blender::Span<const bNodeTreeInterfaceItem *> items = this->items();

  int pos = initial_pos;

  if (sockets_above_panels) {
    if (item.item_type == NODE_INTERFACE_PANEL) {
      /* Find the closest valid position from the end, only panels at or after #position. */
      for (int test_pos = items.size() - 1; test_pos >= initial_pos; test_pos--) {
        if (test_pos < 0) {
          /* Initial position is out of range but valid. */
          break;
        }
        if (items[test_pos]->item_type != NODE_INTERFACE_PANEL) {
          /* Found valid position, insert after the last socket item. */
          pos = test_pos + 1;
          break;
        }
      }
    }
    else {
      /* Find the closest valid position from the start, no panels at or after #position. */
      for (int test_pos = 0; test_pos <= initial_pos; test_pos++) {
        if (test_pos >= items.size()) {
          /* Initial position is out of range but valid. */
          break;
        }
        if (items[test_pos]->item_type == NODE_INTERFACE_PANEL) {
          /* Found valid position, inserting moves the first panel. */
          pos = test_pos;
          break;
        }
      }
    }
  }

  return pos;
}

void bNodeTreeInterfacePanel::add_item(bNodeTreeInterfaceItem &item)
{
  /* Same as inserting at the end. */
  insert_item(item, this->items_num);
}

void bNodeTreeInterfacePanel::insert_item(bNodeTreeInterfaceItem &item, int position)
{
  /* Are child panels allowed? */
  BLI_assert(item.item_type != NODE_INTERFACE_PANEL ||
             (this->flag & NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS));

  /* Apply any constraints on the item positions. */
  position = find_valid_insert_position_for_item(item, position);
  position = std::min(std::max(position, 0), items_num);

  blender::MutableSpan<bNodeTreeInterfaceItem *> old_items = this->items();
  items_num++;
  items_array = MEM_cnew_array<bNodeTreeInterfaceItem *>(items_num, __func__);
  this->items().take_front(position).copy_from(old_items.take_front(position));
  this->items().drop_front(position + 1).copy_from(old_items.drop_front(position));
  this->items()[position] = &item;

  if (old_items.data()) {
    MEM_freeN(old_items.data());
  }
}

bool bNodeTreeInterfacePanel::remove_item(bNodeTreeInterfaceItem &item, const bool free)
{
  const int position = this->item_position(item);
  if (!this->items().index_range().contains(position)) {
    return false;
  }

  blender::MutableSpan<bNodeTreeInterfaceItem *> old_items = this->items();
  items_num--;
  items_array = MEM_cnew_array<bNodeTreeInterfaceItem *>(items_num, __func__);
  this->items().take_front(position).copy_from(old_items.take_front(position));
  this->items().drop_front(position).copy_from(old_items.drop_front(position + 1));

  /* Guaranteed not empty, contains at least the removed item */
  MEM_freeN(old_items.data());

  if (free) {
    item_types::item_free(item, true);
  }

  return true;
}

void bNodeTreeInterfacePanel::clear(bool do_id_user)
{
  for (bNodeTreeInterfaceItem *item : this->items()) {
    item_types::item_free(*item, do_id_user);
  }
  MEM_SAFE_FREE(items_array);
  items_array = nullptr;
  items_num = 0;
}

bool bNodeTreeInterfacePanel::move_item(bNodeTreeInterfaceItem &item, int new_position)
{
  const int old_position = this->item_position(item);
  if (!this->items().index_range().contains(old_position)) {
    return false;
  }
  if (old_position == new_position) {
    /* Nothing changes. */
    return true;
  }

  new_position = find_valid_insert_position_for_item(item, new_position);
  new_position = std::min(std::max(new_position, 0), items_num);

  if (old_position < new_position) {
    /* Actual target position and all existing items shifted by 1. */
    const blender::Span<bNodeTreeInterfaceItem *> moved_items = this->items().slice(
        old_position + 1, new_position - old_position - 1);
    bNodeTreeInterfaceItem *tmp = this->items()[old_position];
    std::copy(
        moved_items.begin(), moved_items.end(), this->items().drop_front(old_position).data());
    this->items()[new_position - 1] = tmp;
  }
  else /* old_position > new_position */ {
    const blender::Span<bNodeTreeInterfaceItem *> moved_items = this->items().slice(
        new_position, old_position - new_position);
    bNodeTreeInterfaceItem *tmp = this->items()[old_position];
    std::copy_backward(
        moved_items.begin(), moved_items.end(), this->items().drop_front(old_position + 1).data());
    this->items()[new_position] = tmp;
  }

  return true;
}

void bNodeTreeInterfacePanel::foreach_item(
    blender::FunctionRef<bool(bNodeTreeInterfaceItem &item)> fn, bool include_self)
{
  using ItemSpan = blender::Span<bNodeTreeInterfaceItem *>;
  blender::Stack<ItemSpan> stack;

  if (include_self && fn(this->item) == false) {
    return;
  }
  stack.push(this->items());

  while (!stack.is_empty()) {
    const ItemSpan current_items = stack.pop();

    for (const int index : current_items.index_range()) {
      bNodeTreeInterfaceItem *item = current_items[index];
      if (fn(*item) == false) {
        return;
      }

      if (item->item_type == NODE_INTERFACE_PANEL) {
        bNodeTreeInterfacePanel *panel = reinterpret_cast<bNodeTreeInterfacePanel *>(item);
        /* Reinsert remaining items. */
        if (index < current_items.size() - 1) {
          const ItemSpan remaining_items = current_items.drop_front(index + 1);
          stack.push(remaining_items);
        }
        /* Handle child items first before continuing with current span. */
        stack.push(panel->items());
        break;
      }
    }
  }
}

void bNodeTreeInterfacePanel::foreach_item(
    blender::FunctionRef<bool(const bNodeTreeInterfaceItem &item)> fn, bool include_self) const
{
  using ItemSpan = blender::Span<const bNodeTreeInterfaceItem *>;
  blender::Stack<ItemSpan> stack;

  if (include_self && fn(this->item) == false) {
    return;
  }
  stack.push(this->items());

  while (!stack.is_empty()) {
    const ItemSpan current_items = stack.pop();

    for (const int index : current_items.index_range()) {
      const bNodeTreeInterfaceItem *item = current_items[index];
      if (fn(*item) == false) {
        return;
      }

      if (item->item_type == NODE_INTERFACE_PANEL) {
        const bNodeTreeInterfacePanel *panel = reinterpret_cast<const bNodeTreeInterfacePanel *>(
            item);
        /* Reinsert remaining items. */
        if (index < current_items.size() - 1) {
          const ItemSpan remaining_items = current_items.drop_front(index + 1);
          stack.push(remaining_items);
        }
        /* Handle child items first before continuing with current span. */
        stack.push(panel->items());
        break;
      }
    }
  }
}

static bNodeTreeInterfaceSocket *make_socket(const int uid,
                                             blender::StringRefNull name,
                                             blender::StringRefNull description,
                                             blender::StringRefNull socket_type,
                                             const NodeTreeInterfaceSocketFlag flag)
{
  BLI_assert(name.c_str() != nullptr);
  BLI_assert(socket_type.c_str() != nullptr);

  const char *idname = socket_types::try_get_supported_socket_type(socket_type.c_str());
  if (idname == nullptr) {
    return nullptr;
  }

  bNodeTreeInterfaceSocket *new_socket = MEM_cnew<bNodeTreeInterfaceSocket>(__func__);
  BLI_assert(new_socket);

  /* Init common socket properties. */
  new_socket->identifier = BLI_sprintfN("Socket_%d", uid);
  new_socket->item.item_type = NODE_INTERFACE_SOCKET;
  new_socket->name = BLI_strdup(name.c_str());
  new_socket->description = BLI_strdup_null(description.c_str());
  new_socket->socket_type = BLI_strdup(socket_type.c_str());
  new_socket->flag = flag;

  new_socket->socket_data = socket_types::make_socket_data(socket_type.c_str());

  return new_socket;
}

static bNodeTreeInterfacePanel *make_panel(const int uid,
                                           blender::StringRefNull name,
                                           blender::StringRefNull description,
                                           const NodeTreeInterfacePanelFlag flag)
{
  BLI_assert(name.c_str() != nullptr);

  bNodeTreeInterfacePanel *new_panel = MEM_cnew<bNodeTreeInterfacePanel>(__func__);
  new_panel->item.item_type = NODE_INTERFACE_PANEL;
  new_panel->name = BLI_strdup(name.c_str());
  new_panel->description = BLI_strdup_null(description.c_str());
  new_panel->identifier = uid;
  new_panel->flag = flag;
  return new_panel;
}

void bNodeTreeInterfacePanel::copy_from(
    const blender::Span<const bNodeTreeInterfaceItem *> items_src, int flag)
{
  items_num = items_src.size();
  items_array = MEM_cnew_array<bNodeTreeInterfaceItem *>(items_num, __func__);

  /* Copy buffers. */
  for (const int i : items_src.index_range()) {
    const bNodeTreeInterfaceItem *item_src = items_src[i];
    BLI_assert(item_src->item_type != NODE_INTERFACE_PANEL ||
               (flag & NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS));
    items_array[i] = static_cast<bNodeTreeInterfaceItem *>(MEM_dupallocN(item_src));
    item_types::item_copy(*items_array[i], *item_src, flag);
  }
}

void bNodeTreeInterface::init_data()
{
  /* Root panel is allowed to contain child panels. */
  root_panel.flag |= NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS;
}

void bNodeTreeInterface::copy_data(const bNodeTreeInterface &src, int flag)
{
  this->root_panel.copy_from(src.root_panel.items(), flag);
  this->active_index = src.active_index;
}

void bNodeTreeInterface::free_data()
{
  /* Called when freeing the main database, don't do user refcount here. */
  this->root_panel.clear(false);
}

void bNodeTreeInterface::write(BlendWriter *writer)
{
  BLO_write_struct(writer, bNodeTreeInterface, this);
  /* Don't write the root panel struct itself, it's nested in the interface struct. */
  item_types::item_write_data(writer, this->root_panel.item);
}

void bNodeTreeInterface::read_data(BlendDataReader *reader)
{
  item_types::item_read_data(reader, this->root_panel.item);
}

bNodeTreeInterfaceItem *bNodeTreeInterface::active_item()
{
  bNodeTreeInterfaceItem *active = nullptr;
  int count = active_index;
  this->foreach_item([&](bNodeTreeInterfaceItem &item) {
    if (count == 0) {
      active = &item;
      return false;
    }
    --count;
    return true;
  });
  return active;
}

const bNodeTreeInterfaceItem *bNodeTreeInterface::active_item() const
{
  const bNodeTreeInterfaceItem *active = nullptr;
  int count = active_index;
  this->foreach_item([&](const bNodeTreeInterfaceItem &item) {
    if (count == 0) {
      active = &item;
      return false;
    }
    --count;
    return true;
  });
  return active;
}

void bNodeTreeInterface::active_item_set(bNodeTreeInterfaceItem *item)
{
  active_index = 0;
  int count = 0;
  this->foreach_item([&](bNodeTreeInterfaceItem &titem) {
    if (&titem == item) {
      active_index = count;
      return false;
    }
    ++count;
    return true;
  });
}

bNodeTreeInterfaceSocket *bNodeTreeInterface::add_socket(blender::StringRefNull name,
                                                         blender::StringRefNull description,
                                                         blender::StringRefNull socket_type,
                                                         const NodeTreeInterfaceSocketFlag flag,
                                                         bNodeTreeInterfacePanel *parent)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  bNodeTreeInterfaceSocket *new_socket = make_socket(
      next_uid++, name, description, socket_type, flag);
  if (new_socket) {
    parent->add_item(new_socket->item);
  }
  return new_socket;
}

bNodeTreeInterfaceSocket *bNodeTreeInterface::insert_socket(blender::StringRefNull name,
                                                            blender::StringRefNull description,
                                                            blender::StringRefNull socket_type,
                                                            const NodeTreeInterfaceSocketFlag flag,
                                                            bNodeTreeInterfacePanel *parent,
                                                            const int position)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  bNodeTreeInterfaceSocket *new_socket = make_socket(
      next_uid++, name, description, socket_type, flag);
  if (new_socket) {
    parent->insert_item(new_socket->item, position);
  }
  return new_socket;
}

bNodeTreeInterfacePanel *bNodeTreeInterface::add_panel(blender::StringRefNull name,
                                                       blender::StringRefNull description,
                                                       const NodeTreeInterfacePanelFlag flag,
                                                       bNodeTreeInterfacePanel *parent)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  if (!(parent->flag & NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS)) {
    /* Parent does not allow adding child panels. */
    return nullptr;
  }

  bNodeTreeInterfacePanel *new_panel = make_panel(next_uid++, name, description, flag);
  if (new_panel) {
    parent->add_item(new_panel->item);
  }
  return new_panel;
}

bNodeTreeInterfacePanel *bNodeTreeInterface::insert_panel(blender::StringRefNull name,
                                                          blender::StringRefNull description,
                                                          const NodeTreeInterfacePanelFlag flag,
                                                          bNodeTreeInterfacePanel *parent,
                                                          const int position)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  if (!(parent->flag & NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS)) {
    /* Parent does not allow adding child panels. */
    return nullptr;
  }

  bNodeTreeInterfacePanel *new_panel = make_panel(next_uid++, name, description, flag);
  if (new_panel) {
    parent->insert_item(new_panel->item, position);
  }
  return new_panel;
}

bNodeTreeInterfaceItem *bNodeTreeInterface::add_item_copy(const bNodeTreeInterfaceItem &item,
                                                          bNodeTreeInterfacePanel *parent)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(item));
  BLI_assert(this->find_item(parent->item));

  if (item.item_type == NODE_INTERFACE_PANEL &&
      !(parent->flag & NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS))
  {
    /* Parent does not allow adding child panels. */
    return nullptr;
  }

  bNodeTreeInterfaceItem *citem = static_cast<bNodeTreeInterfaceItem *>(MEM_dupallocN(&item));
  item_types::item_copy(*citem, item, 0);
  parent->add_item(*citem);

  return citem;
}

bNodeTreeInterfaceItem *bNodeTreeInterface::insert_item_copy(const bNodeTreeInterfaceItem &item,
                                                             bNodeTreeInterfacePanel *parent,
                                                             int position)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(item));
  BLI_assert(this->find_item(parent->item));

  if (item.item_type == NODE_INTERFACE_PANEL &&
      !(parent->flag & NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS))
  {
    /* Parent does not allow adding child panels. */
    return nullptr;
  }

  bNodeTreeInterfaceItem *citem = static_cast<bNodeTreeInterfaceItem *>(MEM_dupallocN(&item));
  item_types::item_copy(*citem, item, 0);
  parent->insert_item(*citem, position);

  return citem;
}

bool bNodeTreeInterface::remove_item(bNodeTreeInterfaceItem &item, bool move_content_to_parent)
{
  bNodeTreeInterfacePanel *parent = this->find_item_parent(item, true);
  if (parent == nullptr) {
    return false;
  }
  if (move_content_to_parent) {
    int position = parent->item_position(item);
    /* Cache children to avoid invalidating the iterator. */
    blender::Array<bNodeTreeInterfaceItem *> children(item_types::item_children(item));
    for (bNodeTreeInterfaceItem *child : children) {
      this->move_item_to_parent(*child, parent, position++);
    }
  }
  if (parent->remove_item(item, true)) {
    return true;
  }
  return false;
}

void bNodeTreeInterface::clear_items()
{
  root_panel.clear(true);
}

bool bNodeTreeInterface::move_item(bNodeTreeInterfaceItem &item, const int new_position)
{
  bNodeTreeInterfacePanel *parent = this->find_item_parent(item, true);
  if (parent == nullptr) {
    return false;
  }
  return parent->move_item(item, new_position);
}

bool bNodeTreeInterface::move_item_to_parent(bNodeTreeInterfaceItem &item,
                                             bNodeTreeInterfacePanel *new_parent,
                                             int new_position)
{
  bNodeTreeInterfacePanel *parent = this->find_item_parent(item, true);
  if (parent == nullptr) {
    return false;
  }
  if (item.item_type == NODE_INTERFACE_PANEL && new_parent &&
      !(new_parent->flag & NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS))
  {
    /* Parent does not allow adding child panels. */
    return false;
  }
  if (parent == new_parent) {
    return parent->move_item(item, new_position);
  }
  else {
    /* Note: only remove and reinsert when parents different, otherwise removing the item can
     * change the desired target position! */
    if (parent->remove_item(item, false)) {
      new_parent->insert_item(item, new_position);
      return true;
    }
  }
  return false;
}

void bNodeTreeInterface::foreach_id(LibraryForeachIDData *cb)
{
  item_types::item_foreach_id(cb, root_panel.item);
}

namespace blender::bke {

void bNodeTreeInterfaceCache::rebuild(bNodeTreeInterface &interface)
{
  /* Rebuild draw-order list of interface items for linear access. */
  items.clear();
  inputs.clear();
  outputs.clear();

  interface.foreach_item([&](bNodeTreeInterfaceItem &item) {
    items.append(&item);
    if (bNodeTreeInterfaceSocket *socket = get_item_as<bNodeTreeInterfaceSocket>(&item)) {
      if (socket->flag & NODE_INTERFACE_SOCKET_INPUT) {
        inputs.append(socket);
      }
      if (socket->flag & NODE_INTERFACE_SOCKET_OUTPUT) {
        outputs.append(socket);
      }
    }
    return true;
  });
}

}  // namespace blender::bke
