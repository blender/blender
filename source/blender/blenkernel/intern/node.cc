/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_color.hh"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_path_util.h"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_threads.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"
#include "BLT_translation.hh"

#include "IMB_imbuf.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_asset.hh"
#include "BKE_bpath.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_cryptomatte.h"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_image_format.h"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_anonymous_attributes.hh"
#include "BKE_node_tree_interface.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_preview_image.hh"
#include "BKE_type_conversions.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "NOD_common.h"
#include "NOD_composite.hh"
#include "NOD_geo_bake.hh"
#include "NOD_geo_capture_attribute.hh"
#include "NOD_geo_index_switch.hh"
#include "NOD_geo_menu_switch.hh"
#include "NOD_geo_repeat.hh"
#include "NOD_geo_simulation.hh"
#include "NOD_geometry.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"
#include "NOD_register.hh"
#include "NOD_shader.h"
#include "NOD_socket.hh"
#include "NOD_texture.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "BLO_read_write.hh"

#define NODE_DEFAULT_MAX_WIDTH 700

using blender::Array;
using blender::Map;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::Stack;
using blender::StringRef;
using blender::Vector;
using blender::VectorSet;
using blender::bke::bNodeRuntime;
using blender::bke::bNodeSocketRuntime;
using blender::bke::bNodeTreeRuntime;
using blender::nodes::FieldInferencingInterface;
using blender::nodes::InputSocketFieldType;
using blender::nodes::NodeDeclaration;
using blender::nodes::OutputFieldDependency;
using blender::nodes::OutputSocketFieldType;
using blender::nodes::SocketDeclaration;

static CLG_LogRef LOG = {"bke.node"};

namespace blender::bke {

/* Forward declaration. */
static void write_node_socket_default_value(BlendWriter *writer, const bNodeSocket *sock);

/* Fallback types for undefined tree, nodes, sockets. */
bNodeTreeType NodeTreeTypeUndefined;
bNodeType NodeTypeUndefined;
bNodeSocketType NodeSocketTypeUndefined;

static void ntree_set_typeinfo(bNodeTree *ntree, bNodeTreeType *typeinfo);
static void node_socket_set_typeinfo(bNodeTree *ntree,
                                     bNodeSocket *sock,
                                     bNodeSocketType *typeinfo);
static void node_socket_copy(bNodeSocket *sock_dst, const bNodeSocket *sock_src, const int flag);
static void free_localized_node_groups(bNodeTree *ntree);
static bool socket_id_user_decrement(bNodeSocket *sock);

static void ntree_init_data(ID *id)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  ntree->tree_interface.init_data();
  ntree->runtime = MEM_new<bNodeTreeRuntime>(__func__);
  ntree_set_typeinfo(ntree, nullptr);
}

static void ntree_copy_data(Main * /*bmain*/,
                            std::optional<Library *> /*owner_library*/,
                            ID *id_dst,
                            const ID *id_src,
                            const int flag)
{
  bNodeTree *ntree_dst = reinterpret_cast<bNodeTree *>(id_dst);
  const bNodeTree *ntree_src = reinterpret_cast<const bNodeTree *>(id_src);

  /* We never handle user-count here for owned data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  ntree_dst->runtime = MEM_new<bNodeTreeRuntime>(__func__);
  bNodeTreeRuntime &dst_runtime = *ntree_dst->runtime;

  Map<const bNodeSocket *, bNodeSocket *> socket_map;

  dst_runtime.nodes_by_id.reserve(ntree_src->all_nodes().size());
  BLI_listbase_clear(&ntree_dst->nodes);
  int i;
  LISTBASE_FOREACH_INDEX (const bNode *, src_node, &ntree_src->nodes, i) {
    /* Don't find a unique name for every node, since they should have valid names already. */
    bNode *new_node = node_copy_with_mapping(
        ntree_dst, *src_node, flag_subdata, false, socket_map);
    dst_runtime.nodes_by_id.add_new(new_node);
    new_node->runtime->index_in_tree = i;
  }

  /* copy links */
  BLI_listbase_clear(&ntree_dst->links);
  LISTBASE_FOREACH (const bNodeLink *, src_link, &ntree_src->links) {
    bNodeLink *dst_link = static_cast<bNodeLink *>(MEM_dupallocN(src_link));
    dst_link->fromnode = dst_runtime.nodes_by_id.lookup_key_as(src_link->fromnode->identifier);
    dst_link->fromsock = socket_map.lookup(src_link->fromsock);
    dst_link->tonode = dst_runtime.nodes_by_id.lookup_key_as(src_link->tonode->identifier);
    dst_link->tosock = socket_map.lookup(src_link->tosock);
    BLI_assert(dst_link->tosock);
    dst_link->tosock->link = dst_link;
    BLI_addtail(&ntree_dst->links, dst_link);
  }

  /* update node->parent pointers */
  for (bNode *node : ntree_dst->all_nodes()) {
    if (node->parent) {
      node->parent = dst_runtime.nodes_by_id.lookup_key_as(node->parent->identifier);
    }
  }

  for (bNode *node : ntree_dst->all_nodes()) {
    nodeDeclarationEnsure(ntree_dst, node);
  }

  ntree_dst->tree_interface.copy_data(ntree_src->tree_interface, flag);
  /* copy preview hash */
  if (ntree_src->previews && (flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    bNodeInstanceHashIterator iter;

    ntree_dst->previews = BKE_node_instance_hash_new("node previews");

    NODE_INSTANCE_HASH_ITER (iter, ntree_src->previews) {
      bNodeInstanceKey key = node_instance_hash_iterator_get_key(&iter);
      bNodePreview *preview = static_cast<bNodePreview *>(
          node_instance_hash_iterator_get_value(&iter));
      BKE_node_instance_hash_insert(ntree_dst->previews, key, node_preview_copy(preview));
    }
  }
  else {
    ntree_dst->previews = nullptr;
  }

  if (ntree_src->runtime->field_inferencing_interface) {
    dst_runtime.field_inferencing_interface = std::make_unique<FieldInferencingInterface>(
        *ntree_src->runtime->field_inferencing_interface);
  }
  if (ntree_src->runtime->anonymous_attribute_inferencing) {
    using namespace anonymous_attribute_inferencing;
    dst_runtime.anonymous_attribute_inferencing =
        std::make_unique<AnonymousAttributeInferencingResult>(
            *ntree_src->runtime->anonymous_attribute_inferencing);
    for (FieldSource &field_source :
         dst_runtime.anonymous_attribute_inferencing->all_field_sources)
    {
      if (auto *socket_field_source = std::get_if<SocketFieldSource>(&field_source.data)) {
        socket_field_source->socket = socket_map.lookup(socket_field_source->socket);
      }
    }
    for (GeometrySource &geometry_source :
         dst_runtime.anonymous_attribute_inferencing->all_geometry_sources)
    {
      if (auto *socket_geometry_source = std::get_if<SocketGeometrySource>(&geometry_source.data))
      {
        socket_geometry_source->socket = socket_map.lookup(socket_geometry_source->socket);
      }
    }
  }

  if (ntree_src->geometry_node_asset_traits) {
    ntree_dst->geometry_node_asset_traits = MEM_new<GeometryNodeAssetTraits>(
        __func__, *ntree_src->geometry_node_asset_traits);
  }

  if (ntree_src->nested_node_refs) {
    ntree_dst->nested_node_refs = static_cast<bNestedNodeRef *>(
        MEM_malloc_arrayN(ntree_src->nested_node_refs_num, sizeof(bNestedNodeRef), __func__));
    uninitialized_copy_n(
        ntree_src->nested_node_refs, ntree_src->nested_node_refs_num, ntree_dst->nested_node_refs);
  }

  if (flag & LIB_ID_COPY_NO_PREVIEW) {
    ntree_dst->preview = nullptr;
  }
  else {
    BKE_previewimg_id_copy(&ntree_dst->id, &ntree_src->id);
  }

  ntree_dst->description = BLI_strdup_null(ntree_src->description);
}

static void ntree_free_data(ID *id)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  /* XXX hack! node trees should not store execution graphs at all.
   * This should be removed when old tree types no longer require it.
   * Currently the execution data for texture nodes remains in the tree
   * after execution, until the node tree is updated or freed. */
  if (ntree->runtime->execdata) {
    switch (ntree->type) {
      case NTREE_SHADER:
        ntreeShaderEndExecTree(ntree->runtime->execdata);
        break;
      case NTREE_TEXTURE:
        ntreeTexEndExecTree(ntree->runtime->execdata);
        ntree->runtime->execdata = nullptr;
        break;
    }
  }

  /* XXX not nice, but needed to free localized node groups properly */
  free_localized_node_groups(ntree);

  BLI_freelistN(&ntree->links);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    node_free_node(ntree, node);
  }

  ntree->tree_interface.free_data();

  /* free preview hash */
  if (ntree->previews) {
    BKE_node_instance_hash_free(ntree->previews, (bNodeInstanceValueFP)node_preview_free);
  }

  if (ntree->id.tag & LIB_TAG_LOCALIZED) {
    BKE_libblock_free_data(&ntree->id, true);
  }

  MEM_delete(ntree->geometry_node_asset_traits);

  if (ntree->nested_node_refs) {
    MEM_freeN(ntree->nested_node_refs);
  }

  MEM_SAFE_FREE(ntree->description);
  BKE_previewimg_free(&ntree->preview);
  MEM_delete(ntree->runtime);
}

static void library_foreach_node_socket(bNodeSocket *sock, LibraryForeachIDData *data)
{
  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
      data, IDP_foreach_property(sock->prop, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
        BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
      }));

  switch (eNodeSocketDatatype(sock->type)) {
    case SOCK_OBJECT: {
      bNodeSocketValueObject &default_value = *sock->default_value_typed<bNodeSocketValueObject>();
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, default_value.value, IDWALK_CB_USER);
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage &default_value = *sock->default_value_typed<bNodeSocketValueImage>();
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, default_value.value, IDWALK_CB_USER);
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection &default_value =
          *sock->default_value_typed<bNodeSocketValueCollection>();
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, default_value.value, IDWALK_CB_USER);
      break;
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture &default_value =
          *sock->default_value_typed<bNodeSocketValueTexture>();
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, default_value.value, IDWALK_CB_USER);
      break;
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial &default_value =
          *sock->default_value_typed<bNodeSocketValueMaterial>();
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, default_value.value, IDWALK_CB_USER);
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_MATRIX:
    case SOCK_INT:
    case SOCK_STRING:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
    case SOCK_MENU:
      break;
  }
}

void node_node_foreach_id(bNode *node, LibraryForeachIDData *data)
{
  BKE_LIB_FOREACHID_PROCESS_ID(data, node->id, IDWALK_CB_USER);

  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
      data, IDP_foreach_property(node->prop, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
        BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
      }));
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, library_foreach_node_socket(sock, data));
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, library_foreach_node_socket(sock, data));
  }

  /* Note that this ID pointer is only a cache, it may be outdated. */
  BKE_LIB_FOREACHID_PROCESS_ID(data, node->runtime->owner_tree, IDWALK_CB_LOOPBACK);
}

static void node_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  BKE_LIB_FOREACHID_PROCESS_ID(
      data,
      ntree->owner_id,
      (IDWALK_CB_LOOPBACK | IDWALK_CB_NEVER_SELF | IDWALK_CB_READFILE_IGNORE));

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, ntree->gpd, IDWALK_CB_USER);

  for (bNode *node : ntree->all_nodes()) {
    node_node_foreach_id(node, data);
  }

  ntree->tree_interface.foreach_id(data);
}

static void node_foreach_cache(ID *id,
                               IDTypeForeachCacheFunctionCallback function_callback,
                               void *user_data)
{
  bNodeTree *nodetree = reinterpret_cast<bNodeTree *>(id);
  IDCacheKey key = {0};
  key.id_session_uid = id->session_uid;
  key.identifier = offsetof(bNodeTree, previews);

  /* TODO: see also `direct_link_nodetree()` in `readfile.cc`. */
#if 0
  function_callback(id, &key, static_cast<void **>(&nodetree->previews), 0, user_data);
#endif

  if (nodetree->type == NTREE_COMPOSIT) {
    for (bNode *node : nodetree->all_nodes()) {
      if (node->type == CMP_NODE_MOVIEDISTORTION) {
        key.identifier = size_t(BLI_ghashutil_strhash_p(node->name));
        function_callback(id, &key, static_cast<void **>(&node->storage), 0, user_data);
      }
    }
  }
}

static void node_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  switch (ntree->type) {
    case NTREE_SHADER: {
      for (bNode *node : ntree->all_nodes()) {
        if (node->type == SH_NODE_SCRIPT) {
          NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);
          BKE_bpath_foreach_path_fixed_process(bpath_data, nss->filepath, sizeof(nss->filepath));
        }
        else if (node->type == SH_NODE_TEX_IES) {
          NodeShaderTexIES *ies = static_cast<NodeShaderTexIES *>(node->storage);
          BKE_bpath_foreach_path_fixed_process(bpath_data, ies->filepath, sizeof(ies->filepath));
        }
      }
      break;
    }
    default:
      break;
  }
}

static ID **node_owner_pointer_get(ID *id, const bool debug_relationship_assert)
{
  if ((id->flag & LIB_EMBEDDED_DATA) == 0) {
    return nullptr;
  }
  /* TODO: Sort this NO_MAIN or not for embedded node trees. See #86119. */
  // BLI_assert((id->tag & LIB_TAG_NO_MAIN) == 0);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  if (debug_relationship_assert) {
    BLI_assert(ntree->owner_id != nullptr);
    BLI_assert(ntreeFromID(ntree->owner_id) == ntree);
  }

  return &ntree->owner_id;
}

namespace forward_compat {

static void write_node_socket_interface(BlendWriter *writer, const bNodeSocket *sock)
{
  BLO_write_struct(writer, bNodeSocket, sock);

  if (sock->prop) {
    IDP_BlendWrite(writer, sock->prop);
  }

  BLO_write_string(writer, sock->default_attribute_name);

  write_node_socket_default_value(writer, sock);
}

/* Construct a bNodeSocket that represents a node group socket the old way. */
static bNodeSocket *make_socket(bNodeTree *ntree,
                                const eNodeSocketInOut in_out,
                                const StringRef idname,

                                const StringRef name,
                                const StringRef identifier)
{
  bNodeSocketType *stype = nodeSocketTypeFind(idname.data());
  if (stype == nullptr) {
    return nullptr;
  }

  bNodeSocket *sock = MEM_cnew<bNodeSocket>(__func__);
  sock->runtime = MEM_new<bNodeSocketRuntime>(__func__);
  STRNCPY(sock->idname, stype->idname);
  sock->in_out = int(in_out);
  sock->type = int(SOCK_CUSTOM); /* int type undefined by default */
  node_socket_set_typeinfo(ntree, sock, stype);

  sock->limit = (in_out == SOCK_IN ? 1 : 0xFFF);

  STRNCPY(sock->identifier, identifier.data());
  STRNCPY(sock->name, name.data());
  sock->storage = nullptr;
  sock->flag |= SOCK_COLLAPSED;

  return sock;
}

/* Include the subtype suffix for old socket idnames. */
static StringRef get_legacy_socket_subtype_idname(StringRef idname, const void *socket_data)
{
  if (idname == "NodeSocketFloat") {
    const bNodeSocketValueFloat &float_data = *static_cast<const bNodeSocketValueFloat *>(
        socket_data);
    switch (float_data.subtype) {
      case PROP_UNSIGNED:
        return "NodeSocketFloatUnsigned";
      case PROP_PERCENTAGE:
        return "NodeSocketFloatPercentage";
      case PROP_FACTOR:
        return "NodeSocketFloatFactor";
      case PROP_ANGLE:
        return "NodeSocketFloatAngle";
      case PROP_TIME:
        return "NodeSocketFloatTime";
      case PROP_TIME_ABSOLUTE:
        return "NodeSocketFloatTimeAbsolute";
      case PROP_DISTANCE:
        return "NodeSocketFloatDistance";
      case PROP_WAVELENGTH:
        return "NodeSocketFloatWavelength";
    }
  }
  if (idname == "NodeSocketInt") {
    const bNodeSocketValueInt &int_data = *static_cast<const bNodeSocketValueInt *>(socket_data);
    switch (int_data.subtype) {
      case PROP_UNSIGNED:
        return "NodeSocketIntUnsigned";
      case PROP_PERCENTAGE:
        return "NodeSocketIntPercentage";
      case PROP_FACTOR:
        return "NodeSocketIntFactor";
    }
  }
  if (idname == "NodeSocketVector") {
    const bNodeSocketValueVector &vector_data = *static_cast<const bNodeSocketValueVector *>(
        socket_data);
    switch (vector_data.subtype) {
      case PROP_TRANSLATION:
        return "NodeSocketVectorTranslation";
      case PROP_DIRECTION:
        return "NodeSocketVectorDirection";
      case PROP_VELOCITY:
        return "NodeSocketVectorVelocity";
      case PROP_ACCELERATION:
        return "NodeSocketVectorAcceleration";
      case PROP_EULER:
        return "NodeSocketVectorEuler";
    }
  }
  return idname;
}

/**
 * Socket interface reconstruction for forward compatibility.
 * To enable previous Blender versions to read the new interface DNA data,
 * construct the bNodeSocket inputs/outputs lists.
 * This discards any information about panels and alternating input/output order,
 * but all functional information is preserved for executing node trees.
 */
static void construct_interface_as_legacy_sockets(bNodeTree *ntree)
{
  BLI_assert(BLI_listbase_is_empty(&ntree->inputs_legacy));
  BLI_assert(BLI_listbase_is_empty(&ntree->outputs_legacy));

  auto make_legacy_socket = [&](const bNodeTreeInterfaceSocket &socket,
                                eNodeSocketInOut in_out) -> bNodeSocket * {
    bNodeSocket *iosock = make_socket(
        ntree,
        in_out,
        get_legacy_socket_subtype_idname(socket.socket_type, socket.socket_data),
        socket.name ? socket.name : "",
        socket.identifier);
    if (!iosock) {
      return nullptr;
    }

    if (socket.description) {
      STRNCPY(iosock->description, socket.description);
    }
    node_socket_copy_default_value_data(
        eNodeSocketDatatype(iosock->typeinfo->type), iosock->default_value, socket.socket_data);
    if (socket.properties) {
      iosock->prop = IDP_CopyProperty(socket.properties);
    }
    SET_FLAG_FROM_TEST(
        iosock->flag, socket.flag & NODE_INTERFACE_SOCKET_HIDE_VALUE, SOCK_HIDE_VALUE);
    SET_FLAG_FROM_TEST(
        iosock->flag, socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER, SOCK_HIDE_IN_MODIFIER);
    iosock->attribute_domain = socket.attribute_domain;
    iosock->default_attribute_name = BLI_strdup_null(socket.default_attribute_name);
    return iosock;
  };

  /* Construct inputs/outputs socket lists in the node tree. */
  ntree->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    if (const bNodeTreeInterfaceSocket *socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(&item))
    {
      if (socket->flag & NODE_INTERFACE_SOCKET_INPUT) {
        if (bNodeSocket *legacy_socket = make_legacy_socket(*socket, SOCK_IN)) {
          BLI_addtail(&ntree->inputs_legacy, legacy_socket);
        }
      }
      if (socket->flag & NODE_INTERFACE_SOCKET_OUTPUT) {
        if (bNodeSocket *legacy_socket = make_legacy_socket(*socket, SOCK_OUT)) {
          BLI_addtail(&ntree->outputs_legacy, legacy_socket);
        }
      }
    }
    return true;
  });
}

