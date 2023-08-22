/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

#include "BLI_linklist.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

/* **************** IMAGE (and RenderResult, multilayer image) ******************** */

static bNodeSocketTemplate cmp_node_rlayers_out[] = {
    {SOCK_RGBA, N_("Image"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_("Alpha"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_(RE_PASSNAME_Z), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_(RE_PASSNAME_NORMAL), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_(RE_PASSNAME_UV), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_(RE_PASSNAME_VECTOR), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_(RE_PASSNAME_POSITION), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_SHADOW), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_AO), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_(RE_PASSNAME_INDEXOB), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_(RE_PASSNAME_INDEXMA), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_(RE_PASSNAME_MIST), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_EMIT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_ENVIRONMENT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DIFFUSE_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DIFFUSE_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_DIFFUSE_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_GLOSSY_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_GLOSSY_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_GLOSSY_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_TRANSM_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_TRANSM_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_TRANSM_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_SUBSURFACE_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_SUBSURFACE_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_(RE_PASSNAME_SUBSURFACE_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};
#define NUM_LEGACY_SOCKETS (ARRAY_SIZE(cmp_node_rlayers_out) - 1)

static void cmp_node_image_add_pass_output(bNodeTree *ntree,
                                           bNode *node,
                                           const char *name,
                                           const char *passname,
                                           int rres_index,
                                           eNodeSocketDatatype type,
                                           int /*is_rlayers*/,
                                           LinkNodePair *available_sockets,
                                           int *prev_index)
{
  bNodeSocket *sock = (bNodeSocket *)BLI_findstring(
      &node->outputs, name, offsetof(bNodeSocket, name));

  /* Replace if types don't match. */
  if (sock && sock->type != type) {
    nodeRemoveSocket(ntree, node, sock);
    sock = nullptr;
  }

  /* Create socket if it doesn't exist yet. */
  if (sock == nullptr) {
    if (rres_index >= 0) {
      sock = node_add_socket_from_template(
          ntree, node, &cmp_node_rlayers_out[rres_index], SOCK_OUT);
    }
    else {
      sock = nodeAddStaticSocket(ntree, node, SOCK_OUT, type, PROP_NONE, name, name);
    }
    /* extra socket info */
    NodeImageLayer *sockdata = MEM_cnew<NodeImageLayer>(__func__);
    sock->storage = sockdata;
  }

  NodeImageLayer *sockdata = (NodeImageLayer *)sock->storage;
  if (sockdata) {
    STRNCPY(sockdata->pass_name, passname);
  }

  /* Reorder sockets according to order that passes are added. */
  const int after_index = (*prev_index)++;
  bNodeSocket *after_sock = (bNodeSocket *)BLI_findlink(&node->outputs, after_index);
  BLI_remlink(&node->outputs, sock);
  BLI_insertlinkafter(&node->outputs, after_sock, sock);

  BLI_linklist_append(available_sockets, sock);
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
          eNodeSocketDatatype type;
          if (rpass->channels == 1) {
            type = SOCK_FLOAT;
          }
          else {
            type = SOCK_RGBA;
          }

          cmp_node_image_add_pass_output(ntree,
                                         node,
                                         rpass->name,
                                         rpass->name,
                                         -1,
                                         type,
                                         false,
                                         available_sockets,
                                         &prev_index);
          /* Special handling for the Combined pass to ensure compatibility. */
          if (STREQ(rpass->name, RE_PASSNAME_COMBINED)) {
            cmp_node_image_add_pass_output(ntree,
                                           node,
                                           "Alpha",
                                           rpass->name,
                                           -1,
                                           SOCK_FLOAT,
                                           false,
                                           available_sockets,
                                           &prev_index);
          }
        }
        BKE_image_release_ibuf(ima, ibuf, nullptr);
        return;
      }
    }
  }

  cmp_node_image_add_pass_output(ntree,
                                 node,
                                 "Image",
                                 RE_PASSNAME_COMBINED,
                                 -1,
                                 SOCK_RGBA,
                                 false,
                                 available_sockets,
                                 &prev_index);
  cmp_node_image_add_pass_output(ntree,
                                 node,
                                 "Alpha",
                                 RE_PASSNAME_COMBINED,
                                 -1,
                                 SOCK_FLOAT,
                                 false,
                                 available_sockets,
                                 &prev_index);

  if (ima) {
    BKE_image_release_ibuf(ima, ibuf, nullptr);
  }
}

