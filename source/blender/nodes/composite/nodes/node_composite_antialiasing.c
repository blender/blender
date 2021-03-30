/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): IRIE Shinsuke
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_antialiasing.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** Anti-Aliasing (SMAA 1x) ******************** */

static bNodeSocketTemplate cmp_node_antialiasing_in[] = {
    {SOCK_RGBA, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f}, {-1, ""}};

static bNodeSocketTemplate cmp_node_antialiasing_out[] = {{SOCK_RGBA, N_("Image")}, {-1, ""}};

static void node_composit_init_antialiasing(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAntiAliasingData *data = MEM_callocN(sizeof(NodeAntiAliasingData), "node antialiasing data");

  data->threshold = 1.0f;
  data->contrast_limit = 0.2f;
  data->corner_rounding = 0.25f;

  node->storage = data;
}

void register_node_type_cmp_antialiasing(void)
{
  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_ANTIALIASING, "Anti-Aliasing", NODE_CLASS_OP_FILTER, NODE_PREVIEW);
  node_type_socket_templates(&ntype, cmp_node_antialiasing_in, cmp_node_antialiasing_out);
  node_type_size(&ntype, 170, 140, 200);
  node_type_init(&ntype, node_composit_init_antialiasing);
  node_type_storage(
      &ntype, "NodeAntiAliasingData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