static void write_legacy_sockets(BlendWriter *writer, bNodeTree *ntree)
{
  /* Write inputs/outputs */
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->inputs_legacy) {
    write_node_socket_interface(writer, sock);
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->outputs_legacy) {
    write_node_socket_interface(writer, sock);
  }
}

static void legacy_socket_interface_free(bNodeSocket *sock)
{
  if (sock->prop) {
    IDP_FreeProperty_ex(sock->prop, false);
  }

  if (sock->default_value) {
    MEM_freeN(sock->default_value);
  }
  if (sock->default_attribute_name) {
    MEM_freeN(sock->default_attribute_name);
  }
  MEM_delete(sock->runtime);
}

static void cleanup_legacy_sockets(bNodeTree *ntree)
{
  /* Clean up temporary inputs/outputs. */
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, socket, &ntree->inputs_legacy) {
    legacy_socket_interface_free(socket);
    MEM_freeN(socket);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, socket, &ntree->outputs_legacy) {
    legacy_socket_interface_free(socket);
    MEM_freeN(socket);
  }
  BLI_listbase_clear(&ntree->inputs_legacy);
  BLI_listbase_clear(&ntree->outputs_legacy);
}

}  // namespace forward_compat

static void write_node_socket_default_value(BlendWriter *writer, const bNodeSocket *sock)
{
  if (sock->default_value == nullptr) {
    return;
  }

  switch (eNodeSocketDatatype(sock->type)) {
    case SOCK_FLOAT:
      BLO_write_struct(writer, bNodeSocketValueFloat, sock->default_value);
      break;
    case SOCK_VECTOR:
      BLO_write_struct(writer, bNodeSocketValueVector, sock->default_value);
      break;
    case SOCK_RGBA:
      BLO_write_struct(writer, bNodeSocketValueRGBA, sock->default_value);
      break;
    case SOCK_BOOLEAN:
      BLO_write_struct(writer, bNodeSocketValueBoolean, sock->default_value);
      break;
    case SOCK_INT:
      BLO_write_struct(writer, bNodeSocketValueInt, sock->default_value);
      break;
    case SOCK_STRING:
      BLO_write_struct(writer, bNodeSocketValueString, sock->default_value);
      break;
    case SOCK_OBJECT:
      BLO_write_struct(writer, bNodeSocketValueObject, sock->default_value);
      break;
    case SOCK_IMAGE:
      BLO_write_struct(writer, bNodeSocketValueImage, sock->default_value);
      break;
    case SOCK_COLLECTION:
      BLO_write_struct(writer, bNodeSocketValueCollection, sock->default_value);
      break;
    case SOCK_TEXTURE:
      BLO_write_struct(writer, bNodeSocketValueTexture, sock->default_value);
      break;
    case SOCK_MATERIAL:
      BLO_write_struct(writer, bNodeSocketValueMaterial, sock->default_value);
      break;
    case SOCK_ROTATION:
      BLO_write_struct(writer, bNodeSocketValueRotation, sock->default_value);
      break;
    case SOCK_MENU:
      BLO_write_struct(writer, bNodeSocketValueMenu, sock->default_value);
      break;
    case SOCK_MATRIX:
      /* Matrix sockets currently have no default value. */
      break;
    case SOCK_CUSTOM:
      /* Custom node sockets where default_value is defined uses custom properties for storage. */
      break;
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      BLI_assert_unreachable();
      break;
  }
}

static void write_node_socket(BlendWriter *writer, const bNodeSocket *sock)
{
  BLO_write_struct(writer, bNodeSocket, sock);

  if (sock->prop) {
    IDP_BlendWrite(writer, sock->prop);
  }

  /* This property should only be used for group node "interface" sockets. */
  BLI_assert(sock->default_attribute_name == nullptr);

  write_node_socket_default_value(writer, sock);
}

void ntreeBlendWrite(BlendWriter *writer, bNodeTree *ntree)
{
  BKE_id_blend_write(writer, &ntree->id);
  BLO_write_string(writer, ntree->description);

  for (bNode *node : ntree->all_nodes()) {
    if (ntree->type == NTREE_SHADER && node->type == SH_NODE_BSDF_HAIR_PRINCIPLED) {
      /* For Principeld Hair BSDF, also write to `node->custom1` for forward compatibility, because
       * prior to 4.0 `node->custom1` was used for color parametrization instead of
       * `node->storage->parametrization`. */
      NodeShaderHairPrincipled *data = static_cast<NodeShaderHairPrincipled *>(node->storage);
      node->custom1 = data->parametrization;
    }

    BLO_write_struct(writer, bNode, node);

    if (node->prop) {
      IDP_BlendWrite(writer, node->prop);
    }

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      write_node_socket(writer, sock);
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      write_node_socket(writer, sock);
    }
    BLO_write_struct_array(
        writer, bNodePanelState, node->num_panel_states, node->panel_states_array);

    if (node->storage) {
      if (ELEM(ntree->type, NTREE_SHADER, NTREE_GEOMETRY) &&
          ELEM(node->type, SH_NODE_CURVE_VEC, SH_NODE_CURVE_RGB, SH_NODE_CURVE_FLOAT))
      {
        BKE_curvemapping_blend_write(writer, static_cast<const CurveMapping *>(node->storage));
      }
      else if (ntree->type == NTREE_SHADER && (node->type == SH_NODE_SCRIPT)) {
        NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);
        if (nss->bytecode) {
          BLO_write_string(writer, nss->bytecode);
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) && ELEM(node->type,
                                                       CMP_NODE_TIME,
                                                       CMP_NODE_CURVE_VEC,
                                                       CMP_NODE_CURVE_RGB,
                                                       CMP_NODE_HUECORRECT))
      {
        BKE_curvemapping_blend_write(writer, static_cast<const CurveMapping *>(node->storage));
      }
      else if ((ntree->type == NTREE_TEXTURE) &&
               ELEM(node->type, TEX_NODE_CURVE_RGB, TEX_NODE_CURVE_TIME))
      {
        BKE_curvemapping_blend_write(writer, static_cast<const CurveMapping *>(node->storage));
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_MOVIEDISTORTION)) {
        /* pass */
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_GLARE)) {
        /* Simple forward compatibility for fix for #50736.
         * Not ideal (there is no ideal solution here), but should do for now. */
        NodeGlare *ndg = static_cast<NodeGlare *>(node->storage);
        /* Not in undo case. */
        if (!BLO_write_is_undo(writer)) {
          switch (ndg->type) {
            case CMP_NODE_GLARE_STREAKS:
              ndg->angle = ndg->streaks;
              break;
            case CMP_NODE_GLARE_SIMPLE_STAR:
              ndg->angle = ndg->star_45;
              break;
            default:
              break;
          }
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) &&
               ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY))
      {
        NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
        BLO_write_string(writer, nc->matte_id);
        LISTBASE_FOREACH (CryptomatteEntry *, entry, &nc->entries) {
          BLO_write_struct(writer, CryptomatteEntry, entry);
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
      else if (node->type == FN_NODE_INPUT_STRING) {
        NodeInputString *storage = static_cast<NodeInputString *>(node->storage);
        if (storage->string) {
          BLO_write_string(writer, storage->string);
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, storage);
      }
      else if (node->type == GEO_NODE_CAPTURE_ATTRIBUTE) {
        auto &storage = *static_cast<NodeGeometryAttributeCapture *>(node->storage);
        /* Improve forward compatibility. */
        storage.data_type_legacy = CD_PROP_FLOAT;
        for (const NodeGeometryAttributeCaptureItem &item :
             Span{storage.capture_items, storage.capture_items_num})
        {
          if (item.identifier == 0) {
            /* The sockets of this item have the same identifiers that have been used by older
             * Blender versions before the node supported capturing multiple attributes. */
            storage.data_type_legacy = item.data_type;
            break;
          }
        }
        BLO_write_struct(writer, NodeGeometryAttributeCapture, node->storage);
        nodes::CaptureAttributeItemsAccessor::blend_write(writer, *node);
      }
      else if (node->typeinfo != &NodeTypeUndefined) {
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
    }

    if (node->type == CMP_NODE_OUTPUT_FILE) {
      /* Inputs have their own storage data. */
      NodeImageMultiFile *nimf = (NodeImageMultiFile *)node->storage;
      BKE_image_format_blend_write(writer, &nimf->format);

      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        NodeImageMultiFileSocket *sockdata = static_cast<NodeImageMultiFileSocket *>(
            sock->storage);
        BLO_write_struct(writer, NodeImageMultiFileSocket, sockdata);
        BKE_image_format_blend_write(writer, &sockdata->format);
      }
    }
    if (ELEM(node->type, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS)) {
      /* Write extra socket info. */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        BLO_write_struct(writer, NodeImageLayer, sock->storage);
      }
    }
    if (node->type == GEO_NODE_SIMULATION_OUTPUT) {
      nodes::SimulationItemsAccessor::blend_write(writer, *node);
    }
    if (node->type == GEO_NODE_REPEAT_OUTPUT) {
      nodes::RepeatItemsAccessor::blend_write(writer, *node);
    }
    if (node->type == GEO_NODE_INDEX_SWITCH) {
      nodes::IndexSwitchItemsAccessor::blend_write(writer, *node);
    }
    if (node->type == GEO_NODE_BAKE) {
      nodes::BakeItemsAccessor::blend_write(writer, *node);
    }
    if (node->type == GEO_NODE_MENU_SWITCH) {
      nodes::MenuSwitchItemsAccessor::blend_write(writer, *node);
    }
  }

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    BLO_write_struct(writer, bNodeLink, link);
  }

  ntree->tree_interface.write(writer);
  if (!BLO_write_is_undo(writer)) {
    forward_compat::write_legacy_sockets(writer, ntree);
  }

  BLO_write_struct(writer, GeometryNodeAssetTraits, ntree->geometry_node_asset_traits);

  BLO_write_struct_array(
      writer, bNestedNodeRef, ntree->nested_node_refs_num, ntree->nested_node_refs);

  BKE_previewimg_blend_write(writer, ntree->preview);
}

static void ntree_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  ntree->typeinfo = nullptr;
  ntree->runtime->execdata = nullptr;

  if (!BLO_write_is_undo(writer)) {
    /* Generate legacy inputs/outputs socket ListBase for forward compatibility.
     * NOTE: this has to happen before writing the ntree struct itself so that the ListBase
     * first/last pointers are valid. */
    forward_compat::construct_interface_as_legacy_sockets(ntree);
  }

  BLO_write_id_struct(writer, bNodeTree, id_address, &ntree->id);

  ntreeBlendWrite(writer, ntree);

  if (!BLO_write_is_undo(writer)) {
    forward_compat::cleanup_legacy_sockets(ntree);
  }
}

/**
 * Sockets with default_value data must be known built-in types, otherwise reading and writing data
 * correctly cannot be guaranteed. Discard any socket with default_value data that has an unknown
 * type.
 */
static bool is_node_socket_supported(const bNodeSocket *sock)
{
  switch (eNodeSocketDatatype(sock->type)) {
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_INT:
    case SOCK_STRING:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
    case SOCK_OBJECT:
    case SOCK_IMAGE:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_MATERIAL:
    case SOCK_ROTATION:
    case SOCK_MENU:
    case SOCK_MATRIX:
      return true;
  }
  return false;
}

static void direct_link_node_socket(BlendDataReader *reader, bNodeSocket *sock)
{
  BLO_read_struct(reader, IDProperty, &sock->prop);
  IDP_BlendDataRead(reader, &sock->prop);

  BLO_read_struct(reader, bNodeLink, &sock->link);
  sock->typeinfo = nullptr;
  BLO_read_data_address(reader, &sock->storage);
  BLO_read_data_address(reader, &sock->default_value);
  BLO_read_string(reader, &sock->default_attribute_name);
  sock->runtime = MEM_new<bNodeSocketRuntime>(__func__);

  switch (eNodeSocketDatatype(sock->type)) {
    case SOCK_MENU: {
      bNodeSocketValueMenu &default_value = *sock->default_value_typed<bNodeSocketValueMenu>();
      /* Clear runtime data. */
      default_value.enum_items = nullptr;
      default_value.runtime_flag = 0;
      break;
    }
    default:
      break;
  }
}

static void remove_unsupported_sockets(ListBase *sockets, ListBase *links)
{
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, sockets) {
    if (is_node_socket_supported(sock)) {
      continue;
    }

    /* First remove any link pointing to the socket. */
    if (links) {
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, links) {
        if (link->fromsock == sock || link->tosock == sock) {
          BLI_remlink(links, link);
          if (link->tosock) {
            link->tosock->link = nullptr;
          }
          MEM_freeN(link);
        }
      }
    }

    BLI_remlink(sockets, sock);
    MEM_delete(sock->runtime);
    MEM_freeN(sock);
  }
}

void ntreeBlendReadData(BlendDataReader *reader, ID *owner_id, bNodeTree *ntree)
{
  /* Special case for this pointer, do not rely on regular `lib_link` process here. Avoids needs
   * for do_versioning, and ensures coherence of data in any case.
   *
   * NOTE: Old versions are very often 'broken' here, just fix it silently in these cases.
   */
  if (BLO_read_fileversion_get(reader) > 300) {
    BLI_assert((ntree->id.flag & LIB_EMBEDDED_DATA) != 0 || owner_id == nullptr);
  }
  BLI_assert(owner_id == nullptr || owner_id->lib == ntree->id.lib);
  if (owner_id != nullptr && (ntree->id.flag & LIB_EMBEDDED_DATA) == 0) {
    /* This is unfortunate, but currently a lot of existing files (including startup ones) have
     * missing `LIB_EMBEDDED_DATA` flag.
     *
     * NOTE: Using do_version is not a solution here, since this code will be called before any
     * do_version takes place. Keeping it here also ensures future (or unknown existing) similar
     * bugs won't go easily unnoticed. */
    if (BLO_read_fileversion_get(reader) > 300) {
      CLOG_WARN(&LOG,
                "Fixing root node tree '%s' owned by '%s' missing EMBEDDED tag, please consider "
                "re-saving your (startup) file",
                ntree->id.name,
                owner_id->name);
    }
    ntree->id.flag |= LIB_EMBEDDED_DATA;
  }
  ntree->owner_id = owner_id;

  /* NOTE: writing and reading goes in sync, for speed. */
  ntree->typeinfo = nullptr;

  ntree->runtime = MEM_new<bNodeTreeRuntime>(__func__);
  BKE_ntree_update_tag_missing_runtime_data(ntree);

  BLO_read_string(reader, &ntree->description);

  BLO_read_struct_list(reader, bNode, &ntree->nodes);
  int i;
  LISTBASE_FOREACH_INDEX (bNode *, node, &ntree->nodes, i) {
    node->runtime = MEM_new<bNodeRuntime>(__func__);
    node->typeinfo = nullptr;
    node->runtime->index_in_tree = i;

    /* Create the `nodes_by_id` cache eagerly so it can be expected to be valid. Because
     * we create it here we also have to check for zero identifiers from previous versions. */
    if (node->identifier == 0 || ntree->runtime->nodes_by_id.contains_as(node->identifier)) {
      nodeUniqueID(ntree, node);
    }
    else {
      ntree->runtime->nodes_by_id.add_new(node);
    }

    BLO_read_struct_list(reader, bNodeSocket, &node->inputs);
    BLO_read_struct_list(reader, bNodeSocket, &node->outputs);
    BLO_read_struct_array(
        reader, bNodePanelState, node->num_panel_states, &node->panel_states_array);

    BLO_read_struct(reader, IDProperty, &node->prop);
    IDP_BlendDataRead(reader, &node->prop);

    if (node->type == CMP_NODE_MOVIEDISTORTION) {
      /* Do nothing, this is runtime cache and hence handled by generic code using
       * `IDTypeInfo.foreach_cache` callback. */
    }
    else {
      BLO_read_data_address(reader, &node->storage);
    }

    if (node->storage) {
      switch (node->type) {
        case SH_NODE_CURVE_VEC:
        case SH_NODE_CURVE_RGB:
        case SH_NODE_CURVE_FLOAT:
        case CMP_NODE_TIME:
        case CMP_NODE_CURVE_VEC:
        case CMP_NODE_CURVE_RGB:
        case CMP_NODE_HUECORRECT:
        case TEX_NODE_CURVE_RGB:
        case TEX_NODE_CURVE_TIME: {
          BKE_curvemapping_blend_read(reader, static_cast<CurveMapping *>(node->storage));
          break;
        }
        case SH_NODE_SCRIPT: {
          NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);
          BLO_read_string(reader, &nss->bytecode);
          break;
        }
        case SH_NODE_TEX_POINTDENSITY: {
          NodeShaderTexPointDensity *npd = static_cast<NodeShaderTexPointDensity *>(node->storage);
          npd->pd = dna::shallow_zero_initialize();
          break;
        }
        case SH_NODE_TEX_IMAGE: {
          NodeTexImage *tex = static_cast<NodeTexImage *>(node->storage);
          tex->iuser.scene = nullptr;
          break;
        }
        case SH_NODE_TEX_ENVIRONMENT: {
          NodeTexEnvironment *tex = static_cast<NodeTexEnvironment *>(node->storage);
          tex->iuser.scene = nullptr;
          break;
        }
        case CMP_NODE_IMAGE:
        case CMP_NODE_R_LAYERS:
        case CMP_NODE_VIEWER: {
          ImageUser *iuser = static_cast<ImageUser *>(node->storage);
          iuser->scene = nullptr;
          break;
        }
        case CMP_NODE_CRYPTOMATTE_LEGACY:
        case CMP_NODE_CRYPTOMATTE: {
          NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
          BLO_read_string(reader, &nc->matte_id);
          BLO_read_struct_list(reader, CryptomatteEntry, &nc->entries);
          BLI_listbase_clear(&nc->runtime.layers);
          break;
        }
        case TEX_NODE_IMAGE: {
          ImageUser *iuser = static_cast<ImageUser *>(node->storage);
          iuser->scene = nullptr;
          break;
        }
        case CMP_NODE_OUTPUT_FILE: {
          NodeImageMultiFile *nimf = static_cast<NodeImageMultiFile *>(node->storage);
          BKE_image_format_blend_read_data(reader, &nimf->format);
          break;
        }
        case FN_NODE_INPUT_STRING: {
          NodeInputString *storage = static_cast<NodeInputString *>(node->storage);
          BLO_read_string(reader, &storage->string);
          break;
        }
        case GEO_NODE_SIMULATION_OUTPUT: {
          nodes::SimulationItemsAccessor::blend_read_data(reader, *node);
          break;
        }
        case GEO_NODE_REPEAT_OUTPUT: {
          nodes::RepeatItemsAccessor::blend_read_data(reader, *node);
          break;
        }
        case GEO_NODE_INDEX_SWITCH: {
          nodes::IndexSwitchItemsAccessor::blend_read_data(reader, *node);
          break;
        }
        case GEO_NODE_BAKE: {
          nodes::BakeItemsAccessor::blend_read_data(reader, *node);
          break;
        }
        case GEO_NODE_MENU_SWITCH: {
          nodes::MenuSwitchItemsAccessor::blend_read_data(reader, *node);
          break;
        }
        case GEO_NODE_CAPTURE_ATTRIBUTE: {
          nodes::CaptureAttributeItemsAccessor::blend_read_data(reader, *node);
          break;
        }

        default:
          break;
      }
    }
  }
  BLO_read_struct_list(reader, bNodeLink, &ntree->links);
  BLI_assert(ntree->all_nodes().size() == BLI_listbase_count(&ntree->nodes));

  /* and we connect the rest */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    BLO_read_struct(reader, bNode, &node->parent);

    LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->inputs) {
      direct_link_node_socket(reader, sock);
    }
    LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->outputs) {
      direct_link_node_socket(reader, sock);
    }

    /* Socket storage. */
    if (node->type == CMP_NODE_OUTPUT_FILE) {
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        NodeImageMultiFileSocket *sockdata = static_cast<NodeImageMultiFileSocket *>(
            sock->storage);
        BKE_image_format_blend_read_data(reader, &sockdata->format);
      }
    }
  }

  /* Read legacy interface socket lists for versioning. */
  BLO_read_struct_list(reader, bNodeSocket, &ntree->inputs_legacy);
  BLO_read_struct_list(reader, bNodeSocket, &ntree->outputs_legacy);
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &ntree->inputs_legacy) {
    direct_link_node_socket(reader, sock);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &ntree->outputs_legacy) {
    direct_link_node_socket(reader, sock);
  }

  ntree->tree_interface.read_data(reader);

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    BLO_read_struct(reader, bNode, &link->fromnode);
    BLO_read_struct(reader, bNode, &link->tonode);
    BLO_read_struct(reader, bNodeSocket, &link->fromsock);
    BLO_read_struct(reader, bNodeSocket, &link->tosock);
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    remove_unsupported_sockets(&node->inputs, &ntree->links);
    remove_unsupported_sockets(&node->outputs, &ntree->links);
  }
  remove_unsupported_sockets(&ntree->inputs_legacy, nullptr);
  remove_unsupported_sockets(&ntree->outputs_legacy, nullptr);

  BLO_read_struct(reader, GeometryNodeAssetTraits, &ntree->geometry_node_asset_traits);
  BLO_read_struct_array(
      reader, bNestedNodeRef, ntree->nested_node_refs_num, &ntree->nested_node_refs);

  /* TODO: should be dealt by new generic cache handling of IDs... */
  ntree->previews = nullptr;

  BLO_read_struct(reader, PreviewImage, &ntree->preview);
  BKE_previewimg_blend_read(reader, ntree->preview);

  /* type verification is in lib-link */
}

