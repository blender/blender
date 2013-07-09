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
 * Contributor(s): Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_crop.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Crop  ******************** */

static bNodeSocketTemplate cmp_node_crop_in[] = {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_crop_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void node_composit_init_crop(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeTwoXYs *nxy = MEM_callocN(sizeof(NodeTwoXYs), "node xy data");
	node->storage = nxy;
	nxy->x1 = 0;
	nxy->x2 = 0;
	nxy->y1 = 0;
	nxy->y2 = 0;
}

void register_node_type_cmp_crop(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_CROP, "Crop", NODE_CLASS_DISTORT, 0);
	node_type_socket_templates(&ntype, cmp_node_crop_in, cmp_node_crop_out);
	node_type_init(&ntype, node_composit_init_crop);
	node_type_storage(&ntype, "NodeTwoXYs", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
