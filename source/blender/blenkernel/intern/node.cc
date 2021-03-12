/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_colortools.h"
#include "BKE_cryptomatte.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_ui_storage.hh"

#include "BLI_ghash.h"
#include "BLI_threads.h"
#include "RNA_access.h"
#include "RNA_define.h"

#include "NOD_common.h"
#include "NOD_composite.h"
#include "NOD_function.h"
#include "NOD_geometry.h"
#include "NOD_shader.h"
#include "NOD_socket.h"
#include "NOD_texture.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "BLO_read_write.h"

#include "MOD_nodes.h"

#define NODE_DEFAULT_MAX_WIDTH 700

/* Fallback types for undefined tree, nodes, sockets */
static bNodeTreeType NodeTreeTypeUndefined;
bNodeType NodeTypeUndefined;
bNodeSocketType NodeSocketTypeUndefined;

static CLG_LogRef LOG = {"bke.node"};

static void ntree_set_typeinfo(bNodeTree *ntree, bNodeTreeType *typeinfo);
static void node_socket_copy(bNodeSocket *sock_dst, const bNodeSocket *sock_src, const int flag);
static void free_localized_node_groups(bNodeTree *ntree);
static void node_free_node(bNodeTree *ntree, bNode *node);
static void node_socket_interface_free(bNodeTree *UNUSED(ntree),
                                       bNodeSocket *sock,
                                       const bool do_id_user);

static void ntree_init_data(ID *id)
{
  bNodeTree *ntree = (bNodeTree *)id;
  ntree_set_typeinfo(ntree, nullptr);
}

static void ntree_copy_data(Main *UNUSED(bmain), ID *id_dst, const ID *id_src, const int flag)
{
  bNodeTree *ntree_dst = (bNodeTree *)id_dst;
  const bNodeTree *ntree_src = (const bNodeTree *)id_src;

  /* We never handle usercount here for own data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  /* in case a running nodetree is copied */
  ntree_dst->execdata = nullptr;

  BLI_listbase_clear(&ntree_dst->nodes);
  BLI_listbase_clear(&ntree_dst->links);

  /* Since source nodes and sockets are unique pointers we can put everything in a single map. */
  GHash *new_pointers = BLI_ghash_ptr_new(__func__);

  LISTBASE_FOREACH (const bNode *, node_src, &ntree_src->nodes) {
    bNode *new_node = BKE_node_copy_ex(ntree_dst, node_src, flag_subdata, true);
    BLI_ghash_insert(new_pointers, (void *)node_src, new_node);
    /* Store mapping to inputs. */
    bNodeSocket *new_input_sock = (bNodeSocket *)new_node->inputs.first;
    const bNodeSocket *input_sock_src = (const bNodeSocket *)node_src->inputs.first;
    while (new_input_sock != nullptr) {
      BLI_ghash_insert(new_pointers, (void *)input_sock_src, new_input_sock);
      new_input_sock = new_input_sock->next;
      input_sock_src = input_sock_src->next;
    }
    /* Store mapping to outputs. */
    bNodeSocket *new_output_sock = (bNodeSocket *)new_node->outputs.first;
    const bNodeSocket *output_sock_src = (const bNodeSocket *)node_src->outputs.first;
    while (new_output_sock != nullptr) {
      BLI_ghash_insert(new_pointers, (void *)output_sock_src, new_output_sock);
      new_output_sock = new_output_sock->next;
      output_sock_src = output_sock_src->next;
    }
  }

  /* copy links */
  BLI_duplicatelist(&ntree_dst->links, &ntree_src->links);
  LISTBASE_FOREACH (bNodeLink *, link_dst, &ntree_dst->links) {
    link_dst->fromnode = (bNode *)BLI_ghash_lookup_default(
        new_pointers, link_dst->fromnode, nullptr);
    link_dst->fromsock = (bNodeSocket *)BLI_ghash_lookup_default(
        new_pointers, link_dst->fromsock, nullptr);
    link_dst->tonode = (bNode *)BLI_ghash_lookup_default(new_pointers, link_dst->tonode, nullptr);
    link_dst->tosock = (bNodeSocket *)BLI_ghash_lookup_default(
        new_pointers, link_dst->tosock, nullptr);
    /* update the link socket's pointer */
    if (link_dst->tosock) {
      link_dst->tosock->link = link_dst;
    }
  }

  /* copy interface sockets */
  BLI_duplicatelist(&ntree_dst->inputs, &ntree_src->inputs);
  bNodeSocket *sock_dst, *sock_src;
  for (sock_dst = (bNodeSocket *)ntree_dst->inputs.first,
      sock_src = (bNodeSocket *)ntree_src->inputs.first;
       sock_dst != nullptr;
       sock_dst = (bNodeSocket *)sock_dst->next, sock_src = (bNodeSocket *)sock_src->next) {
    node_socket_copy(sock_dst, sock_src, flag_subdata);
  }

  BLI_duplicatelist(&ntree_dst->outputs, &ntree_src->outputs);
  for (sock_dst = (bNodeSocket *)ntree_dst->outputs.first,
      sock_src = (bNodeSocket *)ntree_src->outputs.first;
       sock_dst != nullptr;
       sock_dst = (bNodeSocket *)sock_dst->next, sock_src = (bNodeSocket *)sock_src->next) {
    node_socket_copy(sock_dst, sock_src, flag_subdata);
  }

  /* copy preview hash */
  if (ntree_src->previews && (flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    bNodeInstanceHashIterator iter;

    ntree_dst->previews = BKE_node_instance_hash_new("node previews");

    NODE_INSTANCE_HASH_ITER (iter, ntree_src->previews) {
      bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
      bNodePreview *preview = (bNodePreview *)BKE_node_instance_hash_iterator_get_value(&iter);
      BKE_node_instance_hash_insert(ntree_dst->previews, key, BKE_node_preview_copy(preview));
    }
  }
  else {
    ntree_dst->previews = nullptr;
  }

  /* update node->parent pointers */
  for (bNode *node_dst = (bNode *)ntree_dst->nodes.first,
             *node_src = (bNode *)ntree_src->nodes.first;
       node_dst;
       node_dst = (bNode *)node_dst->next, node_src = (bNode *)node_src->next) {
    if (node_dst->parent) {
      node_dst->parent = (bNode *)BLI_ghash_lookup_default(
          new_pointers, node_dst->parent, nullptr);
    }
  }

  BLI_ghash_free(new_pointers, nullptr, nullptr);

  /* node tree will generate its own interface type */
  ntree_dst->interface_type = nullptr;

  /* Don't copy error messages in the runtime struct.
   * They should be filled during execution anyway. */
  ntree_dst->ui_storage = nullptr;
}

static void ntree_free_data(ID *id)
{
  bNodeTree *ntree = (bNodeTree *)id;

  /* XXX hack! node trees should not store execution graphs at all.
   * This should be removed when old tree types no longer require it.
   * Currently the execution data for texture nodes remains in the tree
   * after execution, until the node tree is updated or freed.
   */
  if (ntree->execdata) {
    switch (ntree->type) {
      case NTREE_SHADER:
        ntreeShaderEndExecTree(ntree->execdata);
        break;
      case NTREE_TEXTURE:
        ntreeTexEndExecTree(ntree->execdata);
        ntree->execdata = nullptr;
        break;
    }
  }

  /* XXX not nice, but needed to free localized node groups properly */
  free_localized_node_groups(ntree);

  /* unregister associated RNA types */
  ntreeInterfaceTypeFree(ntree);

  BLI_freelistN(&ntree->links); /* do first, then unlink_node goes fast */

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    node_free_node(ntree, node);
  }

  /* free interface sockets */
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &ntree->inputs) {
    node_socket_interface_free(ntree, sock, false);
    MEM_freeN(sock);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &ntree->outputs) {
    node_socket_interface_free(ntree, sock, false);
    MEM_freeN(sock);
  }

  /* free preview hash */
  if (ntree->previews) {
    BKE_node_instance_hash_free(ntree->previews, (bNodeInstanceValueFP)BKE_node_preview_free);
  }

  if (ntree->id.tag & LIB_TAG_LOCALIZED) {
    BKE_libblock_free_data(&ntree->id, true);
  }

  delete ntree->ui_storage;
}

static void library_foreach_node_socket(LibraryForeachIDData *data, bNodeSocket *sock)
{
  IDP_foreach_property(
      sock->prop, IDP_TYPE_FILTER_ID, BKE_lib_query_idpropertiesForeachIDLink_callback, data);

  switch ((eNodeSocketDatatype)sock->type) {
    case SOCK_OBJECT: {
      bNodeSocketValueObject *default_value = (bNodeSocketValueObject *)sock->default_value;
      BKE_LIB_FOREACHID_PROCESS(data, default_value->value, IDWALK_CB_USER);
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *default_value = (bNodeSocketValueImage *)sock->default_value;
      BKE_LIB_FOREACHID_PROCESS(data, default_value->value, IDWALK_CB_USER);
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *default_value = (bNodeSocketValueCollection *)
                                                      sock->default_value;
      BKE_LIB_FOREACHID_PROCESS(data, default_value->value, IDWALK_CB_USER);
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_INT:
    case SOCK_STRING:
    case __SOCK_MESH:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      break;
  }
}

static void node_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bNodeTree *ntree = (bNodeTree *)id;

  BKE_LIB_FOREACHID_PROCESS(data, ntree->gpd, IDWALK_CB_USER);

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    BKE_LIB_FOREACHID_PROCESS_ID(data, node->id, IDWALK_CB_USER);

    IDP_foreach_property(
        node->prop, IDP_TYPE_FILTER_ID, BKE_lib_query_idpropertiesForeachIDLink_callback, data);
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      library_foreach_node_socket(data, sock);
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      library_foreach_node_socket(data, sock);
    }
  }

  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->inputs) {
    library_foreach_node_socket(data, sock);
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->outputs) {
    library_foreach_node_socket(data, sock);
  }
}

static void node_foreach_cache(ID *id,
                               IDTypeForeachCacheFunctionCallback function_callback,
                               void *user_data)
{
  bNodeTree *nodetree = (bNodeTree *)id;
  IDCacheKey key = {0};
  key.id_session_uuid = id->session_uuid;
  key.offset_in_ID = offsetof(bNodeTree, previews);
  key.cache_v = nodetree->previews;

  /* TODO, see also `direct_link_nodetree()` in readfile.c. */
#if 0
  function_callback(id, &key, (void **)&nodetree->previews, 0, user_data);
#endif

  if (nodetree->type == NTREE_COMPOSIT) {
    LISTBASE_FOREACH (bNode *, node, &nodetree->nodes) {
      if (node->type == CMP_NODE_MOVIEDISTORTION) {
        key.offset_in_ID = (size_t)BLI_ghashutil_strhash_p(node->name);
        key.cache_v = node->storage;
        function_callback(id, &key, (void **)&node->storage, 0, user_data);
      }
    }
  }
}

static ID *node_owner_get(Main *bmain, ID *id)
{
  if ((id->flag & LIB_EMBEDDED_DATA) == 0) {
    return id;
  }
  /* TODO: Sort this NO_MAIN or not for embedded node trees. See T86119. */
  // BLI_assert((id->tag & LIB_TAG_NO_MAIN) == 0);

  ListBase *lists[] = {&bmain->materials,
                       &bmain->lights,
                       &bmain->worlds,
                       &bmain->textures,
                       &bmain->scenes,
                       &bmain->linestyles,
                       &bmain->simulations,
                       nullptr};

  bNodeTree *ntree = (bNodeTree *)id;
  for (int i = 0; lists[i] != nullptr; i++) {
    LISTBASE_FOREACH (ID *, id_iter, lists[i]) {
      if (ntreeFromID(id_iter) == ntree) {
        return id_iter;
      }
    }
  }

  BLI_assert(!"Embedded node tree with no owner. Critical Main inconsistency.");
  return nullptr;
}

static void write_node_socket_default_value(BlendWriter *writer, bNodeSocket *sock)
{
  if (sock->default_value == nullptr) {
    return;
  }

  switch ((eNodeSocketDatatype)sock->type) {
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
    case __SOCK_MESH:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      BLI_assert(false);
      break;
  }
}

static void write_node_socket(BlendWriter *writer, bNodeSocket *sock)
{
  /* actual socket writing */
  BLO_write_struct(writer, bNodeSocket, sock);

  if (sock->prop) {
    IDP_BlendWrite(writer, sock->prop);
  }

  write_node_socket_default_value(writer, sock);
}
static void write_node_socket_interface(BlendWriter *writer, bNodeSocket *sock)
{
  /* actual socket writing */
  BLO_write_struct(writer, bNodeSocket, sock);

  if (sock->prop) {
    IDP_BlendWrite(writer, sock->prop);
  }

  write_node_socket_default_value(writer, sock);
}

/* this is only direct data, tree itself should have been written */
void ntreeBlendWrite(BlendWriter *writer, bNodeTree *ntree)
{
  BKE_id_blend_write(writer, &ntree->id);

  /* for link_list() speed, we write per list */

  if (ntree->adt) {
    BKE_animdata_blend_write(writer, ntree->adt);
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
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

    LISTBASE_FOREACH (bNodeLink *, link, &node->internal_links) {
      BLO_write_struct(writer, bNodeLink, link);
    }

    if (node->storage) {
      /* could be handlerized at some point, now only 1 exception still */
      if ((ntree->type == NTREE_SHADER) &&
          ELEM(node->type, SH_NODE_CURVE_VEC, SH_NODE_CURVE_RGB)) {
        BKE_curvemapping_blend_write(writer, (const CurveMapping *)node->storage);
      }
      else if (ntree->type == NTREE_SHADER && (node->type == SH_NODE_SCRIPT)) {
        NodeShaderScript *nss = (NodeShaderScript *)node->storage;
        if (nss->bytecode) {
          BLO_write_string(writer, nss->bytecode);
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) && ELEM(node->type,
                                                       CMP_NODE_TIME,
                                                       CMP_NODE_CURVE_VEC,
                                                       CMP_NODE_CURVE_RGB,
                                                       CMP_NODE_HUECORRECT)) {
        BKE_curvemapping_blend_write(writer, (const CurveMapping *)node->storage);
      }
      else if ((ntree->type == NTREE_TEXTURE) &&
               (node->type == TEX_NODE_CURVE_RGB || node->type == TEX_NODE_CURVE_TIME)) {
        BKE_curvemapping_blend_write(writer, (const CurveMapping *)node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_MOVIEDISTORTION)) {
        /* pass */
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_GLARE)) {
        /* Simple forward compatibility for fix for T50736.
         * Not ideal (there is no ideal solution here), but should do for now. */
        NodeGlare *ndg = (NodeGlare *)node->storage;
        /* Not in undo case. */
        if (!BLO_write_is_undo(writer)) {
          switch (ndg->type) {
            case 2: /* Grrrr! magic numbers :( */
              ndg->angle = ndg->streaks;
              break;
            case 0:
              ndg->angle = ndg->star_45;
              break;
            default:
              break;
          }
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
      else if ((ntree->type == NTREE_COMPOSIT) && (node->type == CMP_NODE_CRYPTOMATTE)) {
        NodeCryptomatte *nc = (NodeCryptomatte *)node->storage;
        BLO_write_string(writer, nc->matte_id);
        LISTBASE_FOREACH (CryptomatteEntry *, entry, &nc->entries) {
          BLO_write_struct(writer, CryptomatteEntry, entry);
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
      else if (node->type == FN_NODE_INPUT_STRING) {
        NodeInputString *storage = (NodeInputString *)node->storage;
        if (storage->string) {
          BLO_write_string(writer, storage->string);
        }
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, storage);
      }
      else if (node->typeinfo != &NodeTypeUndefined) {
        BLO_write_struct_by_name(writer, node->typeinfo->storagename, node->storage);
      }
    }

    if (node->type == CMP_NODE_OUTPUT_FILE) {
      /* inputs have own storage data */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        BLO_write_struct(writer, NodeImageMultiFileSocket, sock->storage);
      }
    }
    if (ELEM(node->type, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS)) {
      /* write extra socket info */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        BLO_write_struct(writer, NodeImageLayer, sock->storage);
      }
    }
  }

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    BLO_write_struct(writer, bNodeLink, link);
  }

  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->inputs) {
    write_node_socket_interface(writer, sock);
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->outputs) {
    write_node_socket_interface(writer, sock);
  }
}

static void ntree_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bNodeTree *ntree = (bNodeTree *)id;
  if (ntree->id.us > 0 || BLO_write_is_undo(writer)) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    ntree->init = 0; /* to set callbacks and force setting types */
    ntree->is_updating = false;
    ntree->typeinfo = nullptr;
    ntree->interface_type = nullptr;
    ntree->progress = nullptr;
    ntree->execdata = nullptr;
    ntree->ui_storage = nullptr;

    BLO_write_id_struct(writer, bNodeTree, id_address, &ntree->id);

    ntreeBlendWrite(writer, ntree);
  }
}