static void ntree_blend_read_data(BlendDataReader *reader, ID *id)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  ntreeBlendReadData(reader, nullptr, ntree);
}

static void ntree_blend_read_after_liblink(BlendLibReader *reader, ID *id)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  /* Set `node->typeinfo` pointers. This is done in lib linking, after the
   * first versioning that can change types still without functions that
   * update the `typeinfo` pointers. Versioning after lib linking needs
   * these top be valid. */
  ntreeSetTypes(nullptr, ntree);

  /* For nodes with static socket layout, add/remove sockets as needed
   * to match the static layout. */
  if (!BLO_read_lib_is_undo(reader)) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      /* Don't update node groups here because they may depend on other node groups which are not
       * fully versioned yet and don't have `typeinfo` pointers set. */
      if (!node->is_group()) {
        node_verify_sockets(ntree, node, false);
      }
    }
  }
}

void node_update_asset_metadata(bNodeTree &node_tree)
{
  AssetMetaData *asset_data = node_tree.id.asset_data;
  if (!asset_data) {
    return;
  }

  BKE_asset_metadata_idprop_ensure(asset_data, idprop::create("type", node_tree.type).release());
  auto inputs = idprop::create_group("inputs");
  auto outputs = idprop::create_group("outputs");
  node_tree.ensure_interface_cache();
  for (const bNodeTreeInterfaceSocket *socket : node_tree.interface_inputs()) {
    auto property = idprop::create(socket->name ? socket->name : "", socket->socket_type);
    IDP_AddToGroup(inputs.get(), property.release());
  }
  for (const bNodeTreeInterfaceSocket *socket : node_tree.interface_outputs()) {
    auto property = idprop::create(socket->name ? socket->name : "", socket->socket_type);
    IDP_AddToGroup(outputs.get(), property.release());
  }
  BKE_asset_metadata_idprop_ensure(asset_data, inputs.release());
  BKE_asset_metadata_idprop_ensure(asset_data, outputs.release());
  if (node_tree.geometry_node_asset_traits) {
    auto property = idprop::create("geometry_node_asset_traits_flag",
                                   node_tree.geometry_node_asset_traits->flag);
    BKE_asset_metadata_idprop_ensure(asset_data, property.release());
  }
}

static void node_tree_asset_pre_save(void *asset_ptr, AssetMetaData * /*asset_data*/)
{
  bNodeTree &ntree = *static_cast<bNodeTree *>(asset_ptr);
  node_update_asset_metadata(ntree);
}

static void node_tree_asset_on_mark_asset(void *asset_ptr, AssetMetaData *asset_data)
{
  bNodeTree &ntree = *static_cast<bNodeTree *>(asset_ptr);
  node_update_asset_metadata(ntree);

  /* Copy node tree description to asset description so that the user does not have to write it
   * again. */
  if (!asset_data->description) {
    asset_data->description = BLI_strdup_null(ntree.description);
  }
}

static void node_tree_asset_on_clear_asset(void *asset_ptr, AssetMetaData *asset_data)
{
  bNodeTree &ntree = *static_cast<bNodeTree *>(asset_ptr);

  /* Copy asset description to node tree description so that it is not lost when the asset data is
   * removed. */
  if (asset_data->description) {
    MEM_SAFE_FREE(ntree.description);
    ntree.description = BLI_strdup_null(asset_data->description);
  }
}

}  // namespace blender::bke

static AssetTypeInfo AssetType_NT = {
    /*pre_save_fn*/ blender::bke::node_tree_asset_pre_save,
    /*on_mark_asset_fn*/ blender::bke::node_tree_asset_on_mark_asset,
    /*on_clear_asset_fn*/ blender::bke::node_tree_asset_on_clear_asset,
};

IDTypeInfo IDType_ID_NT = {
    /*id_code*/ ID_NT,
    /*id_filter*/ FILTER_ID_NT,
    /* IDProps of nodes, and #bNode.id, can use any type of ID. */
    /*dependencies_id_types*/ FILTER_ID_ALL,
    /*main_listbase_index*/ INDEX_ID_NT,
    /*struct_size*/ sizeof(bNodeTree),
    /*name*/ "NodeTree",
    /*name_plural*/ N_("node_groups"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_NODETREE,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ &AssetType_NT,

    /*init_data*/ blender::bke::ntree_init_data,
    /*copy_data*/ blender::bke::ntree_copy_data,
    /*free_data*/ blender::bke::ntree_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ blender::bke::node_foreach_id,
    /*foreach_cache*/ blender::bke::node_foreach_cache,
    /*foreach_path*/ blender::bke::node_foreach_path,
    /*owner_pointer_get*/ blender::bke::node_owner_pointer_get,

    /*blend_write*/ blender::bke::ntree_blend_write,
    /*blend_read_data*/ blender::bke::ntree_blend_read_data,
    /*blend_read_after_liblink*/ blender::bke::ntree_blend_read_after_liblink,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

namespace blender::bke {

static void node_add_sockets_from_type(bNodeTree *ntree, bNode *node, bNodeType *ntype)
{
  if (ntype->declare) {
    node_verify_sockets(ntree, node, true);
    return;
  }
  bNodeSocketTemplate *sockdef;

  if (ntype->inputs) {
    sockdef = ntype->inputs;
    while (sockdef->type != -1) {
      node_add_socket_from_template(ntree, node, sockdef, SOCK_IN);
      sockdef++;
    }
  }
  if (ntype->outputs) {
    sockdef = ntype->outputs;
    while (sockdef->type != -1) {
      node_add_socket_from_template(ntree, node, sockdef, SOCK_OUT);
      sockdef++;
    }
  }
}

/* NOTE: This function is called to initialize node data based on the type.
 * The #bNodeType may not be registered at creation time of the node,
 * so this can be delayed until the node type gets registered.
 */
static void node_init(const bContext *C, bNodeTree *ntree, bNode *node)
{
  BLI_assert(ntree != nullptr);
  bNodeType *ntype = node->typeinfo;
  if (ntype == &NodeTypeUndefined) {
    return;
  }

  /* only do this once */
  if (node->flag & NODE_INIT) {
    return;
  }

  node->flag = NODE_SELECT | NODE_OPTIONS | ntype->flag;
  node->width = ntype->width;
  node->height = ntype->height;
  node->color[0] = node->color[1] = node->color[2] = 0.608; /* default theme color */
  /* initialize the node name with the node label.
   * NOTE: do this after the initfunc so nodes get their data set which may be used in naming
   * (node groups for example) */
  /* XXX Do not use nodeLabel() here, it returns translated content for UI,
   *     which should *only* be used in UI, *never* in data...
   *     Data have their own translation option!
   *     This solution may be a bit rougher than nodeLabel()'s returned string, but it's simpler
   *     than adding "do_translate" flags to this func (and labelfunc() as well). */
  STRNCPY_UTF8(node->name, DATA_(ntype->ui_name));
  nodeUniqueName(ntree, node);

  /* Generally sockets should be added after the initialization, because the set of sockets might
   * depend on node properties. */
  const bool add_sockets_before_init = node->type == CMP_NODE_R_LAYERS;
  if (add_sockets_before_init) {
    node_add_sockets_from_type(ntree, node, ntype);
  }

  if (ntype->initfunc != nullptr) {
    ntype->initfunc(ntree, node);
  }

  if (ntree->typeinfo && ntree->typeinfo->node_add_init) {
    ntree->typeinfo->node_add_init(ntree, node);
  }

  if (!add_sockets_before_init) {
    node_add_sockets_from_type(ntree, node, ntype);
  }

  if (node->id) {
    id_us_plus(node->id);
  }

  if (ntype->initfunc_api) {
    PointerRNA ptr = RNA_pointer_create(&ntree->id, &RNA_Node, node);

    /* XXX WARNING: context can be nullptr in case nodes are added in do_versions.
     * Delayed init is not supported for nodes with context-based `initfunc_api` at the moment. */
    BLI_assert(C != nullptr);
    ntype->initfunc_api(C, &ptr);
  }

  node->flag |= NODE_INIT;
}

static void ntree_set_typeinfo(bNodeTree *ntree, bNodeTreeType *typeinfo)
{
  if (typeinfo) {
    ntree->typeinfo = typeinfo;
  }
  else {
    ntree->typeinfo = &NodeTreeTypeUndefined;
  }

  /* Deprecated integer type. */
  ntree->type = ntree->typeinfo->type;
  BKE_ntree_update_tag_all(ntree);
}

static void node_set_typeinfo(const bContext *C,
                              bNodeTree *ntree,
                              bNode *node,
                              bNodeType *typeinfo)
{
  /* for nodes saved in older versions storage can get lost, make undefined then */
  if (node->flag & NODE_INIT) {
    if (typeinfo && typeinfo->storagename[0] && !node->storage) {
      typeinfo = nullptr;
    }
  }

  if (typeinfo) {
    node->typeinfo = typeinfo;

    /* deprecated integer type */
    node->type = typeinfo->type;

    /* initialize the node if necessary */
    node_init(C, ntree, node);
  }
  else {
    node->typeinfo = &NodeTypeUndefined;
  }
}

/* WARNING: default_value must either be null or match the typeinfo at this point.
 * This function is called both for initializing new sockets and after loading files.
 */
static void node_socket_set_typeinfo(bNodeTree *ntree,
                                     bNodeSocket *sock,
                                     bNodeSocketType *typeinfo)
{
  if (typeinfo) {
    sock->typeinfo = typeinfo;

    /* deprecated integer type */
    sock->type = typeinfo->type;

    if (sock->default_value == nullptr) {
      /* initialize the default_value pointer used by standard socket types */
      node_socket_init_default_value(sock);
    }
  }
  else {
    sock->typeinfo = &NodeSocketTypeUndefined;
  }
  BKE_ntree_update_tag_socket_type(ntree, sock);
}

/* Set specific typeinfo pointers in all node trees on register/unregister */
static void update_typeinfo(Main *bmain,
                            const bContext *C,
                            bNodeTreeType *treetype,
                            bNodeType *nodetype,
                            bNodeSocketType *socktype,
                            const bool unregister)
{
  if (!bmain) {
    return;
  }

  FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
    if (treetype && STREQ(ntree->idname, treetype->idname)) {
      ntree_set_typeinfo(ntree, unregister ? nullptr : treetype);
    }

    /* initialize nodes */
    for (bNode *node : ntree->all_nodes()) {
      if (nodetype && STREQ(node->idname, nodetype->idname)) {
        node_set_typeinfo(C, ntree, node, unregister ? nullptr : nodetype);
      }

      /* initialize node sockets */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        if (socktype && STREQ(sock->idname, socktype->idname)) {
          node_socket_set_typeinfo(ntree, sock, unregister ? nullptr : socktype);
        }
      }
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        if (socktype && STREQ(sock->idname, socktype->idname)) {
          node_socket_set_typeinfo(ntree, sock, unregister ? nullptr : socktype);
        }
      }
    }
  }
  FOREACH_NODETREE_END;
}

void ntreeSetTypes(const bContext *C, bNodeTree *ntree)
{
  ntree_set_typeinfo(ntree, ntreeTypeFind(ntree->idname));

  for (bNode *node : ntree->all_nodes()) {
    /* Set socket typeinfo first because node initialization may rely on socket typeinfo for
     * generating declarations. */
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
    }

    node_set_typeinfo(C, ntree, node, nodeTypeFind(node->idname));
  }
}

static GHash *nodetreetypes_hash = nullptr;
static GHash *nodetypes_hash = nullptr;
static GHash *nodetypes_alias_hash = nullptr;
static GHash *nodesockettypes_hash = nullptr;

bNodeTreeType *ntreeTypeFind(const char *idname)
{
  if (idname[0]) {
    bNodeTreeType *nt = static_cast<bNodeTreeType *>(BLI_ghash_lookup(nodetreetypes_hash, idname));
    if (nt) {
      return nt;
    }
  }

  return nullptr;
}

void ntreeTypeAdd(bNodeTreeType *nt)
{
  BLI_ghash_insert(nodetreetypes_hash, nt->idname, nt);
  /* XXX pass Main to register function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nt, nullptr, nullptr, false);
}

static void ntree_free_type(void *treetype_v)
{
  bNodeTreeType *treetype = static_cast<bNodeTreeType *>(treetype_v);
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, treetype, nullptr, nullptr, true);
  MEM_freeN(treetype);
}

void ntreeTypeFreeLink(const bNodeTreeType *nt)
{
  BLI_ghash_remove(nodetreetypes_hash, nt->idname, nullptr, ntree_free_type);
}

bool ntreeIsRegistered(const bNodeTree *ntree)
{
  return (ntree->typeinfo != &NodeTreeTypeUndefined);
}

GHashIterator *ntreeTypeGetIterator()
{
  return BLI_ghashIterator_new(nodetreetypes_hash);
}

bNodeType *nodeTypeFind(const char *idname)
{
  if (idname[0]) {
    bNodeType *nt = static_cast<bNodeType *>(BLI_ghash_lookup(nodetypes_hash, idname));
    if (nt) {
      return nt;
    }
  }

  return nullptr;
}

const char *nodeTypeFindAlias(const char *alias)
{
  if (alias[0]) {
    const char *idname = static_cast<const char *>(BLI_ghash_lookup(nodetypes_alias_hash, alias));
    if (idname) {
      return idname;
    }
  }

  return alias;
}

static void node_free_type(void *nodetype_v)
{
  bNodeType *nodetype = static_cast<bNodeType *>(nodetype_v);
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, nodetype, nullptr, true);

  delete nodetype->static_declaration;
  nodetype->static_declaration = nullptr;

  /* Can be null when the type is not dynamically allocated. */
  if (nodetype->free_self) {
    nodetype->free_self(nodetype);
  }
}

void nodeRegisterType(bNodeType *nt)
{
  /* debug only: basic verification of registered types */
  BLI_assert(nt->idname[0] != '\0');
  BLI_assert(nt->poll != nullptr);

  if (nt->declare) {
    nt->static_declaration = new nodes::NodeDeclaration();
    nodes::build_node_declaration(*nt, *nt->static_declaration, nullptr, nullptr);
  }

  BLI_ghash_insert(nodetypes_hash, nt->idname, nt);
  /* XXX pass Main to register function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, nt, nullptr, false);
}

void nodeUnregisterType(bNodeType *nt)
{
  BLI_ghash_remove(nodetypes_hash, nt->idname, nullptr, node_free_type);
}

void nodeRegisterAlias(bNodeType *nt, const char *alias)
{
  BLI_ghash_insert(nodetypes_alias_hash, BLI_strdup(alias), BLI_strdup(nt->idname));
}

bool node_type_is_undefined(const bNode *node)
{
  if (node->typeinfo == &NodeTypeUndefined) {
    return true;
  }

  if (node->is_group()) {
    const ID *group_tree = node->id;
    if (group_tree == nullptr) {
      return false;
    }
    if (!ID_IS_LINKED(group_tree)) {
      return false;
    }
    if ((group_tree->tag & LIB_TAG_MISSING) == 0) {
      return false;
    }
    return true;
  }
  return false;
}

GHashIterator *nodeTypeGetIterator()
{
  return BLI_ghashIterator_new(nodetypes_hash);
}

bNodeSocketType *nodeSocketTypeFind(const char *idname)
{
  if (idname && idname[0]) {
    bNodeSocketType *st = static_cast<bNodeSocketType *>(
        BLI_ghash_lookup(nodesockettypes_hash, idname));
    if (st) {
      return st;
    }
  }

  return nullptr;
}

static void node_free_socket_type(void *socktype_v)
{
  bNodeSocketType *socktype = static_cast<bNodeSocketType *>(socktype_v);
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, nullptr, socktype, true);

  socktype->free_self(socktype);
}

void nodeRegisterSocketType(bNodeSocketType *st)
{
  BLI_ghash_insert(nodesockettypes_hash, st->idname, st);
  /* XXX pass Main to register function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, nullptr, st, false);
}

void nodeUnregisterSocketType(bNodeSocketType *st)
{
  BLI_ghash_remove(nodesockettypes_hash, st->idname, nullptr, node_free_socket_type);
}

bool nodeSocketIsRegistered(const bNodeSocket *sock)
{
  return (sock->typeinfo != &NodeSocketTypeUndefined);
}

GHashIterator *nodeSocketTypeGetIterator()
{
  return BLI_ghashIterator_new(nodesockettypes_hash);
}

const char *nodeSocketTypeLabel(const bNodeSocketType *stype)
{
  /* Use socket type name as a fallback if label is undefined. */
  if (stype->label[0] == '\0') {
    return RNA_struct_ui_name(stype->ext_socket.srna);
  }
  return stype->label;
}

