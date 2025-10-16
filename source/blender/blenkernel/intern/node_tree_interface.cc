/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <queue>

#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_node.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_interface.hh"

#include "BLI_math_vector.h"
#include "BLI_stack.hh"
#include "BLI_string.h"

#include "BLO_read_write.hh"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"
#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"

#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations.hh"

using blender::StringRef;

namespace blender::bke::node_interface {

namespace socket_types {

/* Try to get a supported socket type from some final type.
 * Built-in socket can have multiple registered RNA types for the base type, e.g.
 * `NodeSocketFloatUnsigned`, `NodeSocketFloatFactor`. Only the "base type" (`NodeSocketFloat`)
 * is considered valid for interface sockets.
 */
static std::optional<StringRef> try_get_supported_socket_type(const StringRef socket_type)
{
  const blender::bke::bNodeSocketType *typeinfo = bke::node_socket_type_find(socket_type);
  if (typeinfo == nullptr) {
    return std::nullopt;
  }
  /* For builtin socket types only the base type is supported. */
  if (node_is_static_socket_type(*typeinfo)) {
    if (const std::optional<StringRefNull> type_name = bke::node_static_socket_type(typeinfo->type,
                                                                                    PROP_NONE))
    {
      return *type_name;
    }
    return std::nullopt;
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
  data.dimensions = 3;
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
template<> void socket_data_init_impl(bNodeSocketValueMenu &data)
{
  data.value = -1;
  data.enum_items = nullptr;
  data.runtime_flag = 0;
}

static void *make_socket_data(const StringRef socket_type)
{
  void *socket_data = nullptr;
  socket_data_to_static_type_tag(socket_type, [&socket_data](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    SocketDataType *new_socket_data = MEM_callocN<SocketDataType>(__func__);
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
template<> void socket_data_free_impl(bNodeSocketValueMenu &dst, const bool /*do_id_user*/)
{
  if (dst.enum_items) {
    /* Release shared data pointer. */
    dst.enum_items->remove_user_and_delete_if_last();
  }
}

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
template<>
void socket_data_copy_impl(bNodeSocketValueMenu &dst, const bNodeSocketValueMenu & /*src*/)
{
  /* Copy of shared data pointer. */
  if (dst.enum_items) {
    dst.enum_items->add_user();
  }
}

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

/* NOTE: no default implementation, every used type must write at least the base struct. */

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
inline void socket_data_write_impl(BlendWriter *writer, bNodeSocketValueMenu &data)
{
  BLO_write_struct(writer, bNodeSocketValueMenu, &data);
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
  /* FIXME Avoid using low-level untyped read function here. Cannot use the BLO_read_struct
   * currently (macro expansion would process `T` instead of the actual type). */
  BLO_read_data_address(reader, data);
}
template<> void socket_data_read_data_impl(BlendDataReader *reader, bNodeSocketValueMenu **data)
{
  /* FIXME Avoid using low-level untyped read function here. No type info available here currently.
   */
  BLO_read_data_address(reader, data);
  /* Clear runtime data. */
  (*data)->enum_items = nullptr;
  (*data)->runtime_flag = 0;
}

static void socket_data_read_data(BlendDataReader *reader, bNodeTreeInterfaceSocket &socket)
{
  bool data_read = false;
  socket_data_to_static_type_tag(socket.socket_type, [&](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    socket_data_read_data_impl(reader, reinterpret_cast<SocketDataType **>(&socket.socket_data));
    data_read = true;
  });
  if (!data_read && socket.socket_data) {
    /* Not sure how this can happen exactly, but it did happen in #127855. */
    socket.socket_data = nullptr;
  }
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

using UidGeneratorFn = blender::FunctionRef<int()>;

static void item_copy(bNodeTreeInterfaceItem &dst,
                      const bNodeTreeInterfaceItem &src,
                      int flag,
                      UidGeneratorFn generate_uid);

/**
 * Copy the source items and give each a new unique identifier.
 * \param generate_uid: Optional generator function for new item UIDs, copies existing identifiers
 * if null.
 */
static void panel_init(bNodeTreeInterfacePanel &panel,
                       const Span<const bNodeTreeInterfaceItem *> items_src,
                       const int flag,
                       UidGeneratorFn generate_uid)
{
  panel.items_num = items_src.size();
  panel.items_array = MEM_calloc_arrayN<bNodeTreeInterfaceItem *>(panel.items_num, __func__);

  /* Copy buffers. */
  for (const int i : items_src.index_range()) {
    const bNodeTreeInterfaceItem *item_src = items_src[i];
    panel.items_array[i] = static_cast<bNodeTreeInterfaceItem *>(MEM_dupallocN(item_src));
    item_types::item_copy(*panel.items_array[i], *item_src, flag, generate_uid);
  }
}

/**
 * Copy data from a source item.
 * \param generate_uid: Optional generator function for new item UIDs, copies existing identifiers
 * if null.
 */
static void item_copy(bNodeTreeInterfaceItem &dst,
                      const bNodeTreeInterfaceItem &src,
                      const int flag,
                      UidGeneratorFn generate_uid)
{
  switch (NodeTreeInterfaceItemType(dst.item_type)) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &dst_socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(dst);
      const bNodeTreeInterfaceSocket &src_socket =
          reinterpret_cast<const bNodeTreeInterfaceSocket &>(src);
      BLI_assert(src_socket.socket_type != nullptr);

      dst_socket.name = BLI_strdup_null(src_socket.name);
      dst_socket.description = BLI_strdup_null(src_socket.description);
      dst_socket.socket_type = BLI_strdup(src_socket.socket_type);
      dst_socket.default_attribute_name = BLI_strdup_null(src_socket.default_attribute_name);
      dst_socket.identifier = generate_uid ? BLI_sprintfN("Socket_%d", generate_uid()) :
                                             BLI_strdup(src_socket.identifier);
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

      dst_panel.name = BLI_strdup_null(src_panel.name);
      dst_panel.description = BLI_strdup_null(src_panel.description);
      dst_panel.identifier = generate_uid ? generate_uid() : src_panel.identifier;

      panel_init(dst_panel, src_panel.items(), flag, generate_uid);
      break;
    }
  }
}

static void item_free(bNodeTreeInterfaceItem &item, const bool do_id_user)
{
  switch (NodeTreeInterfaceItemType(item.item_type)) {
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
  switch (NodeTreeInterfaceItemType(item.item_type)) {
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
  switch (NodeTreeInterfaceItemType(item.item_type)) {
    case NODE_INTERFACE_SOCKET: {
      /* Forward compatible writing of older single value only flag. To be removed in 5.0. */
      bNodeTreeInterfaceSocket &socket = get_item_as<bNodeTreeInterfaceSocket>(item);
      SET_FLAG_FROM_TEST(socket.flag,
                         socket.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE,
                         NODE_INTERFACE_SOCKET_SINGLE_VALUE_ONLY_LEGACY);

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
  switch (NodeTreeInterfaceItemType(item.item_type)) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);
      BLO_read_string(reader, &socket.name);
      BLO_read_string(reader, &socket.description);
      BLO_read_string(reader, &socket.socket_type);
      BLO_read_string(reader, &socket.default_attribute_name);
      BLO_read_string(reader, &socket.identifier);
      BLO_read_struct(reader, IDProperty, &socket.properties);
      IDP_BlendDataRead(reader, &socket.properties);

      /* Improve forward compatibility for unknown default input types. */
      const bNodeSocketType *stype = socket.socket_typeinfo();
      if (!stype || !nodes::socket_type_supports_default_input_type(
                        *stype, NodeDefaultInputType(socket.default_input)))
      {
        socket.default_input = NODE_DEFAULT_INPUT_VALUE;
      }

      socket_types::socket_data_read_data(reader, socket);
      break;
    }
    case NODE_INTERFACE_PANEL: {
      bNodeTreeInterfacePanel &panel = reinterpret_cast<bNodeTreeInterfacePanel &>(item);
      BLO_read_string(reader, &panel.name);
      BLO_read_string(reader, &panel.description);
      BLO_read_pointer_array(
          reader, panel.items_num, reinterpret_cast<void **>(&panel.items_array));

      /* Read the direct-data for each interface item if possible. The pointer becomes null if the
       * struct type is not known. */
      for (const int i : blender::IndexRange(panel.items_num)) {
        BLO_read_struct(reader, bNodeTreeInterfaceItem, &panel.items_array[i]);
      }
      /* Forward compatibility: Discard unknown tree interface item types that may be introduced in
       * the future. Their pointer is set to null above. */
      panel.items_num = std::remove_if(
                            panel.items_array,
                            panel.items_array + panel.items_num,
                            [&](const bNodeTreeInterfaceItem *item) { return item == nullptr; }) -
                        panel.items_array;
      /* Now read the actual data if the known interface items. */
      for (const int i : blender::IndexRange(panel.items_num)) {
        item_read_data(reader, *panel.items_array[i]);
      }
      break;
    }
  }
}

static void item_foreach_id(LibraryForeachIDData *data, bNodeTreeInterfaceItem &item)
{
  switch (NodeTreeInterfaceItemType(item.item_type)) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);

      BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
          data, IDP_foreach_property(socket.properties, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
            BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
          }));

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
  switch (NodeTreeInterfaceItemType(item.item_type)) {
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

blender::bke::bNodeSocketType *bNodeTreeInterfaceSocket::socket_typeinfo() const
{
  return blender::bke::node_socket_type_find(socket_type);
}

blender::ColorGeometry4f bNodeTreeInterfaceSocket::socket_color() const
{
  blender::bke::bNodeSocketType *typeinfo = this->socket_typeinfo();
  if (typeinfo && typeinfo->draw_color_simple) {
    float color[4];
    typeinfo->draw_color_simple(typeinfo, color);
    return blender::ColorGeometry4f(color);
  }
  return blender::ColorGeometry4f(1.0f, 0.0f, 1.0f, 1.0f);
}

bool bNodeTreeInterfaceSocket::set_socket_type(const StringRef new_socket_type)
{
  const std::optional<StringRef> idname = socket_types::try_get_supported_socket_type(
      new_socket_type);
  if (!idname) {
    return false;
  }

  if (this->socket_data != nullptr) {
    socket_types::socket_data_free(*this, true);
    MEM_SAFE_FREE(this->socket_data);
  }
  MEM_SAFE_FREE(this->socket_type);

  this->socket_type = BLI_strdupn(new_socket_type.data(), new_socket_type.size());
  this->socket_data = socket_types::make_socket_data(new_socket_type);

  blender::bke::bNodeSocketType *stype = this->socket_typeinfo();
  if (!blender::nodes::socket_type_supports_default_input_type(
          *stype, NodeDefaultInputType(this->default_input)))
  {
    this->default_input = NODE_DEFAULT_INPUT_VALUE;
  }

  return true;
}

void bNodeTreeInterfaceSocket::init_from_socket_instance(const bNodeSocket *socket)
{
  const std::optional<StringRef> idname = socket_types::try_get_supported_socket_type(
      socket->idname);
  BLI_assert(idname.has_value());

  if (this->socket_data != nullptr) {
    socket_types::socket_data_free(*this, true);
    MEM_SAFE_FREE(this->socket_data);
  }
  MEM_SAFE_FREE(this->socket_type);
  if (socket->flag & SOCK_HIDE_VALUE) {
    this->flag |= NODE_INTERFACE_SOCKET_HIDE_VALUE;
  }

  this->socket_type = BLI_strdupn(idname->data(), idname->size());
  this->socket_data = socket_types::make_socket_data(*idname);
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

  /* True if item a should be above item b. */
  auto must_be_before = [sockets_above_panels](const bNodeTreeInterfaceItem &a,
                                               const bNodeTreeInterfaceItem &b) -> bool {
    /* Keep sockets above panels. */
    if (sockets_above_panels) {
      if (a.item_type == NODE_INTERFACE_SOCKET && b.item_type == NODE_INTERFACE_PANEL) {
        return true;
      }
    }
    /* Keep outputs above inputs. */
    if (a.item_type == NODE_INTERFACE_SOCKET && b.item_type == NODE_INTERFACE_SOCKET) {
      const auto &sa = reinterpret_cast<const bNodeTreeInterfaceSocket &>(a);
      const auto &sb = reinterpret_cast<const bNodeTreeInterfaceSocket &>(b);
      const bool is_output_a = sa.flag & NODE_INTERFACE_SOCKET_OUTPUT;
      const bool is_output_b = sb.flag & NODE_INTERFACE_SOCKET_OUTPUT;
      if ((sa.flag & NODE_INTERFACE_SOCKET_PANEL_TOGGLE) ||
          (sb.flag & NODE_INTERFACE_SOCKET_PANEL_TOGGLE))
      {
        /* Panel toggle inputs are allowed to be above outputs. */
        return false;
      }
      if (is_output_a && !is_output_b) {
        return true;
      }
    }
    return false;
  };

  int min_pos = 0;
  for (const int i : items.index_range()) {
    if (must_be_before(*items[i], item)) {
      min_pos = i + 1;
    }
  }
  int max_pos = items.size();
  for (const int i : items.index_range()) {
    if (must_be_before(item, *items[i])) {
      max_pos = i;
      break;
    }
  }
  BLI_assert(min_pos <= max_pos);
  return std::clamp(initial_pos, min_pos, max_pos);
}

void bNodeTreeInterfacePanel::add_item(bNodeTreeInterfaceItem &item)
{
  /* Same as inserting at the end. */
  insert_item(item, this->items_num);
}

void bNodeTreeInterfacePanel::insert_item(bNodeTreeInterfaceItem &item, int position)
{
  /* Apply any constraints on the item positions. */
  position = find_valid_insert_position_for_item(item, position);
  position = std::min(std::max(position, 0), items_num);

  blender::MutableSpan<bNodeTreeInterfaceItem *> old_items = this->items();
  items_num++;
  items_array = MEM_calloc_arrayN<bNodeTreeInterfaceItem *>(items_num, __func__);
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
  items_array = MEM_calloc_arrayN<bNodeTreeInterfaceItem *>(items_num, __func__);
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

const bNodeTreeInterfaceSocket *bNodeTreeInterfacePanel::header_toggle_socket() const
{
  if (this->items().is_empty()) {
    return nullptr;
  }
  const bNodeTreeInterfaceItem *first_item = this->items().first();
  if (first_item->item_type != NODE_INTERFACE_SOCKET) {
    return nullptr;
  }
  const auto &socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(first_item);
  if (!(socket.flag & NODE_INTERFACE_SOCKET_INPUT) ||
      !(socket.flag & NODE_INTERFACE_SOCKET_PANEL_TOGGLE))
  {
    return nullptr;
  }
  const blender::bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  if (!typeinfo || typeinfo->type != SOCK_BOOLEAN) {
    return nullptr;
  }
  return &socket;
}
bNodeTreeInterfaceSocket *bNodeTreeInterfacePanel::header_toggle_socket()
{
  return const_cast<bNodeTreeInterfaceSocket *>(
      const_cast<const bNodeTreeInterfacePanel *>(this)->header_toggle_socket());
}

namespace blender::bke::node_interface {

static bNodeTreeInterfaceSocket *make_socket(const int uid,
                                             const StringRef name,
                                             const StringRef description,
                                             const StringRef socket_type,
                                             const NodeTreeInterfaceSocketFlag flag)
{
  BLI_assert(!socket_type.is_empty());

  const std::optional<StringRef> idname = socket_types::try_get_supported_socket_type(socket_type);
  if (!idname) {
    return nullptr;
  }

  bNodeTreeInterfaceSocket *new_socket = MEM_callocN<bNodeTreeInterfaceSocket>(__func__);
  BLI_assert(new_socket);

  /* Init common socket properties. */
  new_socket->identifier = BLI_sprintfN("Socket_%d", uid);
  new_socket->item.item_type = NODE_INTERFACE_SOCKET;
  new_socket->name = BLI_strdupn(name.data(), name.size());
  new_socket->description = description.is_empty() ?
                                nullptr :
                                BLI_strdupn(description.data(), description.size());
  new_socket->socket_type = BLI_strdupn(socket_type.data(), socket_type.size());
  new_socket->flag = flag;

  new_socket->socket_data = socket_types::make_socket_data(socket_type);

  return new_socket;
}

bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                         const bNode &from_node,
                                                         const bNodeSocket &from_sock,
                                                         const StringRef socket_type,
                                                         const StringRef name)
{
  ntree.ensure_topology_cache();
  bNodeTreeInterfaceSocket *iosock = nullptr;
  if (from_node.is_group()) {
    if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(from_node.id)) {
      /* Copy interface socket directly from source group to avoid loosing data in the process. */
      group->ensure_interface_cache();
      const bNodeTreeInterfaceSocket &src_io_socket =
          from_sock.is_input() ? *group->interface_inputs()[from_sock.index()] :
                                 *group->interface_outputs()[from_sock.index()];
      iosock = reinterpret_cast<bNodeTreeInterfaceSocket *>(
          ntree.tree_interface.add_item_copy(src_io_socket.item, nullptr));
    }
  }
  if (!iosock) {
    NodeTreeInterfaceSocketFlag flag = NodeTreeInterfaceSocketFlag(0);
    SET_FLAG_FROM_TEST(flag, from_sock.in_out & SOCK_IN, NODE_INTERFACE_SOCKET_INPUT);
    SET_FLAG_FROM_TEST(flag, from_sock.in_out & SOCK_OUT, NODE_INTERFACE_SOCKET_OUTPUT);

    const nodes::SocketDeclaration *decl = from_sock.runtime->declaration;
    StringRef description = from_sock.description;
    if (decl) {
      if (!decl->description.empty()) {
        description = decl->description;
      }
      SET_FLAG_FROM_TEST(flag, decl->optional_label, NODE_INTERFACE_SOCKET_OPTIONAL_LABEL);
      if (socket_type == "NodeSocketMenu" && from_sock.type == SOCK_MENU) {
        if (const auto *menu_decl = dynamic_cast<const nodes::decl::Menu *>(decl)) {
          SET_FLAG_FROM_TEST(flag, menu_decl->is_expanded, NODE_INTERFACE_SOCKET_MENU_EXPANDED);
        }
      }
    }

    iosock = ntree.tree_interface.add_socket(name, description, socket_type, flag, nullptr);

    if (iosock) {
      if (decl) {
        iosock->default_input = decl->default_input_type;
      }
    }
  }
  if (iosock == nullptr) {
    return nullptr;
  }
  const blender::bke::bNodeSocketType *typeinfo = iosock->socket_typeinfo();
  if (typeinfo->interface_from_socket) {
    typeinfo->interface_from_socket(&ntree.id, iosock, &from_node, &from_sock);
  }
  return iosock;
}

static bNodeTreeInterfacePanel *make_panel(const int uid,
                                           const blender::StringRef name,
                                           const blender::StringRef description,
                                           const NodeTreeInterfacePanelFlag flag)
{
  BLI_assert(!name.is_empty());

  bNodeTreeInterfacePanel *new_panel = MEM_callocN<bNodeTreeInterfacePanel>(__func__);
  new_panel->item.item_type = NODE_INTERFACE_PANEL;
  new_panel->name = BLI_strdupn(name.data(), name.size());
  new_panel->description = description.is_empty() ?
                               nullptr :
                               BLI_strdupn(description.data(), description.size());
  new_panel->identifier = uid;
  new_panel->flag = flag;
  return new_panel;
}

}  // namespace blender::bke::node_interface

void bNodeTreeInterface::init_data()
{
  this->runtime = MEM_new<blender::bke::bNodeTreeInterfaceRuntime>(__func__);
  this->tag_missing_runtime_data();
}

void bNodeTreeInterface::copy_data(const bNodeTreeInterface &src, int flag)
{
  item_types::panel_init(this->root_panel, src.root_panel.items(), flag, nullptr);
  this->active_index = src.active_index;

  this->runtime = MEM_new<blender::bke::bNodeTreeInterfaceRuntime>(__func__);
  this->tag_missing_runtime_data();
}

void bNodeTreeInterface::free_data()
{
  MEM_delete(this->runtime);

  /* Called when freeing the main database, don't do user refcount here. */
  this->root_panel.clear(false);
}

void bNodeTreeInterface::write(BlendWriter *writer)
{
  /* Don't write the root panel struct itself, it's nested in the interface struct. */
  item_types::item_write_data(writer, this->root_panel.item);
}

void bNodeTreeInterface::read_data(BlendDataReader *reader)
{
  item_types::item_read_data(reader, this->root_panel.item);

  this->runtime = MEM_new<blender::bke::bNodeTreeInterfaceRuntime>(__func__);
  this->tag_missing_runtime_data();
}

bNodeTreeInterfaceItem *bNodeTreeInterface::active_item()
{
  bNodeTreeInterfaceItem *active = nullptr;
  int count = this->active_index;
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
  int count = this->active_index;
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
  this->active_index = 0;
  int count = 0;
  this->foreach_item([&](bNodeTreeInterfaceItem &titem) {
    if (&titem == item) {
      this->active_index = count;
      return false;
    }
    ++count;
    return true;
  });
}

bNodeTreeInterfaceSocket *bNodeTreeInterface::add_socket(const blender::StringRef name,
                                                         const blender::StringRef description,
                                                         const blender::StringRef socket_type,
                                                         const NodeTreeInterfaceSocketFlag flag,
                                                         bNodeTreeInterfacePanel *parent)
{
  /* Check that each interface socket is either an input or an output. Technically, it can be both
   * at the same time, but we don't want that for the time being. */
  BLI_assert(((NODE_INTERFACE_SOCKET_INPUT | NODE_INTERFACE_SOCKET_OUTPUT) & flag) !=
             (NODE_INTERFACE_SOCKET_INPUT | NODE_INTERFACE_SOCKET_OUTPUT));
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  bNodeTreeInterfaceSocket *new_socket = make_socket(
      this->next_uid++, name, description, socket_type, flag);
  if (new_socket) {
    parent->add_item(new_socket->item);
  }

  this->tag_items_changed();
  return new_socket;
}

bNodeTreeInterfaceSocket *bNodeTreeInterface::insert_socket(const blender::StringRef name,
                                                            const blender::StringRef description,
                                                            const blender::StringRef socket_type,
                                                            const NodeTreeInterfaceSocketFlag flag,
                                                            bNodeTreeInterfacePanel *parent,
                                                            const int position)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  bNodeTreeInterfaceSocket *new_socket = make_socket(
      this->next_uid++, name, description, socket_type, flag);
  if (new_socket) {
    parent->insert_item(new_socket->item, position);
  }

  this->tag_items_changed();
  return new_socket;
}

bNodeTreeInterfacePanel *bNodeTreeInterface::add_panel(const blender::StringRef name,
                                                       const blender::StringRef description,
                                                       const NodeTreeInterfacePanelFlag flag,
                                                       bNodeTreeInterfacePanel *parent)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  bNodeTreeInterfacePanel *new_panel = make_panel(this->next_uid++, name, description, flag);
  if (new_panel) {
    parent->add_item(new_panel->item);
  }

  this->tag_items_changed();
  return new_panel;
}

bNodeTreeInterfacePanel *bNodeTreeInterface::insert_panel(const blender::StringRef name,
                                                          const blender::StringRef description,
                                                          const NodeTreeInterfacePanelFlag flag,
                                                          bNodeTreeInterfacePanel *parent,
                                                          const int position)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  bNodeTreeInterfacePanel *new_panel = make_panel(this->next_uid++, name, description, flag);
  if (new_panel) {
    parent->insert_item(new_panel->item, position);
  }

  this->tag_items_changed();
  return new_panel;
}

bNodeTreeInterfaceItem *bNodeTreeInterface::add_item_copy(const bNodeTreeInterfaceItem &item,
                                                          bNodeTreeInterfacePanel *parent)
{
  if (parent == nullptr) {
    parent = &root_panel;
  }
  BLI_assert(this->find_item(parent->item));

  bNodeTreeInterfaceItem *citem = static_cast<bNodeTreeInterfaceItem *>(MEM_dupallocN(&item));
  item_types::item_copy(*citem, item, 0, [&]() { return this->next_uid++; });
  parent->add_item(*citem);

  this->tag_items_changed();
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

  bNodeTreeInterfaceItem *citem = static_cast<bNodeTreeInterfaceItem *>(MEM_dupallocN(&item));
  item_types::item_copy(*citem, item, 0, [&]() { return this->next_uid++; });
  parent->insert_item(*citem, position);

  this->tag_items_changed();
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
    this->tag_items_changed();
    return true;
  }

