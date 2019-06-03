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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.h"

#include "BLI_utildefines.h"
#include "BLI_linklist.h"

#include "DNA_scene_types.h"

#include "RE_engine.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

/* **************** IMAGE (and RenderResult, multilayer image) ******************** */

static bNodeSocketTemplate cmp_node_rlayers_out[] = {
    {SOCK_RGBA, 0, N_("Image"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, 0, N_("Alpha"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, 0, N_(RE_PASSNAME_Z), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, 0, N_(RE_PASSNAME_NORMAL), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, 0, N_(RE_PASSNAME_UV), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, 0, N_(RE_PASSNAME_VECTOR), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_SHADOW), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_AO), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DEPRECATED), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, 0, N_(RE_PASSNAME_INDEXOB), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, 0, N_(RE_PASSNAME_INDEXMA), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, 0, N_(RE_PASSNAME_MIST), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_EMIT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_ENVIRONMENT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DIFFUSE_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DIFFUSE_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_DIFFUSE_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_GLOSSY_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_GLOSSY_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_GLOSSY_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_TRANSM_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_TRANSM_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_TRANSM_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_SUBSURFACE_DIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_SUBSURFACE_INDIRECT), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, 0, N_(RE_PASSNAME_SUBSURFACE_COLOR), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, 0, ""},
};

static void cmp_node_image_add_pass_output(bNodeTree *ntree,
                                           bNode *node,
                                           const char *name,
                                           const char *passname,
                                           int rres_index,
                                           int type,
                                           int is_rlayers,
                                           LinkNodePair *available_sockets,
                                           int *prev_index)
{
  bNodeSocket *sock;
  int sock_index = BLI_findstringindex(&node->outputs, name, offsetof(bNodeSocket, name));

  if (sock_index < 0) {
    /* The first 31 sockets always are the legacy hardcoded sockets.
     * Any dynamically allocated sockets follow afterwards,
     * and are sorted in the order in which they were stored in the RenderResult.
     * Therefore, we remember the index of the last matched socket.
     * New sockets are placed behind the previously traversed one,
     * but always after the first 31. */
    int after_index = *prev_index;
    if (is_rlayers && after_index < 30) {
      after_index = 30;
    }

    if (rres_index >= 0) {
      sock = node_add_socket_from_template(
          ntree, node, &cmp_node_rlayers_out[rres_index], SOCK_OUT);
    }
    else {
      sock = nodeAddStaticSocket(ntree, node, SOCK_OUT, type, PROP_NONE, name, name);
    }
    /* extra socket info */
    NodeImageLayer *sockdata = MEM_callocN(sizeof(NodeImageLayer), "node image layer");
    sock->storage = sockdata;

    BLI_strncpy(sockdata->pass_name, passname, sizeof(sockdata->pass_name));

    sock_index = BLI_listbase_count(&node->outputs) - 1;
    if (sock_index != after_index + 1) {
      bNodeSocket *after_sock = BLI_findlink(&node->outputs, after_index);
      BLI_remlink(&node->outputs, sock);
      BLI_insertlinkafter(&node->outputs, after_sock, sock);
    }
  }
  else {
    sock = BLI_findlink(&node->outputs, sock_index);
    NodeImageLayer *sockdata = sock->storage;
    if (sockdata) {
      BLI_strncpy(sockdata->pass_name, passname, sizeof(sockdata->pass_name));
    }
  }

  BLI_linklist_append(available_sockets, sock);
  *prev_index = sock_index;
}

