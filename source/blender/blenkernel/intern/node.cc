/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_world_types.h"

#include "BLI_color.hh"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_time.h"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"
#include "BLT_translation.hh"

#include "IMB_imbuf.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_asset.hh"
#include "BKE_bpath.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_image_format.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_interface.hh"
#include "BKE_node_tree_reference_lifetimes.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_preview_image.hh"
#include "BKE_type_conversions.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "NOD_common.hh"
#include "NOD_composite.hh"
#include "NOD_geo_bake.hh"
#include "NOD_geo_bundle.hh"
#include "NOD_geo_capture_attribute.hh"
#include "NOD_geo_closure.hh"
#include "NOD_geo_foreach_geometry_element.hh"
#include "NOD_geo_index_switch.hh"
#include "NOD_geo_menu_switch.hh"
#include "NOD_geo_repeat.hh"
#include "NOD_geo_simulation.hh"
#include "NOD_geometry_nodes_dependencies.hh"
#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_menu_value.hh"
#include "NOD_node_declaration.hh"
#include "NOD_register.hh"
#include "NOD_shader.h"
#include "NOD_socket.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_texture.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "BLO_read_write.hh"

using blender::nodes::FieldInferencingInterface;
using blender::nodes::InputSocketFieldType;
using blender::nodes::NodeDeclaration;
using blender::nodes::OutputFieldDependency;
using blender::nodes::OutputSocketFieldType;
using blender::nodes::SocketDeclaration;

static CLG_LogRef LOG = {"node"};

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
  ntree->default_group_node_width = GROUP_NODE_DEFAULT_WIDTH;
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
        ntree_dst, *src_node, flag_subdata, src_node->name, src_node->identifier, socket_map);
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
    node_declaration_ensure(*ntree_dst, *node);
  }

  ntree_dst->tree_interface.copy_data(ntree_src->tree_interface, flag);
  /* copy preview hash */
  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    for (const auto &item : ntree_src->runtime->previews.items()) {
      dst_runtime.previews.add_new(item.key, item.value);
    }
  }

  if (ntree_src->runtime->field_inferencing_interface) {
    dst_runtime.field_inferencing_interface = std::make_unique<FieldInferencingInterface>(
        *ntree_src->runtime->field_inferencing_interface);
  }
  if (ntree_src->runtime->structure_type_interface) {
    dst_runtime.structure_type_interface = std::make_unique<nodes::StructureTypeInterface>(
        *ntree_src->runtime->structure_type_interface);
  }
  if (ntree_src->runtime->reference_lifetimes_info) {
    using namespace node_tree_reference_lifetimes;
    dst_runtime.reference_lifetimes_info = std::make_unique<ReferenceLifetimesInfo>(
        *ntree_src->runtime->reference_lifetimes_info);
    for (ReferenceSetInfo &reference_set : dst_runtime.reference_lifetimes_info->reference_sets) {
      if (ELEM(reference_set.type,
               ReferenceSetType::LocalReferenceSet,
               ReferenceSetType::ClosureInputReferenceSet,
               ReferenceSetType::ClosureOutputData))
      {
        reference_set.socket = socket_map.lookup(reference_set.socket);
      }
      for (auto &socket : reference_set.potential_data_origins) {
        socket = socket_map.lookup(socket);
      }
    }
  }

  if (ntree_src->geometry_node_asset_traits) {
    ntree_dst->geometry_node_asset_traits = MEM_dupallocN<GeometryNodeAssetTraits>(
        __func__, *ntree_src->geometry_node_asset_traits);
  }

  if (ntree_src->nested_node_refs) {
    ntree_dst->nested_node_refs = MEM_malloc_arrayN<bNestedNodeRef>(
        size_t(ntree_src->nested_node_refs_num), __func__);
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

  /* Iterate backwards because this allows for more efficient node deletion while keeping
   * bNodeTreeRuntime::nodes_by_id valid. */
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNode *, node, &ntree->nodes) {
    node_free_node(ntree, *node);
  }

  ntree->tree_interface.free_data();

  if (ntree->id.tag & ID_TAG_LOCALIZED) {
    BKE_libblock_free_data(&ntree->id, true);
  }

  if (ntree->geometry_node_asset_traits) {
    MEM_freeN(ntree->geometry_node_asset_traits);
  }

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
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
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
  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
      data,
      IDP_foreach_property(node->system_properties, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
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

  if (ntree->runtime->geometry_nodes_eval_dependencies) {
    for (ID *&id_ref : ntree->runtime->geometry_nodes_eval_dependencies->ids.values()) {
      BKE_LIB_FOREACHID_PROCESS_ID(data, id_ref, IDWALK_CB_HASH_IGNORE);
    }
  }
}

static void node_foreach_cache(ID *id,
                               IDTypeForeachCacheFunctionCallback function_callback,
                               void *user_data)
{
  bNodeTree *nodetree = reinterpret_cast<bNodeTree *>(id);
  IDCacheKey key = {0};
  key.id_session_uid = id->session_uid;

  if (nodetree->type == NTREE_COMPOSIT) {
    for (bNode *node : nodetree->all_nodes()) {
      if (node->type_legacy == CMP_NODE_MOVIEDISTORTION) {
        key.identifier = size_t(BLI_ghashutil_strhash_p(node->name));
        function_callback(id, &key, (&node->storage), 0, user_data);
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
        if (node->type_legacy == SH_NODE_SCRIPT) {
          NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);
          if (nss->mode == NODE_SCRIPT_EXTERNAL && nss->filepath[0]) {
            BKE_bpath_foreach_path_fixed_process(bpath_data, nss->filepath, sizeof(nss->filepath));
          }
        }
        else if (node->type_legacy == SH_NODE_TEX_IES) {
          NodeShaderTexIES *ies = static_cast<NodeShaderTexIES *>(node->storage);
          if (ies->mode == NODE_IES_EXTERNAL && ies->filepath[0]) {
            BKE_bpath_foreach_path_fixed_process(bpath_data, ies->filepath, sizeof(ies->filepath));
          }
        }
      }
      break;
    }
    case NTREE_GEOMETRY: {
      ntree->ensure_topology_cache(); /* Otherwise node->input_sockets() doesn't work. */
      for (bNode *node : ntree->all_nodes()) {
        for (bNodeSocket *socket : node->input_sockets()) {
          /* Find file path input sockets. */
          if (socket->type != SOCK_STRING) {
            continue;
          }
          bNodeSocketValueString *socket_value = static_cast<bNodeSocketValueString *>(
              socket->default_value);
          if (socket_value->value[0] == '\0' || socket_value->subtype != PROP_FILEPATH) {
            continue;
          }

          /* Process the file path. */
          BKE_bpath_foreach_path_fixed_process(
              bpath_data, socket_value->value, sizeof(socket_value->value));
        }
      }
      break;
    }
    default:
      break;
  }
}

static void node_foreach_working_space_color(ID *id, const IDTypeForeachColorFunctionCallback &fn)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  ntree->ensure_topology_cache();
  ntree->ensure_interface_cache();

  for (bNode *node : ntree->all_nodes()) {
    /* Hardcoded exception for some non-color data. Should ideally be a RNA property subtype,
     * as this won't work with group nodes*/
    if (node->type_legacy == SH_NODE_NORMAL_MAP ||
        node->type_legacy == SH_NODE_VECTOR_DISPLACEMENT)
    {
      continue;
    }

    for (bNodeSocket *socket : node->input_sockets()) {
      if (socket->type == SOCK_RGBA && socket->default_value) {
        bNodeSocketValueRGBA *rgba = static_cast<bNodeSocketValueRGBA *>(socket->default_value);
        fn.single(rgba->value);
      }
      /* Exception for subsurface radius which is color-like and may be outside the 0..1 range. */
      else if (socket->type == SOCK_VECTOR && socket->default_value &&
               (STREQ(socket->name, "Subsurface Radius") ||
                STREQ(socket->name, "Subsurface Radius Scale") ||
                (node->type_legacy == SH_NODE_SUBSURFACE_SCATTERING &&
                 STREQ(socket->name, "Radius"))))
      {
        bNodeSocketValueVector *vec = static_cast<bNodeSocketValueVector *>(socket->default_value);
        float length;
        blender::float3 radius = blender::math::normalize_and_get_length(
            blender::float3(vec->value), length);
        fn.single(radius);
        copy_v3_v3(vec->value, radius * length);
      }
    }
    /* For the RGB shader node that stores color in an output socket. */
    for (bNodeSocket *socket : node->output_sockets()) {
      if (socket->type == SOCK_RGBA && socket->default_value) {
        bNodeSocketValueRGBA *rgba = static_cast<bNodeSocketValueRGBA *>(socket->default_value);
        fn.single(rgba->value);
      }
    }

    /* Most colors are in sockets, but a few exceptions. */
    if (node->type_legacy == FN_NODE_INPUT_COLOR) {
      NodeInputColor *input_color_storage = static_cast<NodeInputColor *>(node->storage);
      fn.single(input_color_storage->color);
    }
    else if (ELEM(node->type_legacy, TEX_NODE_VALTORGB, SH_NODE_VALTORGB)) {
      ColorBand *coba = static_cast<ColorBand *>(node->storage);
      BKE_colorband_foreach_working_space_color(coba, fn);
    }
  }

  for (bNodeTreeInterfaceSocket *socket : ntree->interface_inputs()) {
    const blender::bke::bNodeSocketType *typeinfo = socket->socket_typeinfo();
    if (typeinfo && typeinfo->type == SOCK_RGBA && socket->socket_data) {
      bNodeSocketValueRGBA *rgba = static_cast<bNodeSocketValueRGBA *>(socket->socket_data);
      fn.single(rgba->value);
    }
  }
  for (bNodeTreeInterfaceSocket *socket : ntree->interface_outputs()) {
    const blender::bke::bNodeSocketType *typeinfo = socket->socket_typeinfo();
    if (typeinfo && typeinfo->type == SOCK_RGBA && socket->socket_data) {
      bNodeSocketValueRGBA *rgba = static_cast<bNodeSocketValueRGBA *>(socket->socket_data);
      fn.single(rgba->value);
    }
  }
}

static ID **node_owner_pointer_get(ID *id, const bool debug_relationship_assert)
{
  if ((id->flag & ID_FLAG_EMBEDDED_DATA) == 0) {
    return nullptr;
  }
  /* TODO: Sort this NO_MAIN or not for embedded node trees. See #86119. */
  // BLI_assert((id->tag & ID_TAG_NO_MAIN) == 0);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  if (debug_relationship_assert) {
    BLI_assert(ntree->owner_id != nullptr);
    BLI_assert(node_tree_from_id(ntree->owner_id) == ntree);
  }

  return &ntree->owner_id;
}