static void direct_link_node_socket(BlendDataReader *reader, bNodeSocket *sock)
{
  BLO_read_data_address(reader, &sock->prop);
  IDP_BlendDataRead(reader, &sock->prop);

  BLO_read_data_address(reader, &sock->link);
  sock->typeinfo = nullptr;
  BLO_read_data_address(reader, &sock->storage);
  BLO_read_data_address(reader, &sock->default_value);
  sock->total_inputs = 0; /* Clear runtime data set before drawing. */
  sock->cache = nullptr;
}

/* ntree itself has been read! */
void ntreeBlendReadData(BlendDataReader *reader, bNodeTree *ntree)
{
  /* note: writing and reading goes in sync, for speed */
  ntree->init = 0; /* to set callbacks and force setting types */
  ntree->is_updating = false;
  ntree->typeinfo = nullptr;
  ntree->interface_type = nullptr;

  ntree->progress = nullptr;
  ntree->execdata = nullptr;
  ntree->ui_storage = nullptr;

  BLO_read_data_address(reader, &ntree->adt);
  BKE_animdata_blend_read_data(reader, ntree->adt);

  BLO_read_list(reader, &ntree->nodes);
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->typeinfo = nullptr;

    BLO_read_list(reader, &node->inputs);
    BLO_read_list(reader, &node->outputs);

    BLO_read_data_address(reader, &node->prop);
    IDP_BlendDataRead(reader, &node->prop);

    BLO_read_list(reader, &node->internal_links);
    LISTBASE_FOREACH (bNodeLink *, link, &node->internal_links) {
      BLO_read_data_address(reader, &link->fromnode);
      BLO_read_data_address(reader, &link->fromsock);
      BLO_read_data_address(reader, &link->tonode);
      BLO_read_data_address(reader, &link->tosock);
    }

    if (node->type == CMP_NODE_MOVIEDISTORTION) {
      /* Do nothing, this is runtime cache and hence handled by generic code using
       * `IDTypeInfo.foreach_cache` callback. */
    }
    else {
      BLO_read_data_address(reader, &node->storage);
    }

    if (node->storage) {
      /* could be handlerized at some point */
      switch (node->type) {
        case SH_NODE_CURVE_VEC:
        case SH_NODE_CURVE_RGB:
        case CMP_NODE_TIME:
        case CMP_NODE_CURVE_VEC:
        case CMP_NODE_CURVE_RGB:
        case CMP_NODE_HUECORRECT:
        case TEX_NODE_CURVE_RGB:
        case TEX_NODE_CURVE_TIME: {
          BKE_curvemapping_blend_read(reader, (CurveMapping *)node->storage);
          break;
        }
        case SH_NODE_SCRIPT: {
          NodeShaderScript *nss = (NodeShaderScript *)node->storage;
          BLO_read_data_address(reader, &nss->bytecode);
          break;
        }
        case SH_NODE_TEX_POINTDENSITY: {
          NodeShaderTexPointDensity *npd = (NodeShaderTexPointDensity *)node->storage;
          memset(&npd->pd, 0, sizeof(npd->pd));
          break;
        }
        case SH_NODE_TEX_IMAGE: {
          NodeTexImage *tex = (NodeTexImage *)node->storage;
          tex->iuser.ok = 1;
          tex->iuser.scene = nullptr;
          break;
        }
        case SH_NODE_TEX_ENVIRONMENT: {
          NodeTexEnvironment *tex = (NodeTexEnvironment *)node->storage;
          tex->iuser.ok = 1;
          tex->iuser.scene = nullptr;
          break;
        }
        case CMP_NODE_IMAGE:
        case CMP_NODE_R_LAYERS:
        case CMP_NODE_VIEWER:
        case CMP_NODE_SPLITVIEWER: {
          ImageUser *iuser = (ImageUser *)node->storage;
          iuser->ok = 1;
          iuser->scene = nullptr;
          break;
        }
        case CMP_NODE_CRYPTOMATTE: {
          NodeCryptomatte *nc = (NodeCryptomatte *)node->storage;
          BLO_read_data_address(reader, &nc->matte_id);
          BLO_read_list(reader, &nc->entries);
          break;
        }
        case TEX_NODE_IMAGE: {
          ImageUser *iuser = (ImageUser *)node->storage;
          iuser->ok = 1;
          iuser->scene = nullptr;
          break;
        }
        case FN_NODE_INPUT_STRING: {
          NodeInputString *storage = (NodeInputString *)node->storage;
          BLO_read_data_address(reader, &storage->string);
          break;
        }
        default:
          break;
      }
    }
  }
  BLO_read_list(reader, &ntree->links);

  /* and we connect the rest */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    BLO_read_data_address(reader, &node->parent);

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      direct_link_node_socket(reader, sock);
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      direct_link_node_socket(reader, sock);
    }
  }

  /* interface socket lists */
  BLO_read_list(reader, &ntree->inputs);
  BLO_read_list(reader, &ntree->outputs);
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->inputs) {
    direct_link_node_socket(reader, sock);
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->outputs) {
    direct_link_node_socket(reader, sock);
  }

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    BLO_read_data_address(reader, &link->fromnode);
    BLO_read_data_address(reader, &link->tonode);
    BLO_read_data_address(reader, &link->fromsock);
    BLO_read_data_address(reader, &link->tosock);
  }

  /* TODO, should be dealt by new generic cache handling of IDs... */
  ntree->previews = nullptr;

  /* type verification is in lib-link */
}

static void ntree_blend_read_data(BlendDataReader *reader, ID *id)
{
  bNodeTree *ntree = (bNodeTree *)id;
  ntreeBlendReadData(reader, ntree);
}

static void lib_link_node_socket(BlendLibReader *reader, Library *lib, bNodeSocket *sock)
{
  IDP_BlendReadLib(reader, sock->prop);

  switch ((eNodeSocketDatatype)sock->type) {
    case SOCK_OBJECT: {
      bNodeSocketValueObject *default_value = (bNodeSocketValueObject *)sock->default_value;
      BLO_read_id_address(reader, lib, &default_value->value);
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *default_value = (bNodeSocketValueImage *)sock->default_value;
      BLO_read_id_address(reader, lib, &default_value->value);
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *default_value = (bNodeSocketValueCollection *)
                                                      sock->default_value;
      BLO_read_id_address(reader, lib, &default_value->value);
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_INT:
    case SOCK_STRING:
    case __SOCK_MESH:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      break;
  }
}

static void lib_link_node_sockets(BlendLibReader *reader, Library *lib, ListBase *sockets)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    lib_link_node_socket(reader, lib, sock);
  }
}

void ntreeBlendReadLib(struct BlendLibReader *reader, struct bNodeTree *ntree)
{
  Library *lib = ntree->id.lib;

  BLO_read_id_address(reader, lib, &ntree->gpd);

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    /* Link ID Properties -- and copy this comment EXACTLY for easy finding
     * of library blocks that implement this.*/
    IDP_BlendReadLib(reader, node->prop);

    BLO_read_id_address(reader, lib, &node->id);

    lib_link_node_sockets(reader, lib, &node->inputs);
    lib_link_node_sockets(reader, lib, &node->outputs);
  }

  lib_link_node_sockets(reader, lib, &ntree->inputs);
  lib_link_node_sockets(reader, lib, &ntree->outputs);

  /* Set node->typeinfo pointers. This is done in lib linking, after the
   * first versioning that can change types still without functions that
   * update the typeinfo pointers. Versioning after lib linking needs
   * these top be valid. */
  ntreeSetTypes(nullptr, ntree);

  /* For nodes with static socket layout, add/remove sockets as needed
   * to match the static layout. */
  if (!BLO_read_lib_is_undo(reader)) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      node_verify_socket_templates(ntree, node);
    }
  }
}

static void ntree_blend_read_lib(BlendLibReader *reader, ID *id)
{
  bNodeTree *ntree = (bNodeTree *)id;
  ntreeBlendReadLib(reader, ntree);
}

static void expand_node_socket(BlendExpander *expander, bNodeSocket *sock)
{
  IDP_BlendReadExpand(expander, sock->prop);

  if (sock->default_value != nullptr) {

    switch ((eNodeSocketDatatype)sock->type) {
      case SOCK_OBJECT: {
        bNodeSocketValueObject *default_value = (bNodeSocketValueObject *)sock->default_value;
        BLO_expand(expander, default_value->value);
        break;
      }
      case SOCK_IMAGE: {
        bNodeSocketValueImage *default_value = (bNodeSocketValueImage *)sock->default_value;
        BLO_expand(expander, default_value->value);
        break;
      }
      case SOCK_COLLECTION: {
        bNodeSocketValueCollection *default_value = (bNodeSocketValueCollection *)
                                                        sock->default_value;
        BLO_expand(expander, default_value->value);
        break;
      }
      case SOCK_FLOAT:
      case SOCK_VECTOR:
      case SOCK_RGBA:
      case SOCK_BOOLEAN:
      case SOCK_INT:
      case SOCK_STRING:
      case __SOCK_MESH:
      case SOCK_CUSTOM:
      case SOCK_SHADER:
      case SOCK_GEOMETRY:
        break;
    }
  }
}

static void expand_node_sockets(BlendExpander *expander, ListBase *sockets)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    expand_node_socket(expander, sock);
  }
}

void ntreeBlendReadExpand(BlendExpander *expander, bNodeTree *ntree)
{
  if (ntree->gpd) {
    BLO_expand(expander, ntree->gpd);
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->id && node->type != CMP_NODE_R_LAYERS) {
      BLO_expand(expander, node->id);
    }

    IDP_BlendReadExpand(expander, node->prop);

    expand_node_sockets(expander, &node->inputs);
    expand_node_sockets(expander, &node->outputs);
  }

  expand_node_sockets(expander, &ntree->inputs);
  expand_node_sockets(expander, &ntree->outputs);
}

static void ntree_blend_read_expand(BlendExpander *expander, ID *id)
{
  bNodeTree *ntree = (bNodeTree *)id;
  ntreeBlendReadExpand(expander, ntree);
}

IDTypeInfo IDType_ID_NT = {
    /* id_code */ ID_NT,
    /* id_filter */ FILTER_ID_NT,
    /* main_listbase_index */ INDEX_ID_NT,
    /* struct_size */ sizeof(bNodeTree),
    /* name */ "NodeTree",
    /* name_plural */ "node_groups",
    /* translation_context */ BLT_I18NCONTEXT_ID_NODETREE,
    /* flags */ 0,

    /* init_data */ ntree_init_data,
    /* copy_data */ ntree_copy_data,
    /* free_data */ ntree_free_data,
    /* make_local */ nullptr,
    /* foreach_id */ node_foreach_id,
    /* foreach_cache */ node_foreach_cache,
    /* owner_get */ node_owner_get,

    /* blend_write */ ntree_blend_write,
    /* blend_read_data */ ntree_blend_read_data,
    /* blend_read_lib */ ntree_blend_read_lib,
    /* blend_read_expand */ ntree_blend_read_expand,

    /* blend_read_undo_preserve */ nullptr,

    /* lib_override_apply_post */ nullptr,
};

static void node_add_sockets_from_type(bNodeTree *ntree, bNode *node, bNodeType *ntype)
{
  bNodeSocketTemplate *sockdef;
  /* bNodeSocket *sock; */ /* UNUSED */

  if (ntype->inputs) {
    sockdef = ntype->inputs;
    while (sockdef->type != -1) {
      /* sock = */ node_add_socket_from_template(ntree, node, sockdef, SOCK_IN);

      sockdef++;
    }
  }
  if (ntype->outputs) {
    sockdef = ntype->outputs;
    while (sockdef->type != -1) {
      /* sock = */ node_add_socket_from_template(ntree, node, sockdef, SOCK_OUT);

      sockdef++;
    }
  }
}

/* Note: This function is called to initialize node data based on the type.
 * The bNodeType may not be registered at creation time of the node,
 * so this can be delayed until the node type gets registered.
 */
static void node_init(const struct bContext *C, bNodeTree *ntree, bNode *node)
{
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
  node->miniwidth = 42.0f;
  node->height = ntype->height;
  node->color[0] = node->color[1] = node->color[2] = 0.608; /* default theme color */
  /* initialize the node name with the node label.
   * note: do this after the initfunc so nodes get their data set which may be used in naming
   * (node groups for example) */
  /* XXX Do not use nodeLabel() here, it returns translated content for UI,
   *     which should *only* be used in UI, *never* in data...
   *     Data have their own translation option!
   *     This solution may be a bit rougher than nodeLabel()'s returned string, but it's simpler
   *     than adding "do_translate" flags to this func (and labelfunc() as well). */
  BLI_strncpy(node->name, DATA_(ntype->ui_name), NODE_MAXSTR);
  nodeUniqueName(ntree, node);

  node_add_sockets_from_type(ntree, node, ntype);

  if (ntype->initfunc != nullptr) {
    ntype->initfunc(ntree, node);
  }

  if (ntree->typeinfo->node_add_init != nullptr) {
    ntree->typeinfo->node_add_init(ntree, node);
  }

  if (node->id) {
    id_us_plus(node->id);
  }

  /* extra init callback */
  if (ntype->initfunc_api) {
    PointerRNA ptr;
    RNA_pointer_create((ID *)ntree, &RNA_Node, node, &ptr);

    /* XXX Warning: context can be nullptr in case nodes are added in do_versions.
     * Delayed init is not supported for nodes with context-based initfunc_api atm.
     */
    BLI_assert(C != nullptr);
    ntype->initfunc_api(C, &ptr);
  }

  node->flag |= NODE_INIT;
}

static void ntree_set_typeinfo(bNodeTree *ntree, bNodeTreeType *typeinfo)
{
  if (typeinfo) {
    ntree->typeinfo = typeinfo;

    /* deprecated integer type */
    ntree->type = typeinfo->type;
  }
  else {
    ntree->typeinfo = &NodeTreeTypeUndefined;

    ntree->init &= ~NTREE_TYPE_INIT;
  }
}

static void node_set_typeinfo(const struct bContext *C,
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

    ntree->init &= ~NTREE_TYPE_INIT;
  }
}

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

    ntree->init &= ~NTREE_TYPE_INIT;
  }
}

/* Set specific typeinfo pointers in all node trees on register/unregister */
static void update_typeinfo(Main *bmain,
                            const struct bContext *C,
                            bNodeTreeType *treetype,
                            bNodeType *nodetype,
                            bNodeSocketType *socktype,
                            bool unregister)
{
  if (!bmain) {
    return;
  }

  FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
    ntree->init |= NTREE_TYPE_INIT;

    if (treetype && STREQ(ntree->idname, treetype->idname)) {
      ntree_set_typeinfo(ntree, unregister ? nullptr : treetype);
    }

    /* initialize nodes */
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
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

    /* initialize tree sockets */
    LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->inputs) {
      if (socktype && STREQ(sock->idname, socktype->idname)) {
        node_socket_set_typeinfo(ntree, sock, unregister ? nullptr : socktype);
      }
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->outputs) {
      if (socktype && STREQ(sock->idname, socktype->idname)) {
        node_socket_set_typeinfo(ntree, sock, unregister ? nullptr : socktype);
      }
    }
  }
  FOREACH_NODETREE_END;
}

/* Try to initialize all typeinfo in a node tree.
 * NB: In general undefined typeinfo is a perfectly valid case,
 * the type may just be registered later.
 * In that case the update_typeinfo function will set typeinfo on registration
 * and do necessary updates.
 */
void ntreeSetTypes(const struct bContext *C, bNodeTree *ntree)
{
  ntree->init |= NTREE_TYPE_INIT;

  ntree_set_typeinfo(ntree, ntreeTypeFind(ntree->idname));

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node_set_typeinfo(C, ntree, node, nodeTypeFind(node->idname));

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
    }
  }

  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->inputs) {
    node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->outputs) {
    node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(sock->idname));
  }
}

static GHash *nodetreetypes_hash = nullptr;
static GHash *nodetypes_hash = nullptr;
static GHash *nodesockettypes_hash = nullptr;

