/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

#include "BLI_assert.h"
#include "BLI_dynstr.h"
#include "BLI_hash_mm3.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_cryptomatte.hh"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"

#include "COM_node_operation.hh"

#include <optional>

/* -------------------------------------------------------------------- */
/** \name Cryptomatte
 * \{ */

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_render(
    const bNode &node, const bool use_meta_data)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;

  Scene *scene = (Scene *)node.id;
  if (!scene) {
    return session;
  }
  BLI_assert(GS(scene->id.name) == ID_SCE);

  if (use_meta_data) {
    Render *render = RE_GetSceneRender(scene);
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
  return session;
}

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_image(
    const Scene &scene, const bNode &node)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;
  Image *image = (Image *)node.id;
  if (!image) {
    return session;
  }
  BLI_assert(GS(image->id.name) == ID_IM);

  NodeCryptomatte *node_cryptomatte = static_cast<NodeCryptomatte *>(node.storage);
  ImageUser *iuser = &node_cryptomatte->iuser;
  BKE_image_user_frame_calc(image, iuser, scene.r.cfra);
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, nullptr);
  RenderResult *render_result = image->rr;
  if (render_result) {
    session = blender::bke::cryptomatte::CryptomatteSessionPtr(
        BKE_cryptomatte_init_from_render_result(render_result));
  }
  BKE_image_release_ibuf(image, ibuf, nullptr);
  return session;
}

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node(
    const Scene &scene, const bNode &node, const bool use_meta_data)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;
  if (node.type != CMP_NODE_CRYPTOMATTE) {
    return session;
  }

  switch (node.custom1) {
    case CMP_CRYPTOMATTE_SRC_RENDER: {
      return cryptomatte_init_from_node_render(node, use_meta_data);
    }

    case CMP_CRYPTOMATTE_SRC_IMAGE: {
      return cryptomatte_init_from_node_image(scene, node);
    }
  }
  return session;
}

static CryptomatteEntry *cryptomatte_find(const NodeCryptomatte &n, float encoded_hash)
{
  LISTBASE_FOREACH (CryptomatteEntry *, entry, &n.entries) {
    if (entry->encoded_hash == encoded_hash) {
      return entry;
    }
  }
  return nullptr;
}

static void cryptomatte_add(const Scene &scene,
                            bNode &node,
                            NodeCryptomatte &node_cryptomatte,
                            float encoded_hash)
{
  /* Check if entry already exist. */
  if (cryptomatte_find(node_cryptomatte, encoded_hash)) {
    return;
  }

  CryptomatteEntry *entry = MEM_cnew<CryptomatteEntry>(__func__);
  entry->encoded_hash = encoded_hash;
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      scene, node, true);
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

void ntreeCompositCryptomatteSyncFromAdd(const Scene *scene, bNode *node)
{
  BLI_assert(ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.add[0] != 0.0f) {
    cryptomatte_add(*scene, *node, *n, n->runtime.add[0]);
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
void ntreeCompositCryptomatteUpdateLayerNames(const Scene *scene, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  BLI_freelistN(&n->runtime.layers);

  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      *scene, *node, false);

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session))
    {
      CryptomatteLayer *layer = MEM_cnew<CryptomatteLayer>(__func__);
      layer_name.copy(layer->name);
      BLI_addtail(&n->runtime.layers, layer);
    }
  }
}

void ntreeCompositCryptomatteLayerPrefix(const Scene *scene,
                                         const bNode *node,
                                         char *r_prefix,
                                         size_t prefix_maxncpy)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *node_cryptomatte = (NodeCryptomatte *)node->storage;
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      *scene, *node, false);
  std::string first_layer_name;

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session))
    {
      if (first_layer_name.empty()) {
        first_layer_name = layer_name;
      }

      if (layer_name == node_cryptomatte->layer_name) {
        BLI_strncpy(r_prefix, node_cryptomatte->layer_name, prefix_maxncpy);
        return;
      }
    }
  }

  const char *cstr = first_layer_name.c_str();
  BLI_strncpy(r_prefix, cstr, prefix_maxncpy);
}

CryptomatteSession *ntreeCompositCryptomatteSession(const Scene *scene, bNode *node)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session_ptr = cryptomatte_init_from_node(
      *scene, *node, true);
  return session_ptr.release();
}

namespace blender::nodes::node_composite_cryptomatte_cc {

static bNodeSocketTemplate cmp_node_cryptomatte_in[] = {
    {SOCK_RGBA, N_("Image"), 0.0f, 0.0f, 0.0f, 1.0f}, {-1, ""}};

static bNodeSocketTemplate cmp_node_cryptomatte_out[] = {
    {SOCK_RGBA, N_("Image")},
    {SOCK_FLOAT, N_("Matte")},
    {SOCK_RGBA, N_("Pick")},
    {-1, ""},
};

static void node_init_cryptomatte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeCryptomatte *user = MEM_cnew<NodeCryptomatte>(__func__);
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
    MEM_SAFE_FREE(nc->matte_id);
    BLI_freelistN(&nc->runtime.layers);
    BLI_freelistN(&nc->entries);
    MEM_freeN(nc);
  }
}