namespace forward_compat {

static void update_node_location_legacy(bNodeTree &ntree)
{
  for (bNode *node : ntree.all_nodes()) {
    node->locx_legacy = node->location[0];
    node->locy_legacy = node->location[1];
    if (const bNode *parent = node->parent) {
      node->locx_legacy -= parent->location[0];
      node->locy_legacy -= parent->location[1];
    }
  }
}

static void write_legacy_properties(bNodeTree &ntree)
{
  switch (ntree.type) {
    case NTREE_GEOMETRY: {
      for (bNode *node : ntree.all_nodes()) {
        if (node->type_legacy == GEO_NODE_TRANSFORM_GEOMETRY) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Mode");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_POINTS_TO_VOLUME) {
          auto &storage = *static_cast<NodeGeometryPointsToVolume *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Resolution Mode");
          storage.resolution_mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_TRIANGULATE) {
          const bNodeSocket *quad_method_socket = node_find_socket(*node, SOCK_IN, "Quad Method");
          const bNodeSocket *ngon_method_socket = node_find_socket(*node, SOCK_IN, "N-gon Method");
          node->custom1 = quad_method_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          node->custom2 = ngon_method_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_VOLUME_TO_MESH) {
          auto &storage = *static_cast<NodeGeometryVolumeToMesh *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Resolution Mode");
          storage.resolution_mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_FILL_CURVE) {
          auto &storage = *static_cast<NodeGeometryCurveFill *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Mode");
          storage.mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_FILLET_CURVE) {
          auto &storage = *static_cast<NodeGeometryCurveFillet *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Mode");
          storage.mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_RESAMPLE_CURVE) {
          auto &storage = *static_cast<NodeGeometryCurveResample *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Mode");
          storage.mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME) {
          auto &storage = *static_cast<NodeGeometryDistributePointsInVolume *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Mode");
          storage.mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_MERGE_BY_DISTANCE) {
          auto &storage = *static_cast<NodeGeometryMergeByDistance *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Mode");
          storage.mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_MESH_TO_VOLUME) {
          auto &storage = *static_cast<NodeGeometryMeshToVolume *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Resolution Mode");
          storage.resolution_mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_RAYCAST) {
          auto &storage = *static_cast<NodeGeometryRaycast *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.mapping = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_REMOVE_ATTRIBUTE) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Pattern Mode");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_SAMPLE_GRID) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          node->custom2 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_SCALE_ELEMENTS) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Scale Mode");
          node->custom2 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_SET_CURVE_NORMAL) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Mode");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_SUBDIVISION_SURFACE) {
          auto &storage = *static_cast<NodeGeometrySubdivisionSurface *>(node->storage);
          const bNodeSocket *uv_smooth_socket = node_find_socket(*node, SOCK_IN, "UV Smooth");
          const bNodeSocket *boundary_smooth_socket = node_find_socket(
              *node, SOCK_IN, "Boundary Smooth");
          storage.uv_smooth = uv_smooth_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          storage.boundary_smooth =
              boundary_smooth_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_UV_PACK_ISLANDS) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Method");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == GEO_NODE_UV_UNWRAP) {
          auto &storage = *static_cast<NodeGeometryUVUnwrap *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Method");
          storage.method = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->is_type("FunctionNodeMatchString")) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Operation");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
      }
      break;
    }
    case NTREE_COMPOSIT: {
      for (bNode *node : ntree.all_nodes()) {
        if (node->type_legacy == CMP_NODE_BLUR) {
          auto &storage = *static_cast<NodeBlurData *>(node->storage);
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          storage.filtertype = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_FILTER) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_VIEW_LEVELS) {
          const bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Channel");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_DILATEERODE) {
          const bNodeSocket *type_socket = node_find_socket(*node, SOCK_IN, "Type");
          node->custom1 = type_socket->default_value_typed<bNodeSocketValueMenu>()->value;

          auto &storage = *static_cast<NodeDilateErode *>(node->storage);
          const bNodeSocket *falloff_socket = node_find_socket(*node, SOCK_IN, "Falloff");
          storage.falloff = falloff_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_TONEMAP) {
          auto &storage = *static_cast<NodeTonemap *>(node->storage);
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          storage.type = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_LENSDIST) {
          auto &storage = *static_cast<NodeLensDist *>(node->storage);
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          storage.distortion_type = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_KUWAHARA) {
          auto &storage = *static_cast<NodeKuwaharaData *>(node->storage);
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          storage.variation = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_DENOISE) {
          auto &storage = *static_cast<NodeDenoise *>(node->storage);
          bNodeSocket *prefilter_socket = node_find_socket(*node, SOCK_IN, "Prefilter");
          storage.prefilter = prefilter_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *quality_socket = node_find_socket(*node, SOCK_IN, "Quality");
          storage.quality = quality_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_TRANSLATE) {
          auto &storage = *static_cast<NodeTranslateData *>(node->storage);
          bNodeSocket *interpolation_socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.interpolation =
              interpolation_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_x_socket = node_find_socket(*node, SOCK_IN, "Extension X");
          storage.extension_x =
              extension_x_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_y_socket = node_find_socket(*node, SOCK_IN, "Extension Y");
          storage.extension_y =
              extension_y_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_TRANSFORM) {
          auto &storage = *static_cast<NodeTransformData *>(node->storage);
          bNodeSocket *interpolation_socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.interpolation =
              interpolation_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_x_socket = node_find_socket(*node, SOCK_IN, "Extension X");
          storage.extension_x =
              extension_x_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_y_socket = node_find_socket(*node, SOCK_IN, "Extension Y");
          storage.extension_y =
              extension_y_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_CORNERPIN) {
          auto &storage = *static_cast<NodeCornerPinData *>(node->storage);
          bNodeSocket *interpolation_socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.interpolation =
              interpolation_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_x_socket = node_find_socket(*node, SOCK_IN, "Extension X");
          storage.extension_x =
              extension_x_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_y_socket = node_find_socket(*node, SOCK_IN, "Extension Y");
          storage.extension_y =
              extension_y_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_MAP_UV) {
          auto &storage = *static_cast<NodeMapUVData *>(node->storage);
          bNodeSocket *interpolation_socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.interpolation =
              interpolation_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_x_socket = node_find_socket(*node, SOCK_IN, "Extension X");
          storage.extension_x =
              extension_x_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_y_socket = node_find_socket(*node, SOCK_IN, "Extension Y");
          storage.extension_y =
              extension_y_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_SCALE) {
          bNodeSocket *type_socket = node_find_socket(*node, SOCK_IN, "Type");
          node->custom1 = type_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *frame_type_socket = node_find_socket(*node, SOCK_IN, "Frame Type");
          node->custom2 = frame_type_socket->default_value_typed<bNodeSocketValueMenu>()->value;

          auto &storage = *static_cast<NodeScaleData *>(node->storage);
          bNodeSocket *interpolation_socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.interpolation =
              interpolation_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_x_socket = node_find_socket(*node, SOCK_IN, "Extension X");
          storage.extension_x =
              extension_x_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_y_socket = node_find_socket(*node, SOCK_IN, "Extension Y");
          storage.extension_y =
              extension_y_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_ROTATE) {
          auto &storage = *static_cast<NodeRotateData *>(node->storage);
          bNodeSocket *interpolation_socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.interpolation =
              interpolation_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_x_socket = node_find_socket(*node, SOCK_IN, "Extension X");
          storage.extension_x =
              extension_x_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_y_socket = node_find_socket(*node, SOCK_IN, "Extension Y");
          storage.extension_y =
              extension_y_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_DISPLACE) {
          auto &storage = *static_cast<NodeDisplaceData *>(node->storage);
          bNodeSocket *interpolation_socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          storage.interpolation =
              interpolation_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_x_socket = node_find_socket(*node, SOCK_IN, "Extension X");
          storage.extension_x =
              extension_x_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *extension_y_socket = node_find_socket(*node, SOCK_IN, "Extension Y");
          storage.extension_y =
              extension_y_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_STABILIZE2D) {
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Interpolation");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_MASK_BOX) {
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Operation");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_MASK_ELLIPSE) {
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Operation");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_TRACKPOS) {
          bNodeSocket *mode_socket = node_find_socket(*node, SOCK_IN, "Mode");
          node->custom1 = mode_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *frame_socket = node_find_socket(*node, SOCK_IN, "Frame");
          node->custom2 = frame_socket->default_value_typed<bNodeSocketValueInt>()->value;
        }
        else if (node->type_legacy == CMP_NODE_KEYING) {
          auto &storage = *static_cast<NodeKeyingData *>(node->storage);
          bNodeSocket *feather_falloff_socket = node_find_socket(
              *node, SOCK_IN, "Feather Falloff");
          storage.feather_falloff =
              feather_falloff_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_MASK) {
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Size Source");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_MOVIEDISTORTION) {
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_GLARE) {
          auto &storage = *static_cast<NodeGlare *>(node->storage);
          bNodeSocket *type_socket = node_find_socket(*node, SOCK_IN, "Type");
          storage.type = type_socket->default_value_typed<bNodeSocketValueMenu>()->value;
          bNodeSocket *quality_socket = node_find_socket(*node, SOCK_IN, "Quality");
          storage.quality = quality_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_SETALPHA) {
          auto &storage = *static_cast<NodeSetAlpha *>(node->storage);
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          storage.mode = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_CHANNEL_MATTE) {
          auto &storage = *static_cast<NodeChroma *>(node->storage);
          bNodeSocket *color_space_socket = node_find_socket(*node, SOCK_IN, "Color Space");
          node->custom1 = color_space_socket->default_value_typed<bNodeSocketValueMenu>()->value +
                          1;

          switch (CMPNodeChannelMatteColorSpace(node->custom1 - 1)) {
            case CMP_NODE_CHANNEL_MATTE_CS_RGB: {
              bNodeSocket *channel_socket = node_find_socket(*node, SOCK_IN, "RGB Key Channel");
              node->custom2 = channel_socket->default_value_typed<bNodeSocketValueMenu>()->value +
                              1;
              break;
            }
            case CMP_NODE_CHANNEL_MATTE_CS_HSV: {
              bNodeSocket *channel_socket = node_find_socket(*node, SOCK_IN, "HSV Key Channel");
              node->custom2 = channel_socket->default_value_typed<bNodeSocketValueMenu>()->value +
                              1;
              break;
            }
            case CMP_NODE_CHANNEL_MATTE_CS_YUV: {
              bNodeSocket *channel_socket = node_find_socket(*node, SOCK_IN, "YUV Key Channel");
              node->custom2 = channel_socket->default_value_typed<bNodeSocketValueMenu>()->value +
                              1;
              break;
            }
            case CMP_NODE_CHANNEL_MATTE_CS_YCC: {
              bNodeSocket *channel_socket = node_find_socket(*node, SOCK_IN, "YCbCr Key Channel");
              node->custom2 = channel_socket->default_value_typed<bNodeSocketValueMenu>()->value +
                              1;
              break;
            }
          }

          bNodeSocket *limit_method_socket = node_find_socket(*node, SOCK_IN, "Limit Method");
          storage.algorithm =
              limit_method_socket->default_value_typed<bNodeSocketValueMenu>()->value;

          switch (CMPNodeChannelMatteColorSpace(node->custom1 - 1)) {
            case CMP_NODE_CHANNEL_MATTE_CS_RGB: {
              bNodeSocket *channel_socket = node_find_socket(*node, SOCK_IN, "RGB Limit Channel");
              storage.channel =
                  channel_socket->default_value_typed<bNodeSocketValueMenu>()->value + 1;
              break;
            }
            case CMP_NODE_CHANNEL_MATTE_CS_HSV: {
              bNodeSocket *channel_socket = node_find_socket(*node, SOCK_IN, "HSV Limit Channel");
              storage.channel =
                  channel_socket->default_value_typed<bNodeSocketValueMenu>()->value + 1;
              break;
            }
            case CMP_NODE_CHANNEL_MATTE_CS_YUV: {
              bNodeSocket *channel_socket = node_find_socket(*node, SOCK_IN, "YUV Limit Channel");
              storage.channel =
                  channel_socket->default_value_typed<bNodeSocketValueMenu>()->value + 1;
              break;
            }
            case CMP_NODE_CHANNEL_MATTE_CS_YCC: {
              bNodeSocket *channel_socket = node_find_socket(
                  *node, SOCK_IN, "YCbCr Limit Channel");
              storage.channel =
                  channel_socket->default_value_typed<bNodeSocketValueMenu>()->value + 1;
              break;
            }
          }
        }
        else if (node->type_legacy == CMP_NODE_COLORBALANCE) {
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_PREMULKEY) {
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Type");
          node->custom1 = socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_DIST_MATTE) {
          auto &storage = *static_cast<NodeChroma *>(node->storage);
          bNodeSocket *socket = node_find_socket(*node, SOCK_IN, "Color Space");
          storage.channel = socket->default_value_typed<bNodeSocketValueMenu>()->value + 1;
        }
        else if (node->type_legacy == CMP_NODE_COLOR_SPILL) {
          bNodeSocket *spill_channel_socket = node_find_socket(*node, SOCK_IN, "Spill Channel");
          node->custom1 =
              spill_channel_socket->default_value_typed<bNodeSocketValueMenu>()->value + 1;

          bNodeSocket *limit_method_socket = node_find_socket(*node, SOCK_IN, "Limit Method");
          node->custom2 = limit_method_socket->default_value_typed<bNodeSocketValueMenu>()->value;

          auto &storage = *static_cast<NodeColorspill *>(node->storage);
          bNodeSocket *limit_channel_socket = node_find_socket(*node, SOCK_IN, "Limit Channel");
          storage.limchan =
              limit_channel_socket->default_value_typed<bNodeSocketValueMenu>()->value;
        }
        else if (node->type_legacy == CMP_NODE_DOUBLEEDGEMASK) {
          bNodeSocket *image_edges_socket = node_find_socket(*node, SOCK_IN, "Image Edges");
          node->custom2 = bool(
              image_edges_socket->default_value_typed<bNodeSocketValueBoolean>()->value);

          bNodeSocket *only_inside_outer_socket = node_find_socket(
              *node, SOCK_IN, "Only Inside Outer");
          node->custom1 = bool(
              only_inside_outer_socket->default_value_typed<bNodeSocketValueBoolean>()->value);
        }
      }
    }
    default:
      break;
  }
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
      /* Custom node sockets where default_value is defined use custom properties for storage. */
      break;
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
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

static void node_blend_write_storage(BlendWriter *writer, bNodeTree *ntree, bNode *node)
{
  if (!node->storage) {
    return;
  }

  if (node->type_legacy == CMP_NODE_GLARE) {
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
  }
  else if (node->type_legacy == GEO_NODE_CAPTURE_ATTRIBUTE) {
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
  }
  else if (node->type_legacy == GEO_NODE_VIEWER) {
    /* Forward compatibility for older Blender versionins where the viewer node only had a geometry
     * and field input. */
    auto &storage = *static_cast<NodeGeometryViewer *>(node->storage);
    for (const NodeGeometryViewerItem &item : Span{storage.items, storage.items_num}) {
      if (ELEM(item.socket_type,
               SOCK_FLOAT,
               SOCK_INT,
               SOCK_VECTOR,
               SOCK_RGBA,
               SOCK_BOOLEAN,
               SOCK_ROTATION,
               SOCK_MATRIX))
      {
        storage.data_type_legacy = *socket_type_to_custom_data_type(
            eNodeSocketDatatype(item.socket_type));
        break;
      }
    }
  }

  const bNodeType *ntype = node->typeinfo;
  if (!ntype->storagename.empty()) {
    BLO_write_struct_by_name(writer, ntype->storagename.c_str(), node->storage);
  }
  if (ntype->blend_write_storage_content) {
    ntype->blend_write_storage_content(*ntree, *node, *writer);
    return;
  }

  /* These nodes don't use #blend_write_storage_content because their corresponding blend-read
   * can't use it since they were introduced before there were node idnames. */
  if (ELEM(node->type_legacy,
           SH_NODE_CURVE_VEC,
           SH_NODE_CURVE_RGB,
           SH_NODE_CURVE_FLOAT,
           CMP_NODE_TIME,
           CMP_NODE_CURVE_VEC_DEPRECATED,
           CMP_NODE_CURVE_RGB,
           CMP_NODE_HUECORRECT,
           TEX_NODE_CURVE_RGB,
           TEX_NODE_CURVE_TIME))
  {
    BKE_curvemapping_curves_blend_write(writer, static_cast<const CurveMapping *>(node->storage));
  }
  else if (node->type_legacy == SH_NODE_SCRIPT) {
    NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);
    if (nss->bytecode) {
      BLO_write_string(writer, nss->bytecode);
    }
  }
  else if (node->type_legacy == CMP_NODE_MOVIEDISTORTION) {
    /* pass */
  }
  else if (ELEM(node->type_legacy, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY)) {
    NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
    BLO_write_string(writer, nc->matte_id);
    LISTBASE_FOREACH (CryptomatteEntry *, entry, &nc->entries) {
      BLO_write_struct(writer, CryptomatteEntry, entry);
    }
  }
}

void node_tree_blend_write(BlendWriter *writer, bNodeTree *ntree)
{
  BKE_id_blend_write(writer, &ntree->id);
  BLO_write_string(writer, ntree->description);

  if (!BLO_write_is_undo(writer)) {
    forward_compat::update_node_location_legacy(*ntree);
    forward_compat::write_legacy_properties(*ntree);
  }

  for (bNode *node : ntree->all_nodes()) {
    if (ntree->type == NTREE_SHADER && node->type_legacy == SH_NODE_BSDF_HAIR_PRINCIPLED) {
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
    if (node->system_properties) {
      IDP_BlendWrite(writer, node->system_properties);
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
      node_blend_write_storage(writer, ntree, node);
    }

    if (ELEM(node->type_legacy, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS)) {
      /* Write extra socket info. */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        BLO_write_struct(writer, NodeImageLayer, sock->storage);
      }
    }
  }

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    BLO_write_struct(writer, bNodeLink, link);
  }

  ntree->tree_interface.write(writer);

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

  BLO_write_id_struct(writer, bNodeTree, id_address, &ntree->id);

  node_tree_blend_write(writer, ntree);
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
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
      return true;
  }
  return false;
}

namespace versioning_internal {

/* Specific code required to properly handle older blend-files (pre-2.83), where some node data
 * (like the sockets default values) were written as raw bytes buffer, without any DNA type
 * information. */

/* Node socket default values were historically written and read as raw bytes buffers, without any
 * DNA typing information.
 *
 * The writing code was fixed in commit `50d5050e9c`, which is included in the 2.83 release.
 * However the matching reading code was only fixed in the 4.5 release.
 *
 * So currently, reading code assumes that any blend-file >= 3.0 has correct DNA info for these
 * default values, and it keeps previous 'raw buffer' reading code for older ones.
 *
 * This means that special care must be taken when the various DNA types used for these default
 * values are modified, as a 'manual' version of DNA internal versioning must be performed on data
 * from older blend-files (see also #direct_link_node_socket_default_value).
 */
constexpr int MIN_BLENDFILE_VERSION_FOR_MODERN_NODE_SOCKET_DEFAULT_VALUE_READING = 300;

/* The `_404` structs below are copies of DNA structs as they were in Blender 4.4 and before. Their
 * data layout should never have to be modified in any way, as it matches the expected data layout
 * in the raw bytes buffers read from older blend-files.
 *
 * NOTE: There is _no_ need to protect DNA structs definition in any way to ensure forward
 * compatibility, for the following reasons:
 *   - The DNA struct info _is_ properly written in blend-files since 2.83.
 *   - When there is DNA info for a #BHead in the blend-file, even if that #BHead is ultimately
 *     'read'/used as raw bytes buffer through a call to `BLO_read_data_address`, the actual
 *     reading of that #BHead from the blend-file will have already gone through the lower-level
 *     'DNA versioning' process, which means that DNA struct changes (like adding new properties,
 *     increasing an array size, etc.) will be handled properly.
 *   - Blender versions prior to 3.6 will not be able to load any 4.0+ blend-files without
 *     immediate crash, so trying to preserve forward compatibility for versions older than
 *     2.83 would be totally pointless.
 */

typedef struct bNodeSocketValueInt_404 {
  /** RNA subtype. */
  int subtype;
  int value;
  int min, max;
} bNodeSocketValueInt_404;

typedef struct bNodeSocketValueFloat_404 {
  /** RNA subtype. */
  int subtype;
  float value;
  float min, max;
} bNodeSocketValueFloat_404;

typedef struct bNodeSocketValueBoolean_404 {
  char value;
} bNodeSocketValueBoolean_404;

typedef struct bNodeSocketValueVector_404 {
  /** RNA subtype. */
  int subtype;
  float value[3];
  float min, max;
} bNodeSocketValueVector_404;

typedef struct bNodeSocketValueRotation_404 {
  float value_euler[3];
} bNodeSocketValueRotation_404;

typedef struct bNodeSocketValueRGBA_404 {
  float value[4];
} bNodeSocketValueRGBA_404;

typedef struct bNodeSocketValueString_404 {
  int subtype;
  char _pad[4];
  char value[/*FILE_MAX*/ 1024];
} bNodeSocketValueString_404;

typedef struct bNodeSocketValueObject_404 {
  Object *value;
} bNodeSocketValueObject_404;

typedef struct bNodeSocketValueImage_404 {
  Image *value;
} bNodeSocketValueImage_404;

typedef struct bNodeSocketValueCollection_404 {
  Collection *value;
} bNodeSocketValueCollection_404;

typedef struct bNodeSocketValueTexture_404 {
  Tex *value;
} bNodeSocketValueTexture_404;

typedef struct bNodeSocketValueMaterial_404 {
  Material *value;
} bNodeSocketValueMaterial_404;

typedef struct bNodeSocketValueMenu_404 {
  /* Default input enum identifier. */
  int value;
  /* #NodeSocketValueMenuRuntimeFlag */
  int runtime_flag;
  /* Immutable runtime enum definition. */
  const RuntimeNodeEnumItemsHandle *enum_items;
} bNodeSocketValueMenu_404;

/* Generic code handling the conversion between a legacy (pre-2.83) socket data, and its current
 * data. Currently used for `bNodeSocket.default_value`. */
template<typename T, typename T_404>
static void direct_link_node_socket_legacy_data_version_do(
    void **dest_data, void **raw_data, blender::FunctionRef<void(T &dest, T_404 &source)> copy_fn)
{
  /* Cannot check for equality because of potential alignment offset. */
  BLI_assert(MEM_allocN_len(*raw_data) >= sizeof(T_404));
  T_404 *orig_data = static_cast<T_404 *>(*raw_data);
  *raw_data = nullptr;
  T *final_data = MEM_callocN<T>(__func__);
  /* Could use `memcpy` here, since we also require historic members of these DNA structs to
   * never be moved or re-ordered. But better be verbose and explicit here. */
  copy_fn(*final_data, *orig_data);
  *dest_data = final_data;
  MEM_freeN(orig_data);
}

}  // namespace versioning_internal