  return false;
}

void bNodeTreeInterface::clear_items()
{
  root_panel.clear(true);
  this->tag_items_changed();
}

bool bNodeTreeInterface::move_item(bNodeTreeInterfaceItem &item, const int new_position)
{
  bNodeTreeInterfacePanel *parent = this->find_item_parent(item, true);
  if (parent == nullptr) {
    return false;
  }

  if (parent->move_item(item, new_position)) {
    this->tag_items_changed();
    return true;
  }
  return false;
}

bool bNodeTreeInterface::move_item_to_parent(bNodeTreeInterfaceItem &item,
                                             bNodeTreeInterfacePanel *new_parent,
                                             int new_position)
{
  if (new_parent == nullptr) {
    new_parent = &this->root_panel;
  }

  if (item.item_type == NODE_INTERFACE_PANEL) {
    bNodeTreeInterfacePanel &src_item = reinterpret_cast<bNodeTreeInterfacePanel &>(item);
    if (src_item.contains_recursive(new_parent->item)) {
      return false;
    }
  }

  bNodeTreeInterfacePanel *parent = this->find_item_parent(item, true);
  if (parent == nullptr) {
    return false;
  }

  if (parent == new_parent) {
    if (parent->move_item(item, new_position)) {
      this->tag_items_changed();
      return true;
    }
  }
  else {
    /* NOTE: only remove and reinsert when parents different, otherwise removing the item can
     * change the desired target position! */
    if (parent->remove_item(item, false)) {
      new_parent->insert_item(item, new_position);
      this->tag_items_changed();
      return true;
    }
  }
  return false;
}

void bNodeTreeInterface::foreach_id(LibraryForeachIDData *cb)
{
  item_types::item_foreach_id(cb, root_panel.item);
}

bool bNodeTreeInterface::items_cache_is_available() const
{
  return !this->runtime->items_cache_mutex_.is_dirty();
}

void bNodeTreeInterface::ensure_items_cache() const
{
  blender::bke::bNodeTreeInterfaceRuntime &runtime = *this->runtime;

  runtime.items_cache_mutex_.ensure([&]() {
    /* Rebuild draw-order list of interface items for linear access. */
    runtime.items_.clear();
    runtime.inputs_.clear();
    runtime.outputs_.clear();

    /* Items in the cache are mutable pointers, but node tree update considers ID data to be
     * immutable when caching. DNA ListBase pointers can be mutable even if their container is
     * const, but the items returned by #foreach_item inherit qualifiers from the container. */
    bNodeTreeInterface &mutable_self = const_cast<bNodeTreeInterface &>(*this);

    mutable_self.foreach_item([&](bNodeTreeInterfaceItem &item) {
      runtime.items_.add_new(&item);
      if (bNodeTreeInterfaceSocket *socket = get_item_as<bNodeTreeInterfaceSocket>(&item)) {
        if (socket->flag & NODE_INTERFACE_SOCKET_INPUT) {
          runtime.inputs_.add_new(socket);
        }
        if (socket->flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          runtime.outputs_.add_new(socket);
        }
      }
      return true;
    });
  });
}

void bNodeTreeInterface::tag_interface_changed()
{
  this->runtime->interface_changed_.store(true);
}

bool bNodeTreeInterface::requires_dependent_tree_updates() const
{
  return this->runtime->interface_changed_.load(std::memory_order_relaxed);
}

void bNodeTreeInterface::tag_items_changed()
{
  this->tag_interface_changed();
  this->runtime->items_cache_mutex_.tag_dirty();
}

void bNodeTreeInterface::tag_items_changed_generic()
{
  /* Perform a full update since we don't know what changed exactly. */
  this->tag_items_changed();
}

void bNodeTreeInterface::tag_item_property_changed()
{
  this->tag_interface_changed();
}

void bNodeTreeInterface::tag_missing_runtime_data()
{
  this->tag_items_changed();
}

void bNodeTreeInterface::reset_interface_changed()
{
  this->runtime->interface_changed_.store(false);
}
