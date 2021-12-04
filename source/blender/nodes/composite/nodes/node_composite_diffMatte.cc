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

/* ******************* channel Difference Matte ********************************* */

namespace blender::nodes {

static void cmp_node_diff_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image 1")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>(N_("Image 2")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
  b.add_output<decl::Color>(N_("Matte"));
}

}  // namespace blender::nodes

static void node_composit_init_diff_matte(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeChroma *c = (NodeChroma *)MEM_callocN(sizeof(NodeChroma), "node chroma");
  node->storage = c;
  c->t1 = 0.1f;
  c->t2 = 0.1f;
}

void register_node_type_cmp_diff_matte(void)
{
  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_DIFF_MATTE, "Difference Key", NODE_CLASS_MATTE, NODE_PREVIEW);
  ntype.declare = blender::nodes::cmp_node_diff_matte_declare;
  node_type_init(&ntype, node_composit_init_diff_matte);
  node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
