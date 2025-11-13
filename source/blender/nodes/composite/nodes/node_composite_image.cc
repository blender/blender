/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_composite_util.hh"

#include "BLI_assert.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_image.hh"
#include "BKE_main.hh"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "UI_interface.hh"

#include "COM_algorithm_extract_alpha.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::nodes::node_composite_image_cc {

static void cmp_node_image_add_pass_output(bNodeTree *ntree,
                                           bNode *node,
                                           const char *name,
                                           const char *passname,
                                           eNodeSocketDatatype type,
                                           LinkNodePair *available_sockets,
                                           int *prev_index)
{
  bNodeSocket *sock = (bNodeSocket *)BLI_findstring(
      &node->outputs, name, offsetof(bNodeSocket, name));

  /* Replace if types don't match. */
  if (sock && sock->type != type) {
    blender::bke::node_remove_socket(*ntree, *node, *sock);
    sock = nullptr;
  }

  /* Create socket if it doesn't exist yet. */
  if (sock == nullptr) {
    sock = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_OUT, type, PROP_NONE, name, name);
    /* extra socket info */
    NodeImageLayer *sockdata = MEM_callocN<NodeImageLayer>(__func__);
    sock->storage = sockdata;
  }

  NodeImageLayer *sockdata = (NodeImageLayer *)sock->storage;
  if (sockdata) {
    STRNCPY_UTF8(sockdata->pass_name, passname);
  }

  /* Reorder sockets according to order that passes are added. */
  const int after_index = (*prev_index)++;
  bNodeSocket *after_sock = (bNodeSocket *)BLI_findlink(&node->outputs, after_index);
  BLI_remlink(&node->outputs, sock);
  BLI_insertlinkafter(&node->outputs, after_sock, sock);

  BLI_linklist_append(available_sockets, sock);
}

static eNodeSocketDatatype socket_type_from_pass(const RenderPass *pass)
{
  switch (pass->channels) {
    case 1:
      return SOCK_FLOAT;
    case 2:
    case 3:
      if (STR_ELEM(pass->chan_id, "RGB", "rgb")) {
        return SOCK_RGBA;
      }
      else {
        return SOCK_VECTOR;
      }
    case 4:
      if (STR_ELEM(pass->chan_id, "RGBA", "rgba")) {
        return SOCK_RGBA;
      }
      else {
        return SOCK_VECTOR;
      }
    default:
      break;
  }

  BLI_assert_unreachable();
  return SOCK_FLOAT;
}

static void cmp_node_image_create_outputs(bNodeTree *ntree,
                                          bNode *node,
                                          LinkNodePair *available_sockets)
{
  Image *ima = (Image *)node->id;
  ImBuf *ibuf;
  int prev_index = -1;
  if (ima) {
    ImageUser *iuser = (ImageUser *)node->storage;
    ImageUser load_iuser = {nullptr};
    int offset = BKE_image_sequence_guess_offset(ima);

    /* It is possible that image user in this node is not
     * properly updated yet. In this case loading image will
     * fail and sockets detection will go wrong.
     *
     * So we manually construct image user to be sure first
     * image from sequence (that one which is set as filename
     * for image data-block) is used for sockets detection. */
    load_iuser.framenr = offset;

    /* make sure ima->type is correct */
    ibuf = BKE_image_acquire_ibuf(ima, &load_iuser, nullptr);

    if (ima->rr) {
      RenderLayer *rl = (RenderLayer *)BLI_findlink(&ima->rr->layers, iuser->layer);

      if (rl) {
        LISTBASE_FOREACH (RenderPass *, rpass, &rl->passes) {
          const eNodeSocketDatatype type = socket_type_from_pass(rpass);
          cmp_node_image_add_pass_output(
              ntree, node, rpass->name, rpass->name, type, available_sockets, &prev_index);
          /* Special handling for the Combined pass to ensure compatibility. */
          if (STREQ(rpass->name, RE_PASSNAME_COMBINED)) {
            cmp_node_image_add_pass_output(
                ntree, node, "Alpha", rpass->name, SOCK_FLOAT, available_sockets, &prev_index);
          }
        }
        BKE_image_release_ibuf(ima, ibuf, nullptr);
        return;
      }
    }
  }

  cmp_node_image_add_pass_output(
      ntree, node, "Image", RE_PASSNAME_COMBINED, SOCK_RGBA, available_sockets, &prev_index);
  cmp_node_image_add_pass_output(
      ntree, node, "Alpha", RE_PASSNAME_COMBINED, SOCK_FLOAT, available_sockets, &prev_index);

  if (ima) {
    BKE_image_release_ibuf(ima, ibuf, nullptr);
  }
}