bNodeTreeType *ntreeTypeFind(const char *idname)
{
  if (idname[0]) {
    bNodeTreeType *nt = (bNodeTreeType *)BLI_ghash_lookup(nodetreetypes_hash, idname);
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

/* callback for hash value free function */
static void ntree_free_type(void *treetype_v)
{
  bNodeTreeType *treetype = (bNodeTreeType *)treetype_v;
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

bool ntreeIsRegistered(bNodeTree *ntree)
{
  return (ntree->typeinfo != &NodeTreeTypeUndefined);
}

GHashIterator *ntreeTypeGetIterator(void)
{
  return BLI_ghashIterator_new(nodetreetypes_hash);
}

bNodeType *nodeTypeFind(const char *idname)
{
  if (idname[0]) {
    bNodeType *nt = (bNodeType *)BLI_ghash_lookup(nodetypes_hash, idname);
    if (nt) {
      return nt;
    }
  }

  return nullptr;
}

static void free_dynamic_typeinfo(bNodeType *ntype)
{
  if (ntype->type == NODE_DYNAMIC) {
    if (ntype->inputs) {
      MEM_freeN(ntype->inputs);
    }
    if (ntype->outputs) {
      MEM_freeN(ntype->outputs);
    }
  }
}

/* callback for hash value free function */
static void node_free_type(void *nodetype_v)
{
  bNodeType *nodetype = (bNodeType *)nodetype_v;
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, nodetype, nullptr, true);

  /* XXX deprecated */
  if (nodetype->type == NODE_DYNAMIC) {
    free_dynamic_typeinfo(nodetype);
  }

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

bool nodeTypeUndefined(bNode *node)
{
  return (node->typeinfo == &NodeTypeUndefined) ||
         (node->type == NODE_GROUP && node->id && ID_IS_LINKED(node->id) &&
          (node->id->tag & LIB_TAG_MISSING));
}

GHashIterator *nodeTypeGetIterator(void)
{
  return BLI_ghashIterator_new(nodetypes_hash);
}

bNodeSocketType *nodeSocketTypeFind(const char *idname)
{
  if (idname[0]) {
    bNodeSocketType *st = (bNodeSocketType *)BLI_ghash_lookup(nodesockettypes_hash, idname);
    if (st) {
      return st;
    }
  }

  return nullptr;
}

/* callback for hash value free function */
static void node_free_socket_type(void *socktype_v)
{
  bNodeSocketType *socktype = (bNodeSocketType *)socktype_v;
  /* XXX pass Main to unregister function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, nullptr, socktype, true);

  socktype->free_self(socktype);
}

void nodeRegisterSocketType(bNodeSocketType *st)
{
  BLI_ghash_insert(nodesockettypes_hash, (void *)st->idname, st);
  /* XXX pass Main to register function? */
  /* Probably not. It is pretty much expected we want to update G_MAIN here I think -
   * or we'd want to update *all* active Mains, which we cannot do anyway currently. */
  update_typeinfo(G_MAIN, nullptr, nullptr, nullptr, st, false);
}

void nodeUnregisterSocketType(bNodeSocketType *st)
{
  BLI_ghash_remove(nodesockettypes_hash, st->idname, nullptr, node_free_socket_type);
}

bool nodeSocketIsRegistered(bNodeSocket *sock)
{
  return (sock->typeinfo != &NodeSocketTypeUndefined);
}

GHashIterator *nodeSocketTypeGetIterator(void)
{
  return BLI_ghashIterator_new(nodesockettypes_hash);
}

struct bNodeSocket *nodeFindSocket(const bNode *node, int in_out, const char *identifier)
{
  const ListBase *sockets = (in_out == SOCK_IN) ? &node->inputs : &node->outputs;
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

/* find unique socket identifier */
static bool unique_identifier_check(void *arg, const char *identifier)
{
  const ListBase *lb = (const ListBase *)arg;
  LISTBASE_FOREACH (bNodeSocket *, sock, lb) {
    if (STREQ(sock->identifier, identifier)) {
      return true;
    }
  }
  return false;
}

static bNodeSocket *make_socket(bNodeTree *ntree,
                                bNode *UNUSED(node),
                                int in_out,
                                ListBase *lb,
                                const char *idname,
                                const char *identifier,
                                const char *name)
{
  char auto_identifier[MAX_NAME];

  if (identifier && identifier[0] != '\0') {
    /* use explicit identifier */
    BLI_strncpy(auto_identifier, identifier, sizeof(auto_identifier));
  }
  else {
    /* if no explicit identifier is given, assign a unique identifier based on the name */
    BLI_strncpy(auto_identifier, name, sizeof(auto_identifier));
  }
  /* make the identifier unique */
  BLI_uniquename_cb(
      unique_identifier_check, lb, "socket", '.', auto_identifier, sizeof(auto_identifier));

  bNodeSocket *sock = (bNodeSocket *)MEM_callocN(sizeof(bNodeSocket), "sock");
  sock->in_out = in_out;

  BLI_strncpy(sock->identifier, auto_identifier, NODE_MAXSTR);
  sock->limit = (in_out == SOCK_IN ? 1 : 0xFFF);

  BLI_strncpy(sock->name, name, NODE_MAXSTR);
  sock->storage = nullptr;
  sock->flag |= SOCK_COLLAPSED;
  sock->type = SOCK_CUSTOM; /* int type undefined by default */

  BLI_strncpy(sock->idname, idname, sizeof(sock->idname));
  node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(idname));

  return sock;
}

static void socket_id_user_increment(bNodeSocket *sock)
{
  switch ((eNodeSocketDatatype)sock->type) {
    case SOCK_OBJECT: {
      bNodeSocketValueObject *default_value = (bNodeSocketValueObject *)sock->default_value;
      id_us_plus((ID *)default_value->value);
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *default_value = (bNodeSocketValueImage *)sock->default_value;
      id_us_plus((ID *)default_value->value);
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *default_value = (bNodeSocketValueCollection *)
                                                      sock->default_value;
      id_us_plus((ID *)default_value->value);
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_INT:
    case SOCK_STRING:
    case __SOCK_MESH:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      break;
  }
}

static void socket_id_user_decrement(bNodeSocket *sock)
{
  switch ((eNodeSocketDatatype)sock->type) {
    case SOCK_OBJECT: {
      bNodeSocketValueObject *default_value = (bNodeSocketValueObject *)sock->default_value;
      if (default_value->value != nullptr) {
        id_us_min(&default_value->value->id);
      }
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *default_value = (bNodeSocketValueImage *)sock->default_value;
      if (default_value->value != nullptr) {
        id_us_min(&default_value->value->id);
      }
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *default_value = (bNodeSocketValueCollection *)
                                                      sock->default_value;
      if (default_value->value != nullptr) {
        id_us_min(&default_value->value->id);
      }
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_BOOLEAN:
    case SOCK_INT:
    case SOCK_STRING:
    case __SOCK_MESH:
    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      break;
  }
}

void nodeModifySocketType(
    bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, int type, int subtype)
{
  const char *idname = nodeStaticSocketType(type, subtype);

  if (!idname) {
    CLOG_ERROR(&LOG, "static node socket type %d undefined", type);
    return;
  }

  if (sock->default_value) {
    socket_id_user_decrement(sock);
    MEM_freeN(sock->default_value);
    sock->default_value = nullptr;
  }

  sock->type = type;
  BLI_strncpy(sock->idname, idname, sizeof(sock->idname));
  node_socket_set_typeinfo(ntree, sock, nodeSocketTypeFind(idname));
}

bNodeSocket *nodeAddSocket(bNodeTree *ntree,
                           bNode *node,
                           int in_out,
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

  node->update |= NODE_UPDATE;

  return sock;
}

bNodeSocket *nodeInsertSocket(bNodeTree *ntree,
                              bNode *node,
                              int in_out,
                              const char *idname,
                              bNodeSocket *next_sock,
                              const char *identifier,
                              const char *name)
{
  ListBase *lb = (in_out == SOCK_IN ? &node->inputs : &node->outputs);
  bNodeSocket *sock = make_socket(ntree, node, in_out, lb, idname, identifier, name);

  BLI_remlink(lb, sock); /* does nothing for new socket */
  BLI_insertlinkbefore(lb, next_sock, sock);

  node->update |= NODE_UPDATE;

  return sock;
}

const char *nodeStaticSocketType(int type, int subtype)
{
  switch (type) {
    case SOCK_FLOAT:
      switch (subtype) {
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
        case PROP_NONE:
        default:
          return "NodeSocketFloat";
      }
    case SOCK_INT:
      switch (subtype) {
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
    case SOCK_VECTOR:
      switch (subtype) {
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
  }
  return nullptr;
}

const char *nodeStaticSocketInterfaceType(int type, int subtype)
{
  switch (type) {
    case SOCK_FLOAT:
      switch (subtype) {
        case PROP_UNSIGNED:
          return "NodeSocketInterfaceFloatUnsigned";
        case PROP_PERCENTAGE:
          return "NodeSocketInterfaceFloatPercentage";
        case PROP_FACTOR:
          return "NodeSocketInterfaceFloatFactor";
        case PROP_ANGLE:
          return "NodeSocketInterfaceFloatAngle";
        case PROP_TIME:
          return "NodeSocketInterfaceFloatTime";
        case PROP_NONE:
        default:
          return "NodeSocketInterfaceFloat";
      }
    case SOCK_INT:
      switch (subtype) {
        case PROP_UNSIGNED:
          return "NodeSocketInterfaceIntUnsigned";
        case PROP_PERCENTAGE:
          return "NodeSocketInterfaceIntPercentage";
        case PROP_FACTOR:
          return "NodeSocketInterfaceIntFactor";
        case PROP_NONE:
        default:
          return "NodeSocketInterfaceInt";
      }
    case SOCK_BOOLEAN:
      return "NodeSocketInterfaceBool";
    case SOCK_VECTOR:
      switch (subtype) {
        case PROP_TRANSLATION:
          return "NodeSocketInterfaceVectorTranslation";
        case PROP_DIRECTION:
          return "NodeSocketInterfaceVectorDirection";
        case PROP_VELOCITY:
          return "NodeSocketInterfaceVectorVelocity";
        case PROP_ACCELERATION:
          return "NodeSocketInterfaceVectorAcceleration";
        case PROP_EULER:
          return "NodeSocketInterfaceVectorEuler";
        case PROP_XYZ:
          return "NodeSocketInterfaceVectorXYZ";
        case PROP_NONE:
        default:
          return "NodeSocketInterfaceVector";
      }
    case SOCK_RGBA:
      return "NodeSocketInterfaceColor";
    case SOCK_STRING:
      return "NodeSocketInterfaceString";
    case SOCK_SHADER:
      return "NodeSocketInterfaceShader";
    case SOCK_OBJECT:
      return "NodeSocketInterfaceObject";
    case SOCK_IMAGE:
      return "NodeSocketInterfaceImage";
    case SOCK_GEOMETRY:
      return "NodeSocketInterfaceGeometry";
    case SOCK_COLLECTION:
      return "NodeSocketInterfaceCollection";
  }
  return nullptr;
}

bNodeSocket *nodeAddStaticSocket(bNodeTree *ntree,
                                 bNode *node,
                                 int in_out,
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

bNodeSocket *nodeInsertStaticSocket(bNodeTree *ntree,
                                    bNode *node,
                                    int in_out,
                                    int type,
                                    int subtype,
                                    bNodeSocket *next_sock,
                                    const char *identifier,
                                    const char *name)
{
  const char *idname = nodeStaticSocketType(type, subtype);

  if (!idname) {
    CLOG_ERROR(&LOG, "static node socket type %d undefined", type);
    return nullptr;
  }

  bNodeSocket *sock = nodeInsertSocket(ntree, node, in_out, idname, next_sock, identifier, name);
  sock->type = type;
  return sock;
}

static void node_socket_free(bNodeTree *UNUSED(ntree),
                             bNodeSocket *sock,
                             bNode *UNUSED(node),
                             const bool do_id_user)
{
  if (sock->prop) {
    IDP_FreePropertyContent_ex(sock->prop, do_id_user);
    MEM_freeN(sock->prop);
  }

  if (sock->default_value) {
    if (do_id_user) {
      socket_id_user_decrement(sock);
    }
    MEM_freeN(sock->default_value);
  }
}

void nodeRemoveSocket(bNodeTree *ntree, bNode *node, bNodeSocket *sock)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->fromsock == sock || link->tosock == sock) {
      nodeRemLink(ntree, link);
    }
  }

  /* this is fast, this way we don't need an in_out argument */
  BLI_remlink(&node->inputs, sock);
  BLI_remlink(&node->outputs, sock);

  node_socket_free(ntree, sock, node, true);
  MEM_freeN(sock);

  node->update |= NODE_UPDATE;
}

void nodeRemoveAllSockets(bNodeTree *ntree, bNode *node)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->fromnode == node || link->tonode == node) {
      nodeRemLink(ntree, link);
    }
  }

  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->inputs) {
    node_socket_free(ntree, sock, node, true);
    MEM_freeN(sock);
  }
  BLI_listbase_clear(&node->inputs);

  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->outputs) {
    node_socket_free(ntree, sock, node, true);
    MEM_freeN(sock);
  }
  BLI_listbase_clear(&node->outputs);

  node->update |= NODE_UPDATE;
}

/* finds a node based on its name */
bNode *nodeFindNodebyName(bNodeTree *ntree, const char *name)
{
  return (bNode *)BLI_findstring(&ntree->nodes, name, offsetof(bNode, name));
}

/* Finds a node based on given socket and returns true on success. */
bool nodeFindNode(bNodeTree *ntree, bNodeSocket *sock, bNode **r_node, int *r_sockindex)
{
  *r_node = nullptr;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    ListBase *sockets = (sock->in_out == SOCK_IN) ? &node->inputs : &node->outputs;
    int index = 0;
    LISTBASE_FOREACH (bNodeSocket *, tsock, sockets) {
      if (sock == tsock) {
        if (r_node != nullptr) {
          *r_node = node;
        }
        if (r_sockindex != nullptr) {
          *r_sockindex = index;
        }
        return true;
      }
      index++;
    }
  }
  return false;
}

/**
 * \note Recursive
 */
bNode *nodeFindRootParent(bNode *node)
{
  if (node->parent) {
    return nodeFindRootParent(node->parent);
  }
  return node->type == NODE_FRAME ? node : nullptr;
}

/**
 * \returns true if \a child has \a parent as a parent/grandparent/...
 * \note Recursive
 */
bool nodeIsChildOf(const bNode *parent, const bNode *child)
{
  if (parent == child) {
    return true;
  }
  if (child->parent) {
    return nodeIsChildOf(parent, child->parent);
  }
  return false;
}

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * \param reversed: for backwards iteration
 * \note Recursive
 */
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
    if (link->tonode && link->fromnode) {
      /* Is the link part of the chain meaning node_start == fromnode
       * (or tonode for reversed case)? */
      if ((reversed && (link->tonode == node_start)) ||
          (!reversed && link->fromnode == node_start)) {
        if (!callback(link->fromnode, link->tonode, userdata, reversed)) {
          return;
        }
        nodeChainIter(
            ntree, reversed ? link->fromnode : link->tonode, callback, userdata, reversed);
      }
    }
  }
}

static void iter_backwards_ex(const bNodeTree *ntree,
                              const bNode *node_start,
                              bool (*callback)(bNode *, bNode *, void *),
                              void *userdata,
                              char recursion_mask)
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
    if (link->fromnode->iter_flag & recursion_mask) {
      continue;
    }

    link->fromnode->iter_flag |= recursion_mask;

    if (!callback(link->fromnode, link->tonode, userdata)) {
      return;
    }
    iter_backwards_ex(ntree, link->fromnode, callback, userdata, recursion_mask);
  }
}

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * Faster than nodeChainIter. Iter only once per node.
 * Can be called recursively (using another nodeChainIterBackwards) by
 * setting the recursion_lvl accordingly.
 *
 * \note Needs updated socket links (ntreeUpdateTree).
 * \note Recursive
 */
