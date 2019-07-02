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
#include "BKE_library.h"

#include "../node_composite_util.h"

/* **************** SWITCH VIEW ******************** */
static bNodeSocketTemplate cmp_node_switch_view_out[] = {
    {SOCK_RGBA, 0, N_("Image"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
    {-1, 0, ""},
};

static bNodeSocket *ntreeCompositSwitchViewAddSocket(bNodeTree *ntree,
                                                     bNode *node,
                                                     const char *name)
{
  bNodeSocket *sock = nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, NULL, name);
  return sock;
}

static void cmp_node_switch_view_sanitycheck(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock;

  if (!BLI_listbase_is_empty(&node->inputs)) {
    return;
  }

  sock = ntreeCompositSwitchViewAddSocket(ntree, node, "No View");
  sock->flag |= SOCK_HIDDEN;
}

static void cmp_node_switch_view_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock;
  SceneRenderView *srv;
  Scene *scene = (Scene *)node->id;

  /* only update when called from the operator button */
  if (node->update != NODE_UPDATE_OPERATOR) {
    return;
  }

  if (scene == NULL) {
    nodeRemoveAllSockets(ntree, node);
    /* make sure there is always one socket */
    cmp_node_switch_view_sanitycheck(ntree, node);
    return;
  }

  /* remove the views that were removed */
  sock = node->inputs.last;
  while (sock) {
    srv = BLI_findstring(&scene->r.views, sock->name, offsetof(SceneRenderView, name));

    if (srv == NULL) {
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
  for (srv = scene->r.views.first; srv; srv = srv->next) {
    sock = BLI_findstring(&node->inputs, srv->name, offsetof(bNodeSocket, name));

    if (sock == NULL) {
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
  bNodeTree *ntree = ptr->id.data;
  bNode *node = ptr->data;
  SceneRenderView *srv;
  bNodeSocket *sock;
  int nr;

  /* store scene for updates */
  node->id = (ID *)scene;
  id_us_plus(node->id);

  if (scene) {
    RenderData *rd = &scene->r;

    for (nr = 0, srv = rd->views.first; srv; srv = srv->next, nr++) {
      sock = ntreeCompositSwitchViewAddSocket(ntree, node, srv->name);

      if ((srv->viewflag & SCE_VIEW_DISABLE)) {
        sock->flag |= SOCK_HIDDEN;
      }
    }
  }

  /* make sure there is always one socket */
  cmp_node_switch_view_sanitycheck(ntree, node);
}

void register_node_type_cmp_switch_view(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SWITCH_VIEW, "Switch View", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, NULL, cmp_node_switch_view_out);

  ntype.initfunc_api = init_switch_view;

  node_type_update(&ntype, cmp_node_switch_view_update);

  nodeRegisterType(&ntype);
}