static void cmp_node_image_verify_outputs(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock, *sock_next;
  LinkNodePair available_sockets = {nullptr, nullptr};

  cmp_node_image_create_outputs(ntree, node, &available_sockets);

  /* Get rid of sockets whose passes are not available in the image.
   * If sockets that are not available would be deleted, the connections to them would be lost
   * when e.g. opening a file (since there's no render at all yet).
   * Therefore, sockets with connected links will just be set as unavailable.
   *
   * Another important detail comes from compatibility with the older socket model, where there
   * was a fixed socket per pass type that was just hidden or not. Therefore, older versions expect
   * the first 31 passes to belong to a specific pass type.
   * So, we keep those 31 always allocated before the others as well,
   * even if they have no links attached. */
  int sock_index = 0;
  for (sock = (bNodeSocket *)node->outputs.first; sock; sock = sock_next, sock_index++) {
    sock_next = sock->next;
    if (BLI_linklist_index(available_sockets.list, sock) >= 0) {
      blender::bke::node_set_socket_availability(*ntree, *sock, true);
    }
    else {
      bNodeLink *link;
      for (link = (bNodeLink *)ntree->links.first; link; link = link->next) {
        if (link->fromsock == sock) {
          break;
        }
      }
      if (!link) {
        MEM_freeN(reinterpret_cast<NodeImageLayer *>(sock->storage));
        blender::bke::node_remove_socket(*ntree, *node, *sock);
      }
      else {
        blender::bke::node_set_socket_availability(*ntree, *sock, false);
      }
    }
  }

  BLI_linklist_free(available_sockets.list, nullptr);
}

static void cmp_node_image_update(bNodeTree *ntree, bNode *node)
{
  /* avoid unnecessary updates, only changes to the image/image user data are of interest */
  if (node->runtime->update & NODE_UPDATE_ID) {
    cmp_node_image_verify_outputs(ntree, node);
  }

  cmp_node_update_default(ntree, node);
}

static void node_composit_init_image(bNodeTree *ntree, bNode *node)
{
  ImageUser *iuser = MEM_callocN<ImageUser>(__func__);
  node->storage = iuser;
  iuser->frames = 1;
  iuser->sfra = 1;
  iuser->flag |= IMA_ANIM_ALWAYS;

  /* setup initial outputs */
  cmp_node_image_verify_outputs(ntree, node);
}

static void node_composit_free_image(bNode *node)
{
  /* free extra socket info */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    MEM_freeN(reinterpret_cast<NodeImageLayer *>(sock->storage));
  }

  MEM_freeN(reinterpret_cast<ImageUser *>(node->storage));
}

static void node_composit_copy_image(bNodeTree * /*dst_ntree*/,
                                     bNode *dest_node,
                                     const bNode *src_node)
{
  dest_node->storage = MEM_dupallocN(src_node->storage);

  const bNodeSocket *src_output_sock = (bNodeSocket *)src_node->outputs.first;
  bNodeSocket *dest_output_sock = (bNodeSocket *)dest_node->outputs.first;
  while (dest_output_sock != nullptr) {
    dest_output_sock->storage = MEM_dupallocN(src_output_sock->storage);

    src_output_sock = src_output_sock->next;
    dest_output_sock = dest_output_sock->next;
  }
}

using namespace blender::compositor;

class ImageOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    for (const bNodeSocket *output : this->node()->output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      compute_output(output->identifier);
    }
  }

  void compute_output(StringRef identifier)
  {
    if (!should_compute_output(identifier)) {
      return;
    }

    const StringRef pass_name = this->get_pass_name(identifier);
    Result cached_image = context().cache_manager().cached_images.get(
        context(), get_image(), get_image_user(), pass_name.data());

    Result &result = get_result(identifier);
    if (!cached_image.is_allocated()) {
      result.allocate_invalid();
      return;
    }

    /* Alpha is not an actual pass, but one that is extracted from the combined pass. */
    if (identifier == "Alpha" && pass_name == RE_PASSNAME_COMBINED) {
      extract_alpha(context(), cached_image, result);
    }
    else {
      result.set_type(cached_image.type());
      result.set_precision(cached_image.precision());
      result.wrap_external(cached_image);
    }
  }

  /* Get the name of the pass corresponding to the output with the given identifier. */
  const char *get_pass_name(StringRef identifier)
  {
    DOutputSocket output = node().output_by_identifier(identifier);
    return static_cast<NodeImageLayer *>(output->storage)->pass_name;
  }

  Image *get_image()
  {
    return reinterpret_cast<Image *>(bnode().id);
  }

  ImageUser *get_image_user()
  {
    return static_cast<ImageUser *>(bnode().storage);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ImageOperation(context, node);
}

static void register_node_type_cmp_image()
{
  namespace file_ns = blender::nodes::node_composite_image_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeImage", CMP_NODE_IMAGE);
  ntype.ui_name = "Image";
  ntype.ui_description = "Input image or movie file";
  ntype.enum_name_legacy = "IMAGE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.initfunc = file_ns::node_composit_init_image;
  blender::bke::node_type_storage(
      ntype, "ImageUser", file_ns::node_composit_free_image, file_ns::node_composit_copy_image);
  ntype.updatefunc = file_ns::cmp_node_image_update;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.labelfunc = node_image_label;
  ntype.flag |= NODE_PREVIEW;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_image)

}  // namespace blender::nodes::node_composite_image_cc