const char *nodeSocketSubTypeLabel(int subtype)
{
  const char *name;
  if (RNA_enum_name(rna_enum_property_subtype_items, subtype, &name)) {
    return name;
  }
  return "";
}

bNodeSocket *nodeFindSocket(bNode *node, const eNodeSocketInOut in_out, const StringRef identifier)
{
  const ListBase *sockets = (in_out == SOCK_IN) ? &node->inputs : &node->outputs;
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    if (sock->identifier == identifier) {
      return sock;
    }
  }
  return nullptr;
}

const bNodeSocket *nodeFindSocket(const bNode *node,
                                  const eNodeSocketInOut in_out,
                                  const StringRef identifier)
{
  /* Reuse the implementation of the mutable accessor. */
  return nodeFindSocket(const_cast<bNode *>(node), in_out, identifier);
}

bNodeSocket *node_find_enabled_socket(bNode &node,
                                      const eNodeSocketInOut in_out,
                                      const StringRef name)
{
  ListBase *sockets = (in_out == SOCK_IN) ? &node.inputs : &node.outputs;
  LISTBASE_FOREACH (bNodeSocket *, socket, sockets) {
    if (socket->is_available() && socket->name == name) {
      return socket;
    }
  }
  return nullptr;
}

bNodeSocket *node_find_enabled_input_socket(bNode &node, const StringRef name)
{
  return node_find_enabled_socket(node, SOCK_IN, name);
}

bNodeSocket *node_find_enabled_output_socket(bNode &node, const StringRef name)
{
  return node_find_enabled_socket(node, SOCK_OUT, name);
}

static bool unique_identifier_check(void *arg, const char *identifier)
{
  const ListBase *lb = static_cast<const ListBase *>(arg);
  LISTBASE_FOREACH (bNodeSocket *, sock, lb) {
    if (STREQ(sock->identifier, identifier)) {
      return true;
    }
  }
  return false;
}

static bNodeSocket *make_socket(bNodeTree *ntree,
                                bNode * /*node*/,
                                const int in_out,
                                ListBase *lb,
                                const char *idname,
                                const char *identifier,
                                const char *name)
{
  char auto_identifier[MAX_NAME];

  if (identifier && identifier[0] != '\0') {
    /* use explicit identifier */
    STRNCPY(auto_identifier, identifier);
  }
  else {
    /* if no explicit identifier is given, assign a unique identifier based on the name */
    STRNCPY(auto_identifier, name);
  }
  /* Make the identifier unique. */
  BLI_uniquename_cb(
      unique_identifier_check, lb, "socket", '_', auto_identifier, sizeof(auto_identifier));

  bNodeSocket *sock = MEM_cnew<bNodeSocket>("sock");
  sock->runtime = MEM_new<bNodeSocketRuntime>(__func__);
  sock->in_out = in_out;

  STRNCPY(sock->identifier, auto_identifier);
  sock->limit = (in_out == SOCK_IN ? 1 : 0xFFF);

  STRNCPY(sock->name, name);
  sock->storage = nullptr;
  sock->flag |= SOCK_COLLAPSED;
  sock->type = SOCK_CUSTOM; /* int type undefined by default */

  STRNCPY(sock->idname, idname);
  node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(idname));

  return sock;
}

static void socket_id_user_increment(bNodeSocket *sock)
{
  switch (eNodeSocketDatatype(sock->type)) {
    case SOCK_OBJECT: {
      bNodeSocketValueObject &default_value = *sock->default_value_typed<bNodeSocketValueObject>();
      id_us_plus(reinterpret_cast<ID *>(default_value.value));
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage &default_value = *sock->default_value_typed<bNodeSocketValueImage>();
      id_us_plus(reinterpret_cast<ID *>(default_value.value));
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection &default_value =
          *sock->default_value_typed<bNodeSocketValueCollection>();
      id_us_plus(reinterpret_cast<ID *>(default_value.value));
      break;
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture &default_value =
          *sock->default_value_typed<bNodeSocketValueTexture>();
      id_us_plus(reinterpret_cast<ID *>(default_value.value));
      break;
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial &default_value =
          *sock->default_value_typed<bNodeSocketValueMaterial>();
      id_us_plus(reinterpret_cast<ID *>(default_value.value));
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_MATRIX:
    case SOCK_INT:
    case SOCK_STRING:
    case SOCK_MENU:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      break;
  }
}

/** \return True if the socket had an ID default value. */
static bool socket_id_user_decrement(bNodeSocket *sock)
{
  switch (eNodeSocketDatatype(sock->type)) {
    case SOCK_OBJECT: {
      bNodeSocketValueObject &default_value = *sock->default_value_typed<bNodeSocketValueObject>();
      id_us_min(reinterpret_cast<ID *>(default_value.value));
      return default_value.value != nullptr;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage &default_value = *sock->default_value_typed<bNodeSocketValueImage>();
      id_us_min(reinterpret_cast<ID *>(default_value.value));
      return default_value.value != nullptr;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection &default_value =
          *sock->default_value_typed<bNodeSocketValueCollection>();
      id_us_min(reinterpret_cast<ID *>(default_value.value));
      return default_value.value != nullptr;
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture &default_value =
          *sock->default_value_typed<bNodeSocketValueTexture>();
      id_us_min(reinterpret_cast<ID *>(default_value.value));
      return default_value.value != nullptr;
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial &default_value =
          *sock->default_value_typed<bNodeSocketValueMaterial>();
      id_us_min(reinterpret_cast<ID *>(default_value.value));
      return default_value.value != nullptr;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_MATRIX:
    case SOCK_INT:
    case SOCK_STRING:
    case SOCK_MENU:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      break;
  }
  return false;
}

void nodeModifySocketType(bNodeTree *ntree,
                          bNode * /*node*/,
                          bNodeSocket *sock,
                          const char *idname)
{
  bNodeSocketType *socktype = nodeSocketTypeFind(idname);

  if (!socktype) {
    CLOG_ERROR(&LOG, "node socket type %s undefined", idname);
    return;
  }

  if (sock->default_value) {
    if (sock->type != socktype->type) {
      /* Only reallocate the default value if the type changed so that UI data like min and max
       * isn't removed. This assumes that the default value is stored in the same format for all
       * socket types with the same #eNodeSocketDatatype. */
      socket_id_user_decrement(sock);
      MEM_freeN(sock->default_value);
      sock->default_value = nullptr;
    }
    else {
      /* Update the socket subtype when the storage isn't freed and recreated. */
      switch (eNodeSocketDatatype(sock->type)) {
        case SOCK_FLOAT: {
          sock->default_value_typed<bNodeSocketValueFloat>()->subtype = socktype->subtype;
          break;
        }
        case SOCK_VECTOR: {
          sock->default_value_typed<bNodeSocketValueVector>()->subtype = socktype->subtype;
          break;
        }
        case SOCK_INT: {
          sock->default_value_typed<bNodeSocketValueInt>()->subtype = socktype->subtype;
          break;
        }
        case SOCK_STRING: {
          sock->default_value_typed<bNodeSocketValueString>()->subtype = socktype->subtype;
          break;
        }
        case SOCK_RGBA:
        case SOCK_SHADER:
        case SOCK_BOOLEAN:
        case SOCK_ROTATION:
        case SOCK_MATRIX:
        case SOCK_CUSTOM:
        case SOCK_OBJECT:
        case SOCK_IMAGE:
        case SOCK_GEOMETRY:
        case SOCK_COLLECTION:
        case SOCK_TEXTURE:
        case SOCK_MATERIAL:
        case SOCK_MENU:
          break;
      }
    }
  }

  STRNCPY(sock->idname, idname);
  node_socket_set_typeinfo(ntree, sock, socktype);
}

void nodeModifySocketTypeStatic(
    bNodeTree *ntree, bNode *node, bNodeSocket *sock, const int type, const int subtype)
{
  const char *idname = nodeStaticSocketType(type, subtype);

  if (!idname) {
    CLOG_ERROR(&LOG, "static node socket type %d undefined", type);
    return;
  }

  nodeModifySocketType(ntree, node, sock, idname);
}

bNodeSocket *nodeAddSocket(bNodeTree *ntree,
                           bNode *node,
                           const eNodeSocketInOut in_out,
                           const char *idname,
                           const char *identifier,
                           const char *name)
{
  BLI_assert(node->type != NODE_FRAME);
  BLI_assert(!(in_out == SOCK_IN && node->type == NODE_GROUP_INPUT));
  BLI_assert(!(in_out == SOCK_OUT && node->type == NODE_GROUP_OUTPUT));

  ListBase *lb = (in_out == SOCK_IN ? &node->inputs : &node->outputs);
  bNodeSocket *sock = make_socket(ntree, node, in_out, lb, idname, identifier, name);

  BLI_remlink(lb, sock); /* does nothing for new socket */
  BLI_addtail(lb, sock);

  BKE_ntree_update_tag_socket_new(ntree, sock);

  return sock;
}

bool nodeIsStaticSocketType(const bNodeSocketType *stype)
{
  /*
   * Cannot rely on type==SOCK_CUSTOM here, because type is 0 by default
   * and can be changed on custom sockets.
   */
  return RNA_struct_is_a(stype->ext_socket.srna, &RNA_NodeSocketStandard);
}

const char *nodeStaticSocketType(const int type, const int subtype)
{
  switch (eNodeSocketDatatype(type)) {
    case SOCK_FLOAT:
      switch (PropertySubType(subtype)) {
        case PROP_UNSIGNED:
          return "NodeSocketFloatUnsigned";
        case PROP_PERCENTAGE:
          return "NodeSocketFloatPercentage";
        case PROP_FACTOR:
          return "NodeSocketFloatFactor";
        case PROP_ANGLE:
          return "NodeSocketFloatAngle";
        case PROP_TIME:
          return "NodeSocketFloatTime";
        case PROP_TIME_ABSOLUTE:
          return "NodeSocketFloatTimeAbsolute";
        case PROP_DISTANCE:
          return "NodeSocketFloatDistance";
        case PROP_WAVELENGTH:
          return "NodeSocketFloatWavelength";
        case PROP_NONE:
        default:
          return "NodeSocketFloat";
      }
    case SOCK_INT:
      switch (PropertySubType(subtype)) {
        case PROP_UNSIGNED:
          return "NodeSocketIntUnsigned";
        case PROP_PERCENTAGE:
          return "NodeSocketIntPercentage";
        case PROP_FACTOR:
          return "NodeSocketIntFactor";
        case PROP_NONE:
        default:
          return "NodeSocketInt";
      }
    case SOCK_BOOLEAN:
      return "NodeSocketBool";
    case SOCK_ROTATION:
      return "NodeSocketRotation";
    case SOCK_MATRIX:
      return "NodeSocketMatrix";
    case SOCK_VECTOR:
      switch (PropertySubType(subtype)) {
        case PROP_TRANSLATION:
          return "NodeSocketVectorTranslation";
        case PROP_DIRECTION:
          return "NodeSocketVectorDirection";
        case PROP_VELOCITY:
          return "NodeSocketVectorVelocity";
        case PROP_ACCELERATION:
          return "NodeSocketVectorAcceleration";
        case PROP_EULER:
          return "NodeSocketVectorEuler";
        case PROP_XYZ:
          return "NodeSocketVectorXYZ";
        case PROP_NONE:
        default:
          return "NodeSocketVector";
      }
    case SOCK_RGBA:
      return "NodeSocketColor";
    case SOCK_STRING:
      return "NodeSocketString";
    case SOCK_SHADER:
      return "NodeSocketShader";
    case SOCK_OBJECT:
      return "NodeSocketObject";
    case SOCK_IMAGE:
      return "NodeSocketImage";
    case SOCK_GEOMETRY:
      return "NodeSocketGeometry";
    case SOCK_COLLECTION:
      return "NodeSocketCollection";
    case SOCK_TEXTURE:
      return "NodeSocketTexture";
    case SOCK_MATERIAL:
      return "NodeSocketMaterial";
    case SOCK_MENU:
      return "NodeSocketMenu";
    case SOCK_CUSTOM:
      break;
  }
  return nullptr;
}

const char *nodeStaticSocketInterfaceTypeNew(const int type, const int subtype)
{
  switch (eNodeSocketDatatype(type)) {
    case SOCK_FLOAT:
      switch (PropertySubType(subtype)) {
        case PROP_UNSIGNED:
          return "NodeTreeInterfaceSocketFloatUnsigned";
        case PROP_PERCENTAGE:
          return "NodeTreeInterfaceSocketFloatPercentage";
        case PROP_FACTOR:
          return "NodeTreeInterfaceSocketFloatFactor";
        case PROP_ANGLE:
          return "NodeTreeInterfaceSocketFloatAngle";
        case PROP_TIME:
          return "NodeTreeInterfaceSocketFloatTime";
        case PROP_TIME_ABSOLUTE:
          return "NodeTreeInterfaceSocketFloatTimeAbsolute";
        case PROP_DISTANCE:
          return "NodeTreeInterfaceSocketFloatDistance";
        case PROP_WAVELENGTH:
          return "NodeTreeInterfaceSocketFloatWavelength";
        case PROP_NONE:
        default:
          return "NodeTreeInterfaceSocketFloat";
      }
    case SOCK_INT:
      switch (PropertySubType(subtype)) {
        case PROP_UNSIGNED:
          return "NodeTreeInterfaceSocketIntUnsigned";
        case PROP_PERCENTAGE:
          return "NodeTreeInterfaceSocketIntPercentage";
        case PROP_FACTOR:
          return "NodeTreeInterfaceSocketIntFactor";
        case PROP_NONE:
        default:
          return "NodeTreeInterfaceSocketInt";
      }
    case SOCK_BOOLEAN:
      return "NodeTreeInterfaceSocketBool";
    case SOCK_ROTATION:
      return "NodeTreeInterfaceSocketRotation";
    case SOCK_MATRIX:
      return "NodeTreeInterfaceSocketMatrix";
    case SOCK_VECTOR:
      switch (PropertySubType(subtype)) {
        case PROP_TRANSLATION:
          return "NodeTreeInterfaceSocketVectorTranslation";
        case PROP_DIRECTION:
          return "NodeTreeInterfaceSocketVectorDirection";
        case PROP_VELOCITY:
          return "NodeTreeInterfaceSocketVectorVelocity";
        case PROP_ACCELERATION:
          return "NodeTreeInterfaceSocketVectorAcceleration";
        case PROP_EULER:
          return "NodeTreeInterfaceSocketVectorEuler";
        case PROP_XYZ:
          return "NodeTreeInterfaceSocketVectorXYZ";
        case PROP_NONE:
        default:
          return "NodeTreeInterfaceSocketVector";
      }
    case SOCK_RGBA:
      return "NodeTreeInterfaceSocketColor";
    case SOCK_STRING:
      return "NodeTreeInterfaceSocketString";
    case SOCK_SHADER:
      return "NodeTreeInterfaceSocketShader";
    case SOCK_OBJECT:
      return "NodeTreeInterfaceSocketObject";
    case SOCK_IMAGE:
      return "NodeTreeInterfaceSocketImage";
    case SOCK_GEOMETRY:
      return "NodeTreeInterfaceSocketGeometry";
    case SOCK_COLLECTION:
      return "NodeTreeInterfaceSocketCollection";
    case SOCK_TEXTURE:
      return "NodeTreeInterfaceSocketTexture";
    case SOCK_MATERIAL:
      return "NodeTreeInterfaceSocketMaterial";
    case SOCK_MENU:
      return "NodeTreeInterfaceSocketMenu";
    case SOCK_CUSTOM:
      break;
  }
  return nullptr;
}

const char *nodeStaticSocketLabel(const int type, const int /*subtype*/)
{
  switch (eNodeSocketDatatype(type)) {
    case SOCK_FLOAT:
      return "Float";
    case SOCK_INT:
      return "Integer";
    case SOCK_BOOLEAN:
      return "Boolean";
    case SOCK_ROTATION:
      return "Rotation";
    case SOCK_MATRIX:
      return "Matrix";
    case SOCK_VECTOR:
      return "Vector";
    case SOCK_RGBA:
      return "Color";
    case SOCK_STRING:
      return "String";
    case SOCK_SHADER:
      return "Shader";
    case SOCK_OBJECT:
      return "Object";
    case SOCK_IMAGE:
      return "Image";
    case SOCK_GEOMETRY:
      return "Geometry";
    case SOCK_COLLECTION:
      return "Collection";
    case SOCK_TEXTURE:
      return "Texture";
    case SOCK_MATERIAL:
      return "Material";
    case SOCK_MENU:
      return "Menu";
    case SOCK_CUSTOM:
      break;
  }
  return nullptr;
}

bNodeSocket *nodeAddStaticSocket(bNodeTree *ntree,
                                 bNode *node,
                                 eNodeSocketInOut in_out,
                                 int type,
                                 int subtype,
                                 const char *identifier,
                                 const char *name)
{
  const char *idname = nodeStaticSocketType(type, subtype);

  if (!idname) {
    CLOG_ERROR(&LOG, "static node socket type %d undefined", type);
    return nullptr;
  }

  bNodeSocket *sock = nodeAddSocket(ntree, node, in_out, idname, identifier, name);
  sock->type = type;
  return sock;
}

static void node_socket_free(bNodeSocket *sock, const bool do_id_user)
{
  if (sock->prop) {
    IDP_FreePropertyContent_ex(sock->prop, do_id_user);
    MEM_freeN(sock->prop);
  }

  if (sock->default_value) {
    if (do_id_user) {
      socket_id_user_decrement(sock);
    }
    if (sock->type == SOCK_MENU) {
      auto &default_value_menu = *sock->default_value_typed<bNodeSocketValueMenu>();
      if (default_value_menu.enum_items) {
        /* Release shared data pointer. */
        default_value_menu.enum_items->remove_user_and_delete_if_last();
      }
    }
    MEM_freeN(sock->default_value);
  }
  if (sock->default_attribute_name) {
    MEM_freeN(sock->default_attribute_name);
  }
  MEM_delete(sock->runtime);
}

void nodeRemoveSocket(bNodeTree *ntree, bNode *node, bNodeSocket *sock)
{
  nodeRemoveSocketEx(ntree, node, sock, true);
}

void nodeRemoveSocketEx(bNodeTree *ntree, bNode *node, bNodeSocket *sock, const bool do_id_user)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->fromsock == sock || link->tosock == sock) {
      nodeRemLink(ntree, link);
    }
  }

  for (const int64_t i : node->runtime->internal_links.index_range()) {
    const bNodeLink &link = node->runtime->internal_links[i];
    if (link.fromsock == sock || link.tosock == sock) {
      node->runtime->internal_links.remove_and_reorder(i);
      BKE_ntree_update_tag_node_internal_link(ntree, node);
      break;
    }
  }

  /* this is fast, this way we don't need an in_out argument */
  BLI_remlink(&node->inputs, sock);
  BLI_remlink(&node->outputs, sock);

  node_socket_free(sock, do_id_user);
  MEM_freeN(sock);

  BKE_ntree_update_tag_socket_removed(ntree);
}