void nodeChainIterBackwards(const bNodeTree *ntree,
                            const bNode *node_start,
                            bool (*callback)(bNode *, bNode *, void *),
                            void *userdata,
                            int recursion_lvl)
{
  if (!node_start) {
    return;
  }

  /* Limited by iter_flag type. */
  BLI_assert(recursion_lvl < 8);
  char recursion_mask = (1 << recursion_lvl);

  /* Reset flag. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->iter_flag &= ~recursion_mask;
  }

  iter_backwards_ex(ntree, node_start, callback, userdata, recursion_mask);
}

/**
 * Iterate over all parents of \a node, executing \a callback for each parent
 * (which can return false to end iterator)
 *
 * \note Recursive
 */
void nodeParentsIter(bNode *node, bool (*callback)(bNode *, void *), void *userdata)
{
  if (node->parent) {
    if (!callback(node->parent, userdata)) {
      return;
    }
    nodeParentsIter(node->parent, callback, userdata);
  }
}

/* ************** Add stuff ********** */

/* Find the first available, non-duplicate name for a given node */
void nodeUniqueName(bNodeTree *ntree, bNode *node)
{
  BLI_uniquename(
      &ntree->nodes, node, DATA_("Node"), '.', offsetof(bNode, name), sizeof(node->name));
}

bNode *nodeAddNode(const struct bContext *C, bNodeTree *ntree, const char *idname)
{
  bNode *node = (bNode *)MEM_callocN(sizeof(bNode), "new node");
  BLI_addtail(&ntree->nodes, node);

  BLI_strncpy(node->idname, idname, sizeof(node->idname));
  node_set_typeinfo(C, ntree, node, nodeTypeFind(idname));

  ntree->update |= NTREE_UPDATE_NODES;

  return node;
}

bNode *nodeAddStaticNode(const struct bContext *C, bNodeTree *ntree, int type)
{
  const char *idname = nullptr;

  NODE_TYPES_BEGIN (ntype) {
    /* do an extra poll here, because some int types are used
     * for multiple node types, this helps find the desired type
     */
    if (ntype->type == type && (!ntype->poll || ntype->poll(ntype, ntree))) {
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
  if (sock_src->prop) {
    sock_dst->prop = IDP_CopyProperty_ex(sock_src->prop, flag);
  }

  if (sock_src->default_value) {
    sock_dst->default_value = MEM_dupallocN(sock_src->default_value);

    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      socket_id_user_increment(sock_dst);
    }
  }

  sock_dst->stack_index = 0;
  /* XXX some compositor node (e.g. image, render layers) still store
   * some persistent buffer data here, need to clear this to avoid dangling pointers.
   */
  sock_dst->cache = nullptr;
}

/* keep socket listorder identical, for copying links */
/* ntree is the target tree */
/* unique_name needs to be true. It's only disabled for speed when doing GPUnodetrees. */
bNode *BKE_node_copy_ex(bNodeTree *ntree,
                        const bNode *node_src,
                        const int flag,
                        const bool unique_name)
{
  bNode *node_dst = (bNode *)MEM_callocN(sizeof(bNode), "dupli node");
  bNodeSocket *sock_dst, *sock_src;
  bNodeLink *link_dst, *link_src;

  *node_dst = *node_src;
  /* can be called for nodes outside a node tree (e.g. clipboard) */
  if (ntree) {
    if (unique_name) {
      nodeUniqueName(ntree, node_dst);
    }

    BLI_addtail(&ntree->nodes, node_dst);
  }

  BLI_duplicatelist(&node_dst->inputs, &node_src->inputs);
  for (sock_dst = (bNodeSocket *)node_dst->inputs.first,
      sock_src = (bNodeSocket *)node_src->inputs.first;
       sock_dst != nullptr;
       sock_dst = (bNodeSocket *)sock_dst->next, sock_src = (bNodeSocket *)sock_src->next) {
    node_socket_copy(sock_dst, sock_src, flag);
  }

  BLI_duplicatelist(&node_dst->outputs, &node_src->outputs);
  for (sock_dst = (bNodeSocket *)node_dst->outputs.first,
      sock_src = (bNodeSocket *)node_src->outputs.first;
       sock_dst != nullptr;
       sock_dst = (bNodeSocket *)sock_dst->next, sock_src = (bNodeSocket *)sock_src->next) {
    node_socket_copy(sock_dst, sock_src, flag);
  }

  if (node_src->prop) {
    node_dst->prop = IDP_CopyProperty_ex(node_src->prop, flag);
  }

  BLI_duplicatelist(&node_dst->internal_links, &node_src->internal_links);
  for (link_dst = (bNodeLink *)node_dst->internal_links.first,
      link_src = (bNodeLink *)node_src->internal_links.first;
       link_dst != nullptr;
       link_dst = (bNodeLink *)link_dst->next, link_src = (bNodeLink *)link_src->next) {
    /* This is a bit annoying to do index lookups in a list, but is likely to be faster than
     * trying to create a hash-map. At least for usual nodes, which only have so much sockets
     * and internal links. */
    const int from_sock_index = BLI_findindex(&node_src->inputs, link_src->fromsock);
    const int to_sock_index = BLI_findindex(&node_src->outputs, link_src->tosock);
    BLI_assert(from_sock_index != -1);
    BLI_assert(to_sock_index != -1);
    link_dst->fromnode = node_dst;
    link_dst->tonode = node_dst;
    link_dst->fromsock = (bNodeSocket *)BLI_findlink(&node_dst->inputs, from_sock_index);
    link_dst->tosock = (bNodeSocket *)BLI_findlink(&node_dst->outputs, to_sock_index);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus(node_dst->id);
  }

  if (node_src->typeinfo->copyfunc) {
    node_src->typeinfo->copyfunc(ntree, node_dst, node_src);
  }

  node_dst->new_node = nullptr;

  /* Only call copy function when a copy is made for the main database, not
   * for cases like the dependency graph and localization. */
  if (node_dst->typeinfo->copyfunc_api && !(flag & LIB_ID_CREATE_NO_MAIN)) {
    PointerRNA ptr;
    RNA_pointer_create((ID *)ntree, &RNA_Node, node_dst, &ptr);

    node_dst->typeinfo->copyfunc_api(&ptr, node_src);
  }

  if (ntree) {
    ntree->update |= NTREE_UPDATE_NODES;
  }

  return node_dst;
}

static void node_set_new_pointers(bNode *node_src, bNode *new_node)
{
  /* Store mapping to the node itself. */
  node_src->new_node = new_node;
  /* Store mapping to inputs. */
  bNodeSocket *new_input_sock = (bNodeSocket *)new_node->inputs.first;
  bNodeSocket *input_sock_src = (bNodeSocket *)node_src->inputs.first;
  while (new_input_sock != nullptr) {
    input_sock_src->new_sock = new_input_sock;
    new_input_sock = new_input_sock->next;
    input_sock_src = input_sock_src->next;
  }
  /* Store mapping to outputs. */
  bNodeSocket *new_output_sock = (bNodeSocket *)new_node->outputs.first;
  bNodeSocket *output_sock_src = (bNodeSocket *)node_src->outputs.first;
  while (new_output_sock != nullptr) {
    output_sock_src->new_sock = new_output_sock;
    new_output_sock = new_output_sock->next;
    output_sock_src = output_sock_src->next;
  }
}

bNode *BKE_node_copy_store_new_pointers(bNodeTree *ntree, bNode *node_src, const int flag)
{
  bNode *new_node = BKE_node_copy_ex(ntree, node_src, flag, true);
  node_set_new_pointers(node_src, new_node);
  return new_node;
}

bNodeTree *ntreeCopyTree_ex_new_pointers(const bNodeTree *ntree,
                                         Main *bmain,
                                         const bool do_id_user)
{
  bNodeTree *new_ntree = ntreeCopyTree_ex(ntree, bmain, do_id_user);
  bNode *new_node = (bNode *)new_ntree->nodes.first;
  bNode *node_src = (bNode *)ntree->nodes.first;
  while (new_node != nullptr) {
    node_set_new_pointers(node_src, new_node);
    new_node = new_node->next;
    node_src = node_src->next;
  }
  return new_ntree;
}

/* also used via rna api, so we check for proper input output direction */
bNodeLink *nodeAddLink(
    bNodeTree *ntree, bNode *fromnode, bNodeSocket *fromsock, bNode *tonode, bNodeSocket *tosock)
{
  bNodeLink *link = nullptr;

  /* test valid input */
  BLI_assert(fromnode);
  BLI_assert(tonode);

  if (fromsock->in_out == SOCK_OUT && tosock->in_out == SOCK_IN) {
    link = (bNodeLink *)MEM_callocN(sizeof(bNodeLink), "link");
    if (ntree) {
      BLI_addtail(&ntree->links, link);
    }
    link->fromnode = fromnode;
    link->fromsock = fromsock;
    link->tonode = tonode;
    link->tosock = tosock;
  }
  else if (fromsock->in_out == SOCK_IN && tosock->in_out == SOCK_OUT) {
    /* OK but flip */
    link = (bNodeLink *)MEM_callocN(sizeof(bNodeLink), "link");
    if (ntree) {
      BLI_addtail(&ntree->links, link);
    }
    link->fromnode = tonode;
    link->fromsock = tosock;
    link->tonode = fromnode;
    link->tosock = fromsock;
  }

  if (ntree) {
    ntree->update |= NTREE_UPDATE_LINKS;
  }

  return link;
}

void nodeRemLink(bNodeTree *ntree, bNodeLink *link)
{
  /* can be called for links outside a node tree (e.g. clipboard) */
  if (ntree) {
    BLI_remlink(&ntree->links, link);
  }

  if (link->tosock) {
    link->tosock->link = nullptr;
  }
  MEM_freeN(link);

  if (ntree) {
    ntree->update |= NTREE_UPDATE_LINKS;
  }
}

void nodeRemSocketLinks(bNodeTree *ntree, bNodeSocket *sock)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->fromsock == sock || link->tosock == sock) {
      nodeRemLink(ntree, link);
    }
  }

  ntree->update |= NTREE_UPDATE_LINKS;
}

bool nodeLinkIsHidden(const bNodeLink *link)
{
  return nodeSocketIsHidden(link->fromsock) || nodeSocketIsHidden(link->tosock);
}

void nodeInternalRelink(bNodeTree *ntree, bNode *node)
{
  /* store link pointers in output sockets, for efficient lookup */
  LISTBASE_FOREACH (bNodeLink *, link, &node->internal_links) {
    link->tosock->link = link;
  }

  /* redirect downstream links */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    /* do we have internal link? */
    if (link->fromnode == node) {
      if (link->fromsock->link) {
        /* get the upstream input link */
        bNodeLink *fromlink = link->fromsock->link->fromsock->link;
        /* skip the node */
        if (fromlink) {
          link->fromnode = fromlink->fromnode;
          link->fromsock = fromlink->fromsock;

          /* if the up- or downstream link is invalid,
           * the replacement link will be invalid too.
           */
          if (!(fromlink->flag & NODE_LINK_VALID)) {
            link->flag &= ~NODE_LINK_VALID;
          }

          ntree->update |= NTREE_UPDATE_LINKS;
        }
        else {
          nodeRemLink(ntree, link);
        }
      }
      else {
        nodeRemLink(ntree, link);
      }
    }
  }

  /* remove remaining upstream links */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->tonode == node) {
      nodeRemLink(ntree, link);
    }
  }
}

void nodeToView(const bNode *node, float x, float y, float *rx, float *ry)
{
  if (node->parent) {
    nodeToView(node->parent, x + node->locx, y + node->locy, rx, ry);
  }
  else {
    *rx = x + node->locx;
    *ry = y + node->locy;
  }
}

void nodeFromView(const bNode *node, float x, float y, float *rx, float *ry)
{
  if (node->parent) {
    nodeFromView(node->parent, x, y, rx, ry);
    *rx -= node->locx;
    *ry -= node->locy;
  }
  else {
    *rx = x - node->locx;
    *ry = y - node->locy;
  }
}

bool nodeAttachNodeCheck(const bNode *node, const bNode *parent)
{
  for (const bNode *parent_iter = node; parent_iter; parent_iter = parent_iter->parent) {
    if (parent_iter == parent) {
      return true;
    }
  }

  return false;
}

void nodeAttachNode(bNode *node, bNode *parent)
{
  BLI_assert(parent->type == NODE_FRAME);
  BLI_assert(nodeAttachNodeCheck(parent, node) == false);

  float locx, locy;
  nodeToView(node, 0.0f, 0.0f, &locx, &locy);

  node->parent = parent;
  /* transform to parent space */
  nodeFromView(parent, locx, locy, &node->locx, &node->locy);
}

void nodeDetachNode(struct bNode *node)
{
  if (node->parent) {
    BLI_assert(node->parent->type == NODE_FRAME);

    /* transform to view space */
    float locx, locy;
    nodeToView(node, 0.0f, 0.0f, &locx, &locy);
    node->locx = locx;
    node->locy = locy;
    node->parent = nullptr;
  }
}

void nodePositionRelative(bNode *from_node,
                          bNode *to_node,
                          bNodeSocket *from_sock,
                          bNodeSocket *to_sock)
{
  float offset_x;
  int tot_sock_idx;

  /* Socket to plug into. */
  if (SOCK_IN == to_sock->in_out) {
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
    if (SOCK_IN == from_sock->in_out) {
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
  LISTBASE_FOREACH (bNodeSocket *, nsock, &node->inputs) {
    if (nsock->link != nullptr) {
      bNodeLink *link = nsock->link;
      nodePositionRelative(link->fromnode, link->tonode, link->fromsock, link->tosock);
      nodePositionPropagate(link->fromnode);
    }
  }
}

bNodeTree *ntreeAddTree(Main *bmain, const char *name, const char *idname)
{
  /* trees are created as local trees for compositor, material or texture nodes,
   * node groups and other tree types are created as library data.
   */
  const bool is_embedded = (bmain == nullptr);
  int flag = 0;
  if (is_embedded) {
    flag |= LIB_ID_CREATE_NO_MAIN;
  }
  bNodeTree *ntree = (bNodeTree *)BKE_libblock_alloc(bmain, ID_NT, name, flag);
  if (is_embedded) {
    ntree->id.flag |= LIB_EMBEDDED_DATA;
  }

  /* Types are fully initialized at this point,
   * if an undefined node is added later this will be reset.
   */
  ntree->init |= NTREE_TYPE_INIT;

  BLI_strncpy(ntree->idname, idname, sizeof(ntree->idname));
  ntree_set_typeinfo(ntree, ntreeTypeFind(idname));

  return ntree;
}

bNodeTree *ntreeCopyTree_ex(const bNodeTree *ntree, Main *bmain, const bool do_id_user)
{
  const int flag = do_id_user ? 0 : LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN;

  bNodeTree *ntree_copy = (bNodeTree *)BKE_id_copy_ex(bmain, (ID *)ntree, nullptr, flag);
  return ntree_copy;
}
bNodeTree *ntreeCopyTree(Main *bmain, const bNodeTree *ntree)
{
  return ntreeCopyTree_ex(ntree, bmain, true);
}

/* *************** Node Preview *********** */

/* XXX this should be removed eventually ...
 * Currently BKE functions are modeled closely on previous code,
 * using BKE_node_preview_init_tree to set up previews for a whole node tree in advance.
 * This should be left more to the individual node tree implementations.
 */
bool BKE_node_preview_used(const bNode *node)
{
  /* XXX check for closed nodes? */
  return (node->typeinfo->flag & NODE_PREVIEW) != 0;
}

bNodePreview *BKE_node_preview_verify(
    bNodeInstanceHash *previews, bNodeInstanceKey key, int xsize, int ysize, bool create)
{
  bNodePreview *preview = (bNodePreview *)BKE_node_instance_hash_lookup(previews, key);
  if (!preview) {
    if (create) {
      preview = (bNodePreview *)MEM_callocN(sizeof(bNodePreview), "node preview");
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
  if (preview->rect) {
    if (preview->xsize != xsize || preview->ysize != ysize) {
      MEM_freeN(preview->rect);
      preview->rect = nullptr;
    }
  }

  if (preview->rect == nullptr) {
    preview->rect = (unsigned char *)MEM_callocN(4 * xsize + xsize * ysize * sizeof(char[4]),
                                                 "node preview rect");
    preview->xsize = xsize;
    preview->ysize = ysize;
  }
  /* no clear, makes nicer previews */

  return preview;
}

bNodePreview *BKE_node_preview_copy(bNodePreview *preview)
{
  bNodePreview *new_preview = (bNodePreview *)MEM_dupallocN(preview);
  if (preview->rect) {
    new_preview->rect = (unsigned char *)MEM_dupallocN(preview->rect);
  }
  return new_preview;
}

void BKE_node_preview_free(bNodePreview *preview)
{
  if (preview->rect) {
    MEM_freeN(preview->rect);
  }
  MEM_freeN(preview);
}

static void node_preview_init_tree_recursive(bNodeInstanceHash *previews,
                                             bNodeTree *ntree,
                                             bNodeInstanceKey parent_key,
                                             int xsize,
                                             int ysize,
                                             bool create_previews)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);

    if (BKE_node_preview_used(node)) {
      node->preview_xsize = xsize;
      node->preview_ysize = ysize;

      BKE_node_preview_verify(previews, key, xsize, ysize, create_previews);
    }

    if (node->type == NODE_GROUP && node->id) {
      node_preview_init_tree_recursive(
          previews, (bNodeTree *)node->id, key, xsize, ysize, create_previews);
    }
  }
}

void BKE_node_preview_init_tree(bNodeTree *ntree, int xsize, int ysize, bool create_previews)
{
  if (!ntree) {
    return;
  }

  if (!ntree->previews) {
    ntree->previews = BKE_node_instance_hash_new("node previews");
  }

  node_preview_init_tree_recursive(
      ntree->previews, ntree, NODE_INSTANCE_KEY_BASE, xsize, ysize, create_previews);
}

static void node_preview_tag_used_recursive(bNodeInstanceHash *previews,
                                            bNodeTree *ntree,
                                            bNodeInstanceKey parent_key)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);

    if (BKE_node_preview_used(node)) {
      BKE_node_instance_hash_tag_key(previews, key);
    }

    if (node->type == NODE_GROUP && node->id) {
      node_preview_tag_used_recursive(previews, (bNodeTree *)node->id, key);
    }
  }
}

void BKE_node_preview_remove_unused(bNodeTree *ntree)
{
  if (!ntree || !ntree->previews) {
    return;
  }

  /* use the instance hash functions for tagging and removing unused previews */
  BKE_node_instance_hash_clear_tags(ntree->previews);
  node_preview_tag_used_recursive(ntree->previews, ntree, NODE_INSTANCE_KEY_BASE);

  BKE_node_instance_hash_remove_untagged(ntree->previews,
                                         (bNodeInstanceValueFP)BKE_node_preview_free);
}

void BKE_node_preview_free_tree(bNodeTree *ntree)
{
  if (!ntree) {
    return;
  }

  if (ntree->previews) {
    BKE_node_instance_hash_free(ntree->previews, (bNodeInstanceValueFP)BKE_node_preview_free);
    ntree->previews = nullptr;
  }
}

void BKE_node_preview_clear(bNodePreview *preview)
{
  if (preview && preview->rect) {
    memset(preview->rect, 0, MEM_allocN_len(preview->rect));
  }
}

