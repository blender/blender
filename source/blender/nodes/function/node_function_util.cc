/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"
#include "node_util.h"

#include "NOD_socket_search_link.hh"

static bool fn_node_poll_default(bNodeType *UNUSED(ntype),
                                 bNodeTree *ntree,
                                 const char **r_disabled_hint)
{
  /* Function nodes are only supported in simulation node trees so far. */
  if (!STREQ(ntree->idname, "GeometryNodeTree", "ParticleNodeTree")) {
    *r_disabled_hint = TIP_("Not a geometry node tree");
    return false;
  }
  return true;
}

void fn_node_type_base(bNodeType *ntype, int type, const char *name, short nclass)
{
  node_type_base(ntype, type, name, nclass);
  ntype->poll = fn_node_poll_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
}