static void direct_link_node_socket_default_value(BlendDataReader *reader, bNodeSocket *sock)
{
  if (sock->default_value == nullptr) {
    return;
  }

  if (sock->type == SOCK_CUSTOM) {
    /* There are some files around that have non-null default value for custom sockets. See e.g.
     * #140083.
     *
     * It is unclear how this could happen, but for now simply systematically set this pointer to
     * null. */
    sock->default_value = nullptr;
    return;
  }

  if (BLO_read_fileversion_get(reader) >=
      versioning_internal::MIN_BLENDFILE_VERSION_FOR_MODERN_NODE_SOCKET_DEFAULT_VALUE_READING)
  {
    /* Modern, standard DNA-typed reading of sockets default values. */
    switch (eNodeSocketDatatype(sock->type)) {
      case SOCK_FLOAT:
        BLO_read_struct(reader, bNodeSocketValueFloat, &sock->default_value);
        break;
      case SOCK_VECTOR:
        BLO_read_struct(reader, bNodeSocketValueVector, &sock->default_value);
        break;
      case SOCK_RGBA:
        BLO_read_struct(reader, bNodeSocketValueRGBA, &sock->default_value);
        break;
      case SOCK_BOOLEAN:
        BLO_read_struct(reader, bNodeSocketValueBoolean, &sock->default_value);
        break;
      case SOCK_INT:
        BLO_read_struct(reader, bNodeSocketValueInt, &sock->default_value);
        break;
      case SOCK_STRING:
        BLO_read_struct(reader, bNodeSocketValueString, &sock->default_value);
        break;
      case SOCK_OBJECT:
        BLO_read_struct(reader, bNodeSocketValueObject, &sock->default_value);
        break;
      case SOCK_IMAGE:
        BLO_read_struct(reader, bNodeSocketValueImage, &sock->default_value);
        break;
      case SOCK_COLLECTION:
        BLO_read_struct(reader, bNodeSocketValueCollection, &sock->default_value);
        break;
      case SOCK_TEXTURE:
        BLO_read_struct(reader, bNodeSocketValueTexture, &sock->default_value);
        break;
      case SOCK_MATERIAL:
        BLO_read_struct(reader, bNodeSocketValueMaterial, &sock->default_value);
        break;
      case SOCK_ROTATION:
        BLO_read_struct(reader, bNodeSocketValueRotation, &sock->default_value);
        break;
      case SOCK_MENU:
        BLO_read_struct(reader, bNodeSocketValueMenu, &sock->default_value);
        break;
      case SOCK_MATRIX:
        /* Matrix sockets currently have no default value. */
      case SOCK_CUSTOM:
        /* Custom node sockets where default_value is defined use custom properties for storage. */
      case SOCK_SHADER:
      case SOCK_GEOMETRY:
      case SOCK_BUNDLE:
      case SOCK_CLOSURE:
        BLI_assert_unreachable();
        break;
    }
  }
  else {
    /* Legacy-compatible, raw-buffer-based reading of sockets default values. */
    void *temp_data = sock->default_value;
    BLO_read_data_address(reader, &temp_data);
    if (!temp_data) {
      sock->default_value = nullptr;
      return;
    }

    switch (eNodeSocketDatatype(sock->type)) {
      case SOCK_FLOAT:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueFloat,
            versioning_internal::bNodeSocketValueFloat_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueFloat &dest, versioning_internal::bNodeSocketValueFloat_404 &src) {
              dest.subtype = src.subtype;
              dest.value = src.value;
              dest.min = src.min;
              dest.max = src.max;
            });
        break;
      case SOCK_VECTOR:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueVector,
            versioning_internal::bNodeSocketValueVector_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueVector &dest,
               versioning_internal::bNodeSocketValueVector_404 &src) {
              dest.subtype = src.subtype;
              copy_v3_v3(dest.value, src.value);
              dest.min = src.min;
              dest.max = src.max;
            });
        break;
      case SOCK_RGBA:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueRGBA,
            versioning_internal::bNodeSocketValueRGBA_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueRGBA &dest, versioning_internal::bNodeSocketValueRGBA_404 &src) {
              copy_v4_v4(dest.value, src.value);
            });
        break;
      case SOCK_BOOLEAN:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueBoolean,
            versioning_internal::bNodeSocketValueBoolean_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueBoolean &dest,
               versioning_internal::bNodeSocketValueBoolean_404 &src) { dest.value = src.value; });
        break;
      case SOCK_INT:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueInt,
            versioning_internal::bNodeSocketValueInt_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueInt &dest, versioning_internal::bNodeSocketValueInt_404 &src) {
              dest.subtype = src.subtype;
              dest.value = src.value;
              dest.min = src.min;
              dest.max = src.max;
            });
        break;
      case SOCK_STRING:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueString,
            versioning_internal::bNodeSocketValueString_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueString &dest,
               versioning_internal::bNodeSocketValueString_404 &src) {
              dest.subtype = src.subtype;
              STRNCPY(dest.value, src.value);
            });
        break;
      case SOCK_OBJECT:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueObject,
            versioning_internal::bNodeSocketValueObject_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueObject &dest,
               versioning_internal::bNodeSocketValueObject_404 &src) { dest.value = src.value; });
        break;
      case SOCK_IMAGE:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueImage,
            versioning_internal::bNodeSocketValueImage_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueImage &dest, versioning_internal::bNodeSocketValueImage_404 &src) {
              dest.value = src.value;
            });
        break;
      case SOCK_COLLECTION:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueCollection,
            versioning_internal::bNodeSocketValueCollection_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueCollection &dest,
               versioning_internal::bNodeSocketValueCollection_404 &src) {
              dest.value = src.value;
            });
        break;
      case SOCK_TEXTURE:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueTexture,
            versioning_internal::bNodeSocketValueTexture_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueTexture &dest,
               versioning_internal::bNodeSocketValueTexture_404 &src) { dest.value = src.value; });
        break;
      case SOCK_MATERIAL:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueMaterial,
            versioning_internal::bNodeSocketValueMaterial_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueMaterial &dest,
               versioning_internal::bNodeSocketValueMaterial_404 &src) {
              dest.value = src.value;
            });
        break;
      case SOCK_ROTATION:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueRotation,
            versioning_internal::bNodeSocketValueRotation_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueRotation &dest,
               versioning_internal::bNodeSocketValueRotation_404 &src) {
              copy_v3_v3(dest.value_euler, src.value_euler);
            });
        break;
      case SOCK_MENU:
        versioning_internal::direct_link_node_socket_legacy_data_version_do<
            bNodeSocketValueMenu,
            versioning_internal::bNodeSocketValueMenu_404>(
            &sock->default_value,
            &temp_data,
            [](bNodeSocketValueMenu &dest, versioning_internal::bNodeSocketValueMenu_404 &src) {
              dest.value = src.value;
              /* Other members are runtime-only. */
            });
        break;
      case SOCK_MATRIX:
        /* Matrix sockets had no default value. */
      case SOCK_CUSTOM:
        /* Custom node sockets where default_value is defined were using custom properties for
         * storage. */
      case SOCK_SHADER:
      case SOCK_GEOMETRY:
      case SOCK_BUNDLE:
      case SOCK_CLOSURE:
        BLI_assert_unreachable();
        break;
    }
  }

  /* Post-reading processing. */
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

static void direct_link_node_socket_storage(BlendDataReader *reader,
                                            const bNode *node,
                                            bNodeSocket *sock)
{
  if (!sock->storage) {
    return;
  }
  if (!node) {
    /* Sockets not owned by a node should never have storage data. */
    BLI_assert_unreachable();
    sock->storage = nullptr;
    return;
  }

  /* Sockets storage data seem to have always been written with correct DNA type info (see
   * 3bae60d0c9 and 9d91bc38d3). So no need to use the same versioning work-around for old files as
   * done for default values. */
  switch (node->type_legacy) {
    case CMP_NODE_OUTPUT_FILE:
      BLO_read_struct(reader, NodeImageMultiFileSocket, &sock->storage);
      if (sock->storage) {
        NodeImageMultiFileSocket *sockdata = static_cast<NodeImageMultiFileSocket *>(
            sock->storage);
        BKE_image_format_blend_read_data(reader, &sockdata->format);
      }
      break;
    case CMP_NODE_IMAGE:
    case CMP_NODE_R_LAYERS:
      BLO_read_struct(reader, NodeImageLayer, &sock->storage);
      break;
    default:
      BLI_assert_unreachable();
      sock->storage = nullptr;
      break;
  }
}

static void direct_link_node_socket(BlendDataReader *reader, const bNode *node, bNodeSocket *sock)
{
  BLO_read_struct(reader, IDProperty, &sock->prop);
  IDP_BlendDataRead(reader, &sock->prop);

  BLO_read_struct(reader, bNodeLink, &sock->link);
  sock->typeinfo = nullptr;

  direct_link_node_socket_storage(reader, node, sock);

  direct_link_node_socket_default_value(reader, sock);

  BLO_read_string(reader, &sock->default_attribute_name);
  sock->runtime = MEM_new<bNodeSocketRuntime>(__func__);
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

static void node_blend_read_data_storage(BlendDataReader *reader, bNodeTree *ntree, bNode *node)
{
  if (!node->storage) {
    return;
  }
  if (node->type_legacy == CMP_NODE_MOVIEDISTORTION) {
    /* Do nothing, this is a runtime cache and hence handled by generic code using
     * `IDTypeInfo.foreach_cache` callback. */
    return;
  }

  /* This may not always find the type for legacy nodes when the idname did not exist yet or it was
   * changed. Versioning code will update the nodes with unknown types. */
  const bNodeType *ntype = node_type_find(node->idname);

  if (ntype && !ntype->storagename.empty()) {
    node->storage = BLO_read_struct_by_name_array(
        reader, ntype->storagename.c_str(), 1, node->storage);
  }
  else {
    /* Untyped read because we don't know the type yet. */
    BLO_read_data_address(reader, &node->storage);
  }

  if (ntype && ntype->blend_data_read_storage_content) {
    ntype->blend_data_read_storage_content(*ntree, *node, *reader);
    return;
  }

  /* Some nodes don't use the callback above, because they were introduced before there were node
   * idnames. Therefore, we can't rely on the idname to lookup the node type. */
  switch (node->type_legacy) {
    case SH_NODE_CURVE_VEC:
    case SH_NODE_CURVE_RGB:
    case SH_NODE_CURVE_FLOAT:
    case CMP_NODE_TIME:
    case CMP_NODE_CURVE_VEC_DEPRECATED:
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
    default:
      break;
  }
}

/**
 * Update idnames of nodes. Note that this is *not* forward-compatible and thus should only be done
 * if the node was not officially released yet. It's ok to add it here while it's still an
 * experimental feature.
 */
static void node_update_idname_from_experimental(bNode &node)
{
  static Map<std::string, std::string> idname_map = []() {
    Map<std::string, std::string> map;
    map.add("GeometryNodeEvaluateClosure", "NodeEvaluateClosure");
    map.add("GeometryNodeClosureInput", "NodeClosureInput");
    map.add("GeometryNodeClosureOutput", "NodeClosureOutput");
    map.add("GeometryNodeCombineBundle", "NodeCombineBundle");
    map.add("GeometryNodeSeparateBundle", "NodeSeparateBundle");
    return map;
  }();
  if (const std::string *new_idname = idname_map.lookup_ptr_as(node.idname)) {
    STRNCPY_UTF8(node.idname, new_idname->c_str());
  }
}

void node_tree_blend_read_data(BlendDataReader *reader, ID *owner_id, bNodeTree *ntree)
{
  /* Special case for this pointer, do not rely on regular `lib_link` process here. Avoids needs
   * for do_versioning, and ensures coherence of data in any case.
   *
   * NOTE: Old versions are very often 'broken' here, just fix it silently in these cases.
   */
  if (BLO_read_fileversion_get(reader) > 300) {
    BLI_assert((ntree->id.flag & ID_FLAG_EMBEDDED_DATA) != 0 || owner_id == nullptr);
  }
  BLI_assert(owner_id == nullptr || owner_id->lib == ntree->id.lib);
  if (owner_id != nullptr && (ntree->id.flag & ID_FLAG_EMBEDDED_DATA) == 0) {
    /* This is unfortunate, but currently a lot of existing files (including startup ones) have
     * missing `ID_FLAG_EMBEDDED_DATA` flag.
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
    ntree->id.flag |= ID_FLAG_EMBEDDED_DATA;
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
    node_update_idname_from_experimental(*node);
    node->runtime = MEM_new<bNodeRuntime>(__func__);
    node->typeinfo = nullptr;
    node->runtime->index_in_tree = i;

    /* Create the `nodes_by_id` cache eagerly so it can be expected to be valid. Because
     * we create it here we also have to check for zero identifiers from previous versions. */
    if (node->identifier == 0 || ntree->runtime->nodes_by_id.contains_as(node->identifier)) {
      node_unique_id(*ntree, *node);
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
    BLO_read_struct(reader, IDProperty, &node->system_properties);
    IDP_BlendDataRead(reader, &node->system_properties);

    node_blend_read_data_storage(reader, ntree, node);
  }
  BLO_read_struct_list(reader, bNodeLink, &ntree->links);
  BLI_assert(ntree->all_nodes().size() == BLI_listbase_count(&ntree->nodes));

  /* and we connect the rest */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    BLO_read_struct(reader, bNode, &node->parent);

    LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->inputs) {
      direct_link_node_socket(reader, node, sock);
    }
    LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->outputs) {
      direct_link_node_socket(reader, node, sock);
    }
  }

  /* Read legacy interface socket lists for versioning. */
  BLO_read_struct_list(reader, bNodeSocket, &ntree->inputs_legacy);
  BLO_read_struct_list(reader, bNodeSocket, &ntree->outputs_legacy);
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &ntree->inputs_legacy) {
    direct_link_node_socket(reader, nullptr, sock);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &ntree->outputs_legacy) {
    direct_link_node_socket(reader, nullptr, sock);
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

  BLO_read_struct(reader, PreviewImage, &ntree->preview);
  BKE_previewimg_blend_read(reader, ntree->preview);

  /* type verification is in lib-link */
}