void BKE_node_preview_clear_tree(bNodeTree *ntree)
{
  if (!ntree || !ntree->previews) {
    return;
  }

  bNodeInstanceHashIterator iter;
  NODE_INSTANCE_HASH_ITER (iter, ntree->previews) {
    bNodePreview *preview = (bNodePreview *)BKE_node_instance_hash_iterator_get_value(&iter);
    BKE_node_preview_clear(preview);
  }
}

static void node_preview_sync(bNodePreview *to, bNodePreview *from)
{
  /* sizes should have been initialized by BKE_node_preview_init_tree */
  BLI_assert(to->xsize == from->xsize && to->ysize == from->ysize);

  /* copy over contents of previews */
  if (to->rect && from->rect) {
    int xsize = to->xsize;
    int ysize = to->ysize;
    memcpy(to->rect, from->rect, xsize * ysize * sizeof(char[4]));
  }
}

void BKE_node_preview_sync_tree(bNodeTree *to_ntree, bNodeTree *from_ntree)
{
  bNodeInstanceHash *from_previews = from_ntree->previews;
  bNodeInstanceHash *to_previews = to_ntree->previews;

  if (!from_previews || !to_previews) {
    return;
  }

  bNodeInstanceHashIterator iter;
  NODE_INSTANCE_HASH_ITER (iter, from_previews) {
    bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
    bNodePreview *from = (bNodePreview *)BKE_node_instance_hash_iterator_get_value(&iter);
    bNodePreview *to = (bNodePreview *)BKE_node_instance_hash_lookup(to_previews, key);

    if (from && to) {
      node_preview_sync(to, from);
    }
  }
}

void BKE_node_preview_merge_tree(bNodeTree *to_ntree, bNodeTree *from_ntree, bool remove_old)
{
  if (remove_old || !to_ntree->previews) {
    /* free old previews */
    if (to_ntree->previews) {
      BKE_node_instance_hash_free(to_ntree->previews, (bNodeInstanceValueFP)BKE_node_preview_free);
    }

    /* transfer previews */
    to_ntree->previews = from_ntree->previews;
    from_ntree->previews = nullptr;

    /* clean up, in case any to_ntree nodes have been removed */
    BKE_node_preview_remove_unused(to_ntree);
  }
  else {
    if (from_ntree->previews) {
      bNodeInstanceHashIterator iter;
      NODE_INSTANCE_HASH_ITER (iter, from_ntree->previews) {
        bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
        bNodePreview *preview = (bNodePreview *)BKE_node_instance_hash_iterator_get_value(&iter);

        /* replace existing previews */
        BKE_node_instance_hash_remove(
            to_ntree->previews, key, (bNodeInstanceValueFP)BKE_node_preview_free);
        BKE_node_instance_hash_insert(to_ntree->previews, key, preview);
      }

      /* Note: null free function here,
       * because pointers have already been moved over to to_ntree->previews! */
      BKE_node_instance_hash_free(from_ntree->previews, nullptr);
      from_ntree->previews = nullptr;
    }
  }
}

/* hack warning! this function is only used for shader previews, and
 * since it gets called multiple times per pixel for Ztransp we only
 * add the color once. Preview gets cleared before it starts render though */
void BKE_node_preview_set_pixel(
    bNodePreview *preview, const float col[4], int x, int y, bool do_manage)
{
  if (preview) {
    if (x >= 0 && y >= 0) {
      if (x < preview->xsize && y < preview->ysize) {
        unsigned char *tar = preview->rect + 4 * ((preview->xsize * y) + x);

        if (do_manage) {
          linearrgb_to_srgb_uchar4(tar, col);
        }
        else {
          rgba_float_to_uchar(tar, col);
        }
      }
      // else printf("prv out bound x y %d %d\n", x, y);
    }
    // else printf("prv out bound x y %d %d\n", x, y);
  }
}

/* ************** Free stuff ********** */

/* goes over entire tree */
void nodeUnlinkNode(bNodeTree *ntree, bNode *node)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    ListBase *lb;
    if (link->fromnode == node) {
      lb = &node->outputs;
      if (link->tonode) {
        link->tonode->update |= NODE_UPDATE;
      }
    }
    else if (link->tonode == node) {
      lb = &node->inputs;
    }
    else {
      lb = nullptr;
    }

    if (lb) {
      LISTBASE_FOREACH (bNodeSocket *, sock, lb) {
        if (link->fromsock == sock || link->tosock == sock) {
          nodeRemLink(ntree, link);
          break;
        }
      }
    }
  }
}

static void node_unlink_attached(bNodeTree *ntree, bNode *parent)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->parent == parent) {
      nodeDetachNode(node);
    }
  }
}

/* Free the node itself. ID user refcounting is up the caller,
 * that does not happen here. */
static void node_free_node(bNodeTree *ntree, bNode *node)
{
  /* since it is called while free database, node->id is undefined */

  /* can be called for nodes outside a node tree (e.g. clipboard) */
  if (ntree) {
    /* remove all references to this node */
    nodeUnlinkNode(ntree, node);
    node_unlink_attached(ntree, node);

    BLI_remlink(&ntree->nodes, node);

    if (ntree->typeinfo->free_node_cache) {
      ntree->typeinfo->free_node_cache(ntree, node);
    }

    /* texture node has bad habit of keeping exec data around */
    if (ntree->type == NTREE_TEXTURE && ntree->execdata) {
      ntreeTexEndExecTree(ntree->execdata);
      ntree->execdata = nullptr;
    }
  }

  if (node->typeinfo->freefunc) {
    node->typeinfo->freefunc(node);
  }

  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->inputs) {
    /* Remember, no ID user refcount management here! */
    node_socket_free(ntree, sock, node, false);
    MEM_freeN(sock);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->outputs) {
    /* Remember, no ID user refcount management here! */
    node_socket_free(ntree, sock, node, false);
    MEM_freeN(sock);
  }

  BLI_freelistN(&node->internal_links);

  if (node->prop) {
    /* Remember, no ID user refcount management here! */
    IDP_FreePropertyContent_ex(node->prop, false);
    MEM_freeN(node->prop);
  }

  MEM_freeN(node);

  if (ntree) {
    ntree->update |= NTREE_UPDATE_NODES;
  }
}

void ntreeFreeLocalNode(bNodeTree *ntree, bNode *node)
{
  /* For removing nodes while editing localized node trees. */
  BLI_assert((ntree->id.tag & LIB_TAG_LOCALIZED) != 0);
  node_free_node(ntree, node);
}

void nodeRemoveNode(Main *bmain, bNodeTree *ntree, bNode *node, bool do_id_user)
{
  /* This function is not for localized node trees, we do not want
   * do to ID user refcounting and removal of animdation data then. */
  BLI_assert((ntree->id.tag & LIB_TAG_LOCALIZED) == 0);

  if (do_id_user) {
    /* Free callback for NodeCustomGroup. */
    if (node->typeinfo->freefunc_api) {
      PointerRNA ptr;
      RNA_pointer_create((ID *)ntree, &RNA_Node, node, &ptr);

      node->typeinfo->freefunc_api(&ptr);
    }

    /* Do user counting. */
    if (node->id) {
      id_us_min(node->id);
    }

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      socket_id_user_decrement(sock);
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      socket_id_user_decrement(sock);
    }
  }

  /* Remove animation data. */
  char propname_esc[MAX_IDPROP_NAME * 2];
  char prefix[MAX_IDPROP_NAME * 2];

  BLI_str_escape(propname_esc, node->name, sizeof(propname_esc));
  BLI_snprintf(prefix, sizeof(prefix), "nodes[\"%s\"]", propname_esc);

  if (BKE_animdata_fix_paths_remove((ID *)ntree, prefix)) {
    if (bmain != nullptr) {
      DEG_relations_tag_update(bmain);
    }
  }

  /* Free node itself. */
  node_free_node(ntree, node);
}

static void node_socket_interface_free(bNodeTree *UNUSED(ntree),
                                       bNodeSocket *sock,
                                       const bool do_id_user)
{
  if (sock->prop) {
    IDP_FreeProperty_ex(sock->prop, do_id_user);
  }

  if (sock->default_value) {
    if (do_id_user) {
      socket_id_user_decrement(sock);
    }
    MEM_freeN(sock->default_value);
  }
}

static void free_localized_node_groups(bNodeTree *ntree)
{
  /* Only localized node trees store a copy for each node group tree.
   * Each node group tree in a localized node tree can be freed,
   * since it is a localized copy itself (no risk of accessing free'd
   * data in main, see T37939).
   */
  if (!(ntree->id.tag & LIB_TAG_LOCALIZED)) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if ((ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) && node->id) {
      bNodeTree *ngroup = (bNodeTree *)node->id;
      ntreeFreeTree(ngroup);
      MEM_freeN(ngroup);
    }
  }
}

/* Free (or release) any data used by this nodetree. Does not free the
 * nodetree itself and does no ID user counting. */
void ntreeFreeTree(bNodeTree *ntree)
{
  ntree_free_data(&ntree->id);
  BKE_animdata_free(&ntree->id, false);
}

void ntreeFreeEmbeddedTree(bNodeTree *ntree)
{
  ntreeFreeTree(ntree);
  BKE_libblock_free_data(&ntree->id, true);
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

void ntreeFreeCache(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }

  if (ntree->typeinfo->free_cache) {
    ntree->typeinfo->free_cache(ntree);
  }
}

void ntreeSetOutput(bNodeTree *ntree)
{
  /* find the active outputs, might become tree type dependent handler */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->typeinfo->nclass == NODE_CLASS_OUTPUT) {
      /* we need a check for which output node should be tagged like this, below an exception */
      if (node->type == CMP_NODE_OUTPUT_FILE) {
        continue;
      }

      int output = 0;
      /* there is more types having output class, each one is checked */
      LISTBASE_FOREACH (bNode *, tnode, &ntree->nodes) {
        if (tnode->typeinfo->nclass == NODE_CLASS_OUTPUT) {
          if (ntree->type == NTREE_COMPOSIT) {
            /* same type, exception for viewer */
            if (tnode->type == node->type ||
                (ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER) &&
                 ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))) {
              if (tnode->flag & NODE_DO_OUTPUT) {
                output++;
                if (output > 1) {
                  tnode->flag &= ~NODE_DO_OUTPUT;
                }
              }
            }
          }
          else {
            /* same type */
            if (tnode->type == node->type) {
              if (tnode->flag & NODE_DO_OUTPUT) {
                output++;
                if (output > 1) {
                  tnode->flag &= ~NODE_DO_OUTPUT;
                }
              }
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
        if (tnode->type == NODE_GROUP_OUTPUT) {
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
  }

  /* here we could recursively set which nodes have to be done,
   * might be different for editor or for "real" use... */
}

/** Get address of potential nodetree pointer of given ID.
 *
 * \warning Using this function directly is potentially dangerous, if you don't know or are not
 * sure, please use `ntreeFromID()` instead. */
bNodeTree **BKE_ntree_ptr_from_id(ID *id)
{
  switch (GS(id->name)) {
    case ID_MA:
      return &((Material *)id)->nodetree;
    case ID_LA:
      return &((Light *)id)->nodetree;
    case ID_WO:
      return &((World *)id)->nodetree;
    case ID_TE:
      return &((Tex *)id)->nodetree;
    case ID_SCE:
      return &((Scene *)id)->nodetree;
    case ID_LS:
      return &((FreestyleLineStyle *)id)->nodetree;
    case ID_SIM:
      return &((Simulation *)id)->nodetree;
    default:
      return nullptr;
  }
}

/* Returns the private NodeTree object of the datablock, if it has one. */
bNodeTree *ntreeFromID(ID *id)
{
  bNodeTree **nodetree = BKE_ntree_ptr_from_id(id);
  return (nodetree != nullptr) ? *nodetree : nullptr;
}

bool ntreeNodeExists(const bNodeTree *ntree, const bNode *testnode)
{
  LISTBASE_FOREACH (const bNode *, node, &ntree->nodes) {
    if (node == testnode) {
      return true;
    }
  }
  return false;
}

bool ntreeOutputExists(const bNode *node, const bNodeSocket *testsock)
{
  LISTBASE_FOREACH (const bNodeSocket *, sock, &node->outputs) {
    if (sock == testsock) {
      return true;
    }
  }
  return false;
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

/* returns localized tree for execution in threads */
bNodeTree *ntreeLocalize(bNodeTree *ntree)
{
  if (ntree) {
    /* Make full copy outside of Main database.
     * Note: previews are not copied here.
     */
    bNodeTree *ltree = (bNodeTree *)BKE_id_copy_ex(
        nullptr, &ntree->id, nullptr, (LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA));

    ltree->id.tag |= LIB_TAG_LOCALIZED;

    LISTBASE_FOREACH (bNode *, node, &ltree->nodes) {
      if ((ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) && node->id) {
        node->id = (ID *)ntreeLocalize((bNodeTree *)node->id);
      }
    }

    /* ensures only a single output node is enabled */
    ntreeSetOutput(ntree);

    bNode *node_src = (bNode *)ntree->nodes.first;
    bNode *node_local = (bNode *)ltree->nodes.first;
    while (node_src != nullptr) {
      node_local->original = node_src;
      node_src = node_src->next;
      node_local = node_local->next;
    }

    if (ntree->typeinfo->localize) {
      ntree->typeinfo->localize(ltree, ntree);
    }

    return ltree;
  }

  return nullptr;
}

/* sync local composite with real tree */
/* local tree is supposed to be running, be careful moving previews! */
/* is called by jobs manager, outside threads, so it doesn't happen during draw */
void ntreeLocalSync(bNodeTree *localtree, bNodeTree *ntree)
{
  if (localtree && ntree) {
    if (ntree->typeinfo->local_sync) {
      ntree->typeinfo->local_sync(localtree, ntree);
    }
  }
}

/* merge local tree results back, and free local tree */
/* we have to assume the editor already changed completely */
void ntreeLocalMerge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree)
{
  if (ntree && localtree) {
    if (ntree->typeinfo->local_merge) {
      ntree->typeinfo->local_merge(bmain, localtree, ntree);
    }

    ntreeFreeTree(localtree);
    MEM_freeN(localtree);
  }
}

/* ************ NODE TREE INTERFACE *************** */

static bNodeSocket *make_socket_interface(bNodeTree *ntree,
                                          int in_out,
                                          const char *idname,
                                          const char *name)
{
  bNodeSocketType *stype = nodeSocketTypeFind(idname);
  int own_index = ntree->cur_index++;

  if (stype == nullptr) {
    return nullptr;
  }

  bNodeSocket *sock = (bNodeSocket *)MEM_callocN(sizeof(bNodeSocket), "socket template");
  BLI_strncpy(sock->idname, stype->idname, sizeof(sock->idname));
  node_socket_set_typeinfo(ntree, sock, stype);
  sock->in_out = in_out;
  sock->type = SOCK_CUSTOM; /* int type undefined by default */

  /* assign new unique index */
  own_index = ntree->cur_index++;
  /* use the own_index as socket identifier */
  if (in_out == SOCK_IN) {
    BLI_snprintf(sock->identifier, MAX_NAME, "Input_%d", own_index);
  }
  else {
    BLI_snprintf(sock->identifier, MAX_NAME, "Output_%d", own_index);
  }

  sock->limit = (in_out == SOCK_IN ? 1 : 0xFFF);

  BLI_strncpy(sock->name, name, NODE_MAXSTR);
  sock->storage = nullptr;
  sock->flag |= SOCK_COLLAPSED;

  return sock;
}

bNodeSocket *ntreeFindSocketInterface(bNodeTree *ntree, int in_out, const char *identifier)
{
  ListBase *sockets = (in_out == SOCK_IN) ? &ntree->inputs : &ntree->outputs;
  LISTBASE_FOREACH (bNodeSocket *, iosock, sockets) {
    if (STREQ(iosock->identifier, identifier)) {
      return iosock;
    }
  }
  return nullptr;
}

bNodeSocket *ntreeAddSocketInterface(bNodeTree *ntree,
                                     int in_out,
                                     const char *idname,
                                     const char *name)
{
  bNodeSocket *iosock = make_socket_interface(ntree, in_out, idname, name);
  if (in_out == SOCK_IN) {
    BLI_addtail(&ntree->inputs, iosock);
    ntree->update |= NTREE_UPDATE_GROUP_IN;
  }
  else if (in_out == SOCK_OUT) {
    BLI_addtail(&ntree->outputs, iosock);
    ntree->update |= NTREE_UPDATE_GROUP_OUT;
  }
  return iosock;
}

bNodeSocket *ntreeInsertSocketInterface(
    bNodeTree *ntree, int in_out, const char *idname, bNodeSocket *next_sock, const char *name)
{
  bNodeSocket *iosock = make_socket_interface(ntree, in_out, idname, name);
  if (in_out == SOCK_IN) {
    BLI_insertlinkbefore(&ntree->inputs, next_sock, iosock);
    ntree->update |= NTREE_UPDATE_GROUP_IN;
  }
  else if (in_out == SOCK_OUT) {
    BLI_insertlinkbefore(&ntree->outputs, next_sock, iosock);
    ntree->update |= NTREE_UPDATE_GROUP_OUT;
  }
  return iosock;
}

struct bNodeSocket *ntreeAddSocketInterfaceFromSocket(bNodeTree *ntree,
                                                      bNode *from_node,
                                                      bNodeSocket *from_sock)
{
  bNodeSocket *iosock = ntreeAddSocketInterface(
      ntree, from_sock->in_out, from_sock->idname, from_sock->name);
  if (iosock) {
    if (iosock->typeinfo->interface_from_socket) {
      iosock->typeinfo->interface_from_socket(ntree, iosock, from_node, from_sock);
    }
  }
  return iosock;
}

struct bNodeSocket *ntreeInsertSocketInterfaceFromSocket(bNodeTree *ntree,
                                                         bNodeSocket *next_sock,
                                                         bNode *from_node,
                                                         bNodeSocket *from_sock)
{
  bNodeSocket *iosock = ntreeInsertSocketInterface(
      ntree, from_sock->in_out, from_sock->idname, next_sock, from_sock->name);
  if (iosock) {
    if (iosock->typeinfo->interface_from_socket) {
      iosock->typeinfo->interface_from_socket(ntree, iosock, from_node, from_sock);
    }
  }
  return iosock;
}

void ntreeRemoveSocketInterface(bNodeTree *ntree, bNodeSocket *sock)
{
  /* this is fast, this way we don't need an in_out argument */
  BLI_remlink(&ntree->inputs, sock);
  BLI_remlink(&ntree->outputs, sock);

  node_socket_interface_free(ntree, sock, true);
  MEM_freeN(sock);

  ntree->update |= NTREE_UPDATE_GROUP;
}

/* generates a valid RNA identifier from the node tree name */
static void ntree_interface_identifier_base(bNodeTree *ntree, char *base)
{
  /* generate a valid RNA identifier */
  sprintf(base, "NodeTreeInterface_%s", ntree->id.name + 2);
  RNA_identifier_sanitize(base, false);
}

/* check if the identifier is already in use */
static bool ntree_interface_unique_identifier_check(void *UNUSED(data), const char *identifier)
{
  return (RNA_struct_find(identifier) != nullptr);
}

/* generates the actual unique identifier and ui name and description */
static void ntree_interface_identifier(bNodeTree *ntree,
                                       const char *base,
                                       char *identifier,
                                       int maxlen,
                                       char *name,
                                       char *description)
{
  /* There is a possibility that different node tree names get mapped to the same identifier
   * after sanitation (e.g. "SomeGroup_A", "SomeGroup.A" both get sanitized to "SomeGroup_A").
   * On top of the sanitized id string add a number suffix if necessary to avoid duplicates.
   */
  identifier[0] = '\0';
  BLI_uniquename_cb(
      ntree_interface_unique_identifier_check, nullptr, base, '_', identifier, maxlen);

  sprintf(name, "Node Tree %s Interface", ntree->id.name + 2);
  sprintf(description, "Interface properties of node group %s", ntree->id.name + 2);
}

static void ntree_interface_type_create(bNodeTree *ntree)
{
  /* strings are generated from base string + ID name, sizes are sufficient */
  char base[MAX_ID_NAME + 64], identifier[MAX_ID_NAME + 64], name[MAX_ID_NAME + 64],
      description[MAX_ID_NAME + 64];

  /* generate a valid RNA identifier */
  ntree_interface_identifier_base(ntree, base);
  ntree_interface_identifier(ntree, base, identifier, sizeof(identifier), name, description);

  /* register a subtype of PropertyGroup */
  StructRNA *srna = RNA_def_struct_ptr(&BLENDER_RNA, identifier, &RNA_PropertyGroup);
  RNA_def_struct_ui_text(srna, name, description);
  RNA_def_struct_duplicate_pointers(&BLENDER_RNA, srna);

  /* associate the RNA type with the node tree */
  ntree->interface_type = srna;
  RNA_struct_blender_type_set(srna, ntree);

  /* add socket properties */
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->inputs) {
    bNodeSocketType *stype = sock->typeinfo;
    if (stype && stype->interface_register_properties) {
      stype->interface_register_properties(ntree, sock, srna);
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &ntree->outputs) {
    bNodeSocketType *stype = sock->typeinfo;
    if (stype && stype->interface_register_properties) {
      stype->interface_register_properties(ntree, sock, srna);
    }
  }
}

StructRNA *ntreeInterfaceTypeGet(bNodeTree *ntree, bool create)
{
  if (ntree->interface_type) {
    /* strings are generated from base string + ID name, sizes are sufficient */
    char base[MAX_ID_NAME + 64], identifier[MAX_ID_NAME + 64], name[MAX_ID_NAME + 64],
        description[MAX_ID_NAME + 64];

    /* A bit of a hack: when changing the ID name, update the RNA type identifier too,
     * so that the names match. This is not strictly necessary to keep it working,
     * but better for identifying associated NodeTree blocks and RNA types.
     */
    StructRNA *srna = ntree->interface_type;

    ntree_interface_identifier_base(ntree, base);

    /* RNA identifier may have a number suffix, but should start with the idbase string */
    if (!STREQLEN(RNA_struct_identifier(srna), base, sizeof(base))) {
      /* generate new unique RNA identifier from the ID name */
      ntree_interface_identifier(ntree, base, identifier, sizeof(identifier), name, description);

      /* rename the RNA type */
      RNA_def_struct_free_pointers(&BLENDER_RNA, srna);
      RNA_def_struct_identifier(&BLENDER_RNA, srna, identifier);
      RNA_def_struct_ui_text(srna, name, description);
      RNA_def_struct_duplicate_pointers(&BLENDER_RNA, srna);
    }
  }
  else if (create) {
    ntree_interface_type_create(ntree);
  }

  return ntree->interface_type;
}

void ntreeInterfaceTypeFree(bNodeTree *ntree)
{
  if (ntree->interface_type) {
    RNA_struct_free(&BLENDER_RNA, ntree->interface_type);
    ntree->interface_type = nullptr;
  }
}

void ntreeInterfaceTypeUpdate(bNodeTree *ntree)
{
  /* XXX it would be sufficient to just recreate all properties
   * instead of re-registering the whole struct type,
   * but there is currently no good way to do this in the RNA functions.
   * Overhead should be negligible.
   */
  ntreeInterfaceTypeFree(ntree);
  ntree_interface_type_create(ntree);
}

/* ************ find stuff *************** */

bNode *ntreeFindType(const bNodeTree *ntree, int type)
{
  if (ntree) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->type == type) {
        return node;
      }
    }
  }
  return nullptr;
}