struct RLayerUpdateData {
  LinkNodePair *available_sockets;
  int prev_index;
};

void node_cmp_rlayers_register_pass(bNodeTree *ntree,
                                    bNode *node,
                                    Scene *scene,
                                    ViewLayer *view_layer,
                                    const char *name,
                                    eNodeSocketDatatype type)
{
  RLayerUpdateData *data = (RLayerUpdateData *)node->storage;

  if (scene == nullptr || view_layer == nullptr || data == nullptr || node->id != (ID *)scene) {
    return;
  }

  ViewLayer *node_view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, node->custom1);
  if (node_view_layer != view_layer) {
    return;
  }

  /* Special handling for the Combined pass to ensure compatibility. */
  if (STREQ(name, RE_PASSNAME_COMBINED)) {
    cmp_node_image_add_pass_output(
        ntree, node, "Image", name, -1, type, true, data->available_sockets, &data->prev_index);
    cmp_node_image_add_pass_output(ntree,
                                   node,
                                   "Alpha",
                                   name,
                                   -1,
                                   SOCK_FLOAT,
                                   true,
                                   data->available_sockets,
                                   &data->prev_index);
  }
  else {
    cmp_node_image_add_pass_output(
        ntree, node, name, name, -1, type, true, data->available_sockets, &data->prev_index);
  }
}

struct CreateOutputUserData {
  bNodeTree &ntree;
  bNode &node;
};

static void cmp_node_rlayer_create_outputs_cb(void *userdata,
                                              Scene *scene,
                                              ViewLayer *view_layer,
                                              const char *name,
                                              int /*channels*/,
                                              const char * /*chanid*/,
                                              eNodeSocketDatatype type)
{
  CreateOutputUserData &data = *(CreateOutputUserData *)userdata;
  node_cmp_rlayers_register_pass(&data.ntree, &data.node, scene, view_layer, name, type);
}

static void cmp_node_rlayer_create_outputs(bNodeTree *ntree,
                                           bNode *node,
                                           LinkNodePair *available_sockets)
{
  Scene *scene = (Scene *)node->id;

  if (scene) {
    RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
    if (engine_type && engine_type->update_render_passes) {
      ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, node->custom1);
      if (view_layer) {
        RLayerUpdateData *data = (RLayerUpdateData *)MEM_mallocN(sizeof(RLayerUpdateData),
                                                                 "render layer update data");
        data->available_sockets = available_sockets;
        data->prev_index = -1;
        node->storage = data;

        CreateOutputUserData userdata = {*ntree, *node};

        RenderEngine *engine = RE_engine_create(engine_type);
        RE_engine_update_render_passes(
            engine, scene, view_layer, cmp_node_rlayer_create_outputs_cb, &userdata);
        RE_engine_free(engine);

        if ((scene->r.mode & R_EDGE_FRS) &&
            (view_layer->freestyle_config.flags & FREESTYLE_AS_RENDER_PASS))
        {
          node_cmp_rlayers_register_pass(
              ntree, node, scene, view_layer, RE_PASSNAME_FREESTYLE, SOCK_RGBA);
        }

        MEM_freeN(data);
        node->storage = nullptr;

        return;
      }
    }
  }

  int prev_index = -1;
  cmp_node_image_add_pass_output(ntree,
                                 node,
                                 "Image",
                                 RE_PASSNAME_COMBINED,
                                 RRES_OUT_IMAGE,
                                 SOCK_RGBA,
                                 true,
                                 available_sockets,
                                 &prev_index);
  cmp_node_image_add_pass_output(ntree,
                                 node,
                                 "Alpha",
                                 RE_PASSNAME_COMBINED,
                                 RRES_OUT_ALPHA,
                                 SOCK_FLOAT,
                                 true,
                                 available_sockets,
                                 &prev_index);
}

/* XXX make this into a generic socket verification function for dynamic socket replacement
 * (multilayer, groups, static templates) */