bNode *nodeFindNodebyName(bNodeTree *ntree, const char *name)
{
  return reinterpret_cast<bNode *>(BLI_findstring(&ntree->nodes, name, offsetof(bNode, name)));
}

void nodeFindNode(bNodeTree *ntree, bNodeSocket *sock, bNode **r_node, int *r_sockindex)
{
  *r_node = nullptr;
  if (ntree->runtime->topology_cache_mutex.is_cached()) {
    bNode *node = &sock->owner_node();
    *r_node = node;
    if (r_sockindex) {
      const ListBase *sockets = (sock->in_out == SOCK_IN) ? &node->inputs : &node->outputs;
      *r_sockindex = BLI_findindex(sockets, sock);
    }
    return;
  }
  const bool success = nodeFindNodeTry(ntree, sock, r_node, r_sockindex);
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

bool nodeFindNodeTry(bNodeTree *ntree, bNodeSocket *sock, bNode **r_node, int *r_sockindex)
{
  for (bNode *node : ntree->all_nodes()) {
    const ListBase *sockets = (sock->in_out == SOCK_IN) ? &node->inputs : &node->outputs;
    int i;
    LISTBASE_FOREACH_INDEX (const bNodeSocket *, tsock, sockets, i) {
      if (sock == tsock) {
        if (r_node != nullptr) {
          *r_node = node;
        }
        if (r_sockindex != nullptr) {
          *r_sockindex = i;
        }
        return true;
      }
    }
  }
  return false;
}

bNode *nodeFindRootParent(bNode *node)
{
  bNode *parent_iter = node;
  while (parent_iter->parent != nullptr) {
    parent_iter = parent_iter->parent;
  }
  if (parent_iter->type != NODE_FRAME) {
    return nullptr;
  }
  return parent_iter;
}

bool nodeIsParentAndChild(const bNode *parent, const bNode *child)
{
  for (const bNode *child_iter = child; child_iter; child_iter = child_iter->parent) {
    if (child_iter == parent) {
      return true;
    }
  }
  return false;
}

void nodeChainIter(const bNodeTree *ntree,
                   const bNode *node_start,
                   bool (*callback)(bNode *, bNode *, void *, const bool),
                   void *userdata,
                   const bool reversed)
{
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if ((link->flag & NODE_LINK_VALID) == 0) {
      /* Skip links marked as cyclic. */
      continue;
    }
    /* Is the link part of the chain meaning node_start == fromnode
     * (or tonode for reversed case)? */
    if (!reversed) {
      if (link->fromnode != node_start) {
        continue;
      }
    }
    else {
      if (link->tonode != node_start) {
        continue;
      }
    }

    if (!callback(link->fromnode, link->tonode, userdata, reversed)) {
      return;
    }
    nodeChainIter(ntree, reversed ? link->fromnode : link->tonode, callback, userdata, reversed);
  }
}

static void iter_backwards_ex(const bNodeTree *ntree,
                              const bNode *node_start,
                              bool (*callback)(bNode *, bNode *, void *),
                              void *userdata,
                              const char recursion_mask)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node_start->inputs) {
    bNodeLink *link = sock->link;
    if (link == nullptr) {
      continue;
    }
    if ((link->flag & NODE_LINK_VALID) == 0) {
      /* Skip links marked as cyclic. */
      continue;
    }
    if (link->fromnode->runtime->iter_flag & recursion_mask) {
      continue;
    }

    link->fromnode->runtime->iter_flag |= recursion_mask;

    if (!callback(link->fromnode, link->tonode, userdata)) {
      return;
    }
    iter_backwards_ex(ntree, link->fromnode, callback, userdata, recursion_mask);
  }
}

void nodeChainIterBackwards(const bNodeTree *ntree,
                            const bNode *node_start,
                            bool (*callback)(bNode *, bNode *, void *),
                            void *userdata,
                            const int recursion_lvl)
{
  if (!node_start) {
    return;
  }

  /* Limited by iter_flag type. */
  BLI_assert(recursion_lvl < 8);
  const char recursion_mask = (1 << recursion_lvl);

  /* Reset flag. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->runtime->iter_flag &= ~recursion_mask;
  }

  iter_backwards_ex(ntree, node_start, callback, userdata, recursion_mask);
}

void nodeParentsIter(bNode *node, bool (*callback)(bNode *, void *), void *userdata)
{
  if (node->parent) {
    if (!callback(node->parent, userdata)) {
      return;
    }
    nodeParentsIter(node->parent, callback, userdata);
  }
}

void nodeUniqueName(bNodeTree *ntree, bNode *node)
{
  BLI_uniquename(
      &ntree->nodes, node, DATA_("Node"), '.', offsetof(bNode, name), sizeof(node->name));
}

void nodeUniqueID(bNodeTree *ntree, bNode *node)
{
  /* Use a pointer cast to avoid overflow warnings. */
  const double time = BLI_time_now_seconds() * 1000000.0;
  RandomNumberGenerator id_rng{*reinterpret_cast<const uint32_t *>(&time)};

  /* In the unlikely case that the random ID doesn't match, choose a new one until it does. */
  int32_t new_id = id_rng.get_int32();
  while (ntree->runtime->nodes_by_id.contains_as(new_id) || new_id <= 0) {
    new_id = id_rng.get_int32();
  }

  node->identifier = new_id;
  ntree->runtime->nodes_by_id.add_new(node);
  node->runtime->index_in_tree = ntree->runtime->nodes_by_id.index_range().last();
  BLI_assert(node->runtime->index_in_tree == ntree->runtime->nodes_by_id.index_of(node));
}

bNode *nodeAddNode(const bContext *C, bNodeTree *ntree, const char *idname)
{
  bNode *node = MEM_cnew<bNode>("new node");
  node->runtime = MEM_new<bNodeRuntime>(__func__);
  BLI_addtail(&ntree->nodes, node);
  nodeUniqueID(ntree, node);
  node->ui_order = ntree->all_nodes().size();

  STRNCPY(node->idname, idname);
  node_set_typeinfo(C, ntree, node, nodeTypeFind(idname));

  BKE_ntree_update_tag_node_new(ntree, node);

  if (ELEM(node->type,
           GEO_NODE_INPUT_SCENE_TIME,
           GEO_NODE_INPUT_ACTIVE_CAMERA,
           GEO_NODE_SELF_OBJECT,
           GEO_NODE_SIMULATION_INPUT))
  {
    DEG_relations_tag_update(CTX_data_main(C));
  }

  return node;
}

bNode *nodeAddStaticNode(const bContext *C, bNodeTree *ntree, const int type)
{
  const char *idname = nullptr;

  NODE_TYPES_BEGIN (ntype) {
    /* Do an extra poll here, because some int types are used
     * for multiple node types, this helps find the desired type. */
    if (ntype->type != type) {
      continue;
    }

    const char *disabled_hint;
    if (ntype->poll && ntype->poll(ntype, ntree, &disabled_hint)) {
      idname = ntype->idname;
      break;
    }
  }
  NODE_TYPES_END;
  if (!idname) {
    CLOG_ERROR(&LOG, "static node type %d undefined", type);
    return nullptr;
  }
  return nodeAddNode(C, ntree, idname);
}

static void node_socket_copy(bNodeSocket *sock_dst, const bNodeSocket *sock_src, const int flag)
{
  sock_dst->runtime = MEM_new<bNodeSocketRuntime>(__func__);
  if (sock_src->prop) {
    sock_dst->prop = IDP_CopyProperty_ex(sock_src->prop, flag);
  }

  if (sock_src->default_value) {
    sock_dst->default_value = MEM_dupallocN(sock_src->default_value);

    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      socket_id_user_increment(sock_dst);
    }

    if (sock_src->type == SOCK_MENU) {
      auto &default_value_menu = *sock_dst->default_value_typed<bNodeSocketValueMenu>();
      if (default_value_menu.enum_items) {
        /* Copy of shared data pointer. */
        default_value_menu.enum_items->add_user();
      }
    }
  }

  sock_dst->default_attribute_name = static_cast<char *>(
      MEM_dupallocN(sock_src->default_attribute_name));

  sock_dst->stack_index = 0;
}

bNode *node_copy_with_mapping(bNodeTree *dst_tree,
                              const bNode &node_src,
                              const int flag,
                              const bool use_unique,
                              Map<const bNodeSocket *, bNodeSocket *> &socket_map)
{
  bNode *node_dst = static_cast<bNode *>(MEM_mallocN(sizeof(bNode), __func__));
  *node_dst = node_src;

  node_dst->runtime = MEM_new<bNodeRuntime>(__func__);

  /* Can be called for nodes outside a node tree (e.g. clipboard). */
  if (dst_tree) {
    if (use_unique) {
      nodeUniqueName(dst_tree, node_dst);
      nodeUniqueID(dst_tree, node_dst);
    }
    BLI_addtail(&dst_tree->nodes, node_dst);
  }

  BLI_listbase_clear(&node_dst->inputs);
  LISTBASE_FOREACH (const bNodeSocket *, src_socket, &node_src.inputs) {
    bNodeSocket *dst_socket = static_cast<bNodeSocket *>(MEM_dupallocN(src_socket));
    node_socket_copy(dst_socket, src_socket, flag);
    BLI_addtail(&node_dst->inputs, dst_socket);
    socket_map.add_new(src_socket, dst_socket);
  }

  BLI_listbase_clear(&node_dst->outputs);
  LISTBASE_FOREACH (const bNodeSocket *, src_socket, &node_src.outputs) {
    bNodeSocket *dst_socket = static_cast<bNodeSocket *>(MEM_dupallocN(src_socket));
    node_socket_copy(dst_socket, src_socket, flag);
    BLI_addtail(&node_dst->outputs, dst_socket);
    socket_map.add_new(src_socket, dst_socket);
  }

  if (node_src.prop) {
    node_dst->prop = IDP_CopyProperty_ex(node_src.prop, flag);
  }

  node_dst->panel_states_array = static_cast<bNodePanelState *>(
      MEM_dupallocN(node_src.panel_states_array));

  node_dst->runtime->internal_links = node_src.runtime->internal_links;
  for (bNodeLink &dst_link : node_dst->runtime->internal_links) {
    dst_link.fromnode = node_dst;
    dst_link.tonode = node_dst;
    dst_link.fromsock = socket_map.lookup(dst_link.fromsock);
    dst_link.tosock = socket_map.lookup(dst_link.tosock);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus(node_dst->id);
  }

  if (node_src.typeinfo->copyfunc) {
    node_src.typeinfo->copyfunc(dst_tree, node_dst, &node_src);
  }

  if (dst_tree) {
    BKE_ntree_update_tag_node_new(dst_tree, node_dst);
  }

  /* Only call copy function when a copy is made for the main database, not
   * for cases like the dependency graph and localization. */
  if (node_dst->typeinfo->copyfunc_api && !(flag & LIB_ID_CREATE_NO_MAIN)) {
    PointerRNA ptr = RNA_pointer_create(reinterpret_cast<ID *>(dst_tree), &RNA_Node, node_dst);

    node_dst->typeinfo->copyfunc_api(&ptr, &node_src);
  }

  return node_dst;
}

/**
 * Type of value storage related with socket is the same.
 * \param socket: Node can have multiple sockets & storages pairs.
 */
static void *node_static_value_storage_for(bNode &node, const bNodeSocket &socket)
{
  if (!socket.is_output()) {
    return nullptr;
  }

  switch (node.type) {
    case FN_NODE_INPUT_BOOL:
      return &reinterpret_cast<NodeInputBool *>(node.storage)->boolean;
    case SH_NODE_VALUE:
      /* The value is stored in the default value of the first output socket. */
      return &static_cast<bNodeSocket *>(node.outputs.first)
                  ->default_value_typed<bNodeSocketValueFloat>()
                  ->value;
    case FN_NODE_INPUT_INT:
      return &reinterpret_cast<NodeInputInt *>(node.storage)->integer;
    case FN_NODE_INPUT_VECTOR:
      return &reinterpret_cast<NodeInputVector *>(node.storage)->vector;
    case FN_NODE_INPUT_COLOR:
      return &reinterpret_cast<NodeInputColor *>(node.storage)->color;
    case GEO_NODE_IMAGE:
      return &node.id;
    default:
      break;
  }

  return nullptr;
}

static void *socket_value_storage(bNodeSocket &socket)
{
  switch (eNodeSocketDatatype(socket.type)) {
    case SOCK_BOOLEAN:
      return &socket.default_value_typed<bNodeSocketValueBoolean>()->value;
    case SOCK_INT:
      return &socket.default_value_typed<bNodeSocketValueInt>()->value;
    case SOCK_FLOAT:
      return &socket.default_value_typed<bNodeSocketValueFloat>()->value;
    case SOCK_VECTOR:
      return &socket.default_value_typed<bNodeSocketValueVector>()->value;
    case SOCK_RGBA:
      return &socket.default_value_typed<bNodeSocketValueRGBA>()->value;
    case SOCK_IMAGE:
      return &socket.default_value_typed<bNodeSocketValueImage>()->value;
    case SOCK_TEXTURE:
      return &socket.default_value_typed<bNodeSocketValueTexture>()->value;
    case SOCK_COLLECTION:
      return &socket.default_value_typed<bNodeSocketValueCollection>()->value;
    case SOCK_OBJECT:
      return &socket.default_value_typed<bNodeSocketValueObject>()->value;
    case SOCK_MATERIAL:
      return &socket.default_value_typed<bNodeSocketValueMaterial>()->value;
    case SOCK_ROTATION:
      return &socket.default_value_typed<bNodeSocketValueRotation>()->value_euler;
    case SOCK_MENU:
      return &socket.default_value_typed<bNodeSocketValueMenu>()->value;
    case SOCK_MATRIX:
      /* Matrix sockets currently have no default value. */
      return nullptr;
    case SOCK_STRING:
      /* We don't want do this now! */
      return nullptr;
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      /* Unmovable types. */
      break;
  }

  return nullptr;
}

void node_socket_move_default_value(Main & /*bmain*/,
                                    bNodeTree &tree,
                                    bNodeSocket &src,
                                    bNodeSocket &dst)
{
  tree.ensure_topology_cache();

  bNode &dst_node = dst.owner_node();
  bNode &src_node = src.owner_node();

  const CPPType &src_type = *src.typeinfo->base_cpp_type;
  const CPPType &dst_type = *dst.typeinfo->base_cpp_type;

  const bke::DataTypeConversions &convert = bke::get_implicit_type_conversions();

  if (src.is_multi_input()) {
    /* Multi input sockets no have value. */
    return;
  }
  if (ELEM(NODE_REROUTE, dst_node.type, src_node.type)) {
    /* Reroute node can't have ownership of socket value directly. */
    return;
  }
  if (&src_type != &dst_type) {
    if (!convert.is_convertible(src_type, dst_type)) {
      return;
    }
  }

  void *src_value = socket_value_storage(src);
  void *dst_value = node_static_value_storage_for(dst_node, dst);
  if (!dst_value || !src_value) {
    return;
  }

  convert.convert_to_uninitialized(src_type, dst_type, src_value, dst_value);

  src_type.destruct(src_value);
  if (ELEM(eNodeSocketDatatype(src.type),
           SOCK_COLLECTION,
           SOCK_IMAGE,
           SOCK_MATERIAL,
           SOCK_TEXTURE,
           SOCK_OBJECT))
  {
    src_type.value_initialize(src_value);
  }
}

bNode *node_copy(bNodeTree *dst_tree, const bNode &src_node, const int flag, const bool use_unique)
{
  Map<const bNodeSocket *, bNodeSocket *> socket_map;
  return node_copy_with_mapping(dst_tree, src_node, flag, use_unique, socket_map);
}

static int node_count_links(const bNodeTree *ntree, const bNodeSocket *socket)
{
  int count = 0;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (ELEM(socket, link->fromsock, link->tosock)) {
      count++;
    }
  }
  return count;
}

bNodeLink *nodeAddLink(
    bNodeTree *ntree, bNode *fromnode, bNodeSocket *fromsock, bNode *tonode, bNodeSocket *tosock)
{
  BLI_assert(fromnode);
  BLI_assert(tonode);
  BLI_assert(ntree->all_nodes().contains(fromnode));
  BLI_assert(ntree->all_nodes().contains(tonode));

  bNodeLink *link = nullptr;
  if (eNodeSocketInOut(fromsock->in_out) == SOCK_OUT &&
      eNodeSocketInOut(tosock->in_out) == SOCK_IN)
  {
    link = MEM_cnew<bNodeLink>("link");
    if (ntree) {
      BLI_addtail(&ntree->links, link);
    }
    link->fromnode = fromnode;
    link->fromsock = fromsock;
    link->tonode = tonode;
    link->tosock = tosock;
  }
  else if (eNodeSocketInOut(fromsock->in_out) == SOCK_IN &&
           eNodeSocketInOut(tosock->in_out) == SOCK_OUT)
  {
    /* OK but flip */
    link = MEM_cnew<bNodeLink>("link");
    if (ntree) {
      BLI_addtail(&ntree->links, link);
    }
    link->fromnode = tonode;
    link->fromsock = tosock;
    link->tonode = fromnode;
    link->tosock = fromsock;
  }

  if (ntree) {
    BKE_ntree_update_tag_link_added(ntree, link);
  }

  if (link != nullptr && link->tosock->is_multi_input()) {
    link->multi_input_sort_id = node_count_links(ntree, link->tosock) - 1;
  }

  return link;
}

void nodeRemLink(bNodeTree *ntree, bNodeLink *link)
{
  /* Can be called for links outside a node tree (e.g. clipboard). */
  if (ntree) {
    BLI_remlink(&ntree->links, link);
  }

  if (link->tosock) {
    link->tosock->link = nullptr;
  }
  MEM_freeN(link);

  if (ntree) {
    BKE_ntree_update_tag_link_removed(ntree);
  }
}

void nodeLinkSetMute(bNodeTree *ntree, bNodeLink *link, const bool muted)
{
  const bool was_muted = link->is_muted();
  SET_FLAG_FROM_TEST(link->flag, muted, NODE_LINK_MUTED);
  if (muted != was_muted) {
    BKE_ntree_update_tag_link_mute(ntree, link);
  }
}

void nodeRemSocketLinks(bNodeTree *ntree, bNodeSocket *sock)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->fromsock == sock || link->tosock == sock) {
      nodeRemLink(ntree, link);
    }
  }
}