bool ntreeHasType(const bNodeTree *ntree, int type)
{
  return ntreeFindType(ntree, type) != nullptr;
}

bool ntreeHasTree(const bNodeTree *ntree, const bNodeTree *lookup)
{
  if (ntree == lookup) {
    return true;
  }
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && node->id) {
      if (ntreeHasTree((bNodeTree *)node->id, lookup)) {
        return true;
      }
    }
  }
  return false;
}

bNodeLink *nodeFindLink(bNodeTree *ntree, const bNodeSocket *from, const bNodeSocket *to)
{
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->fromsock == from && link->tosock == to) {
      return link;
    }
    if (link->fromsock == to && link->tosock == from) { /* hrms? */
      return link;
    }
  }
  return nullptr;
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

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & NODE_ACTIVE) {
      return node;
    }
  }
  return nullptr;
}

static bNode *node_get_active_id_recursive(bNodeInstanceKey active_key,
                                           bNodeInstanceKey parent_key,
                                           bNodeTree *ntree,
                                           short idtype)
{
  if (parent_key.value == active_key.value || active_key.value == 0) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->id && GS(node->id->name) == idtype) {
        if (node->flag & NODE_ACTIVE_ID) {
          return node;
        }
      }
    }
  }
  else {
    /* no node with active ID in this tree, look inside groups */
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->type == NODE_GROUP) {
        bNodeTree *group = (bNodeTree *)node->id;
        if (group) {
          bNodeInstanceKey group_key = BKE_node_instance_key(parent_key, ntree, node);
          bNode *tnode = node_get_active_id_recursive(active_key, group_key, group, idtype);
          if (tnode) {
            return tnode;
          }
        }
      }
    }
  }
  return nullptr;
}

/* two active flags, ID nodes have special flag for buttons display */
bNode *nodeGetActiveID(bNodeTree *ntree, short idtype)
{
  if (ntree) {
    return node_get_active_id_recursive(
        ntree->active_viewer_key, NODE_INSTANCE_KEY_BASE, ntree, idtype);
  }
  return nullptr;
}

bool nodeSetActiveID(bNodeTree *ntree, short idtype, ID *id)
{
  bool ok = false;

  if (ntree == nullptr) {
    return ok;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->id && GS(node->id->name) == idtype) {
      if (id && ok == false && node->id == id) {
        node->flag |= NODE_ACTIVE_ID;
        ok = true;
      }
      else {
        node->flag &= ~NODE_ACTIVE_ID;
      }
    }
  }

  /* update all groups linked from here
   * if active ID node has been found already,
   * just pass null so other matching nodes are deactivated.
   */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == NODE_GROUP) {
      ok |= nodeSetActiveID((bNodeTree *)node->id, idtype, (ok == false ? id : nullptr));
    }
  }

  return ok;
}

/* two active flags, ID nodes have special flag for buttons display */
void nodeClearActiveID(bNodeTree *ntree, short idtype)
{
  if (ntree == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->id && GS(node->id->name) == idtype) {
      node->flag &= ~NODE_ACTIVE_ID;
    }
  }
}

void nodeSetSelected(bNode *node, bool select)
{
  if (select) {
    node->flag |= NODE_SELECT;
  }
  else {
    node->flag &= ~NODE_SELECT;

    /* deselect sockets too */
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      sock->flag &= ~NODE_SELECT;
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      sock->flag &= ~NODE_SELECT;
    }
  }
}

void nodeClearActive(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->flag &= ~(NODE_ACTIVE | NODE_ACTIVE_ID);
  }
}

/* two active flags, ID nodes have special flag for buttons display */
void nodeSetActive(bNodeTree *ntree, bNode *node)
{
  /* make sure only one node is active, and only one per ID type */
  LISTBASE_FOREACH (bNode *, tnode, &ntree->nodes) {
    tnode->flag &= ~NODE_ACTIVE;

    if (node->id && tnode->id) {
      if (GS(node->id->name) == GS(tnode->id->name)) {
        tnode->flag &= ~NODE_ACTIVE_ID;
      }
    }
    if ((node->typeinfo->nclass == NODE_CLASS_TEXTURE) ||
        (node->typeinfo->type == GEO_NODE_ATTRIBUTE_SAMPLE_TEXTURE)) {
      tnode->flag &= ~NODE_ACTIVE_TEXTURE;
    }
  }

  node->flag |= NODE_ACTIVE;
  if (node->id) {
    node->flag |= NODE_ACTIVE_ID;
  }
  if ((node->typeinfo->nclass == NODE_CLASS_TEXTURE) ||
      (node->typeinfo->type == GEO_NODE_ATTRIBUTE_SAMPLE_TEXTURE)) {
    node->flag |= NODE_ACTIVE_TEXTURE;
  }
}

int nodeSocketIsHidden(const bNodeSocket *sock)
{
  return ((sock->flag & (SOCK_HIDDEN | SOCK_UNAVAIL)) != 0);
}

void nodeSetSocketAvailability(bNodeSocket *sock, bool is_available)
{
  if (is_available) {
    sock->flag &= ~SOCK_UNAVAIL;
  }
  else {
    sock->flag |= SOCK_UNAVAIL;
  }
}

int nodeSocketLinkLimit(const bNodeSocket *sock)
{
  bNodeSocketType *stype = sock->typeinfo;
  if (sock->flag & SOCK_MULTI_INPUT) {
    return 4095;
  }
  if (stype != nullptr && stype->use_link_limits_of_type) {
    int limit = (sock->in_out == SOCK_IN) ? stype->input_link_limit : stype->output_link_limit;
    return limit;
  }

  return sock->limit;
}

/* ************** Node Clipboard *********** */

#define USE_NODE_CB_VALIDATE

#ifdef USE_NODE_CB_VALIDATE
/**
 * This data structure is to validate the node on creation,
 * otherwise we may reference missing data.
 *
 * Currently its only used for ID's, but nodes may one day
 * reference other pointers which need validation.
 */
struct bNodeClipboardExtraInfo {
  struct bNodeClipboardExtraInfo *next, *prev;
  ID *id;
  char id_name[MAX_ID_NAME];
  char library_name[FILE_MAX];
};
#endif /* USE_NODE_CB_VALIDATE */

struct bNodeClipboard {
  ListBase nodes;

#ifdef USE_NODE_CB_VALIDATE
  ListBase nodes_extra_info;
#endif

  ListBase links;
  int type;
};

static bNodeClipboard node_clipboard = {{nullptr}};

void BKE_node_clipboard_init(const struct bNodeTree *ntree)
{
  node_clipboard.type = ntree->type;
}

void BKE_node_clipboard_clear(void)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &node_clipboard.links) {
    nodeRemLink(nullptr, link);
  }
  BLI_listbase_clear(&node_clipboard.links);

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_clipboard.nodes) {
    node_free_node(nullptr, node);
  }
  BLI_listbase_clear(&node_clipboard.nodes);

#ifdef USE_NODE_CB_VALIDATE
  BLI_freelistN(&node_clipboard.nodes_extra_info);
#endif
}

/* return false when one or more ID's are lost */
bool BKE_node_clipboard_validate(void)
{
  bool ok = true;

#ifdef USE_NODE_CB_VALIDATE
  bNodeClipboardExtraInfo *node_info;
  bNode *node;

  /* lists must be aligned */
  BLI_assert(BLI_listbase_count(&node_clipboard.nodes) ==
             BLI_listbase_count(&node_clipboard.nodes_extra_info));

  for (node = (bNode *)node_clipboard.nodes.first,
      node_info = (bNodeClipboardExtraInfo *)node_clipboard.nodes_extra_info.first;
       node;
       node = (bNode *)node->next, node_info = (bNodeClipboardExtraInfo *)node_info->next) {
    /* validate the node against the stored node info */

    /* re-assign each loop since we may clear,
     * open a new file where the ID is valid, and paste again */
    node->id = node_info->id;

    /* currently only validate the ID */
    if (node->id) {
      /* We want to search into current blend file, so using G_MAIN is valid here too. */
      ListBase *lb = which_libbase(G_MAIN, GS(node_info->id_name));
      BLI_assert(lb != nullptr);

      if (BLI_findindex(lb, node_info->id) == -1) {
        /* May assign null. */
        node->id = (ID *)BLI_findstring(lb, node_info->id_name + 2, offsetof(ID, name) + 2);

        if (node->id == nullptr) {
          ok = false;
        }
      }
    }
  }
#endif /* USE_NODE_CB_VALIDATE */

  return ok;
}

void BKE_node_clipboard_add_node(bNode *node)
{
#ifdef USE_NODE_CB_VALIDATE
  /* add extra info */
  bNodeClipboardExtraInfo *node_info = (bNodeClipboardExtraInfo *)MEM_mallocN(
      sizeof(bNodeClipboardExtraInfo), __func__);

  node_info->id = node->id;
  if (node->id) {
    BLI_strncpy(node_info->id_name, node->id->name, sizeof(node_info->id_name));
    if (ID_IS_LINKED(node->id)) {
      BLI_strncpy(
          node_info->library_name, node->id->lib->filepath_abs, sizeof(node_info->library_name));
    }
    else {
      node_info->library_name[0] = '\0';
    }
  }
  else {
    node_info->id_name[0] = '\0';
    node_info->library_name[0] = '\0';
  }
  BLI_addtail(&node_clipboard.nodes_extra_info, node_info);
  /* end extra info */
#endif /* USE_NODE_CB_VALIDATE */

  /* add node */
  BLI_addtail(&node_clipboard.nodes, node);
}

void BKE_node_clipboard_add_link(bNodeLink *link)
{
  BLI_addtail(&node_clipboard.links, link);
}

const ListBase *BKE_node_clipboard_get_nodes(void)
{
  return &node_clipboard.nodes;
}

const ListBase *BKE_node_clipboard_get_links(void)
{
  return &node_clipboard.links;
}

int BKE_node_clipboard_get_type(void)
{
  return node_clipboard.type;
}

void BKE_node_clipboard_free(void)
{
  BKE_node_clipboard_validate();
  BKE_node_clipboard_clear();
}

/* Node Instance Hash */

/* magic number for initial hash key */
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

static unsigned int node_instance_hash_key(const void *key)
{
  return ((const bNodeInstanceKey *)key)->value;
}

static bool node_instance_hash_key_cmp(const void *a, const void *b)
{
  unsigned int value_a = ((const bNodeInstanceKey *)a)->value;
  unsigned int value_b = ((const bNodeInstanceKey *)b)->value;

  return (value_a != value_b);
}