static void cmp_node_image_verify_outputs(bNodeTree *ntree, bNode *node, bool rlayer)
{
  bNodeSocket *sock, *sock_next;
  LinkNodePair available_sockets = {nullptr, nullptr};

  /* XXX make callback */
  if (rlayer) {
    cmp_node_rlayer_create_outputs(ntree, node, &available_sockets);
  }
  else {
    cmp_node_image_create_outputs(ntree, node, &available_sockets);
  }

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
      sock->flag &= ~SOCK_HIDDEN;
      blender::bke::nodeSetSocketAvailability(ntree, sock, true);
    }
    else {
      bNodeLink *link;
      for (link = (bNodeLink *)ntree->links.first; link; link = link->next) {
        if (link->fromsock == sock) {
          break;
        }
      }
      if (!link && (!rlayer || sock_index >= NUM_LEGACY_SOCKETS)) {
        MEM_freeN(sock->storage);
        nodeRemoveSocket(ntree, node, sock);
      }
      else {
        blender::bke::nodeSetSocketAvailability(ntree, sock, false);
      }
    }
  }

  BLI_linklist_free(available_sockets.list, nullptr);
}

namespace blender::nodes::node_composite_image_cc {

static void cmp_node_image_update(bNodeTree *ntree, bNode *node)
{
  /* avoid unnecessary updates, only changes to the image/image user data are of interest */
  if (node->runtime->update & NODE_UPDATE_ID) {
    cmp_node_image_verify_outputs(ntree, node, false);
  }

  cmp_node_update_default(ntree, node);
}

static void node_composit_init_image(bNodeTree *ntree, bNode *node)
{
  ImageUser *iuser = MEM_cnew<ImageUser>(__func__);
  node->storage = iuser;
  iuser->frames = 1;
  iuser->sfra = 1;
  iuser->flag |= IMA_ANIM_ALWAYS;

  /* setup initial outputs */
  cmp_node_image_verify_outputs(ntree, node, false);
}

static void node_composit_free_image(bNode *node)
{
  /* free extra socket info */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    MEM_freeN(sock->storage);
  }

  MEM_freeN(node->storage);
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

using namespace blender::realtime_compositor;

class ImageOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (!is_valid()) {
      allocate_invalid();
      return;
    }

    update_image_frame_number();

