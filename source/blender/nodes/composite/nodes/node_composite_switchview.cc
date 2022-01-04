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
 * Dalai Felinto
 */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_context.h"
#include "BKE_lib_id.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** SWITCH VIEW ******************** */

static bNodeSocketTemplate cmp_node_switch_view_out[] = {
    {SOCK_RGBA, N_("Image"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
    {-1, ""},
};

static bNodeSocket *ntreeCompositSwitchViewAddSocket(bNodeTree *ntree,
                                                     bNode *node,
                                                     const char *name)
{
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, nullptr, name);
  return sock;
}

static void cmp_node_switch_view_sanitycheck(bNodeTree *ntree, bNode *node)
{
  if (!BLI_listbase_is_empty(&node->inputs)) {
    return;
  }

  bNodeSocket *sock = ntreeCompositSwitchViewAddSocket(ntree, node, "No View");
  sock->flag |= SOCK_HIDDEN;
}

static void cmp_node_switch_view_update(bNodeTree *ntree, bNode *node)
{
  Scene *scene = (Scene *)node->id;

  /* only update when called from the operator button */
  if (node->update != NODE_UPDATE_OPERATOR) {
    return;
  }

  if (scene == nullptr) {
    nodeRemoveAllSockets(ntree, node);
    /* make sure there is always one socket */
    cmp_node_switch_view_sanitycheck(ntree, node);
    return;
  }

  /* remove the views that were removed */
  bNodeSocket *sock = (bNodeSocket *)node->inputs.last;
  while (sock) {
    SceneRenderView *srv = (SceneRenderView *)BLI_findstring(
        &scene->r.views, sock->name, offsetof(SceneRenderView, name));

    if (srv == nullptr) {
      bNodeSocket *sock_del = sock;
      sock = sock->prev;
      nodeRemoveSocket(ntree, node, sock_del);
    }
    else {
      if (srv->viewflag & SCE_VIEW_DISABLE) {
        sock->flag |= SOCK_HIDDEN;
      }
      else {
        sock->flag &= ~SOCK_HIDDEN;
      }

      sock = sock->prev;
    }
  }

  /* add the new views */
  LISTBASE_FOREACH (SceneRenderView *, srv, &scene->r.views) {
    sock = (bNodeSocket *)BLI_findstring(&node->inputs, srv->name, offsetof(bNodeSocket, name));

    if (sock == nullptr) {
      sock = ntreeCompositSwitchViewAddSocket(ntree, node, srv->name);
    }

    if (srv->viewflag & SCE_VIEW_DISABLE) {
      sock->flag |= SOCK_HIDDEN;
    }
    else {
      sock->flag &= ~SOCK_HIDDEN;
    }
  }

  /* make sure there is always one socket */
  cmp_node_switch_view_sanitycheck(ntree, node);
}

static void init_switch_view(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
  bNode *node = (bNode *)ptr->data;

  /* store scene for updates */
  node->id = (ID *)scene;
  id_us_plus(node->id);

  if (scene) {
    RenderData *rd = &scene->r;

    LISTBASE_FOREACH (SceneRenderView *, srv, &rd->views) {
      bNodeSocket *sock = ntreeCompositSwitchViewAddSocket(ntree, node, srv->name);

      if (srv->viewflag & SCE_VIEW_DISABLE) {
        sock->flag |= SOCK_HIDDEN;
      }
    }
  }

  /* make sure there is always one socket */
  cmp_node_switch_view_sanitycheck(ntree, node);
}

static void node_composit_buts_switch_view_ex(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *UNUSED(ptr))
{
  uiItemFullO(layout,
              "NODE_OT_switch_view_update",
              "Update Views",
              ICON_FILE_REFRESH,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              nullptr);
}

void register_node_type_cmp_switch_view()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SWITCH_VIEW, "Switch View", NODE_CLASS_CONVERTER);
  node_type_socket_templates(&ntype, nullptr, cmp_node_switch_view_out);
  ntype.draw_buttons_ex = node_composit_buts_switch_view_ex;
  ntype.initfunc_api = init_switch_view;

  node_type_update(&ntype, cmp_node_switch_view_update);

  nodeRegisterType(&ntype);
}