bool nodeLinkIsHidden(const bNodeLink *link)
{
  return !(link->fromsock->is_visible() && link->tosock->is_visible());
}

bool nodeLinkIsSelected(const bNodeLink *link)
{
  return (link->fromnode->flag & NODE_SELECT) || (link->tonode->flag & NODE_SELECT);
}

/* Adjust the indices of links connected to the given multi input socket after deleting the link at
 * `deleted_index`. This function also works if the link has not yet been deleted. */
static void adjust_multi_input_indices_after_removed_link(bNodeTree *ntree,
                                                          const bNodeSocket *sock,
                                                          const int deleted_index)
{
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    /* We only need to adjust those with a greater index, because the others will have the same
     * index. */
    if (link->tosock != sock || link->multi_input_sort_id <= deleted_index) {
      continue;
    }
    link->multi_input_sort_id -= 1;
  }
}

void nodeInternalRelink(bNodeTree *ntree, bNode *node)
{
  /* store link pointers in output sockets, for efficient lookup */
  for (bNodeLink &link : node->runtime->internal_links) {
    link.tosock->link = &link;
  }

  Vector<bNodeLink *> duplicate_links_to_remove;

  /* redirect downstream links */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    /* do we have internal link? */
    if (link->fromnode != node) {
      continue;
    }

    bNodeLink *internal_link = link->fromsock->link;
    bNodeLink *fromlink = internal_link ? internal_link->fromsock->link : nullptr;

    if (fromlink == nullptr) {
      if (link->tosock->is_multi_input()) {
        adjust_multi_input_indices_after_removed_link(
            ntree, link->tosock, link->multi_input_sort_id);
      }
      nodeRemLink(ntree, link);
      continue;
    }

    if (link->tosock->is_multi_input()) {
      /* remove the link that would be the same as the relinked one */
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link_to_compare, &ntree->links) {
        if (link_to_compare->fromsock == fromlink->fromsock &&
            link_to_compare->tosock == link->tosock)
        {
          adjust_multi_input_indices_after_removed_link(
              ntree, link_to_compare->tosock, link_to_compare->multi_input_sort_id);
          duplicate_links_to_remove.append_non_duplicates(link_to_compare);
        }
      }
    }

    link->fromnode = fromlink->fromnode;
    link->fromsock = fromlink->fromsock;

    /* if the up- or downstream link is invalid,
     * the replacement link will be invalid too.
     */
    if (!(fromlink->flag & NODE_LINK_VALID)) {
      link->flag &= ~NODE_LINK_VALID;
    }

    if (fromlink->flag & NODE_LINK_MUTED) {
      link->flag |= NODE_LINK_MUTED;
    }

    BKE_ntree_update_tag_link_changed(ntree);
  }

  for (bNodeLink *link : duplicate_links_to_remove) {
    nodeRemLink(ntree, link);
  }

  /* remove remaining upstream links */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->tonode == node) {
      nodeRemLink(ntree, link);
    }
  }
}

float2 nodeToView(const bNode *node, const float2 loc)
{
  float2 view_loc = loc;
  for (const bNode *node_iter = node; node_iter; node_iter = node_iter->parent) {
    view_loc += float2(node_iter->locx, node_iter->locy);
  }
  return view_loc;
}

float2 nodeFromView(const bNode *node, const float2 view_loc)
{
  float2 loc = view_loc;
  for (const bNode *node_iter = node; node_iter; node_iter = node_iter->parent) {
    loc -= float2(node_iter->locx, node_iter->locy);
  }
  return loc;
}

void nodeAttachNode(bNodeTree *ntree, bNode *node, bNode *parent)
{
  BLI_assert(parent->type == NODE_FRAME);
  BLI_assert(!nodeIsParentAndChild(parent, node));

  const float2 loc = nodeToView(node, {});

  node->parent = parent;
  BKE_ntree_update_tag_parent_change(ntree, node);
  /* transform to parent space */
  const float2 new_loc = nodeFromView(parent, loc);
  node->locx = new_loc.x;
  node->locy = new_loc.y;
}

void nodeDetachNode(bNodeTree *ntree, bNode *node)
{
  if (node->parent) {
    BLI_assert(node->parent->type == NODE_FRAME);

    /* transform to view space */
    const float2 loc = nodeToView(node, {});
    node->locx = loc.x;
    node->locy = loc.y;
    node->parent = nullptr;
    BKE_ntree_update_tag_parent_change(ntree, node);
  }
}

void nodePositionRelative(bNode *from_node,
                          const bNode *to_node,
                          const bNodeSocket *from_sock,
                          const bNodeSocket *to_sock)
{
  float offset_x;
  int tot_sock_idx;

  /* Socket to plug into. */
  if (eNodeSocketInOut(to_sock->in_out) == SOCK_IN) {
    offset_x = -(from_node->typeinfo->width + 50);
    tot_sock_idx = BLI_listbase_count(&to_node->outputs);
    tot_sock_idx += BLI_findindex(&to_node->inputs, to_sock);
  }
  else {
    offset_x = to_node->typeinfo->width + 50;
    tot_sock_idx = BLI_findindex(&to_node->outputs, to_sock);
  }

  BLI_assert(tot_sock_idx != -1);

  float offset_y = U.widget_unit * tot_sock_idx;

  /* Output socket. */
  if (from_sock) {
    if (eNodeSocketInOut(from_sock->in_out) == SOCK_IN) {
      tot_sock_idx = BLI_listbase_count(&from_node->outputs);
      tot_sock_idx += BLI_findindex(&from_node->inputs, from_sock);
    }
    else {
      tot_sock_idx = BLI_findindex(&from_node->outputs, from_sock);
    }
  }

  BLI_assert(tot_sock_idx != -1);

  offset_y -= U.widget_unit * tot_sock_idx;

  from_node->locx = to_node->locx + offset_x;
  from_node->locy = to_node->locy - offset_y;
}

void nodePositionPropagate(bNode *node)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    if (socket->link != nullptr) {
      bNodeLink *link = socket->link;
      nodePositionRelative(link->fromnode, link->tonode, link->fromsock, link->tosock);
      nodePositionPropagate(link->fromnode);
    }
  }
}

static bNodeTree *ntreeAddTree_do(Main *bmain,
                                  std::optional<Library *> owner_library,
                                  ID *owner_id,
                                  const bool is_embedded,
                                  const char *name,
                                  const char *idname)
{
  /* trees are created as local trees for compositor, material or texture nodes,
   * node groups and other tree types are created as library data.
   */
  int flag = 0;
  if (is_embedded || bmain == nullptr) {
    flag |= LIB_ID_CREATE_NO_MAIN;
  }
  BLI_assert_msg(!owner_library || !owner_id,
                 "Embedded NTrees should never have a defined owner library here");
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(
      BKE_libblock_alloc_in_lib(bmain, owner_library, ID_NT, name, flag));
  BKE_libblock_init_empty(&ntree->id);
  if (is_embedded) {
    BLI_assert(owner_id != nullptr);
    ntree->id.flag |= LIB_EMBEDDED_DATA;
    ntree->owner_id = owner_id;
    bNodeTree **ntree_owner_ptr = BKE_ntree_ptr_from_id(owner_id);
    BLI_assert(ntree_owner_ptr != nullptr);
    *ntree_owner_ptr = ntree;
  }
  else {
    BLI_assert(owner_id == nullptr);
  }

  STRNCPY(ntree->idname, idname);
  ntree_set_typeinfo(ntree, ntreeTypeFind(idname));

  return ntree;
}

bNodeTree *ntreeAddTree(Main *bmain, const char *name, const char *idname)
{
  return ntreeAddTree_do(bmain, std::nullopt, nullptr, false, name, idname);
}

bNodeTree *BKE_node_tree_add_in_lib(Main *bmain,
                                    Library *owner_library,
                                    const char *name,
                                    const char *idname)
{
  return ntreeAddTree_do(bmain, owner_library, nullptr, false, name, idname);
}

bNodeTree *ntreeAddTreeEmbedded(Main * /*bmain*/,
                                ID *owner_id,
                                const char *name,
                                const char *idname)
{
  return ntreeAddTree_do(nullptr, std::nullopt, owner_id, true, name, idname);
}

bNodeTree *ntreeCopyTree_ex(const bNodeTree *ntree, Main *bmain, const bool do_id_user)
{
  const int flag = do_id_user ? 0 : LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN;

  bNodeTree *ntree_copy = reinterpret_cast<bNodeTree *>(
      BKE_id_copy_ex(bmain, reinterpret_cast<const ID *>(ntree), nullptr, flag));
  return ntree_copy;
}
bNodeTree *ntreeCopyTree(Main *bmain, const bNodeTree *ntree)
{
  return ntreeCopyTree_ex(ntree, bmain, true);
}

/* *************** Node Preview *********** */

/* XXX this should be removed eventually ...
 * Currently BKE functions are modeled closely on previous code,
 * using node_preview_init_tree to set up previews for a whole node tree in advance.
 * This should be left more to the individual node tree implementations. */

bool node_preview_used(const bNode *node)
{
  /* XXX check for closed nodes? */
  return (node->typeinfo->flag & NODE_PREVIEW) != 0;
}

bNodePreview *node_preview_verify(bNodeInstanceHash *previews,
                                  bNodeInstanceKey key,
                                  const int xsize,
                                  const int ysize,
                                  const bool create)
{
  bNodePreview *preview = static_cast<bNodePreview *>(
      BKE_node_instance_hash_lookup(previews, key));
  if (!preview) {
    if (create) {
      preview = MEM_cnew<bNodePreview>("node preview");
      preview->ibuf = IMB_allocImBuf(xsize, ysize, 32, IB_rect);
      BKE_node_instance_hash_insert(previews, key, preview);
    }
    else {
      return nullptr;
    }
  }

  /* node previews can get added with variable size this way */
  if (xsize == 0 || ysize == 0) {
    return preview;
  }

  /* sanity checks & initialize */
  const uint size[2] = {uint(xsize), uint(ysize)};
  IMB_rect_size_set(preview->ibuf, size);
  if (preview->ibuf->byte_buffer.data == nullptr) {
    imb_addrectImBuf(preview->ibuf);
  }
  /* no clear, makes nicer previews */

  return preview;
}

bNodePreview *node_preview_copy(bNodePreview *preview)
{
  bNodePreview *new_preview = static_cast<bNodePreview *>(MEM_dupallocN(preview));
  new_preview->ibuf = IMB_dupImBuf(preview->ibuf);
  return new_preview;
}

void node_preview_free(bNodePreview *preview)
{
  if (preview->ibuf) {
    IMB_freeImBuf(preview->ibuf);
  }
  MEM_freeN(preview);
}

static void node_preview_init_tree_recursive(bNodeInstanceHash *previews,
                                             bNodeTree *ntree,
                                             bNodeInstanceKey parent_key,
                                             const int xsize,
                                             const int ysize)
{
  for (bNode *node : ntree->all_nodes()) {
    bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);

    if (node_preview_used(node)) {
      node->runtime->preview_xsize = xsize;
      node->runtime->preview_ysize = ysize;

      node_preview_verify(previews, key, xsize, ysize, false);
    }

    bNodeTree *group = reinterpret_cast<bNodeTree *>(node->id);
    if (node->is_group() && group != nullptr) {
      node_preview_init_tree_recursive(previews, group, key, xsize, ysize);
    }
  }
}

void node_preview_init_tree(bNodeTree *ntree, int xsize, int ysize)
{
  if (!ntree) {
    return;
  }

  if (!ntree->previews) {
    ntree->previews = BKE_node_instance_hash_new("node previews");
  }

  node_preview_init_tree_recursive(ntree->previews, ntree, NODE_INSTANCE_KEY_BASE, xsize, ysize);
}

static void node_preview_tag_used_recursive(bNodeInstanceHash *previews,
                                            bNodeTree *ntree,
                                            bNodeInstanceKey parent_key)
{
  for (bNode *node : ntree->all_nodes()) {
    bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);

    if (node_preview_used(node)) {
      BKE_node_instance_hash_tag_key(previews, key);
    }

    bNodeTree *group = reinterpret_cast<bNodeTree *>(node->id);
    if (node->is_group() && group != nullptr) {
      node_preview_tag_used_recursive(previews, group, key);
    }
  }
}

void node_preview_remove_unused(bNodeTree *ntree)
{
  if (!ntree || !ntree->previews) {
    return;
  }

  /* use the instance hash functions for tagging and removing unused previews */
  BKE_node_instance_hash_clear_tags(ntree->previews);
  node_preview_tag_used_recursive(ntree->previews, ntree, NODE_INSTANCE_KEY_BASE);

  BKE_node_instance_hash_remove_untagged(
      ntree->previews, reinterpret_cast<bNodeInstanceValueFP>(node_preview_free));
}

void node_preview_merge_tree(bNodeTree *to_ntree, bNodeTree *from_ntree, bool remove_old)
{
  if (remove_old || !to_ntree->previews) {
    /* free old previews */
    if (to_ntree->previews) {
      BKE_node_instance_hash_free(to_ntree->previews,
                                  reinterpret_cast<bNodeInstanceValueFP>(node_preview_free));
    }

    /* transfer previews */
    to_ntree->previews = from_ntree->previews;
    from_ntree->previews = nullptr;

    /* clean up, in case any to_ntree nodes have been removed */
    node_preview_remove_unused(to_ntree);
  }
  else {
    if (from_ntree->previews) {
      bNodeInstanceHashIterator iter;
      NODE_INSTANCE_HASH_ITER (iter, from_ntree->previews) {
        bNodeInstanceKey key = node_instance_hash_iterator_get_key(&iter);
        bNodePreview *preview = static_cast<bNodePreview *>(
            node_instance_hash_iterator_get_value(&iter));

        /* replace existing previews */
        BKE_node_instance_hash_remove(
            to_ntree->previews, key, reinterpret_cast<bNodeInstanceValueFP>(node_preview_free));
        BKE_node_instance_hash_insert(to_ntree->previews, key, preview);
      }

      /* NOTE: null free function here,
       * because pointers have already been moved over to to_ntree->previews! */
      BKE_node_instance_hash_free(from_ntree->previews, nullptr);
      from_ntree->previews = nullptr;
    }
  }
}

void nodeUnlinkNode(bNodeTree *ntree, bNode *node)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    ListBase *lb = nullptr;
    if (link->fromnode == node) {
      lb = &node->outputs;
    }
    else if (link->tonode == node) {
      lb = &node->inputs;
    }

    if (lb) {
      /* Only bother adjusting if the socket is not on the node we're deleting. */
      if (link->tonode != node && link->tosock->is_multi_input()) {
        adjust_multi_input_indices_after_removed_link(
            ntree, link->tosock, link->multi_input_sort_id);
      }
      LISTBASE_FOREACH (const bNodeSocket *, sock, lb) {
        if (link->fromsock == sock || link->tosock == sock) {
          nodeRemLink(ntree, link);
          break;
        }
      }
    }
  }
}

static void node_unlink_attached(bNodeTree *ntree, const bNode *parent)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->parent == parent) {
      nodeDetachNode(ntree, node);
    }
  }
}

void nodeRebuildIDVector(bNodeTree *node_tree)
{
  /* Rebuild nodes #VectorSet which must have the same order as the list. */
  node_tree->runtime->nodes_by_id.clear();
  int i;
  LISTBASE_FOREACH_INDEX (bNode *, node, &node_tree->nodes, i) {
    node_tree->runtime->nodes_by_id.add_new(node);
    node->runtime->index_in_tree = i;
  }
}

void node_free_node(bNodeTree *ntree, bNode *node)
{
  /* since it is called while free database, node->id is undefined */

  /* can be called for nodes outside a node tree (e.g. clipboard) */
  if (ntree) {
    BLI_remlink(&ntree->nodes, node);
    /* Rebuild nodes #VectorSet which must have the same order as the list. */
    nodeRebuildIDVector(ntree);

    /* texture node has bad habit of keeping exec data around */
    if (ntree->type == NTREE_TEXTURE && ntree->runtime->execdata) {
      ntreeTexEndExecTree(ntree->runtime->execdata);
      ntree->runtime->execdata = nullptr;
    }
  }

  if (node->typeinfo->freefunc) {
    node->typeinfo->freefunc(node);
  }

  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->inputs) {
    /* Remember, no ID user refcount management here! */
    node_socket_free(sock, false);
    MEM_freeN(sock);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->outputs) {
    /* Remember, no ID user refcount management here! */
    node_socket_free(sock, false);
    MEM_freeN(sock);
  }

  MEM_SAFE_FREE(node->panel_states_array);

  if (node->prop) {
    /* Remember, no ID user refcount management here! */
    IDP_FreePropertyContent_ex(node->prop, false);
    MEM_freeN(node->prop);
  }

  if (node->runtime->declaration) {
    /* Only free if this declaration is not shared with the node type, which can happen if it does
     * not depend on any context. */
    if (node->runtime->declaration != node->typeinfo->static_declaration) {
      delete node->runtime->declaration;
    }
  }

  MEM_delete(node->runtime);
  MEM_freeN(node);

  if (ntree) {
    BKE_ntree_update_tag_node_removed(ntree);
  }
}

void ntreeFreeLocalNode(bNodeTree *ntree, bNode *node)
{
  /* For removing nodes while editing localized node trees. */
  BLI_assert((ntree->id.tag & LIB_TAG_LOCALIZED) != 0);

  /* These two lines assume the caller might want to free a single node and maintain
   * a valid state in the node tree. */
  nodeUnlinkNode(ntree, node);
  node_unlink_attached(ntree, node);

  node_free_node(ntree, node);
  nodeRebuildIDVector(ntree);
}

void nodeRemoveNode(Main *bmain, bNodeTree *ntree, bNode *node, const bool do_id_user)
{
  BLI_assert(ntree != nullptr);
  /* This function is not for localized node trees, we do not want
   * do to ID user reference-counting and removal of animation data then. */
  BLI_assert((ntree->id.tag & LIB_TAG_LOCALIZED) == 0);

  bool node_has_id = false;

  if (do_id_user) {
    /* Free callback for NodeCustomGroup. */
    if (node->typeinfo->freefunc_api) {
      PointerRNA ptr = RNA_pointer_create(&ntree->id, &RNA_Node, node);

      node->typeinfo->freefunc_api(&ptr);
    }

    /* Do user counting. */
    if (node->id) {
      id_us_min(node->id);
      node_has_id = true;
    }

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      node_has_id |= socket_id_user_decrement(sock);
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      node_has_id |= socket_id_user_decrement(sock);
    }
  }

  /* Remove animation data. */
  char propname_esc[MAX_IDPROP_NAME * 2];
  char prefix[MAX_IDPROP_NAME * 2];

  BLI_str_escape(propname_esc, node->name, sizeof(propname_esc));
  SNPRINTF(prefix, "nodes[\"%s\"]", propname_esc);

  if (BKE_animdata_fix_paths_remove(&ntree->id, prefix)) {
    if (bmain != nullptr) {
      DEG_relations_tag_update(bmain);
    }
  }

  /* Also update relations for the scene time node, which causes a dependency
   * on time that users expect to be removed when the node is removed. */
  if (node_has_id ||
      ELEM(node->type, GEO_NODE_INPUT_SCENE_TIME, GEO_NODE_SELF_OBJECT, GEO_NODE_SIMULATION_INPUT))
  {
    if (bmain != nullptr) {
      DEG_relations_tag_update(bmain);
    }
  }

  nodeUnlinkNode(ntree, node);
  node_unlink_attached(ntree, node);

  /* Free node itself. */
  node_free_node(ntree, node);
  nodeRebuildIDVector(ntree);
}