static void ntree_blend_read_data(BlendDataReader *reader, ID *id)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  node_tree_blend_read_data(reader, nullptr, ntree);
}

static void ntree_blend_read_after_liblink(BlendLibReader *reader, ID *id)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  /* Set `node->typeinfo` pointers. This is done in lib linking, after the
   * first versioning that can change types still without functions that
   * update the `typeinfo` pointers. Versioning after lib linking needs
   * these top be valid. */
  node_tree_set_type(*ntree);

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
    auto *prop = idprop::create(socket->name ? socket->name : "", socket->socket_type).release();
    if (!IDP_AddToGroup(inputs.get(), prop)) {
      IDP_FreeProperty(prop);
    }
  }
  for (const bNodeTreeInterfaceSocket *socket : node_tree.interface_outputs()) {
    auto *prop = idprop::create(socket->name ? socket->name : "", socket->socket_type).release();
    if (!IDP_AddToGroup(outputs.get(), prop)) {
      IDP_FreeProperty(prop);
    }
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
    /*id_code*/ bNodeTree::id_type,
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
    /*foreach_working_space_color*/ blender::bke::node_foreach_working_space_color,
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
  DATA_(ntype->ui_name).copy_utf8_truncated(node->name);
  node_unique_name(*ntree, *node);

  /* Generally sockets should be added after the initialization, because the set of sockets might
   * depend on node properties. */
  const bool add_sockets_before_init = node->type_legacy == CMP_NODE_R_LAYERS;
  if (add_sockets_before_init) {
    node_add_sockets_from_type(ntree, node, ntype);
  }

  if (ntype->initfunc != nullptr) {
    ntype->initfunc(ntree, node);
  }

  if (ntype->initfunc_api) {
    PointerRNA ptr = RNA_pointer_create_discrete(&ntree->id, &RNA_Node, node);

    /* XXX WARNING: context can be nullptr in case nodes are added in do_versions.
     * Delayed init is not supported for nodes with context-based `initfunc_api` at the moment. */
    BLI_assert(C != nullptr);
    ntype->initfunc_api(C, &ptr);
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
    node->type_legacy = typeinfo->type_legacy;

    /* initialize the node if necessary */
    node_init(C, ntree, node);
  }
  else {
    node->typeinfo = &NodeTypeUndefined;
  }
  BKE_ntree_update_tag_node_type(ntree, node);
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
                            bNodeTreeType *treetype,
                            bNodeType *nodetype,
                            bNodeSocketType *socktype,
                            const bool unregister)
{
  if (!bmain) {
    return;
  }

  FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
    if (treetype && ntree->idname == treetype->idname) {
      ntree_set_typeinfo(ntree, unregister ? nullptr : treetype);
    }

    /* initialize nodes */
    for (bNode *node : ntree->all_nodes()) {
      if (nodetype && node->idname == nodetype->idname) {
        node_set_typeinfo(nullptr, ntree, node, unregister ? nullptr : nodetype);
      }

      /* initialize node sockets */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        if (socktype && sock->idname == socktype->idname) {
          node_socket_set_typeinfo(ntree, sock, unregister ? nullptr : socktype);
        }
      }
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        if (socktype && sock->idname == socktype->idname) {
          node_socket_set_typeinfo(ntree, sock, unregister ? nullptr : socktype);
        }
      }
    }
  }
  FOREACH_NODETREE_END;
}

void node_tree_set_type(bNodeTree &ntree)
{
  ntree_set_typeinfo(&ntree, node_tree_type_find(ntree.idname));

  for (bNode *node : ntree.all_nodes()) {
    /* Set socket typeinfo first because node initialization may rely on socket typeinfo for
     * generating declarations. */
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      node_socket_set_typeinfo(&ntree, sock, node_socket_type_find(sock->idname));
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      node_socket_set_typeinfo(&ntree, sock, node_socket_type_find(sock->idname));
    }

    node_set_typeinfo(nullptr, &ntree, node, node_type_find(node->idname));
  }
}

template<typename T> struct NodeStructIDNameGetter {
  StringRef operator()(const T *value) const
  {
    return StringRef(value->idname);
  }
};

static auto &get_node_tree_type_map()
{
  static CustomIDVectorSet<bNodeTreeType *, NodeStructIDNameGetter<bNodeTreeType>> map;
  return map;
}

static auto &get_node_type_map()
{
  static CustomIDVectorSet<bNodeType *, NodeStructIDNameGetter<bNodeType>> map;
  return map;
}

static auto &get_node_type_alias_map()
{
  static Map<std::string, std::string> map;
  return map;
}

static auto &get_socket_type_map()
{
  static CustomIDVectorSet<bNodeSocketType *, NodeStructIDNameGetter<bNodeSocketType>> map;
  return map;
}

bNodeTreeType *node_tree_type_find(const StringRef idname)
{
  bNodeTreeType *const *value = get_node_tree_type_map().lookup_key_ptr_as(idname);
  if (!value) {
    return nullptr;
  }
  return *value;
}

static void defer_free_tree_type(bNodeTreeType *tree_type)
{
  static ResourceScope scope;
  scope.add_destruct_call([tree_type]() { MEM_delete(tree_type); });
}

static void defer_free_node_type(bNodeType *ntype)
{
  static ResourceScope scope;
  scope.add_destruct_call([ntype]() {
    /* May be null if the type is statically allocated. */
    if (ntype->free_self) {
      ntype->free_self(ntype);
    }
  });
}

static void defer_free_socket_type(bNodeSocketType *stype)
{
  static ResourceScope scope;
  scope.add_destruct_call([stype]() {
    /* May be null if the type is statically allocated. */
    if (stype->free_self) {
      stype->free_self(stype);
    }
  });
}

void node_tree_type_add(bNodeTreeType &nt)
{
  get_node_tree_type_map().add(&nt);
  /* XXX pass Main to register function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, &nt, nullptr, nullptr, false);
}

static void ntree_free_type(void *treetype_v)
{
  bNodeTreeType *treetype = static_cast<bNodeTreeType *>(treetype_v);
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, treetype, nullptr, nullptr, true);

  /* Defer freeing the tree type, because it may still be referenced by trees in depsgraph
   * copies. We can't just remove these tree types, because the depsgraph may exist completely
   * separately from original data. */
  defer_free_tree_type(treetype);
}

void node_tree_type_free_link(const bNodeTreeType &nt)
{
  get_node_tree_type_map().remove(const_cast<bNodeTreeType *>(&nt));
  ntree_free_type(const_cast<bNodeTreeType *>(&nt));
}

bool node_tree_is_registered(const bNodeTree &ntree)
{
  return (ntree.typeinfo != &NodeTreeTypeUndefined);
}

Span<bNodeTreeType *> node_tree_types_get()
{
  return get_node_tree_type_map().as_span();
}

bNodeType *node_type_find(const StringRef idname)
{
  bNodeType *const *value = get_node_type_map().lookup_key_ptr_as(idname);
  if (!value) {
    return nullptr;
  }
  return *value;
}

StringRefNull node_type_find_alias(const StringRefNull alias)
{
  const std::string *idname = get_node_type_alias_map().lookup_ptr_as(alias);
  if (!idname) {
    return alias;
  }
  return *idname;
}

static void node_free_type(void *nodetype_v)
{
  bNodeType *nodetype = static_cast<bNodeType *>(nodetype_v);
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nodetype, nullptr, true);

  /* Setting this to null is necessary for the case of static node types. When running tests,
   * they may be registered and unregistered multiple times. */
  delete nodetype->static_declaration;
  nodetype->static_declaration = nullptr;

  /* Defer freeing the node type, because it may still be referenced by nodes in depsgraph
   * copies. We can't just remove these node types, because the depsgraph may exist completely
   * separate from original data. */
  defer_free_node_type(nodetype);
}

void node_register_type(bNodeType &nt)
{
  /* debug only: basic verification of registered types */
  BLI_assert(!nt.idname.empty());
  BLI_assert(nt.poll != nullptr);

  RNA_def_struct_ui_text(nt.rna_ext.srna, nt.ui_name.c_str(), nt.ui_description.c_str());

  if (!nt.enum_name_legacy) {
    /* For new nodes, use the idname as a unique identifier. */
    nt.enum_name_legacy = nt.idname.c_str();
  }

  if (nt.declare) {
    nt.static_declaration = new nodes::NodeDeclaration();
    nodes::build_node_declaration(nt, *nt.static_declaration, nullptr, nullptr);
  }

  get_node_type_map().add_new(&nt);
  /* XXX pass Main to register function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, &nt, nullptr, false);
}

void node_unregister_type(bNodeType &nt)
{
  get_node_type_map().remove(&nt);
  node_free_type(&nt);
}

Span<bNodeType *> node_types_get()
{
  return get_node_type_map().as_span();
}

void node_register_alias(bNodeType &nt, const StringRef alias)
{
  get_node_type_alias_map().add_new(alias, nt.idname);
}

Span<bNodeSocketType *> node_socket_types_get()
{
  return get_socket_type_map().as_span();
}

bNodeSocketType *node_socket_type_find(const StringRef idname)
{
  bNodeSocketType *const *value = get_socket_type_map().lookup_key_ptr_as(idname);
  if (!value) {
    return nullptr;
  }
  return *value;
}

bNodeSocketType *node_socket_type_find_static(const int type, const int subtype)
{
  const std::optional<StringRefNull> idname = node_static_socket_type(type, subtype);
  if (!idname) {
    return nullptr;
  }
  return node_socket_type_find(*idname);
}

static void node_free_socket_type(void *socktype_v)
{
  bNodeSocketType *socktype = static_cast<bNodeSocketType *>(socktype_v);
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, socktype, true);

  /* Defer freeing the socket type, because it may still be referenced by nodes in depsgraph
   * copies. We can't just remove these socket types, because the depsgraph may exist completely
   * separate from original data. */
  defer_free_socket_type(socktype);
}

void node_register_socket_type(bNodeSocketType &st)
{
  get_socket_type_map().add(&st);
  /* XXX pass Main to register function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, &st, false);
}

void node_unregister_socket_type(bNodeSocketType &st)
{
  get_socket_type_map().remove(&st);
  node_free_socket_type(&st);
}

bool node_socket_is_registered(const bNodeSocket &sock)
{
  return (sock.typeinfo != &NodeSocketTypeUndefined);
}

StringRefNull node_socket_type_label(const bNodeSocketType &stype)
{
  /* Use socket type name as a fallback if label is undefined. */
  if (stype.label[0] == '\0') {
    return RNA_struct_ui_name(stype.ext_socket.srna);
  }
  return stype.label;
}

StringRefNull node_socket_sub_type_label(int subtype)
{
  const char *name;
  if (RNA_enum_name(rna_enum_property_subtype_items, subtype, &name)) {
    return name;
  }
  return "";
}

bNodeSocket *node_find_socket(bNode &node,
                              const eNodeSocketInOut in_out,
                              const StringRef identifier)
{
  const ListBase *sockets = (in_out == SOCK_IN) ? &node.inputs : &node.outputs;
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    if (sock->identifier == identifier) {
      return sock;
    }
  }
  return nullptr;
}

const bNodeSocket *node_find_socket(const bNode &node,
                                    const eNodeSocketInOut in_out,
                                    const StringRef identifier)
{
  /* Reuse the implementation of the mutable accessor. */
  return node_find_socket(*const_cast<bNode *>(&node), in_out, identifier);
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

static bNodeSocket *make_socket(bNodeTree *ntree,
                                bNode * /*node*/,
                                const int in_out,
                                ListBase *lb,
                                const StringRefNull idname,
                                const StringRefNull identifier,
                                const StringRefNull name)
{
  char auto_identifier[MAX_NAME];

  if (identifier[0] != '\0') {
    /* use explicit identifier */
    identifier.copy_utf8_truncated(auto_identifier);
  }
  else {
    /* if no explicit identifier is given, assign a unique identifier based on the name */
    name.copy_utf8_truncated(auto_identifier);
  }
  /* Make the identifier unique. */
  BLI_uniquename_cb(
      [&](const StringRef check_name) {
        LISTBASE_FOREACH (bNodeSocket *, sock, lb) {
          if (sock->identifier == check_name) {
            return true;
          }
        }
        return false;
      },
      "socket",
      '_',
      auto_identifier,
      sizeof(auto_identifier));

  bNodeSocket *sock = MEM_callocN<bNodeSocket>(__func__);
  sock->runtime = MEM_new<bNodeSocketRuntime>(__func__);
  sock->in_out = in_out;

  STRNCPY_UTF8(sock->identifier, auto_identifier);
  sock->limit = (in_out == SOCK_IN ? 1 : 0xFFF);

  name.copy_utf8_truncated(sock->name);
  sock->storage = nullptr;
  sock->flag |= SOCK_COLLAPSED;
  sock->type = SOCK_CUSTOM; /* int type undefined by default */

  idname.copy_utf8_truncated(sock->idname);
  node_socket_set_typeinfo(ntree, sock, node_socket_type_find(idname));

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
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
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
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
      break;
  }
  return false;
}