static void node_copy_cryptomatte(bNodeTree * /*dst_ntree*/,
                                  bNode *dest_node,
                                  const bNode *src_node)
{
  NodeCryptomatte *src_nc = static_cast<NodeCryptomatte *>(src_node->storage);
  NodeCryptomatte *dest_nc = static_cast<NodeCryptomatte *>(MEM_dupallocN(src_nc));

  BLI_duplicatelist(&dest_nc->entries, &src_nc->entries);
  BLI_listbase_clear(&dest_nc->runtime.layers);
  dest_nc->matte_id = static_cast<char *>(MEM_dupallocN(src_nc->matte_id));
  dest_node->storage = dest_nc;
}

static bool node_poll_cryptomatte(const bNodeType * /*ntype*/,
                                  const bNodeTree *ntree,
                                  const char **r_disabled_hint)
{
  if (STREQ(ntree->idname, "CompositorNodeTree")) {
    Scene *scene;

    /* See node_composit_poll_rlayers. */
    for (scene = static_cast<Scene *>(G.main->scenes.first); scene;
         scene = static_cast<Scene *>(scene->id.next))
    {
      if (scene->nodetree == ntree) {
        break;
      }
    }

    if (scene == nullptr) {
      *r_disabled_hint = TIP_(
          "The node tree must be the compositing node tree of any scene in the file");
    }
    return scene != nullptr;
  }
  *r_disabled_hint = TIP_("Not a compositor node tree");
  return false;
}

using namespace blender::realtime_compositor;

class CryptoMatteOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
    get_result("Matte").allocate_invalid();
    get_result("Pick").allocate_invalid();
    context().set_info_message("Viewport compositor setup not fully supported");
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CryptoMatteOperation(context, node);
}

}  // namespace blender::nodes::node_composite_cryptomatte_cc

void register_node_type_cmp_cryptomatte()
{
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CRYPTOMATTE, "Cryptomatte", NODE_CLASS_MATTE);
  blender::bke::node_type_socket_templates(
      &ntype, file_ns::cmp_node_cryptomatte_in, file_ns::cmp_node_cryptomatte_out);
  blender::bke::node_type_size(&ntype, 240, 100, 700);
  ntype.initfunc = file_ns::node_init_cryptomatte;
  ntype.initfunc_api = file_ns::node_init_api_cryptomatte;
  ntype.poll = file_ns::node_poll_cryptomatte;
  node_type_storage(
      &ntype, "NodeCryptomatte", file_ns::node_free_cryptomatte, file_ns::node_copy_cryptomatte);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Legacy
 * \{ */

bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  char sockname[32];
  n->inputs_num++;
  SNPRINTF(sockname, "Crypto %.2d", n->inputs_num - 1);
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, nullptr, sockname);
  return sock;
}

int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->inputs_num < 2) {
    return 0;
  }
  bNodeSocket *sock = static_cast<bNodeSocket *>(node->inputs.last);
  nodeRemoveSocket(ntree, node, sock);
  n->inputs_num--;
  return 1;
}

namespace blender::nodes::node_composite_legacy_cryptomatte_cc {

static void node_init_cryptomatte_legacy(bNodeTree *ntree, bNode *node)
{
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;
  file_ns::node_init_cryptomatte(ntree, node);

  nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "image", "Image");

  /* Add three inputs by default, as recommended by the Cryptomatte specification. */
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
}

using namespace blender::realtime_compositor;

class CryptoMatteOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("image").pass_through(get_result("Image"));
    get_result("Matte").allocate_invalid();
    get_result("Pick").allocate_invalid();
    context().set_info_message("Viewport compositor setup not fully supported");
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CryptoMatteOperation(context, node);
}

}  // namespace blender::nodes::node_composite_legacy_cryptomatte_cc

void register_node_type_cmp_cryptomatte_legacy()
{
  namespace legacy_file_ns = blender::nodes::node_composite_legacy_cryptomatte_cc;
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;

  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_CRYPTOMATTE_LEGACY, "Cryptomatte (Legacy)", NODE_CLASS_MATTE);
  blender::bke::node_type_socket_templates(&ntype, nullptr, file_ns::cmp_node_cryptomatte_out);
  ntype.initfunc = legacy_file_ns::node_init_cryptomatte_legacy;
  node_type_storage(
      &ntype, "NodeCryptomatte", file_ns::node_free_cryptomatte, file_ns::node_copy_cryptomatte);
  ntype.gather_link_search_ops = nullptr;
  ntype.gather_add_node_search_ops = nullptr;
  ntype.get_compositor_operation = legacy_file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}

/** \} */