static void free_localized_node_groups(bNodeTree *ntree)
{
  /* Only localized node trees store a copy for each node group tree.
   * Each node group tree in a localized node tree can be freed,
   * since it is a localized copy itself (no risk of accessing freed
   * data in main, see #37939). */
  if (!(ntree->id.tag & LIB_TAG_LOCALIZED)) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    bNodeTree *ngroup = reinterpret_cast<bNodeTree *>(node->id);
    if (node->is_group() && ngroup != nullptr) {
      ntreeFreeTree(ngroup);
      MEM_freeN(ngroup);
    }
  }
}

void ntreeFreeTree(bNodeTree *ntree)
{
  ntree_free_data(&ntree->id);
  BKE_animdata_free(&ntree->id, false);
}

void ntreeFreeEmbeddedTree(bNodeTree *ntree)
{
  ntreeFreeTree(ntree);
  BKE_libblock_free_data(&ntree->id, true);
  BKE_libblock_free_data_py(&ntree->id);
}

void ntreeFreeLocalTree(bNodeTree *ntree)
{
  if (ntree->id.tag & LIB_TAG_LOCALIZED) {
    ntreeFreeTree(ntree);
  }
  else {
    ntreeFreeTree(ntree);
    BKE_libblock_free_data(&ntree->id, true);
  }
}

void ntreeSetOutput(bNodeTree *ntree)
{
  const bool is_compositor = ntree->type == NTREE_COMPOSIT;
  /* find the active outputs, might become tree type dependent handler */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->typeinfo->nclass == NODE_CLASS_OUTPUT) {
      /* we need a check for which output node should be tagged like this, below an exception */
      if (ELEM(node->type, CMP_NODE_OUTPUT_FILE, GEO_NODE_VIEWER)) {
        continue;
      }
      const bool node_is_output = node->type == CMP_NODE_VIEWER;

      int output = 0;
      /* there is more types having output class, each one is checked */

      LISTBASE_FOREACH (bNode *, tnode, &ntree->nodes) {
        if (tnode->typeinfo->nclass != NODE_CLASS_OUTPUT) {
          continue;
        }

        /* same type, exception for viewer */
        const bool tnode_is_output = tnode->type == CMP_NODE_VIEWER;
        const bool compositor_case = is_compositor && tnode_is_output && node_is_output;
        if (tnode->type == node->type || compositor_case) {
          if (tnode->flag & NODE_DO_OUTPUT) {
            output++;
            if (output > 1) {
              tnode->flag &= ~NODE_DO_OUTPUT;
            }
          }
        }
      }

      if (output == 0) {
        node->flag |= NODE_DO_OUTPUT;
      }
    }

    /* group node outputs use this flag too */
    if (node->type == NODE_GROUP_OUTPUT) {
      int output = 0;
      LISTBASE_FOREACH (bNode *, tnode, &ntree->nodes) {
        if (tnode->type != NODE_GROUP_OUTPUT) {
          continue;
        }
        if (tnode->flag & NODE_DO_OUTPUT) {
          output++;
          if (output > 1) {
            tnode->flag &= ~NODE_DO_OUTPUT;
          }
        }
      }
      if (output == 0) {
        node->flag |= NODE_DO_OUTPUT;
      }
    }
  }

  /* here we could recursively set which nodes have to be done,
   * might be different for editor or for "real" use... */
}

bNodeTree **BKE_ntree_ptr_from_id(ID *id)
{
  switch (GS(id->name)) {
    case ID_MA:
      return &reinterpret_cast<Material *>(id)->nodetree;
    case ID_LA:
      return &reinterpret_cast<Light *>(id)->nodetree;
    case ID_WO:
      return &reinterpret_cast<World *>(id)->nodetree;
    case ID_TE:
      return &reinterpret_cast<Tex *>(id)->nodetree;
    case ID_SCE:
      return &reinterpret_cast<Scene *>(id)->nodetree;
    case ID_LS:
      return &reinterpret_cast<FreestyleLineStyle *>(id)->nodetree;
    default:
      return nullptr;
  }
}

bNodeTree *ntreeFromID(ID *id)
{
  bNodeTree **nodetree = BKE_ntree_ptr_from_id(id);
  return (nodetree != nullptr) ? *nodetree : nullptr;
}

void ntreeNodeFlagSet(const bNodeTree *ntree, const int flag, const bool enable)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (enable) {
      node->flag |= flag;
    }
    else {
      node->flag &= ~flag;
    }
  }
}

bNodeTree *ntreeLocalize(bNodeTree *ntree, ID *new_owner_id)
{
  if (ntree == nullptr) {
    return nullptr;
  }

  /* Make full copy outside of Main database.
   * NOTE: previews are not copied here. */
  bNodeTree *ltree = reinterpret_cast<bNodeTree *>(
      BKE_id_copy_in_lib(nullptr,
                         std::nullopt,
                         &ntree->id,
                         new_owner_id,
                         nullptr,
                         (LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA)));

  ltree->id.tag |= LIB_TAG_LOCALIZED;

  LISTBASE_FOREACH (bNode *, node, &ltree->nodes) {
    bNodeTree *group = reinterpret_cast<bNodeTree *>(node->id);
    if (node->is_group() && group != nullptr) {
      node->id = reinterpret_cast<ID *>(ntreeLocalize(group, nullptr));
    }
  }

  /* Ensures only a single output node is enabled. */
  ntreeSetOutput(ntree);

  bNode *node_src = reinterpret_cast<bNode *>(ntree->nodes.first);
  bNode *node_local = reinterpret_cast<bNode *>(ltree->nodes.first);
  while (node_src != nullptr) {
    node_local->runtime->original = node_src;
    node_src = node_src->next;
    node_local = node_local->next;
  }

  if (ntree->typeinfo->localize) {
    ntree->typeinfo->localize(ltree, ntree);
  }

  return ltree;
}

void ntreeLocalMerge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree)
{
  if (ntree && localtree) {
    if (ntree->typeinfo->local_merge) {
      ntree->typeinfo->local_merge(bmain, localtree, ntree);
    }
  }
}

static bool ntree_contains_tree_exec(const bNodeTree *tree_to_search_in,
                                     const bNodeTree *tree_to_search_for,
                                     Set<const bNodeTree *> &already_passed)
{
  if (tree_to_search_in == tree_to_search_for) {
    return true;
  }

  tree_to_search_in->ensure_topology_cache();
  for (const bNode *node_group : tree_to_search_in->group_nodes()) {
    const bNodeTree *sub_tree_search_in = reinterpret_cast<const bNodeTree *>(node_group->id);
    if (!sub_tree_search_in) {
      continue;
    }
    if (!already_passed.add(sub_tree_search_in)) {
      continue;
    }
    if (ntree_contains_tree_exec(sub_tree_search_in, tree_to_search_for, already_passed)) {
      return true;
    }
  }

  return false;
}

bool ntreeContainsTree(const bNodeTree *tree_to_search_in, const bNodeTree *tree_to_search_for)
{
  if (tree_to_search_in == tree_to_search_for) {
    return true;
  }

  Set<const bNodeTree *> already_passed;
  return ntree_contains_tree_exec(tree_to_search_in, tree_to_search_for, already_passed);
}

int nodeCountSocketLinks(const bNodeTree *ntree, const bNodeSocket *sock)
{
  int tot = 0;
  LISTBASE_FOREACH (const bNodeLink *, link, &ntree->links) {
    if (link->fromsock == sock || link->tosock == sock) {
      tot++;
    }
  }
  return tot;
}

bNode *nodeGetActive(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return nullptr;
  }

  for (bNode *node : ntree->all_nodes()) {
    if (node->flag & NODE_ACTIVE) {
      return node;
    }
  }
  return nullptr;
}

bool nodeSetSelected(bNode *node, const bool select)
{
  bool changed = false;
  if (select != ((node->flag & NODE_SELECT) != 0)) {
    changed = true;
    SET_FLAG_FROM_TEST(node->flag, select, NODE_SELECT);
  }
  if (select) {
    return changed;
  }
  /* Deselect sockets too. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    changed |= (sock->flag & NODE_SELECT) != 0;
    sock->flag &= ~NODE_SELECT;
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    changed |= (sock->flag & NODE_SELECT) != 0;
    sock->flag &= ~NODE_SELECT;
  }
  return changed;
}

void nodeClearActive(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }

  for (bNode *node : ntree->all_nodes()) {
    node->flag &= ~NODE_ACTIVE;
  }
}

void nodeSetActive(bNodeTree *ntree, bNode *node)
{
  const bool is_paint_canvas = nodeSupportsActiveFlag(node, NODE_ACTIVE_PAINT_CANVAS);
  const bool is_texture_class = nodeSupportsActiveFlag(node, NODE_ACTIVE_TEXTURE);
  int flags_to_set = NODE_ACTIVE;
  SET_FLAG_FROM_TEST(flags_to_set, is_paint_canvas, NODE_ACTIVE_PAINT_CANVAS);
  SET_FLAG_FROM_TEST(flags_to_set, is_texture_class, NODE_ACTIVE_TEXTURE);

  /* Make sure only one node is active per node tree. */
  for (bNode *tnode : ntree->all_nodes()) {
    tnode->flag &= ~flags_to_set;
  }
  node->flag |= flags_to_set;
}

void nodeSetSocketAvailability(bNodeTree *ntree, bNodeSocket *sock, const bool is_available)
{
  if (is_available == sock->is_available()) {
    return;
  }
  if (is_available) {
    sock->flag &= ~SOCK_UNAVAIL;
  }
  else {
    sock->flag |= SOCK_UNAVAIL;
  }
  BKE_ntree_update_tag_socket_availability(ntree, sock);
}

int nodeSocketLinkLimit(const bNodeSocket *sock)
{
  if (sock->is_multi_input()) {
    return 4095;
  }
  if (sock->typeinfo == nullptr) {
    return sock->limit;
  }
  const bNodeSocketType &stype = *sock->typeinfo;
  if (!stype.use_link_limits_of_type) {
    return sock->limit;
  }
  return eNodeSocketInOut(sock->in_out) == SOCK_IN ? stype.input_link_limit :
                                                     stype.output_link_limit;
}

static void update_socket_declarations(ListBase *sockets,
                                       Span<nodes::SocketDeclaration *> declarations)
{
  int index;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, sockets, index) {
    const SocketDeclaration &socket_decl = *declarations[index];
    socket->runtime->declaration = &socket_decl;
  }
}

static void reset_socket_declarations(ListBase *sockets)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, sockets) {
    socket->runtime->declaration = nullptr;
  }
}

void nodeSocketDeclarationsUpdate(bNode *node)
{
  BLI_assert(node->runtime->declaration != nullptr);
  if (node->runtime->declaration->skip_updating_sockets) {
    reset_socket_declarations(&node->inputs);
    reset_socket_declarations(&node->outputs);
    return;
  }
  update_socket_declarations(&node->inputs, node->runtime->declaration->inputs);
  update_socket_declarations(&node->outputs, node->runtime->declaration->outputs);
}

bool nodeDeclarationEnsureOnOutdatedNode(bNodeTree *ntree, bNode *node)
{
  if (node->runtime->declaration != nullptr) {
    return false;
  }
  if (node->typeinfo->declare) {
    if (node->typeinfo->static_declaration) {
      if (!node->typeinfo->static_declaration->is_context_dependent) {
        node->runtime->declaration = node->typeinfo->static_declaration;
        return true;
      }
    }
  }
  if (node->typeinfo->declare) {
    BLI_assert(ntree != nullptr);
    BLI_assert(node != nullptr);
    nodes::update_node_declaration_and_sockets(*ntree, *node);
    return true;
  }
  return false;
}

bool nodeDeclarationEnsure(bNodeTree *ntree, bNode *node)
{
  if (nodeDeclarationEnsureOnOutdatedNode(ntree, node)) {
    nodeSocketDeclarationsUpdate(node);
    return true;
  }
  return false;
}

void nodeDimensionsGet(const bNode *node, float *r_width, float *r_height)
{
  *r_width = node->runtime->totr.xmax - node->runtime->totr.xmin;
  *r_height = node->runtime->totr.ymax - node->runtime->totr.ymin;
}

void nodeTagUpdateID(bNode *node)
{
  node->runtime->update |= NODE_UPDATE_ID;
}

void nodeInternalLinks(bNode *node, bNodeLink **r_links, int *r_len)
{
  *r_links = node->runtime->internal_links.data();
  *r_len = node->runtime->internal_links.size();
}

/* Node Instance Hash */

const bNodeInstanceKey NODE_INSTANCE_KEY_BASE = {5381};
const bNodeInstanceKey NODE_INSTANCE_KEY_NONE = {0};

/* Generate a hash key from ntree and node names
 * Uses the djb2 algorithm with xor by Bernstein:
 * http://www.cse.yorku.ca/~oz/hash.html
 */
static bNodeInstanceKey node_hash_int_str(bNodeInstanceKey hash, const char *str)
{
  char c;

  while ((c = *str++)) {
    hash.value = ((hash.value << 5) + hash.value) ^ c; /* (hash * 33) ^ c */
  }

  /* separator '\0' character, to avoid ambiguity from concatenated strings */
  hash.value = (hash.value << 5) + hash.value; /* hash * 33 */

  return hash;
}

bNodeInstanceKey BKE_node_instance_key(bNodeInstanceKey parent_key,
                                       const bNodeTree *ntree,
                                       const bNode *node)
{
  bNodeInstanceKey key = node_hash_int_str(parent_key, ntree->id.name + 2);

  if (node) {
    key = node_hash_int_str(key, node->name);
  }

  return key;
}

static uint node_instance_hash_key(const void *key)
{
  return static_cast<const bNodeInstanceKey *>(key)->value;
}

static bool node_instance_hash_key_cmp(const void *a, const void *b)
{
  uint value_a = static_cast<const bNodeInstanceKey *>(a)->value;
  uint value_b = static_cast<const bNodeInstanceKey *>(b)->value;

  return (value_a != value_b);
}

bNodeInstanceHash *BKE_node_instance_hash_new(const char *info)
{
  bNodeInstanceHash *hash = static_cast<bNodeInstanceHash *>(
      MEM_mallocN(sizeof(bNodeInstanceHash), info));
  hash->ghash = BLI_ghash_new(
      node_instance_hash_key, node_instance_hash_key_cmp, "node instance hash ghash");
  return hash;
}

void BKE_node_instance_hash_free(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp)
{
  BLI_ghash_free(hash->ghash, nullptr, reinterpret_cast<GHashValFreeFP>(valfreefp));
  MEM_freeN(hash);
}

void BKE_node_instance_hash_insert(bNodeInstanceHash *hash, bNodeInstanceKey key, void *value)
{
  bNodeInstanceHashEntry *entry = static_cast<bNodeInstanceHashEntry *>(value);
  entry->key = key;
  entry->tag = 0;
  BLI_ghash_insert(hash->ghash, &entry->key, value);
}

void *BKE_node_instance_hash_lookup(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
  return BLI_ghash_lookup(hash->ghash, &key);
}

int BKE_node_instance_hash_remove(bNodeInstanceHash *hash,
                                  bNodeInstanceKey key,
                                  bNodeInstanceValueFP valfreefp)
{
  return BLI_ghash_remove(hash->ghash, &key, nullptr, reinterpret_cast<GHashValFreeFP>(valfreefp));
}

void BKE_node_instance_hash_clear(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp)
{
  BLI_ghash_clear(hash->ghash, nullptr, reinterpret_cast<GHashValFreeFP>(valfreefp));
}

void *BKE_node_instance_hash_pop(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
  return BLI_ghash_popkey(hash->ghash, &key, nullptr);
}

int BKE_node_instance_hash_haskey(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
  return BLI_ghash_haskey(hash->ghash, &key);
}

int BKE_node_instance_hash_size(bNodeInstanceHash *hash)
{
  return BLI_ghash_len(hash->ghash);
}

void BKE_node_instance_hash_clear_tags(bNodeInstanceHash *hash)
{
  bNodeInstanceHashIterator iter;

  NODE_INSTANCE_HASH_ITER (iter, hash) {
    bNodeInstanceHashEntry *value = static_cast<bNodeInstanceHashEntry *>(
        node_instance_hash_iterator_get_value(&iter));

    value->tag = 0;
  }
}

void BKE_node_instance_hash_tag(bNodeInstanceHash * /*hash*/, void *value)
{
  bNodeInstanceHashEntry *entry = static_cast<bNodeInstanceHashEntry *>(value);
  entry->tag = 1;
}

bool BKE_node_instance_hash_tag_key(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
  bNodeInstanceHashEntry *entry = static_cast<bNodeInstanceHashEntry *>(
      BKE_node_instance_hash_lookup(hash, key));

  if (entry) {
    entry->tag = 1;
    return true;
  }

  return false;
}

void BKE_node_instance_hash_remove_untagged(bNodeInstanceHash *hash,
                                            bNodeInstanceValueFP valfreefp)
{
  /* NOTE: Hash must not be mutated during iterating!
   * Store tagged entries in a separate list and remove items afterward.
   */
  bNodeInstanceKey *untagged = static_cast<bNodeInstanceKey *>(
      MEM_mallocN(sizeof(bNodeInstanceKey) * BKE_node_instance_hash_size(hash),
                  "temporary node instance key list"));
  bNodeInstanceHashIterator iter;
  int num_untagged = 0;
  NODE_INSTANCE_HASH_ITER (iter, hash) {
    bNodeInstanceHashEntry *value = static_cast<bNodeInstanceHashEntry *>(
        node_instance_hash_iterator_get_value(&iter));

    if (!value->tag) {
      untagged[num_untagged++] = node_instance_hash_iterator_get_key(&iter);
    }
  }

  for (int i = 0; i < num_untagged; i++) {
    BKE_node_instance_hash_remove(hash, untagged[i], valfreefp);
  }

  MEM_freeN(untagged);
}

/* Build a set of built-in node types to check for known types. */
static Set<int> get_known_node_types_set()
{
  Set<int> result;
  NODE_TYPES_BEGIN (ntype) {
    result.add(ntype->type);
  }
  NODE_TYPES_END;
  return result;
}

static bool can_read_node_type(const int type)
{
  /* Can always read custom node types. */
  if (ELEM(type, NODE_CUSTOM, NODE_CUSTOM_GROUP)) {
    return true;
  }

  /* Check known built-in types. */
  static Set<int> known_types = get_known_node_types_set();
  return known_types.contains(type);
}