void node_modify_socket_type(bNodeTree &ntree,
                             bNode & /*node*/,
                             bNodeSocket &sock,
                             const StringRefNull idname)
{
  bNodeSocketType *socktype = node_socket_type_find(idname);

  if (!socktype) {
    CLOG_ERROR(&LOG, "node socket type %s undefined", idname.c_str());
    return;
  }

  if (sock.default_value) {
    if (sock.type != socktype->type) {
      /* Only reallocate the default value if the type changed so that UI data like min and max
       * isn't removed. This assumes that the default value is stored in the same format for all
       * socket types with the same #eNodeSocketDatatype. */
      socket_id_user_decrement(&sock);
      MEM_freeN(sock.default_value);
      sock.default_value = nullptr;
    }
    else {
      /* Update the socket subtype when the storage isn't freed and recreated. */
      switch (eNodeSocketDatatype(sock.type)) {
        case SOCK_FLOAT: {
          sock.default_value_typed<bNodeSocketValueFloat>()->subtype = socktype->subtype;
          break;
        }
        case SOCK_VECTOR: {
          sock.default_value_typed<bNodeSocketValueVector>()->subtype = socktype->subtype;
          break;
        }
        case SOCK_INT: {
          sock.default_value_typed<bNodeSocketValueInt>()->subtype = socktype->subtype;
          break;
        }
        case SOCK_STRING: {
          sock.default_value_typed<bNodeSocketValueString>()->subtype = socktype->subtype;
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
        case SOCK_BUNDLE:
        case SOCK_CLOSURE:
          break;
      }
    }
  }

  idname.copy_utf8_truncated(sock.idname);
  node_socket_set_typeinfo(&ntree, &sock, socktype);
}

void node_modify_socket_type_static(
    bNodeTree *ntree, bNode *node, bNodeSocket *sock, const int type, const int subtype)
{
  const std::optional<StringRefNull> idname = node_static_socket_type(type, subtype);

  if (!idname.has_value()) {
    CLOG_ERROR(&LOG, "static node socket type %d undefined", type);
    return;
  }

  node_modify_socket_type(*ntree, *node, *sock, *idname);
}

bNodeSocket *node_add_socket(bNodeTree &ntree,
                             bNode &node,
                             const eNodeSocketInOut in_out,
                             const StringRefNull idname,
                             const StringRefNull identifier,
                             const StringRefNull name)
{
  BLI_assert(!node.is_frame());
  BLI_assert(!(in_out == SOCK_IN && node.is_group_input()));
  BLI_assert(!(in_out == SOCK_OUT && node.is_group_output()));

  ListBase *lb = (in_out == SOCK_IN ? &node.inputs : &node.outputs);
  bNodeSocket *sock = make_socket(&ntree, &node, in_out, lb, idname, identifier, name);

  BLI_remlink(lb, sock); /* does nothing for new socket */
  BLI_addtail(lb, sock);

  BKE_ntree_update_tag_socket_new(&ntree, sock);

  return sock;
}

bool node_is_static_socket_type(const bNodeSocketType &stype)
{
  /*
   * Cannot rely on type==SOCK_CUSTOM here, because type is 0 by default
   * and can be changed on custom sockets.
   */
  return RNA_struct_is_a(stype.ext_socket.srna, &RNA_NodeSocketStandard);
}

std::optional<StringRefNull> node_static_socket_type(const int type,
                                                     const int subtype,
                                                     const std::optional<int> dimensions)
{
  BLI_assert(!(dimensions.has_value() && type != SOCK_VECTOR));

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
        case PROP_COLOR_TEMPERATURE:
          return "NodeSocketFloatColorTemperature";
        case PROP_FREQUENCY:
          return "NodeSocketFloatFrequency";
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
      if (!dimensions.has_value() || dimensions.value() == 3) {
        switch (PropertySubType(subtype)) {
          case PROP_FACTOR:
            return "NodeSocketVectorFactor";
          case PROP_PERCENTAGE:
            return "NodeSocketVectorPercentage";
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
      }
      else if (dimensions.value() == 2) {
        switch (PropertySubType(subtype)) {
          case PROP_FACTOR:
            return "NodeSocketVectorFactor2D";
          case PROP_PERCENTAGE:
            return "NodeSocketVectorPercentage2D";
          case PROP_TRANSLATION:
            return "NodeSocketVectorTranslation2D";
          case PROP_DIRECTION:
            return "NodeSocketVectorDirection2D";
          case PROP_VELOCITY:
            return "NodeSocketVectorVelocity2D";
          case PROP_ACCELERATION:
            return "NodeSocketVectorAcceleration2D";
          case PROP_EULER:
            return "NodeSocketVectorEuler2D";
          case PROP_XYZ:
            return "NodeSocketVectorXYZ2D";
          case PROP_NONE:
          default:
            return "NodeSocketVector2D";
        }
      }
      else if (dimensions.value() == 4) {
        switch (PropertySubType(subtype)) {
          case PROP_FACTOR:
            return "NodeSocketVectorFactor4D";
          case PROP_PERCENTAGE:
            return "NodeSocketVectorPercentage4D";
          case PROP_TRANSLATION:
            return "NodeSocketVectorTranslation4D";
          case PROP_DIRECTION:
            return "NodeSocketVectorDirection4D";
          case PROP_VELOCITY:
            return "NodeSocketVectorVelocity4D";
          case PROP_ACCELERATION:
            return "NodeSocketVectorAcceleration4D";
          case PROP_EULER:
            return "NodeSocketVectorEuler4D";
          case PROP_XYZ:
            return "NodeSocketVectorXYZ4D";
          case PROP_NONE:
          default:
            return "NodeSocketVector4D";
        }
      }
      else {
        BLI_assert_unreachable();
        return "NodeSocketVector";
      }
    case SOCK_RGBA:
      return "NodeSocketColor";
    case SOCK_STRING:
      switch (PropertySubType(subtype)) {
        case PROP_FILEPATH:
          return "NodeSocketStringFilePath";
        default:
          return "NodeSocketString";
      }
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
    case SOCK_BUNDLE:
      return "NodeSocketBundle";
    case SOCK_CLOSURE:
      return "NodeSocketClosure";
    case SOCK_CUSTOM:
      break;
  }
  return std::nullopt;
}

std::optional<StringRefNull> node_static_socket_interface_type_new(
    const int type, const int subtype, const std::optional<int> dimensions)
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
        case PROP_COLOR_TEMPERATURE:
          return "NodeTreeInterfaceSocketFloatColorTemperature";
        case PROP_FREQUENCY:
          return "NodeTreeInterfaceSocketFloatFrequency";
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
      if (!dimensions.has_value() || dimensions.value() == 3) {
        switch (PropertySubType(subtype)) {
          case PROP_FACTOR:
            return "NodeTreeInterfaceSocketVectorFactor";
          case PROP_PERCENTAGE:
            return "NodeTreeInterfaceSocketVectorPercentage";
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
      }
      else if (dimensions.value() == 2) {
        switch (PropertySubType(subtype)) {
          case PROP_FACTOR:
            return "NodeTreeInterfaceSocketVectorFactor2D";
          case PROP_PERCENTAGE:
            return "NodeTreeInterfaceSocketVectorPercentage2D";
          case PROP_TRANSLATION:
            return "NodeTreeInterfaceSocketVectorTranslation2D";
          case PROP_DIRECTION:
            return "NodeTreeInterfaceSocketVectorDirection2D";
          case PROP_VELOCITY:
            return "NodeTreeInterfaceSocketVectorVelocity2D";
          case PROP_ACCELERATION:
            return "NodeTreeInterfaceSocketVectorAcceleration2D";
          case PROP_EULER:
            return "NodeTreeInterfaceSocketVectorEuler2D";
          case PROP_XYZ:
            return "NodeTreeInterfaceSocketVectorXYZ2D";
          case PROP_NONE:
          default:
            return "NodeTreeInterfaceSocketVector2D";
        }
      }
      else if (dimensions.value() == 4) {
        switch (PropertySubType(subtype)) {
          case PROP_FACTOR:
            return "NodeTreeInterfaceSocketVectorFactor4D";
          case PROP_PERCENTAGE:
            return "NodeTreeInterfaceSocketVectorPercentage4D";
          case PROP_TRANSLATION:
            return "NodeTreeInterfaceSocketVectorTranslation4D";
          case PROP_DIRECTION:
            return "NodeTreeInterfaceSocketVectorDirection4D";
          case PROP_VELOCITY:
            return "NodeTreeInterfaceSocketVectorVelocity4D";
          case PROP_ACCELERATION:
            return "NodeTreeInterfaceSocketVectorAcceleration4D";
          case PROP_EULER:
            return "NodeTreeInterfaceSocketVectorEuler4D";
          case PROP_XYZ:
            return "NodeTreeInterfaceSocketVectorXYZ4D";
          case PROP_NONE:
          default:
            return "NodeTreeInterfaceSocketVector4D";
        }
      }
      else {
        BLI_assert_unreachable();
        return "NodeTreeInterfaceSocketVector";
      }
    case SOCK_RGBA:
      return "NodeTreeInterfaceSocketColor";
    case SOCK_STRING:
      switch (PropertySubType(subtype)) {
        case PROP_FILEPATH:
          return "NodeTreeInterfaceSocketStringFilePath";
        default:
          return "NodeTreeInterfaceSocketString";
      }
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
    case SOCK_BUNDLE:
      return "NodeTreeInterfaceSocketBundle";
    case SOCK_CLOSURE:
      return "NodeTreeInterfaceSocketClosure";
    case SOCK_CUSTOM:
      break;
  }
  return std::nullopt;
}

std::optional<StringRefNull> node_static_socket_label(const int type, const int /*subtype*/)
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
    case SOCK_BUNDLE:
      return "Bundle";
    case SOCK_CLOSURE:
      return "Closure";
    case SOCK_CUSTOM:
      break;
  }
  return std::nullopt;
}

bNodeSocket *node_add_static_socket(bNodeTree &ntree,
                                    bNode &node,
                                    eNodeSocketInOut in_out,
                                    int type,
                                    int subtype,
                                    const StringRefNull identifier,
                                    const StringRefNull name)
{
  const std::optional<StringRefNull> idname = node_static_socket_type(type, subtype);

  if (!idname.has_value()) {
    CLOG_ERROR(&LOG, "static node socket type %d undefined", type);
    return nullptr;
  }

  bNodeSocket *sock = node_add_socket(ntree, node, in_out, *idname, identifier, name);
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

void node_remove_socket(bNodeTree &ntree, bNode &node, bNodeSocket &sock)
{
  node_remove_socket_ex(ntree, node, sock, true);
}

void node_remove_socket_ex(bNodeTree &ntree, bNode &node, bNodeSocket &sock, const bool do_id_user)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    if (link->fromsock == &sock || link->tosock == &sock) {
      node_remove_link(&ntree, *link);
    }
  }

  for (const int64_t i : node.runtime->internal_links.index_range()) {
    const bNodeLink &link = node.runtime->internal_links[i];
    if (link.fromsock == &sock || link.tosock == &sock) {
      node.runtime->internal_links.remove_and_reorder(i);
      BKE_ntree_update_tag_node_internal_link(&ntree, &node);
      break;
    }
  }

  /* this is fast, this way we don't need an in_out argument */
  BLI_remlink(&node.inputs, &sock);
  BLI_remlink(&node.outputs, &sock);

  node_socket_free(&sock, do_id_user);
  MEM_freeN(&sock);

  BKE_ntree_update_tag_socket_removed(&ntree);
}

bNode *node_find_node_by_name(bNodeTree &ntree, const StringRefNull name)
{
  return reinterpret_cast<bNode *>(
      BLI_findstring(&ntree.nodes, name.c_str(), offsetof(bNode, name)));
}

bNode &node_find_node(bNodeTree &ntree, bNodeSocket &socket)
{
  ntree.ensure_topology_cache();
  return socket.owner_node();
}

const bNode &node_find_node(const bNodeTree &ntree, const bNodeSocket &socket)
{
  ntree.ensure_topology_cache();
  return socket.owner_node();
}

bNode *node_find_node_try(bNodeTree &ntree, bNodeSocket &socket)
{
  for (bNode *node : ntree.all_nodes()) {
    const ListBase *sockets = (socket.in_out == SOCK_IN) ? &node->inputs : &node->outputs;
    LISTBASE_FOREACH (const bNodeSocket *, socket_iter, sockets) {
      if (socket_iter == &socket) {
        return node;
      }
    }
  }
  return nullptr;
}

const bNodeTreeInterfaceSocket *node_find_interface_input_by_identifier(const bNodeTree &ntree,
                                                                        const StringRef identifier)
{
  ntree.ensure_interface_cache();
  for (const bNodeTreeInterfaceSocket *input : ntree.interface_inputs()) {
    if (input->identifier == identifier) {
      return input;
    }
  }
  return nullptr;
}

bNode *node_find_root_parent(bNode &node)
{
  bNode *parent_iter = &node;
  while (parent_iter->parent != nullptr) {
    parent_iter = parent_iter->parent;
  }
  if (!parent_iter->is_frame()) {
    return nullptr;
  }
  return parent_iter;
}

bool node_is_parent_and_child(const bNode &parent, const bNode &child)
{
  for (const bNode *child_iter = &child; child_iter; child_iter = child_iter->parent) {
    if (child_iter == &parent) {
      return true;
    }
  }
  return false;
}

void node_chain_iterator(const bNodeTree *ntree,
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
    node_chain_iterator(
        ntree, reversed ? link->fromnode : link->tonode, callback, userdata, reversed);
  }
}

static void iter_backwards_ex(const bNodeTree *ntree,
                              bNode *node_start,
                              bool (*callback)(bNode *, bNode *, void *),
                              void *userdata,
                              const char recursion_mask)
{
  blender::Stack<bNode *> stack;
  blender::Stack<bNode *> zone_stack;
  stack.push(node_start);

  while (!stack.is_empty() || !zone_stack.is_empty()) {
    bNode *node = !stack.is_empty() ? stack.pop() : zone_stack.pop();

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
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
        break;
      }
      stack.push(link->fromnode);
    }
    /* Zone input nodes are implicitly linked to their corresponding zone output nodes,
     * even if there is no bNodeLink between them. */
    if (const bNodeZoneType *zone_type = zone_type_by_node_type(node->type_legacy)) {
      if (zone_type->output_type == node->type_legacy) {
        if (bNode *zone_input_node = const_cast<bNode *>(
                zone_type->get_corresponding_input(*ntree, *node)))
        {
          if (callback(zone_input_node, node, userdata)) {
            zone_stack.push(zone_input_node);
          }
        }
      }
    }
  }
}

