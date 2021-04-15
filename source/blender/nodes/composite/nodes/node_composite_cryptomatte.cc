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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.h"

#include "BLI_assert.h"
#include "BLI_dynstr.h"
#include "BLI_hash_mm3.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_cryptomatte.hh"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include <optional>

/** \name Cryptomatte
 * \{ */

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node(
    const bNode &node, const int frame_number, const bool use_meta_data)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;
  if (node.type != CMP_NODE_CRYPTOMATTE) {
    return session;
  }

  NodeCryptomatte *node_cryptomatte = static_cast<NodeCryptomatte *>(node.storage);
  switch (node.custom1) {
    case CMP_CRYPTOMATTE_SRC_RENDER: {
      Scene *scene = (Scene *)node.id;
      if (!scene) {
        return session;
      }
      BLI_assert(GS(scene->id.name) == ID_SCE);

      if (use_meta_data) {
        Render *render = (scene) ? RE_GetSceneRender(scene) : nullptr;
        RenderResult *render_result = render ? RE_AcquireResultRead(render) : nullptr;
        if (render_result) {
          session = blender::bke::cryptomatte::CryptomatteSessionPtr(
              BKE_cryptomatte_init_from_render_result(render_result));
        }
        if (render) {
          RE_ReleaseResult(render);
        }
      }

      if (session == nullptr) {
        session = blender::bke::cryptomatte::CryptomatteSessionPtr(
            BKE_cryptomatte_init_from_scene(scene));
      }

      break;
    }

    case CMP_CRYPTOMATTE_SRC_IMAGE: {
      Image *image = (Image *)node.id;
      if (!image) {
        break;
      }
      BLI_assert(GS(image->id.name) == ID_IM);

      ImageUser *iuser = &node_cryptomatte->iuser;
      BKE_image_user_frame_calc(image, iuser, frame_number);
      ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, nullptr);
      RenderResult *render_result = image->rr;
      if (render_result) {
        session = blender::bke::cryptomatte::CryptomatteSessionPtr(
            BKE_cryptomatte_init_from_render_result(render_result));
      }
      BKE_image_release_ibuf(image, ibuf, nullptr);
      break;
    }
  }
  return session;
}

extern "C" {
static CryptomatteEntry *cryptomatte_find(const NodeCryptomatte &n, float encoded_hash)
{
  LISTBASE_FOREACH (CryptomatteEntry *, entry, &n.entries) {
    if (entry->encoded_hash == encoded_hash) {
      return entry;
    }
  }
  return nullptr;
}

static void cryptomatte_add(bNode &node, NodeCryptomatte &node_cryptomatte, float encoded_hash)
{
  /* Check if entry already exist. */
  if (cryptomatte_find(node_cryptomatte, encoded_hash)) {
    return;
  }

  CryptomatteEntry *entry = static_cast<CryptomatteEntry *>(
      MEM_callocN(sizeof(CryptomatteEntry), __func__));
  entry->encoded_hash = encoded_hash;
  /* TODO(jbakker): Get current frame from scene. */
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      node, 0, true);
  if (session) {
    BKE_cryptomatte_find_name(session.get(), encoded_hash, entry->name, sizeof(entry->name));
  }

  BLI_addtail(&node_cryptomatte.entries, entry);
}

static void cryptomatte_remove(NodeCryptomatte &n, float encoded_hash)
{
  CryptomatteEntry *entry = cryptomatte_find(n, encoded_hash);
  if (!entry) {
    return;
  }
  BLI_remlink(&n.entries, entry);
  MEM_freeN(entry);
}

static bNodeSocketTemplate cmp_node_cryptomatte_in[] = {
    {SOCK_RGBA, N_("Image"), 0.0f, 0.0f, 0.0f, 1.0f}, {-1, ""}};

static bNodeSocketTemplate cmp_node_cryptomatte_out[] = {
    {SOCK_RGBA, N_("Image")},
    {SOCK_FLOAT, N_("Matte")},
    {SOCK_RGBA, N_("Pick")},
    {-1, ""},
};

void ntreeCompositCryptomatteSyncFromAdd(bNode *node)
{
  BLI_assert(ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.add[0] != 0.0f) {
    cryptomatte_add(*node, *n, n->runtime.add[0]);
    zero_v3(n->runtime.add);
  }
}

void ntreeCompositCryptomatteSyncFromRemove(bNode *node)
{
  BLI_assert(ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.remove[0] != 0.0f) {
    cryptomatte_remove(*n, n->runtime.remove[0]);
    zero_v3(n->runtime.remove);
  }
}
void ntreeCompositCryptomatteUpdateLayerNames(bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  BLI_freelistN(&n->runtime.layers);

  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      *node, 0, false);

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session)) {
      CryptomatteLayer *layer = static_cast<CryptomatteLayer *>(
          MEM_callocN(sizeof(CryptomatteLayer), __func__));
      layer_name.copy(layer->name);
      BLI_addtail(&n->runtime.layers, layer);
    }
  }
}

