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
 * \ingroup nodes
 */

#include "NOD_socket_search_link.hh"

#include "node_composite_util.hh"

bool cmp_node_poll_default(bNodeType *UNUSED(ntype),
                           bNodeTree *ntree,
                           const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "CompositorNodeTree")) {
    *r_disabled_hint = TIP_("Not a compositor node tree");
    return false;
  }
  return true;
}

void cmp_node_update_default(bNodeTree *UNUSED(ntree), bNode *node)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (sock->cache) {
      // free_compbuf(sock->cache);
      // sock->cache = nullptr;
    }
  }
  node->need_exec = 1;
}

void cmp_node_type_base(bNodeType *ntype, int type, const char *name, short nclass)
{
  node_type_base(ntype, type, name, nclass);

  ntype->poll = cmp_node_poll_default;
  ntype->updatefunc = cmp_node_update_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
}