void node_chain_iterator_backwards(const bNodeTree *ntree,
                                   bNode *node_start,
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

void node_parents_iterator(bNode *node, bool (*callback)(bNode *, void *), void *userdata)
{
  if (node->parent) {
    if (!callback(node->parent, userdata)) {
      return;
    }
    node_parents_iterator(node->parent, callback, userdata);
  }
}

void node_unique_name(bNodeTree &ntree, bNode &node)
{
  BLI_uniquename(
      &ntree.nodes, &node, DATA_("Node"), '.', offsetof(bNode, name), sizeof(node.name));
}

void node_unique_id(bNodeTree &ntree, bNode &node)
{
  /* Use a pointer cast to avoid overflow warnings. */
  const double time = BLI_time_now_seconds() * 1000000.0;
  RandomNumberGenerator id_rng{*reinterpret_cast<const uint32_t *>(&time)};

  /* In the unlikely case that the random ID doesn't match, choose a new one until it does. */
  int32_t new_id = id_rng.get_int32();
  while (ntree.runtime->nodes_by_id.contains_as(new_id) || new_id <= 0) {
    new_id = id_rng.get_int32();
  }

  node.identifier = new_id;
  ntree.runtime->nodes_by_id.add_new(&node);
  node.runtime->index_in_tree = ntree.runtime->nodes_by_id.index_range().last();
  BLI_assert(node.runtime->index_in_tree == ntree.runtime->nodes_by_id.index_of(&node));
}

bNode *node_add_node(const bContext *C,
                     bNodeTree &ntree,
                     const StringRef idname,
                     std::optional<int> unique_identifier)
{
  bNode *node = MEM_callocN<bNode>(__func__);
  node->runtime = MEM_new<bNodeRuntime>(__func__);
  BLI_addtail(&ntree.nodes, node);
  if (unique_identifier) {
    node->identifier = *unique_identifier;
    ntree.runtime->nodes_by_id.add_new(node);
  }
  else {
    node_unique_id(ntree, *node);
  }
  node->ui_order = ntree.all_nodes().size();

  idname.copy_utf8_truncated(node->idname);
  node_set_typeinfo(C, &ntree, node, node_type_find(idname));

  BKE_ntree_update_tag_node_new(&ntree, node);

  return node;
}

bNode *node_add_static_node(const bContext *C, bNodeTree &ntree, const int type)
{
  std::optional<StringRefNull> idname;

  for (bNodeType *ntype : node_types_get()) {
    /* Do an extra poll here, because some int types are used
     * for multiple node types, this helps find the desired type. */
    if (ntype->type_legacy != type) {
      continue;
    }

    const char *disabled_hint;
    if (ntype->poll && ntype->poll(ntype, &ntree, &disabled_hint)) {
      idname = ntype->idname;
      break;
    }
  }
  if (!idname) {
    CLOG_ERROR(&LOG, "static node type %d undefined", type);
    return nullptr;
  }
  return node_add_node(C, ntree, *idname);
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
                              const std::optional<StringRefNull> dst_unique_name,
                              const std::optional<int> dst_unique_identifier,
                              Map<const bNodeSocket *, bNodeSocket *> &socket_map,
                              const bool allow_duplicate_names)
{
  bNode *node_dst = MEM_mallocN<bNode>(__func__);
  *node_dst = node_src;
  node_dst->runtime = MEM_new<bNodeRuntime>(__func__);
  if (dst_unique_name) {
    BLI_assert(dst_unique_name->size() < sizeof(node_dst->name));
    STRNCPY_UTF8(node_dst->name, dst_unique_name->c_str());
  }
  else if (dst_tree) {
    if (!allow_duplicate_names) {
      node_unique_name(*dst_tree, *node_dst);
    }
  }
  if (dst_unique_identifier) {
    node_dst->identifier = *dst_unique_identifier;
    if (dst_tree) {
      dst_tree->runtime->nodes_by_id.add_new(node_dst);
    }
  }
  else if (dst_tree) {
    node_unique_id(*dst_tree, *node_dst);
  }

  /* Can be called for nodes outside a node tree (e.g. clipboard). */
  if (dst_tree) {
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
  if (node_src.system_properties) {
    node_dst->system_properties = IDP_CopyProperty_ex(node_src.system_properties, flag);
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
    PointerRNA ptr = RNA_pointer_create_discrete(
        reinterpret_cast<ID *>(dst_tree), &RNA_Node, node_dst);

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

  switch (node.type_legacy) {
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
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
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

  const bke::DataTypeConversions &convert = bke::get_implicit_type_conversions();

  if (src.is_multi_input()) {
    /* Multi input sockets no have value. */
    return;
  }
  if (dst_node.is_reroute() || src_node.is_reroute()) {
    /* Reroute node can't have ownership of socket value directly. */
    return;
  }

  const CPPType &src_type = *src.typeinfo->base_cpp_type;
  const CPPType &dst_type = *dst.typeinfo->base_cpp_type;
  if (&src_type != &dst_type) {
    if (!convert.is_convertible(src_type, dst_type)) {
      return;
    }
  }

  /* Special handling for strings because the generic code below can't handle them. */
  if (src.type == SOCK_STRING && dst.type == SOCK_STRING &&
      dst_node.is_type("FunctionNodeInputString"))
  {
    auto *src_value = static_cast<bNodeSocketValueString *>(src.default_value);
    auto *dst_storage = static_cast<NodeInputString *>(dst_node.storage);
    MEM_SAFE_FREE(dst_storage->string);
    dst_storage->string = BLI_strdup_null(src_value->value);
    return;
  }

  void *src_value = socket_value_storage(src);
  if (!src_value) {
    return;
  }

  BUFFER_FOR_CPP_TYPE_VALUE(dst_type, dst_buffer);
  convert.convert_to_uninitialized(src_type, dst_type, src_value, dst_buffer);

  if (dst_node.is_type("ShaderNodeCombineXYZ")) {
    const float3 &src_value = *static_cast<float3 *>(dst_buffer);
    dst_node.input_socket(0).default_value_typed<bNodeSocketValueFloat>()->value = src_value.x;
    dst_node.input_socket(1).default_value_typed<bNodeSocketValueFloat>()->value = src_value.y;
    dst_node.input_socket(2).default_value_typed<bNodeSocketValueFloat>()->value = src_value.z;
    return;
  }

  void *dst_value = node_static_value_storage_for(dst_node, dst);
  if (!dst_value) {
    return;
  }

  dst_type.move_assign(dst_buffer, dst_value);

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

bNodeLink &node_add_link(
    bNodeTree &ntree, bNode &fromnode, bNodeSocket &fromsock, bNode &tonode, bNodeSocket &tosock)
{
  BLI_assert(ntree.all_nodes().contains(&fromnode));
  BLI_assert(ntree.all_nodes().contains(&tonode));

  bNodeLink *link = nullptr;
  if (eNodeSocketInOut(fromsock.in_out) == SOCK_OUT && eNodeSocketInOut(tosock.in_out) == SOCK_IN)
  {
    link = MEM_callocN<bNodeLink>(__func__);
    BLI_addtail(&ntree.links, link);
    link->fromnode = &fromnode;
    link->fromsock = &fromsock;
    link->tonode = &tonode;
    link->tosock = &tosock;
  }
  else if (eNodeSocketInOut(fromsock.in_out) == SOCK_IN &&
           eNodeSocketInOut(tosock.in_out) == SOCK_OUT)
  {
    /* OK but flip */
    link = MEM_callocN<bNodeLink>(__func__);
    BLI_addtail(&ntree.links, link);
    link->fromnode = &tonode;
    link->fromsock = &tosock;
    link->tonode = &fromnode;
    link->tosock = &fromsock;
  }

  BKE_ntree_update_tag_link_added(&ntree, link);

  if (link != nullptr && link->tosock->is_multi_input()) {
    link->multi_input_sort_id = node_count_links(&ntree, link->tosock) - 1;
  }

  return *link;
}

void node_remove_link(bNodeTree *ntree, bNodeLink &link)
{
  /* Can be called for links outside a node tree (e.g. clipboard). */
  if (ntree) {
    BLI_remlink(&ntree->links, &link);
  }

  if (link.tosock) {
    link.tosock->link = nullptr;
  }
  MEM_freeN(&link);

  if (ntree) {
    BKE_ntree_update_tag_link_removed(ntree);
  }
}

void node_link_set_mute(bNodeTree &ntree, bNodeLink &link, const bool muted)
{
  const bool was_muted = link.is_muted();
  SET_FLAG_FROM_TEST(link.flag, muted, NODE_LINK_MUTED);
  if (muted != was_muted) {
    BKE_ntree_update_tag_link_mute(&ntree, &link);
  }
}

void node_remove_socket_links(bNodeTree &ntree, bNodeSocket &sock)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    if (link->fromsock == &sock || link->tosock == &sock) {
      node_remove_link(&ntree, *link);
    }
  }
}

bool node_link_is_hidden(const bNodeLink &link)
{
  return !(link.fromsock->is_visible() && link.tosock->is_visible());
}

bool node_link_is_selected(const bNodeLink &link)
{
  return (link.fromnode->flag & NODE_SELECT) || (link.tonode->flag & NODE_SELECT);
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

void node_internal_relink(bNodeTree &ntree, bNode &node)
{
  /* store link pointers in output sockets, for efficient lookup */
  for (bNodeLink &link : node.runtime->internal_links) {
    link.tosock->link = &link;
  }

  Vector<bNodeLink *> duplicate_links_to_remove;

  /* redirect downstream links */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    /* do we have internal link? */
    if (link->fromnode != &node) {
      continue;
    }

    bNodeLink *internal_link = link->fromsock->link;
    bNodeLink *fromlink = internal_link ? internal_link->fromsock->link : nullptr;

    if (fromlink == nullptr) {
      if (link->tosock->is_multi_input()) {
        adjust_multi_input_indices_after_removed_link(
            &ntree, link->tosock, link->multi_input_sort_id);
      }
      node_remove_link(&ntree, *link);
      continue;
    }

    if (link->tosock->is_multi_input()) {
      /* remove the link that would be the same as the relinked one */
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link_to_compare, &ntree.links) {
        if (link_to_compare->fromsock == fromlink->fromsock &&
            link_to_compare->tosock == link->tosock)
        {
          adjust_multi_input_indices_after_removed_link(
              &ntree, link_to_compare->tosock, link_to_compare->multi_input_sort_id);
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

    BKE_ntree_update_tag_link_changed(&ntree);
  }

  for (bNodeLink *link : duplicate_links_to_remove) {
    node_remove_link(&ntree, *link);
  }

  /* remove remaining upstream links */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    if (link->tonode == &node) {
      node_remove_link(&ntree, *link);
    }
  }
}

void node_attach_node(bNodeTree &ntree, bNode &node, bNode &parent)
{
  BLI_assert(parent.is_frame());
  BLI_assert(!node_is_parent_and_child(parent, node));
  node.parent = &parent;
  BKE_ntree_update_tag_parent_change(&ntree, &node);
}

void node_detach_node(bNodeTree &ntree, bNode &node)
{
  if (node.parent) {
    BLI_assert(node.parent->is_frame());
    node.parent = nullptr;
    BKE_ntree_update_tag_parent_change(&ntree, &node);
  }
}

void node_position_relative(bNode &from_node,
                            const bNode &to_node,
                            const bNodeSocket *from_sock,
                            const bNodeSocket &to_sock)
{
  float offset_x;
  int tot_sock_idx;

  /* Socket to plug into. */
  if (eNodeSocketInOut(to_sock.in_out) == SOCK_IN) {
    offset_x = -(from_node.typeinfo->width + 50);
    tot_sock_idx = BLI_listbase_count(&to_node.outputs);
    tot_sock_idx += BLI_findindex(&to_node.inputs, &to_sock);
  }
  else {
    offset_x = to_node.typeinfo->width + 50;
    tot_sock_idx = BLI_findindex(&to_node.outputs, &to_sock);
  }

  BLI_assert(tot_sock_idx != -1);

  float offset_y = U.widget_unit * tot_sock_idx;

  /* Output socket. */
  if (from_sock) {
    if (eNodeSocketInOut(from_sock->in_out) == SOCK_IN) {
      tot_sock_idx = BLI_listbase_count(&from_node.outputs);
      tot_sock_idx += BLI_findindex(&from_node.inputs, from_sock);
    }
    else {
      tot_sock_idx = BLI_findindex(&from_node.outputs, from_sock);
    }
  }

  BLI_assert(tot_sock_idx != -1);

  offset_y -= U.widget_unit * tot_sock_idx;

  from_node.location[0] = to_node.location[0] + offset_x;
  from_node.location[1] = to_node.location[1] - offset_y;
}

void node_position_propagate(bNode &node)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (socket->link != nullptr) {
      bNodeLink *link = socket->link;
      node_position_relative(*link->fromnode, *link->tonode, link->fromsock, *link->tosock);
      node_position_propagate(*link->fromnode);
    }
  }
}

static bNodeTree *node_tree_add_tree_do(Main *bmain,
                                        std::optional<Library *> owner_library,
                                        ID *owner_id,
                                        const bool is_embedded,
                                        const StringRef name,
                                        const StringRef idname)
{
  /* trees are created as local trees for material or texture nodes,
   * node groups and other tree types are created as library data.
   */
  int flag = 0;
  if (is_embedded || bmain == nullptr) {
    flag |= LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT;
  }
  BLI_assert_msg(!owner_library || !owner_id,
                 "Embedded NTrees should never have a defined owner library here");
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(
      BKE_libblock_alloc_in_lib(bmain, owner_library, ID_NT, std::string(name).c_str(), flag));
  BKE_libblock_init_empty(&ntree->id);
  if (is_embedded) {
    BLI_assert(owner_id != nullptr);
    ntree->id.flag |= ID_FLAG_EMBEDDED_DATA;
    ntree->owner_id = owner_id;
    bNodeTree **ntree_owner_ptr = node_tree_ptr_from_id(owner_id);
    BLI_assert(ntree_owner_ptr != nullptr);
    *ntree_owner_ptr = ntree;
  }
  else {
    BLI_assert(owner_id == nullptr);
  }

  idname.copy_utf8_truncated(ntree->idname);
  ntree_set_typeinfo(ntree, node_tree_type_find(idname));

  return ntree;
}

bNodeTree *node_tree_add_tree(Main *bmain, const StringRef name, const StringRef idname)
{
  return node_tree_add_tree_do(bmain, std::nullopt, nullptr, false, name, idname);
}

bNodeTree *node_tree_add_in_lib(Main *bmain,
                                Library *owner_library,
                                const StringRefNull name,
                                const StringRefNull idname)
{
  return node_tree_add_tree_do(bmain, owner_library, nullptr, false, name, idname);
}

bNodeTree *node_tree_add_tree_embedded(Main * /*bmain*/,
                                       ID *owner_id,
                                       const StringRefNull name,
                                       const StringRefNull idname)
{
  return node_tree_add_tree_do(nullptr, std::nullopt, owner_id, true, name, idname);
}

bNodeTree *node_tree_copy_tree_ex(const bNodeTree &ntree, Main *bmain, const bool do_id_user)
{
  const int flag = do_id_user ? 0 : LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN;

  bNodeTree *ntree_copy = reinterpret_cast<bNodeTree *>(
      BKE_id_copy_ex(bmain, reinterpret_cast<const ID *>(&ntree), nullptr, flag));
  return ntree_copy;
}
bNodeTree *node_tree_copy_tree(Main *bmain, const bNodeTree &ntree)
{
  return node_tree_copy_tree_ex(ntree, bmain, true);
}

/* *************** Node Preview *********** */

/* XXX this should be removed eventually ...
 * Currently BKE functions are modeled closely on previous code,
 * using node_preview_init_tree to set up previews for a whole node tree in advance.
 * This should be left more to the individual node tree implementations. */

bool node_preview_used(const bNode &node)
{
  /* XXX check for closed nodes? */
  return (node.typeinfo->flag & NODE_PREVIEW) != 0;
}

bNodePreview *node_preview_verify(Map<bNodeInstanceKey, bNodePreview> &previews,
                                  bNodeInstanceKey key,
                                  const int xsize,
                                  const int ysize,
                                  const bool create)
{
  bNodePreview *preview = create ?
                              &previews.lookup_or_add_cb(key,
                                                         [&]() {
                                                           bNodePreview preview;
                                                           preview.ibuf = IMB_allocImBuf(
                                                               xsize, ysize, 32, IB_byte_data);
                                                           return preview;
                                                         }) :
                              previews.lookup_ptr(key);
  if (!preview) {
    return nullptr;
  }

  /* node previews can get added with variable size this way */
  if (xsize == 0 || ysize == 0) {
    return preview;
  }

  /* sanity checks & initialize */
  const uint size[2] = {uint(xsize), uint(ysize)};
  IMB_rect_size_set(preview->ibuf, size);
  if (preview->ibuf->byte_buffer.data == nullptr) {
    IMB_alloc_byte_pixels(preview->ibuf);
  }
  /* no clear, makes nicer previews */

  return preview;
}

bNodePreview::bNodePreview(const bNodePreview &other)
{
  this->ibuf = IMB_dupImBuf(other.ibuf);
}

bNodePreview::bNodePreview(bNodePreview &&other)
{
  this->ibuf = other.ibuf;
  other.ibuf = nullptr;
}

bNodePreview::~bNodePreview()
{
  if (this->ibuf) {
    IMB_freeImBuf(this->ibuf);
  }
}

static void node_preview_init_tree_recursive(Map<bNodeInstanceKey, bNodePreview> &previews,
                                             bNodeTree *ntree,
                                             bNodeInstanceKey parent_key,
                                             const int xsize,
                                             const int ysize)
{
  for (bNode *node : ntree->all_nodes()) {
    bNodeInstanceKey key = node_instance_key(parent_key, ntree, node);

    if (node_preview_used(*node)) {
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
  node_preview_init_tree_recursive(
      ntree->runtime->previews, ntree, NODE_INSTANCE_KEY_BASE, xsize, ysize);
}

static void collect_used_previews(Map<bNodeInstanceKey, bNodePreview> &previews,
                                  bNodeTree *ntree,
                                  bNodeInstanceKey parent_key,
                                  Set<bNodeInstanceKey> &used)
{
  for (bNode *node : ntree->all_nodes()) {
    bNodeInstanceKey key = node_instance_key(parent_key, ntree, node);

    if (node_preview_used(*node)) {
      used.add(key);
    }

    if (node->is_group()) {
      if (bNodeTree *group = reinterpret_cast<bNodeTree *>(node->id)) {
        collect_used_previews(previews, group, key, used);
      }
    }
  }
}

void node_preview_remove_unused(bNodeTree *ntree)
{
  Set<bNodeInstanceKey> used_previews;
  collect_used_previews(ntree->runtime->previews, ntree, NODE_INSTANCE_KEY_BASE, used_previews);
  ntree->runtime->previews.remove_if([&](const MapItem<bNodeInstanceKey, bNodePreview> &item) {
    return !used_previews.contains(item.key);
  });
}

void node_preview_merge_tree(bNodeTree *to_ntree, bNodeTree *from_ntree, bool remove_old)
{
  if (remove_old || to_ntree->runtime->previews.is_empty()) {
    to_ntree->runtime->previews.clear();
    to_ntree->runtime->previews = std::move(from_ntree->runtime->previews);
    node_preview_remove_unused(to_ntree);
  }
  else {
    for (const auto &item : from_ntree->runtime->previews.items()) {
      to_ntree->runtime->previews.add(item.key, std::move(item.value));
    }
    from_ntree->runtime->previews.clear();
  }
}

void node_unlink_node(bNodeTree &ntree, bNode &node)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    ListBase *lb = nullptr;
    if (link->fromnode == &node) {
      lb = &node.outputs;
    }
    else if (link->tonode == &node) {
      lb = &node.inputs;
    }

    if (lb) {
      /* Only bother adjusting if the socket is not on the node we're deleting. */
      if (link->tonode != &node && link->tosock->is_multi_input()) {
        adjust_multi_input_indices_after_removed_link(
            &ntree, link->tosock, link->multi_input_sort_id);
      }
      LISTBASE_FOREACH (const bNodeSocket *, sock, lb) {
        if (link->fromsock == sock || link->tosock == sock) {
          node_remove_link(&ntree, *link);
          break;
        }
      }
    }
  }
}

void node_unlink_attached(bNodeTree *ntree, const bNode *parent)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->parent == parent) {
      node_detach_node(*ntree, *node);
    }
  }
}

