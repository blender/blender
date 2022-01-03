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

#include "node_composite_util.hh"

#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DNA_scene_types.h"

#include "RE_engine.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

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
                                           int UNUSED(is_rlayers),
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
    BLI_strncpy(sockdata->pass_name, passname, sizeof(sockdata->pass_name));
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

static void cmp_node_rlayer_create_outputs_cb(void *UNUSED(userdata),
                                              Scene *scene,
                                              ViewLayer *view_layer,
                                              const char *name,
                                              int UNUSED(channels),
                                              const char *UNUSED(chanid),
                                              eNodeSocketDatatype type)
{
  /* Register the pass in all scenes that have a render layer node for this layer.
   * Since multiple scenes can be used in the compositor, the code must loop over all scenes
   * and check whether their nodetree has a node that needs to be updated. */
  /* NOTE: using G_MAIN seems valid here,
   * unless we want to register that for every other temp Main we could generate??? */
  ntreeCompositRegisterPass(scene->nodetree, scene, view_layer, name, type);

  for (Scene *sce = (Scene *)G_MAIN->scenes.first; sce; sce = (Scene *)sce->id.next) {
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
      ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, node->custom1);
      if (view_layer) {
        RLayerUpdateData *data = (RLayerUpdateData *)MEM_mallocN(sizeof(RLayerUpdateData),
                                                                 "render layer update data");
        data->available_sockets = available_sockets;
        data->prev_index = -1;
        node->storage = data;

        RenderEngine *engine = RE_engine_create(engine_type);
        RE_engine_update_render_passes(
            engine, scene, view_layer, cmp_node_rlayer_create_outputs_cb, nullptr);
        RE_engine_free(engine);

        if ((scene->r.mode & R_EDGE_FRS) &&
            (view_layer->freestyle_config.flags & FREESTYLE_AS_RENDER_PASS)) {
          ntreeCompositRegisterPass(ntree, scene, view_layer, RE_PASSNAME_FREESTYLE, SOCK_RGBA);
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
      nodeSetSocketAvailability(ntree, sock, true);
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
        nodeSetSocketAvailability(ntree, sock, false);
      }
    }
  }

  BLI_linklist_free(available_sockets.list, nullptr);
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

static void node_composit_copy_image(bNodeTree *UNUSED(dest_ntree),
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

void register_node_type_cmp_image()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_IMAGE, "Image", NODE_CLASS_INPUT, NODE_PREVIEW);
  node_type_init(&ntype, node_composit_init_image);
  node_type_storage(&ntype, "ImageUser", node_composit_free_image, node_composit_copy_image);
  node_type_update(&ntype, cmp_node_image_update);
  ntype.labelfunc = node_image_label;

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
  return (STREQ(name, "Alpha")) ? RE_PASSNAME_COMBINED : name;
}

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

    BLI_strncpy(sockdata->pass_name,
                node_cmp_rlayers_sock_to_pass(sock_index),
                sizeof(sockdata->pass_name));
  }
}

static bool node_composit_poll_rlayers(bNodeType *UNUSED(ntype),
                                       bNodeTree *ntree,
                                       const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "CompositorNodeTree")) {
    *r_disabled_hint = TIP_("Not a compositor node tree");
    return false;
  }

  Scene *scene;

  /* XXX ugly: check if ntree is a local scene node tree.
   * Render layers node can only be used in local scene->nodetree,
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

static void node_composit_copy_rlayers(bNodeTree *UNUSED(dest_ntree),
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
  if (!(RNA_property_enum_identifier(
          C, ptr, prop, RNA_property_enum_get(ptr, prop), &layer_name))) {
    return;
  }

  PointerRNA scn_ptr;
  char scene_name[MAX_ID_NAME - 2];
  scn_ptr = RNA_pointer_get(ptr, "scene");
  RNA_string_get(&scn_ptr, "name", scene_name);

  PointerRNA op_ptr;
  uiItemFullO(
      row, "RENDER_OT_render", "", ICON_RENDER_STILL, nullptr, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
  RNA_string_set(&op_ptr, "layer", layer_name);
  RNA_string_set(&op_ptr, "scene", scene_name);
}

void register_node_type_cmp_rlayers()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_R_LAYERS, "Render Layers", NODE_CLASS_INPUT, NODE_PREVIEW);
  node_type_socket_templates(&ntype, nullptr, cmp_node_rlayers_out);
  ntype.draw_buttons = node_composit_buts_viewlayers;
  ntype.initfunc_api = node_composit_init_rlayers;
  ntype.poll = node_composit_poll_rlayers;
  node_type_storage(&ntype, nullptr, node_composit_free_rlayers, node_composit_copy_rlayers);
  node_type_update(&ntype, cmp_node_rlayers_update);
  node_type_init(&ntype, node_cmp_rlayers_outputs);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);

  nodeRegisterType(&ntype);
}