void ntreeCompositCryptomatteLayerPrefix(const bNode *node, char *r_prefix, size_t prefix_len)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *node_cryptomatte = (NodeCryptomatte *)node->storage;
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      *node, 0, false);
  std::string first_layer_name;

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session)) {
      if (first_layer_name.empty()) {
        first_layer_name = layer_name;
      }

      if (layer_name == node_cryptomatte->layer_name) {
        BLI_strncpy(r_prefix, node_cryptomatte->layer_name, prefix_len);
        return;
      }
    }
  }

  const char *cstr = first_layer_name.c_str();
  BLI_strncpy(r_prefix, cstr, prefix_len);
}

CryptomatteSession *ntreeCompositCryptomatteSession(bNode *node)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session_ptr = cryptomatte_init_from_node(
      *node, 0, true);
  return session_ptr.release();
}

static void node_init_cryptomatte(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeCryptomatte *user = static_cast<NodeCryptomatte *>(
      MEM_callocN(sizeof(NodeCryptomatte), __func__));
  node->storage = user;
}

static void node_init_api_cryptomatte(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = static_cast<bNode *>(ptr->data);
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  node->id = &scene->id;
  id_us_plus(node->id);
}

static void node_free_cryptomatte(bNode *node)
{
  BLI_assert(ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);

  if (nc) {
    BLI_freelistN(&nc->runtime.layers);
    BLI_freelistN(&nc->entries);
    MEM_freeN(nc);
  }
}

static void node_copy_cryptomatte(bNodeTree *UNUSED(dest_ntree),
                                  bNode *dest_node,
                                  const bNode *src_node)
{
  NodeCryptomatte *src_nc = static_cast<NodeCryptomatte *>(src_node->storage);
  NodeCryptomatte *dest_nc = static_cast<NodeCryptomatte *>(MEM_dupallocN(src_nc));

  BLI_duplicatelist(&dest_nc->entries, &src_nc->entries);
  BLI_listbase_clear(&dest_nc->runtime.layers);
  dest_node->storage = dest_nc;
}

static bool node_poll_cryptomatte(bNodeType *UNUSED(ntype),
                                  bNodeTree *ntree,
                                  const char **r_disabled_hint)
{
  if (STREQ(ntree->idname, "CompositorNodeTree")) {
    Scene *scene;

    /* See node_composit_poll_rlayers. */
    for (scene = static_cast<Scene *>(G.main->scenes.first); scene;
         scene = static_cast<Scene *>(scene->id.next)) {
      if (scene->nodetree == ntree) {
        break;
      }
    }

    if (scene == nullptr) {
      *r_disabled_hint =
          "The node tree must be the compositing node tree of any scene in the file";
    }
    return scene != nullptr;
  }
  *r_disabled_hint = "Not a compositor node tree";
  return false;
}

void register_node_type_cmp_cryptomatte(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CRYPTOMATTE, "Cryptomatte", NODE_CLASS_MATTE, 0);
  node_type_socket_templates(&ntype, cmp_node_cryptomatte_in, cmp_node_cryptomatte_out);
  node_type_size(&ntype, 240, 100, 700);
  node_type_init(&ntype, node_init_cryptomatte);
  ntype.initfunc_api = node_init_api_cryptomatte;
  ntype.poll = node_poll_cryptomatte;
  node_type_storage(&ntype, "NodeCryptomatte", node_free_cryptomatte, node_copy_cryptomatte);
  nodeRegisterType(&ntype);
}

/** \} */

/** \name Cryptomatte Legacy
 * \{ */
static void node_init_cryptomatte_legacy(bNodeTree *ntree, bNode *node)
{
  node_init_cryptomatte(ntree, node);

  nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "image", "Image");

  /* Add three inputs by default, as recommended by the Cryptomatte specification. */
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
}

bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  char sockname[32];
  n->num_inputs++;
  BLI_snprintf(sockname, sizeof(sockname), "Crypto %.2d", n->num_inputs - 1);
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, nullptr, sockname);
  return sock;
}

int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->num_inputs < 2) {
    return 0;
  }
  bNodeSocket *sock = static_cast<bNodeSocket *>(node->inputs.last);
  nodeRemoveSocket(ntree, node, sock);
  n->num_inputs--;
  return 1;
}

void register_node_type_cmp_cryptomatte_legacy(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CRYPTOMATTE_LEGACY, "Cryptomatte", NODE_CLASS_MATTE, 0);
  node_type_socket_templates(&ntype, nullptr, cmp_node_cryptomatte_out);
  node_type_init(&ntype, node_init_cryptomatte_legacy);
  node_type_storage(&ntype, "NodeCryptomatte", node_free_cryptomatte, node_copy_cryptomatte);
  nodeRegisterType(&ntype);
}

/** \} */
}
