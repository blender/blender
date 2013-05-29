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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Alfredo de Greef  (eeshlo)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_directionalblur.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_dblur_in[] = {
	{   SOCK_RGBA, 1, N_("Image"), 1.0f, 1.0f, 1.0f, 1.f},
	{   -1, 0, ""       }
};

static bNodeSocketTemplate cmp_node_dblur_out[] = {
	{   SOCK_RGBA, 0, N_("Image")},
	{   -1, 0, ""       }
};

static void node_composit_init_dblur(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeDBlurData *ndbd = MEM_callocN(sizeof(NodeDBlurData), "node dblur data");
	node->storage = ndbd;
	ndbd->center_x = 0.5;
	ndbd->center_y = 0.5;
}

void register_node_type_cmp_dblur(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_DBLUR, "Directional Blur", NODE_CLASS_OP_FILTER, 0);
	node_type_socket_templates(&ntype, cmp_node_dblur_in, cmp_node_dblur_out);
	node_type_init(&ntype, node_composit_init_dblur);
	node_type_storage(&ntype, "NodeDBlurData", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
