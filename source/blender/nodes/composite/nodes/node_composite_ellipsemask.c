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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_ellipsemask.c
 *  \ingroup cmpnodes
 */


#include "../node_composite_util.h"

/* **************** SCALAR MATH ******************** */ 
static bNodeSocketTemplate cmp_node_ellipsemask_in[] = {
	{	SOCK_FLOAT, 1, N_("Mask"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Value"), 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{   -1, 0, "" }
};

static bNodeSocketTemplate cmp_node_ellipsemask_out[] = {
	{	SOCK_FLOAT, 0, N_("Mask"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{   -1, 0, "" }
};

static void node_composit_init_ellipsemask(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeEllipseMask *data = MEM_callocN(sizeof(NodeEllipseMask), "NodeEllipseMask");
	data->x = 0.5;
	data->y = 0.5;
	data->width = 0.2;
	data->height = 0.1;
	data->rotation = 0.0;
	node->storage = data;
}

void register_node_type_cmp_ellipsemask(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_MASK_ELLIPSE, "Ellipse Mask", NODE_CLASS_MATTE, 0);
	node_type_socket_templates(&ntype, cmp_node_ellipsemask_in, cmp_node_ellipsemask_out);
	node_type_size(&ntype, 260, 110, 320);
	node_type_init(&ntype, node_composit_init_ellipsemask);
	node_type_storage(&ntype, "NodeEllipseMask", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
