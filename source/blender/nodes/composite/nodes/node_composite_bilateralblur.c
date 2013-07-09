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
 * Contributor(s): Vilem Novak
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_bilateralblur.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** BILATERALBLUR ******************** */
static bNodeSocketTemplate cmp_node_bilateralblur_in[] = {
	{ SOCK_RGBA, 1, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f},
	{ SOCK_RGBA, 1, N_("Determinator"), 1.0f, 1.0f, 1.0f, 1.0f},
	{ -1, 0, "" }
};

static bNodeSocketTemplate cmp_node_bilateralblur_out[] = {
	{ SOCK_RGBA, 0, N_("Image")},
	{ -1, 0, "" }
};

static void node_composit_init_bilateralblur(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeBilateralBlurData *nbbd = MEM_callocN(sizeof(NodeBilateralBlurData), "node bilateral blur data");
	node->storage = nbbd;
	nbbd->sigma_color = 0.3;
	nbbd->sigma_space = 5.0;
}

void register_node_type_cmp_bilateralblur(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_BILATERALBLUR, "Bilateral Blur", NODE_CLASS_OP_FILTER, 0);
	node_type_socket_templates(&ntype, cmp_node_bilateralblur_in, cmp_node_bilateralblur_out);
	node_type_init(&ntype, node_composit_init_bilateralblur);
	node_type_storage(&ntype, "NodeBilateralBlurData", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