static void node_replace_undefined_types(bNode *node)
{
  /* If the integer type is unknown then this node cannot be read. */
  if (!can_read_node_type(node->type)) {
    node->type = NODE_CUSTOM;
    /* This type name is arbitrary, it just has to be unique enough to not match a future node
     * idname. Includes the old type identifier for debugging purposes. */
    const std::string old_idname = node->idname;
    SNPRINTF(node->idname, "Undefined[%s]", old_idname.c_str());
    node->typeinfo = &NodeTypeUndefined;
  }
}

void ntreeUpdateAllNew(Main *main)
{
  /* Replace unknown node types with "Undefined".
   * This happens when loading files from newer Blender versions. Such nodes cannot be read
   * reliably so replace the idname with an undefined type. This keeps links and socket names but
   * discards storage and other type-specific data.
   *
   * Replacement has to happen after after-liblink-versioning, since some node types still get
   * replaced in those late versioning steps. */
  FOREACH_NODETREE_BEGIN (main, ntree, owner_id) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      node_replace_undefined_types(node);
    }
  }
  FOREACH_NODETREE_END;
  /* Update all new node trees on file read or append, to add/remove sockets
   * in groups nodes if the group changed, and handle any update flags that
   * might have been set in file reading or versioning. */
  FOREACH_NODETREE_BEGIN (main, ntree, owner_id) {
    if (owner_id->tag & LIB_TAG_NEW) {
      BKE_ntree_update_tag_all(ntree);
    }
  }
  FOREACH_NODETREE_END;
  BKE_ntree_update_main(main, nullptr);
}

void ntreeUpdateAllUsers(Main *main, ID *id)
{
  if (id == nullptr) {
    return;
  }

  bool need_update = false;

  /* Update all users of ngroup, to add/remove sockets as needed. */
  FOREACH_NODETREE_BEGIN (main, ntree, owner_id) {
    for (bNode *node : ntree->all_nodes()) {
      if (node->id == id) {
        BKE_ntree_update_tag_node_property(ntree, node);
        need_update = true;
      }
    }
  }
  FOREACH_NODETREE_END;
  if (need_update) {
    BKE_ntree_update_main(main, nullptr);
  }
}

/* ************* node type access ********** */

void nodeLabel(const bNodeTree *ntree, const bNode *node, char *label, const int label_maxncpy)
{
  label[0] = '\0';

  if (node->label[0] != '\0') {
    BLI_strncpy(label, node->label, label_maxncpy);
  }
  else if (node->typeinfo->labelfunc) {
    node->typeinfo->labelfunc(ntree, node, label, label_maxncpy);
  }
  if (label[0] != '\0') {
    /* The previous methods (labelfunc) could not provide an adequate label for the node. */
    return;
  }

  BLI_strncpy(label, IFACE_(node->typeinfo->ui_name), label_maxncpy);
}

const char *nodeSocketShortLabel(const bNodeSocket *sock)
{
  if (sock->runtime->declaration != nullptr) {
    StringRefNull short_label = sock->runtime->declaration->short_label;
    if (!short_label.is_empty()) {
      return sock->runtime->declaration->short_label.data();
    }
  }
  return nullptr;
}

const char *nodeSocketLabel(const bNodeSocket *sock)
{
  return (sock->label[0] != '\0') ? sock->label : sock->name;
}

static void node_type_base_defaults(bNodeType *ntype)
{
  /* default size values */
  node_type_size_preset(ntype, eNodeSizePreset::Default);
  ntype->height = 100;
  ntype->minheight = 30;
  ntype->maxheight = FLT_MAX;
}

/* allow this node for any tree type */
static bool node_poll_default(const bNodeType * /*ntype*/,
                              const bNodeTree * /*ntree*/,
                              const char ** /*disabled_hint*/)
{
  return true;
}

static bool node_poll_instance_default(const bNode *node,
                                       const bNodeTree *ntree,
                                       const char **disabled_hint)
{
  return node->typeinfo->poll(node->typeinfo, ntree, disabled_hint);
}

void node_type_base(bNodeType *ntype, const int type, const char *name, const short nclass)
{
  /* Use static type info header to map static int type to identifier string and RNA struct type.
   * Associate the RNA struct type with the bNodeType.
   * Dynamically registered nodes will create an RNA type at runtime
   * and call RNA_struct_blender_type_set, so this only needs to be done for old RNA types
   * created in makesrna, which can not be associated to a bNodeType immediately,
   * since bNodeTypes are registered afterward ...
   */
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
  case ID: { \
    STRNCPY(ntype->idname, #Category #StructName); \
    StructRNA *srna = RNA_struct_find(#Category #StructName); \
    BLI_assert(srna != nullptr); \
    ntype->rna_ext.srna = srna; \
    RNA_struct_blender_type_set(srna, ntype); \
    RNA_def_struct_ui_text(srna, UIName, UIDesc); \
    ntype->enum_name_legacy = EnumName; \
    STRNCPY(ntype->ui_description, UIDesc); \
    break; \
  }

  switch (type) {
#include "NOD_static_types.h"
  }

  /* make sure we have a valid type (everything registered) */
  BLI_assert(ntype->idname[0] != '\0');

  ntype->type = type;
  STRNCPY(ntype->ui_name, name);
  ntype->nclass = nclass;

  node_type_base_defaults(ntype);

  ntype->poll = node_poll_default;
  ntype->poll_instance = node_poll_instance_default;
}

void node_type_base_custom(bNodeType *ntype,
                           const char *idname,
                           const char *name,
                           const char *enum_name,
                           const short nclass)
{
  STRNCPY(ntype->idname, idname);
  ntype->type = NODE_CUSTOM;
  STRNCPY(ntype->ui_name, name);
  ntype->nclass = nclass;
  ntype->enum_name_legacy = enum_name;

  node_type_base_defaults(ntype);
}

std::optional<eCustomDataType> socket_type_to_custom_data_type(eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_FLOAT:
      return CD_PROP_FLOAT;
    case SOCK_VECTOR:
      return CD_PROP_FLOAT3;
    case SOCK_RGBA:
      return CD_PROP_COLOR;
    case SOCK_BOOLEAN:
      return CD_PROP_BOOL;
    case SOCK_ROTATION:
      return CD_PROP_QUATERNION;
    case SOCK_MATRIX:
      return CD_PROP_FLOAT4X4;
    case SOCK_INT:
      return CD_PROP_INT32;
    case SOCK_STRING:
      return CD_PROP_STRING;
    default:
      return std::nullopt;
  }
}

std::optional<eNodeSocketDatatype> custom_data_type_to_socket_type(eCustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT:
      return SOCK_FLOAT;
    case CD_PROP_INT8:
      return SOCK_INT;
    case CD_PROP_INT32:
      return SOCK_INT;
    case CD_PROP_FLOAT3:
      return SOCK_VECTOR;
    case CD_PROP_FLOAT2:
      return SOCK_VECTOR;
    case CD_PROP_BOOL:
      return SOCK_BOOLEAN;
    case CD_PROP_COLOR:
      return SOCK_RGBA;
    case CD_PROP_BYTE_COLOR:
      return SOCK_RGBA;
    case CD_PROP_QUATERNION:
      return SOCK_ROTATION;
    case CD_PROP_FLOAT4X4:
      return SOCK_MATRIX;
    default:
      return std::nullopt;
  }
}

static const CPPType *slow_socket_type_to_geo_nodes_base_cpp_type(const eNodeSocketDatatype type)
{
  const char *socket_idname = nodeStaticSocketType(type, 0);
  const bNodeSocketType *typeinfo = nodeSocketTypeFind(socket_idname);
  return typeinfo->base_cpp_type;
}

const CPPType *socket_type_to_geo_nodes_base_cpp_type(const eNodeSocketDatatype type)
{
  const CPPType *cpp_type;
  switch (type) {
    case SOCK_FLOAT:
      cpp_type = &CPPType::get<float>();
      break;
    case SOCK_INT:
      cpp_type = &CPPType::get<int>();
      break;
    case SOCK_RGBA:
      cpp_type = &CPPType::get<ColorGeometry4f>();
      break;
    case SOCK_BOOLEAN:
      cpp_type = &CPPType::get<bool>();
      break;
    case SOCK_VECTOR:
      cpp_type = &CPPType::get<float3>();
      break;
    case SOCK_ROTATION:
      cpp_type = &CPPType::get<math::Quaternion>();
      break;
    case SOCK_MATRIX:
      cpp_type = &CPPType::get<float4x4>();
      break;
    default:
      cpp_type = slow_socket_type_to_geo_nodes_base_cpp_type(type);
      break;
  }
  BLI_assert(cpp_type == slow_socket_type_to_geo_nodes_base_cpp_type(type));
  return cpp_type;
}

std::optional<eNodeSocketDatatype> geo_nodes_base_cpp_type_to_socket_type(const CPPType &type)
{
  if (type.is<float>()) {
    return SOCK_FLOAT;
  }
  if (type.is<int>()) {
    return SOCK_INT;
  }
  if (type.is<float3>()) {
    return SOCK_VECTOR;
  }
  if (type.is<ColorGeometry4f>()) {
    return SOCK_RGBA;
  }
  if (type.is<bool>()) {
    return SOCK_BOOLEAN;
  }
  if (type.is<math::Quaternion>()) {
    return SOCK_ROTATION;
  }
  if (type.is<float4x4>()) {
    return SOCK_MATRIX;
  }
  if (type.is<std::string>()) {
    return SOCK_STRING;
  }
  return std::nullopt;
}

std::optional<VolumeGridType> socket_type_to_grid_type(const eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_BOOLEAN:
      return VOLUME_GRID_BOOLEAN;
    case SOCK_FLOAT:
      return VOLUME_GRID_FLOAT;
    case SOCK_INT:
      return VOLUME_GRID_INT;
    case SOCK_VECTOR:
      return VOLUME_GRID_VECTOR_FLOAT;
    default:
      return std::nullopt;
  }
}

std::optional<eNodeSocketDatatype> grid_type_to_socket_type(const VolumeGridType type)
{
  switch (type) {
    case VOLUME_GRID_BOOLEAN:
      return SOCK_BOOLEAN;
    case VOLUME_GRID_FLOAT:
      return SOCK_FLOAT;
    case VOLUME_GRID_INT:
      return SOCK_INT;
    case VOLUME_GRID_VECTOR_FLOAT:
      return SOCK_VECTOR;
    default:
      return std::nullopt;
  }
}

struct SocketTemplateIdentifierCallbackData {
  bNodeSocketTemplate *list;
  bNodeSocketTemplate *ntemp;
};

static bool unique_socket_template_identifier_check(void *arg, const char *name)
{
  const SocketTemplateIdentifierCallbackData *data =
      static_cast<const SocketTemplateIdentifierCallbackData *>(arg);

  for (bNodeSocketTemplate *ntemp = data->list; ntemp->type >= 0; ntemp++) {
    if (ntemp != data->ntemp) {
      if (STREQ(ntemp->identifier, name)) {
        return true;
      }
    }
  }

  return false;
}

static void unique_socket_template_identifier(bNodeSocketTemplate *list,
                                              bNodeSocketTemplate *ntemp,
                                              const char defname[],
                                              const char delim)
{
  SocketTemplateIdentifierCallbackData data;
  data.list = list;
  data.ntemp = ntemp;

  BLI_uniquename_cb(unique_socket_template_identifier_check,
                    &data,
                    defname,
                    delim,
                    ntemp->identifier,
                    sizeof(ntemp->identifier));
}

void node_type_socket_templates(bNodeType *ntype,
                                bNodeSocketTemplate *inputs,
                                bNodeSocketTemplate *outputs)
{
  ntype->inputs = inputs;
  ntype->outputs = outputs;

  /* automatically generate unique identifiers */
  if (inputs) {
    /* clear identifier strings (uninitialized memory) */
    for (bNodeSocketTemplate *ntemp = inputs; ntemp->type >= 0; ntemp++) {
      ntemp->identifier[0] = '\0';
    }

    for (bNodeSocketTemplate *ntemp = inputs; ntemp->type >= 0; ntemp++) {
      STRNCPY(ntemp->identifier, ntemp->name);
      unique_socket_template_identifier(inputs, ntemp, ntemp->identifier, '_');
    }
  }
  if (outputs) {
    /* clear identifier strings (uninitialized memory) */
    for (bNodeSocketTemplate *ntemp = outputs; ntemp->type >= 0; ntemp++) {
      ntemp->identifier[0] = '\0';
    }

    for (bNodeSocketTemplate *ntemp = outputs; ntemp->type >= 0; ntemp++) {
      STRNCPY(ntemp->identifier, ntemp->name);
      unique_socket_template_identifier(outputs, ntemp, ntemp->identifier, '_');
    }
  }
}

void node_type_size(bNodeType *ntype, const int width, const int minwidth, const int maxwidth)
{
  ntype->width = width;
  ntype->minwidth = minwidth;
  if (maxwidth <= minwidth) {
    ntype->maxwidth = FLT_MAX;
  }
  else {
    ntype->maxwidth = maxwidth;
  }
}

void node_type_size_preset(bNodeType *ntype, const eNodeSizePreset size)
{
  switch (size) {
    case eNodeSizePreset::Default:
      node_type_size(ntype, 140, 100, NODE_DEFAULT_MAX_WIDTH);
      break;
    case eNodeSizePreset::Small:
      node_type_size(ntype, 100, 80, NODE_DEFAULT_MAX_WIDTH);
      break;
    case eNodeSizePreset::Middle:
      node_type_size(ntype, 150, 120, NODE_DEFAULT_MAX_WIDTH);
      break;
    case eNodeSizePreset::Large:
      node_type_size(ntype, 240, 140, NODE_DEFAULT_MAX_WIDTH);
      break;
  }
}

void node_type_storage(bNodeType *ntype,
                       const char *storagename,
                       void (*freefunc)(bNode *node),
                       void (*copyfunc)(bNodeTree *dest_ntree,
                                        bNode *dest_node,
                                        const bNode *src_node))
{
  if (storagename) {
    STRNCPY(ntype->storagename, storagename);
  }
  else {
    ntype->storagename[0] = '\0';
  }
  ntype->copyfunc = copyfunc;
  ntype->freefunc = freefunc;
}

void BKE_node_system_init()
{
  nodetreetypes_hash = BLI_ghash_str_new("nodetreetypes_hash gh");
  nodetypes_hash = BLI_ghash_str_new("nodetypes_hash gh");
  nodetypes_alias_hash = BLI_ghash_str_new("nodetypes_alias_hash gh");
  nodesockettypes_hash = BLI_ghash_str_new("nodesockettypes_hash gh");

  register_nodes();
}

void BKE_node_system_exit()
{
  if (nodetypes_alias_hash) {
    BLI_ghash_free(nodetypes_alias_hash, MEM_freeN, MEM_freeN);
    nodetypes_alias_hash = nullptr;
  }

  if (nodetypes_hash) {
    NODE_TYPES_BEGIN (nt) {
      if (nt->rna_ext.free) {
        nt->rna_ext.free(nt->rna_ext.data);
      }
    }
    NODE_TYPES_END;

    BLI_ghash_free(nodetypes_hash, nullptr, node_free_type);
    nodetypes_hash = nullptr;
  }

  if (nodesockettypes_hash) {
    NODE_SOCKET_TYPES_BEGIN (st) {
      if (st->ext_socket.free) {
        st->ext_socket.free(st->ext_socket.data);
      }
      if (st->ext_interface.free) {
        st->ext_interface.free(st->ext_interface.data);
      }
    }
    NODE_SOCKET_TYPES_END;

    BLI_ghash_free(nodesockettypes_hash, nullptr, node_free_socket_type);
    nodesockettypes_hash = nullptr;
  }

  if (nodetreetypes_hash) {
    NODE_TREE_TYPES_BEGIN (nt) {
      if (nt->rna_ext.free) {
        nt->rna_ext.free(nt->rna_ext.data);
      }
    }
    NODE_TREE_TYPES_END;

    BLI_ghash_free(nodetreetypes_hash, nullptr, ntree_free_type);
    nodetreetypes_hash = nullptr;
  }
}

/* -------------------------------------------------------------------- */
/* NodeTree Iterator Helpers (FOREACH_NODETREE_BEGIN) */

void BKE_node_tree_iter_init(NodeTreeIterStore *ntreeiter, Main *bmain)
{
  ntreeiter->ngroup = (bNodeTree *)bmain->nodetrees.first;
  ntreeiter->scene = (Scene *)bmain->scenes.first;
  ntreeiter->mat = (Material *)bmain->materials.first;
  ntreeiter->tex = (Tex *)bmain->textures.first;
  ntreeiter->light = (Light *)bmain->lights.first;
  ntreeiter->world = (World *)bmain->worlds.first;
  ntreeiter->linestyle = (FreestyleLineStyle *)bmain->linestyles.first;
}
bool BKE_node_tree_iter_step(NodeTreeIterStore *ntreeiter, bNodeTree **r_nodetree, ID **r_id)
{
  if (ntreeiter->ngroup) {
    bNodeTree &node_tree = *reinterpret_cast<bNodeTree *>(ntreeiter->ngroup);
    *r_nodetree = &node_tree;
    *r_id = &node_tree.id;
    ntreeiter->ngroup = reinterpret_cast<bNodeTree *>(node_tree.id.next);
  }
  else if (ntreeiter->scene) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->scene->nodetree);
    *r_id = &ntreeiter->scene->id;
    ntreeiter->scene = reinterpret_cast<Scene *>(ntreeiter->scene->id.next);
  }
  else if (ntreeiter->mat) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->mat->nodetree);
    *r_id = &ntreeiter->mat->id;
    ntreeiter->mat = reinterpret_cast<Material *>(ntreeiter->mat->id.next);
  }
  else if (ntreeiter->tex) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->tex->nodetree);
    *r_id = &ntreeiter->tex->id;
    ntreeiter->tex = reinterpret_cast<Tex *>(ntreeiter->tex->id.next);
  }
  else if (ntreeiter->light) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->light->nodetree);
    *r_id = &ntreeiter->light->id;
    ntreeiter->light = reinterpret_cast<Light *>(ntreeiter->light->id.next);
  }
  else if (ntreeiter->world) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->world->nodetree);
    *r_id = &ntreeiter->world->id;
    ntreeiter->world = reinterpret_cast<World *>(ntreeiter->world->id.next);
  }
  else if (ntreeiter->linestyle) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->linestyle->nodetree);
    *r_id = &ntreeiter->linestyle->id;
    ntreeiter->linestyle = reinterpret_cast<FreestyleLineStyle *>(ntreeiter->linestyle->id.next);
  }
  else {
    return false;
  }

  return true;
}

void BKE_nodetree_remove_layer_n(bNodeTree *ntree, Scene *scene, const int layer_index)
{
  BLI_assert(layer_index != -1);
  BLI_assert(scene != nullptr);
  for (bNode *node : ntree->all_nodes()) {
    if (node->type == CMP_NODE_R_LAYERS && node->id == &scene->id) {
      if (node->custom1 == layer_index) {
        node->custom1 = 0;
      }
      else if (node->custom1 > layer_index) {
        node->custom1--;
      }
    }
  }
}

}  // namespace blender::bke