    for (const bNodeSocket *output : this->node()->output_sockets()) {
      compute_output(output->identifier);
    }
  }

  /* Returns true if the node results can be computed, otherwise, returns false. */
  bool is_valid()
  {
    Image *image = get_image();
    ImageUser *image_user = get_image_user();
    if (!image || !image_user) {
      return false;
    }

    if (BKE_image_is_multilayer(image)) {
      if (!image->rr) {
        return false;
      }

      RenderLayer *render_layer = get_render_layer();
      if (!render_layer) {
        return false;
      }
    }

    return true;
  }

  /* Allocate all needed outputs as invalid. This should be called when is_valid returns false. */
  void allocate_invalid()
  {
    for (const bNodeSocket *output : this->node()->output_sockets()) {
      if (!should_compute_output(output->identifier)) {
        continue;
      }

      Result &result = get_result(output->identifier);
      result.allocate_invalid();
    }
  }

  /* Compute the effective frame number of the image if it was animated and invalidate the cached
   * GPU texture if the computed frame number is different. */
  void update_image_frame_number()
  {
    BKE_image_user_frame_calc(get_image(), get_image_user(), context().get_frame_number());
  }

  void compute_output(StringRef identifier)
  {
    if (!should_compute_output(identifier)) {
      return;
    }

    ImageUser image_user = compute_image_user_for_output(identifier);
    BKE_image_ensure_gpu_texture(get_image(), &image_user);
    GPUTexture *image_texture = BKE_image_get_gpu_texture(get_image(), &image_user, nullptr);

    const int2 size = int2(GPU_texture_width(image_texture), GPU_texture_height(image_texture));
    Result &result = get_result(identifier);
    result.allocate_texture(Domain(size));

    GPUShader *shader = shader_manager().get(get_shader_name(identifier));
    GPU_shader_bind(shader);

    const int input_unit = GPU_shader_get_sampler_binding(shader, "input_tx");
    GPU_texture_bind(image_texture, input_unit);

    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, size);

    GPU_shader_unbind();
    GPU_texture_unbind(image_texture);
    result.unbind_as_image();
  }

  /* Get a copy of the image user that is appropriate to retrieve the image buffer for the output
   * with the given identifier. This essentially sets the appropriate pass and view indices that
   * corresponds to the output. */
  ImageUser compute_image_user_for_output(StringRef identifier)
  {
    ImageUser image_user = *get_image_user();

    /* Set the needed view. */
    image_user.view = get_view_index();

    /* Set the needed pass. */
    if (BKE_image_is_multilayer(get_image())) {
      image_user.pass = get_pass_index(get_pass_name(identifier));
      BKE_image_multilayer_index(get_image()->rr, &image_user);
    }
    else {
      BKE_image_multiview_index(get_image(), &image_user);
    }

    return image_user;
  }

  /* Get the shader that should be used to compute the output with the given identifier. The
   * shaders just copy the retrieved image textures into the results except for the alpha output,
   * which extracts the alpha and writes it to the result instead. Note that a call to a host
   * texture copy doesn't work because results are stored in a different half float formats. */
  const char *get_shader_name(StringRef identifier)
  {
    if (identifier == "Alpha") {
      return "compositor_extract_alpha_from_color";
    }
    else if (get_result(identifier).type() == ResultType::Color) {
      return "compositor_convert_color_to_half_color";
    }
    else {
      return "compositor_convert_float_to_half_float";
    }
  }

  Image *get_image()
  {
    return (Image *)bnode().id;
  }

  ImageUser *get_image_user()
  {
    return static_cast<ImageUser *>(bnode().storage);
  }

  /* Get the render layer selected in the node assuming the image is a multilayer image. */
  RenderLayer *get_render_layer()
  {
    const ListBase *layers = &get_image()->rr->layers;
    return static_cast<RenderLayer *>(BLI_findlink(layers, get_image_user()->layer));
  }

  /* Get the name of the pass corresponding to the output with the given identifier assuming the
   * image is a multilayer image. */
  const char *get_pass_name(StringRef identifier)
  {
    DOutputSocket output = node().output_by_identifier(identifier);
    return static_cast<NodeImageLayer *>(output->storage)->pass_name;
  }

  /* Get the index of the pass with the given name in the selected render layer's passes list
   * assuming the image is a multilayer image. */
  int get_pass_index(const char *name)
  {
    return BLI_findstringindex(&get_render_layer()->passes, name, offsetof(RenderPass, name));
  }

  /* Get the index of the view selected in the node. If the image is not a multi-view image or only
   * has a single view, then zero is returned. Otherwise, if the image is a multi-view image, the
   * index of the selected view is returned. However, note that the value of the view member of the
   * image user is not the actual index of the view. More specifically, the index 0 is reserved to
   * denote the special mode of operation "All", which dynamically selects the view whose name
   * matches the view currently being rendered. It follows that the views are then indexed starting
   * from 1. So for non zero view values, the actual index of the view is the value of the view
   * member of the image user minus 1. */
  int get_view_index()
  {
    /* The image is not a multi-view image, so just return zero. */
    if (!BKE_image_is_multiview(get_image())) {
      return 0;
    }

    const ListBase *views = &get_image()->rr->views;
    /* There is only one view and its index is 0. */
    if (BLI_listbase_count_at_most(views, 2) < 2) {
      return 0;
    }

    const int view = get_image_user()->view;
    /* The view is not zero, which means it is manually specified and the actual index is then the
     * view value minus 1. */
    if (view != 0) {
      return view - 1;
    }

    /* Otherwise, the view value is zero, denoting the special mode of operation "All", which finds
     * the index of the view whose name matches the view currently being rendered. */
    const char *view_name = context().get_view_name().data();
    const int matched_view = BLI_findstringindex(views, view_name, offsetof(RenderView, name));

    /* No view matches the view currently being rendered, so fallback to the first view. */
    if (matched_view == -1) {
      return 0;
    }

    return matched_view;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ImageOperation(context, node);
}

}  // namespace blender::nodes::node_composite_image_cc