void node_rebuild_id_vector(bNodeTree &node_tree)
{
  /* Rebuild nodes #VectorSet which must have the same order as the list. */
  node_tree.runtime->nodes_by_id.clear();
  int i;
  LISTBASE_FOREACH_INDEX (bNode *, node, &node_tree.nodes, i) {
    node_tree.runtime->nodes_by_id.add_new(node);
    node->runtime->index_in_tree = i;
  }
}

void node_free_node(bNodeTree *ntree, bNode &node)
{
  /* since it is called while free database, node->id is undefined */

  /* can be called for nodes outside a node tree (e.g. clipboard) */
  if (ntree) {
    BLI_remlink(&ntree->nodes, &node);

    const bool was_last = ntree->runtime->nodes_by_id.as_span().last() == &node;
    if (was_last) {
      /* No need to rebuild the entire bNodeTreeRuntime::nodes_by_id when the removed node is the
       * last one. */
      ntree->runtime->nodes_by_id.pop();
    }
    else {
      /* Rebuild nodes #VectorSet which must have the same order as the list. */
      node_rebuild_id_vector(*ntree);
    }

    /* texture node has bad habit of keeping exec data around */
    if (ntree->type == NTREE_TEXTURE && ntree->runtime->execdata) {
      ntreeTexEndExecTree(ntree->runtime->execdata);
      ntree->runtime->execdata = nullptr;
    }
  }

  if (node.typeinfo->freefunc) {
    node.typeinfo->freefunc(&node);
  }

  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node.inputs) {
    /* Remember, no ID user refcount management here! */
    node_socket_free(sock, false);
    MEM_freeN(sock);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node.outputs) {
    /* Remember, no ID user refcount management here! */
    node_socket_free(sock, false);
    MEM_freeN(sock);
  }

  MEM_SAFE_FREE(node.panel_states_array);

  if (node.prop) {
    /* Remember, no ID user refcount management here! */
    IDP_FreePropertyContent_ex(node.prop, false);
    MEM_freeN(node.prop);
  }
  if (node.system_properties) {
    /* Remember, no ID user refcount management here! */
    IDP_FreePropertyContent_ex(node.system_properties, false);
    MEM_freeN(node.system_properties);
  }

  if (node.runtime->declaration) {
    /* Only free if this declaration is not shared with the node type, which can happen if it does
     * not depend on any context. */
    if (node.runtime->declaration != node.typeinfo->static_declaration) {
      delete node.runtime->declaration;
    }
  }

  MEM_delete(node.runtime);
  MEM_freeN(&node);

  if (ntree) {
    BKE_ntree_update_tag_node_removed(ntree);
  }
}

void node_tree_free_local_node(bNodeTree &ntree, bNode &node)
{
  /* For removing nodes while editing localized node trees. */
  BLI_assert((ntree.id.tag & ID_TAG_LOCALIZED) != 0);

  /* These two lines assume the caller might want to free a single node and maintain
   * a valid state in the node tree. */
  node_unlink_node(ntree, node);
  node_unlink_attached(&ntree, &node);

  node_free_node(&ntree, node);
  node_rebuild_id_vector(ntree);
}

void node_remove_node(
    Main *bmain, bNodeTree &ntree, bNode &node, const bool do_id_user, const bool remove_animation)
{
  /* This function is not for localized node trees, we do not want
   * do to ID user reference-counting and removal of animation data then. */
  BLI_assert((ntree.id.tag & ID_TAG_LOCALIZED) == 0);

  if (do_id_user) {
    /* Free callback for NodeCustomGroup. */
    if (node.typeinfo->freefunc_api) {
      PointerRNA ptr = RNA_pointer_create_discrete(&ntree.id, &RNA_Node, &node);

      node.typeinfo->freefunc_api(&ptr);
    }

    /* Do user counting. */
    if (node.id) {
      id_us_min(node.id);
    }

    LISTBASE_FOREACH (bNodeSocket *, sock, &node.inputs) {
      socket_id_user_decrement(sock);
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node.outputs) {
      socket_id_user_decrement(sock);
    }
  }

  if (remove_animation) {
    char propname_esc[MAX_IDPROP_NAME * 2];
    char prefix[MAX_IDPROP_NAME * 2];

    BLI_str_escape(propname_esc, node.name, sizeof(propname_esc));
    SNPRINTF_UTF8(prefix, "nodes[\"%s\"]", propname_esc);

    if (BKE_animdata_fix_paths_remove(&ntree.id, prefix)) {
      if (bmain != nullptr) {
        DEG_relations_tag_update(bmain);
      }
    }
  }

  node_unlink_node(ntree, node);
  node_unlink_attached(&ntree, &node);

  /* Free node itself. */
  node_free_node(&ntree, node);
  node_rebuild_id_vector(ntree);
}

static void free_localized_node_groups(bNodeTree *ntree)
{
  /* Only localized node trees store a copy for each node group tree.
   * Each node group tree in a localized node tree can be freed,
   * since it is a localized copy itself (no risk of accessing freed
   * data in main, see #37939). */
  if (!(ntree->id.tag & ID_TAG_LOCALIZED)) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    bNodeTree *ngroup = reinterpret_cast<bNodeTree *>(node->id);
    if (node->is_group() && ngroup != nullptr) {
      node_tree_free_tree(*ngroup);
      MEM_freeN(ngroup);
    }
  }
}

void node_tree_free_tree(bNodeTree &ntree)
{
  ntree_free_data(&ntree.id);
  BKE_animdata_free(&ntree.id, false);
  BKE_libblock_free_runtime_data(&ntree.id);
}

void node_tree_free_embedded_tree(bNodeTree *ntree)
{
  node_tree_free_tree(*ntree);
  BKE_libblock_free_data(&ntree->id, true);
  BKE_libblock_free_data_py(&ntree->id);
}