static void cmp_node_image_create_outputs(bNodeTree *ntree,
                                          bNode *node,
                                          LinkNodePair *available_sockets)
{
  Image *ima = (Image *)node->id;
  ImBuf *ibuf;
  int prev_index = -1;
  if (ima) {
    ImageUser *iuser = node->storage;
    ImageUser load_iuser = {NULL};
    int offset = BKE_image_sequence_guess_offset(ima);

    /* It is possible that image user in this node is not
     * properly updated yet. In this case loading image will
     * fail and sockets detection will go wrong.
     *
     * So we manually construct image user to be sure first
     * image from sequence (that one which is set as filename
     * for image datablock) is used for sockets detection
     */
    load_iuser.ok = 1;
    load_iuser.framenr = offset;

    /* make sure ima->type is correct */
    ibuf = BKE_image_acquire_ibuf(ima, &load_iuser, NULL);

    if (ima->rr) {
      RenderLayer *rl = BLI_findlink(&ima->rr->layers, iuser->layer);

      if (rl) {
        RenderPass *rpass;
        for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
          int type;
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
        BKE_image_release_ibuf(ima, ibuf, NULL);
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
    if (!ima->rr) {
      cmp_node_image_add_pass_output(ntree,
                                     node,
                                     RE_PASSNAME_Z,
                                     RE_PASSNAME_Z,
                                     -1,
                                     SOCK_FLOAT,
                                     false,
                                     available_sockets,
                                     &prev_index);
    }
    BKE_image_release_ibuf(ima, ibuf, NULL);
  }
}

typedef struct RLayerUpdateData {
  LinkNodePair *available_sockets;
  int prev_index;
} RLayerUpdateData;

void node_cmp_rlayers_register_pass(
    bNodeTree *ntree, bNode *node, Scene *scene, ViewLayer *view_layer, const char *name, int type)
{
  RLayerUpdateData *data = node->storage;

  if (scene == NULL || view_layer == NULL || data == NULL || node->id != (ID *)scene) {
    return;
  }

  ViewLayer *node_view_layer = BLI_findlink(&scene->view_layers, node->custom1);
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

static void cmp_node_rlayer_create_outputs_cb(void *UNUSED(userdata),
                                              Scene *scene,
                                              ViewLayer *view_layer,
                                              const char *name,
                                              int UNUSED(channels),
                                              const char *UNUSED(chanid),
                                              int type)
{
  /* Register the pass in all scenes that have a render layer node for this layer.
   * Since multiple scenes can be used in the compositor, the code must loop over all scenes
   * and check whether their nodetree has a node that needs to be updated. */
  /* NOTE: using G_MAIN seems valid here,
   * unless we want to register that for every other temp Main we could generate??? */
  ntreeCompositRegisterPass(scene->nodetree, scene, view_layer, name, type);

  for (Scene *sce = G_MAIN->scenes.first; sce; sce = sce->id.next) {
    if (sce->nodetree && sce != scene) {
      ntreeCompositRegisterPass(sce->nodetree, scene, view_layer, name, type);
    }
  }
}

static void cmp_node_rlayer_create_outputs(bNodeTree *ntree,
                                           bNode *node,
                                           LinkNodePair *available_sockets)
{
  Scene *scene = (Scene *)node->id;

  if (scene) {
    RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
    if (engine_type && engine_type->update_render_passes) {
      ViewLayer *view_layer = BLI_findlink(&scene->view_layers, node->custom1);
      if (view_layer) {
        RLayerUpdateData *data = MEM_mallocN(sizeof(RLayerUpdateData), "render layer update data");
        data->available_sockets = available_sockets;
        data->prev_index = -1;
        node->storage = data;

        RenderEngine *engine = RE_engine_create(engine_type);
        RE_engine_update_render_passes(
            engine, scene, view_layer, cmp_node_rlayer_create_outputs_cb, NULL);
        RE_engine_free(engine);

        MEM_freeN(data);
        node->storage = NULL;

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
  LinkNodePair available_sockets = {NULL, NULL};
  int sock_index;

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
  sock_index = 0;
  for (sock = node->outputs.first; sock; sock = sock_next, sock_index++) {
    sock_next = sock->next;
    if (BLI_linklist_index(available_sockets.list, sock) >= 0) {
      sock->flag &= ~(SOCK_UNAVAIL | SOCK_HIDDEN);
    }
    else {
      bNodeLink *link;
      for (link = ntree->links.first; link; link = link->next) {
        if (link->fromsock == sock) {
          break;
        }
      }
      if (!link && (!rlayer || sock_index > 30)) {
        MEM_freeN(sock->storage);
        nodeRemoveSocket(ntree, node, sock);
      }
      else {
        sock->flag |= SOCK_UNAVAIL;
      }
    }
  }

  BLI_linklist_free(available_sockets.list, NULL);
}

static void cmp_node_image_update(bNodeTree *ntree, bNode *node)
{
  /* avoid unnecessary updates, only changes to the image/image user data are of interest */
  if (node->update & NODE_UPDATE_ID) {
    cmp_node_image_verify_outputs(ntree, node, false);
  }

  cmp_node_update_default(ntree, node);
}

static void node_composit_init_image(bNodeTree *ntree, bNode *node)
{
  ImageUser *iuser = MEM_callocN(sizeof(ImageUser), "node image user");
  node->storage = iuser;
  iuser->frames = 1;
  iuser->sfra = 1;
  iuser->ok = 1;
  iuser->flag |= IMA_ANIM_ALWAYS;

  /* setup initial outputs */
  cmp_node_image_verify_outputs(ntree, node, false);
}

static void node_composit_free_image(bNode *node)
{
  bNodeSocket *sock;

  /* free extra socket info */
  for (sock = node->outputs.first; sock; sock = sock->next) {
    MEM_freeN(sock->storage);
  }

  MEM_freeN(node->storage);
}

static void node_composit_copy_image(bNodeTree *UNUSED(dest_ntree),
                                     bNode *dest_node,
                                     const bNode *src_node)
{
  dest_node->storage = MEM_dupallocN(src_node->storage);

  const bNodeSocket *src_output_sock = src_node->outputs.first;
  bNodeSocket *dest_output_sock = dest_node->outputs.first;
  while (dest_output_sock != NULL) {
    dest_output_sock->storage = MEM_dupallocN(src_output_sock->storage);

    src_output_sock = src_output_sock->next;
    dest_output_sock = dest_output_sock->next;
  }
}

void register_node_type_cmp_image(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_IMAGE, "Image", NODE_CLASS_INPUT, NODE_PREVIEW);
  node_type_init(&ntype, node_composit_init_image);
  node_type_storage(&ntype, "ImageUser", node_composit_free_image, node_composit_copy_image);
  node_type_update(&ntype, cmp_node_image_update);
  node_type_label(&ntype, node_image_label);

  nodeRegisterType(&ntype);
}

/* **************** RENDER RESULT ******************** */

void node_cmp_rlayers_outputs(bNodeTree *ntree, bNode *node)
{
  cmp_node_image_verify_outputs(ntree, node, true);
}

const char *node_cmp_rlayers_sock_to_pass(int sock_index)
{
  const char *sock_to_passname[] = {
      RE_PASSNAME_COMBINED,
      RE_PASSNAME_COMBINED,
      RE_PASSNAME_Z,
      RE_PASSNAME_NORMAL,
      RE_PASSNAME_UV,
      RE_PASSNAME_VECTOR,
      RE_PASSNAME_DEPRECATED,
      RE_PASSNAME_DEPRECATED,
      RE_PASSNAME_DEPRECATED,
      RE_PASSNAME_SHADOW,
      RE_PASSNAME_AO,
      RE_PASSNAME_DEPRECATED,
      RE_PASSNAME_DEPRECATED,
      RE_PASSNAME_DEPRECATED,
      RE_PASSNAME_INDEXOB,
      RE_PASSNAME_INDEXMA,
      RE_PASSNAME_MIST,
      RE_PASSNAME_EMIT,
      RE_PASSNAME_ENVIRONMENT,
      RE_PASSNAME_DIFFUSE_DIRECT,
      RE_PASSNAME_DIFFUSE_INDIRECT,
      RE_PASSNAME_DIFFUSE_COLOR,
      RE_PASSNAME_GLOSSY_DIRECT,
      RE_PASSNAME_GLOSSY_INDIRECT,
      RE_PASSNAME_GLOSSY_COLOR,
      RE_PASSNAME_TRANSM_DIRECT,
      RE_PASSNAME_TRANSM_INDIRECT,
      RE_PASSNAME_TRANSM_COLOR,
      RE_PASSNAME_SUBSURFACE_DIRECT,
      RE_PASSNAME_SUBSURFACE_INDIRECT,
      RE_PASSNAME_SUBSURFACE_COLOR,
  };
  if (sock_index > 30) {
    return NULL;
  }
  return sock_to_passname[sock_index];
}

static void node_composit_init_rlayers(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = ptr->data;
  int sock_index = 0;

  node->id = &scene->id;

  for (bNodeSocket *sock = node->outputs.first; sock; sock = sock->next, sock_index++) {
    NodeImageLayer *sockdata = MEM_callocN(sizeof(NodeImageLayer), "node image layer");
    sock->storage = sockdata;

    BLI_strncpy(sockdata->pass_name,
                node_cmp_rlayers_sock_to_pass(sock_index),
                sizeof(sockdata->pass_name));
  }
}

static bool node_composit_poll_rlayers(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
  if (STREQ(ntree->idname, "CompositorNodeTree")) {
    Scene *scene;

    /* XXX ugly: check if ntree is a local scene node tree.
     * Render layers node can only be used in local scene->nodetree,
     * since it directly links to the scene.
     */
    for (scene = G.main->scenes.first; scene; scene = scene->id.next) {
      if (scene->nodetree == ntree) {
        break;
      }
    }

    return (scene != NULL);
  }
  return false;
}

static void node_composit_free_rlayers(bNode *node)
{
  bNodeSocket *sock;

  /* free extra socket info */
  for (sock = node->outputs.first; sock; sock = sock->next) {
    if (sock->storage) {
      MEM_freeN(sock->storage);
    }
  }
}

static void node_composit_copy_rlayers(bNodeTree *UNUSED(dest_ntree),
                                       bNode *dest_node,
                                       const bNode *src_node)
{
  /* copy extra socket info */
  const bNodeSocket *src_output_sock = src_node->outputs.first;
  bNodeSocket *dest_output_sock = dest_node->outputs.first;
  while (dest_output_sock != NULL) {
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

void register_node_type_cmp_rlayers(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_R_LAYERS, "Render Layers", NODE_CLASS_INPUT, NODE_PREVIEW);
  node_type_socket_templates(&ntype, NULL, cmp_node_rlayers_out);
  ntype.initfunc_api = node_composit_init_rlayers;
  ntype.poll = node_composit_poll_rlayers;
  node_type_storage(&ntype, NULL, node_composit_free_rlayers, node_composit_copy_rlayers);
  node_type_update(&ntype, cmp_node_rlayers_update);
  node_type_init(&ntype, node_cmp_rlayers_outputs);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);

  nodeRegisterType(&ntype);
}