void register_node_type_cmp_image()
{
  namespace file_ns = blender::nodes::node_composite_image_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_IMAGE, "Image", NODE_CLASS_INPUT);
  ntype.initfunc = file_ns::node_composit_init_image;
  node_type_storage(
      &ntype, "ImageUser", file_ns::node_composit_free_image, file_ns::node_composit_copy_image);
  ntype.updatefunc = file_ns::cmp_node_image_update;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.labelfunc = node_image_label;
  ntype.flag |= NODE_PREVIEW;

  nodeRegisterType(&ntype);
}

/* **************** RENDER RESULT ******************** */

void node_cmp_rlayers_outputs(bNodeTree *ntree, bNode *node)
{
  cmp_node_image_verify_outputs(ntree, node, true);
}

const char *node_cmp_rlayers_sock_to_pass(int sock_index)
{
  if (sock_index >= NUM_LEGACY_SOCKETS) {
    return nullptr;
  }
  const char *name = cmp_node_rlayers_out[sock_index].name;
  /* Exception for alpha, which is derived from Combined. */
  return STREQ(name, "Alpha") ? RE_PASSNAME_COMBINED : name;
}

namespace blender::nodes::node_composite_render_layer_cc {

static void node_composit_init_rlayers(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = (bNode *)ptr->data;
  int sock_index = 0;

  node->id = &scene->id;
  id_us_plus(node->id);

  for (bNodeSocket *sock = (bNodeSocket *)node->outputs.first; sock;
       sock = sock->next, sock_index++) {
    NodeImageLayer *sockdata = MEM_cnew<NodeImageLayer>(__func__);
    sock->storage = sockdata;

    STRNCPY(sockdata->pass_name, node_cmp_rlayers_sock_to_pass(sock_index));
  }
}

static bool node_composit_poll_rlayers(const bNodeType * /*ntype*/,
                                       const bNodeTree *ntree,
                                       const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "CompositorNodeTree")) {
    *r_disabled_hint = TIP_("Not a compositor node tree");
    return false;
  }

  Scene *scene;

  /* XXX ugly: check if ntree is a local scene node tree.
   * Render layers node can only be used in local `scene->nodetree`,
   * since it directly links to the scene.
   */
  for (scene = (Scene *)G.main->scenes.first; scene; scene = (Scene *)scene->id.next) {
    if (scene->nodetree == ntree) {
      break;
    }
  }

  if (scene == nullptr) {
    *r_disabled_hint = TIP_(
        "The node tree must be the compositing node tree of any scene in the file");
    return false;
  }
  return true;
}

static void node_composit_free_rlayers(bNode *node)
{
  /* free extra socket info */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (sock->storage) {
      MEM_freeN(sock->storage);
    }
  }
}

static void node_composit_copy_rlayers(bNodeTree * /*dst_ntree*/,
                                       bNode *dest_node,
                                       const bNode *src_node)
{
  /* copy extra socket info */
  const bNodeSocket *src_output_sock = (bNodeSocket *)src_node->outputs.first;
  bNodeSocket *dest_output_sock = (bNodeSocket *)dest_node->outputs.first;
  while (dest_output_sock != nullptr) {
    dest_output_sock->storage = MEM_dupallocN(src_output_sock->storage);

    src_output_sock = src_output_sock->next;
    dest_output_sock = dest_output_sock->next;
  }
}

static void cmp_node_rlayers_update(bNodeTree *ntree, bNode *node)
{
  cmp_node_image_verify_outputs(ntree, node, true);

  cmp_node_update_default(ntree, node);
}

