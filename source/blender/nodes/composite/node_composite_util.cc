/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include "BKE_node_runtime.hh"

#include "NOD_socket_search_link.hh"

#include "node_composite_util.hh"

bool cmp_node_poll_default(const bNodeType * /*ntype*/,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "CompositorNodeTree")) {
    *r_disabled_hint = RPT_("Not a compositor node tree");
    return false;
  }
  return true;
}

void cmp_node_update_default(bNodeTree * /*ntree*/, bNode *node)
{
  node->runtime->need_exec = 1;
}

void cmp_node_type_base(bNodeType *ntype, int type, const char *name, short nclass)
{
  blender::bke::node_type_base(ntype, type, name, nclass);

  ntype->poll = cmp_node_poll_default;
  ntype->updatefunc = cmp_node_update_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
}