bNodeInstanceHash *BKE_node_instance_hash_new(const char *info)
{
  bNodeInstanceHash *hash = (bNodeInstanceHash *)MEM_mallocN(sizeof(bNodeInstanceHash), info);
  hash->ghash = BLI_ghash_new(
      node_instance_hash_key, node_instance_hash_key_cmp, "node instance hash ghash");
  return hash;
}

void BKE_node_instance_hash_free(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp)
{
  BLI_ghash_free(hash->ghash, nullptr, (GHashValFreeFP)valfreefp);
  MEM_freeN(hash);
}

void BKE_node_instance_hash_insert(bNodeInstanceHash *hash, bNodeInstanceKey key, void *value)
{
  bNodeInstanceHashEntry *entry = (bNodeInstanceHashEntry *)value;
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
  return BLI_ghash_remove(hash->ghash, &key, nullptr, (GHashValFreeFP)valfreefp);
}

void BKE_node_instance_hash_clear(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp)
{
  BLI_ghash_clear(hash->ghash, nullptr, (GHashValFreeFP)valfreefp);
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
    bNodeInstanceHashEntry *value = (bNodeInstanceHashEntry *)
        BKE_node_instance_hash_iterator_get_value(&iter);

    value->tag = 0;
  }
}

void BKE_node_instance_hash_tag(bNodeInstanceHash *UNUSED(hash), void *value)
{
  bNodeInstanceHashEntry *entry = (bNodeInstanceHashEntry *)value;
  entry->tag = 1;
}

bool BKE_node_instance_hash_tag_key(bNodeInstanceHash *hash, bNodeInstanceKey key)
{
  bNodeInstanceHashEntry *entry = (bNodeInstanceHashEntry *)BKE_node_instance_hash_lookup(hash,
                                                                                          key);

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
  bNodeInstanceKey *untagged = (bNodeInstanceKey *)MEM_mallocN(
      sizeof(bNodeInstanceKey) * BKE_node_instance_hash_size(hash),
      "temporary node instance key list");
  bNodeInstanceHashIterator iter;
  int num_untagged = 0;
  NODE_INSTANCE_HASH_ITER (iter, hash) {
    bNodeInstanceHashEntry *value = (bNodeInstanceHashEntry *)
        BKE_node_instance_hash_iterator_get_value(&iter);

    if (!value->tag) {
      untagged[num_untagged++] = BKE_node_instance_hash_iterator_get_key(&iter);
    }
  }

  for (int i = 0; i < num_untagged; i++) {
    BKE_node_instance_hash_remove(hash, untagged[i], valfreefp);
  }

  MEM_freeN(untagged);
}

/* ************** dependency stuff *********** */

/* node is guaranteed to be not checked before */
static int node_get_deplist_recurs(bNodeTree *ntree, bNode *node, bNode ***nsort)
{
  int level = 0xFFF;

  node->done = true;

  /* check linked nodes */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tonode == node) {
      bNode *fromnode = link->fromnode;
      if (fromnode->done == 0) {
        fromnode->level = node_get_deplist_recurs(ntree, fromnode, nsort);
      }
      if (fromnode->level <= level) {
        level = fromnode->level - 1;
      }
    }
  }

  /* check parent node */
  if (node->parent) {
    if (node->parent->done == 0) {
      node->parent->level = node_get_deplist_recurs(ntree, node->parent, nsort);
    }
    if (node->parent->level <= level) {
      level = node->parent->level - 1;
    }
  }

  if (nsort) {
    **nsort = node;
    (*nsort)++;
  }

  return level;
}

void ntreeGetDependencyList(struct bNodeTree *ntree, struct bNode ***r_deplist, int *r_deplist_len)
{
  *r_deplist_len = 0;

  /* first clear data */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->done = false;
    (*r_deplist_len)++;
  }
  if (*r_deplist_len == 0) {
    *r_deplist = nullptr;
    return;
  }

  bNode **nsort;
  nsort = *r_deplist = (bNode **)MEM_callocN((*r_deplist_len) * sizeof(bNode *),
                                             "sorted node array");

  /* recursive check */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->done == 0) {
      node->level = node_get_deplist_recurs(ntree, node, &nsort);
    }
  }
}

/* only updates node->level for detecting cycles links */
static void ntree_update_node_level(bNodeTree *ntree)
{
  /* first clear tag */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->done = false;
  }

  /* recursive check */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->done == 0) {
      node->level = node_get_deplist_recurs(ntree, node, nullptr);
    }
  }
}

void ntreeTagUsedSockets(bNodeTree *ntree)
{
  /* first clear data */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      sock->flag &= ~SOCK_IN_USE;
    }
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      sock->flag &= ~SOCK_IN_USE;
    }
  }

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    link->fromsock->flag |= SOCK_IN_USE;
    link->tosock->flag |= SOCK_IN_USE;
  }
}

static void ntree_update_link_pointers(bNodeTree *ntree)
{
  /* first clear data */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      sock->link = nullptr;
    }
  }

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    link->tosock->link = link;
  }

  ntreeTagUsedSockets(ntree);
}

static void ntree_validate_links(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    link->flag |= NODE_LINK_VALID;
    if (link->fromnode && link->tonode && link->fromnode->level <= link->tonode->level) {
      link->flag &= ~NODE_LINK_VALID;
    }
    else if (ntree->typeinfo->validate_link) {
      if (!ntree->typeinfo->validate_link(ntree, link)) {
        link->flag &= ~NODE_LINK_VALID;
      }
    }
  }
}

void ntreeUpdateAllNew(Main *main)
{
  /* Update all new node trees on file read or append, to add/remove sockets
   * in groups nodes if the group changed, and handle any update flags that
   * might have been set in file reading or versioning. */
  FOREACH_NODETREE_BEGIN (main, ntree, owner_id) {
    if (owner_id->tag & LIB_TAG_NEW) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->typeinfo->group_update_func) {
          node->typeinfo->group_update_func(ntree, node);
        }
      }

      ntreeUpdateTree(nullptr, ntree);
    }
  }
  FOREACH_NODETREE_END;
}

void ntreeUpdateAllUsers(Main *main, ID *id)
{
  if (id == nullptr) {
    return;
  }

  /* Update all users of ngroup, to add/remove sockets as needed. */
  FOREACH_NODETREE_BEGIN (main, ntree, owner_id) {
    bool need_update = false;

    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->id == id) {
        if (node->typeinfo->group_update_func) {
          node->typeinfo->group_update_func(ntree, node);
        }

        need_update = true;
      }
    }

    if (need_update) {
      ntreeUpdateTree(nullptr, ntree);
    }
  }
  FOREACH_NODETREE_END;

  if (GS(id->name) == ID_NT) {
    bNodeTree *ngroup = (bNodeTree *)id;
    if (ngroup->type == NTREE_GEOMETRY) {
      LISTBASE_FOREACH (Object *, object, &main->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Nodes) {
            NodesModifierData *nmd = (NodesModifierData *)md;
            if (nmd->node_group == ngroup) {
              MOD_nodes_update_interface(object, nmd);
            }
          }
        }
      }
    }
  }
}

void ntreeUpdateTree(Main *bmain, bNodeTree *ntree)
{
  if (!ntree) {
    return;
  }

  /* Avoid re-entrant updates, can be caused by RNA update callbacks. */
  if (ntree->is_updating) {
    return;
  }
  ntree->is_updating = true;

  if (ntree->update & (NTREE_UPDATE_LINKS | NTREE_UPDATE_NODES)) {
    /* set the bNodeSocket->link pointers */
    ntree_update_link_pointers(ntree);
  }

  /* update individual nodes */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    /* node tree update tags override individual node update flags */
    if ((node->update & NODE_UPDATE) || (ntree->update & NTREE_UPDATE)) {
      if (node->typeinfo->updatefunc) {
        node->typeinfo->updatefunc(ntree, node);
      }

      nodeUpdateInternalLinks(ntree, node);
    }
  }

  /* generic tree update callback */
  if (ntree->typeinfo->update) {
    ntree->typeinfo->update(ntree);
  }
  /* XXX this should be moved into the tree type update callback for tree supporting node groups.
   * Currently the node tree interface is still a generic feature of the base NodeTree type.
   */
  if (ntree->update & NTREE_UPDATE_GROUP) {
    ntreeInterfaceTypeUpdate(ntree);
  }

  if (bmain) {
    ntreeUpdateAllUsers(bmain, &ntree->id);
  }

  if (ntree->update & (NTREE_UPDATE_LINKS | NTREE_UPDATE_NODES)) {
    /* node updates can change sockets or links, repeat link pointer update afterward */
    ntree_update_link_pointers(ntree);

    /* update the node level from link dependencies */
    ntree_update_node_level(ntree);

    /* check link validity */
    ntree_validate_links(ntree);
  }

  /* clear update flags */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->update = 0;
  }
  ntree->update = 0;

  ntree->is_updating = false;
}

void nodeUpdate(bNodeTree *ntree, bNode *node)
{
  /* Avoid re-entrant updates, can be caused by RNA update callbacks. */
  if (ntree->is_updating) {
    return;
  }
  ntree->is_updating = true;

  if (node->typeinfo->updatefunc) {
    node->typeinfo->updatefunc(ntree, node);
  }

  nodeUpdateInternalLinks(ntree, node);

  /* clear update flag */
  node->update = 0;

  ntree->is_updating = false;
}

bool nodeUpdateID(bNodeTree *ntree, ID *id)
{
  bool changed = false;

  if (ELEM(nullptr, id, ntree)) {
    return changed;
  }

  /* Avoid re-entrant updates, can be caused by RNA update callbacks. */
  if (ntree->is_updating) {
    return changed;
  }
  ntree->is_updating = true;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->id == id) {
      changed = true;
      node->update |= NODE_UPDATE_ID;
      if (node->typeinfo->updatefunc) {
        node->typeinfo->updatefunc(ntree, node);
      }
      /* clear update flag */
      node->update = 0;
    }
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    nodeUpdateInternalLinks(ntree, node);
  }

  ntree->is_updating = false;
  return changed;
}

void nodeUpdateInternalLinks(bNodeTree *ntree, bNode *node)
{
  BLI_freelistN(&node->internal_links);

  if (node->typeinfo && node->typeinfo->update_internal_links) {
    node->typeinfo->update_internal_links(ntree, node);
  }
}

/* ************* node type access ********** */

void nodeLabel(bNodeTree *ntree, bNode *node, char *label, int maxlen)
{
  label[0] = '\0';

  if (node->label[0] != '\0') {
    BLI_strncpy(label, node->label, maxlen);
  }
  else if (node->typeinfo->labelfunc) {
    node->typeinfo->labelfunc(ntree, node, label, maxlen);
  }

  /* The previous methods (labelfunc) could not provide an adequate label for the node. */
  if (label[0] == '\0') {
    /* Kind of hacky and weak... Ideally would be better to use RNA here. :| */
    const char *tmp = CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, node->typeinfo->ui_name);
    if (tmp == node->typeinfo->ui_name) {
      tmp = IFACE_(node->typeinfo->ui_name);
    }
    BLI_strncpy(label, tmp, maxlen);
  }
}

/* Get node socket label if it is set */
const char *nodeSocketLabel(const bNodeSocket *sock)
{
  return (sock->label[0] != '\0') ? sock->label : sock->name;
}

static void node_type_base_defaults(bNodeType *ntype)
{
  /* default size values */
  node_type_size_preset(ntype, NODE_SIZE_DEFAULT);
  ntype->height = 100;
  ntype->minheight = 30;
  ntype->maxheight = FLT_MAX;
}

/* allow this node for any tree type */
static bool node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *UNUSED(ntree))
{
  return true;
}

/* use the basic poll function */
static bool node_poll_instance_default(bNode *node, bNodeTree *ntree)
{
  return node->typeinfo->poll(node->typeinfo, ntree);
}

/* NOLINTNEXTLINE: readability-function-size */
void node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  /* Use static type info header to map static int type to identifier string and RNA struct type.
   * Associate the RNA struct type with the bNodeType.
   * Dynamically registered nodes will create an RNA type at runtime
   * and call RNA_struct_blender_type_set, so this only needs to be done for old RNA types
   * created in makesrna, which can not be associated to a bNodeType immediately,
   * since bNodeTypes are registered afterward ...
   */
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
  case ID: \
    BLI_strncpy(ntype->idname, #Category #StructName, sizeof(ntype->idname)); \
    ntype->rna_ext.srna = RNA_struct_find(#Category #StructName); \
    BLI_assert(ntype->rna_ext.srna != nullptr); \
    RNA_struct_blender_type_set(ntype->rna_ext.srna, ntype); \
    break;

  switch (type) {
#include "NOD_static_types.h"
  }

  /* make sure we have a valid type (everything registered) */
  BLI_assert(ntype->idname[0] != '\0');

  ntype->type = type;
  BLI_strncpy(ntype->ui_name, name, sizeof(ntype->ui_name));
  ntype->nclass = nclass;
  ntype->flag = flag;

  node_type_base_defaults(ntype);

  ntype->poll = node_poll_default;
  ntype->poll_instance = node_poll_instance_default;
}

void node_type_base_custom(
    bNodeType *ntype, const char *idname, const char *name, short nclass, short flag)
{
  BLI_strncpy(ntype->idname, idname, sizeof(ntype->idname));
  ntype->type = NODE_CUSTOM;
  BLI_strncpy(ntype->ui_name, name, sizeof(ntype->ui_name));
  ntype->nclass = nclass;
  ntype->flag = flag;

  node_type_base_defaults(ntype);
}

struct SocketTemplateIdentifierCallbackData {
  bNodeSocketTemplate *list;
  bNodeSocketTemplate *ntemp;
};

static bool unique_socket_template_identifier_check(void *arg, const char *name)
{
  SocketTemplateIdentifierCallbackData *data = (SocketTemplateIdentifierCallbackData *)arg;

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
                                              char delim)
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

void node_type_socket_templates(struct bNodeType *ntype,
                                struct bNodeSocketTemplate *inputs,
                                struct bNodeSocketTemplate *outputs)
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
      BLI_strncpy(ntemp->identifier, ntemp->name, sizeof(ntemp->identifier));
      unique_socket_template_identifier(inputs, ntemp, ntemp->identifier, '_');
    }
  }
  if (outputs) {
    /* clear identifier strings (uninitialized memory) */
    for (bNodeSocketTemplate *ntemp = outputs; ntemp->type >= 0; ntemp++) {
      ntemp->identifier[0] = '\0';
    }

    for (bNodeSocketTemplate *ntemp = outputs; ntemp->type >= 0; ntemp++) {
      BLI_strncpy(ntemp->identifier, ntemp->name, sizeof(ntemp->identifier));
      unique_socket_template_identifier(outputs, ntemp, ntemp->identifier, '_');
    }
  }
}

void node_type_init(struct bNodeType *ntype,
                    void (*initfunc)(struct bNodeTree *ntree, struct bNode *node))
{
  ntype->initfunc = initfunc;
}

void node_type_size(struct bNodeType *ntype, int width, int minwidth, int maxwidth)
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

void node_type_size_preset(struct bNodeType *ntype, eNodeSizePreset size)
{
  switch (size) {
    case NODE_SIZE_DEFAULT:
      node_type_size(ntype, 140, 100, NODE_DEFAULT_MAX_WIDTH);
      break;
    case NODE_SIZE_SMALL:
      node_type_size(ntype, 100, 80, NODE_DEFAULT_MAX_WIDTH);
      break;
    case NODE_SIZE_MIDDLE:
      node_type_size(ntype, 150, 120, NODE_DEFAULT_MAX_WIDTH);
      break;
    case NODE_SIZE_LARGE:
      node_type_size(ntype, 240, 140, NODE_DEFAULT_MAX_WIDTH);
      break;
  }
}

/**
 * \warning Nodes defining a storage type _must_ allocate this for new nodes.
 * Otherwise nodes will reload as undefined (T46619).
 */
void node_type_storage(bNodeType *ntype,
                       const char *storagename,
                       void (*freefunc)(struct bNode *node),
                       void (*copyfunc)(struct bNodeTree *dest_ntree,
                                        struct bNode *dest_node,
                                        const struct bNode *src_node))
{
  if (storagename) {
    BLI_strncpy(ntype->storagename, storagename, sizeof(ntype->storagename));
  }
  else {
    ntype->storagename[0] = '\0';
  }
  ntype->copyfunc = copyfunc;
  ntype->freefunc = freefunc;
}

void node_type_label(
    struct bNodeType *ntype,
    void (*labelfunc)(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen))
{
  ntype->labelfunc = labelfunc;
}

void node_type_update(struct bNodeType *ntype,
                      void (*updatefunc)(struct bNodeTree *ntree, struct bNode *node))
{
  ntype->updatefunc = updatefunc;
}

void node_type_group_update(struct bNodeType *ntype,
                            void (*group_update_func)(struct bNodeTree *ntree, struct bNode *node))
{
  ntype->group_update_func = group_update_func;
}

void node_type_exec(struct bNodeType *ntype,
                    NodeInitExecFunction init_exec_fn,
                    NodeFreeExecFunction free_exec_fn,
                    NodeExecFunction exec_fn)
{
  ntype->init_exec_fn = init_exec_fn;
  ntype->free_exec_fn = free_exec_fn;
  ntype->exec_fn = exec_fn;
}