static void node_composit_buts_viewlayers(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  uiLayout *col, *row;

  uiTemplateID(layout,
               C,
               ptr,
               "scene",
               nullptr,
               nullptr,
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (!node->id) {
    return;
  }

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "layer", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  PropertyRNA *prop = RNA_struct_find_property(ptr, "layer");
  const char *layer_name;
  if (!RNA_property_enum_identifier(C, ptr, prop, RNA_property_enum_get(ptr, prop), &layer_name)) {
    return;
  }

  PointerRNA scn_ptr;
  char scene_name[MAX_ID_NAME - 2];
  scn_ptr = RNA_pointer_get(ptr, "scene");
  RNA_string_get(&scn_ptr, "name", scene_name);

  PointerRNA op_ptr;
  uiItemFullO(row,
              "RENDER_OT_render",
              "",
              ICON_RENDER_STILL,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &op_ptr);
  RNA_string_set(&op_ptr, "layer", layer_name);
  RNA_string_set(&op_ptr, "scene", scene_name);
}

using namespace blender::realtime_compositor;

class RenderLayerOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Scene *scene = reinterpret_cast<const Scene *>(bnode().id);
    const int view_layer = bnode().custom1;

    Result &image_result = get_result("Image");
    Result &alpha_result = get_result("Alpha");

    if (image_result.should_compute() || alpha_result.should_compute()) {
      GPUTexture *combined_texture = context().get_input_texture(
          scene, view_layer, RE_PASSNAME_COMBINED);
      if (image_result.should_compute()) {
        execute_pass(image_result, combined_texture, "compositor_read_pass_color");
      }
      if (alpha_result.should_compute()) {
        execute_pass(alpha_result, combined_texture, "compositor_read_pass_alpha");
      }
    }

    /* Other output passes are not supported for now, so allocate them as invalid. */
    for (const bNodeSocket *output : this->node()->output_sockets()) {
      if (STR_ELEM(output->identifier, "Image", "Alpha")) {
        continue;
      }

      Result &result = get_result(output->identifier);
      if (!result.should_compute()) {
        continue;
      }

      GPUTexture *pass_texture = context().get_input_texture(
          scene, view_layer, output->identifier);
      if (output->type == SOCK_FLOAT) {
        execute_pass(result, pass_texture, "compositor_read_pass_float");
      }
      else if (output->type == SOCK_VECTOR) {
        execute_pass(result, pass_texture, "compositor_read_pass_vector");
      }
      else if (output->type == SOCK_RGBA) {
        execute_pass(result, pass_texture, "compositor_read_pass_color");
      }
      else {
        BLI_assert_unreachable();
      }
    }
  }

  void execute_pass(Result &result, GPUTexture *pass_texture, const char *shader_name)
  {
    if (pass_texture == nullptr) {
      /* Pass not rendered yet, or not supported by viewport. */
      result.allocate_invalid();
      context().set_info_message("Viewport compositor setup not fully supported");
      return;
    }

    GPUShader *shader = shader_manager().get(shader_name);
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized texture. */
    const rcti compositing_region = context().get_compositing_region();
    const int2 lower_bound = int2(compositing_region.xmin, compositing_region.ymin);
    GPU_shader_uniform_2iv(shader, "compositing_region_lower_bound", lower_bound);

    const int input_unit = GPU_shader_get_sampler_binding(shader, "input_tx");
    GPU_texture_bind(pass_texture, input_unit);

    const int2 compositing_region_size = context().get_compositing_region_size();
    result.allocate_texture(Domain(compositing_region_size));
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, compositing_region_size);

    GPU_shader_unbind();
    GPU_texture_unbind(pass_texture);
    result.unbind_as_image();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new RenderLayerOperation(context, node);
}

}  // namespace blender::nodes::node_composite_render_layer_cc

void register_node_type_cmp_rlayers()
{
  namespace file_ns = blender::nodes::node_composite_render_layer_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_R_LAYERS, "Render Layers", NODE_CLASS_INPUT);
  blender::bke::node_type_socket_templates(&ntype, nullptr, cmp_node_rlayers_out);
  ntype.draw_buttons = file_ns::node_composit_buts_viewlayers;
  ntype.initfunc_api = file_ns::node_composit_init_rlayers;
  ntype.poll = file_ns::node_composit_poll_rlayers;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Render passes not supported in the Viewport compositor");
  ntype.flag |= NODE_PREVIEW;
  node_type_storage(
      &ntype, nullptr, file_ns::node_composit_free_rlayers, file_ns::node_composit_copy_rlayers);
  ntype.updatefunc = file_ns::cmp_node_rlayers_update;
  ntype.initfunc = node_cmp_rlayers_outputs;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);

  nodeRegisterType(&ntype);
}
