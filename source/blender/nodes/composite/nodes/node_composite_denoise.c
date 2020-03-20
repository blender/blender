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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_denoise_in[] = {
    {SOCK_RGBA, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Normal"), 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {SOCK_RGBA, N_("Albedo"), 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {-1, ""}};
static bNodeSocketTemplate cmp_node_denoise_out[] = {{SOCK_RGBA, N_("Image")}, {-1, ""}};

static void node_composit_init_denonise(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeDenoise *ndg = MEM_callocN(sizeof(NodeDenoise), "node denoise data");
  ndg->hdr = true;
  node->storage = ndg;
}

void register_node_type_cmp_denoise(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DENOISE, "Denoise", NODE_CLASS_OP_FILTER, 0);
  node_type_socket_templates(&ntype, cmp_node_denoise_in, cmp_node_denoise_out);
  node_type_init(&ntype, node_composit_init_denonise);
  node_type_storage(&ntype, "NodeDenoise", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