void node_type_gpu(struct bNodeType *ntype, NodeGPUExecFunction gpu_fn)
{
  ntype->gpu_fn = gpu_fn;
}

void node_type_internal_links(bNodeType *ntype,
                              void (*update_internal_links)(bNodeTree *, bNode *))
{
  ntype->update_internal_links = update_internal_links;
}

/* callbacks for undefined types */

static bool node_undefined_poll(bNodeType *UNUSED(ntype), bNodeTree *UNUSED(nodetree))
{
  /* this type can not be added deliberately, it's just a placeholder */
  return false;
}

/* register fallback types used for undefined tree, nodes, sockets */
static void register_undefined_types()
{
  /* Note: these types are not registered in the type hashes,
   * they are just used as placeholders in case the actual types are not registered.
   */

  strcpy(NodeTreeTypeUndefined.idname, "NodeTreeUndefined");
  strcpy(NodeTreeTypeUndefined.ui_name, N_("Undefined"));
  strcpy(NodeTreeTypeUndefined.ui_description, N_("Undefined Node Tree Type"));

  node_type_base_custom(&NodeTypeUndefined, "NodeUndefined", "Undefined", 0, 0);
  NodeTypeUndefined.poll = node_undefined_poll;

  BLI_strncpy(NodeSocketTypeUndefined.idname,
              "NodeSocketUndefined",
              sizeof(NodeSocketTypeUndefined.idname));
  /* extra type info for standard socket types */
  NodeSocketTypeUndefined.type = SOCK_CUSTOM;
  NodeSocketTypeUndefined.subtype = PROP_NONE;

  NodeSocketTypeUndefined.use_link_limits_of_type = true;
  NodeSocketTypeUndefined.input_link_limit = 0xFFF;
  NodeSocketTypeUndefined.output_link_limit = 0xFFF;
}

static void registerCompositNodes()
{
  register_node_type_cmp_group();

  register_node_type_cmp_rlayers();
  register_node_type_cmp_image();
  register_node_type_cmp_texture();
  register_node_type_cmp_value();
  register_node_type_cmp_rgb();
  register_node_type_cmp_curve_time();
  register_node_type_cmp_movieclip();

  register_node_type_cmp_composite();
  register_node_type_cmp_viewer();
  register_node_type_cmp_splitviewer();
  register_node_type_cmp_output_file();
  register_node_type_cmp_view_levels();

  register_node_type_cmp_curve_rgb();
  register_node_type_cmp_mix_rgb();
  register_node_type_cmp_hue_sat();
  register_node_type_cmp_brightcontrast();
  register_node_type_cmp_gamma();
  register_node_type_cmp_exposure();
  register_node_type_cmp_invert();
  register_node_type_cmp_alphaover();
  register_node_type_cmp_zcombine();
  register_node_type_cmp_colorbalance();
  register_node_type_cmp_huecorrect();

  register_node_type_cmp_normal();
  register_node_type_cmp_curve_vec();
  register_node_type_cmp_map_value();
  register_node_type_cmp_map_range();
  register_node_type_cmp_normalize();

  register_node_type_cmp_filter();
  register_node_type_cmp_blur();
  register_node_type_cmp_dblur();
  register_node_type_cmp_bilateralblur();
  register_node_type_cmp_vecblur();
  register_node_type_cmp_dilateerode();
  register_node_type_cmp_inpaint();
  register_node_type_cmp_despeckle();
  register_node_type_cmp_defocus();
  register_node_type_cmp_sunbeams();
  register_node_type_cmp_denoise();

  register_node_type_cmp_valtorgb();
  register_node_type_cmp_rgbtobw();
  register_node_type_cmp_setalpha();
  register_node_type_cmp_idmask();
  register_node_type_cmp_math();
  register_node_type_cmp_seprgba();
  register_node_type_cmp_combrgba();
  register_node_type_cmp_sephsva();
  register_node_type_cmp_combhsva();
  register_node_type_cmp_sepyuva();
  register_node_type_cmp_combyuva();
  register_node_type_cmp_sepycca();
  register_node_type_cmp_combycca();
  register_node_type_cmp_premulkey();

  register_node_type_cmp_diff_matte();
  register_node_type_cmp_distance_matte();
  register_node_type_cmp_chroma_matte();
  register_node_type_cmp_color_matte();
  register_node_type_cmp_channel_matte();
  register_node_type_cmp_color_spill();
  register_node_type_cmp_luma_matte();
  register_node_type_cmp_doubleedgemask();
  register_node_type_cmp_keyingscreen();
  register_node_type_cmp_keying();
  register_node_type_cmp_cryptomatte();

  register_node_type_cmp_translate();
  register_node_type_cmp_rotate();
  register_node_type_cmp_scale();
  register_node_type_cmp_flip();
  register_node_type_cmp_crop();
  register_node_type_cmp_displace();
  register_node_type_cmp_mapuv();
  register_node_type_cmp_glare();
  register_node_type_cmp_tonemap();
  register_node_type_cmp_lensdist();
  register_node_type_cmp_transform();
  register_node_type_cmp_stabilize2d();
  register_node_type_cmp_moviedistortion();

  register_node_type_cmp_colorcorrection();
  register_node_type_cmp_boxmask();
  register_node_type_cmp_ellipsemask();
  register_node_type_cmp_bokehimage();
  register_node_type_cmp_bokehblur();
  register_node_type_cmp_switch();
  register_node_type_cmp_switch_view();
  register_node_type_cmp_pixelate();

  register_node_type_cmp_mask();
  register_node_type_cmp_trackpos();
  register_node_type_cmp_planetrackdeform();
  register_node_type_cmp_cornerpin();
}

static void registerShaderNodes()
{
  register_node_type_sh_group();

  register_node_type_sh_camera();
  register_node_type_sh_gamma();
  register_node_type_sh_brightcontrast();
  register_node_type_sh_value();
  register_node_type_sh_rgb();
  register_node_type_sh_wireframe();
  register_node_type_sh_wavelength();
  register_node_type_sh_blackbody();
  register_node_type_sh_mix_rgb();
  register_node_type_sh_valtorgb();
  register_node_type_sh_rgbtobw();
  register_node_type_sh_shadertorgb();
  register_node_type_sh_normal();
  register_node_type_sh_mapping();
  register_node_type_sh_curve_vec();
  register_node_type_sh_curve_rgb();
  register_node_type_sh_map_range();
  register_node_type_sh_clamp();
  register_node_type_sh_math();
  register_node_type_sh_vect_math();
  register_node_type_sh_vector_rotate();
  register_node_type_sh_vect_transform();
  register_node_type_sh_squeeze();
  register_node_type_sh_invert();
  register_node_type_sh_seprgb();
  register_node_type_sh_combrgb();
  register_node_type_sh_sephsv();
  register_node_type_sh_combhsv();
  register_node_type_sh_sepxyz();
  register_node_type_sh_combxyz();
  register_node_type_sh_hue_sat();

  register_node_type_sh_attribute();
  register_node_type_sh_bevel();
  register_node_type_sh_displacement();
  register_node_type_sh_vector_displacement();
  register_node_type_sh_geometry();
  register_node_type_sh_light_path();
  register_node_type_sh_light_falloff();
  register_node_type_sh_object_info();
  register_node_type_sh_fresnel();
  register_node_type_sh_layer_weight();
  register_node_type_sh_tex_coord();
  register_node_type_sh_particle_info();
  register_node_type_sh_bump();
  register_node_type_sh_vertex_color();

  register_node_type_sh_background();
  register_node_type_sh_bsdf_anisotropic();
  register_node_type_sh_bsdf_diffuse();
  register_node_type_sh_bsdf_principled();
  register_node_type_sh_bsdf_glossy();
  register_node_type_sh_bsdf_glass();
  register_node_type_sh_bsdf_translucent();
  register_node_type_sh_bsdf_transparent();
  register_node_type_sh_bsdf_velvet();
  register_node_type_sh_bsdf_toon();
  register_node_type_sh_bsdf_hair();
  register_node_type_sh_bsdf_hair_principled();
  register_node_type_sh_emission();
  register_node_type_sh_holdout();
  register_node_type_sh_volume_absorption();
  register_node_type_sh_volume_scatter();
  register_node_type_sh_volume_principled();
  register_node_type_sh_subsurface_scattering();
  register_node_type_sh_mix_shader();
  register_node_type_sh_add_shader();
  register_node_type_sh_uvmap();
  register_node_type_sh_uvalongstroke();
  register_node_type_sh_eevee_specular();

  register_node_type_sh_output_light();
  register_node_type_sh_output_material();
  register_node_type_sh_output_world();
  register_node_type_sh_output_linestyle();
  register_node_type_sh_output_aov();

  register_node_type_sh_tex_image();
  register_node_type_sh_tex_environment();
  register_node_type_sh_tex_sky();
  register_node_type_sh_tex_noise();
  register_node_type_sh_tex_wave();
  register_node_type_sh_tex_voronoi();
  register_node_type_sh_tex_musgrave();
  register_node_type_sh_tex_gradient();
  register_node_type_sh_tex_magic();
  register_node_type_sh_tex_checker();
  register_node_type_sh_tex_brick();
  register_node_type_sh_tex_pointdensity();
  register_node_type_sh_tex_ies();
  register_node_type_sh_tex_white_noise();
}

static void registerTextureNodes()
{
  register_node_type_tex_group();

  register_node_type_tex_math();
  register_node_type_tex_mix_rgb();
  register_node_type_tex_valtorgb();
  register_node_type_tex_rgbtobw();
  register_node_type_tex_valtonor();
  register_node_type_tex_curve_rgb();
  register_node_type_tex_curve_time();
  register_node_type_tex_invert();
  register_node_type_tex_hue_sat();
  register_node_type_tex_coord();
  register_node_type_tex_distance();
  register_node_type_tex_compose();
  register_node_type_tex_decompose();

  register_node_type_tex_output();
  register_node_type_tex_viewer();
  register_node_type_sh_script();
  register_node_type_sh_tangent();
  register_node_type_sh_normal_map();
  register_node_type_sh_hair_info();
  register_node_type_sh_volume_info();

  register_node_type_tex_checker();
  register_node_type_tex_texture();
  register_node_type_tex_bricks();
  register_node_type_tex_image();
  register_node_type_sh_bsdf_refraction();
  register_node_type_sh_ambient_occlusion();

  register_node_type_tex_rotate();
  register_node_type_tex_translate();
  register_node_type_tex_scale();
  register_node_type_tex_at();

  register_node_type_tex_proc_voronoi();
  register_node_type_tex_proc_blend();
  register_node_type_tex_proc_magic();
  register_node_type_tex_proc_marble();
  register_node_type_tex_proc_clouds();
  register_node_type_tex_proc_wood();
  register_node_type_tex_proc_musgrave();
  register_node_type_tex_proc_noise();
  register_node_type_tex_proc_stucci();
  register_node_type_tex_proc_distnoise();
}

static void registerGeometryNodes()
{
  register_node_type_geo_group();

  register_node_type_geo_align_rotation_to_vector();
  register_node_type_geo_attribute_color_ramp();
  register_node_type_geo_attribute_combine_xyz();
  register_node_type_geo_attribute_compare();
  register_node_type_geo_attribute_fill();
  register_node_type_geo_attribute_math();
  register_node_type_geo_attribute_mix();
  register_node_type_geo_attribute_proximity();
  register_node_type_geo_attribute_randomize();
  register_node_type_geo_attribute_separate_xyz();
  register_node_type_geo_attribute_vector_math();
  register_node_type_geo_attribute_remove();
  register_node_type_geo_boolean();
  register_node_type_geo_collection_info();
  register_node_type_geo_edge_split();
  register_node_type_geo_is_viewport();
  register_node_type_geo_join_geometry();
  register_node_type_geo_object_info();
  register_node_type_geo_point_distribute();
  register_node_type_geo_point_instance();
  register_node_type_geo_point_rotate();
  register_node_type_geo_point_scale();
  register_node_type_geo_point_separate();
  register_node_type_geo_point_translate();
  register_node_type_geo_points_to_volume();
  register_node_type_geo_sample_texture();
  register_node_type_geo_subdivide_smooth();
  register_node_type_geo_subdivide();
  register_node_type_geo_transform();
  register_node_type_geo_triangulate();
  register_node_type_geo_volume_to_mesh();
}

static void registerFunctionNodes()
{
  register_node_type_fn_boolean_math();
  register_node_type_fn_combine_strings();
  register_node_type_fn_float_compare();
  register_node_type_fn_group_instance_id();
  register_node_type_fn_input_string();
  register_node_type_fn_input_vector();
  register_node_type_fn_object_transforms();
  register_node_type_fn_random_float();
  register_node_type_fn_switch();
}

void BKE_node_system_init(void)
{
  nodetreetypes_hash = BLI_ghash_str_new("nodetreetypes_hash gh");
  nodetypes_hash = BLI_ghash_str_new("nodetypes_hash gh");
  nodesockettypes_hash = BLI_ghash_str_new("nodesockettypes_hash gh");

  register_undefined_types();

  register_standard_node_socket_types();

  register_node_tree_type_cmp();
  register_node_tree_type_sh();
  register_node_tree_type_tex();
  register_node_tree_type_geo();

  register_node_type_frame();
  register_node_type_reroute();
  register_node_type_group_input();
  register_node_type_group_output();

  registerCompositNodes();
  registerShaderNodes();
  registerTextureNodes();
  registerGeometryNodes();
  registerFunctionNodes();
}

void BKE_node_system_exit(void)
{
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

void BKE_node_tree_iter_init(struct NodeTreeIterStore *ntreeiter, struct Main *bmain)
{
  ntreeiter->ngroup = (bNodeTree *)bmain->nodetrees.first;
  ntreeiter->scene = (Scene *)bmain->scenes.first;
  ntreeiter->mat = (Material *)bmain->materials.first;
  ntreeiter->tex = (Tex *)bmain->textures.first;
  ntreeiter->light = (Light *)bmain->lights.first;
  ntreeiter->world = (World *)bmain->worlds.first;
  ntreeiter->linestyle = (FreestyleLineStyle *)bmain->linestyles.first;
  ntreeiter->simulation = (Simulation *)bmain->simulations.first;
}
bool BKE_node_tree_iter_step(struct NodeTreeIterStore *ntreeiter,
                             bNodeTree **r_nodetree,
                             struct ID **r_id)
{
  if (ntreeiter->ngroup) {
    *r_nodetree = (bNodeTree *)ntreeiter->ngroup;
    *r_id = (ID *)ntreeiter->ngroup;
    ntreeiter->ngroup = (bNodeTree *)ntreeiter->ngroup->id.next;
  }
  else if (ntreeiter->scene) {
    *r_nodetree = (bNodeTree *)ntreeiter->scene->nodetree;
    *r_id = (ID *)ntreeiter->scene;
    ntreeiter->scene = (Scene *)ntreeiter->scene->id.next;
  }
  else if (ntreeiter->mat) {
    *r_nodetree = (bNodeTree *)ntreeiter->mat->nodetree;
    *r_id = (ID *)ntreeiter->mat;
    ntreeiter->mat = (Material *)ntreeiter->mat->id.next;
  }
  else if (ntreeiter->tex) {
    *r_nodetree = (bNodeTree *)ntreeiter->tex->nodetree;
    *r_id = (ID *)ntreeiter->tex;
    ntreeiter->tex = (Tex *)ntreeiter->tex->id.next;
  }
  else if (ntreeiter->light) {
    *r_nodetree = (bNodeTree *)ntreeiter->light->nodetree;
    *r_id = (ID *)ntreeiter->light;
    ntreeiter->light = (Light *)ntreeiter->light->id.next;
  }
  else if (ntreeiter->world) {
    *r_nodetree = (bNodeTree *)ntreeiter->world->nodetree;
    *r_id = (ID *)ntreeiter->world;
    ntreeiter->world = (World *)ntreeiter->world->id.next;
  }
  else if (ntreeiter->linestyle) {
    *r_nodetree = (bNodeTree *)ntreeiter->linestyle->nodetree;
    *r_id = (ID *)ntreeiter->linestyle;
    ntreeiter->linestyle = (FreestyleLineStyle *)ntreeiter->linestyle->id.next;
  }
  else if (ntreeiter->simulation) {
    *r_nodetree = (bNodeTree *)ntreeiter->simulation->nodetree;
    *r_id = (ID *)ntreeiter->simulation;
    ntreeiter->simulation = (Simulation *)ntreeiter->simulation->id.next;
  }
  else {
    return false;
  }

  return true;
}

/* -------------------------------------------------------------------- */
/* NodeTree kernel functions */

void BKE_nodetree_remove_layer_n(bNodeTree *ntree, Scene *scene, const int layer_index)
{
  BLI_assert(layer_index != -1);
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == CMP_NODE_R_LAYERS && (Scene *)node->id == scene) {
      if (node->custom1 == layer_index) {
        node->custom1 = 0;
      }
      else if (node->custom1 > layer_index) {
        node->custom1--;
      }
    }
  }
}