void node_tree_set_output(bNodeTree &ntree)
{
  const bool is_compositor = ntree.type == NTREE_COMPOSIT;
  const bool is_geometry = ntree.type == NTREE_GEOMETRY;
  /* find the active outputs, might become tree type dependent handler */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->typeinfo->nclass == NODE_CLASS_OUTPUT) {
      /* we need a check for which output node should be tagged like this, below an exception */
      if (node->is_type("CompositorNodeOutputFile")) {
        continue;
      }
      const bool node_is_output = node->is_type("CompositorNodeViewer") ||
                                  node->is_type("GeometryNodeViewer");

      int output = 0;
      /* there is more types having output class, each one is checked */

      LISTBASE_FOREACH (bNode *, tnode, &ntree.nodes) {
        if (tnode->typeinfo->nclass != NODE_CLASS_OUTPUT) {
          continue;
        }

        /* same type, exception for viewer */
        const bool tnode_is_output = tnode->is_type("CompositorNodeViewer") ||
                                     tnode->is_type("GeometryNodeViewer");
        const bool viewer_case = (is_compositor || is_geometry) && tnode_is_output &&
                                 node_is_output;
        const bool has_same_shortcut = viewer_case && node != tnode &&
                                       tnode->custom1 == node->custom1 &&
                                       tnode->custom1 != NODE_VIEWER_SHORTCUT_NONE;

        if (tnode->type_legacy == node->type_legacy || viewer_case) {
          if (tnode->flag & NODE_DO_OUTPUT) {
            output++;
            if (output > 1) {
              tnode->flag &= ~NODE_DO_OUTPUT;
            }
          }
        }
        if (has_same_shortcut) {
          tnode->custom1 = NODE_VIEWER_SHORTCUT_NONE;
        }
      }

      /* Only geometry nodes is allowed to have no active output in the node tree. */
      if (output == 0 && !is_geometry) {
        node->flag |= NODE_DO_OUTPUT;
      }
    }

    /* group node outputs use this flag too */
    if (node->is_group_output()) {
      int output = 0;
      LISTBASE_FOREACH (bNode *, tnode, &ntree.nodes) {
        if (!tnode->is_group_output()) {
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

bNodeTree **node_tree_ptr_from_id(ID *id)
{
  /* If this is ever extended such that a non-animatable ID type can embed a node
   * tree, update blender::animrig::internal::rebuild_slot_user_cache(). That
   * function assumes that node trees can only be embedded by animatable IDs. */

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
      /* Needed for backward compatibility. */
      return &reinterpret_cast<Scene *>(id)->nodetree;
    case ID_LS:
      return &reinterpret_cast<FreestyleLineStyle *>(id)->nodetree;
    default:
      return nullptr;
  }
}

bNodeTree *node_tree_from_id(ID *id)
{
  bNodeTree **nodetree = node_tree_ptr_from_id(id);
  return (nodetree != nullptr) ? *nodetree : nullptr;
}

void node_tree_node_flag_set(bNodeTree &ntree, const int flag, const bool enable)
{
  for (bNode *node : ntree.all_nodes()) {
    if (enable) {
      node->flag |= flag;
    }
    else {
      node->flag &= ~flag;
    }
  }
}

bNodeTree *node_tree_localize(bNodeTree *ntree, std::optional<ID *> new_owner_id)
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

  ltree->id.tag |= ID_TAG_LOCALIZED;

  LISTBASE_FOREACH (bNode *, node, &ltree->nodes) {
    bNodeTree *group = reinterpret_cast<bNodeTree *>(node->id);
    if (node->is_group() && group != nullptr) {
      node->id = reinterpret_cast<ID *>(node_tree_localize(group, nullptr));
    }
  }

  /* Ensures only a single output node is enabled. */
  node_tree_set_output(*ntree);

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

void node_tree_local_merge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree)
{
  if (ntree && localtree) {
    if (ntree->typeinfo->local_merge) {
      ntree->typeinfo->local_merge(bmain, localtree, ntree);
    }
  }
}

static bool ntree_contains_tree_exec(const bNodeTree &tree_to_search_in,
                                     const bNodeTree &tree_to_search_for,
                                     Set<const bNodeTree *> &already_passed)
{
  if (&tree_to_search_in == &tree_to_search_for) {
    return true;
  }

  tree_to_search_in.ensure_topology_cache();
  for (const bNode *node_group : tree_to_search_in.group_nodes()) {
    const bNodeTree *sub_tree_search_in = reinterpret_cast<const bNodeTree *>(node_group->id);
    if (!sub_tree_search_in) {
      continue;
    }
    if (!already_passed.add(sub_tree_search_in)) {
      continue;
    }
    if (ntree_contains_tree_exec(*sub_tree_search_in, tree_to_search_for, already_passed)) {
      return true;
    }
  }

  return false;
}

bool node_tree_contains_tree(const bNodeTree &tree_to_search_in,
                             const bNodeTree &tree_to_search_for)
{
  if (&tree_to_search_in == &tree_to_search_for) {
    return true;
  }

  Set<const bNodeTree *> already_passed;
  return ntree_contains_tree_exec(tree_to_search_in, tree_to_search_for, already_passed);
}

int node_count_socket_links(const bNodeTree &ntree, const bNodeSocket &sock)
{
  int tot = 0;
  LISTBASE_FOREACH (const bNodeLink *, link, &ntree.links) {
    if (link->fromsock == &sock || link->tosock == &sock) {
      tot++;
    }
  }
  return tot;
}

bNode *node_get_active(bNodeTree &ntree)
{
  for (bNode *node : ntree.all_nodes()) {
    if (node->flag & NODE_ACTIVE) {
      return node;
    }
  }
  return nullptr;
}

bool node_set_selected(bNode &node, const bool select)
{
  bool changed = false;
  if (select != ((node.flag & NODE_SELECT) != 0)) {
    changed = true;
    SET_FLAG_FROM_TEST(node.flag, select, NODE_SELECT);
  }
  if (select) {
    return changed;
  }
  /* Deselect sockets too. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node.inputs) {
    changed |= (sock->flag & NODE_SELECT) != 0;
    sock->flag &= ~NODE_SELECT;
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &node.outputs) {
    changed |= (sock->flag & NODE_SELECT) != 0;
    sock->flag &= ~NODE_SELECT;
  }
  return changed;
}

void node_clear_active(bNodeTree &ntree)
{
  for (bNode *node : ntree.all_nodes()) {
    node->flag &= ~NODE_ACTIVE;
  }
}

void node_set_active(bNodeTree &ntree, bNode &node)
{
  const bool is_paint_canvas = node_supports_active_flag(node, NODE_ACTIVE_PAINT_CANVAS);
  const bool is_texture_class = node_supports_active_flag(node, NODE_ACTIVE_TEXTURE);
  int flags_to_set = NODE_ACTIVE;
  SET_FLAG_FROM_TEST(flags_to_set, is_paint_canvas, NODE_ACTIVE_PAINT_CANVAS);
  SET_FLAG_FROM_TEST(flags_to_set, is_texture_class, NODE_ACTIVE_TEXTURE);

  /* Make sure only one node is active per node tree. */
  for (bNode *tnode : ntree.all_nodes()) {
    tnode->flag &= ~flags_to_set;
  }
  node.flag |= flags_to_set;
}

void node_set_socket_availability(bNodeTree &ntree, bNodeSocket &sock, const bool is_available)
{
  if (is_available == sock.is_available()) {
    return;
  }
  if (is_available) {
    sock.flag &= ~SOCK_UNAVAIL;
  }
  else {
    sock.flag |= SOCK_UNAVAIL;
  }
  BKE_ntree_update_tag_socket_availability(&ntree, &sock);
}

int node_socket_link_limit(const bNodeSocket &sock)
{
  if (sock.is_multi_input()) {
    return 4095;
  }
  if (sock.typeinfo == nullptr) {
    return sock.limit;
  }
  const bNodeSocketType &stype = *sock.typeinfo;
  if (!stype.use_link_limits_of_type) {
    return sock.limit;
  }
  return eNodeSocketInOut(sock.in_out) == SOCK_IN ? stype.input_link_limit :
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

void node_socket_declarations_update(bNode *node)
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

bool node_declaration_ensure_on_outdated_node(bNodeTree &ntree, bNode &node)
{
  if (node.runtime->declaration != nullptr) {
    return false;
  }
  if (node.typeinfo->declare) {
    if (node.typeinfo->static_declaration) {
      if (!node.typeinfo->static_declaration->is_context_dependent) {
        node.runtime->declaration = node.typeinfo->static_declaration;
        return true;
      }
    }
  }
  if (node.typeinfo->declare) {
    nodes::update_node_declaration_and_sockets(ntree, node);
    return true;
  }
  return false;
}

bool node_declaration_ensure(bNodeTree &ntree, bNode &node)
{
  if (node_declaration_ensure_on_outdated_node(ntree, node)) {
    node_socket_declarations_update(&node);
    return true;
  }
  return false;
}

float2 node_dimensions_get(const bNode &node)
{
  return float2(node.runtime->draw_bounds.xmax, node.runtime->draw_bounds.ymax) -
         float2(node.runtime->draw_bounds.xmin, node.runtime->draw_bounds.ymin);
}

void node_tag_update_id(bNode &node)
{
  node.runtime->update |= NODE_UPDATE_ID;
}

void node_internal_links(bNode &node, bNodeLink **r_links, int *r_len)
{
  *r_links = node.runtime->internal_links.data();
  *r_len = node.runtime->internal_links.size();
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

bNodeInstanceKey node_instance_key(bNodeInstanceKey parent_key,
                                   const bNodeTree *ntree,
                                   const bNode *node)
{
  bNodeInstanceKey key = node_hash_int_str(parent_key, ntree->id.name + 2);

  if (node) {
    key = node_hash_int_str(key, node->name);
  }

  return key;
}

/* Build a set of built-in node types to check for known types. */
static Set<int> get_known_node_types_set()
{
  Set<int> result;
  for (const bNodeType *ntype : node_types_get()) {
    result.add(ntype->type_legacy);
  }
  return result;
}

static bool can_read_node_type(const bNode &node)
{
  /* Can always read custom node types. */
  if (ELEM(node.type_legacy, NODE_CUSTOM, NODE_CUSTOM_GROUP)) {
    return true;
  }
  if (node.type_legacy < NODE_LEGACY_TYPE_GENERATION_START) {
    /* Check known built-in types. */
    static Set<int> known_types = get_known_node_types_set();
    return known_types.contains(node.type_legacy);
  }
  /* Nodes with larger legacy_type are only identified by their idname. */
  return node_type_find(node.idname) != nullptr;
}

static void node_replace_undefined_types(bNode *node)
{
  /* If the node type is built-in but unknown, the node cannot be read. */
  if (!can_read_node_type(*node)) {
    node->type_legacy = NODE_CUSTOM;
    /* This type name is arbitrary, it just has to be unique enough to not match a future node
     * idname. Includes the old type identifier for debugging purposes. */
    const std::string old_idname = node->idname;
    SNPRINTF_UTF8(node->idname, "Undefined[%s]", old_idname.c_str());
    node->typeinfo = &NodeTypeUndefined;
  }
}

void node_tree_update_all_new(Main &main)
{
  /* Replace unknown node types with "Undefined".
   * This happens when loading files from newer Blender versions. Such nodes cannot be read
   * reliably so replace the idname with an undefined type. This keeps links and socket names but
   * discards storage and other type-specific data.
   *
   * Replacement has to happen after after-liblink-versioning, since some node types still get
   * replaced in those late versioning steps. */
  FOREACH_NODETREE_BEGIN (&main, ntree, owner_id) {
    for (bNode *node : ntree->all_nodes()) {
      node_replace_undefined_types(node);
    }
  }
  FOREACH_NODETREE_END;
  /* Update all new node trees on file read or append, to add/remove sockets
   * in groups nodes if the group changed, and handle any update flags that
   * might have been set in file reading or versioning. */
  FOREACH_NODETREE_BEGIN (&main, ntree, owner_id) {
    if (owner_id->tag & ID_TAG_NEW) {
      BKE_ntree_update_tag_all(ntree);
    }
  }
  FOREACH_NODETREE_END;
  BKE_ntree_update(main);
}

void node_tree_update_all_users(Main *main, ID *id)
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
    BKE_ntree_update(*main);
  }
}

/* ************* node type access ********** */

std::string node_label(const bNodeTree &ntree, const bNode &node)
{
  if (node.label[0] != '\0') {
    return node.label;
  }

  if (node.typeinfo->labelfunc) {
    char label_buffer[MAX_NAME];
    node.typeinfo->labelfunc(&ntree, &node, label_buffer, MAX_NAME);
    return label_buffer;
  }

  return IFACE_(node.typeinfo->ui_name);
}

std::optional<StringRefNull> node_socket_short_label(const bNodeSocket &sock)
{
  if (sock.runtime->declaration != nullptr) {
    StringRefNull short_label = sock.runtime->declaration->short_label;
    if (!short_label.is_empty()) {
      return sock.runtime->declaration->short_label.data();
    }
  }
  return std::nullopt;
}

StringRefNull node_socket_label(const bNodeSocket &sock)
{
  /* The node is not explicitly defined. */
  if (sock.runtime->declaration == nullptr) {
    return (sock.label[0] != '\0') ? sock.label : sock.name;
  }
  if (sock.runtime->declaration->label_fn) {
    return (*sock.runtime->declaration->label_fn)(sock.owner_node());
  }
  return sock.name;
}

const char *node_socket_translation_context(const bNodeSocket &sock)
{
  /* The node is not explicitly defined. */
  if (sock.runtime->declaration == nullptr) {
    return nullptr;
  }

  const std::optional<std::string> &translation_context =
      sock.runtime->declaration->translation_context;

  /* Default context. */
  if (!translation_context.has_value()) {
    return nullptr;
  }

  return translation_context->c_str();
}

NodeColorTag node_color_tag(const bNode &node)
{
  const int nclass = node.typeinfo->ui_class == nullptr ? node.typeinfo->nclass :
                                                          node.typeinfo->ui_class(&node);
  switch (nclass) {
    case NODE_CLASS_INPUT:
      return NodeColorTag::Input;
    case NODE_CLASS_OUTPUT:
      return NodeColorTag::Output;
    case NODE_CLASS_OP_COLOR:
      return NodeColorTag::Color;
    case NODE_CLASS_OP_VECTOR:
      return NodeColorTag::Vector;
    case NODE_CLASS_OP_FILTER:
      return NodeColorTag::Filter;
    case NODE_CLASS_CONVERTER:
      return NodeColorTag::Converter;
    case NODE_CLASS_MATTE:
      return NodeColorTag::Matte;
    case NODE_CLASS_DISTORT:
      return NodeColorTag::Distort;
    case NODE_CLASS_PATTERN:
      return NodeColorTag::Pattern;
    case NODE_CLASS_TEXTURE:
      return NodeColorTag::Texture;
    case NODE_CLASS_SCRIPT:
      return NodeColorTag::Script;
    case NODE_CLASS_INTERFACE:
      return NodeColorTag::Interface;
    case NODE_CLASS_SHADER:
      return NodeColorTag::Shader;
    case NODE_CLASS_GEOMETRY:
      return NodeColorTag::Geometry;
    case NODE_CLASS_ATTRIBUTE:
      return NodeColorTag::Attribute;
    case NODE_CLASS_GROUP:
      return NodeColorTag::Group;
    case NODE_CLASS_LAYOUT:
      break;
  }
  return NodeColorTag::None;
}

static void node_type_base_defaults(bNodeType &ntype)
{
  /* default size values */
  node_type_size_preset(ntype, eNodeSizePreset::Default);
  ntype.height = 100;
  ntype.minheight = 30;
  ntype.maxheight = FLT_MAX;
}

/* allow this node for any tree type */
static bool node_poll_default(const bNodeType * /*ntype*/,
                              const bNodeTree * /*ntree*/,
                              const char ** /*r_disabled_hint*/)
{
  return true;
}

static bool node_poll_instance_default(const bNode *node,
                                       const bNodeTree *ntree,
                                       const char **r_disabled_hint)
{
  return node->typeinfo->poll(node->typeinfo, ntree, r_disabled_hint);
}

static int16_t get_next_auto_legacy_type()
{
  static std::atomic<int> next_legacy_type = []() {
    /* Randomize the value a bit to avoid accidentally depending on the generated legacy type to be
     * stable across Blender sessions. */
    RandomNumberGenerator rng = RandomNumberGenerator::from_random_seed();
    return NODE_LEGACY_TYPE_GENERATION_START + rng.get_int32(100);
  }();
  const int new_type = next_legacy_type.fetch_add(1);
  BLI_assert(new_type <= std::numeric_limits<int16_t>::max());
  return new_type;
}

void node_type_base(bNodeType &ntype, std::string idname, std::optional<int16_t> legacy_type)
{
  ntype.idname = std::move(idname);

  if (!legacy_type.has_value()) {
    /* Still auto-generate a legacy type for this node type if none was specified. This is
     * necessary because some code checks if two nodes are the same type by comparing their legacy
     * types. The exact value does not matter, but it must be unique. */
    legacy_type = get_next_auto_legacy_type();
  }

  if (!ELEM(*legacy_type, NODE_CUSTOM, NODE_UNDEFINED)) {
    StructRNA *srna = RNA_struct_find(ntype.idname.c_str());
    BLI_assert(srna != nullptr);
    ntype.rna_ext.srna = srna;
    RNA_struct_blender_type_set(srna, &ntype);
  }

  /* make sure we have a valid type (everything registered) */
  BLI_assert(ntype.idname[0] != '\0');

  ntype.type_legacy = *legacy_type;
  ntype.nclass = NODE_CLASS_CONVERTER;

  node_type_base_defaults(ntype);

  ntype.poll = node_poll_default;
  ntype.poll_instance = node_poll_instance_default;
}

void node_type_base_custom(bNodeType &ntype,
                           const StringRefNull idname,
                           const StringRefNull name,
                           const StringRefNull enum_name,
                           const short nclass)
{
  ntype.idname = idname;
  ntype.type_legacy = NODE_CUSTOM;
  ntype.ui_name = name;
  ntype.nclass = nclass;
  ntype.enum_name_legacy = enum_name.c_str();

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
  const bNodeSocketType *typeinfo = node_socket_type_find_static(type);
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
    case SOCK_BUNDLE:
      cpp_type = &CPPType::get<nodes::BundlePtr>();
      break;
    case SOCK_CLOSURE:
      cpp_type = &CPPType::get<nodes::ClosurePtr>();
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
  if (type.is<nodes::MenuValue>()) {
    return SOCK_MENU;
  }
  if (type.is<float4x4>()) {
    return SOCK_MATRIX;
  }
  if (type.is<std::string>()) {
    return SOCK_STRING;
  }
  if (type.is<nodes::BundlePtr>()) {
    return SOCK_BUNDLE;
  }
  if (type.is<nodes::ClosurePtr>()) {
    return SOCK_CLOSURE;
  }
  if (type.is<GeometrySet>()) {
    return SOCK_GEOMETRY;
  }
  if (type.is<Material *>()) {
    return SOCK_MATERIAL;
  }
  if (type.is<Tex *>()) {
    return SOCK_TEXTURE;
  }
  if (type.is<Object *>()) {
    return SOCK_OBJECT;
  }
  if (type.is<Collection *>()) {
    return SOCK_COLLECTION;
  }
  if (type.is<Image *>()) {
    return SOCK_IMAGE;
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

static void unique_socket_template_identifier(bNodeSocketTemplate *list,
                                              bNodeSocketTemplate *ntemp,
                                              const char defname[],
                                              const char delim)
{
  BLI_uniquename_cb(
      [&](const StringRef check_name) {
        for (bNodeSocketTemplate *ntemp_iter = list; ntemp_iter->type >= 0; ntemp_iter++) {
          if (ntemp_iter != ntemp) {
            if (ntemp_iter->identifier == check_name) {
              return true;
            }
          }
        }
        return false;
      },
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

void node_type_size(bNodeType &ntype, const int width, const int minwidth, const int maxwidth)
{
  ntype.width = width;
  ntype.minwidth = minwidth;
  if (maxwidth <= minwidth) {
    ntype.maxwidth = FLT_MAX;
  }
  else {
    ntype.maxwidth = maxwidth;
  }
}

void node_type_size_preset(bNodeType &ntype, const eNodeSizePreset size)
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

void node_type_storage(bNodeType &ntype,
                       const std::optional<StringRefNull> storagename,
                       void (*freefunc)(bNode *node),
                       void (*copyfunc)(bNodeTree *dest_ntree,
                                        bNode *dest_node,
                                        const bNode *src_node))
{
  ntype.storagename = storagename.value_or("");
  ntype.copyfunc = copyfunc;
  ntype.freefunc = freefunc;
}

void node_system_init()
{
  register_nodes();
}

void node_system_exit()
{
  get_node_type_alias_map().clear();

  const Vector<bNodeType *> node_types = get_node_type_map().extract_vector();
  for (bNodeType *nt : node_types) {
    if (nt->rna_ext.free) {
      nt->rna_ext.free(nt->rna_ext.data);
    }
    node_free_type(nt);
  }

  const Vector<bNodeSocketType *> socket_types = get_socket_type_map().extract_vector();
  for (bNodeSocketType *st : socket_types) {
    if (st->ext_socket.free) {
      st->ext_socket.free(st->ext_socket.data);
    }
    if (st->ext_interface.free) {
      st->ext_interface.free(st->ext_interface.data);
    }
    node_free_socket_type(st);
  }

  const Vector<bNodeTreeType *> tree_types = get_node_tree_type_map().extract_vector();
  for (bNodeTreeType *nt : tree_types) {
    if (nt->rna_ext.free) {
      nt->rna_ext.free(nt->rna_ext.data);
    }
    ntree_free_type(nt);
  }
}

/* -------------------------------------------------------------------- */
/* NodeTree Iterator Helpers (FOREACH_NODETREE_BEGIN) */

void node_tree_iterator_init(NodeTreeIterStore *ntreeiter, Main *bmain)
{
  ntreeiter->ngroup = (bNodeTree *)bmain->nodetrees.first;
  ntreeiter->scene = (Scene *)bmain->scenes.first;
  ntreeiter->mat = (Material *)bmain->materials.first;
  ntreeiter->tex = (Tex *)bmain->textures.first;
  ntreeiter->light = (Light *)bmain->lights.first;
  ntreeiter->world = (World *)bmain->worlds.first;
  ntreeiter->linestyle = (FreestyleLineStyle *)bmain->linestyles.first;
}
bool node_tree_iterator_step(NodeTreeIterStore *ntreeiter, bNodeTree **r_nodetree, ID **r_id)
{
  if (ntreeiter->ngroup) {
    bNodeTree &node_tree = *ntreeiter->ngroup;
    *r_nodetree = &node_tree;
    *r_id = &node_tree.id;
    ntreeiter->ngroup = reinterpret_cast<bNodeTree *>(node_tree.id.next);
    return true;
  }
  if (ntreeiter->scene) {
    /* Embedded compositing trees are deprecated, but still relevant for versioning/backward
     * compatibility. */
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->scene->nodetree);
    *r_id = &ntreeiter->scene->id;
    ntreeiter->scene = reinterpret_cast<Scene *>(ntreeiter->scene->id.next);
    return true;
  }
  if (ntreeiter->mat) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->mat->nodetree);
    *r_id = &ntreeiter->mat->id;
    ntreeiter->mat = reinterpret_cast<Material *>(ntreeiter->mat->id.next);
    return true;
  }
  if (ntreeiter->tex) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->tex->nodetree);
    *r_id = &ntreeiter->tex->id;
    ntreeiter->tex = reinterpret_cast<Tex *>(ntreeiter->tex->id.next);
    return true;
  }
  if (ntreeiter->light) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->light->nodetree);
    *r_id = &ntreeiter->light->id;
    ntreeiter->light = reinterpret_cast<Light *>(ntreeiter->light->id.next);
    return true;
  }
  if (ntreeiter->world) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->world->nodetree);
    *r_id = &ntreeiter->world->id;
    ntreeiter->world = reinterpret_cast<World *>(ntreeiter->world->id.next);
    return true;
  }
  if (ntreeiter->linestyle) {
    *r_nodetree = reinterpret_cast<bNodeTree *>(ntreeiter->linestyle->nodetree);
    *r_id = &ntreeiter->linestyle->id;
    ntreeiter->linestyle = reinterpret_cast<FreestyleLineStyle *>(ntreeiter->linestyle->id.next);
    return true;
  }

  return false;
}

void node_tree_remove_layer_n(bNodeTree *ntree, Scene *scene, const int layer_index)
{
  BLI_assert(layer_index != -1);
  BLI_assert(scene != nullptr);
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == CMP_NODE_R_LAYERS && node->id == &scene->id) {
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
